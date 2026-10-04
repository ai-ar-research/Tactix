#include "rules/semantic_extension/nary_csp_prefix.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <vector>

using namespace tactix::nary_csp_prefix;

namespace {

struct Fixture {
  std::vector<Premise> source;
  Relation relation;
  uint64_t next = 1;

  uint64_t add(std::vector<int32_t> clause) {
    const uint64_t occurrence = next++;
    source.push_back({occurrence, std::move(clause), 1000 + occurrence});
    return occurrence;
  }

  Domain domain(std::vector<int32_t> values) {
    Domain result;
    result.values = std::move(values);
    result.at_least_one = add(result.values);
    const size_t n = result.values.size();
    result.at_most_one.assign(n * n, 0);
    for (size_t i = 0; i < n; ++i)
      for (size_t j = i + 1; j < n; ++j) {
        const uint64_t occurrence =
            add({-result.values[i], -result.values[j]});
        result.at_most_one[i * n + j] = occurrence;
        result.at_most_one[j * n + i] = occurrence;
      }
    return result;
  }

  void domains(std::initializer_list<std::vector<int32_t>> values) {
    for (auto domain_values : values)
      relation.domains.push_back(domain(std::move(domain_values)));
  }

  void forbid(std::vector<uint32_t> coordinates) {
    assert(coordinates.size() == relation.domains.size());
    std::vector<int32_t> clause;
    for (size_t domain = 0; domain < coordinates.size(); ++domain) {
      assert(coordinates[domain] < relation.domains[domain].values.size());
      clause.push_back(
          -relation.domains[domain].values[coordinates[domain]]);
    }
    relation.forbidden.push_back({std::move(coordinates), add(clause)});
  }
};

Bounds generous(int32_t extension = 8) {
  return {1000, 10000, 16, 100, 1000, 15, 100, 1000, 10000,
          extension};
}

bool literal_value(int32_t literal, uint64_t assignment) {
  const uint32_t variable =
      static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
  const bool value = assignment & (uint64_t{1} << (variable - 1));
  return literal > 0 ? value : !value;
}

bool clause_value(const std::vector<int32_t> &clause, uint64_t assignment) {
  for (int32_t literal : clause)
    if (literal_value(literal, assignment)) return true;
  return false;
}

bool rup(const std::vector<std::vector<int32_t>> &formula,
         const std::vector<int32_t> &addition) {
  std::map<uint32_t, bool> assigned;
  auto assign = [&](int32_t literal) {
    const uint32_t variable =
        static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
    const bool value = literal > 0;
    const auto [where, inserted] = assigned.emplace(variable, value);
    return inserted || where->second == value;
  };
  for (int32_t literal : addition)
    if (!assign(-literal)) return true;
  for (;;) {
    bool changed = false;
    for (const auto &clause : formula) {
      bool satisfied = false;
      int32_t unit = 0;
      uint32_t open = 0;
      for (int32_t literal : clause) {
        const uint32_t variable = static_cast<uint32_t>(
            literal < 0 ? -int64_t(literal) : literal);
        const auto found = assigned.find(variable);
        if (found == assigned.end()) {
          unit = literal;
          ++open;
        } else if (found->second == (literal > 0)) {
          satisfied = true;
          break;
        }
      }
      if (satisfied) continue;
      if (!open) return true;
      if (open == 1) {
        const size_t before = assigned.size();
        if (!assign(unit)) return true;
        changed |= assigned.size() != before;
      }
    }
    if (!changed) return false;
  }
}

void verify_rup_steps(const Fixture &fixture, const Plan &plan) {
  std::vector<std::vector<int32_t>> formula;
  for (const Premise &premise : fixture.source)
    formula.push_back(premise.literals);
  for (const Addition &addition : plan.additions) {
    if (addition.proof == ProofStep::Rup)
      assert(rup(formula, addition.clause));
    formula.push_back(addition.clause);
  }
}

void verify_projection(const Fixture &fixture, const Plan &plan,
                       uint32_t original_variables) {
  const int32_t extension = plan.projection.extension_variable;
  assert(extension == int32_t(original_variables + 1));
  for (uint64_t assignment = 0;
       assignment < (uint64_t{1} << original_variables); ++assignment) {
    bool source_model = true;
    for (const Premise &premise : fixture.source)
      source_model &= clause_value(premise.literals, assignment);
    if (!source_model) continue;
    bool conjunction = true;
    for (int32_t literal : plan.projection.conjuncts)
      conjunction &= literal_value(literal, assignment);
    const uint64_t extended =
        assignment |
        (conjunction ? (uint64_t{1} << original_variables) : uint64_t{0});
    for (const Addition &addition : plan.additions)
      assert(clause_value(addition.clause, extended));
  }
  // Any satisfying extended assignment projects to an original model because
  // the source clauses remain present; the definition also fixes p exactly.
  for (uint64_t assignment = 0;
       assignment < (uint64_t{1} << (original_variables + 1)); ++assignment) {
    bool model = true;
    for (const Premise &premise : fixture.source)
      model &= clause_value(premise.literals, assignment);
    for (const Addition &addition : plan.additions)
      model &= clause_value(addition.clause, assignment);
    if (!model) continue;
    bool conjunction = true;
    for (int32_t literal : plan.projection.conjuncts)
      conjunction &= literal_value(literal, assignment);
    assert(literal_value(extension, assignment) == conjunction);
  }
}

void test_three_domain_prefix_definition_and_consequences() {
  Fixture fixture;
  fixture.domains({{1, 2}, {3, 4}, {5, 6, 7}});
  fixture.forbid({0, 1, 0});
  fixture.forbid({0, 1, 2});
  fixture.forbid({1, 0, 1});
  const Selection selection{2, {{0, 0}, {1, 1}}};
  const Plan plan = plan_prefix(fixture.source, fixture.relation, selection,
                                generous(), [](uint64_t) { return true; });
  assert(plan.status == Status::Planned);
  assert(plan.selection == selection);
  assert((plan.prefix_literals == std::vector<int32_t>{1, 4}));
  assert((plan.excluded_residual_values == std::vector<int32_t>{5, 7}));
  assert(plan.matching_forbidden_premises.size() == 2);
  assert(plan.additions.size() == 5);
  assert((plan.additions[0].clause == std::vector<int32_t>{-8, 1}));
  assert((plan.additions[1].clause == std::vector<int32_t>{-8, 4}));
  assert((plan.additions[2].clause == std::vector<int32_t>{8, -1, -4}));
  assert((plan.additions[3].clause == std::vector<int32_t>{-8, -5}));
  assert((plan.additions[4].clause == std::vector<int32_t>{-8, -7}));
  assert(plan.additions[0].proof == ProofStep::FreshNegativeRat &&
         plan.additions[0].pivot == -8);
  assert(plan.additions[2].proof == ProofStep::DefinitionReverseRat &&
         plan.additions[2].pivot == 8 &&
         plan.additions[2].antecedents.size() == 2);
  for (size_t i : {size_t{3}, size_t{4}}) {
    assert(plan.additions[i].proof == ProofStep::Rup);
    assert(plan.additions[i].antecedents.size() == 3);
    const Reference &source = plan.additions[i].antecedents.back();
    assert(source.kind == ReferenceKind::SourceOccurrence);
    assert(source.native_proof_id == 1000 + source.id);
  }
  assert(plan.projection.extension_variable == 8);
  assert(plan.projection.exact_and_definition);
  assert(plan.projection.project_from_external_model);
  assert(!plan.projection.reconstruction_witness_required);
  verify_rup_steps(fixture, plan);
  verify_projection(fixture, plan, 7);
}

void test_complete_residual_exclusion_derives_impossible_prefix() {
  Fixture fixture;
  fixture.domains({{1, 2}, {3, 4}, {5, 6}});
  fixture.forbid({0, 0, 0});
  fixture.forbid({0, 0, 1});
  const Plan plan = plan_prefix(fixture.source, fixture.relation,
                                {2, {{0, 0}, {1, 0}}}, generous(7),
                                [](uint64_t) { return true; });
  assert(plan.status == Status::Planned);
  assert(plan.additions.size() == 6);
  assert((plan.additions.back().clause == std::vector<int32_t>{-7}));
  assert(plan.additions.back().proof == ProofStep::Rup);
  assert(plan.additions.back().antecedents.size() == 3);
  verify_rup_steps(fixture, plan);
  verify_projection(fixture, plan, 6);
}

void test_signed_literals_and_four_domains() {
  Fixture signed_fixture;
  signed_fixture.domains({{-1, 2}, {3, -4}, {5, 6}});
  signed_fixture.forbid({0, 1, 0});
  Plan plan = plan_prefix(signed_fixture.source, signed_fixture.relation,
                          {2, {{0, 0}, {1, 1}}}, generous(7),
                          [](uint64_t) { return true; });
  assert(plan.status == Status::Planned);
  assert((plan.additions[0].clause == std::vector<int32_t>{-7, -1}));
  assert((plan.additions[1].clause == std::vector<int32_t>{-7, -4}));
  assert((plan.additions[2].clause == std::vector<int32_t>{7, 1, 4}));
  assert((plan.additions[3].clause == std::vector<int32_t>{-7, -5}));
  verify_rup_steps(signed_fixture, plan);
  verify_projection(signed_fixture, plan, 6);

  Fixture arity_four;
  arity_four.domains({{1, 2}, {3, 4}, {5, 6}, {7, 8}});
  arity_four.forbid({0, 1, 0, 1});
  plan = plan_prefix(arity_four.source, arity_four.relation,
                     {3, {{0, 0}, {1, 1}, {2, 0}}}, generous(9),
                     [](uint64_t) { return true; });
  assert(plan.status == Status::Planned);
  assert(plan.prefix_literals.size() == 3);
  assert(plan.additions.size() == 5);
  assert((plan.additions.back().clause == std::vector<int32_t>{-9, -8}));
  verify_rup_steps(arity_four, plan);
  verify_projection(arity_four, plan, 8);
}

void test_exact_source_selection_and_declines() {
  Fixture fixture;
  fixture.domains({{1, 2}, {3, 4}, {5, 6}});
  fixture.forbid({1, 1, 0});
  const Selection selection{2, {{0, 0}, {1, 0}}};

  Plan plan = plan_prefix(fixture.source, fixture.relation, selection,
                          generous(7), [](uint64_t) { return true; });
  assert(plan.status == Status::NoConsequence);
  assert(plan.additions.empty() && !plan.projection.extension_variable);

  std::reverse(fixture.source.begin(), fixture.source.end());
  fixture.relation.forbidden.push_back(
      {{0, 0, 1}, fixture.add({-1, -3, -6})});
  std::sort(fixture.relation.forbidden.begin(),
            fixture.relation.forbidden.end(),
            [](const ForbiddenTuple &a, const ForbiddenTuple &b) {
              return a.value_indices < b.value_indices;
            });
  plan = plan_prefix(fixture.source, fixture.relation, selection, generous(7),
                     [](uint64_t) { return true; });
  assert(plan.status == Status::Planned);

  Relation missing_amo = fixture.relation;
  missing_amo.domains[0].at_most_one[1] = 0;
  missing_amo.domains[0].at_most_one[2] = 0;
  plan = plan_prefix(fixture.source, missing_amo, selection, generous(7),
                     [](uint64_t) { return true; });
  assert(plan.status == Status::IncompleteTheory && plan.additions.empty());

  Relation duplicate_tuple = fixture.relation;
  duplicate_tuple.forbidden.push_back(duplicate_tuple.forbidden.back());
  plan = plan_prefix(fixture.source, duplicate_tuple, selection, generous(7),
                     [](uint64_t) { return true; });
  assert(plan.status == Status::InvalidRequest && plan.additions.empty());

  Relation unordered = fixture.relation;
  std::swap(unordered.forbidden[0], unordered.forbidden[1]);
  plan = plan_prefix(fixture.source, unordered, selection, generous(7),
                     [](uint64_t) { return true; });
  assert(plan.status == Status::InvalidRequest && plan.additions.empty());

  Fixture wrong = fixture;
  const uint64_t tuple_occurrence = wrong.relation.forbidden.back().occurrence;
  for (Premise &premise : wrong.source)
    if (premise.occurrence == tuple_occurrence) premise.literals = {-1, -4, -6};
  plan = plan_prefix(wrong.source, wrong.relation, selection, generous(7),
                     [](uint64_t) { return true; });
  assert(plan.status == Status::IncompleteTheory && plan.additions.empty());

  Fixture extra = fixture;
  extra.add({1, 3});
  plan = plan_prefix(extra.source, extra.relation, selection, generous(7),
                     [](uint64_t) { return true; });
  assert(plan.status == Status::IncompleteTheory && plan.additions.empty());

  const Selection incomplete{2, {{0, 0}}};
  plan = plan_prefix(fixture.source, fixture.relation, incomplete, generous(7),
                     [](uint64_t) { return true; });
  assert(plan.status == Status::InvalidRequest && plan.additions.empty());

  Bounds bounded = generous(7);
  bounded.maximum_additions = 3;
  plan = plan_prefix(fixture.source, fixture.relation, selection, bounded,
                     [](uint64_t) { return true; });
  assert(plan.status == Status::BoundExceeded && plan.additions.empty());

  uint64_t remaining = 10;
  plan = plan_prefix(
      fixture.source, fixture.relation, selection, generous(7),
      [&](uint64_t amount) {
        if (amount > remaining) return false;
        remaining -= amount;
        return true;
      });
  assert(plan.status == Status::EffortExhausted &&
         plan.consumed_work <= 10 && plan.additions.empty());
}

} // namespace

int main() {
  test_three_domain_prefix_definition_and_consequences();
  test_complete_residual_exclusion_derives_impossible_prefix();
  test_signed_literals_and_four_domains();
  test_exact_source_selection_and_declines();
  std::cout << "n-ary CSP prefix planner tests passed\n";
}
