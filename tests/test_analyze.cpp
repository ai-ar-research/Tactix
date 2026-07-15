// Analyze/Backjump unit test: forced 1-UIP scenario with a known learned
// clause and backjump level.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

static void test_uip() {
  // c1 = (-1 -2 3), c2 = (-1 -2 -3); decide 1, decide 2:
  // c1 forces 3, c2 conflicts; 1-UIP learns (-2 -1) with UIP 2, jump to 1
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c1[3] = {internalize(-1), internalize(-2), internalize(3)};
  const Lit c2[3] = {internalize(-1), internalize(-2), internalize(-3)};
  kernel_add_large_clause(k, c1, 3, false, 0);
  kernel_add_large_clause(k, c2, 3, false, 0);

  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(2));
  assert(propagate_apply(k) == ApplyResult::Conflict);

  assert(analyze_guard(k));
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.pending.valid);
  assert(k.pending.lits.size() == 2);
  assert(k.pending.lits[0] == internalize(-2)); // negated UIP
  assert(k.pending.lits[1] == internalize(-1));
  assert(k.pending.jump_level == 1);

  assert(backjump_guard(k));
  assert(backjump_apply(k) == ApplyResult::Ok);
  assert(k.level == 1);
  assert(k.values[internalize(-2)] > 0); // asserted
  assert(!k.pending.valid);

  // and the search must now conclude SAT (model: 1, -2, any 3)
  assert(default_solve(k) == 10);
}

static void test_learned_unit() {
  // (-1 2), (-1 -2): deciding 1 conflicts and learns unit -1
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Conflict);
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.pending.lits.size() == 1);
  assert(k.pending.lits[0] == internalize(-1));
  assert(k.pending.jump_level == 0);
  assert(backjump_apply(k) == ApplyResult::Ok);
  assert(k.level == 0);
  assert(k.values[internalize(-1)] > 0);
  assert(k.stats.learned_units == 1);
}

static void test_shrink() {
  // level 1: 1 forces 2 and 3; level 2: 4 forces 5 then 6. The conflict
  // clause (-6 -5 -2 -3) leaves the two-literal level-1 block {-2, -3} in
  // the 1-UIP clause; neither literal is removable by plain minimization
  // (their chains hit the decision 1, which is not in the clause), but the
  // block's walk converges to that decision, so shrinking replaces it by
  // -1: learned (-4 -1) instead of (-4 -2 -3).
  Kernel k;
  kernel_reserve_vars(k, 6);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  const Lit c3[4] = {internalize(-4), internalize(-2), internalize(-3),
                     internalize(5)};
  kernel_add_large_clause(k, c3, 4, false, 0);
  const Lit c4[3] = {internalize(-4), internalize(-5), internalize(6)};
  kernel_add_large_clause(k, c4, 3, false, 0);
  const Lit c5[4] = {internalize(-6), internalize(-5), internalize(-2),
                     internalize(-3)};
  kernel_add_large_clause(k, c5, 4, false, 0);

  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(4));
  assert(propagate_apply(k) == ApplyResult::Conflict);

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.shrunken == 1);
  assert(k.pending.lits.size() == 2);
  assert(k.pending.lits[0] == internalize(-4)); // negated UIP
  assert(k.pending.lits[1] == internalize(-1)); // shrunken level-1 block
  assert(k.pending.jump_level == 1);
  assert(backjump_apply(k) == ApplyResult::Ok);
  assert(default_solve(k) == 10); // still satisfiable (1 2 3, -4)
}

static void test_otfs() {
  // r = (-1 -2 3) propagates 3 at level 2 (after deciding 1, then 2);
  // d = (-1 -2 -3) then conflicts. Resolving d with r on 3 yields (-1 -2),
  // which is smaller than both: OTFS strengthens r to the binary (-1 -2)
  // and, since this was the first resolution and the resolvent is smaller
  // than d, subsumes (deletes) d. Analysis restarts on (-1 -2) and learns
  // it as the driving clause with UIP 2.
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit r[3] = {internalize(-1), internalize(-2), internalize(3)};
  const Lit d[3] = {internalize(-1), internalize(-2), internalize(-3)};
  const Ref rref = kernel_add_large_clause(k, r, 3, false, 0);
  const Ref dref = kernel_add_large_clause(k, d, 3, false, 0);

  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(2));
  assert(propagate_apply(k) == ApplyResult::Conflict);
  assert(k.conflict_ref == dref);

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.otfs_strengthened == 1);
  assert(k.stats.otfs_subsumed == 1);
  assert(clause_at(k.arena, dref)->garbage()); // conflict subsumed
  assert(!clause_at(k.arena, rref)->garbage()); // reason kept (deviation)
  assert(k.pending.valid);
  assert(k.pending.lits.size() == 2);
  assert(k.pending.lits[0] == internalize(-2)); // negated UIP
  assert(k.pending.lits[1] == internalize(-1));
  assert(k.pending.jump_level == 1);
  assert(backjump_apply(k) == ApplyResult::Ok);
  assert(default_solve(k) == 10); // satisfiable: 1, -2
}

static void test_binres() {
  // Level 1: decide 1, (-1 2) forces 2. Level 2: decide 3, (-3 4) forces
  // 4, (-4 5) forces 5, e = (-4 -2 -5) conflicts. Resolving 5 through its
  // BINARY reason (no OTFS on binary reasons) leaves the 1-UIP clause
  // (-4 -2); neither literal is removable recursively (both chains hit
  // decisions). The planted binary (-4 2) then resolves -2 away: the
  // learned clause becomes the unit (-4). The binary (-4 5) must NOT fire
  // (var 5 was resolved away — SEEN but not in the clause), which is
  // exactly the fresh-marking subtlety binres_minimize handles.
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-3), internalize(4));
  kernel_add_binary_clause(k, internalize(-4), internalize(5));
  const Lit e[3] = {internalize(-4), internalize(-2), internalize(-5)};
  kernel_add_large_clause(k, e, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-4), internalize(2));

  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(3));
  assert(propagate_apply(k) == ApplyResult::Conflict);

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.binres_minimized == 1);
  assert(k.stats.minimized == 0); // the removal is binres's alone
  assert(k.pending.valid);
  assert(k.pending.lits.size() == 1); // (-4 -2) minimized to the unit -4
  assert(k.pending.lits[0] == internalize(-4));
  assert(k.pending.jump_level == 0);
  assert(backjump_apply(k) == ApplyResult::Ok);
  assert(k.level == 0);
  assert(k.values[internalize(-4)] > 0);
  assert(default_solve(k) == 10); // satisfiable: 1 2 -3 -4
}

static void test_chrono_backjump() {
  // c1 = (-1 -3 4), c2 = (-1 -3 -4); decide 1 (level 1), decide 2 (level
  // 2, unrelated), decide 3 (level 3): c1 forces 4, c2 conflicts. 1-UIP
  // learns (-3 -1) with jump level 1. The N1 surface may land anywhere in
  // [jump, conflict): target 2 is chronological backtracking — level 2's
  // unrelated decision survives and the UIP asserts at level 2.
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c1[3] = {internalize(-1), internalize(-3), internalize(4)};
  const Lit c2[3] = {internalize(-1), internalize(-3), internalize(-4)};
  kernel_add_large_clause(k, c1, 3, false, 0);
  kernel_add_large_clause(k, c2, 3, false, 0);

  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(2));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(3));
  assert(propagate_apply(k) == ApplyResult::Conflict);

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.pending.lits.size() == 2);
  assert(k.pending.lits[0] == internalize(-3)); // negated UIP
  assert(k.pending.jump_level == 1);
  assert(backjump_apply_to(k, 2) == ApplyResult::Ok); // chrono target
  assert(k.level == 2);
  assert(k.values[internalize(2)] > 0);  // level-2 decision kept
  assert(k.values[internalize(-3)] > 0); // asserted...
  assert(k.assigned[var_of(internalize(3))].level == 2); // ...at level 2
  assert(!k.pending.valid);
  assert(default_solve(k) == 10); // satisfiable: 1 2 -3
}

static void test_backjump_target_clamps() {
  // targets below the assertion level clamp up to it: below jump_level
  // the learned clause would no longer be asserting
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c1[3] = {internalize(-1), internalize(-2), internalize(3)};
  const Lit c2[3] = {internalize(-1), internalize(-2), internalize(-3)};
  kernel_add_large_clause(k, c1, 3, false, 0);
  kernel_add_large_clause(k, c2, 3, false, 0);
  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(2));
  assert(propagate_apply(k) == ApplyResult::Conflict);
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.pending.jump_level == 1);
  assert(backjump_apply_to(k, 0) == ApplyResult::Ok); // clamps to 1
  assert(k.level == 1);
  assert(k.values[internalize(-2)] > 0); // asserted at the jump level
  assert(default_solve(k) == 10);
}

int main() {
  test_uip();
  test_learned_unit();
  test_shrink();
  test_otfs();
  test_binres();
  test_chrono_backjump();
  test_backjump_target_clamps();
  puts("test_analyze OK");
  return 0;
}
