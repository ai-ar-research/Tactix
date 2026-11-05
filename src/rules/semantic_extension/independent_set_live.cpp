#include "rules/semantic_extension/independent_set_live.hpp"

#include "kernel/types.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/csp_physical_scan.hpp"
#include "rules/semantic_extension/independent_set_recognition.hpp"
#include "rules/semantic_extension/interruptible_work.hpp"
#include "rules/semantic_extension/live_adapter.hpp"
#include "rules/structural_text.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
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
  if (!unassigned_external_clause(kernel, literals, size, source.literals))
    return false;
  std::sort(source.literals.begin(), source.literals.end());
  snapshot.literals += size;
  snapshot.clauses.push_back(std::move(source));
  return true;
}

bool snapshot_live(const Kernel &kernel,
                   const IndependentSetLiveRequest &request, WorkMeter &meter,
                   Snapshot &snapshot) {
  snapshot = {};
  auto take = [&](const Lit *literals, uint32_t size, uint64_t native_id) {
    return take_clause(kernel, request, meter, snapshot, literals, size,
                       native_id);
  };
  if (csp_physical_scan::run<true>(kernel, meter, take) !=
      csp_physical_scan::Status::Complete)
    return false;
  csp_physical_scan::sort_premises(snapshot.clauses, &SourceClause::native_id);
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
  return csp_physical_scan::root_ready(kernel) && !kernel_terminating(kernel);
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
          external_variable_of(literal) > owner.external_variables ||
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
    if (!unassigned_external_clause(kernel, literals, size, external))
      return false;
    const auto found = missing.find(source_key(native_id, std::move(external)));
    if (found != missing.end() && found->second) {
      --found->second;
      --remaining;
    }
    // Stop the scan, charging nothing more, once every source is found.
    return remaining != 0;
  };
  if (remaining) csp_physical_scan::run<true>(kernel, meter, observe);
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
  for (const auto [reference, clause] : arena_records(kernel.arena)) {
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

// The action's journal records, spelled for a stream and otherwise measured.
void render_journal(const IndependentSetLiveRequest &request,
                    const Snapshot &source,
                    const independent::Image &image,
                    const independent::CoverCertificate &certificate,
                    const std::vector<uint64_t> &proof_ids,
                    uint64_t published_id,
                    const IndependentSetLiveResult &result,
                    JournalText &journal) {
  const bool proof_mode = !proof_ids.empty();
  const auto head = [&request](const char *kind) {
    return [&request, kind](auto &out) {
      out("{\"action\":", request.action_id, ",\"semantic\":",
          request.semantic_id, ",\"kind\":\"", kind, "\"");
    };
  };
  const auto clause = [&](const char *kind, uint64_t id,
                          const std::vector<int32_t> &literals) {
    journal.put(head(kind), ",\"id\":", id, ",\"clause\":[",
                journal_list(literals), "]}\n");
  };
  journal.put(head("independent-set-live-begin"), ",\"maximum_clauses\":",
              request.maximum_clauses, ",\"maximum_literals\":",
              request.maximum_literals, ",\"maximum_recognition_work\":",
              request.maximum_recognition_work, ",\"maximum_proof_work\":",
              request.maximum_proof_work, ",\"maximum_proof_additions\":",
              request.maximum_proof_additions, ",\"maximum_proof_deletions\":",
              request.maximum_proof_deletions,
              ",\"maximum_proof_literal_occurrences\":",
              request.maximum_proof_literal_occurrences,
              ",\"maximum_journal_bytes\":", request.maximum_journal_bytes,
              "}\n");
  for (const SourceClause &source_clause : source.clauses)
    journal.put(head("independent-set-live-source"), ",\"occurrence\":",
                source_clause.occurrence, ",\"native_id\":",
                source_clause.native_id, ",\"clause\":[",
                journal_list(source_clause.literals), "]}\n");
  journal.put(head("independent-set-theory"), ",\"vertices\":", image.vertices,
              ",\"target\":", image.target,
              ",\"meaning\":\"selected vertices form an independent set of "
              "cardinality at least target\","
              "\"projection\":\"original variables unchanged\","
              "\"advice_vertices\":[",
              journal_list(image.vertex_literals), "]}\n");
  for (size_t index = 0; index < certificate.steps.size(); ++index) {
    const auto &step = certificate.steps[index];
    const uint64_t id = proof_mode ? proof_ids[index] : 0;
    journal.put(
        head("independent-set-cover-rup"), ",\"id\":", id, ",\"step\":", index,
        ",\"role\":\"",
        step.kind == independent::CertificateStepKind::ForbiddenAssignment
            ? "forbidden-assignment"
            : "resolution",
        "\",\"clause\":[", journal_list(step.clause), "],\"premises\":[",
        journal_list(step.premise_occurrences), "],\"left\":", step.left,
        ",\"right\":", step.right, ",\"pivot\":", step.pivot, "}\n");
  }
  journal.put(head("independent-set-conclusion-binding"), ",\"proof_mode\":\"",
              proof_mode ? "rup" : "none", "\",\"conclusion_step\":",
              certificate.steps.size() - 1, ",\"conclusion_proof_id\":",
              proof_mode ? proof_ids.back() : 0, ",\"published_clause_id\":",
              published_id, ",\"clause\":[",
              journal_list(certificate.conclusion), "]}\n");
  if (proof_mode)
    for (size_t index = 0; index + 1 < certificate.steps.size(); ++index)
      clause("independent-set-delete-proof-only", proof_ids[index],
             certificate.steps[index].clause);
  clause("independent-set-publish-cover", published_id, certificate.conclusion);
  journal.put(head("independent-set-live-complete"), ",\"result\":",
              static_cast<unsigned>(result.result), ",\"recognition_work\":",
              result.recognition_work, ",\"proof_work\":", result.proof_work,
              ",\"planned_additions\":", result.planned_proof_additions,
              ",\"emitted_additions\":", result.emitted_proof_additions,
              ",\"emitted_deletions\":", result.emitted_proof_deletions,
              ",\"proof_literal_occurrences\":",
              result.proof_literal_occurrences, ",\"published_clause_id\":",
              result.published_clause_id, "}\n");
}

} // namespace

IndependentSetOwnerCaptureResult
independent_set_live_capture_owner_detailed(
    Kernel &kernel, const IndependentSetLiveRequest &request,
    IndependentSetOwnerSnapshot &output) {
  IndependentSetOwnerCaptureResult result;
  if (!owner_request_valid(request) ||
      !root_admitted(kernel, root_snapshot_guard, result.status))
    return result;
  WorkMeter recognition{kernel, request.maximum_recognition_work, 0};
  Snapshot snapshot;
  const uint64_t scan_before = recognition.used;
  if (!snapshot_live(kernel, request, recognition, snapshot)) {
    result.source_scan_work = recognition.used - scan_before;
    result.recognition_work = recognition.used;
    result.status = stopped_status(recognition,
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
    result.status = stopped_status(recognition,
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
      (request.include_owner_capture_effort && !request.owner_snapshot) ||
      !live_admitted(kernel, root_snapshot_guard, result.status))
    return result;

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
    result.status = stopped_status(recognition, fallback);
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
    result.status = stopped_status(proof_work,
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
  JournalText journal(request.records);
  render_journal(request, first, image, certificate, proof_ids, published_id,
                 result, journal);
  result.journal_work = journal.size();
  if (journal.size() > request.maximum_journal_bytes) {
    result.status =
        terminating_status(kernel, IndependentSetLiveStatus::ResourceExhausted);
    result.result = ApplyResult::NoOp;
    result.emitted_proof_additions = 0;
    result.emitted_proof_deletions = 0;
    result.published_clause_id = 0;
    result.advice_vertex_literals.clear();
    result.published_cover.clear();
    return result;
  }

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
  journal.write(request.records);
  return result;
}

} // namespace tactix
