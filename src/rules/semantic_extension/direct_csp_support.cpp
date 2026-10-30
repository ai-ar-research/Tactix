#include "rules/semantic_extension/direct_csp_support.hpp"

#include "kernel/types.hpp"

#include <algorithm>
#include <climits>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace tactix::direct_csp_support {
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

struct Snapshot {
  std::map<uint64_t, const Premise *> by_occurrence;
  uint64_t literals = 0;
  uint32_t maximum_variable = 0;
};

bool add_count(uint64_t amount, uint64_t &total) {
  if (amount > UINT64_MAX - total) return false;
  total += amount;
  return true;
}

} // namespace

Plan plan_support(const std::vector<Premise> &source, const Relation &relation,
                  uint32_t selected_left_value, const Bounds &bounds,
                  Charge charge) {
  Plan result;
  result.selected_left_value = selected_left_value;
  auto decline = [&](Status status) {
    result.status = status;
    result.compatible_right_values.clear();
    result.forbidden_premises.clear();
    result.additions.clear();
    result.projection = {};
    return result;
  };
  auto spend = [&](uint64_t amount) {
    if (!charge || !charge(amount)) return false;
    return add_count(amount, result.consumed_work);
  };

  if (!bounds.maximum_source_clauses ||
      !bounds.maximum_source_literals || !bounds.maximum_domain_values ||
      !bounds.maximum_direct_clause_literals ||
      !bounds.maximum_definition_clause_literals ||
      !bounds.maximum_additions || !bounds.maximum_addition_literals ||
      source.empty() || relation.left.values.size() < 2 ||
      relation.right.values.size() < 2 ||
      selected_left_value >= relation.left.values.size())
    return decline(Status::InvalidRequest);
  if (source.size() > bounds.maximum_source_clauses ||
      relation.left.values.size() > bounds.maximum_domain_values ||
      relation.right.values.size() > bounds.maximum_domain_values)
    return decline(Status::BoundExceeded);
  if (!valid_literals(relation.left.values) ||
      !valid_literals(relation.right.values))
    return decline(Status::InvalidRequest);

  std::set<uint32_t> domain_variables;
  for (int32_t literal : relation.left.values)
    domain_variables.insert(variable(literal));
  for (int32_t literal : relation.right.values)
    if (!domain_variables.insert(variable(literal)).second)
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
  auto complement = [](int32_t literal) { return -literal; };

  auto validate_domain = [&](const Domain &domain) {
    size_t square = 0;
    if (!safe_product(domain.values.size(), domain.values.size(), square) ||
        domain.at_most_one.size() != square ||
        !exact(domain.at_least_one, domain.values))
      return false;
    for (size_t first = 0; first < domain.values.size(); ++first) {
      if (domain.at_most_one[first * domain.values.size() + first])
        return false;
      for (size_t second = first + 1; second < domain.values.size(); ++second) {
        const uint64_t forward =
            domain.at_most_one[first * domain.values.size() + second];
        const uint64_t reverse =
            domain.at_most_one[second * domain.values.size() + first];
        if (!forward || forward != reverse ||
            !exact(forward, {complement(domain.values[first]),
                             complement(domain.values[second])}))
          return false;
      }
    }
    return true;
  };
  if (!validate_domain(relation.left) || !validate_domain(relation.right))
    return decline(effort_exhausted ? Status::EffortExhausted
                                    : Status::IncompleteTheory);

  size_t relation_size = 0;
  if (!safe_product(relation.left.values.size(), relation.right.values.size(),
                    relation_size) ||
      relation.forbidden.size() != relation_size)
    return decline(Status::InvalidRequest);
  for (size_t left = 0; left < relation.left.values.size(); ++left)
    for (size_t right = 0; right < relation.right.values.size(); ++right) {
      if (!spend(1)) return decline(Status::EffortExhausted);
      const uint64_t occurrence =
          relation.forbidden[left * relation.right.values.size() + right];
      if (occurrence &&
          !exact(occurrence,
                 {complement(relation.left.values[left]),
                  complement(relation.right.values[right])}))
        return decline(effort_exhausted ? Status::EffortExhausted
                                        : Status::IncompleteTheory);
    }
  // Exact slice ownership prevents an unclassified cross-domain binary or a
  // silently omitted domain premise from being mistaken for compatibility.
  if (used.size() != snapshot.by_occurrence.size())
    return decline(Status::IncompleteTheory);

  const size_t row = size_t(selected_left_value) * relation.right.values.size();
  for (size_t right = 0; right < relation.right.values.size(); ++right) {
    const uint64_t occurrence = relation.forbidden[row + right];
    if (occurrence)
      result.forbidden_premises.push_back(occurrence);
    else
      result.compatible_right_values.push_back(relation.right.values[right]);
  }
  // With no forbidden pair this is merely the existing right-domain ALO
  // weakened by ~left[selected], so publishing it has no useful effect.
  if (result.forbidden_premises.empty())
    return decline(Status::NoConsequence);

  auto source_reference = [&](uint64_t occurrence) {
    const Premise *premise = snapshot.by_occurrence.at(occurrence);
    return Reference{ReferenceKind::SourceOccurrence, occurrence,
                     premise->native_proof_id};
  };
  auto bounded_push = [&](Addition addition, uint64_t &addition_literals) {
    if (result.additions.size() >= bounds.maximum_additions ||
        addition.clause.size() >
            bounds.maximum_addition_literals - addition_literals)
      return false;
    addition_literals += addition.clause.size();
    result.additions.push_back(std::move(addition));
    return true;
  };

  uint64_t addition_literals = 0;
  const uint64_t direct_width =
      uint64_t{1} + result.compatible_right_values.size();
  if (direct_width <= bounds.maximum_direct_clause_literals) {
    Addition support;
    support.clause.push_back(-relation.left.values[selected_left_value]);
    support.clause.insert(support.clause.end(),
                          result.compatible_right_values.begin(),
                          result.compatible_right_values.end());
    support.proof = ProofStep::Rup;
    for (uint64_t occurrence : result.forbidden_premises)
      support.antecedents.push_back(source_reference(occurrence));
    support.antecedents.push_back(source_reference(relation.right.at_least_one));
    if (!bounded_push(std::move(support), addition_literals))
      return decline(Status::BoundExceeded);
  } else {
    if (!bounds.allow_extension || result.compatible_right_values.size() < 2 ||
        bounds.extension_variable <= 0 ||
        uint32_t(bounds.extension_variable) <= snapshot.maximum_variable ||
        uint64_t{1} + result.compatible_right_values.size() >
            bounds.maximum_definition_clause_literals)
      return decline(Status::BoundExceeded);
    const int32_t extension = bounds.extension_variable;
    std::vector<uint64_t> divider_indices;
    for (int32_t value : result.compatible_right_values) {
      Addition divider;
      divider.clause = {extension, -value};
      divider.proof = ProofStep::FreshPositiveRat;
      divider.pivot = extension;
      if (!bounded_push(std::move(divider), addition_literals))
        return decline(Status::BoundExceeded);
      divider_indices.push_back(result.additions.size() - 1);
    }
    Addition reverse;
    reverse.clause.push_back(-extension);
    reverse.clause.insert(reverse.clause.end(),
                          result.compatible_right_values.begin(),
                          result.compatible_right_values.end());
    reverse.proof = ProofStep::DefinitionReverseRat;
    reverse.pivot = -extension;
    for (uint64_t index : divider_indices)
      reverse.antecedents.push_back(
          {ReferenceKind::PlannedAddition, index, 0});
    if (!bounded_push(std::move(reverse), addition_literals))
      return decline(Status::BoundExceeded);

    Addition support;
    support.clause = {-relation.left.values[selected_left_value], extension};
    support.proof = ProofStep::Rup;
    for (uint64_t occurrence : result.forbidden_premises)
      support.antecedents.push_back(source_reference(occurrence));
    for (uint64_t index : divider_indices)
      support.antecedents.push_back(
          {ReferenceKind::PlannedAddition, index, 0});
    support.antecedents.push_back(source_reference(relation.right.at_least_one));
    if (!bounded_push(std::move(support), addition_literals))
      return decline(Status::BoundExceeded);
    result.projection = {extension, result.compatible_right_values, true, true,
                         false};
  }

  if (!spend(uint64_t{1} + result.additions.size() + addition_literals))
    return decline(Status::EffortExhausted);
  result.status = Status::Planned;
  return result;
}

} // namespace tactix::direct_csp_support
