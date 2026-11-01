#include "rules/semantic_extension/nary_csp_equality_action.hpp"

#include "strategy/action_status_mapping.hpp"

#include "rules/structural_action_guard.hpp"
#include "rules/semantic_extension/csp_domain_identity.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <utility>

namespace tactix {
namespace {

NaryCspEqualityActionStatus live_status(NaryCspEqualityLiveStatus status) {
  if (const auto failure = strategy::map_standard_action_failure<
          NaryCspEqualityActionStatus>(status))
    return *failure;
  switch (status) {
  case NaryCspEqualityLiveStatus::NoConsequence:
    return NaryCspEqualityActionStatus::NoConsequence;
  case NaryCspEqualityLiveStatus::Duplicate:
    return NaryCspEqualityActionStatus::AlreadyPublished;
  case NaryCspEqualityLiveStatus::Applied:
    return NaryCspEqualityActionStatus::Applied;
  default: return NaryCspEqualityActionStatus::Invalid;
  }
}
} // namespace

bool nary_csp_equality_action_selection_valid(
    const NaryCspEqualityActionSelection &selection) {
  if (selection.domains.size() != 3) return false;
  const auto &coordinates = selection.coordinates;
  std::array<uint32_t, 3> permutation{
      coordinates.left_domain, coordinates.right_domain,
      coordinates.residual_domain};
  std::sort(permutation.begin(), permutation.end());
  if (permutation != std::array<uint32_t, 3>{0, 1, 2}) return false;

  return csp_domain_identity::ordered_three_valid(selection.domains, 2);
}

bool nary_csp_equality_action_selection_within_limits(
    const NaryCspEqualityActionSelection &selection,
    const NaryCspEqualityLimits &limits) {
  return limits.valid() &&
         nary_csp_equality_action_selection_valid(selection) &&
         selection.domains.front().size() <= limits.maximum_domain_values;
}

RuleEligibility nary_csp_equality_action_guard(const Kernel &kernel) {
  return extend_action_guard(kernel, nary_csp_relation_owner_guard(kernel))
      .identity()
      .semantic_candidate()
      .clausal_certificate()
      .finish();
}

NaryCspEqualityActionResult nary_csp_equality_action_apply(
    Kernel &kernel, NaryCspRelationOwnerSession &relation_session,
    NaryCspEqualitySession &equality_session,
    const NaryCspEqualityLimits &limits,
    const NaryCspEqualityActionSelection &selection) {
  NaryCspEqualityActionResult result;
  if (!nary_csp_equality_action_guard(kernel)) {
    result.status = NaryCspEqualityActionStatus::Inapplicable;
    return result;
  }
  if (!limits.valid() ||
      !nary_csp_equality_action_selection_within_limits(selection, limits) ||
      kernel.structural_action_sequence == UINT64_MAX)
    return result;
  if ((relation_session.retained() && !relation_session.belongs_to(kernel)) ||
      (equality_session.retained() && !equality_session.belongs_to(kernel)) ||
      !kernel.structural_provenance) {
    result.status = NaryCspEqualityActionStatus::Inapplicable;
    return result;
  }
  const NaryCspRelationOwnerSnapshot *owner =
      relation_session.snapshot(selection.domains);
  NaryCspRelationOwnerSession staged_owner;
  uint64_t acquisition_work = 0;
  bool owner_freshly_captured = false;
  if (!owner) {
    const auto acquired = nary_csp_relation_prepare_owner(
        kernel, relation_session,
        nary_csp_ternary_capture_request(selection.domains, limits),
        {limits.maximum_session_keys, limits.maximum_recognition_work},
        staged_owner);
    acquisition_work = acquired.recognition_work;
    switch (acquired.status) {
    case NaryCspRelationCaptureStatus::Applied:
      owner = staged_owner.snapshot(selection.domains);
      owner_freshly_captured = true;
      break;
    case NaryCspRelationCaptureStatus::NoStructure:
      result.status = NaryCspEqualityActionStatus::NoStructure;
      return result;
    case NaryCspRelationCaptureStatus::ResourceExhausted:
      result.status = NaryCspEqualityActionStatus::ResourceExhausted;
      return result;
    case NaryCspRelationCaptureStatus::Interrupted:
      result.status = NaryCspEqualityActionStatus::Interrupted;
      return result;
    case NaryCspRelationCaptureStatus::Inapplicable:
      result.status = NaryCspEqualityActionStatus::Inapplicable;
      return result;
    case NaryCspRelationCaptureStatus::Stale:
      result.status = NaryCspEqualityActionStatus::Stale;
      return result;
    case NaryCspRelationCaptureStatus::Invalid:
    case NaryCspRelationCaptureStatus::Duplicate:
      return result;
    }
    if (!owner) return result;
  }

  result.proposed_action_id = kernel.structural_action_sequence + 1;
  result.semantic_id = result.proposed_action_id;
  NaryCspEqualityRequest request;
  request.owner = owner;
  request.owner_freshly_captured = owner_freshly_captured;
  request.selection = selection.coordinates;
  request.limits = limits;
  request.limits.maximum_recognition_work -=
      std::min(request.limits.maximum_recognition_work, acquisition_work);
  if (!request.limits.maximum_recognition_work) {
    result.status = NaryCspEqualityActionStatus::ResourceExhausted;
    return result;
  }
  request.action_id = result.proposed_action_id;
  request.semantic_id = result.semantic_id;
  request.records = kernel.structural_provenance;
  result.live = nary_csp_equality_live_apply(
      kernel, equality_session, request);
  result.live.recognition_work += acquisition_work;
  result.status = live_status(result.live.status);
  result.result = result.live.result;
  result.semantic_candidate_ready = result.live.semantic_candidate_ready;
  result.semantic_variables = result.live.semantic_variables;
  if (result.status == NaryCspEqualityActionStatus::Applied) {
    if (owner_freshly_captured) relation_session = std::move(staged_owner);
    kernel.structural_action_sequence = result.proposed_action_id;
    result.committed_action_id = result.proposed_action_id;
  }
  return result;
}
} // namespace tactix
