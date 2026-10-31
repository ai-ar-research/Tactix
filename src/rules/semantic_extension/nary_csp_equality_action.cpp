#include "rules/semantic_extension/nary_csp_equality_action.hpp"

#include "strategy/action_status_mapping.hpp"

#include "rules/structural_action_guard.hpp"
#include "rules/semantic_extension/csp_domain_identity.hpp"
#include "rules/semantic_extension/csp_live_domain.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace tactix {
namespace {

template <class Charge>
bool binary_present(Kernel &kernel, Lit first, Lit second, Charge &charge,
                    bool &stopped) {
  if (!charge()) {
    stopped = true;
    return false;
  }
  if (kernel.watches.binary_size(second) < kernel.watches.binary_size(first))
    std::swap(first, second);
  const bool binaries_first = kernel.watches.heads[first].binaries_first();
  for (const WatchEntry watch : kernel.watches.entries(first)) {
    if (!charge()) {
      stopped = true;
      return false;
    }
    if (!watch.binary()) {
      if (binaries_first) break;
      continue;
    }
    if (watch.other() == second) return true;
  }
  return false;
}

template <class Charge>
bool complete_amo(Kernel &kernel, const std::vector<int32_t> &domain,
                  Charge &charge, bool &stopped) {
  for (size_t first = 0; first < domain.size(); ++first)
    for (size_t second = first + 1; second < domain.size(); ++second) {
      const Lit left = kernel_internalize(kernel, -domain[first]);
      const Lit right = kernel_internalize(kernel, -domain[second]);
      if (left == INVALID_LIT || right == INVALID_LIT ||
          !binary_present(kernel, left, right, charge, stopped))
        return false;
    }
  return true;
}

bool disjoint_domains(const std::vector<std::vector<int32_t>> &domains) {
  std::set<uint32_t> seen;
  for (const auto &domain : domains)
    for (int32_t literal : domain)
      if (!seen.insert(external_variable_of(literal)).second) return false;
  return true;
}

std::vector<std::vector<int32_t>> owner_domains(
    const NaryCspRelationOwnerSnapshot &owner) {
  std::vector<std::vector<int32_t>> domains;
  domains.reserve(owner.relation.domains.size());
  for (const auto &domain : owner.relation.domains)
    domains.push_back(domain.values);
  return domains;
}

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
    const NaryCspRelationOwnerSnapshot &owner, uint64_t &work,
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
    NaryCspRelationCaptureRequest capture;
    capture.domains = selection.domains;
    capture.maximum_source_clauses = limits.maximum_source_clauses;
    capture.maximum_source_literals = limits.maximum_source_literals;
    capture.maximum_recognition_work = limits.maximum_recognition_work;
    capture.maximum_domains = 3;
    capture.maximum_domain_values = limits.maximum_domain_values;
    capture.maximum_forbidden_tuples = limits.maximum_forbidden_tuples;
    capture.maximum_external_extent = limits.maximum_external_extent;
    const auto acquired = nary_csp_relation_prepare_owner(
        kernel, relation_session, capture,
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
    Kernel &kernel, const NaryCspRelationOwnerSession &relation_session,
    const NaryCspEqualitySession &equality_session,
    const NaryCspEqualityLimits &limits,
    const NaryCspEqualityActionOfferWindow &window) {
  NaryCspEqualityActionOfferResult result;
  if (!nary_csp_equality_action_guard(kernel)) {
    result.status = NaryCspEqualityActionOfferStatus::Inapplicable;
    return result;
  }
  if (!limits.valid() || !window.valid()) return result;
  if ((relation_session.retained() && !relation_session.belongs_to(kernel)) ||
      (equality_session.retained() && !equality_session.belongs_to(kernel))) {
    result.status = NaryCspEqualityActionOfferStatus::Stale;
    return result;
  }
  result.effort.retained_owners = relation_session.retained_owners();
  auto charge = [&](uint64_t amount = 1) {
    if (amount > window.maximum_enumeration_work -
                     std::min(window.maximum_enumeration_work,
                              result.effort.enumeration_work))
      return false;
    result.effort.enumeration_work += amount;
    return !kernel_terminating(kernel);
  };
  std::vector<NaryCspRelationOwnerSnapshot> owners =
      relation_session.snapshots();
  std::set<std::vector<std::vector<int32_t>>> freshly_captured;

  // Native equality discovery owns its relation census. It recognizes exact
  // one-hot domains and clause-backed ternary relations without asking X17 to
  // plan or publish a prefix consequence.
  std::set<std::vector<int32_t>> domain_candidates;
  bool stopped = false;
  auto observe_domain = [&](const Lit *literals, uint32_t size) {
    if (!charge(uint64_t{1} + size)) return false;
    std::vector<int32_t> domain;
    if (!csp_live_domain::canonicalize(kernel, literals, size,
                                       limits.maximum_domain_values, domain))
      return true;
    if (!complete_amo(kernel, domain, charge, stopped)) return !stopped;
    if (domain_candidates.contains(domain)) return true;
    if (domain_candidates.size() == window.maximum_owner_candidates)
      return false;
    domain_candidates.insert(std::move(domain));
    return true;
  };
  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
    if (!charge()) {
      stopped = true;
      break;
    }
    if (!clause->garbage() &&
        !observe_domain(clause->lits, clause->size)) {
      stopped = true;
      break;
    }
  }
  if (!stopped)
    for (Lit first = 0; first < 2 * kernel.nvars && !stopped; ++first)
      for (const WatchEntry watch : kernel.watches.entries(first)) {
        if (!charge()) {
          stopped = true;
          break;
        }
        if (!watch.binary() || first >= watch.other()) continue;
        const Lit pair[] = {first, watch.other()};
        if (!observe_domain(pair, 2)) {
          stopped = true;
          break;
        }
      }
  if (stopped) {
    result.status = kernel_terminating(kernel)
                        ? NaryCspEqualityActionOfferStatus::Interrupted
                        : NaryCspEqualityActionOfferStatus::ResourceExhausted;
    return result;
  }

  std::vector<std::vector<int32_t>> domains(domain_candidates.begin(),
                                             domain_candidates.end());
  std::vector<uint32_t> variable_occurrences(kernel.nvars, 0);
  for (const auto &domain : domains)
    for (int32_t external : domain) {
      const Lit literal = kernel_internalize(kernel, external);
      if (literal == INVALID_LIT) continue;
      ++variable_occurrences[var_of(literal)];
    }
  std::map<int32_t, nary_csp_relation::DomainValue> tuple_literals;
  for (uint32_t domain = 0; domain < domains.size(); ++domain)
    for (uint32_t value = 0; value < domains[domain].size(); ++value) {
      const int32_t external = domains[domain][value];
      const Lit literal = kernel_internalize(kernel, external);
      if (literal != INVALID_LIT &&
          variable_occurrences[var_of(literal)] == 1)
        tuple_literals.emplace(
            -external, nary_csp_relation::DomainValue{domain, value});
    }
  std::set<std::vector<uint32_t>> relation_indices;
  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
    if (!charge()) {
      stopped = true;
      break;
    }
    if (clause->garbage() || clause->size != 3) continue;
    if (!charge(uint64_t{1} + clause->size)) {
      stopped = true;
      break;
    }
    std::vector<uint32_t> relation;
    relation.reserve(3);
    for (uint32_t index = 0; index < clause->size; ++index) {
      const Lit literal = clause->lits[index];
      const auto found = tuple_literals.find(kernel_externalize(kernel, literal));
      if (found == tuple_literals.end()) {
        relation.clear();
        break;
      }
      relation.push_back(found->second.domain);
    }
    if (relation.size() != 3) continue;
    std::sort(relation.begin(), relation.end());
    if (std::adjacent_find(relation.begin(), relation.end()) != relation.end())
      continue;
    if (!relation_indices.contains(relation) &&
        relation_indices.size() == window.maximum_owner_candidates) {
      stopped = true;
      break;
    }
    relation_indices.insert(std::move(relation));
  }
  if (stopped) {
    result.status = kernel_terminating(kernel)
                        ? NaryCspEqualityActionOfferStatus::Interrupted
                        : NaryCspEqualityActionOfferStatus::ResourceExhausted;
    return result;
  }
  for (const auto &indices : relation_indices) {
    std::vector<std::vector<int32_t>> relation_domains;
    for (uint32_t index : indices) relation_domains.push_back(domains[index]);
    if (!disjoint_domains(relation_domains) ||
        relation_session.snapshot(relation_domains))
      continue;
    const uint64_t remaining = window.maximum_enumeration_work -
                               result.effort.enumeration_work;
    if (!remaining) {
      result.status = NaryCspEqualityActionOfferStatus::ResourceExhausted;
      return result;
    }
    NaryCspRelationCaptureRequest capture{
        relation_domains,
        limits.maximum_source_clauses,
        limits.maximum_source_literals,
        std::min(limits.maximum_recognition_work, remaining),
        3,
        limits.maximum_domain_values,
        limits.maximum_forbidden_tuples,
        limits.maximum_external_extent};
    NaryCspRelationOwnerSnapshot owner;
    const auto captured =
        nary_csp_relation_capture_owner(kernel, capture, owner);
    if (!charge(captured.recognition_work)) {
      result.status = kernel_terminating(kernel)
                          ? NaryCspEqualityActionOfferStatus::Interrupted
                          : NaryCspEqualityActionOfferStatus::ResourceExhausted;
      return result;
    }
    if (captured.status == NaryCspRelationCaptureStatus::Interrupted) {
      result.status = NaryCspEqualityActionOfferStatus::Interrupted;
      return result;
    }
    if (captured.status == NaryCspRelationCaptureStatus::ResourceExhausted) {
      result.status = NaryCspEqualityActionOfferStatus::ResourceExhausted;
      return result;
    }
    if (captured.status != NaryCspRelationCaptureStatus::Applied) continue;
    freshly_captured.insert(relation_domains);
    owners.push_back(std::move(owner));
  }
  std::sort(owners.begin(), owners.end(), [](const auto &left,
                                             const auto &right) {
    return owner_domains(left) < owner_domains(right);
  });
  owners.erase(std::unique(owners.begin(), owners.end(), [](const auto &left,
                                                            const auto &right) {
                 return owner_domains(left) == owner_domains(right);
               }),
               owners.end());

  for (const auto &owner : owners) {
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
    const auto domains_key = owner_domains(owner);
    if (freshly_captured.contains(domains_key)) {
      // This exact source image was captured during the bounded census above.
    } else {
    NaryCspRelationCaptureRequest capture_request;
    for (const auto &domain : owner.relation.domains)
      capture_request.domains.push_back(domain.values);
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
    capture_request.maximum_external_extent = limits.maximum_external_extent;
    NaryCspRelationOwnerSnapshot current;
    const auto captured = nary_csp_relation_capture_owner(
        kernel, capture_request, current);
    if (!charge(captured.recognition_work)) {
      result.status = kernel_terminating(kernel)
                          ? NaryCspEqualityActionOfferStatus::Interrupted
                          : NaryCspEqualityActionOfferStatus::ResourceExhausted;
      result.offers.clear();
      return result;
    }
    if (captured.status != NaryCspRelationCaptureStatus::Applied) {
      switch (captured.status) {
      case NaryCspRelationCaptureStatus::Interrupted:
        result.status = NaryCspEqualityActionOfferStatus::Interrupted; break;
      case NaryCspRelationCaptureStatus::ResourceExhausted:
        result.status = NaryCspEqualityActionOfferStatus::ResourceExhausted; break;
      case NaryCspRelationCaptureStatus::Inapplicable:
        result.status = NaryCspEqualityActionOfferStatus::Inapplicable; break;
      case NaryCspRelationCaptureStatus::NoStructure:
      case NaryCspRelationCaptureStatus::Stale:
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
