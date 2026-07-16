// M2 Decide: the guard's propagation-fixpoint precondition (corpus 004).
// A state-advancing rule (Decide, and all inprocessing) may only run at a
// propagation fixpoint; deciding on an un-propagated trail would mis-level
// the pending implications (this kernel assigns forced literals at the
// current level, not their true level), letting a conflict escape.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

static void test_decide_requires_propagation_fixpoint() {
  Kernel k;
  kernel_reserve_vars(k, 4); // var 4 stays free so Decide has work post-BCP
  kernel_add_binary_clause(k, internalize(-1), internalize(2)); // 1 -> 2
  kernel_add_binary_clause(k, internalize(-2), internalize(3)); // 2 -> 3
  // assert unit 1 at root, UN-propagated
  kernel_assign_unit(k, internalize(1));
  assert(k.propagate_head < k.trail.size()); // pending propagation
  // Decide must NOT be applicable while propagation is pending — else a
  // decision would push a new level over the un-propagated unit, and 1's
  // implications (2, then 3) would be forced at the decision level, not root
  assert(!decide_guard(k));
  // draining propagation to fixpoint re-enables Decide
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.propagate_head == k.trail.size());
  assert(k.value(internalize(2)) > 0 && k.value(internalize(3)) > 0);
  assert(decide_guard(k));
}

// a fully unassigned formula is a (trivial) fixpoint: Decide is immediately
// applicable, no propagation needed.
static void test_decide_ok_at_trivial_fixpoint() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  assert(k.propagate_head == k.trail.size()); // empty trail = fixpoint
  assert(decide_guard(k));
  assert(decide_apply(k, internalize(1), true) == ApplyResult::Ok);
  // after deciding, 1 is un-propagated → Decide blocked again until BCP
  assert(!decide_guard(k));
}

int main() {
  test_decide_requires_propagation_fixpoint();
  test_decide_ok_at_trivial_fixpoint();
  puts("test_decide OK");
  return 0;
}
