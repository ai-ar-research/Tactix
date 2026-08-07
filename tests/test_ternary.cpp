// HyperTernaryRes unit tests: the two collapsing shapes, the three rejections
// cadical's `hyper_ternary_resolve` makes, and the instance surface.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/ternary/ternary.hpp"

#include <cassert>
#include <csignal>
#include <cstdio>
#include <vector>

using namespace tactix;

static void add3(Kernel &k, int a, int b, int c) {
  const Lit l[3] = {internalize(a), internalize(b), internalize(c)};
  kernel_add_large_clause(k, l, 3, /*redundant=*/false, 0);
}

// One shared non-pivot literal collapses the resolvent to a ternary.
static void test_ternary_resolvent() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add3(k, 1, 2, 3);
  add3(k, -1, 2, 4);
  assert(ternary_guard(k));
  assert(ternary_apply(k) == ApplyResult::Ok);
  assert(k.stats.ternary_resolvents == 1);
  assert(k.stats.ternary_binaries == 0);
  assert(default_solve(k) == 10);
}

// Two shared non-pivot literals collapse it to a binary, which is permanent.
static void test_binary_resolvent() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add3(k, 1, 2, 3);
  add3(k, -1, 2, 3);
  assert(ternary_apply(k) == ApplyResult::Ok);
  assert(k.stats.ternary_binaries == 1);
  assert(k.stats.ternary_resolvents == 0);
  assert(default_solve(k) == 10);
}

// Disjoint non-pivot literals give a quaternary resolvent: rejected.
static void test_quaternary_rejected() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  add3(k, 1, 2, 3);
  add3(k, -1, 4, 5);
  assert(ternary_apply(k) == ApplyResult::NoOp);
  assert(k.stats.ternary_resolvents == 0 && k.stats.ternary_binaries == 0);
}

// A shared literal in opposite polarity makes the resolvent tautological: it
// is rejected, since a tautological LRAT line is uncheckable.
static void test_tautological_rejected() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add3(k, 1, 2, 3);
  add3(k, -1, -2, 3);
  assert(ternary_apply(k) == ApplyResult::NoOp);
  assert(k.stats.ternary_resolvents == 0 && k.stats.ternary_binaries == 0);
}

// An existing resolvent is not added twice; the rule reaches a fixpoint.
static void test_duplicate_rejected() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add3(k, 1, 2, 3);
  add3(k, -1, 2, 4);
  add3(k, 2, 3, 4); // the resolvent, already present
  assert(ternary_apply(k) == ApplyResult::NoOp);
  assert(k.stats.ternary_resolvents == 0);
}

static void test_idempotent() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add3(k, 1, 2, 3);
  add3(k, -1, 2, 4);
  assert(ternary_apply(k) == ApplyResult::Ok);
  assert(ternary_apply(k) == ApplyResult::NoOp); // fixpoint reached
}

// N1: pivots are literal payloads, and an out-of-range one is a total NoOp.
static void test_instances() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add3(k, 1, 2, 3);
  add3(k, -1, 2, 4);
  std::vector<Lit> pivots;
  ternary_pivots(k, pivots);
  assert(pivots.size() == 1 && pivots[0] == internalize(1));
  assert(ternary_try(k, internalize(2)) == ApplyResult::NoOp); // no ¬2 ternary
  assert(ternary_try(k, internalize(1)) == ApplyResult::Ok);
  assert(k.stats.ternary_resolvents == 1);
}

// N1: the touched-epoch filter is policy. The canonical round may decline on
// it, but `ternary_pivots` and `ternary_try` must never filter on it.
static void test_touch_filter_is_policy_only() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add3(k, 1, 2, 3);
  add3(k, -1, 2, 4);
  k.ternary_touch_mark = k.touch_epoch;
  assert(ternary_apply(k) == ApplyResult::NoOp);
  assert(k.stats.ternary_mark_skipped == 1);
  std::vector<Lit> pivots;
  ternary_pivots(k, pivots); // not narrowed by the mark
  assert(pivots.size() == 1 && pivots[0] == internalize(1));
  assert(ternary_try(k, internalize(1)) == ApplyResult::Ok);
  assert(k.stats.ternary_resolvents == 1);
}

// Both interrupt sites honour a raised stop flag; the third block is the
// control showing the abort, not the formula, suppressed the resolvent.
static void test_interrupt() {
  volatile sig_atomic_t stop = 1;
  { // the round loop over pivots
    Kernel k;
    kernel_reserve_vars(k, 4);
    add3(k, 1, 2, 3);
    add3(k, -1, 2, 4);
    KernelStopPublisher pub(k, &stop);
    assert(ternary_apply(k) == ApplyResult::NoOp);
    assert(k.stats.ternary_resolvents == 0);
    assert(k.stats.ternary_interrupted == 1);
    assert(k.stats.rule_interrupted == 1);
  }
  { // the per-pivot pair loop, through the single-instance entry
    Kernel k;
    kernel_reserve_vars(k, 4);
    add3(k, 1, 2, 3);
    add3(k, -1, 2, 4);
    KernelStopPublisher pub(k, &stop);
    assert(ternary_try(k, internalize(1)) == ApplyResult::NoOp);
    assert(k.stats.ternary_resolvents == 0);
    assert(k.stats.ternary_interrupted == 1);
  }
  {
    Kernel k;
    kernel_reserve_vars(k, 4);
    add3(k, 1, 2, 3);
    add3(k, -1, 2, 4);
    assert(ternary_try(k, internalize(1)) == ApplyResult::Ok);
    assert(k.stats.ternary_resolvents == 1 && k.stats.ternary_interrupted == 0);
  }
}

int main() {
  test_interrupt();
  test_ternary_resolvent();
  test_touch_filter_is_policy_only();
  test_binary_resolvent();
  test_quaternary_rejected();
  test_tautological_rejected();
  test_duplicate_rejected();
  test_idempotent();
  test_instances();
  std::printf("test_ternary: all passed\n");
  return 0;
}
