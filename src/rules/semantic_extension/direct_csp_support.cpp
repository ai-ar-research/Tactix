#include "rules/semantic_extension/direct_csp_support.hpp"

#include "rules/semantic_extension/csp_exact_theory.hpp"

#include <set>
#include <utility>

namespace tactix::direct_csp_support {

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
    return csp_exact_theory::add_count(amount, result.consumed_work);
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
  if (!csp_exact_theory::valid_values(relation.left.values) ||
      !csp_exact_theory::valid_values(relation.right.values))
    return decline(Status::InvalidRequest);

  std::set<uint32_t> domain_variables;
  for (int32_t literal : relation.left.values)
    domain_variables.insert(csp_exact_theory::variable(literal));
  for (int32_t literal : relation.right.values)
    if (!domain_variables.insert(csp_exact_theory::variable(literal)).second)
      return decline(Status::InvalidRequest);

  csp_exact_theory::Snapshot<Premise> snapshot;
  switch (snapshot.build(source, bounds.maximum_source_literals, spend,
                         result.consumed_source_clauses,
                         result.consumed_source_literals)) {
  case csp_exact_theory::BuildStatus::Valid: break;
  case csp_exact_theory::BuildStatus::IncompleteTheory:
    return decline(Status::IncompleteTheory);
  case csp_exact_theory::BuildStatus::BoundExceeded:
    return decline(Status::BoundExceeded);
  case csp_exact_theory::BuildStatus::EffortExhausted:
    return decline(Status::EffortExhausted);
  }
  auto complement = [](int32_t literal) { return -literal; };

  if (!snapshot.validate_domain(relation.left, spend) ||
      !snapshot.validate_domain(relation.right, spend))
    return decline(snapshot.effort_exhausted ? Status::EffortExhausted
                                             : Status::IncompleteTheory);

  size_t relation_size = 0;
  if (!csp_exact_theory::safe_product(relation.left.values.size(),
                                      relation.right.values.size(),
                                      relation_size) ||
      relation.forbidden.size() != relation_size)
    return decline(Status::InvalidRequest);
  for (size_t left = 0; left < relation.left.values.size(); ++left)
    for (size_t right = 0; right < relation.right.values.size(); ++right) {
      if (!spend(1)) return decline(Status::EffortExhausted);
      const uint64_t occurrence =
          relation.forbidden[left * relation.right.values.size() + right];
      if (occurrence &&
          !snapshot.exact(occurrence,
                          {complement(relation.left.values[left]),
                           complement(relation.right.values[right])},
                          spend))
        return decline(snapshot.effort_exhausted ? Status::EffortExhausted
                                                 : Status::IncompleteTheory);
    }
  // Exact slice ownership prevents an unclassified cross-domain binary or a
  // silently omitted domain premise from being mistaken for compatibility.
  if (!snapshot.complete())
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
