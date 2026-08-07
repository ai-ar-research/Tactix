// Decide's guard carries no propagation-fixpoint conjunct, so a policy may
// stack decisions without propagating between them (N1), and backtracking
// from such a state must leave no clause unit with no watch firing. Ledger
// row D-LEVEL-1 in docs/implementation-notes.md.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

// The guard holds on an un-propagated trail: off-fixpoint decisions are legal.
static void test_decide_guard_is_pure_off_fixpoint() {
  Kernel k;
  kernel_reserve_vars(k, 4); // var 4 stays free so Decide always has work
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // 1 -> 2
  kernel_add_binary_clause(k, internalize(-2), internalize(3)); // 2 -> 3
  kernel_assign_unit(k, internalize(1));     // root unit, un-propagated
  assert(k.propagate_head < k.trail.size()); // propagation pending
  assert(decide_guard(k));                   // still applicable
  // decide stacks a decision over the un-drained unit
  assert(decide_apply(k, internalize(4), true) == ApplyResult::Ok);
  assert(k.level == 1 && k.propagate_head < k.trail.size());
  assert(decide_guard(k)); // and again, still off a fixpoint
}

// The escaped-unit hazard: a watch scan treats its clause as satisfied by an
// ahead decision, which a later backtrack unassigns. `trail_tainted` forces
// the full re-propagation that restores the fixpoint invariants.
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
  // decide 2 off a fixpoint: the ahead assignment, which taints the trail
  assert(decide_apply(k, internalize(2), true) == ApplyResult::Ok);
  assert(k.trail_tainted);

  // the clause looks satisfied, so lit 2 is never derived by it
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.propagate_head == k.trail.size());
  assert(k.value(internalize(2)) > 0); // true by decision (not by the clause)

  // now the clause is unit on 2 with a watch that will not re-fire
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
