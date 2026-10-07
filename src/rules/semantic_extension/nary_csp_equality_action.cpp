#include "rules/semantic_extension/nary_csp_equality_action.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <limits>
#include <set>
#include <utility>

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

NaryCspEqualityKey key_of(const NaryCspEqualityActionSelection &selection) {
  NaryCspEqualityKey key;
  key.domains = selection.domains;
  key.first_equal_domain = std::min(selection.coordinates.left_domain,
                                    selection.coordinates.right_domain);
  key.second_equal_domain = std::max(selection.coordinates.left_domain,
                                     selection.coordinates.right_domain);
  key.residual_domain = selection.coordinates.residual_domain;
  return key;
}

enum class DiagonalCheck : uint8_t { Missing, Complete, Exhausted };

DiagonalCheck owner_has_all_diagonal_tuples(
    const NaryCspPrefixOwnerSnapshot &owner, uint64_t &work,
    uint64_t maximum) {
  if (owner.relation.domains.size() != 3) return DiagonalCheck::Missing;
  const size_t values = owner.relation.domains.front().values.size();
  if (values < 2) return DiagonalCheck::Missing;
  for (const auto &domain : owner.relation.domains)
    if (domain.values.size() != values) return DiagonalCheck::Missing;
  std::set<std::vector<uint32_t>> tuples;
  for (const auto &tuple : owner.relation.forbidden) {
    if (work == maximum) return DiagonalCheck::Exhausted;
    ++work;
    if (tuple.value_indices.size() != 3) return DiagonalCheck::Missing;
    tuples.insert(tuple.value_indices);
  }
  for (uint32_t value = 0; value < values; ++value) {
    if (work == maximum) return DiagonalCheck::Exhausted;
    ++work;
    if (!tuples.contains({value, value, value}))
      return DiagonalCheck::Missing;
  }
  return DiagonalCheck::Complete;
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

RuleEligibility nary_csp_equality_action_guard(const Kernel &kernel) {
  return nary_csp_prefix_action_guard(kernel);
}

NaryCspEqualityActionResult nary_csp_equality_action_apply(
    Kernel &kernel, const NaryCspPrefixActionSession &prefix_session,
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

bool NaryCspEqualityActionOfferWindow::valid() const {
  return maximum_owner_candidates && maximum_coordinate_candidates &&
         maximum_offers && maximum_enumeration_work &&
         (!have_after || (after.domains.size() == 3 &&
                          after.first_equal_domain < after.second_equal_domain &&
                          after.second_equal_domain < 3 &&
                          after.residual_domain < 3 &&
                          after.residual_domain != after.first_equal_domain &&
                          after.residual_domain != after.second_equal_domain));
}

NaryCspEqualityActionOfferResult nary_csp_equality_action_offer(
    Kernel &kernel, const NaryCspPrefixActionSession &prefix_session,
    const NaryCspEqualitySession &equality_session,
    const NaryCspEqualityLimits &limits,
    const NaryCspEqualityActionOfferWindow &window) {
  NaryCspEqualityActionOfferResult result;
  if (!nary_csp_equality_action_guard(kernel)) {
    result.status = NaryCspEqualityActionOfferStatus::Inapplicable;
    return result;
  }
  if (!limits.valid() || !window.valid()) return result;
  if (!prefix_session.belongs_to(kernel) ||
      (equality_session.retained() && !equality_session.belongs_to(kernel))) {
    result.status = NaryCspEqualityActionOfferStatus::Stale;
    return result;
  }
  result.effort.retained_owners = prefix_session.retained_owners();
  auto charge = [&](uint64_t amount = 1) {
    if (amount > window.maximum_enumeration_work -
                     std::min(window.maximum_enumeration_work,
                              result.effort.enumeration_work))
      return false;
    result.effort.enumeration_work += amount;
    return !kernel_terminating(kernel);
  };
  for (const auto &owner : prefix_session.snapshots()) {
    if (result.effort.owner_candidates == window.maximum_owner_candidates ||
        !charge()) {
      result.status = kernel_terminating(kernel)
                          ? NaryCspEqualityActionOfferStatus::Interrupted
                          : NaryCspEqualityActionOfferStatus::ResourceExhausted;
      result.offers.clear();
      return result;
    }
    ++result.effort.owner_candidates;
    uint64_t validation_work = result.effort.enumeration_work;
    const DiagonalCheck diagonal = owner_has_all_diagonal_tuples(
        owner, validation_work, window.maximum_enumeration_work);
    if (diagonal != DiagonalCheck::Complete) {
      if (diagonal == DiagonalCheck::Exhausted) {
        result.effort.enumeration_work = validation_work;
        result.status = NaryCspEqualityActionOfferStatus::ResourceExhausted;
        result.offers.clear();
        return result;
      }
      result.effort.enumeration_work = validation_work;
      continue;
    }
    result.effort.enumeration_work = validation_work;
    if (owner.relation.domains.front().values.size() >
        limits.maximum_domain_values)
      continue;
    // Retention alone does not authenticate the current root: search may
    // assign a domain value or remove a premise. Revalidate before offering
    // coordinates, and charge the census to this caller's page allowance.
    NaryCspPrefixLiveRequest capture_request;
    for (const auto &domain : owner.relation.domains)
      capture_request.domains.push_back(domain.values);
    capture_request.selection = {2, {{0, 0}, {1, 0}}};
    capture_request.maximum_source_clauses = limits.maximum_source_clauses;
    capture_request.maximum_source_literals = limits.maximum_source_literals;
    capture_request.maximum_recognition_work = std::min(
        limits.maximum_recognition_work,
        window.maximum_enumeration_work - result.effort.enumeration_work);
    if (!capture_request.maximum_recognition_work) {
      result.status = NaryCspEqualityActionOfferStatus::ResourceExhausted;
      result.offers.clear();
      return result;
    }
    capture_request.maximum_domains = 3;
    capture_request.maximum_domain_values = limits.maximum_domain_values;
    capture_request.maximum_forbidden_tuples = limits.maximum_forbidden_tuples;
    capture_request.maximum_prefix_arity = 2;
    capture_request.maximum_definition_clause_literals = 3;
    capture_request.maximum_proof_additions = limits.maximum_proof_additions;
    capture_request.maximum_proof_literal_occurrences =
        limits.maximum_proof_literal_occurrences;
    capture_request.maximum_external_extent = limits.maximum_external_extent;
    NaryCspPrefixOwnerSnapshot current;
    const auto captured = nary_csp_prefix_live_capture_owner(
        kernel, capture_request, current);
    if (!charge(captured.recognition_work)) {
      result.status = kernel_terminating(kernel)
                          ? NaryCspEqualityActionOfferStatus::Interrupted
                          : NaryCspEqualityActionOfferStatus::ResourceExhausted;
      result.offers.clear();
      return result;
    }
    if (captured.status != NaryCspPrefixLiveStatus::Applied) {
      switch (captured.status) {
      case NaryCspPrefixLiveStatus::Interrupted:
        result.status = NaryCspEqualityActionOfferStatus::Interrupted; break;
      case NaryCspPrefixLiveStatus::ResourceExhausted:
        result.status = NaryCspEqualityActionOfferStatus::ResourceExhausted; break;
      case NaryCspPrefixLiveStatus::Inapplicable:
        result.status = NaryCspEqualityActionOfferStatus::Inapplicable; break;
      case NaryCspPrefixLiveStatus::NoStructure:
      case NaryCspPrefixLiveStatus::Stale:
        result.status = NaryCspEqualityActionOfferStatus::Stale; break;
      default:
        result.status = NaryCspEqualityActionOfferStatus::Invalid; break;
      }
      result.offers.clear();
      return result;
    }
    current.capture_work = owner.capture_work;
    current.external_extent = owner.external_extent;
    if (current != owner) {
      result.status = NaryCspEqualityActionOfferStatus::Stale;
      result.offers.clear();
      return result;
    }
    std::vector<std::vector<int32_t>> domains;
    for (const auto &domain : owner.relation.domains)
      domains.push_back(domain.values);
    for (uint32_t residual = 0; residual != 3; ++residual) {
      if (result.effort.coordinate_candidates ==
              window.maximum_coordinate_candidates ||
          !charge()) {
        result.status = kernel_terminating(kernel)
                            ? NaryCspEqualityActionOfferStatus::Interrupted
                            : NaryCspEqualityActionOfferStatus::ResourceExhausted;
        result.offers.clear();
        return result;
      }
      ++result.effort.coordinate_candidates;
      uint32_t first = residual == 0 ? 1 : 0;
      uint32_t second = residual == 2 ? 1 : 2;
      NaryCspEqualityActionSelection selection{
          domains, {first, second, residual}};
      if (!nary_csp_equality_action_selection_within_limits(selection, limits))
        continue;
      const NaryCspEqualityKey key = key_of(selection);
      if (window.have_after && !(window.after < key)) continue;
      if (std::binary_search(equality_session.keys().begin(),
                             equality_session.keys().end(), key)) {
        ++result.effort.committed_keys_skipped;
        continue;
      }
      if (result.offers.size() == window.maximum_offers) {
        result.status = NaryCspEqualityActionOfferStatus::Ready;
        result.complete = false;
        return result;
      }
      result.offers.push_back(std::move(selection));
      result.have_resume_after = true;
      result.resume_after = key;
    }
  }
  result.status = NaryCspEqualityActionOfferStatus::Ready;
  result.complete = true;
  return result;
}

} // namespace tactix
