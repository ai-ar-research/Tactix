#include "rules/semantic_extension/nary_csp_equality.hpp"

#include "rules/semantic_extension/csp_exact_theory.hpp"

#include <array>
#include <map>
#include <set>
#include <utility>

namespace tactix::nary_csp_equality {

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
    return csp_exact_theory::add_count(amount, result.consumed_work);
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
    if (domain.values.size() != values ||
        !csp_exact_theory::valid_values(domain.values))
      return decline(Status::InvalidRequest);
    for (int32_t literal : domain.values)
      if (!domain_variables.insert(csp_exact_theory::variable(literal)).second)
        return decline(Status::InvalidRequest);
  }

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
  if (uint32_t(bounds.extension_variable) <= snapshot.maximum_variable)
    return decline(Status::InvalidRequest);

  for (const Domain &domain : relation.domains)
    if (!snapshot.validate_domain(domain, spend))
      return decline(snapshot.effort_exhausted ? Status::EffortExhausted
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
    if (!snapshot.exact(tuple.occurrence, std::move(clause), spend))
      return decline(snapshot.effort_exhausted ? Status::EffortExhausted
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
  if (!snapshot.complete())
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
