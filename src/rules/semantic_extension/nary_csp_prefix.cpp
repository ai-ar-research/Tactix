#include "rules/semantic_extension/nary_csp_prefix.hpp"

#include <algorithm>
#include <climits>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace tactix::nary_csp_prefix {
namespace {

uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
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

Plan plan_prefix(const std::vector<Premise> &source,
                 const Relation &relation, const Selection &selection,
                 const Bounds &bounds, Charge charge) {
  Plan result;
  result.selection = selection;
  auto decline = [&](Status status) {
    result.status = status;
    result.prefix_literals.clear();
    result.matching_forbidden_premises.clear();
    result.excluded_residual_values.clear();
    result.additions.clear();
    result.projection = {};
    return result;
  };
  auto spend = [&](uint64_t amount) {
    if (!charge || !charge(amount)) return false;
    return add_count(amount, result.consumed_work);
  };

  if (!bounds.maximum_source_clauses ||
      !bounds.maximum_source_literals || bounds.maximum_domains < 3 ||
      bounds.maximum_domain_values < 2 ||
      !bounds.maximum_forbidden_tuples ||
      bounds.maximum_prefix_arity < 2 ||
      !bounds.maximum_definition_clause_literals ||
      !bounds.maximum_additions || !bounds.maximum_addition_literals ||
      bounds.extension_variable <= 0 || source.empty() ||
      relation.domains.size() < 3 ||
      relation.domains.size() > bounds.maximum_domains ||
      relation.forbidden.size() > bounds.maximum_forbidden_tuples ||
      selection.residual_domain >= relation.domains.size() ||
      selection.prefix.size() + 1 != relation.domains.size() ||
      selection.prefix.size() > bounds.maximum_prefix_arity ||
      selection.prefix.size() + 1 >
          bounds.maximum_definition_clause_literals)
    return decline(Status::InvalidRequest);
  if (source.size() > bounds.maximum_source_clauses)
    return decline(Status::BoundExceeded);

  std::set<uint32_t> all_domain_variables;
  for (const Domain &domain : relation.domains) {
    if (domain.values.size() < 2 ||
        domain.values.size() > bounds.maximum_domain_values ||
        !valid_literals(domain.values))
      return decline(Status::InvalidRequest);
    for (int32_t literal : domain.values)
      if (!all_domain_variables.insert(variable(literal)).second)
        return decline(Status::InvalidRequest);
  }

  uint32_t previous_domain = 0;
  bool first_selection = true;
  std::vector<bool> selected_domain(relation.domains.size(), false);
  for (const DomainValue chosen : selection.prefix) {
    if (chosen.domain >= relation.domains.size() ||
        chosen.domain == selection.residual_domain ||
        chosen.value >= relation.domains[chosen.domain].values.size() ||
        (!first_selection && chosen.domain <= previous_domain) ||
        selected_domain[chosen.domain])
      return decline(Status::InvalidRequest);
    first_selection = false;
    previous_domain = chosen.domain;
    selected_domain[chosen.domain] = true;
    result.prefix_literals.push_back(
        relation.domains[chosen.domain].values[chosen.value]);
  }
  for (size_t domain = 0; domain < relation.domains.size(); ++domain)
    if ((domain != selection.residual_domain) != selected_domain[domain])
      return decline(Status::InvalidRequest);

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
    if (!safe_product(domain.values.size(), domain.values.size(), square) ||
        domain.at_most_one.size() != square ||
        !exact(domain.at_least_one, domain.values))
      return false;
    for (size_t i = 0; i < domain.values.size(); ++i) {
      if (domain.at_most_one[i * domain.values.size() + i]) return false;
      for (size_t j = i + 1; j < domain.values.size(); ++j) {
        const uint64_t forward =
            domain.at_most_one[i * domain.values.size() + j];
        const uint64_t reverse =
            domain.at_most_one[j * domain.values.size() + i];
        if (!forward || forward != reverse ||
            !exact(forward, {-domain.values[i], -domain.values[j]}))
          return false;
      }
    }
    return true;
  };
  for (const Domain &domain : relation.domains)
    if (!validate_domain(domain))
      return decline(effort_exhausted ? Status::EffortExhausted
                                      : Status::IncompleteTheory);

  std::set<std::vector<uint32_t>> tuple_coordinates;
  std::set<uint32_t> excluded_indices;
  std::vector<uint32_t> previous_coordinates;
  bool first_tuple = true;
  for (const ForbiddenTuple &tuple : relation.forbidden) {
    if (!spend(1)) return decline(Status::EffortExhausted);
    if (tuple.value_indices.size() != relation.domains.size() ||
        (!first_tuple && !(previous_coordinates < tuple.value_indices)) ||
        !tuple_coordinates.insert(tuple.value_indices).second)
      return decline(Status::InvalidRequest);
    first_tuple = false;
    previous_coordinates = tuple.value_indices;
    std::vector<int32_t> clause;
    clause.reserve(relation.domains.size());
    bool matches = true;
    for (size_t domain = 0; domain < relation.domains.size(); ++domain) {
      const uint32_t index = tuple.value_indices[domain];
      if (index >= relation.domains[domain].values.size())
        return decline(Status::InvalidRequest);
      clause.push_back(-relation.domains[domain].values[index]);
      if (domain != selection.residual_domain) {
        const auto chosen = std::lower_bound(
            selection.prefix.begin(), selection.prefix.end(), domain,
            [](const DomainValue &value, uint32_t key) {
              return value.domain < key;
            });
        if (chosen == selection.prefix.end() || chosen->domain != domain ||
            chosen->value != index)
          matches = false;
      }
    }
    if (!exact(tuple.occurrence, std::move(clause)))
      return decline(effort_exhausted ? Status::EffortExhausted
                                      : Status::IncompleteTheory);
    if (matches) {
      result.matching_forbidden_premises.push_back(tuple.occurrence);
      const uint32_t residual = tuple.value_indices[selection.residual_domain];
      if (!excluded_indices.insert(residual).second)
        return decline(Status::InvalidRequest);
      result.excluded_residual_values.push_back(
          relation.domains[selection.residual_domain].values[residual]);
    }
  }
  // Exact source ownership prevents an unclassified source premise from being
  // silently treated as part of this theory slice.
  if (used.size() != snapshot.by_occurrence.size())
    return decline(Status::IncompleteTheory);
  if (result.matching_forbidden_premises.empty())
    return decline(Status::NoConsequence);

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

  const int32_t extension = bounds.extension_variable;
  std::vector<uint64_t> forward_indices;
  for (int32_t literal : result.prefix_literals) {
    Addition forward;
    forward.clause = {-extension, literal};
    forward.proof = ProofStep::FreshNegativeRat;
    forward.pivot = -extension;
    if (!bounded_push(std::move(forward)))
      return decline(Status::BoundExceeded);
    forward_indices.push_back(result.additions.size() - 1);
  }
  Addition reverse;
  reverse.clause.push_back(extension);
  for (int32_t literal : result.prefix_literals)
    reverse.clause.push_back(-literal);
  reverse.proof = ProofStep::DefinitionReverseRat;
  reverse.pivot = extension;
  for (uint64_t index : forward_indices)
    reverse.antecedents.push_back(
        {ReferenceKind::PlannedAddition, index, 0});
  if (!bounded_push(std::move(reverse)))
    return decline(Status::BoundExceeded);

  std::vector<uint64_t> consequence_indices;
  for (size_t i = 0; i < result.matching_forbidden_premises.size(); ++i) {
    Addition consequence;
    consequence.clause = {-extension, -result.excluded_residual_values[i]};
    consequence.proof = ProofStep::Rup;
    for (uint64_t index : forward_indices)
      consequence.antecedents.push_back(
          {ReferenceKind::PlannedAddition, index, 0});
    consequence.antecedents.push_back(
        source_reference(result.matching_forbidden_premises[i]));
    if (!bounded_push(std::move(consequence)))
      return decline(Status::BoundExceeded);
    consequence_indices.push_back(result.additions.size() - 1);
  }

  if (excluded_indices.size() ==
      relation.domains[selection.residual_domain].values.size()) {
    Addition impossible;
    impossible.clause = {-extension};
    impossible.proof = ProofStep::Rup;
    for (uint64_t index : consequence_indices)
      impossible.antecedents.push_back(
          {ReferenceKind::PlannedAddition, index, 0});
    impossible.antecedents.push_back(source_reference(
        relation.domains[selection.residual_domain].at_least_one));
    if (!bounded_push(std::move(impossible)))
      return decline(Status::BoundExceeded);
  }

  if (!spend(uint64_t{1} + result.additions.size() + addition_literals))
    return decline(Status::EffortExhausted);
  result.projection = {extension, result.prefix_literals, true, true, false};
  result.status = Status::Planned;
  return result;
}

} // namespace tactix::nary_csp_prefix
