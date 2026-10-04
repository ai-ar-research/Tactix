#include "rules/semantic_extension/direct_csp_support.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <vector>

using namespace tactix::direct_csp_support;

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

  void build(std::vector<int32_t> left, std::vector<int32_t> right,
             const std::vector<uint8_t> &forbidden) {
    relation.left = domain(std::move(left));
    relation.right = domain(std::move(right));
    assert(forbidden.size() ==
           relation.left.values.size() * relation.right.values.size());
    relation.forbidden.assign(forbidden.size(), 0);
    for (size_t i = 0; i < relation.left.values.size(); ++i)
      for (size_t j = 0; j < relation.right.values.size(); ++j)
        if (forbidden[i * relation.right.values.size() + j])
          relation.forbidden[i * relation.right.values.size() + j] =
              add({-relation.left.values[i], -relation.right.values[j]});
  }
};

Bounds generous() {
  return {1000, 10000, 100, 16, 100, 100, 10000, false, 0};
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

void check_semantic_entailment(const Fixture &fixture,
                               const std::vector<int32_t> &consequence,
                               uint32_t variables) {
  for (uint64_t assignment = 0; assignment < (uint64_t{1} << variables);
       ++assignment) {
    bool source_model = true;
    for (const Premise &premise : fixture.source)
      source_model &= clause_value(premise.literals, assignment);
    if (source_model) assert(clause_value(consequence, assignment));
  }
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

void test_direct_support_and_source_identity() {
  Fixture fixture;
  fixture.build({1, 2}, {3, 4, 5},
                {// left 1 is compatible only with 3
                 0, 1, 1,
                 // left 2 is compatible with 4 and 5
                 1, 0, 0});
  Plan plan = plan_support(fixture.source, fixture.relation, 0, generous(),
                           [](uint64_t) { return true; });
  assert(plan.status == Status::Planned);
  assert(plan.compatible_right_values == std::vector<int32_t>{3});
  assert(plan.additions.size() == 1);
  assert((plan.additions[0].clause == std::vector<int32_t>{-1, 3}));
  assert(plan.additions[0].proof == ProofStep::Rup);
  assert(plan.additions[0].antecedents.size() == 3);
  for (const Reference &reference : plan.additions[0].antecedents) {
    assert(reference.kind == ReferenceKind::SourceOccurrence);
    assert(reference.native_proof_id == 1000 + reference.id);
  }
  check_semantic_entailment(fixture, plan.additions[0].clause, 5);
  std::vector<std::vector<int32_t>> formula;
  for (const Premise &premise : fixture.source)
    formula.push_back(premise.literals);
  assert(rup(formula, plan.additions[0].clause));
  assert(!plan.projection.extension_variable);
}

void test_empty_support_is_a_unit() {
  Fixture fixture;
  fixture.build({1, 2}, {3, 4, 5}, {1, 1, 1, 0, 0, 0});
  Plan plan = plan_support(fixture.source, fixture.relation, 0, generous(),
                           [](uint64_t) { return true; });
  assert(plan.status == Status::Planned);
  assert(plan.compatible_right_values.empty());
  assert(plan.additions.size() == 1);
  assert((plan.additions[0].clause == std::vector<int32_t>{-1}));
  check_semantic_entailment(fixture, plan.additions[0].clause, 5);
}

void test_exact_or_extension_and_projection() {
  Fixture fixture;
  fixture.build({1, 2}, {3, 4, 5, 6},
                {0, 0, 0, 1,
                 1, 0, 1, 0});
  Bounds bounds = generous();
  bounds.maximum_direct_clause_literals = 2;
  bounds.allow_extension = true;
  bounds.extension_variable = 7;
  Plan plan = plan_support(fixture.source, fixture.relation, 0, bounds,
                           [](uint64_t) { return true; });
  assert(plan.status == Status::Planned);
  assert((plan.compatible_right_values == std::vector<int32_t>{3, 4, 5}));
  assert(plan.additions.size() == 5);
  assert((plan.additions[0].clause == std::vector<int32_t>{7, -3}));
  assert((plan.additions[1].clause == std::vector<int32_t>{7, -4}));
  assert((plan.additions[2].clause == std::vector<int32_t>{7, -5}));
  assert((plan.additions[3].clause == std::vector<int32_t>{-7, 3, 4, 5}));
  assert((plan.additions[4].clause == std::vector<int32_t>{-1, 7}));
  assert(plan.additions[0].proof == ProofStep::FreshPositiveRat);
  assert(plan.additions[3].proof == ProofStep::DefinitionReverseRat);
  assert(plan.additions[4].proof == ProofStep::Rup);
  assert(plan.projection.extension_variable == 7);
  assert(plan.projection.exact_or_definition);
  assert(plan.projection.project_from_external_model);
  assert(!plan.projection.reconstruction_witness_required);

  std::vector<std::vector<int32_t>> formula;
  for (const Premise &premise : fixture.source)
    formula.push_back(premise.literals);
  for (size_t i = 0; i < plan.additions.size(); ++i) {
    if (plan.additions[i].proof == ProofStep::Rup)
      assert(rup(formula, plan.additions[i].clause));
    formula.push_back(plan.additions[i].clause);
  }
  // Every original model has exactly the advertised extension value, and
  // projection of every extended model remains an original model.
  for (uint64_t original = 0; original < (uint64_t{1} << 6); ++original) {
    bool source_model = true;
    for (const Premise &premise : fixture.source)
      source_model &= clause_value(premise.literals, original);
    if (!source_model) continue;
    const bool expected_extension = literal_value(3, original) ||
                                    literal_value(4, original) ||
                                    literal_value(5, original);
    const uint64_t extended = original |
        (expected_extension ? (uint64_t{1} << 6) : uint64_t{0});
    for (const Addition &addition : plan.additions)
      assert(clause_value(addition.clause, extended));
  }
}

void test_signed_selection_literals() {
  Fixture fixture;
  fixture.build({-1, 2}, {3, -4}, {1, 0, 0, 0});
  Plan plan = plan_support(fixture.source, fixture.relation, 0, generous(),
                           [](uint64_t) { return true; });
  assert(plan.status == Status::Planned);
  assert((plan.additions[0].clause == std::vector<int32_t>{1, -4}));
  check_semantic_entailment(fixture, plan.additions[0].clause, 4);
}

void test_declines_are_empty_and_exact() {
  Fixture fixture;
  fixture.build({1, 2}, {3, 4, 5}, {0, 1, 1, 1, 0, 0});

  Relation missing_amo = fixture.relation;
  missing_amo.right.at_most_one[1] = 0;
  missing_amo.right.at_most_one[3] = 0;
  Plan incomplete = plan_support(fixture.source, missing_amo, 0, generous(),
                                 [](uint64_t) { return true; });
  assert(incomplete.status == Status::IncompleteTheory);
  assert(incomplete.additions.empty());
  assert(!incomplete.projection.extension_variable);

  Relation omitted_forbidden = fixture.relation;
  omitted_forbidden.forbidden[1] = 0;
  incomplete = plan_support(fixture.source, omitted_forbidden, 0, generous(),
                            [](uint64_t) { return true; });
  assert(incomplete.status == Status::IncompleteTheory);

  Fixture wrong = fixture;
  const uint64_t forbidden = wrong.relation.forbidden[1];
  for (Premise &premise : wrong.source)
    if (premise.occurrence == forbidden) premise.literals = {-1, -3};
  incomplete = plan_support(wrong.source, wrong.relation, 0, generous(),
                            [](uint64_t) { return true; });
  assert(incomplete.status == Status::IncompleteTheory);

  Fixture extra = fixture;
  extra.add({1, 3});
  incomplete = plan_support(extra.source, extra.relation, 0, generous(),
                            [](uint64_t) { return true; });
  assert(incomplete.status == Status::IncompleteTheory);

  Fixture duplicate = fixture;
  duplicate.source.back().occurrence = duplicate.source.front().occurrence;
  incomplete = plan_support(duplicate.source, duplicate.relation, 0, generous(),
                            [](uint64_t) { return true; });
  assert(incomplete.status == Status::IncompleteTheory);

  Bounds too_small = generous();
  too_small.maximum_direct_clause_literals = 1;
  Plan bounded = plan_support(fixture.source, fixture.relation, 0, too_small,
                              [](uint64_t) { return true; });
  assert(bounded.status == Status::BoundExceeded);
  assert(bounded.additions.empty());

  Bounds bad_extension = generous();
  bad_extension.maximum_direct_clause_literals = 1;
  bad_extension.allow_extension = true;
  bad_extension.extension_variable = 5;
  bounded = plan_support(fixture.source, fixture.relation, 0, bad_extension,
                         [](uint64_t) { return true; });
  assert(bounded.status == Status::BoundExceeded);
  assert(bounded.additions.empty());

  uint64_t remaining = 10;
  Plan stopped = plan_support(
      fixture.source, fixture.relation, 0, generous(),
      [&](uint64_t amount) {
        if (amount > remaining) return false;
        remaining -= amount;
        return true;
      });
  assert(stopped.status == Status::EffortExhausted);
  assert(stopped.consumed_work <= 10);
  assert(stopped.additions.empty());
}

void test_no_forbidden_pair_is_no_consequence() {
  Fixture fixture;
  fixture.build({1, 2}, {3, 4}, {0, 0, 1, 0});
  Plan plan = plan_support(fixture.source, fixture.relation, 0, generous(),
                           [](uint64_t) { return true; });
  assert(plan.status == Status::NoConsequence);
  assert(plan.additions.empty());
  assert(plan.compatible_right_values.empty());
}

} // namespace

int main() {
  test_direct_support_and_source_identity();
  test_empty_support_is_a_unit();
  test_exact_or_extension_and_projection();
  test_signed_selection_literals();
  test_declines_are_empty_and_exact();
  test_no_forbidden_pair_is_no_consequence();
  std::cout << "direct CSP support planner tests passed\n";
}
