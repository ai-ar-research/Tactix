// S3 Rephase unit tests: type semantics, best/target machinery, the
// registry instance surface, and the policy trigger.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/default_policy.hpp"
#include "rules/registry.hpp"
#include "rules/rephase/rephase.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace tactix;

static void test_best_snapshot() {
  // best/target snapshots happen on backtrack while stable, past their
  // watermarks; best survives shallower episodes
  Kernel k;
  kernel_reserve_vars(k, 3);
  k.h->stable = true;
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(-2));
  kernel_backtrack(k, 0); // deepest so far: snapshot both
  assert(k.h->best[0] == 1 && k.h->best[1] == -1 && k.h->best[2] == 0);
  assert(k.h->best_assigned == 2);
  assert(k.stats.best_saved == 1);
  kernel_assign_decision(k, internalize(2));
  kernel_backtrack(k, 0); // shallower: best untouched
  assert(k.h->best[1] == -1);
  assert(k.stats.best_saved == 1);
}

static void test_types() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  k.h->stable = true;
  kernel_assign_decision(k, internalize(-1));
  kernel_assign_decision(k, internalize(3));
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

  // unknown payload: no-op, never unsound
  assert(rephase_apply_type(k, 99) == ApplyResult::NoOp);
  assert(k.stats.rephased == 3);
}

static void test_registry_surface() {
  Kernel k;
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
  // canonical sentinel routes to Best
  assert(r.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::Ok);
  assert(k.stats.rephased == 2);
}

static void test_policy_trigger() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  DefaultPolicy p;
  k.stats.conflicts = p.rephase.next_conflicts + 1;
  assert(!p.want_rephase(k)); // focused: never
  k.h->stable = true;
  assert(p.want_rephase(k));
  k.stats.rephased = 1;
  p.update_rephase_limit(k);
  assert(!p.want_rephase(k)); // limit pushed out (n·log³n growth)
  assert(p.rephase.next_conflicts > k.stats.conflicts + 900);
}

int main() {
  test_best_snapshot();
  test_types();
  test_registry_surface();
  test_policy_trigger();
  puts("test_rephase OK");
  return 0;
}
