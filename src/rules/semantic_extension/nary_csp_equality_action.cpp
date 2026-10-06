#include "rules/semantic_extension/nary_csp_equality_action.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <limits>

namespace tactix {
namespace {

uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

bool literal_less(int32_t first, int32_t second) {
  return std::pair(variable(first), first) <
         std::pair(variable(second), second);
}

NaryCspEqualityActionStatus live_status(NaryCspEqualityLiveStatus status) {
  switch (status) {
  case NaryCspEqualityLiveStatus::Invalid:
    return NaryCspEqualityActionStatus::Invalid;
  case NaryCspEqualityLiveStatus::Inapplicable:
    return NaryCspEqualityActionStatus::Inapplicable;
  case NaryCspEqualityLiveStatus::NoConsequence:
    return NaryCspEqualityActionStatus::NoConsequence;
  case NaryCspEqualityLiveStatus::ResourceExhausted:
    return NaryCspEqualityActionStatus::ResourceExhausted;
  case NaryCspEqualityLiveStatus::Interrupted:
    return NaryCspEqualityActionStatus::Interrupted;
  case NaryCspEqualityLiveStatus::UnsupportedProof:
    return NaryCspEqualityActionStatus::UnsupportedProof;
  case NaryCspEqualityLiveStatus::Stale:
    return NaryCspEqualityActionStatus::Stale;
  case NaryCspEqualityLiveStatus::Duplicate:
    return NaryCspEqualityActionStatus::AlreadyPublished;
  case NaryCspEqualityLiveStatus::Applied:
    return NaryCspEqualityActionStatus::Applied;
  }
  return NaryCspEqualityActionStatus::Invalid;
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

  const size_t values = selection.domains.front().size();
  if (values < 2) return false;
  std::vector<uint32_t> variables;
  for (size_t domain = 0; domain < selection.domains.size(); ++domain) {
    const auto &literals = selection.domains[domain];
    if (literals.size() != values ||
        (domain && !(selection.domains[domain - 1] < literals)))
      return false;
    for (size_t index = 0; index < literals.size(); ++index) {
      const int32_t literal = literals[index];
      if (!literal || literal == std::numeric_limits<int32_t>::min() ||
          (index && !literal_less(literals[index - 1], literal)))
        return false;
      variables.push_back(variable(literal));
    }
  }
  std::sort(variables.begin(), variables.end());
  return std::adjacent_find(variables.begin(), variables.end()) ==
         variables.end();
}

bool nary_csp_equality_action_selection_within_limits(
    const NaryCspEqualityActionSelection &selection,
    const NaryCspEqualityLimits &limits) {
  return limits.valid() &&
         nary_csp_equality_action_selection_valid(selection) &&
         selection.domains.front().size() <= limits.maximum_domain_values;
}

NaryCspEqualityActionResult nary_csp_equality_action_apply(
    Kernel &kernel, const NaryCspPrefixActionSession &prefix_session,
    NaryCspEqualitySession &equality_session,
    const NaryCspEqualityLimits &limits,
    const NaryCspEqualityActionSelection &selection) {
  NaryCspEqualityActionResult result;
  if (!limits.valid() ||
      !nary_csp_equality_action_selection_within_limits(selection, limits) ||
      kernel.structural_action_sequence == UINT64_MAX)
    return result;
  if (!prefix_session.belongs_to(kernel) ||
      (equality_session.retained() && !equality_session.belongs_to(kernel)) ||
      !kernel.structural_provenance) {
    result.status = NaryCspEqualityActionStatus::Inapplicable;
    return result;
  }
  const NaryCspPrefixOwnerSnapshot *owner =
      prefix_session.snapshot(selection.domains);
  if (!owner) {
    result.status = NaryCspEqualityActionStatus::NoStructure;
    return result;
  }

  result.proposed_action_id = kernel.structural_action_sequence + 1;
  result.semantic_id = result.proposed_action_id;
  NaryCspEqualityRequest request;
  request.owner = owner;
  request.selection = selection.coordinates;
  request.limits = limits;
  request.action_id = result.proposed_action_id;
  request.semantic_id = result.semantic_id;
  request.records = kernel.structural_provenance;
  result.live = nary_csp_equality_live_apply(
      kernel, equality_session, request);
  result.status = live_status(result.live.status);
  result.result = result.live.result;
  result.semantic_candidate_ready = result.live.semantic_candidate_ready;
  result.semantic_variables = result.live.semantic_variables;
  if (result.status == NaryCspEqualityActionStatus::Applied) {
    kernel.structural_action_sequence = result.proposed_action_id;
    result.committed_action_id = result.proposed_action_id;
  }
  return result;
}

} // namespace tactix
