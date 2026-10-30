#include "rules/semantic_extension/nary_csp_equality.hpp"

#include "kernel/types.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace tactix::nary_csp_equality {
namespace {

uint32_t variable(int32_t literal) {
  return external_variable_of(literal);
}

bool literal_less(int32_t first, int32_t second) {
  return std::pair(variable(first), first) <
         std::pair(variable(second), second);
}

bool valid_literals(const std::vector<int32_t> &literals) {
  if (literals.empty()) return false;
  for (int32_t literal : literals)
    if (!literal || literal == INT32_MIN) return false;
  for (size_t i = 1; i < literals.size(); ++i)
    if (!literal_less(literals[i - 1], literals[i]) ||
        variable(literals[i - 1]) == variable(literals[i]))
      return false;
  return true;
}

std::vector<int32_t> normalized(std::vector<int32_t> clause) {
  std::sort(clause.begin(), clause.end());
  return clause;
}

bool safe_product(size_t first, size_t second, size_t &result) {
  if (first && second > std::numeric_limits<size_t>::max() / first)
    return false;
  result = first * second;
  return true;
}

bool add_count(uint64_t amount, uint64_t &total) {
  if (amount > UINT64_MAX - total) return false;
  total += amount;
  return true;
}

struct Snapshot {
  std::map<uint64_t, const Premise *> by_occurrence;
  uint64_t literals = 0;
  uint32_t maximum_variable = 0;
};

} // namespace

Plan plan_equality(const std::vector<Premise> &source,
                   const Relation &relation, const Selection &selection,
                   const Bounds &bounds, Charge charge) {
  Plan result;
  result.selection = selection;
  auto decline = [&](Status status) {
    result.status = status;
    result.diagonal_tuple_premises.clear();
    result.additions.clear();
    result.projection = {};
    return result;
  };
  auto spend = [&](uint64_t amount) {
    if (!charge || !charge(amount)) return false;
    return add_count(amount, result.consumed_work);
  };

  std::array<uint32_t, 3> coordinates{selection.left_domain,
                                      selection.right_domain,
                                      selection.residual_domain};
  std::sort(coordinates.begin(), coordinates.end());
  if (!bounds.maximum_source_clauses || !bounds.maximum_source_literals ||
      bounds.maximum_domain_values < 2 ||
      !bounds.maximum_forbidden_tuples || !bounds.maximum_additions ||
      !bounds.maximum_addition_literals || bounds.extension_variable <= 0 ||
      source.empty() || relation.domains.size() != 3 ||
      coordinates != std::array<uint32_t, 3>{0, 1, 2} ||
      relation.forbidden.empty())
    return decline(Status::InvalidRequest);
  if (source.size() > bounds.maximum_source_clauses ||
      relation.forbidden.size() > bounds.maximum_forbidden_tuples)
    return decline(Status::BoundExceeded);

  const size_t values = relation.domains.front().values.size();
  if (values < 2 || values > bounds.maximum_domain_values)
    return decline(Status::BoundExceeded);
  std::set<uint32_t> domain_variables;
  for (const Domain &domain : relation.domains) {
    if (domain.values.size() != values || !valid_literals(domain.values))
      return decline(Status::InvalidRequest);
    for (int32_t literal : domain.values)
      if (!domain_variables.insert(variable(literal)).second)
        return decline(Status::InvalidRequest);
  }

  Snapshot snapshot;
  for (const Premise &premise : source) {
    if (!spend(uint64_t{1} + premise.literals.size()))
      return decline(Status::EffortExhausted);
    if (!premise.occurrence || premise.literals.empty() ||
        !snapshot.by_occurrence.emplace(premise.occurrence, &premise).second)
      return decline(Status::IncompleteTheory);
    ++result.consumed_source_clauses;
    if (!add_count(premise.literals.size(), snapshot.literals) ||
        snapshot.literals > bounds.maximum_source_literals)
      return decline(Status::BoundExceeded);
    result.consumed_source_literals = snapshot.literals;
    std::set<uint32_t> clause_variables;
    for (int32_t literal : premise.literals) {
      if (!literal || literal == INT32_MIN ||
          !clause_variables.insert(variable(literal)).second)
        return decline(Status::IncompleteTheory);
      snapshot.maximum_variable =
          std::max(snapshot.maximum_variable, variable(literal));
    }
  }
  if (uint32_t(bounds.extension_variable) <= snapshot.maximum_variable)
    return decline(Status::InvalidRequest);

  std::set<uint64_t> used;
  bool effort_exhausted = false;
  auto exact = [&](uint64_t occurrence, std::vector<int32_t> clause) {
    if (!occurrence) return false;
    if (!spend(uint64_t{1} + clause.size())) {
      effort_exhausted = true;
      return false;
    }
    const auto found = snapshot.by_occurrence.find(occurrence);
    if (found == snapshot.by_occurrence.end()) return false;
    if (normalized(found->second->literals) != normalized(std::move(clause)))
      return false;
    used.insert(occurrence);
    return true;
  };

  auto validate_domain = [&](const Domain &domain) {
    size_t square = 0;
    if (!safe_product(values, values, square) ||
        domain.at_most_one.size() != square ||
        !exact(domain.at_least_one, domain.values))
      return false;
    for (size_t i = 0; i < values; ++i) {
      if (domain.at_most_one[i * values + i]) return false;
      for (size_t j = i + 1; j < values; ++j) {
        const uint64_t forward = domain.at_most_one[i * values + j];
        const uint64_t reverse = domain.at_most_one[j * values + i];
        if (!forward || forward != reverse ||
            !exact(forward,
                   {-domain.values[i], -domain.values[j]}))
          return false;
      }
    }
    return true;
  };
  for (const Domain &domain : relation.domains)
    if (!validate_domain(domain))
      return decline(effort_exhausted ? Status::EffortExhausted
                                      : Status::IncompleteTheory);

  std::map<std::vector<uint32_t>, uint64_t> tuples;
  std::vector<uint32_t> previous;
  bool first = true;
  for (const ForbiddenTuple &tuple : relation.forbidden) {
    if (!spend(1)) return decline(Status::EffortExhausted);
    if (tuple.value_indices.size() != 3 ||
        (!first && !(previous < tuple.value_indices)) ||
        !tuples.emplace(tuple.value_indices, tuple.occurrence).second)
      return decline(Status::InvalidRequest);
    first = false;
    previous = tuple.value_indices;
    std::vector<int32_t> clause;
    for (size_t domain = 0; domain < 3; ++domain) {
      const uint32_t index = tuple.value_indices[domain];
      if (index >= values) return decline(Status::InvalidRequest);
      clause.push_back(-relation.domains[domain].values[index]);
    }
    if (!exact(tuple.occurrence, std::move(clause)))
      return decline(effort_exhausted ? Status::EffortExhausted
                                      : Status::IncompleteTheory);
  }

  for (uint32_t index = 0; index < values; ++index) {
    const std::vector<uint32_t> diagonal{index, index, index};
    const auto found = tuples.find(diagonal);
    if (found == tuples.end()) return decline(Status::NoConsequence);
    result.diagonal_tuple_premises.push_back(found->second);
  }
  // The caller owns one exact theory slice.  Unknown premises cannot silently
  // acquire aligned-value semantics.
  if (used.size() != snapshot.by_occurrence.size())
    return decline(Status::IncompleteTheory);

  auto source_reference = [&](uint64_t occurrence) {
    const Premise *premise = snapshot.by_occurrence.at(occurrence);
    return Reference{ReferenceKind::SourceOccurrence, occurrence,
                     premise->native_proof_id};
  };
  uint64_t addition_literals = 0;
  auto bounded_push = [&](Addition addition) {
    if (result.additions.size() >= bounds.maximum_additions ||
        addition.clause.size() >
            bounds.maximum_addition_literals - addition_literals)
      return false;
    addition_literals += addition.clause.size();
    result.additions.push_back(std::move(addition));
    return true;
  };

  const Domain &left = relation.domains[selection.left_domain];
  const Domain &right = relation.domains[selection.right_domain];
  const Domain &residual = relation.domains[selection.residual_domain];
  const int32_t extension = bounds.extension_variable;
  std::vector<uint64_t> forward_indices;
  for (size_t i = 0; i < values; ++i) {
    Addition forward;
    forward.clause = {extension, -left.values[i], -right.values[i]};
    forward.proof = ProofStep::FreshPositiveRat;
    forward.pivot = extension;
    if (!bounded_push(std::move(forward)))
      return decline(Status::BoundExceeded);
    forward_indices.push_back(result.additions.size() - 1);
  }

  std::vector<uint64_t> reverse_indices;
  for (size_t i = 0; i < values; ++i) {
    Addition reverse;
    reverse.clause = {-extension, -left.values[i], right.values[i]};
    reverse.proof = ProofStep::EqualityReverseRat;
    reverse.pivot = -extension;
    for (uint64_t index : forward_indices)
      reverse.antecedents.push_back(
          {ReferenceKind::PlannedAddition, index, 0});
    for (size_t j = 0; j < values; ++j)
      if (i != j)
        reverse.antecedents.push_back(source_reference(
            left.at_most_one[i * values + j]));
    if (!bounded_push(std::move(reverse)))
      return decline(Status::BoundExceeded);
    reverse_indices.push_back(result.additions.size() - 1);
  }

  for (size_t i = 0; i < values; ++i) {
    Addition transfer;
    transfer.clause = {-extension, -left.values[i], -residual.values[i]};
    transfer.proof = ProofStep::Rup;
    transfer.antecedents.push_back(
        {ReferenceKind::PlannedAddition, reverse_indices[i], 0});
    transfer.antecedents.push_back(
        source_reference(result.diagonal_tuple_premises[i]));
    if (!bounded_push(std::move(transfer)))
      return decline(Status::BoundExceeded);
  }

  if (!spend(uint64_t{1} + result.additions.size() + addition_literals))
    return decline(Status::EffortExhausted);
  result.projection = {extension, left.values, right.values, true, true, false};
  result.status = Status::Planned;
  return result;
}

} // namespace tactix::nary_csp_equality
