// SwitchMode / stable-mode unit tests: score heap, EVSIDS bump/decay/rescale,
// mode flip, target phases, trail reuse, and the policy-side schedule.
// ScopedHeuristics attaches k.h: the session, not the kernel, owns it.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/default_policy.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/restart/restart.hpp"
#include "policies/ops/switchmode/switchmode.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

static void test_heap_basic() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 5);
  assert(k.h->chb_scores.pos.empty() && k.h->chb_scores.score.empty());
  assert(k.h->chb_conflicted.empty());
  for (Var v = 0; v < k.nvars; ++v)
    heuristics_score_set(k, v, 0.0);
  heuristics_update_scores(k); // push all five
  for (Var v = 0; v < 5; v++)
    assert(heuristics_heap_contains(k, v));
  heuristics_score_bump(k, 3); // bump 3 above everything, then 1 above 3
  assert(heuristics_heap_next_unassigned(k) == 3);
  heuristics_scores_decay(k); // scinc grows
  heuristics_score_bump(k, 1); // later bump wins (EVSIDS recency)
  assert(heuristics_heap_next_unassigned(k) == 1);
  // assigning the max forces a lazy pop to the runner-up
  k.h->stable = true;
  kernel_assign_decision(k, make_lit(1, false));
  assert(heuristics_heap_next_unassigned(k) == 3);
  kernel_backtrack(k, 0); // unassigning re-inserts
  assert(heuristics_heap_contains(k, 1));
  assert(heuristics_heap_next_unassigned(k) == 1);
}

static void test_heap_uses_import_not_activation_order_for_ties() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 5);
  // Import happens as each literal reaches kissat_add, even if the containing
  // clause later proves tautological and never activates that variable.  Thus
  // the stable heap's all_variables(idx) insertion order is independent of
  // the activation population used by random decisions and initial scores.
  for (const Var v : std::vector<Var>{2, 4, 1, 3})
    kernel_record_import(k, v);
  k.compact_import_order = {1, 3, 2, 4};
  for (Var v = 0; v < k.nvars; ++v)
    heuristics_score_set(k, v, 0.0);
  heuristics_update_scores(k);

  // With equal scores, push/bubble is stable.  This is the exact heap pose
  // Kissat gets from its tumbled import indices.  The never-imported direct-API
  // variable is the policy-neutral total-model backstop and follows them.
  assert(k.h->scores.stack == std::vector<Var>({2, 4, 1, 3, 0}));
  kernel_assign_unit(k, make_lit(2, false));
  assert(heuristics_heap_next_unassigned(k) == 0);
}

static void test_rescale() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3);
  heuristics_update_scores(k);
  // force scinc past the rescale threshold: relative order must survive
  heuristics_score_bump(k, 2);
  for (int i = 0; i < 24000; i++) // 1.0526^24000 >> 1e150
    heuristics_scores_decay(k);
  heuristics_score_bump(k, 1); // huge increment, but rescale keeps it bounded
  assert(k.h->scores.score[1] <= 1e150); // MAX_SCORE cap survives
  assert(k.h->scinc <= 1e150);
  assert(k.h->scores.score[1] > k.h->scores.score[2]);
  assert(heuristics_heap_next_unassigned(k) == 1);
}

static void test_stable_mab_chb_arm() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3);
  k.h->stable = true;
  heuristics_update_scores(k);
  heuristics_configure_mab(k, true, 4.0, 0.4, 0.06, 0.000001);
  for (Var v = 0; v < 3; ++v)
    assert(heuristics_chb_heap_contains(k, v));

  heuristics_mab_note_decision(k);
  heuristics_mab_note_decision(k);
  heuristics_mab_note_conflict(k);
  heuristics_mab_note_conflict(k);
  assert(heuristics_mab_restart(k)); // owner's first exploration selects CHB
  assert(k.h->stable_heuristic == 1);
  assert(k.stats.mab_restarts == 1 && k.stats.mab_switches == 1);

  kernel_assign_decision(k, make_lit(0, false));
  const double before = k.h->chb_scores.score[0];
  heuristics_chb_update_after_propagation(k, false);
  assert(k.h->chb_scores.score[0] > before);
  k.stats.conflicts = 7;
  k.analyzed.push_back(0);
  heuristics_chb_mark_analyzed(k);
  assert(k.h->chb_conflicted[0] == 7);
  k.analyzed.clear();
  kernel_backtrack(k, 0);
  assert(heuristics_chb_heap_contains(k, 0));
  assert(heuristics_next_decision_var(k) == 0);

  for (int i = 0; i < 4; ++i)
    heuristics_mab_note_decision(k);
  heuristics_mab_note_conflict(k);
  heuristics_mab_note_conflict(k);
  assert(!heuristics_mab_restart(k));
  assert(k.h->stable_heuristic == 1);
  assert(k.h->mab_reward[1] == 2.0 && k.h->mab_select[1] == 2);
  assert(k.stats.mab_restarts == 2 && k.stats.mab_chb_selections == 2);
}

static void test_stable_mab_chb_scores_level_boundary() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 4);
  k.h->stable = true;
  heuristics_update_scores(k);
  heuristics_configure_mab(k, true, 4.0, 0.4, 0.06, 0.000001);
  k.h->stable_heuristic = 1;

  // Root v0, level-one v1/v2, level-two v3.  The owner's reverse scan scores
  // the current-level suffix and exactly one literal across the boundary.
  kernel_assign_unit(k, make_lit(0, false));
  kernel_assign_decision(k, make_lit(1, false));
  kernel_assign(k, make_lit(2, false), 1, INVALID_REF, false);
  kernel_assign_decision(k, make_lit(3, false));
  heuristics_chb_update_after_propagation(k, false);
  assert(k.h->chb_scores.score[3] > 0.0);
  assert(k.h->chb_scores.score[2] > 0.0); // first lower-level boundary
  assert(k.h->chb_scores.score[1] == 0.0);
  assert(k.h->chb_scores.score[0] == 0.0);
  assert(k.stats.chb_updates == 2);
}

static void test_switchmode_rule() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 4);
  assert(switchmode_guard(k));
  assert(!k.h->stable);
  assert(switchmode_apply(k) == ApplyResult::Ok);
  assert(k.h->stable);
  assert(k.stats.switched == 1);
  for (Var v = 0; v < 4; v++)
    assert(heuristics_heap_contains(k, v)); // update_scores ran
  assert(switchmode_apply(k) == ApplyResult::Ok);
  assert(!k.h->stable);
  assert(k.h->search_idx == k.h->queue_last); // search cache reset
}

static void test_stable_decide_and_target_phases() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  switchmode_apply(k); // enter stable
  heuristics_score_bump(k, 0); // var 1 (external) ranks first
  const Var v = heuristics_next_decision_var(k);
  assert(v == 0);
  // no target saved yet -> saved phase (positive default)
  assert(heuristics_decide_lit(k, v) == internalize(1));
  decide_apply(k, internalize(1), false);
  assert(propagate_apply(k) == ApplyResult::Ok); // forces 2
  // Target phases are snapshotted at the call sites, never inside
  // kernel_backtrack: the kernel primitive performs no `H`-policy action.
  heuristics_update_target_phases(k);
  kernel_backtrack(k, 0);
  assert(k.h->target_assigned == 2);
  assert(k.h->target[0] == 1 && k.h->target[1] == 1);
  assert(k.stats.target_saved == 1);
  // flip saved phase; target must still win while stable
  k.h->saved[0] = -1;
  assert(heuristics_decide_lit(k, 0) == internalize(1));
  switchmode_apply(k); // focused: saved wins again
  assert(heuristics_decide_lit(k, 0) == internalize(-1));
}

static void test_target_high_water_uses_compact_assigned_count() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 5);
  k.h->stable = true;
  kernel_assign_unit(k, internalize(1));
  kernel_assign_unit(k, internalize(2));
  assert(k.trail.size() == 2 && k.unassigned == 3);

  // Kissat compacts all but one root-fixed identity: VARS becomes four while
  // one variable is assigned in that compact representation.  Tactix keeps
  // both kernel identities and both trail entries, so trail.size() would
  // incorrectly pose a high-water mark of two.
  heuristics_compact_random_order_fixed(k);
  assert(k.compact_import_order.size() == 4);
  heuristics_update_target_phases(k);
  assert(k.h->target_assigned == 1 && k.h->best_assigned == 1);
  assert(k.stats.target_saved == 1 && k.stats.best_saved == 1);

  // Kissat decrements `unassigned` as soon as an unassigned variable is
  // eliminated, while VARS retains that identity until the next compaction.
  // Tactix's global counter intentionally retains reconstructible retired
  // identities; `unassigned_active` is the matching compact counter.
  k.var_status.assign(k.nvars, VarStatus::Active);
  k.var_status[2] = VarStatus::Substituted;
  assert(k.values[make_lit(2, false)] == 0 && k.unassigned_active == 3);
  k.unassigned_active--;
  heuristics_deactivate_var(k, 2);
  heuristics_update_target_phases(k);
  assert(k.h->target_assigned == 2 && k.h->best_assigned == 2);
  assert(k.stats.target_saved == 2 && k.stats.best_saved == 2);
}

static void test_root_collapse_recomputes_phase_high_water() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 5);
  kernel_assign_unit(k, internalize(1));
  kernel_assign_unit(k, internalize(2));

  // Model the intermediate state after identity-preserving physical
  // compaction: all five immutable phase entries survive, but Kissat's compact
  // VARS image retains only one of the two root-fixed identities.
  std::fill(k.h->target.begin(), k.h->target.end(), int8_t{1});
  std::fill(k.h->best.begin(), k.h->best.end(), int8_t{-1});
  k.h->target_assigned = k.h->best_assigned = 5;
  heuristics_compact_random_order_fixed(k);

  assert(k.compact_import_order.size() == 4);
  // Kissat retains one fixed representative in VARS but its active flag is
  // false, so compact_best_and_target_values counts only the three live vars.
  assert(k.h->target_assigned == 3);
  assert(k.h->best_assigned == 3);
}

static void test_stable_trail_reuse() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 4);
  for (Var v = 0; v < k.nvars; ++v)
    heuristics_score_set(k, v, 0.0);
  switchmode_apply(k);
  // scores: v0 > v1 > v2 > v3 (bump under growing scinc, reverse order)
  heuristics_score_bump(k, 3);
  heuristics_scores_decay(k);
  heuristics_score_bump(k, 2);
  heuristics_scores_decay(k);
  heuristics_score_bump(k, 1);
  heuristics_scores_decay(k);
  heuristics_score_bump(k, 0);
  // v1 outranks v2, so only the v0 level survives the restart
  decide_apply(k, make_lit(0, false), false);
  decide_apply(k, make_lit(2, false), false);
  const uint32_t keep = heuristics_restart_reuse_level(k);
  heuristics_update_target_phases(k);
  assert(restart_apply_to(k, keep) == ApplyResult::Ok);
  assert(k.level == 1);
  assert(k.values[make_lit(0, false)] > 0);
  assert(k.values[make_lit(2, false)] == 0);
  assert(k.stats.restarts_reused == 1);
}

static void test_policy_schedule() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2);
  DefaultPolicy p;
  assert(!p.want_switch_mode(k)); // focused until modeinit conflicts
  k.stats.conflicts = 1000;
  assert(p.want_switch_mode(k));
  // simulate the switch the way solve() does
  // The focused phase consumed 500 ticks, all of them search: the kernel
  // charges `search_ticks` whenever `inprocessing` is false, which is what a
  // real focused phase does.
  k.stats.ticks = k.stats.search_ticks = 500;
  switchmode_apply(k);
  p.mode.count++;
  p.reluctant = {};
  p.update_mode_limits(k);
  assert(k.h->stable);
  assert(p.mode.ticks_limit == 1000); // 500 + (500 - 0)
  assert(!p.want_switch_mode(k));
  k.stats.ticks = k.stats.search_ticks = 1000;
  assert(p.want_switch_mode(k));
  // stable restarts come from reluctant doubling, not glue EMAs
  assert(!p.want_restart(k));
  for (int i = 0; i < 1024; i++)
    p.tick_reluctant();
  assert(p.reluctant.trigger);
  assert(p.want_restart(k));
}

static void test_default_policy_solves_across_modes() {
  Kernel k; // a tiny UNSAT chain, but forced through both modes
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_add_binary_clause(k, internalize(-1), internalize(-3));
  assert(default_solve(k) == 20);
}

static void test_default_policy_with_mab_enabled() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_add_binary_clause(k, internalize(-1), internalize(-3));
  DefaultStrategyConfig config;
  config.mode.force = 2;
  DefaultPolicy policy(config);
  assert(policy.solve(k) == 20);
}

int main() {
  test_heap_basic();
  test_heap_uses_import_not_activation_order_for_ties();
  test_rescale();
  test_stable_mab_chb_arm();
  test_stable_mab_chb_scores_level_boundary();
  test_switchmode_rule();
  test_stable_decide_and_target_phases();
  test_target_high_water_uses_compact_assigned_count();
  test_root_collapse_recomputes_phase_high_water();
  test_stable_trail_reuse();
  test_policy_schedule();
  test_default_policy_solves_across_modes();
  test_default_policy_with_mab_enabled();
  printf("test_switchmode: all passed\n");
  return 0;
}
