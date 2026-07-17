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

// The handoff-consumption precondition (plan rev 35, corpus 006): Propagate
// must not run while a learned clause is pending. Otherwise it can find a
// SECOND conflict, reaching (conflict_valid && pending.valid) — a state where
// Backjump is blocked and only Analyze applies, so Analyze overwrites pending
// and ORPHANS the first learned clause: allocated, watched and in the proof,
// but with its asserting literal never assigned. Propagation is watch-driven,
// so nothing ever revisits it and it survives as an escaped unit.
static void test_propagate_requires_consumed_handoff() {
  // same 1-UIP fixture as test_uip, plus a 4th var so a trail suffix can
  // remain unpropagated while the conflict is analyzed
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c1[3] = {internalize(-1), internalize(-2), internalize(3)};
  const Lit c2[3] = {internalize(-1), internalize(-2), internalize(-3)};
  kernel_add_large_clause(k, c1, 3, false, 0);
  kernel_add_large_clause(k, c2, 3, false, 0);
  kernel_add_binary_clause(k, internalize(-4), internalize(1)); // 4 -> 1

  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(2));
  assert(propagate_apply(k) == ApplyResult::Conflict);
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.pending.valid);

  // an unpropagated suffix + a pending clause is exactly the corpus-006 state
  kernel_assign_unit(k, internalize(4)); // un-propagated
  assert(k.propagate_head < k.trail.size());
  assert(!k.conflict_valid);
  // Propagate's OTHER conditions all hold — only the handoff blocks it
  assert(!propagate_guard(k));
  // ...and Backjump is applicable in its place, so no schedule is stuck (N1)
  assert(backjump_guard(k));
  assert(backjump_apply(k) == ApplyResult::Ok);
  assert(!k.pending.valid);
  // handoff consumed: Propagate is available again
  assert(propagate_guard(k));
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

static void test_chrono_backjump_clamps_to_jump_level() {
  // c1 = (-1 -3 4), c2 = (-1 -3 -4); decide 1 (level 1), decide 2 (level
  // 2, unrelated), decide 3 (level 3): c1 forces 4, c2 conflicts. 1-UIP
  // learns (-3 -1) with jump level 1. A chronological target (2, above the
  // assertion level) is UNSOUND under this kernel's current-level
  // assignment: it would stamp the UIP at level 2, and a later partial
  // restart to level 1 would strand the learned clause as an escaped unit
  // (corpus 005). So backjump clamps ANY target to jump_level and asserts
  // the UIP at its TRUE level; genuine chronological backtracking returns
  // with the §5.4 true-level-assignment kernel.
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
  assert(backjump_apply_to(k, 2) == ApplyResult::Ok); // chrono target...
  assert(k.level == 1);                    // ...clamped down to jump_level 1
  assert(k.values[internalize(2)] == 0);   // level-2 decision abandoned
  assert(k.values[internalize(-3)] > 0);   // UIP asserted...
  assert(k.assigned[var_of(internalize(3))].level == 1); // ...at its TRUE level
  assert(!k.pending.valid);
  assert(default_solve(k) == 10); // still satisfiable: 1 -3 (2 re-decided)
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
  test_propagate_requires_consumed_handoff();
  test_learned_unit();
  test_shrink();
  test_otfs();
  test_chrono_backjump_clamps_to_jump_level();
  test_backjump_target_clamps();
  puts("test_analyze OK");
  return 0;
}
