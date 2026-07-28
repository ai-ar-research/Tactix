// X2 HyperTernaryRes unit tests: the two collapsing shapes (ternary and binary
// resolvent), the three rejections cadical's `hyper_ternary_resolve` makes
// (quaternary, tautological, already present), the redundant/permanent split,
// and the instance surface.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/ternary/ternary.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace tactix;

static void add3(Kernel &k, int a, int b, int c) {
  const Lit l[3] = {internalize(a), internalize(b), internalize(c)};
  kernel_add_large_clause(k, l, 3, /*redundant=*/false, 0);
}

// (1 ∨ 2 ∨ 3) and (¬1 ∨ 2 ∨ 4) share literal 2, so resolving on 1 collapses to
// the TERNARY (2 ∨ 3 ∨ 4) — the rule's reason for existing.
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

// (1 ∨ 2 ∨ 3) and (¬1 ∨ 2 ∨ 3) share BOTH non-pivot literals, so the resolvent
// is the BINARY (2 ∨ 3) — the valuable case (binaries are permanent here).
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

// Disjoint non-pivot literals ⇒ a QUATERNARY resolvent ⇒ rejected, nothing
// added (cadical's `size > 3` rejection).
static void test_quaternary_rejected() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  add3(k, 1, 2, 3);
  add3(k, -1, 4, 5);
  assert(ternary_apply(k) == ApplyResult::NoOp);
  assert(k.stats.ternary_resolvents == 0 && k.stats.ternary_binaries == 0);
}

// A shared literal in OPPOSITE polarity ⇒ the resolvent is tautological ⇒
// rejected. This is also the shape that must never reach the proof: a
// tautological LRAT line is uncheckable (Proof::tautological asserts on it).
static void test_tautological_rejected() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add3(k, 1, 2, 3);
  add3(k, -1, -2, 3);
  assert(ternary_apply(k) == ApplyResult::NoOp);
  assert(k.stats.ternary_resolvents == 0 && k.stats.ternary_binaries == 0);
}

// The resolvent already exists ⇒ not added twice, and the second round is a
// NoOp (idempotence — the rule reaches a fixpoint).
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

// Instance surface (N1): pivots are offered as literal payloads, applying one
// does that pivot's resolutions, and an out-of-range payload is a total NoOp.
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

int main() {
  test_ternary_resolvent();
  test_binary_resolvent();
  test_quaternary_rejected();
  test_tautological_rejected();
  test_duplicate_rejected();
  test_idempotent();
  test_instances();
  std::printf("test_ternary: all passed\n");
  return 0;
}
