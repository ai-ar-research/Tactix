// X1 BCE unit tests: a genuinely blocked clause, a NON-blocked one (the
// negative case that proves the tautology test is doing work), the pure-literal
// case BCE subsumes, the redundant-clause exclusion, and — the property that
// makes BCE different from every other deletion in this kernel — MODEL
// RECONSTRUCTION: deleting a blocked clause only preserves satisfiability, so
// every original clause must still be satisfied after `kernel_extend_model`.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/bce/bce.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace tactix;

// (1 ∨ 2) and (¬1 ∨ ¬2): the only resolvent on 1 is (2 ∨ ¬2), tautological, so
// (1 ∨ 2) is blocked on 1.
static void test_blocked_binary() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  assert(bce_guard(k));
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(k.stats.bce_deleted == 1);
}

// (1 ∨ 2) against (¬1 ∨ 3): the resolvent (2 ∨ 3) is NOT tautological, so
// nothing is blocked on 1. Without a working tautology test this would delete.
static void test_not_blocked() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  assert(bce_try(k, internalize(1)) == ApplyResult::NoOp);
  assert(k.stats.bce_deleted == 0);
}

// A literal with NO opposite occurrence blocks vacuously — BCE subsumes the
// pure-literal rule (deviation 4).
static void test_pure_literal() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2)); // 1 is pure
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(k.stats.bce_deleted == 2);
}

// Redundant clauses are not candidates (deviation 1): Reduce may drop them for
// free, so blocking them would only put needless segments on ρ.
static void test_redundant_skipped() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, /*redundant=*/true, 3);
  assert(bce_try(k, internalize(1)) == ApplyResult::NoOp);
  assert(k.stats.bce_deleted == 0);
}

// The reconstruction obligation. (1∨2),(¬1∨¬2) — BCE deletes a blocked clause,
// the solver then finds a model of what remains, and the extended model must
// satisfy BOTH original clauses.
static void test_model_reconstruction() {
  const std::vector<std::vector<int>> orig = {{1, 2}, {-1, -2}};
  Kernel k;
  kernel_reserve_vars(k, 2);
  for (const auto &cl : orig)
    kernel_add_binary_clause(k, internalize(cl[0]), internalize(cl[1]));
  assert(bce_apply(k) == ApplyResult::Ok);
  assert(k.stats.bce_deleted >= 1);
  assert(default_solve(k) == 10);
  kernel_extend_model(k);
  for (const auto &cl : orig) {
    bool sat = false;
    for (const int e : cl)
      sat |= (k.values[internalize(e)] > 0);
    assert(sat && "every original clause satisfied by the extended model");
  }
}

// Instance surface (N1): literals are offered as payloads and an unrelated
// payload is a total NoOp.
static void test_instances() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  std::vector<Lit> lits;
  bce_literals(k, lits);
  assert(lits.size() == 4); // both polarities of both variables
  assert(bce_try(k, internalize(2)) == ApplyResult::Ok); // symmetric here
}

// Regression, fuzz case 2204 (seed 20262913, shrunk 91 -> 6 clauses): BCE
// deleted one clause and the extended model then falsified an ORIGINAL clause.
// The formula's point is that root propagation assigns 39,9,33 — so the two
// ternaries carry an assigned literal — while (19 ∨ 4) does not.
static void test_regression_fuzz_2204() {
  const std::vector<std::vector<int>> orig = {
      {19, 4}, {33, -9}, {-34, -33, -4}, {34, -33, 4}, {39, 9}, {-39}};
  Kernel k;
  kernel_reserve_vars(k, 39);
  for (const auto &cl : orig) {
    std::vector<Lit> l;
    for (const int e : cl)
      l.push_back(internalize(e));
    if (l.size() == 1)
      kernel_assign_unit(k, l[0]);
    else if (l.size() == 2)
      kernel_add_binary_clause(k, l[0], l[1]);
    else
      kernel_add_large_clause(k, l.data(), (uint32_t)l.size(), false, 0);
  }
  bce_apply(k);
  assert(default_solve(k) == 10);
  kernel_extend_model(k);
  for (const auto &cl : orig) {
    bool sat = false;
    for (const int e : cl)
      sat |= (k.values[internalize(e)] > 0);
    if (!sat) {
      std::printf("UNSATISFIED after reconstruction:");
      for (const int e : cl)
        std::printf(" %d", e);
      std::printf("\n");
    }
    assert(sat && "every original clause satisfied by the extended model");
  }
}

int main() {
  test_regression_fuzz_2204();
  test_blocked_binary();
  test_not_blocked();
  test_pure_literal();
  test_redundant_skipped();
  test_model_reconstruction();
  test_instances();
  std::printf("test_bce: all passed\n");
  return 0;
}
