#include "rules/semantic_extension/independent_set_cover_action.hpp"

#include "rules/structural_action_guard.hpp"

#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "strategy/action_status_statistics.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace tactix {
namespace {

IndependentSetLiveRequest request_from_kernel(
    Kernel &kernel, const std::vector<uint32_t> &cover_vertices,
    uint64_t action_id) {
  const auto &parameters = kernel.independent_set_cover_parameters;
  IndependentSetLiveRequest request;
  request.cover_vertices = cover_vertices;
  request.maximum_clauses = parameters.maximum_clauses;
  request.maximum_literals = parameters.maximum_literals;
  request.maximum_recognition_work = parameters.maximum_recognition_work;
  request.maximum_proof_work = parameters.maximum_proof_work;
  request.maximum_proof_additions = parameters.maximum_proof_additions;
  request.maximum_proof_deletions = parameters.maximum_proof_deletions;
  request.maximum_proof_literal_occurrences =
      parameters.maximum_proof_literal_occurrences;
  request.maximum_journal_bytes = parameters.maximum_journal_bytes;
  request.maximum_vertices = parameters.maximum_vertices;
  request.maximum_target = parameters.maximum_target;
  request.action_id = action_id;
  request.semantic_id = action_id;
  request.records = kernel.structural_provenance;
  return request;
}

void copy_effort(IndependentSetCoverActionResult &output,
                 const IndependentSetOwnerCaptureResult &capture) {
  output.effort.source_clauses = capture.source_clauses;
  output.effort.source_literals = capture.source_literals;
  output.effort.recognition_work = capture.recognition_work;
  output.effort.source_scan_work = capture.source_scan_work;
  output.effort.theory_recognition_work =
      capture.theory_recognition_work;
}

void copy_effort(IndependentSetCoverActionResult &output,
                 const IndependentSetLiveResult &live) {
  output.effort.source_clauses = live.source_clauses;
  output.effort.source_literals = live.source_literals;
  output.effort.recognition_work = live.recognition_work;
  output.effort.source_scan_work = live.source_scan_work;
  output.effort.theory_recognition_work = live.theory_recognition_work;
  output.effort.proof_work = live.proof_work;
  output.effort.planned_proof_additions = live.planned_proof_additions;
  output.effort.emitted_proof_additions = live.emitted_proof_additions;
  output.effort.emitted_proof_deletions = live.emitted_proof_deletions;
  output.effort.proof_literal_occurrences =
      live.proof_literal_occurrences;
  output.effort.journal_work = live.journal_work;
}

bool valid_cover_payload(const std::vector<uint32_t> &cover) {
  return cover.size() >= 2 && cover.front() &&
         std::is_sorted(cover.begin(), cover.end()) &&
         std::adjacent_find(cover.begin(), cover.end()) == cover.end();
}

struct RecordIndependentSetCover {
  Kernel &kernel;
  IndependentSetCoverActionResult &result;
  ~RecordIndependentSetCover() {
    auto &stats = kernel.stats;
    const auto &effort = result.effort;
    ++stats.independent_set_cover_attempts;
    stats.independent_set_cover_source_clauses += effort.source_clauses;
    stats.independent_set_cover_source_literals += effort.source_literals;
    stats.independent_set_cover_recognition_work += effort.recognition_work;
    stats.independent_set_cover_source_scan_work += effort.source_scan_work;
    stats.independent_set_cover_theory_recognition_work +=
        effort.theory_recognition_work;
    stats.independent_set_cover_proof_work += effort.proof_work;
    stats.independent_set_cover_planned_proof_additions +=
        effort.planned_proof_additions;
    stats.independent_set_cover_proof_additions +=
        effort.emitted_proof_additions;
    stats.independent_set_cover_proof_deletions +=
        effort.emitted_proof_deletions;
    stats.independent_set_cover_proof_literal_occurrences +=
        effort.proof_literal_occurrences;
    stats.independent_set_cover_journal_work += effort.journal_work;
    stats.independent_set_cover_published +=
        result.published_clause_id != 0;
    if (result.candidate_staged)
      stats.independent_set_cover_candidate_variables += result.vertices;
    if (!strategy::record_standard_action_status(
            result.status, TACTIX_STANDARD_ACTION_STATUS_COUNTERS(
                               stats, independent_set_cover))) {
      switch (result.status) {
      case IndependentSetLiveStatus::NoStructure:
        ++stats.independent_set_cover_no_structure;
        break;
      case IndependentSetLiveStatus::Duplicate:
        ++stats.independent_set_cover_duplicate;
        break;
      default: break;
      }
    }
  }
};

} // namespace

RuleEligibility independent_set_cover_action_guard(const Kernel &kernel) {
  return root_action_guard(kernel, true)
      .fact("session-attached",
            kernel.independent_set_cover_action_session != nullptr)
      .identity()
      .semantic_candidate()
      .clausal_certificate()
      .finish();
}

ApplyResult independent_set_cover_rule_apply(Kernel &kernel) {
  (void)kernel;
  return ApplyResult::NoOp;
}

IndependentSetCoverActionResult independent_set_cover_action_apply(
    Kernel &kernel, IndependentSetCoverActionSession &session,
    const std::vector<uint32_t> &cover_vertices) {
  IndependentSetCoverActionResult output;
  RecordIndependentSetCover record{kernel, output};
  output.owner_retained = session.retained();
  if (!independent_set_cover_action_guard(kernel) ||
      kernel.independent_set_cover_action_session != &session) {
    output.status = IndependentSetLiveStatus::Inapplicable;
    return output;
  }
  if (!valid_cover_payload(cover_vertices) ||
      (session.retained() && !session.belongs_to(kernel))) {
    if (session.retained() && !session.belongs_to(kernel))
      output.status = IndependentSetLiveStatus::Stale;
    return output;
  }
  const uint64_t action_id = kernel.structural_action_sequence + 1;
  IndependentSetLiveRequest request =
      request_from_kernel(kernel, cover_vertices, action_id);
  IndependentSetOwnerSnapshot captured;
  const bool first = !session.retained();
  if (first) {
    const IndependentSetOwnerCaptureResult capture =
        independent_set_live_capture_owner_detailed(kernel, request, captured);
    if (!capture.captured()) {
      output.status = capture.status;
      copy_effort(output, capture);
      return output;
    }
    request.owner_snapshot = &captured;
    request.include_owner_capture_effort = true;
  } else {
    request.owner_snapshot = &session.snapshot;
  }

  const IndependentSetLiveResult live =
      independent_set_live_apply(kernel, request);
  output.status = live.status;
  output.result = live.result;
  output.vertices = live.vertices;
  output.target = live.target;
  output.published_clause_id = live.published_clause_id;
  copy_effort(output, live);
  if (live.status != IndependentSetLiveStatus::Applied ||
      live.result == ApplyResult::NoOp)
    return output;

  // All failure paths above leave the owner, action counter and candidate
  // transaction unchanged.  From here the live adapter has committed, so the
  // preflighted session bookkeeping is infallible or a programming error.
  if (first) {
    session.owner = &kernel;
    session.snapshot = std::move(captured);
  }
  output.owner_retained = true;
  output.action_id = output.semantic_id = action_id;
  if (live.result == ApplyResult::Ok) {
    std::vector<strategy::SemanticVariable> variables;
    variables.reserve(live.advice_vertex_literals.size());
    for (const int32_t literal : live.advice_vertex_literals) {
      if (literal <= 0)
        std::abort();
      variables.push_back(
          {static_cast<uint32_t>(literal),
           strategy::SemanticVariableRole::ExistingTheory, 1});
    }
    if (!strategy::stage_semantic_variable_candidate(
            kernel, strategy::SemanticVariableProducer::IndependentSet,
            action_id, variables))
      std::abort();
    output.candidate_staged = true;
  }
  kernel.structural_action_sequence = action_id;
  return output;
}

TACTIX_REGISTER_RULE(IndependentSetCover, RuleTier::Inprocess,
                     independent_set_cover_action_guard,
                     independent_set_cover_rule_apply);

} // namespace tactix
