// Rephase unit tests: type semantics, best/target machinery, the registry
// instance surface, and the policy trigger.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/default_policy.hpp"
#include "rules/registry.hpp"
#include "policies/ops/rephase/rephase.hpp"
#include "rules/restart/restart.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

static void test_best_snapshot() {
  // best/target snapshot on a stable backtrack past their watermarks, so
  // best survives shallower episodes
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3);
  k.h->stable = true;
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(-2));
  // the snapshot lives at the call sites, not inside kernel_backtrack
  heuristics_update_target_phases(k);
  kernel_backtrack(k, 0); // deepest so far: snapshot both
  assert(k.h->best[0] == 1 && k.h->best[1] == -1 && k.h->best[2] == 0);
  assert(k.h->best_assigned == 2);
  assert(k.stats.best_saved == 1);
  kernel_assign_decision(k, internalize(2));
  heuristics_update_target_phases(k);
  kernel_backtrack(k, 0); // shallower: best untouched
  assert(k.h->best[1] == -1);
  assert(k.stats.best_saved == 1);
}

static void test_types() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 4);
  k.h->stable = true;
  kernel_assign_decision(k, internalize(-1));
  kernel_assign_decision(k, internalize(3));
  heuristics_update_target_phases(k); // the snapshot is the caller's job
  kernel_backtrack(k, 0); // best = {-1, 0, +3, 0}, saved updated too

  // Inverted: every saved phase forced negative; target restarts
  assert(rephase_apply_type(k, REPHASE_INVERTED) == ApplyResult::Ok);
  for (Var v = 0; v < 4; v++) {
    assert(k.h->saved[v] == -1);
    assert(k.h->target[v] == -1); // target = saved
  }
  assert(k.h->target_assigned == 0);
  assert(k.h->best_assigned == 2); // Inverted does not consume best

  // Original: every saved phase back to the initial positive
  assert(rephase_apply_type(k, REPHASE_ORIGINAL) == ApplyResult::Ok);
  for (Var v = 0; v < 4; v++)
    assert(k.h->saved[v] == 1 && k.h->target[v] == 1);

  // Best: overwrite only where a best phase exists, consume the best
  assert(rephase_apply_type(k, REPHASE_BEST) == ApplyResult::Ok);
  assert(k.h->saved[0] == -1); // from best
  assert(k.h->saved[1] == 1);  // no best entry: untouched
  assert(k.h->saved[2] == 1);  // from best (var 2 was decided +3)
  assert(k.h->saved[3] == 1);
  assert(k.h->best_assigned == 0); // consumed
  assert(k.stats.rephased == 3);

  assert(rephase_apply_type(k, 99) == ApplyResult::NoOp); // unknown payload
  assert(k.stats.rephased == 3);
}

static void test_registry_surface() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  const int id = tactix_rule_id("Rephase");
  assert(id >= 0);
  const RuleVTable &r = tactix_rules()[id];
  assert(r.guard(k));
  uint64_t insts[8];
  assert(r.instances(k, insts, 8) == 3);
  assert(insts[0] == REPHASE_BEST); // canonical first
  assert(insts[1] == REPHASE_INVERTED && insts[2] == REPHASE_ORIGINAL);
  assert(r.apply_instance(k, REPHASE_INVERTED) == ApplyResult::Ok);
  assert(k.h->saved[0] == -1 && k.h->saved[1] == -1);
  // the canonical sentinel routes to Best
  assert(r.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::Ok);
  assert(k.stats.rephased == 2);
}

static void test_policy_trigger() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  DefaultPolicy p;
  k.stats.conflicts = p.rephase.next_conflicts + 1;
  assert(!p.want_rephase(k)); // focused: never
  k.h->stable = true;
  assert(p.want_rephase(k));
  // the interval grows with rephase.count, not with the op count
  p.rephase.count = 1;
  k.stats.rephased = 2; // deliberately different: must not affect the limit
  p.update_rephase_limit(k);
  assert(!p.want_rephase(k)); // limit pushed out (n·log³n growth)
  assert(p.rephase.next_conflicts > k.stats.conflicts + 900);
  const uint64_t one_event = p.rephase.next_conflicts;
  k.stats.rephased = 7;
  p.update_rephase_limit(k);
  assert(p.rephase.next_conflicts == one_event);
}

static void test_rephase_backtrack_preserves_restart_cadence() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  kernel_assign_decision(k, internalize(1));

  // Kissat's logarithmic focused-restart interval first grows from one to two
  // at the 91st canonical restart.  A Rephase trail clear must not cross that
  // source-defined boundary.
  k.stats.conflicts = 2000;
  k.stats.restarts = 90;
  assert(restart_backtrack_to(k, 0) == ApplyResult::Ok);
  assert(k.level == 0 && k.stats.restarts == 90);
  DefaultPolicy p;
  p.update_restart_limit(k);
  assert(p.restart.next_conflicts == 2001);

  kernel_assign_decision(k, internalize(2));
  assert(restart_apply_to(k, 0) == ApplyResult::Ok);
  assert(k.level == 0 && k.stats.restarts == 91);
  p.update_restart_limit(k);
  assert(p.restart.next_conflicts == 2002);
}

static void test_walk_effort_uses_eliminate_anchor() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  DefaultPolicy p;

  // Walk is timed as a simplifier but does not set Kissat's semantic probing
  // flag. Its SET_EFFORT_LIMIT reference is therefore `last.ticks.eliminate`;
  // neither the last Walk nor the Probe clock owns this reference point.
  k.stats.search_ticks = 50'000'000;
  p.inprocess().probe_clock.anchor = 49'000'000;
  p.inprocess().elim_clock.anchor = 20'000'000;
  assert(p.walk_effort_reference(k) == 30'000'000);

  // The shipped mineffort floor still applies to a recent Eliminate anchor.
  p.inprocess().elim_clock.anchor = 45'000'000;
  assert(p.walk_effort_reference(k) ==
         DefaultStrategyConfig::INPROCESS_MINIMUM_EFFORT_DEFAULT);
}

static std::vector<int> warmup_after_retained_root(bool prepare) {
  Kernel k;
  kernel_reserve_vars(k, 5);
  ScopedHeuristics h(k);
  k.h->stable = true;
  kernel_add_binary_clause(k, internalize(-4), internalize(-1));
  kernel_add_binary_clause(k, internalize(-5), internalize(-1));
  const Lit clause[] = {internalize(-2), internalize(3), internalize(-1)};
  const Ref ref = kernel_add_large_clause(k, clause, 3, false, 0);
  // A legal cached blocker need not be the other current watched literal.
  // The root scan is initially satisfied by a transient decision, leaving
  // this watch on +3 until backtracking removes that blocker.
  kernel_unwatch_large(k, internalize(3), ref);
  k.watches.push_large(internalize(3), internalize(-1), ref);
  kernel_assign_decision(k, internalize(-1));
  kernel_assign_unit(k, internalize(-3));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(kernel_propagation_fixpoint(k));
  k.stats.restarts = 90;
  DefaultPolicy p;
  if (prepare) {
    assert(p.prepare_rephase_root(k) == ApplyResult::Ok);
    assert(kernel_propagation_fixpoint(k) && k.unflushed_root == 0);
  } else {
    // Executable old-negative witness, not a failing assertion or crash.
    assert(restart_backtrack_to(k, 0) == ApplyResult::Ok);
    assert(!kernel_propagation_fixpoint(k));
  }
  assert(k.stats.restarts == 90);
  k.warming = true;
  kernel_assign_decision(k, internalize(1));
  const size_t begin = k.trail.size();
  kernel_propagate_beyond_conflicts(k);
  std::vector<int> result;
  for (size_t i = begin; i < k.trail.size(); ++i)
    result.push_back(kernel_externalize(k, k.trail[i]));
  k.warming = false;
  return result;
}

static void test_rephase_root_closure_precedes_warmup() {
  assert(warmup_after_retained_root(false) == std::vector<int>({-2, -4, -5}));
  assert(warmup_after_retained_root(true) == std::vector<int>({-4, -5, -2}));
  Kernel k;
  kernel_reserve_vars(k, 2);
  ScopedHeuristics h(k);
  kernel_assign_decision(k, internalize(1));
  // Public Rephase remains H-only and universally usable mid-trail.
  assert(rephase_apply_type(k, REPHASE_ORIGINAL) == ApplyResult::Ok);
  assert(k.level == 1 && k.value(internalize(1)) > 0);
}

int main() {
  test_best_snapshot();
  test_types();
  test_registry_surface();
  test_policy_trigger();
  test_rephase_backtrack_preserves_restart_cadence();
  test_walk_effort_uses_eliminate_anchor();
  test_rephase_root_closure_precedes_warmup();
  puts("test_rephase OK");
  return 0;
}
