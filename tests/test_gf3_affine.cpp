#undef NDEBUG
#include "rules/semantic_extension/gf3_affine.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <vector>

using namespace tactix::gf3_affine;

namespace {

uint32_t variable(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

struct Fixture {
  std::vector<Premise> source;
  std::vector<std::vector<int32_t>> values;
  uint64_t next = 1;

  uint64_t add(std::vector<int32_t> clause) {
    const uint64_t occurrence = next++;
    source.push_back({occurrence, std::move(clause), 9000 + occurrence});
    return occurrence;
  }

  uint32_t domain(std::vector<int32_t> selection_literals) {
    assert(selection_literals.size() == 3);
    const uint32_t index = static_cast<uint32_t>(values.size());
    values.push_back(selection_literals);
    add(selection_literals);
    for (uint8_t first = 0; first < 3; ++first)
      for (uint8_t second = first + 1; second < 3; ++second)
        add({-selection_literals[first], -selection_literals[second]});
    return index;
  }

  void equation(const std::vector<uint32_t> &domains,
                const std::vector<uint8_t> &coefficients, uint8_t rhs) {
    assert(domains.size() >= 2);
    assert(domains.size() == coefficients.size());
    uint64_t tuples = 1;
    for (size_t i = 0; i < domains.size(); ++i) tuples *= 3;
    for (uint64_t tuple = 0; tuple < tuples; ++tuple) {
      uint64_t rest = tuple;
      uint8_t sum = 0;
      std::vector<int32_t> clause;
      for (size_t i = 0; i < domains.size(); ++i) {
        const uint8_t value = static_cast<uint8_t>(rest % 3);
        rest /= 3;
        sum = static_cast<uint8_t>((sum + coefficients[i] * value) % 3);
        clause.push_back(-values[domains[i]][value]);
      }
      if (sum != rhs) add(std::move(clause));
    }
  }
};

RecognitionBounds recognition_bounds() {
  return {10000, 100000, 100, 100, 10, 60000};
}

PairEliminationBounds plan_bounds() {
  return {10, 60000, 60000, 1000000};
}

RecognitionResult recognize(const Fixture &fixture) {
  return tactix::gf3_affine::recognize(
      fixture.source, recognition_bounds(), [](uint64_t) { return true; });
}

bool literal_value(int32_t literal, uint64_t assignment) {
  const bool value = assignment & (uint64_t{1} << (variable(literal) - 1));
  return literal > 0 ? value : !value;
}

bool clause_value(const std::vector<int32_t> &clause, uint64_t assignment) {
  for (int32_t literal : clause)
    if (literal_value(literal, assignment)) return true;
  return false;
}

// Deliberately independent from the planner: replay reverse unit propagation
// over ordinary clauses, with no use of equation metadata.
bool rup(const std::vector<std::vector<int32_t>> &formula,
         const std::vector<int32_t> &addition) {
  std::map<uint32_t, bool> assigned;
  auto assign = [&](int32_t literal) {
    const auto [where, inserted] =
        assigned.emplace(variable(literal), literal > 0);
    return inserted || where->second == (literal > 0);
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
        const auto found = assigned.find(variable(literal));
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

std::vector<std::vector<int32_t>> clauses(const Fixture &fixture) {
  std::vector<std::vector<int32_t>> result;
  for (const Premise &premise : fixture.source)
    result.push_back(premise.literals);
  return result;
}

Fixture small_fixture(bool signed_values = false) {
  Fixture fixture;
  fixture.domain(signed_values ? std::vector<int32_t>{-1, 2, -3}
                               : std::vector<int32_t>{1, 2, 3});
  fixture.domain({4, 5, 6});
  fixture.domain(signed_values ? std::vector<int32_t>{-7, 8, 9}
                               : std::vector<int32_t>{7, 8, 9});
  // A + P = 0; P + B = 0.  Eliminating P yields A + 2B = 0.
  fixture.equation({0, 1}, {1, 1}, 0);
  fixture.equation({1, 2}, {1, 1}, 0);
  return fixture;
}

void check_decline(const PairEliminationPlan &plan, PlanStatus status) {
  assert(plan.status == status);
  assert(plan.additions.empty());
  assert(plan.consequence.domains.empty());
  assert(plan.projection.extension_variables.empty());
  assert(plan.projection.external_extent_unchanged);
  assert(!plan.projection.reconstruction_witness_required);
}

void test_exact_recognition_and_pair_elimination() {
  const Fixture fixture = small_fixture();
  const RecognitionResult recognized = recognize(fixture);
  assert(recognized.status == RecognitionStatus::Recognized);
  assert(recognized.theory.domains.size() == 3);
  assert(recognized.theory.equations.size() == 2);
  assert(recognized.theory.source.size() == fixture.source.size());
  for (const Equation &equation : recognized.theory.equations) {
    assert(equation.domains.size() == 2);
    assert(equation.coefficients == std::vector<uint8_t>({1, 1}));
    assert(equation.rhs == 0);
    assert(std::count_if(equation.forbidden_by_tuple.begin(),
                         equation.forbidden_by_tuple.end(),
                         [](uint64_t occurrence) { return occurrence != 0; }) ==
           6);
  }

  const PairEliminationPlan plan = plan_pair_elimination(
      recognized.theory, 0, 1, plan_bounds(),
      [](uint64_t) { return true; });
  assert(plan.status == PlanStatus::Planned);
  assert(plan.pivot_domain == 1);
  assert(plan.consequence.domains == std::vector<uint32_t>({0, 2}));
  assert(plan.consequence.coefficients == std::vector<uint8_t>({1, 2}));
  assert(plan.consequence.rhs == 0);
  assert(plan.additions.size() == 6);
  assert(plan.consumed_result_tuples == 9);
  assert(plan.consumed_addition_literals == 12);
  assert(plan.projection.extension_variables.empty());
  assert(plan.projection.external_extent_unchanged);
  assert(!plan.projection.reconstruction_witness_required);

  const auto source_formula = clauses(fixture);
  std::map<uint64_t, std::vector<int32_t>> by_occurrence;
  for (const Premise &premise : fixture.source)
    by_occurrence.emplace(premise.occurrence, premise.literals);
  for (const Addition &addition : plan.additions) {
    assert(addition.antecedents.size() == 5);
    std::set<uint64_t> unique;
    std::vector<std::vector<int32_t>> local_proof_formula;
    for (const SourceReference &reference : addition.antecedents) {
      assert(reference.occurrence);
      assert(reference.native_proof_id == 9000 + reference.occurrence);
      assert(unique.insert(reference.occurrence).second);
      local_proof_formula.push_back(by_occurrence.at(reference.occurrence));
    }
    assert(rup(local_proof_formula, addition.clause));
    assert(rup(source_formula, addition.clause));
  }

  // Exhaustively check Boolean entailment over the nine original variables.
  for (uint64_t assignment = 0; assignment < (uint64_t{1} << 9); ++assignment) {
    bool source_model = true;
    for (const Premise &premise : fixture.source)
      source_model &= clause_value(premise.literals, assignment);
    if (!source_model) continue;
    for (const Addition &addition : plan.additions)
      assert(clause_value(addition.clause, assignment));
  }
}

void test_signed_selection_literals() {
  const Fixture fixture = small_fixture(true);
  const RecognitionResult recognized = recognize(fixture);
  assert(recognized.status == RecognitionStatus::Recognized);
  const PairEliminationPlan plan = plan_pair_elimination(
      recognized.theory, 0, 1, plan_bounds(),
      [](uint64_t) { return true; });
  assert(plan.status == PlanStatus::Planned);
  for (const Addition &addition : plan.additions)
    assert(rup(clauses(fixture), addition.clause));
}

void test_target_arity_shape_and_complete_rup_replay() {
  Fixture fixture;
  for (uint32_t domain = 0; domain < 7; ++domain) {
    const int32_t first = static_cast<int32_t>(3 * domain + 1);
    fixture.domain({first, first + 1, first + 2});
  }
  fixture.equation({0, 1, 2, 3}, {1, 2, 1, 2}, 1);
  fixture.equation({3, 4, 5, 6}, {1, 1, 2, 1}, 2);
  const RecognitionResult recognized = recognize(fixture);
  assert(recognized.status == RecognitionStatus::Recognized);
  PairEliminationPlan plan = plan_pair_elimination(
      recognized.theory, 0, 1, plan_bounds(),
      [](uint64_t) { return true; });
  assert(plan.status == PlanStatus::Planned);
  assert(plan.consequence.domains.size() == 6);
  assert(plan.consumed_result_tuples == 729);
  assert(plan.additions.size() == 486);
  const auto source_formula = clauses(fixture);
  for (const Addition &addition : plan.additions)
    assert(rup(source_formula, addition.clause));
}

void test_recognition_rejects_incomplete_duplicate_and_nonaffine_sources() {
  Fixture fixture = small_fixture();
  fixture.source.pop_back();
  RecognitionResult result = recognize(fixture);
  assert(result.status == RecognitionStatus::IncompleteTheory);
  assert(result.theory.source.empty());

  fixture = small_fixture();
  fixture.source.erase(fixture.source.begin() + 1); // first domain AMO
  result = recognize(fixture);
  assert(result.status == RecognitionStatus::IncompleteTheory);
  assert(result.theory.domains.empty());

  fixture = small_fixture();
  Premise duplicate = fixture.source.back();
  duplicate.occurrence = fixture.next++;
  duplicate.native_proof_id = 9000 + duplicate.occurrence;
  fixture.source.push_back(std::move(duplicate));
  result = recognize(fixture);
  assert(result.status == RecognitionStatus::IncompleteTheory);
  assert(result.theory.equations.empty());

  fixture = small_fixture();
  // Replace one forbidden tuple by one satisfying tuple while preserving the
  // group cardinality.  This is complete in count but has no affine meaning.
  fixture.source.back().literals = {-4, -7};
  result = recognize(fixture);
  assert(result.status == RecognitionStatus::IncompleteTheory);
  assert(result.theory.domains.empty());

  fixture = small_fixture();
  fixture.add({1, 4});
  result = recognize(fixture);
  assert(result.status == RecognitionStatus::IncompleteTheory);
  assert(result.theory.source.empty());
}

void test_overlap_and_stale_theory_declines() {
  Fixture fixture;
  for (uint32_t domain = 0; domain < 4; ++domain) {
    const int32_t first = static_cast<int32_t>(3 * domain + 1);
    fixture.domain({first, first + 1, first + 2});
  }
  fixture.equation({0, 1, 2}, {1, 1, 2}, 0);
  fixture.equation({1, 2, 3}, {1, 2, 1}, 1);
  RecognitionResult recognized = recognize(fixture);
  assert(recognized.status == RecognitionStatus::Recognized);
  check_decline(plan_pair_elimination(recognized.theory, 0, 1, plan_bounds(),
                                      [](uint64_t) { return true; }),
                PlanStatus::UnsupportedOverlap);

  Fixture disjoint;
  for (uint32_t domain = 0; domain < 4; ++domain) {
    const int32_t first = static_cast<int32_t>(3 * domain + 1);
    disjoint.domain({first, first + 1, first + 2});
  }
  disjoint.equation({0, 1}, {1, 2}, 0);
  disjoint.equation({2, 3}, {1, 1}, 1);
  recognized = recognize(disjoint);
  assert(recognized.status == RecognitionStatus::Recognized);
  check_decline(plan_pair_elimination(recognized.theory, 0, 1, plan_bounds(),
                                      [](uint64_t) { return true; }),
                PlanStatus::UnsupportedOverlap);

  fixture = small_fixture();
  recognized = recognize(fixture);
  assert(recognized.status == RecognitionStatus::Recognized);
  Theory stale = recognized.theory;
  const uint64_t required = stale.equations[0].forbidden_by_tuple[1];
  stale.source.erase(std::remove_if(stale.source.begin(), stale.source.end(),
                                    [&](const Premise &premise) {
                                      return premise.occurrence == required;
                                    }),
                     stale.source.end());
  check_decline(plan_pair_elimination(stale, 0, 1, plan_bounds(),
                                      [](uint64_t) { return true; }),
                PlanStatus::IncompleteTheory);
}

void test_recognition_and_planning_bounds_are_atomic() {
  const Fixture fixture = small_fixture();
  RecognitionBounds bounds = recognition_bounds();
  bounds.maximum_source_clauses = fixture.source.size() - 1;
  RecognitionResult recognized = tactix::gf3_affine::recognize(
      fixture.source, bounds, [](uint64_t) { return true; });
  assert(recognized.status == RecognitionStatus::BoundExceeded);
  assert(recognized.theory.source.empty());

  bounds = recognition_bounds();
  bounds.maximum_source_literals = 1;
  recognized = tactix::gf3_affine::recognize(
      fixture.source, bounds, [](uint64_t) { return true; });
  assert(recognized.status == RecognitionStatus::BoundExceeded);
  assert(recognized.theory.domains.empty());

  bounds = recognition_bounds();
  bounds.maximum_domains = 2;
  recognized = tactix::gf3_affine::recognize(
      fixture.source, bounds, [](uint64_t) { return true; });
  assert(recognized.status == RecognitionStatus::BoundExceeded);

  bounds = recognition_bounds();
  bounds.maximum_equations = 1;
  recognized = tactix::gf3_affine::recognize(
      fixture.source, bounds, [](uint64_t) { return true; });
  assert(recognized.status == RecognitionStatus::BoundExceeded);

  bounds = recognition_bounds();
  bounds.maximum_tuples_per_equation = 8;
  recognized = tactix::gf3_affine::recognize(
      fixture.source, bounds, [](uint64_t) { return true; });
  assert(recognized.status == RecognitionStatus::BoundExceeded);

  uint64_t remaining = 12;
  recognized = tactix::gf3_affine::recognize(
      fixture.source, recognition_bounds(), [&](uint64_t amount) {
        if (amount > remaining) return false;
        remaining -= amount;
        return true;
      });
  assert(recognized.status == RecognitionStatus::EffortExhausted);
  assert(recognized.consumed_work <= 12);
  assert(recognized.theory.equations.empty());

  recognized = recognize(fixture);
  assert(recognized.status == RecognitionStatus::Recognized);
  PairEliminationBounds plan_limit = plan_bounds();
  plan_limit.maximum_result_arity = 1;
  check_decline(plan_pair_elimination(recognized.theory, 0, 1, plan_limit,
                                      [](uint64_t) { return true; }),
                PlanStatus::BoundExceeded);
  plan_limit = plan_bounds();
  plan_limit.maximum_result_tuples = 8;
  check_decline(plan_pair_elimination(recognized.theory, 0, 1, plan_limit,
                                      [](uint64_t) { return true; }),
                PlanStatus::BoundExceeded);
  plan_limit = plan_bounds();
  plan_limit.maximum_additions = 5;
  check_decline(plan_pair_elimination(recognized.theory, 0, 1, plan_limit,
                                      [](uint64_t) { return true; }),
                PlanStatus::BoundExceeded);
  plan_limit = plan_bounds();
  plan_limit.maximum_addition_literals = 11;
  check_decline(plan_pair_elimination(recognized.theory, 0, 1, plan_limit,
                                      [](uint64_t) { return true; }),
                PlanStatus::BoundExceeded);
  remaining = 2;
  PairEliminationPlan exhausted = plan_pair_elimination(
      recognized.theory, 0, 1, plan_bounds(), [&](uint64_t amount) {
        if (amount > remaining) return false;
        remaining -= amount;
        return true;
      });
  check_decline(exhausted, PlanStatus::EffortExhausted);
  assert(exhausted.consumed_work <= 2);
}

} // namespace

int main() {
  test_exact_recognition_and_pair_elimination();
  test_signed_selection_literals();
  test_target_arity_shape_and_complete_rup_replay();
  test_recognition_rejects_incomplete_duplicate_and_nonaffine_sources();
  test_overlap_and_stale_theory_declines();
  test_recognition_and_planning_bounds_are_atomic();
  std::cout << "GF(3) affine recognition and consequence tests passed\n";
}
