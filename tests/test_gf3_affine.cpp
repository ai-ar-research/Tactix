#undef NDEBUG
#include "gf3_formula_support.hpp"
#include "clause_oracle.hpp"
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

using Fixture = Gf3Formula;

RecognitionBounds recognition_bounds() {
  return {10000, 100000, 100, 100, 10, 60000};
}

PairEliminationBounds plan_bounds() {
  return {10, 60000, 60000, 1000000};
}

RecognitionResult recognition_of(const Fixture &fixture) {
  return tactix::gf3_affine::recognize(
      fixture.source, recognition_bounds(), [](uint64_t) { return true; });
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
  const RecognitionResult recognized = recognition_of(fixture);
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

  const auto &source_formula = fixture.clauses;
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
    assert(rup_implied(local_proof_formula, addition.clause));
    assert(rup_implied(source_formula, addition.clause));
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
  const RecognitionResult recognized = recognition_of(fixture);
  assert(recognized.status == RecognitionStatus::Recognized);
  const PairEliminationPlan plan = plan_pair_elimination(
      recognized.theory, 0, 1, plan_bounds(),
      [](uint64_t) { return true; });
  assert(plan.status == PlanStatus::Planned);
  for (const Addition &addition : plan.additions)
    assert(rup_implied(fixture.clauses, addition.clause));
}

void test_target_arity_shape_and_complete_rup_replay() {
  Fixture fixture = gf3_domains(7);
  fixture.equation({0, 1, 2, 3}, {1, 2, 1, 2}, 1);
  fixture.equation({3, 4, 5, 6}, {1, 1, 2, 1}, 2);
  const RecognitionResult recognized = recognition_of(fixture);
  assert(recognized.status == RecognitionStatus::Recognized);
  PairEliminationPlan plan = plan_pair_elimination(
      recognized.theory, 0, 1, plan_bounds(),
      [](uint64_t) { return true; });
  assert(plan.status == PlanStatus::Planned);
  assert(plan.consequence.domains.size() == 6);
  assert(plan.consumed_result_tuples == 729);
  assert(plan.additions.size() == 486);
  const auto &source_formula = fixture.clauses;
  for (const Addition &addition : plan.additions)
    assert(rup_implied(source_formula, addition.clause));
}

void test_recognition_rejects_incomplete_duplicate_and_nonaffine_sources() {
  Fixture fixture = small_fixture();
  fixture.source.pop_back();
  RecognitionResult result = recognition_of(fixture);
  assert(result.status == RecognitionStatus::IncompleteTheory);
  assert(result.theory.source.empty());

  fixture = small_fixture();
  fixture.source.erase(fixture.source.begin() + 1); // first domain AMO
  result = recognition_of(fixture);
  assert(result.status == RecognitionStatus::IncompleteTheory);
  assert(result.theory.domains.empty());

  fixture = small_fixture();
  Premise duplicate = fixture.source.back();
  duplicate.occurrence = fixture.source.size() + 1;
  duplicate.native_proof_id = 9000 + duplicate.occurrence;
  fixture.source.push_back(std::move(duplicate));
  result = recognition_of(fixture);
  assert(result.status == RecognitionStatus::IncompleteTheory);
  assert(result.theory.equations.empty());

  fixture = small_fixture();
  // Replace one forbidden tuple by one satisfying tuple while preserving the
  // group cardinality.  This is complete in count but has no affine meaning.
  fixture.source.back().literals = {-4, -7};
  result = recognition_of(fixture);
  assert(result.status == RecognitionStatus::IncompleteTheory);
  assert(result.theory.domains.empty());

  fixture = small_fixture();
  fixture.add({1, 4});
  result = recognition_of(fixture);
  assert(result.status == RecognitionStatus::IncompleteTheory);
  assert(result.theory.source.empty());
}

void test_overlap_and_stale_theory_declines() {
  Fixture fixture = gf3_domains(4);
  fixture.equation({0, 1, 2}, {1, 1, 2}, 0);
  fixture.equation({1, 2, 3}, {1, 2, 1}, 1);
  RecognitionResult recognized = recognition_of(fixture);
  assert(recognized.status == RecognitionStatus::Recognized);
  check_decline(plan_pair_elimination(recognized.theory, 0, 1, plan_bounds(),
                                      [](uint64_t) { return true; }),
                PlanStatus::UnsupportedOverlap);

  Fixture disjoint = gf3_domains(4);
  disjoint.equation({0, 1}, {1, 2}, 0);
  disjoint.equation({2, 3}, {1, 1}, 1);
  recognized = recognition_of(disjoint);
  assert(recognized.status == RecognitionStatus::Recognized);
  check_decline(plan_pair_elimination(recognized.theory, 0, 1, plan_bounds(),
                                      [](uint64_t) { return true; }),
                PlanStatus::UnsupportedOverlap);

  fixture = small_fixture();
  recognized = recognition_of(fixture);
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
  const auto ample = [](uint64_t) { return true; };
  for (int bound = 0; bound < 5; ++bound) {
    RecognitionBounds bounds = recognition_bounds();
    switch (bound) {
    case 0: bounds.maximum_source_clauses = fixture.source.size() - 1; break;
    case 1: bounds.maximum_source_literals = 1; break;
    case 2: bounds.maximum_domains = 2; break;
    case 3: bounds.maximum_equations = 1; break;
    case 4: bounds.maximum_tuples_per_equation = 8; break;
    }
    const RecognitionResult bounded =
        tactix::gf3_affine::recognize(fixture.source, bounds, ample);
    assert(bounded.status == RecognitionStatus::BoundExceeded);
    assert(bounded.theory.source.empty() && bounded.theory.domains.empty() &&
           bounded.theory.equations.empty());
  }

  uint64_t remaining = 12;
  const auto metered = [&](uint64_t amount) {
    if (amount > remaining) return false;
    remaining -= amount;
    return true;
  };
  RecognitionResult recognized = tactix::gf3_affine::recognize(
      fixture.source, recognition_bounds(), metered);
  assert(recognized.status == RecognitionStatus::EffortExhausted);
  assert(recognized.consumed_work <= 12);
  assert(recognized.theory.equations.empty());

  recognized = recognition_of(fixture);
  assert(recognized.status == RecognitionStatus::Recognized);
  for (int bound = 0; bound < 4; ++bound) {
    PairEliminationBounds plan_limit = plan_bounds();
    switch (bound) {
    case 0: plan_limit.maximum_result_arity = 1; break;
    case 1: plan_limit.maximum_result_tuples = 8; break;
    case 2: plan_limit.maximum_additions = 5; break;
    case 3: plan_limit.maximum_addition_literals = 11; break;
    }
    check_decline(plan_pair_elimination(recognized.theory, 0, 1, plan_limit, ample),
                  PlanStatus::BoundExceeded);
  }
  remaining = 2;
  PairEliminationPlan exhausted =
      plan_pair_elimination(recognized.theory, 0, 1, plan_bounds(), metered);
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
