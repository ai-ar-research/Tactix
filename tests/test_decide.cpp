// M2 Decide — D-LEVEL-1 (calculus §3, ledger row D-LEVEL-1). Decide's guard is
// a PURE soundness precondition: an unassigned variable, no recorded conflict,
// F not refuted. It carries NO propagation-fixpoint requirement — a policy may
// take multiple decisions in a row without unit propagation between them (the
// N1 capability a learned/LLM policy reasons over). This test pins both halves:
// (1) the guard holds off a fixpoint, and (2) doing so and then backtracking
// does NOT leave a clause unit with no watch firing (the escaped-unit hazard
// the trail_tainted mechanism cures).
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// The guard is pure: it holds on an UN-propagated trail (propagation pending),
// so an off-fixpoint decision is a legal move — not forbidden as it once was.
static void test_decide_guard_is_pure_off_fixpoint() {
  Kernel k;
  kernel_reserve_vars(k, 4); // var 4 stays free so Decide always has work
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // 1 -> 2
  kernel_add_binary_clause(k, internalize(-2), internalize(3)); // 2 -> 3
  kernel_assign_unit(k, internalize(1));       // root unit, UN-propagated
  assert(k.propagate_head < k.trail.size());   // propagation pending
  assert(decide_guard(k));                     // still applicable (D-LEVEL-1)
  // and decide really can fire here, stacking a decision over the un-drained
  // unit — the multiple-decisions-without-propagation capability
  assert(decide_apply(k, internalize(4), true) == ApplyResult::Ok);
  assert(k.level == 1 && k.propagate_head < k.trail.size());
  assert(decide_guard(k)); // and again, still off a fixpoint
}

// The escaped-unit regression (the reason D-LEVEL-1 needed a kernel change, not
// just a guard edit). Deterministically drive the exact hazard: a watch scan on
// a false literal treats its clause as satisfied by an "ahead" decision, then a
// backtrack unassigns that decision. Without the trail_tainted full
// re-propagation, clause (1 v 2 v 3) is left unit on 2 with no watch firing;
// with it, 2 is correctly propagated and the fixpoint invariants hold.
static void test_off_fixpoint_decide_then_backtrack_no_escape() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c, 3, /*redundant=*/false, /*glue=*/0);
  // watched on lits[0]=1 and lits[1]=2; lit 3 is unwatched.

  kernel_assign_unit(k, internalize(-3)); // c = lit 3 false at root
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.propagate_head == k.trail.size()); // clean root fixpoint

  // decide ¬1 (lit 1 false) at level 1 — in-fixpoint, so no taint yet
  assert(decide_apply(k, internalize(-1), true) == ApplyResult::Ok);
  assert(!k.trail_tainted);
  // decide 2 (lit 2 TRUE) at level 2 — OFF a fixpoint (¬1 still un-propagated):
  // this is the ahead assignment, and it taints the trail
  assert(decide_apply(k, internalize(2), true) == ApplyResult::Ok);
  assert(k.trail_tainted);

  // propagate to a fixpoint: scanning ¬1 sees clause (1 v 2 v 3) satisfied by
  // the true lit 2 (the ahead decision) — lit 2 is never derived, the watch
  // stays on the false lit 1
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.propagate_head == k.trail.size());
  assert(k.value(internalize(2)) > 0); // true by decision (not by the clause)

  // backtrack to level 1 (a search backtrack): lit 2 (level 2) is unassigned,
  // so clause (1 v 2 v 3) becomes unit on 2 — but its watch on the false lit 1
  // will not re-fire on its own. The taint forces a full re-propagation.
  kernel_backtrack(k, 1);
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_check_invariants(k); // aborts on an escaped unit — the actual guard
  assert(k.propagate_head == k.trail.size());
  assert(k.value(internalize(2)) > 0); // re-derived as a unit of the clause
}

int main() {
  test_decide_guard_is_pure_off_fixpoint();
  test_off_fixpoint_decide_then_backtrack_no_escape();
  puts("test_decide OK");
  return 0;
}
