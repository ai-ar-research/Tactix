// S4 SwitchMode / stable-mode machinery unit tests: score heap, EVSIDS
// bump/decay/rescale, mode flip, stable decisions + target phases,
// stable trail reuse, policy-side schedule (reluctant, mode limits).
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/restart/restart.hpp"
#include "rules/switchmode/switchmode.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

static void test_heap_basic() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_update_scores(k); // push all five
  for (Var v = 0; v < 5; v++)
    assert(kernel_heap_contains(k, v));
  // bump 3 above everything, then 1 above 3
  kernel_score_bump(k, 3);
  assert(kernel_heap_next_unassigned(k) == 3);
  kernel_scores_decay(k); // scinc grows
  kernel_score_bump(k, 1); // later bump wins (EVSIDS recency)
  assert(kernel_heap_next_unassigned(k) == 1);
  // assigning the max forces a lazy pop to the runner-up
  k.stable = true;
  kernel_assign_decision(k, make_lit(1, false));
  assert(kernel_heap_next_unassigned(k) == 3);
  // unassigning re-inserts (backtrack path)
  kernel_backtrack(k, 0);
  assert(kernel_heap_contains(k, 1));
  assert(kernel_heap_next_unassigned(k) == 1);
}

static void test_rescale() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_update_scores(k);
  // force scinc past the rescale threshold: relative order must survive
  kernel_score_bump(k, 2);
  for (int i = 0; i < 24000; i++) // 1.0526^24000 >> 1e150
    kernel_scores_decay(k);
  kernel_score_bump(k, 1); // huge increment, but rescale keeps it bounded
  assert(k.scores.score[1] <= 1e150); // MAX_SCORE cap survives
  assert(k.scinc <= 1e150);
  assert(k.scores.score[1] > k.scores.score[2]);
  assert(kernel_heap_next_unassigned(k) == 1);
}

static void test_switchmode_rule() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  assert(switchmode_guard(k));
  assert(!k.stable);
  assert(switchmode_apply(k) == ApplyResult::Ok);
  assert(k.stable);
  assert(k.stats.switched == 1);
  for (Var v = 0; v < 4; v++)
    assert(kernel_heap_contains(k, v)); // update_scores ran
  assert(switchmode_apply(k) == ApplyResult::Ok);
  assert(!k.stable);
  assert(k.search_idx == k.queue_last); // search cache reset
}

static void test_stable_decide_and_target_phases() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  switchmode_apply(k); // enter stable
  kernel_score_bump(k, 0); // var 1 (external) ranks first
  const Var v = kernel_next_decision_var(k);
  assert(v == 0);
  // no target saved yet -> saved phase (positive default)
  assert(kernel_decide_lit(k, v) == internalize(1));
  decide_apply(k, internalize(1), false);
  assert(propagate_apply(k) == ApplyResult::Ok); // forces 2
  // consistent backtrack snapshots target phases (deepest trail = 2)
  kernel_backtrack(k, 0);
  assert(k.target_assigned == 2);
  assert(k.target[0] == 1 && k.target[1] == 1);
  assert(k.stats.target_saved == 1);
  // flip saved phase; target must still win while stable
  k.saved[0] = -1;
  assert(kernel_decide_lit(k, 0) == internalize(1));
  switchmode_apply(k); // focused: saved wins again
  assert(kernel_decide_lit(k, 0) == internalize(-1));
}

static void test_stable_trail_reuse() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  switchmode_apply(k);
  // scores: v0 > v1 > v2 > v3 (bump under growing scinc, reverse order)
  kernel_score_bump(k, 3);
  kernel_scores_decay(k);
  kernel_score_bump(k, 2);
  kernel_scores_decay(k);
  kernel_score_bump(k, 1);
  kernel_scores_decay(k);
  kernel_score_bump(k, 0);
  // decide v0 then v2; next candidate is v1, whose score beats v2's:
  // only the v0 level survives the restart
  decide_apply(k, make_lit(0, false), false);
  decide_apply(k, make_lit(2, false), false);
  assert(restart_apply(k) == ApplyResult::Ok);
  assert(k.level == 1);
  assert(k.values[make_lit(0, false)] > 0);
  assert(k.values[make_lit(2, false)] == 0);
  assert(k.stats.restarts_reused == 1);
}

static void test_policy_schedule() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  DefaultPolicy p;
  // focused until modeinit conflicts
  assert(!p.want_switch_mode(k));
  k.stats.conflicts = 1000;
  assert(p.want_switch_mode(k));
  // simulate the switch the way solve() does
  k.stats.ticks = 500; // focused phase consumed 500 ticks
  switchmode_apply(k);
  p.mode.count++;
  p.reluctant = {};
  p.update_mode_limits(k);
  assert(k.stable);
  assert(p.mode.ticks_limit == 1000); // 500 + (500 - 0)
  assert(!p.want_switch_mode(k));
  k.stats.ticks = 1000;
  assert(p.want_switch_mode(k));
  // stable restarts come from reluctant doubling, not glue EMAs
  assert(!p.want_restart(k));
  for (int i = 0; i < 1024; i++)
    p.tick_reluctant();
  assert(p.reluctant.trigger);
  assert(p.want_restart(k));
}

static void test_default_policy_solves_across_modes() {
  // an unsatisfiable chain small enough to be instant but forced through
  // both modes: run with modeinit-sized conflict budget via direct solve
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_add_binary_clause(k, internalize(-1), internalize(-3));
  assert(default_solve(k) == 20);
}

int main() {
  test_heap_basic();
  test_rescale();
  test_switchmode_rule();
  test_stable_decide_and_target_phases();
  test_stable_trail_reuse();
  test_policy_schedule();
  test_default_policy_solves_across_modes();
  printf("test_switchmode: all passed\n");
  return 0;
}
