#include "rules/semantic_extension/independent_set_live.hpp"

#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/independent_set_recognition.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <set>
#include <utility>

namespace tactix {
namespace {

namespace independent = independent_set_extension;

struct SourceClause {
  uint64_t occurrence = 0;
  uint64_t native_id = 0;
  std::vector<int32_t> literals;

  bool operator==(const SourceClause &other) const {
    return native_id == other.native_id && literals == other.literals;
  }
};

struct Snapshot {
  std::vector<SourceClause> clauses;
  uint64_t literals = 0;
};

struct WorkMeter {
  const Kernel &kernel;
  uint64_t limit = 0;
  uint64_t used = 0;

  bool charge(uint64_t amount) {
    if (kernel_terminating(kernel) || amount > limit - used)
      return false;
    used += amount;
    return true;
  }

  static bool callback(void *context, uint64_t amount) {
    return static_cast<WorkMeter *>(context)->charge(amount);
  }
};

bool take_clause(const Kernel &kernel,
                 const IndependentSetLiveRequest &request, WorkMeter &meter,
                 Snapshot &snapshot, const Lit *literals, uint32_t size,
                 uint64_t native_id) {
  if (snapshot.clauses.size() == request.maximum_clauses ||
      size > request.maximum_literals - snapshot.literals ||
      !meter.charge(uint64_t(size) + 1))
    return false;
  SourceClause source;
  source.native_id = native_id;
  source.literals.reserve(size);
  for (uint32_t index = 0; index < size; ++index) {
    const Var variable = var_of(literals[index]);
    if (kernel_var_retired(kernel, variable) || kernel.value(literals[index]))
      return false;
    source.literals.push_back(kernel_externalize(kernel, literals[index]));
  }
  std::sort(source.literals.begin(), source.literals.end());
  snapshot.literals += size;
  snapshot.clauses.push_back(std::move(source));
  return true;
}

bool snapshot_live(const Kernel &kernel,
                   const IndependentSetLiveRequest &request, WorkMeter &meter,
                   Snapshot &snapshot) {
  snapshot = {};
  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
    if (!meter.charge(1))
      return false;
    if (!clause->garbage() &&
        !take_clause(kernel, request, meter, snapshot, clause->lits,
                     clause->size, clause->id()))
      return false;
  }
  for (Lit first = 0; first < 2 * kernel.nvars; ++first)
    for (const WatchEntry watch : kernel.watches.entries(first)) {
      if (!meter.charge(1))
        return false;
      if (!watch.binary() || first >= watch.other())
        continue;
      const Lit pair[] = {first, watch.other()};
      if (!take_clause(kernel, request, meter, snapshot, pair, 2,
                       kernel_binary_id(kernel, first, watch.other())))
        return false;
    }
  std::sort(snapshot.clauses.begin(), snapshot.clauses.end(),
            [](const SourceClause &left, const SourceClause &right) {
              if (left.literals != right.literals)
                return left.literals < right.literals;
              return left.native_id < right.native_id;
            });
  for (size_t index = 0; index < snapshot.clauses.size(); ++index)
    snapshot.clauses[index].occurrence = index + 1;
  return true;
}

std::vector<independent::Premise> premises(const Snapshot &snapshot) {
  std::vector<independent::Premise> result;
  result.reserve(snapshot.clauses.size());
  for (const SourceClause &source : snapshot.clauses)
    result.push_back({source.occurrence, source.literals});
  return result;
}

bool cover_live(const Kernel &kernel, const std::vector<int32_t> &external) {
  std::vector<Lit> internal;
  internal.reserve(external.size());
  for (int32_t literal : external) {
    const Lit converted = kernel_internalize(kernel, literal);
    if (converted == INVALID_LIT ||
        kernel_var_retired(kernel, var_of(converted)))
      return false;
    if (kernel.value(converted) > 0)
      return true;
    internal.push_back(converted);
  }
  if (internal.size() == 1)
    return kernel.value(internal.front()) > 0;
  if (internal.size() == 2)
    return kernel_binary_live(kernel, internal[0], internal[1]);
  std::vector<int32_t> wanted = external;
  std::sort(wanted.begin(), wanted.end());
  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
    if (clause->garbage() || clause->size != wanted.size())
      continue;
    std::vector<int32_t> found;
    found.reserve(clause->size);
    for (const Lit literal : *clause)
      found.push_back(kernel_externalize(kernel, literal));
    std::sort(found.begin(), found.end());
    if (found == wanted)
      return true;
  }
  return false;
}

void journal_clause(FILE *records, uint64_t action, uint64_t semantic,
                    const char *kind, uint64_t id,
                    const std::vector<int32_t> &clause) {
  std::fprintf(records,
               "{\"action\":%llu,\"semantic\":%llu,\"kind\":\"%s\","
               "\"id\":%llu,\"clause\":[",
               (unsigned long long)action, (unsigned long long)semantic, kind,
               (unsigned long long)id);
  for (size_t index = 0; index < clause.size(); ++index)
    std::fprintf(records, "%s%d", index ? "," : "", clause[index]);
  std::fputs("]}\n", records);
}

} // namespace

IndependentSetLiveResult independent_set_live_apply(
    Kernel &kernel, const IndependentSetLiveRequest &request) {
  IndependentSetLiveResult unchanged;
  if (!request.records || !request.action_id || !request.semantic_id ||
      request.cover_vertices.empty() || !request.maximum_clauses ||
      !request.maximum_literals || !request.maximum_recognition_work ||
      !request.maximum_proof_work || !request.maximum_proof_additions ||
      !request.maximum_proof_literal_occurrences ||
      request.maximum_vertices < 2 || !request.maximum_target ||
      kernel.inconsistent || kernel.level || kernel.conflict_valid ||
      kernel.pending.valid || kernel.dense || kernel.probing.recording ||
      !kernel.large_watches_connected ||
      !kernel_propagation_fixpoint(kernel) || !kernel.trail.empty() ||
      kernel.external_to_internal.empty() ||
      kernel.external_to_internal.size() > INT32_MAX ||
      (kernel.proof && (kernel.proof->lrat() || kernel.proof->pb())) ||
      kernel_terminating(kernel))
    return unchanged;

  std::vector<uint32_t> selected = request.cover_vertices;
  std::sort(selected.begin(), selected.end());
  if (!selected.front() ||
      selected.back() > kernel.external_to_internal.size() ||
      std::adjacent_find(selected.begin(), selected.end()) != selected.end())
    return unchanged;
  std::vector<int32_t> external_cover;
  external_cover.reserve(selected.size());
  for (uint32_t variable : selected) {
    const Lit literal = kernel_internalize(kernel, static_cast<int32_t>(variable));
    if (literal == INVALID_LIT || kernel_var_retired(kernel, var_of(literal)))
      return unchanged;
    external_cover.push_back(static_cast<int32_t>(variable));
  }
  if (cover_live(kernel, external_cover))
    return unchanged;

  WorkMeter recognition{kernel, request.maximum_recognition_work, 0};
  Snapshot first;
  if (!snapshot_live(kernel, request, recognition, first))
    return unchanged;
  const std::vector<independent::Premise> source = premises(first);
  independent::Limits recognition_limits{
      request.maximum_clauses, request.maximum_literals,
      request.maximum_recognition_work, request.maximum_vertices,
      request.maximum_target};
  independent::Image image;
  if (!independent::recognize(source, kernel.external_to_internal.size(),
                              recognition_limits, WorkMeter::callback,
                              &recognition, image))
    return unchanged;

  WorkMeter proof_work{kernel, request.maximum_proof_work, 0};
  independent::CertificateLimits certificate_limits{
      request.maximum_proof_additions,
      request.maximum_proof_literal_occurrences, request.maximum_proof_work};
  independent::CoverCertificate certificate;
  if (!independent::derive_cover_certificate(
          image, selected, certificate_limits, WorkMeter::callback,
          &proof_work, certificate) ||
      certificate.steps.empty() ||
      certificate.steps.size() > request.maximum_proof_additions ||
      certificate.steps.size() - 1 > request.maximum_proof_deletions ||
      certificate.conclusion != external_cover)
    return unchanged;

  Snapshot second;
  if (!snapshot_live(kernel, request, recognition, second) ||
      first.clauses != second.clauses || first.literals != second.literals ||
      kernel_terminating(kernel) || cover_live(kernel, external_cover))
    return unchanged;

  std::vector<Lit> internal_cover;
  internal_cover.reserve(external_cover.size());
  for (int32_t external : external_cover) {
    const Lit literal = kernel_internalize(kernel, external);
    if (literal == INVALID_LIT || kernel_var_retired(kernel, var_of(literal)) ||
        kernel.value(literal))
      return unchanged;
    internal_cover.push_back(literal);
  }
  if (internal_cover.size() > 2 &&
      (kernel.arena.size() > INVALID_REF - CLAUSE_HEADER_WORDS ||
       internal_cover.size() >
           INVALID_REF - CLAUSE_HEADER_WORDS - kernel.arena.size()))
    return unchanged;
  const uint64_t emitted_additions = kernel.proof ? certificate.steps.size() : 0;
  const uint64_t ids_needed = kernel.proof ? emitted_additions : 1;
  if (ids_needed > uint64_t(INT64_MAX) - kernel.next_clause_id)
    return unchanged;

  // Commit begins here.  Every bounded, structural, liveness, proof-format,
  // and interruption check has completed.  The proof writer and records sink
  // are fatal I/O boundaries: a short write aborts instead of returning a
  // partially published kernel state.
  kernel.close_startup_certificate();
  std::vector<uint64_t> proof_ids(certificate.steps.size(), 0);
  uint64_t published_id = 0;
  if (kernel.proof) {
    for (size_t index = 0; index < certificate.steps.size(); ++index) {
      const auto &step = certificate.steps[index];
      const uint64_t id = ++kernel.next_clause_id;
      proof_ids[index] = id;
      if (!kernel.proof->add_external_rup(
              id, step.clause.empty() ? nullptr : step.clause.data(),
              static_cast<uint32_t>(step.clause.size())))
        std::abort();
    }
    published_id = proof_ids.back();
  } else {
    published_id = ++kernel.next_clause_id;
  }

  if (internal_cover.size() == 1) {
    kernel.unit_ids[var_of(internal_cover[0])] = published_id;
    kernel_assign_unit(kernel, internal_cover[0]);
  } else if (internal_cover.size() == 2) {
    kernel_add_binary_clause(kernel, internal_cover[0], internal_cover[1],
                             published_id);
  } else {
    kernel_add_large_clause(kernel, internal_cover.data(),
                            static_cast<uint32_t>(internal_cover.size()), true,
                            static_cast<uint32_t>(internal_cover.size()),
                            published_id);
  }
  if (kernel.proof)
    for (size_t index = 0; index + 1 < certificate.steps.size(); ++index) {
      const auto &step = certificate.steps[index];
      if (!kernel.proof->del_external(
              kernel.next_clause_id, proof_ids[index],
              step.clause.empty() ? nullptr : step.clause.data(),
              static_cast<uint32_t>(step.clause.size())))
        std::abort();
    }

  std::fprintf(request.records,
               "{\"action\":%llu,\"semantic\":%llu,"
               "\"kind\":\"independent-set-live-begin\","
               "\"maximum_clauses\":%llu,\"maximum_literals\":%llu,"
               "\"maximum_recognition_work\":%llu,"
               "\"maximum_proof_work\":%llu,"
               "\"maximum_proof_additions\":%llu,"
               "\"maximum_proof_deletions\":%llu,"
               "\"maximum_proof_literal_occurrences\":%llu}\n",
               (unsigned long long)request.action_id,
               (unsigned long long)request.semantic_id,
               (unsigned long long)request.maximum_clauses,
               (unsigned long long)request.maximum_literals,
               (unsigned long long)request.maximum_recognition_work,
               (unsigned long long)request.maximum_proof_work,
               (unsigned long long)request.maximum_proof_additions,
               (unsigned long long)request.maximum_proof_deletions,
               (unsigned long long)request.maximum_proof_literal_occurrences);
  for (const SourceClause &source_clause : first.clauses) {
    std::fprintf(request.records,
                 "{\"action\":%llu,\"semantic\":%llu,"
                 "\"kind\":\"independent-set-live-source\","
                 "\"occurrence\":%llu,\"native_id\":%llu,\"clause\":[",
                 (unsigned long long)request.action_id,
                 (unsigned long long)request.semantic_id,
                 (unsigned long long)source_clause.occurrence,
                 (unsigned long long)source_clause.native_id);
    for (size_t index = 0; index < source_clause.literals.size(); ++index)
      std::fprintf(request.records, "%s%d", index ? "," : "",
                   source_clause.literals[index]);
    std::fputs("]}\n", request.records);
  }
  std::fprintf(request.records,
               "{\"action\":%llu,\"semantic\":%llu,"
               "\"kind\":\"independent-set-theory\",\"vertices\":%u,"
               "\"target\":%u,\"meaning\":\"selected vertices form an "
               "independent set of cardinality at least target\","
               "\"projection\":\"original variables unchanged\","
               "\"advice_vertices\":[",
               (unsigned long long)request.action_id,
               (unsigned long long)request.semantic_id, image.vertices,
               image.target);
  for (size_t index = 0; index < image.vertex_literals.size(); ++index)
    std::fprintf(request.records, "%s%d", index ? "," : "",
                 image.vertex_literals[index]);
  std::fputs("]}\n", request.records);
  for (size_t index = 0; index < certificate.steps.size(); ++index) {
    const auto &step = certificate.steps[index];
    const uint64_t id = kernel.proof ? proof_ids[index] : 0;
    std::fprintf(request.records,
                 "{\"action\":%llu,\"semantic\":%llu,"
                 "\"kind\":\"independent-set-cover-rup\",\"id\":%llu,"
                 "\"step\":%zu,\"role\":\"%s\",\"clause\":[",
                 (unsigned long long)request.action_id,
                 (unsigned long long)request.semantic_id,
                 (unsigned long long)id, index,
                 step.kind == independent::CertificateStepKind::ForbiddenAssignment
                     ? "forbidden-assignment"
                     : "resolution");
    for (size_t literal = 0; literal < step.clause.size(); ++literal)
      std::fprintf(request.records, "%s%d", literal ? "," : "",
                   step.clause[literal]);
    std::fputs("],\"premises\":[", request.records);
    for (size_t premise = 0; premise < step.premise_occurrences.size();
         ++premise)
      std::fprintf(request.records, "%s%llu", premise ? "," : "",
                   (unsigned long long)step.premise_occurrences[premise]);
    std::fprintf(request.records,
                 "],\"left\":%u,\"right\":%u,\"pivot\":%d}\n",
                 step.left, step.right, step.pivot);
  }
  if (kernel.proof)
    for (size_t index = 0; index + 1 < certificate.steps.size(); ++index)
      journal_clause(request.records, request.action_id, request.semantic_id,
                     "independent-set-delete-proof-only", proof_ids[index],
                     certificate.steps[index].clause);
  journal_clause(request.records, request.action_id, request.semantic_id,
                 "independent-set-publish-cover", published_id,
                 external_cover);

  const ApplyResult propagated = propagate_service(kernel);
  IndependentSetLiveResult result;
  result.result = propagated == ApplyResult::Unsat ? ApplyResult::Unsat
                                                    : ApplyResult::Ok;
  result.vertices = image.vertices;
  result.target = image.target;
  result.source_clauses = first.clauses.size();
  result.source_literals = first.literals;
  result.recognition_work = recognition.used;
  result.proof_work = proof_work.used;
  result.planned_proof_additions = certificate.steps.size();
  result.emitted_proof_additions = emitted_additions;
  result.emitted_proof_deletions =
      kernel.proof ? certificate.steps.size() - 1 : 0;
  result.proof_literal_occurrences = certificate.literal_occurrences;
  result.published_clause_id = published_id;
  result.advice_vertex_literals = image.vertex_literals;
  result.published_cover = external_cover;
  std::fprintf(request.records,
               "{\"action\":%llu,\"semantic\":%llu,"
               "\"kind\":\"independent-set-live-complete\","
               "\"result\":%u,\"recognition_work\":%llu,"
               "\"proof_work\":%llu,\"planned_additions\":%llu,"
               "\"emitted_additions\":%llu,\"emitted_deletions\":%llu,"
               "\"proof_literal_occurrences\":%llu,"
               "\"published_clause_id\":%llu}\n",
               (unsigned long long)request.action_id,
               (unsigned long long)request.semantic_id,
               static_cast<unsigned>(result.result),
               (unsigned long long)result.recognition_work,
               (unsigned long long)result.proof_work,
               (unsigned long long)result.planned_proof_additions,
               (unsigned long long)result.emitted_proof_additions,
               (unsigned long long)result.emitted_proof_deletions,
               (unsigned long long)result.proof_literal_occurrences,
               (unsigned long long)result.published_clause_id);
  if (std::fflush(request.records) || std::ferror(request.records))
    std::abort();
  return result;
}

} // namespace tactix
