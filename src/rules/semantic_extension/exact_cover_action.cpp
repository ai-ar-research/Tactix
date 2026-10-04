#include "rules/semantic_extension/exact_cover_action.hpp"

#include "strategy/semantic_variable_advice.hpp"

#include <cstdlib>
#include <limits>
#include <vector>

namespace tactix {

RuleEligibility exact_cover_extension_action_guard(const Kernel &k) {
  return RuleEligibility{}
      .fact("consistent", !k.inconsistent)
      .fact("root", !k.level)
      .fact("assignment-free", k.trail.empty())
      .fact("no-conflict", !k.conflict_valid && !k.pending.valid)
      .fact("propagated", kernel_propagation_fixpoint(k))
      .fact("sparse", !k.dense && k.large_watches_connected)
      .fact("no-probe-transaction", !k.probing.recording)
      .fact("session-attached",
            k.exact_cover_extension_action_session != nullptr)
      .fact("provenance-bound", k.structural_provenance != nullptr)
      .fact("action-id-available",
            k.structural_action_sequence != UINT64_MAX)
      .fact("clausal-certificate",
            !k.proof || (!k.proof->lrat() && !k.proof->pb()))
      .fact("candidate-slot-empty", !k.semantic_variable_candidate_pending)
      .fact("candidate-id-available",
            k.semantic_variable_publication != (UINT64_MAX >> 8));
}

ExactCoverExtensionActionResult exact_cover_extension_action_apply(
    Kernel &k, ExactCoverExtensionActionSession &session) {
  ExactCoverExtensionActionResult result;
  if (!exact_cover_extension_action_guard(k) ||
      k.exact_cover_extension_action_session != &session) {
    result.live.status = ExactCoverLiveStatus::Inapplicable;
    return result;
  }

  const auto &parameters = k.exact_cover_extension_parameters;
  if (!parameters.valid())
    return result;
  const uint64_t prospective_action_id =
      k.structural_action_sequence + 1;

  ExactCoverLiveRequest request;
  request.arity = parameters.arity;
  request.maximum_items = parameters.maximum_items;
  request.maximum_source_clauses = parameters.maximum_source_clauses;
  request.maximum_source_literals = parameters.maximum_source_literals;
  request.maximum_recognition_work = parameters.maximum_recognition_work;
  request.maximum_premise_references =
      parameters.maximum_premise_references;
  request.maximum_definitions = parameters.maximum_definitions;
  request.maximum_additions = parameters.maximum_additions;
  request.maximum_literal_occurrences =
      parameters.maximum_literal_occurrences;
  request.maximum_journal_bytes = parameters.maximum_journal_bytes;
  request.maximum_extent_growth = parameters.maximum_extent_growth;
  request.maximum_external_extent = parameters.maximum_external_extent;
  request.action_id = prospective_action_id;
  request.records = k.structural_provenance;
  request.session = &session.live_session;

  result.live = exact_cover_live_apply(k, request);
  if (result.live.status != ExactCoverLiveStatus::Applied ||
      result.live.result == ApplyResult::NoOp)
    return result;

  if (result.live.result == ApplyResult::Ok) {
    std::vector<strategy::SemanticVariable> candidates;
    candidates.reserve(result.live.introduced_theory_variables.size());
    for (const uint32_t external : result.live.introduced_theory_variables)
      candidates.push_back({external,
                            strategy::SemanticVariableRole::IntroducedTheory,
                            1});
    // Publication already committed. Guard and the live adapter guarantee the
    // staging preconditions; failure here is an internal atomicity violation.
    if (!strategy::stage_semantic_variable_candidate(
            k, strategy::SemanticVariableProducer::ExactCover,
            prospective_action_id, candidates))
      std::abort();
    result.semantic_candidate_staged = true;
  }

  k.structural_action_sequence = prospective_action_id;
  result.action_id = prospective_action_id;
  return result;
}

ApplyResult exact_cover_extension_rule_apply(Kernel &kernel) {
  if (!exact_cover_extension_action_guard(kernel)) return ApplyResult::NoOp;
  return exact_cover_extension_action_apply(
             kernel, *kernel.exact_cover_extension_action_session)
      .live.result;
}

} // namespace tactix
