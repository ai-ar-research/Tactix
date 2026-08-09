// Lucky unit tests: constant probes, sweep probes, abandonment restoring the
// entry state with phases untouched, and the registry surface.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/default_policy.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

static void add3(Kernel &k, int a, int b, int c) {
  const Lit lits[3] = {internalize(a), internalize(b), internalize(c)};
  kernel_add_large_clause(k, lits, 3, false, 0);
}

static void test_all_true() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  add3(k, 1, 2, 3);
  assert(lucky_guard(k));
  assert(lucky_apply_probe(k, LUCKY_ALL_TRUE) == ApplyResult::Sat);
  assert(k.unassigned == 0);
  for (Var v = 0; v < 3; v++)
    assert(k.values[make_lit(v, false)] > 0);
  assert(k.stats.lucky_probes == 1);
}

static void test_all_false() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  add3(k, -1, -2, -3);
  assert(lucky_apply_probe(k, LUCKY_ALL_TRUE) == ApplyResult::Ok);
  assert(k.unassigned == 3); // a defeated probe leaves the state untouched
  assert(lucky_apply_probe(k, LUCKY_ALL_FALSE) == ApplyResult::Sat);
  for (Var v = 0; v < 3; v++)
    assert(k.values[make_lit(v, true)] > 0);
}

static void test_sweep() {
  // mixed polarity defeats both constants; the forward-false sweep succeeds
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));   // no all-F
  kernel_add_binary_clause(k, internalize(-1), internalize(-2)); // no all-T
  assert(lucky_apply_probe(k, LUCKY_ALL_TRUE) == ApplyResult::Ok);
  assert(lucky_apply_probe(k, LUCKY_ALL_FALSE) == ApplyResult::Ok);
  assert(lucky_apply_probe(k, LUCKY_FORWARD_FALSE) == ApplyResult::Sat);
  assert(k.values[internalize(-1)] > 0); // ¬1 assumed
  assert(k.values[internalize(2)] > 0);  // 2 propagated by (1 2)
}

static void test_unsat_probes_abandon() {
  // on UNSAT every probe abandons and restores the entry state exactly,
  // saved phases included
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 6);
  auto var = [](int p, int h) { return p * 2 + h + 1; };
  for (int p = 0; p < 3; p++)
    kernel_add_binary_clause(k, internalize(var(p, 0)),
                             internalize(var(p, 1)));
  for (int h = 0; h < 2; h++)
    for (int p1 = 0; p1 < 3; p1++)
      for (int p2 = p1 + 1; p2 < 3; p2++)
        kernel_add_binary_clause(k, internalize(-var(p1, h)),
                                 internalize(-var(p2, h)));
  assert(lucky_apply(k) == ApplyResult::Ok); // canonical cascade: no luck
  assert(k.level == 0);
  assert(k.unassigned == 6);
  assert(k.trail.empty());
  assert(k.propagate_head == 0);
  for (Var v = 0; v < 6; v++) {
    assert(k.h->saved[v] == 1);  // probe assignments never touch phases
    assert(k.h->target[v] == 0);
    assert(k.h->best[v] == 0);
  }
  assert(k.stats.lucky_probes == 6);
  assert(k.stats.conflicts == 0); // probe conflicts are not search ones
  assert(setenv("TACTIX_SLOT_DISABLE",
                "Congruence,Substitute,Backbone,Sweep,Factor", 1) == 0);
  assert(default_solve(k) == 20); // and the formula still solves correctly
  assert(unsetenv("TACTIX_SLOT_DISABLE") == 0);
  assert(k.stats.lucky_early_calls == 1);
  assert(k.stats.lucky_late_calls == 1);
  assert(k.stats.lucky_late_sat == 0);

  Kernel off;
  ScopedHeuristics _hg_off(off);
  kernel_reserve_vars(off, 6);
  for (int p = 0; p < 3; p++)
    kernel_add_binary_clause(off, internalize(var(p, 0)),
                             internalize(var(p, 1)));
  for (int h = 0; h < 2; h++)
    for (int p1 = 0; p1 < 3; p1++)
      for (int p2 = p1 + 1; p2 < 3; p2++)
        kernel_add_binary_clause(off, internalize(-var(p1, h)),
                                 internalize(-var(p2, h)));
  assert(setenv("TACTIX_SLOT_DISABLE",
                "Congruence,Substitute,Backbone,Sweep,Factor", 1) == 0);
  assert(setenv("TACTIX_LUCKY_LATE", "0", 1) == 0);
  assert(default_solve(off) == 20);
  assert(unsetenv("TACTIX_LUCKY_LATE") == 0);
  assert(unsetenv("TACTIX_SLOT_DISABLE") == 0);
  assert(off.stats.lucky_early_calls == 1);
  assert(off.stats.lucky_late_calls == 0 && off.stats.lucky_late_sat == 0);
}

static void test_registry_surface() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  const int id = tactix_rule_id("Lucky");
  assert(id >= 0);
  const RuleVTable &r = tactix_rules()[id];
  assert(r.guard(k));
  uint64_t insts[8];
  assert(r.instances(k, insts, 8) == 6);
  assert(insts[0] == LUCKY_ALL_TRUE);
  assert(r.apply_instance(k, 99) == ApplyResult::NoOp); // unknown payload
  assert(r.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::Sat);
  // the guard reads state shape only; SAT comes from the status predicate
  assert(!r.guard(k)); // no longer quiescent-with-work: all assigned
}

static void test_guard_needs_quiescent_root() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  assert(lucky_guard(k));
  kernel_assign_decision(k, internalize(1));
  assert(!lucky_guard(k)); // not at the root (and unpropagated)
}

int main() {
  test_all_true();
  test_all_false();
  test_sweep();
  test_unsat_probes_abandon();
  test_registry_surface();
  test_guard_needs_quiescent_root();
  puts("test_lucky OK");
  return 0;
}
