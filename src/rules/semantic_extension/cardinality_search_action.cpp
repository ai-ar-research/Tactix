#include "rules/semantic_extension/cardinality_search_action.hpp"

#include "proof/proof.hpp"
#include "rules/structural_action_guard.hpp"

#include <climits>

namespace tactix {

bool CardinalitySearchActionLimits::valid() const {
  return inspection.maximum_support_variables &&
         inspection.maximum_support_variables <= 20 &&
         inspection.maximum_constraints &&
         inspection.maximum_source_clauses &&
         inspection.maximum_reason_width && inspection.maximum_work &&
         resolution.maximum_work && resolution.maximum_additions &&
         resolution.maximum_added_literals && maximum_source_scan_work &&
         maximum_arena_words && maximum_journal_bytes;
}

RuleEligibility cardinality_search_action_guard(const Kernel &kernel) {
  return action_guard(kernel).identity().clausal_certificate().finish();
}

CardinalitySearchActionResult cardinality_search_action_apply(
    Kernel &kernel, const std::vector<CardinalitySearchBound> &bounds,
    const CardinalitySearchActionLimits &limits) {
  CardinalitySearchActionResult output;
  if (!cardinality_search_action_guard(kernel)) {
    output.status = CardinalitySearchActionStatus::Inapplicable;
    return output;
  }
  if (!limits.valid() || bounds.empty()) return output;
  ++kernel.stats.cardinality_search_attempts;
  if (bounds.size() > limits.inspection.maximum_constraints) {
    output.status = CardinalitySearchActionStatus::ResourceExhausted;
    return output;
  }
  CardinalitySearchLiveRequest request;
  request.inspection = limits.inspection;
  request.resolution = limits.resolution;
  request.maximum_source_scan_work = limits.maximum_source_scan_work;
  request.maximum_arena_words = limits.maximum_arena_words;
  request.maximum_journal_bytes = limits.maximum_journal_bytes;
  request.action_id = kernel.structural_action_sequence + 1;
  request.semantic_id = request.action_id;
  request.records = kernel.structural_provenance;
  for (const CardinalitySearchBound &bound : bounds) {
    auto constructed = cardinality_search::construct(
        bound.literals, bound.lower, bound.upper, limits.inspection);
    output.construction_work += constructed.work;
    if (constructed.status ==
        cardinality_search::ConstructStatus::ResourceExhausted) {
      output.status = CardinalitySearchActionStatus::ResourceExhausted;
      return output;
    }
    if (constructed.status !=
        cardinality_search::ConstructStatus::Constructed)
      return output;
    request.constraints.push_back(std::move(constructed.constraint));
  }
  output.proposed_action_id = request.action_id;
  output.live = cardinality_search_live_apply(kernel, request);
  output.result = output.live.result;
  switch (output.live.status) {
  case CardinalitySearchLiveStatus::Invalid:
    output.status = CardinalitySearchActionStatus::Invalid;
    break;
  case CardinalitySearchLiveStatus::Inapplicable:
    output.status = CardinalitySearchActionStatus::Inapplicable;
    break;
  case CardinalitySearchLiveStatus::UnsupportedProof:
    output.status = CardinalitySearchActionStatus::UnsupportedProof;
    break;
  case CardinalitySearchLiveStatus::NoSource:
    output.status = CardinalitySearchActionStatus::NoSource;
    break;
  case CardinalitySearchLiveStatus::NoEvent:
    output.status = CardinalitySearchActionStatus::NoEvent;
    break;
  case CardinalitySearchLiveStatus::ResourceExhausted:
    output.status = CardinalitySearchActionStatus::ResourceExhausted;
    break;
  case CardinalitySearchLiveStatus::Interrupted:
    output.status = CardinalitySearchActionStatus::Interrupted;
    break;
  case CardinalitySearchLiveStatus::Applied:
    output.status = CardinalitySearchActionStatus::Applied;
    ++kernel.stats.cardinality_search_applied;
    kernel.structural_action_sequence = request.action_id;
    output.committed_action_id = request.action_id;
    break;
  }
  return output;
}

} // namespace tactix
