#include "rules/semantic_extension/independent_set_live.hpp"

#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/independent_set_recognition.hpp"
#include "rules/semantic_extension/interruptible_work.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace tactix {
namespace {

namespace independent = independent_set_extension;

using SourceClause = IndependentSetOwnerClause;

struct Snapshot {
  std::vector<SourceClause> clauses;
  uint64_t literals = 0;
};

using WorkMeter = InterruptibleWork;

IndependentSetLiveStatus meter_status(const WorkMeter &meter,
                                      IndependentSetLiveStatus otherwise) {
  if (meter.interrupted)
    return IndependentSetLiveStatus::Interrupted;
  if (meter.exhausted)
    return IndependentSetLiveStatus::ResourceExhausted;
  return otherwise;
}

bool take_clause(const Kernel &kernel,
                 const IndependentSetLiveRequest &request, WorkMeter &meter,
                 Snapshot &snapshot, const Lit *literals, uint32_t size,
                 uint64_t native_id) {
  if (snapshot.clauses.size() == request.maximum_clauses ||
      size > request.maximum_literals - snapshot.literals) {
    meter.exhausted = true;
    return false;
  }
  if (!meter.charge(uint64_t(size) + 1))
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

bool root_snapshot_guard(const Kernel &kernel) {
  return !kernel.inconsistent && !kernel.level && !kernel.conflict_valid &&
         !kernel.pending.valid && !kernel.dense && !kernel.probing.recording &&
         kernel.large_watches_connected &&
         kernel_propagation_fixpoint(kernel) && kernel.trail.empty() &&
         !kernel.external_to_internal.empty() &&
         kernel.external_to_internal.size() <= INT32_MAX &&
         !kernel_terminating(kernel);
}

bool owner_request_valid(const IndependentSetLiveRequest &request) {
  return request.maximum_clauses && request.maximum_literals &&
         request.maximum_recognition_work && request.maximum_vertices >= 2 &&
         request.maximum_target;
}

bool import_owner_snapshot(const IndependentSetOwnerSnapshot &owner,
                           const IndependentSetLiveRequest &request,
                           const Kernel &kernel, WorkMeter &meter,
                           Snapshot &snapshot) {
  if (!owner.external_variables || owner.external_variables > INT32_MAX ||
      owner.external_variables > kernel.external_to_internal.size() ||
      owner.vertices < 2 || !owner.target || owner.target > owner.vertices ||
      owner.clauses.empty() ||
      owner.clauses.size() > request.maximum_clauses)
    return false;
  Snapshot result;
  result.clauses.reserve(owner.clauses.size());
  for (size_t index = 0; index < owner.clauses.size(); ++index) {
    const SourceClause &source = owner.clauses[index];
    if (!meter.charge(uint64_t(source.literals.size()) + 1))
      return false;
    if (source.occurrence != index + 1 || source.literals.empty() ||
        source.literals.size() > request.maximum_literals - result.literals ||
        !std::is_sorted(source.literals.begin(), source.literals.end()) ||
        std::adjacent_find(source.literals.begin(), source.literals.end()) !=
            source.literals.end())
      return false;
    for (int32_t literal : source.literals) {
      if (!literal || literal == INT32_MIN ||
          uint64_t(literal < 0 ? -int64_t(literal) : literal) >
              owner.external_variables ||
          std::binary_search(source.literals.begin(), source.literals.end(),
                             -literal))
        return false;
    }
    result.literals += source.literals.size();
    result.clauses.push_back(source);
  }
  if (result.literals != owner.literals)
    return false;
  snapshot = std::move(result);
  return true;
}

using SourceKey = std::pair<uint64_t, std::vector<int32_t>>;

SourceKey source_key(uint64_t native_id, std::vector<int32_t> literals) {
  std::sort(literals.begin(), literals.end());
  return {native_id, std::move(literals)};
}

// Revalidate retained sources by walking only current arena boundaries and
// live binary watches. This deliberately never dereferences a retained raw
// Ref, so collection/compaction can only produce a clean miss.
bool retained_sources_live(const Kernel &kernel, const Snapshot &snapshot,
                           WorkMeter &meter) {
  std::map<SourceKey, uint64_t> missing;
  std::set<std::pair<uint64_t, size_t>> possible;
  for (const SourceClause &source : snapshot.clauses) {
    ++missing[{source.native_id, source.literals}];
    possible.insert({source.native_id, source.literals.size()});
  }
  uint64_t remaining = snapshot.clauses.size();
  auto observe = [&](const Lit *literals, uint32_t size,
                     uint64_t native_id) {
    if (!possible.contains({native_id, size}))
      return true;
    if (!meter.charge(uint64_t(size) + 1))
      return false;
    std::vector<int32_t> external;
    external.reserve(size);
    for (uint32_t index = 0; index < size; ++index) {
      if (kernel_var_retired(kernel, var_of(literals[index])) ||
          kernel.value(literals[index]))
        return false;
      external.push_back(kernel_externalize(kernel, literals[index]));
    }
    const auto found = missing.find(source_key(native_id, std::move(external)));
    if (found != missing.end() && found->second) {
      --found->second;
      --remaining;
    }
    return true;
  };
  for (Ref reference = 0; reference < kernel.arena.size() && remaining;) {
    const Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
    if (!meter.charge(1))
      return false;
    if (!clause->garbage() &&
        !observe(clause->lits, clause->size, clause->id()))
      return false;
  }
  for (Lit first = 0; first < 2 * kernel.nvars && remaining; ++first)
    for (const WatchEntry watch : kernel.watches.entries(first)) {
      if (!meter.charge(1))
        return false;
      if (!watch.binary() || first >= watch.other())
        continue;
      const Lit pair[] = {first, watch.other()};
      if (!observe(pair, 2,
                   kernel_binary_id(kernel, first, watch.other())))
        return false;
      if (!remaining)
        break;
    }
  return !remaining;
}

enum class CoverLiveness : uint8_t { Absent, Present, Stopped };

CoverLiveness cover_live(const Kernel &kernel,
                         const std::vector<int32_t> &external,
                         WorkMeter &meter) {
  std::vector<Lit> internal;
  internal.reserve(external.size());
  for (int32_t literal : external) {
    if (!meter.charge(1))
      return CoverLiveness::Stopped;
    const Lit converted = kernel_internalize(kernel, literal);
    if (converted == INVALID_LIT ||
        kernel_var_retired(kernel, var_of(converted)))
      return CoverLiveness::Absent;
    if (kernel.value(converted) > 0)
      return CoverLiveness::Present;
    internal.push_back(converted);
  }
  if (internal.size() == 1)
    return kernel.value(internal.front()) > 0 ? CoverLiveness::Present
                                             : CoverLiveness::Absent;
  if (internal.size() == 2)
    return kernel_binary_live(kernel, internal[0], internal[1])
               ? CoverLiveness::Present
               : CoverLiveness::Absent;
  std::vector<int32_t> wanted = external;
  std::sort(wanted.begin(), wanted.end());
  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
    if (!meter.charge(1))
      return CoverLiveness::Stopped;
    if (clause->garbage() || clause->size != wanted.size())
      continue;
    if (!meter.charge(clause->size))
      return CoverLiveness::Stopped;
    std::vector<int32_t> found;
    found.reserve(clause->size);
    for (const Lit literal : *clause)
      found.push_back(kernel_externalize(kernel, literal));
    std::sort(found.begin(), found.end());
    if (found == wanted)
      return CoverLiveness::Present;
  }
  return CoverLiveness::Absent;
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

bool render_journal(const IndependentSetLiveRequest &request,
                    const Snapshot &source,
                    const independent::Image &image,
                    const independent::CoverCertificate &certificate,
                    const std::vector<uint64_t> &proof_ids,
                    uint64_t published_id,
                    const IndependentSetLiveResult &result,
                    std::string &output, uint64_t &work) {
  work = 0;
  FILE *records = std::tmpfile();
  if (!records)
    return false;
  const bool proof_mode = !proof_ids.empty();
  std::fprintf(records,
               "{\"action\":%llu,\"semantic\":%llu,"
               "\"kind\":\"independent-set-live-begin\","
               "\"maximum_clauses\":%llu,\"maximum_literals\":%llu,"
               "\"maximum_recognition_work\":%llu,"
               "\"maximum_proof_work\":%llu,"
               "\"maximum_proof_additions\":%llu,"
               "\"maximum_proof_deletions\":%llu,"
               "\"maximum_proof_literal_occurrences\":%llu,"
               "\"maximum_journal_bytes\":%llu}\n",
               (unsigned long long)request.action_id,
               (unsigned long long)request.semantic_id,
               (unsigned long long)request.maximum_clauses,
               (unsigned long long)request.maximum_literals,
               (unsigned long long)request.maximum_recognition_work,
               (unsigned long long)request.maximum_proof_work,
               (unsigned long long)request.maximum_proof_additions,
               (unsigned long long)request.maximum_proof_deletions,
               (unsigned long long)request.maximum_proof_literal_occurrences,
               (unsigned long long)request.maximum_journal_bytes);
  for (const SourceClause &source_clause : source.clauses) {
    std::fprintf(records,
                 "{\"action\":%llu,\"semantic\":%llu,"
                 "\"kind\":\"independent-set-live-source\","
                 "\"occurrence\":%llu,\"native_id\":%llu,\"clause\":[",
                 (unsigned long long)request.action_id,
                 (unsigned long long)request.semantic_id,
                 (unsigned long long)source_clause.occurrence,
                 (unsigned long long)source_clause.native_id);
    for (size_t index = 0; index < source_clause.literals.size(); ++index)
      std::fprintf(records, "%s%d", index ? "," : "",
                   source_clause.literals[index]);
    std::fputs("]}\n", records);
  }
  std::fprintf(records,
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
    std::fprintf(records, "%s%d", index ? "," : "",
                 image.vertex_literals[index]);
  std::fputs("]}\n", records);
  for (size_t index = 0; index < certificate.steps.size(); ++index) {
    const auto &step = certificate.steps[index];
    const uint64_t id = proof_mode ? proof_ids[index] : 0;
    std::fprintf(records,
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
      std::fprintf(records, "%s%d", literal ? "," : "",
                   step.clause[literal]);
    std::fputs("],\"premises\":[", records);
    for (size_t premise = 0; premise < step.premise_occurrences.size();
         ++premise)
      std::fprintf(records, "%s%llu", premise ? "," : "",
                   (unsigned long long)step.premise_occurrences[premise]);
    std::fprintf(records,
                 "],\"left\":%u,\"right\":%u,\"pivot\":%d}\n",
                 step.left, step.right, step.pivot);
  }
  std::fprintf(records,
               "{\"action\":%llu,\"semantic\":%llu,"
               "\"kind\":\"independent-set-conclusion-binding\","
               "\"proof_mode\":\"%s\",\"conclusion_step\":%zu,"
               "\"conclusion_proof_id\":%llu,"
               "\"published_clause_id\":%llu,\"clause\":[",
               (unsigned long long)request.action_id,
               (unsigned long long)request.semantic_id,
               proof_mode ? "rup" : "none", certificate.steps.size() - 1,
               (unsigned long long)(proof_mode ? proof_ids.back() : 0),
               (unsigned long long)published_id);
  for (size_t index = 0; index < certificate.conclusion.size(); ++index)
    std::fprintf(records, "%s%d", index ? "," : "",
                 certificate.conclusion[index]);
  std::fputs("]}\n", records);
  if (proof_mode)
    for (size_t index = 0; index + 1 < certificate.steps.size(); ++index)
      journal_clause(records, request.action_id, request.semantic_id,
                     "independent-set-delete-proof-only", proof_ids[index],
                     certificate.steps[index].clause);
  journal_clause(records, request.action_id, request.semantic_id,
                 "independent-set-publish-cover", published_id,
                 certificate.conclusion);
  std::fprintf(records,
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
  bool ok = !std::fflush(records) && !std::ferror(records);
  const long bytes = ok ? std::ftell(records) : -1;
  if (bytes >= 0)
    work = static_cast<uint64_t>(bytes);
  ok = bytes >= 0 && uint64_t(bytes) <= request.maximum_journal_bytes;
  std::string rendered;
  if (ok) {
    rendered.resize(static_cast<size_t>(bytes));
    std::rewind(records);
    ok = std::fread(rendered.data(), 1, rendered.size(), records) ==
             rendered.size() &&
         !std::ferror(records);
  }
  if (std::fclose(records))
    std::abort();
  if (!ok)
    return false;
  output = std::move(rendered);
  return true;
}

} // namespace

IndependentSetOwnerCaptureResult
independent_set_live_capture_owner_detailed(
    Kernel &kernel, const IndependentSetLiveRequest &request,
    IndependentSetOwnerSnapshot &output) {
  IndependentSetOwnerCaptureResult result;
  if (!owner_request_valid(request))
    return result;
  if (!root_snapshot_guard(kernel)) {
    result.status = kernel_terminating(kernel)
                        ? IndependentSetLiveStatus::Interrupted
                        : IndependentSetLiveStatus::Inapplicable;
    return result;
  }
  WorkMeter recognition{kernel, request.maximum_recognition_work, 0};
  Snapshot snapshot;
  const uint64_t scan_before = recognition.used;
  if (!snapshot_live(kernel, request, recognition, snapshot)) {
    result.source_scan_work = recognition.used - scan_before;
    result.recognition_work = recognition.used;
    result.status = meter_status(recognition,
                                 IndependentSetLiveStatus::NoStructure);
    return result;
  }
  result.source_scan_work = recognition.used - scan_before;
  independent::Image image;
  const independent::Limits limits{
      request.maximum_clauses, request.maximum_literals,
      request.maximum_recognition_work, request.maximum_vertices,
      request.maximum_target};
  const uint64_t recognition_before = recognition.used;
  if (!independent::recognize(
          premises(snapshot), kernel.external_to_internal.size(), limits,
          WorkMeter::callback, &recognition, image)) {
    result.theory_recognition_work = recognition.used - recognition_before;
    result.recognition_work = recognition.used;
    result.status = meter_status(recognition,
                                 IndependentSetLiveStatus::NoStructure);
    return result;
  }
  result.theory_recognition_work = recognition.used - recognition_before;
  result.recognition_work = recognition.used;
  IndependentSetOwnerSnapshot captured;
  captured.external_variables = image.external_variables;
  captured.vertices = image.vertices;
  captured.target = image.target;
  captured.literals = snapshot.literals;
  captured.clauses = std::move(snapshot.clauses);
  captured.capture_source_scan_work = result.source_scan_work;
  captured.capture_theory_recognition_work =
      result.theory_recognition_work;
  result.source_clauses = captured.clauses.size();
  result.source_literals = captured.literals;
  result.status = IndependentSetLiveStatus::Applied;
  output = std::move(captured);
  return result;
}

bool independent_set_live_capture_owner(
    Kernel &kernel, const IndependentSetLiveRequest &request,
    IndependentSetOwnerSnapshot &output) {
  return independent_set_live_capture_owner_detailed(kernel, request, output)
      .captured();
}

IndependentSetLiveResult independent_set_live_apply(
    Kernel &kernel, const IndependentSetLiveRequest &request) {
  IndependentSetLiveResult result;
  if (!request.records || !request.action_id || !request.semantic_id ||
      request.cover_vertices.empty() || !owner_request_valid(request) ||
      !request.maximum_proof_work || !request.maximum_proof_additions ||
      !request.maximum_proof_literal_occurrences ||
      !request.maximum_journal_bytes ||
      (request.include_owner_capture_effort && !request.owner_snapshot))
    return result;
  if (kernel.proof && (kernel.proof->lrat() || kernel.proof->terminal_pbp())) {
    result.status = IndependentSetLiveStatus::UnsupportedProof;
    return result;
  }
  if (!root_snapshot_guard(kernel)) {
    result.status = kernel_terminating(kernel)
                        ? IndependentSetLiveStatus::Interrupted
                        : IndependentSetLiveStatus::Inapplicable;
    return result;
  }

  std::vector<uint32_t> selected = request.cover_vertices;
  std::sort(selected.begin(), selected.end());
  if (selected.size() < 2) {
    result.status = IndependentSetLiveStatus::Inapplicable;
    return result;
  }
  if (!selected.front() ||
      selected.back() > kernel.external_to_internal.size() ||
      std::adjacent_find(selected.begin(), selected.end()) != selected.end())
    return result;
  std::vector<int32_t> external_cover;
  external_cover.reserve(selected.size());
  for (uint32_t variable : selected)
    external_cover.push_back(static_cast<int32_t>(variable));

  uint64_t capture_source_work = 0;
  uint64_t capture_theory_work = 0;
  if (request.include_owner_capture_effort) {
    capture_source_work = request.owner_snapshot->capture_source_scan_work;
    capture_theory_work =
        request.owner_snapshot->capture_theory_recognition_work;
    if (capture_theory_work >
        std::numeric_limits<uint64_t>::max() - capture_source_work)
      return result;
  }
  const uint64_t capture_work = capture_source_work + capture_theory_work;
  result.source_scan_work = capture_source_work;
  result.theory_recognition_work = capture_theory_work;
  result.recognition_work = capture_work;
  if (capture_work > request.maximum_recognition_work) {
    result.status = IndependentSetLiveStatus::ResourceExhausted;
    return result;
  }

  WorkMeter recognition{kernel, request.maximum_recognition_work,
                        capture_work};
  const auto update_recognition = [&] {
    result.recognition_work = recognition.used;
  };
  const auto scan = [&](auto &&operation) {
    const uint64_t before = recognition.used;
    const auto outcome = operation();
    result.source_scan_work += recognition.used - before;
    update_recognition();
    return outcome;
  };
  const auto recognize = [&](auto &&operation) {
    const uint64_t before = recognition.used;
    const bool outcome = operation();
    result.theory_recognition_work += recognition.used - before;
    update_recognition();
    return outcome;
  };
  const auto recognition_decline = [&](IndependentSetLiveStatus fallback) {
    result.status = meter_status(recognition, fallback);
    update_recognition();
    return result;
  };

  const CoverLiveness duplicate =
      scan([&] { return cover_live(kernel, external_cover, recognition); });
  if (duplicate == CoverLiveness::Stopped)
    return recognition_decline(IndependentSetLiveStatus::Stale);
  if (duplicate == CoverLiveness::Present) {
    result.status = IndependentSetLiveStatus::Duplicate;
    return result;
  }

  Snapshot first;
  uint32_t source_external_variables = kernel.external_to_internal.size();
  if (request.owner_snapshot) {
    if (!scan([&] {
          return import_owner_snapshot(*request.owner_snapshot, request,
                                       kernel, recognition, first);
        }))
      return recognition_decline(IndependentSetLiveStatus::Invalid);
    if (!scan([&] {
          return retained_sources_live(kernel, first, recognition);
        }))
      return recognition_decline(IndependentSetLiveStatus::Stale);
    source_external_variables = request.owner_snapshot->external_variables;
  } else if (!scan([&] {
               return snapshot_live(kernel, request, recognition, first);
             })) {
    return recognition_decline(IndependentSetLiveStatus::NoStructure);
  }
  result.source_clauses = first.clauses.size();
  result.source_literals = first.literals;
  const std::vector<independent::Premise> source = premises(first);
  independent::Limits recognition_limits{
      request.maximum_clauses, request.maximum_literals,
      request.maximum_recognition_work, request.maximum_vertices,
      request.maximum_target};
  independent::Image image;
  if (!recognize([&] {
        return independent::recognize(
            source, source_external_variables, recognition_limits,
            WorkMeter::callback, &recognition, image);
      }))
    return recognition_decline(IndependentSetLiveStatus::NoStructure);
  if (request.owner_snapshot &&
      (image.external_variables != request.owner_snapshot->external_variables ||
       image.vertices != request.owner_snapshot->vertices ||
       image.target != request.owner_snapshot->target)) {
    result.status = IndependentSetLiveStatus::Stale;
    return result;
  }
  result.vertices = image.vertices;
  result.target = image.target;
  if (selected.size() != image.vertices - image.target + 1) {
    result.status = IndependentSetLiveStatus::Invalid;
    return result;
  }

  WorkMeter proof_work{kernel, request.maximum_proof_work, 0};
  independent::CertificateLimits certificate_limits{
      request.maximum_proof_additions,
      request.maximum_proof_literal_occurrences, request.maximum_proof_work};
  independent::CoverCertificate certificate;
  if (!independent::derive_cover_certificate(
          image, selected, certificate_limits, WorkMeter::callback,
          &proof_work, certificate)) {
    result.proof_work = proof_work.used;
    result.status = meter_status(proof_work,
                                 IndependentSetLiveStatus::ResourceExhausted);
    return result;
  }
  result.proof_work = proof_work.used;
  result.planned_proof_additions = certificate.steps.size();
  result.proof_literal_occurrences = certificate.literal_occurrences;
  if (certificate.steps.empty() ||
      certificate.steps.size() > request.maximum_proof_additions ||
      certificate.steps.size() - 1 > request.maximum_proof_deletions ||
      certificate.conclusion != external_cover) {
    result.status = IndependentSetLiveStatus::ResourceExhausted;
    return result;
  }

  if (request.owner_snapshot) {
    if (!scan([&] {
          return retained_sources_live(kernel, first, recognition);
        }))
      return recognition_decline(IndependentSetLiveStatus::Stale);
  } else {
    Snapshot second;
    if (!scan([&] {
          return snapshot_live(kernel, request, recognition, second);
        }) ||
        first.clauses != second.clauses ||
        first.literals != second.literals) {
      if (recognition.interrupted || recognition.exhausted)
        return recognition_decline(IndependentSetLiveStatus::Stale);
      result.status = IndependentSetLiveStatus::Stale;
      return result;
    }
  }
  if (kernel_terminating(kernel)) {
    result.status = IndependentSetLiveStatus::Interrupted;
    return result;
  }
  const CoverLiveness postflight =
      scan([&] { return cover_live(kernel, external_cover, recognition); });
  if (postflight == CoverLiveness::Stopped)
    return recognition_decline(IndependentSetLiveStatus::Stale);
  if (postflight == CoverLiveness::Present) {
    result.status = IndependentSetLiveStatus::Duplicate;
    return result;
  }

  std::vector<Lit> internal_cover;
  internal_cover.reserve(external_cover.size());
  for (int32_t external : external_cover) {
    const Lit literal = kernel_internalize(kernel, external);
    if (literal == INVALID_LIT || kernel_var_retired(kernel, var_of(literal)) ||
        kernel.value(literal)) {
      result.status = IndependentSetLiveStatus::Stale;
      return result;
    }
    internal_cover.push_back(literal);
  }
  if (internal_cover.size() > 2 &&
      (kernel.arena.size() > INVALID_REF - CLAUSE_HEADER_WORDS ||
       internal_cover.size() >
           INVALID_REF - CLAUSE_HEADER_WORDS - kernel.arena.size())) {
    result.status = IndependentSetLiveStatus::ResourceExhausted;
    return result;
  }
  const uint64_t emitted_additions = kernel.proof ? certificate.steps.size() : 0;
  const uint64_t ids_needed = kernel.proof ? emitted_additions : 1;
  if (kernel.next_clause_id > uint64_t(INT64_MAX) ||
      ids_needed > uint64_t(INT64_MAX) - kernel.next_clause_id) {
    result.status = IndependentSetLiveStatus::ResourceExhausted;
    return result;
  }

  std::vector<uint64_t> proof_ids;
  if (kernel.proof) {
    proof_ids.resize(certificate.steps.size());
    for (size_t index = 0; index < proof_ids.size(); ++index)
      proof_ids[index] = kernel.next_clause_id + index + 1;
  }
  const uint64_t published_id =
      kernel.proof ? proof_ids.back() : kernel.next_clause_id + 1;
  result.status = IndependentSetLiveStatus::Applied;
  result.result = ApplyResult::Ok;
  result.emitted_proof_additions = emitted_additions;
  result.emitted_proof_deletions =
      kernel.proof ? certificate.steps.size() - 1 : 0;
  result.published_clause_id = published_id;
  result.advice_vertex_literals = image.vertex_literals;
  result.published_cover = external_cover;
  std::string journal;
  uint64_t journal_work = 0;
  if (!render_journal(request, first, image, certificate, proof_ids,
                      published_id, result, journal, journal_work)) {
    result.status = kernel_terminating(kernel)
                        ? IndependentSetLiveStatus::Interrupted
                        : IndependentSetLiveStatus::ResourceExhausted;
    result.result = ApplyResult::NoOp;
    result.emitted_proof_additions = 0;
    result.emitted_proof_deletions = 0;
    result.published_clause_id = 0;
    result.advice_vertex_literals.clear();
    result.published_cover.clear();
    result.journal_work = journal_work;
    return result;
  }
  result.journal_work = journal_work;

  // Commit begins here.  Every bounded, structural, liveness, proof-format,
  // interruption, clause-identity, and journal-size check has completed. The
  // proof writer and records sink are fatal I/O boundaries: a short write
  // aborts instead of returning a partially published kernel state.
  kernel.close_startup_certificate();
  if (kernel.proof) {
    for (size_t index = 0; index < certificate.steps.size(); ++index) {
      const auto &step = certificate.steps[index];
      const uint64_t id = ++kernel.next_clause_id;
      if (id != proof_ids[index])
        std::abort();
      if (!kernel.proof->add_external_rup(
              id, step.clause.empty() ? nullptr : step.clause.data(),
              static_cast<uint32_t>(step.clause.size())))
        std::abort();
    }
  } else {
    if (++kernel.next_clause_id != published_id)
      std::abort();
  }

  if (internal_cover.size() == 2) {
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
  if (propagate_service(kernel) != ApplyResult::Ok)
    std::abort();
  if (std::fwrite(journal.data(), 1, journal.size(), request.records) !=
          journal.size() ||
      std::fflush(request.records) || std::ferror(request.records))
    std::abort();
  return result;
}

} // namespace tactix
