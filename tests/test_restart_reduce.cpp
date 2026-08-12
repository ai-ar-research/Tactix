// Restart / Reduce / arena-collection unit tests. Each test attaches a
// ScopedHeuristics guard: the session, not the kernel, owns k.h.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include "rules/restart/restart.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>

using namespace tactix;

struct ReverseArenaOrder final : ArenaOrdering {
  void order(const Kernel &, std::vector<Ref> &refs) override {
    std::reverse(refs.begin(), refs.end());
  }
};

static void test_restart() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  // the guard holds at any level > 0; scheduling is policy, not rule
  assert(restart_guard(k));
  DefaultPolicy p;
  assert(!p.want_restart(k)); // conflicts 0 < the initial limit
  p.averages[0].initialized = true; // pose degrading focused search:
  p.averages[0].fast_glue.value = 10.0; // fast >> margin * slow
  p.averages[0].slow_glue.value = 4.0;
  k.stats.conflicts = p.restart.next_conflicts;
  assert(p.want_restart(k));
  assert(restart_apply(k) == ApplyResult::Ok);
  assert(k.level == 0);
  assert(k.values[internalize(1)] == 0); // decision undone
  assert(k.stats.restarts == 1);
  assert(!restart_guard(k)); // level 0 now
}

static void test_collect_preserves_propagation() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 4);
  const Lit c1[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit c2[3] = {internalize(1), internalize(2), internalize(4)};
  const Ref r1 = kernel_add_large_clause(k, c1, 3, true, 5);
  kernel_add_large_clause(k, c2, 3, true, 5);
  clause_at(k.arena, r1)->flags |= Clause::GARBAGE;
  kernel_collect_garbage(k);
  kernel_assign_unit(k, internalize(-1)); // c2 must still force 4, and only 4
  kernel_assign_unit(k, internalize(-2));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.values[internalize(4)] > 0);
  assert(k.values[internalize(3)] == 0); // c1 is gone
}

static void test_collect_accepts_policy_order() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 6);
  const Lit first[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit dead[3] = {internalize(1), internalize(4), internalize(5)};
  const Lit last[3] = {internalize(-1), internalize(-2), internalize(6)};
  kernel_add_large_clause(k, first, 3, false, 0);
  const Ref garbage = kernel_add_large_clause(k, dead, 3, false, 0);
  kernel_add_large_clause(k, last, 3, false, 0);
  clause_at(k.arena, garbage)->flags |= Clause::GARBAGE;
  ReverseArenaOrder reverse;
  k.arena_ordering = &reverse;
  kernel_collect_garbage(k);
  const Clause *front = clause_at(k.arena, 0);
  assert(front->lits[0] == last[0]);
  const Ref second = clause_words(front->size);
  assert(clause_at(k.arena, second)->lits[0] == first[0]);
}

static void test_sparse_collect_preserves_prefix_watch_order() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 8);
  const Lit p1[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit p2[3] = {internalize(1), internalize(4), internalize(5)};
  const Lit dead[3] = {internalize(1), internalize(6), internalize(7)};
  const Lit tail[3] = {internalize(1), internalize(7), internalize(8)};
  const Ref r1 = kernel_add_large_clause(k, p1, 3, false, 0);
  const Ref r2 = kernel_add_large_clause(k, p2, 3, false, 0);
  const Ref first_reducible =
      kernel_add_large_clause(k, dead, 3, true, 9);
  const Ref tail_ref = kernel_add_large_clause(k, tail, 3, true, 9);

  // Stand in for search-time watch relocation: the prefix no longer follows
  // arena order. Kissat's sparse collector retains this order and appends the
  // rebuilt reducible tail.
  kernel_swap_large_watch_refs(k, internalize(1), r1, r2, p1[1], p2[1]);
  clause_at(k.arena, r1)->searched = 3;
  clause_at(k.arena, r2)->searched = 7;
  clause_at(k.arena, tail_ref)->searched = 6;
  clause_at(k.arena, first_reducible)->flags |= Clause::GARBAGE;
  ReverseArenaOrder reverse;
  k.arena_ordering = &reverse; // posed for inprocessing, ignored by Reduce
  kernel_collect_garbage(k, first_reducible);

  std::vector<Ref> refs;
  for (const WatchEntry w : k.watches.entries(internalize(1)))
    if (!w.binary())
      refs.push_back(w.ref());
  assert(refs.size() == 3);
  assert(refs[0] == r2 && refs[1] == r1 &&
         "sparse collection must retain prefix watch order");
  assert(refs[2] == first_reducible &&
         "the relocated reducible tail must be appended in arena order");
  assert(clause_at(k.arena, r1)->searched == 3 &&
         clause_at(k.arena, r2)->searched == 7 &&
         "sparse collection must retain prefix replacement cursors");
  assert(clause_at(k.arena, first_reducible)->searched == 2 &&
         "rewatching the relocated tail must reset its replacement cursor");
}

static void test_sparse_collect_forwards_garbage_prefix_watches() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 10);
  const Lit p1[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit prefix_dead[3] = {internalize(1), internalize(4),
                              internalize(5)};
  const Lit p2[3] = {internalize(1), internalize(6), internalize(7)};
  const Lit reducible_dead[3] = {internalize(1), internalize(8),
                                 internalize(9)};
  const Lit tail[3] = {internalize(1), internalize(9), internalize(10)};
  const Ref r1 = kernel_add_large_clause(k, p1, 3, false, 0);
  const Ref dead_prefix_ref =
      kernel_add_large_clause(k, prefix_dead, 3, false, 0);
  const Ref r2 = kernel_add_large_clause(k, p2, 3, false, 0);
  const Ref first_reducible =
      kernel_add_large_clause(k, reducible_dead, 3, true, 9);
  const Ref tail_ref = kernel_add_large_clause(k, tail, 3, true, 9);

  kernel_swap_large_watch_refs(k, internalize(1), r1, r2, p1[1], p2[1]);
  clause_at(k.arena, r1)->searched = 3;
  clause_at(k.arena, r2)->searched = 7;
  clause_at(k.arena, tail_ref)->searched = 6;
  clause_at(k.arena, dead_prefix_ref)->flags |= Clause::GARBAGE;
  clause_at(k.arena, first_reducible)->flags |= Clause::GARBAGE;

  kernel_collect_garbage(k, first_reducible);

  const Ref moved_r2 = r2 - clause_words(3);
  const Ref moved_tail = 2 * clause_words(3);
  std::vector<Ref> refs;
  for (const WatchEntry w : k.watches.entries(internalize(1)))
    if (!w.binary())
      refs.push_back(w.ref());
  assert(refs == std::vector<Ref>({moved_r2, r1, moved_tail}) &&
         "sparse collection must forward live prefix watches in place");
  assert(clause_at(k.arena, r1)->searched == 3);
  assert(clause_at(k.arena, moved_r2)->searched == 7);
  assert(clause_at(k.arena, moved_tail)->searched == 2 &&
         "the relocated tail must still be rewatched from cursor two");
  kernel_check_invariants(k);
}

static void test_collect_reselects_watches_by_assignment(void) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3);
  const Lit lits[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, lits, 3, true, 9);
  kernel_assign_decision(k, internalize(-1)); // false at level 1
  kernel_assign_decision(k, internalize(-2)); // false at level 2

  kernel_collect_garbage(k);
  const Clause *c = clause_at(k.arena, 0);
  assert(c->lits[0] == internalize(3) &&
         "Kissat rewatching selects an unassigned first watch");
  assert(c->lits[1] == internalize(2) &&
         "among false literals Kissat selects the higher level");
}

static void test_default_arena_order_tracks_search() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 6);
  heuristics_queue_bump(k, 0);
  const Lit older[3] = {internalize(-2), internalize(4), internalize(5)};
  const Lit fresher[3] = {internalize(-1), internalize(3), internalize(6)};
  const Ref ro = kernel_add_large_clause(k, older, 3, false, 0);
  const Ref rf = kernel_add_large_clause(k, fresher, 3, false, 0);
  std::vector<Ref> refs = {ro, rf};
  DefaultArenaOrder order;
  order.order(k, refs);
  assert(refs.size() == 2 && refs[0] == rf && refs[1] == ro);
}

static void test_reduce_deletes_and_logs() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  Proof proof;
  char path[] = "/tmp/tactix_reduce_test.drat";
  assert(proof.open(path, ProofFormat::DratAscii));
  k.proof = &proof;
  kernel_reserve_vars(k, 12);
  for (int i = 0; i < 4; i++) { // four high-glue candidates, plus one keeper
    const Lit c[3] = {internalize(3 * i + 1), internalize(3 * i + 2),
                      internalize(3 * i + 3)};
    kernel_add_large_clause(k, c, 3, true, 8 + i);
  }
  const Lit keeper[3] = {internalize(1), internalize(5), internalize(9)};
  const Ref kref = kernel_add_large_clause(k, keeper, 3, true, 1);
  (void)kref;
  DefaultPolicy rp;
  assert(rp.reduce.next_conflicts == DefaultPolicy::REDUCE_INTERVAL_INIT);
  k.stats.conflicts = rp.reduce.next_conflicts;
  assert(reduce_guard(k) && rp.want_reduce(k));
  assert(reduce_apply(k) == ApplyResult::Ok);
  assert(k.stats.deleted == 3); // 75% of the four candidates
  proof.flush();
  FILE *f = fopen(path, "r");
  char buf[256];
  int dels = 0;
  while (fgets(buf, sizeof buf, f))
    if (buf[0] == 'd')
      dels++;
  fclose(f);
  remove(path);
  assert(dels == 3); // every deletion hit the proof
  // the policy owns the budget now: advancing it stops the advice
  rp.reduce.next_conflicts = k.stats.conflicts +
                             DefaultPolicy::REDUCE_INTERVAL_ARITH +
                             DefaultPolicy::REDUCE_INTERVAL_STEP *
                                 k.stats.reductions;
  assert(!rp.want_reduce(k)); // budget pushed out (guard still holds)
}

static void test_reduce_flushes_new_root_units_before_selection() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 4);
  kernel_assign_unit(k, internalize(1));
  assert(k.unflushed_root == 1);
  kernel_assign_decision(k, internalize(2));
  kernel_assign_decision(k, internalize(3));
  const uint64_t restarts = k.stats.restarts;
  reduce_prepare(k);
  assert(k.level == 0);
  assert(k.values[internalize(1)] > 0);
  assert(k.values[internalize(2)] == 0);
  assert(k.values[internalize(3)] == 0);
  assert(k.unflushed_root == 0);
  assert(k.stats.restarts == restarts &&
         "Reduce's internal unwind is not a scheduled Restart");

  kernel_assign_decision(k, internalize(4));
  reduce_prepare(k); // no newly fixed root: preserve the search prefix
  assert(k.level == 1);
}

static void test_reduce_prepare_propagates_root_before_collection() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 3);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_assign_unit(k, internalize(1));
  kernel_assign_decision(k, internalize(3));
  reduce_prepare(k);
  assert(k.level == 0);
  assert(k.values[internalize(2)] > 0);
  assert(k.propagate_head == k.trail.size());
  assert(k.unflushed_root == 0);
  kernel_check_invariants(k);
}

static void test_reduce_prepare_reports_root_conflict() {
  Kernel k;
  ScopedHeuristics h(k);
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_assign_unit(k, internalize(1));
  kernel_assign_unit(k, internalize(2));
  reduce_prepare(k);
  assert(k.inconsistent);
  assert(k.stats.conflicts == 1);
}

static void test_root_propagation_consumes_unflushed_marker() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_assign_unit(k, internalize(1));
  assert(k.unflushed_root == 1);
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.unflushed_root == 0);
}

static void test_reduce_used_retention() {
  // kissat reduce.c recency: tier2 survives two reductions, aged per apply
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 6);
  const Lit c1[3] = {internalize(1), internalize(2), internalize(3)};
  const Ref tier2 = kernel_add_large_clause(k, c1, 3, true, 4);
  const Lit c2[3] = {internalize(4), internalize(5), internalize(6)};
  kernel_add_large_clause(k, c2, 3, true, 9); // high glue: never protected
  assert(clause_at(k.arena, tier2)->used() == Clause::MAX_USED);

  std::vector<Ref> cands;
  reduce_candidates(k, cands);
  assert(cands.size() == 1); // only the high-glue clause; tier2 protected
  // enumeration must be side-effect-free (N1 introspection surface)
  assert(clause_at(k.arena, tier2)->used() == Clause::MAX_USED);

  // two applications age used MAX -> MAX-2: tier2 protection lapses
  reduce_apply_refs(k, nullptr, 0);
  reduce_candidates(k, cands);
  assert(cands.size() == 1); // used == MAX-1: still protected
  reduce_apply_refs(k, nullptr, 0);
  reduce_candidates(k, cands);
  assert(cands.size() == 2); // protection lapsed; both are candidates
}

static void test_reduce_equal_rank_preserves_arena_order() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 6);
  const Lit first[3] = {internalize(1), internalize(2), internalize(3)};
  const Lit second[3] = {internalize(4), internalize(5), internalize(6)};
  const Ref first_ref = kernel_add_large_clause(k, first, 3, true, 9);
  const Ref second_ref = kernel_add_large_clause(k, second, 3, true, 9);
  std::vector<Ref> candidates;
  reduce_candidates(k, candidates);
  assert(candidates.size() == 2);
  assert(candidates[0] == first_ref && candidates[1] == second_ref &&
         "equal Kissat ranks must retain the arena scan order");
}

// The retention-band instrument tracks the live tier: moving `k.tier2` moves
// the dynamic count and leaves the static one alone.
static void test_tier2_band_tracks_the_live_tier() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 6);
  const Lit c1[3] = {internalize(1), internalize(2), internalize(3)};
  kernel_add_large_clause(k, c1, 3, true, 4); // inside the static 6
  const Lit c2[3] = {internalize(4), internalize(5), internalize(6)};
  kernel_add_large_clause(k, c2, 3, true, 9); // outside it

  k.tier2 = TIER2_STATIC;
  reduce_apply_refs(k, nullptr, 0);
  assert(k.stats.tier2_band_dynamic == 1); // glue 4 only
  assert(k.stats.tier2_band_static == 1);

  k.tier2 = 10; // the band a live glue distribution can open up to
  reduce_apply_refs(k, nullptr, 0);
  assert(k.stats.tier2_band_dynamic == 3); // +2: both clauses this round
  assert(k.stats.tier2_band_static == 2);  // +1: the static 6 still admits one
}

static void test_retier_uses_kissat_truncation() {
  Kernel k;
  uint64_t used[Stats::MAX_GLUE_USED + 1] = {};
  // total=11: Kissat's uint64 conversion makes the 90% threshold 9 (not 10).
  // The ninth observation is at glue 5 and the tenth at glue 6.
  used[1] = 6;
  used[5] = 3;
  used[6] = 1;
  used[7] = 1;
  reduce_retier_from_histogram(k, 1, used, 2, 6, 500, 900);
  assert(k.tier1_by_mode[1] == 1);
  assert(k.tier2_by_mode[1] == 5);
}

static void test_solve_with_restarts_still_correct() {
  Kernel k; // PHP(4,3): UNSAT even under tiny restart intervals
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 12);
  auto var = [](int p, int h) { return p * 3 + h + 1; };
  std::vector<Lit> scratch;
  for (int p = 0; p < 4; p++) {
    scratch.clear();
    for (int h = 0; h < 3; h++)
      scratch.push_back(internalize(var(p, h)));
    kernel_add_large_clause(k, scratch.data(), 3, false, 0);
  }
  for (int h = 0; h < 3; h++)
    for (int p1 = 0; p1 < 4; p1++)
      for (int p2 = p1 + 1; p2 < 4; p2++)
        kernel_add_binary_clause(k, internalize(-var(p1, h)),
                                 internalize(-var(p2, h)));
  // A restart storm posed on the policy instance, since schedules are policy
  // state. Alpha stays 0, so updates cannot wash the biased values out.
  DefaultPolicy storm;
  // solve() re-initialises its schedule at entry; this hatch opts out
  storm.reset_schedule_per_solve = false;
  storm.averages[0].initialized = true;
  storm.averages[0].fast_glue.biased = 100.0;
  storm.averages[0].fast_glue.value = 100.0;
  storm.averages[0].slow_glue.biased = 1.0;
  storm.averages[0].slow_glue.value = 1.0;
  storm.restart.next_conflicts = 0;
  storm.reduce.next_conflicts = 4; // and aggressive reduction
  assert(storm.solve(k) == 20);
}

static void test_restart_reuses_trail() {
  // A restart keeps every level whose decision outranks the next candidate:
  // here var 0 (stamp 4) beats var 2 (stamp 3) and var 1 (stamp 2) does not.
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3);
  heuristics_queue_bump(k, 0);
  kernel_assign_decision(k, internalize(1)); // var 0
  kernel_assign_decision(k, internalize(2)); // var 1
  assert(restart_apply(k) == ApplyResult::Ok);
  assert(k.level == 1);
  assert(k.values[internalize(1)] > 0);  // level-1 prefix kept
  assert(k.values[internalize(2)] == 0); // level 2 abandoned
  assert(k.stats.restarts_reused == 1);

  heuristics_queue_bump(k, 1); // now every level outranks the candidate
  kernel_assign_decision(k, internalize(2));
  assert(restart_apply(k) == ApplyResult::Ok);
  assert(k.level == 2); // full reuse: no backtrack at all
  assert(k.stats.restarts_reused == 2);
  assert(k.stats.restarts == 2);
}

static void test_restart_apply_to() {
  // N1: the keep-level is the policy's choice, not the rule's
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3);
  heuristics_queue_bump(k, 0); // var 0 freshest (stamp 4 > var 2's 3)
  kernel_assign_decision(k, internalize(1)); // var 0 at level 1
  kernel_assign_decision(k, internalize(2)); // var 1 at level 2
  assert(restart_reuse_level(k) == 1); // canonical would keep level 1...
  assert(restart_apply_to(k, 0) == ApplyResult::Ok); // ...full restart anyway
  assert(k.level == 0);
  assert(k.values[internalize(1)] == 0); // nothing kept
  assert(k.stats.restarts == 1);
  assert(k.stats.restarts_reused == 0);
  // keep-levels above the current level clamp to it (nothing abandoned)
  kernel_assign_decision(k, internalize(3));
  assert(restart_apply_to(k, 5) == ApplyResult::Ok);
  assert(k.level == 1);
  assert(k.values[internalize(3)] > 0);
  assert(k.stats.restarts == 2);
  assert(k.stats.restarts_reused == 1);
}

int main() {
  test_restart();
  test_restart_apply_to();
  test_restart_reuses_trail();
  test_collect_preserves_propagation();
  test_collect_accepts_policy_order();
  test_sparse_collect_preserves_prefix_watch_order();
  test_sparse_collect_forwards_garbage_prefix_watches();
  test_collect_reselects_watches_by_assignment();
  test_default_arena_order_tracks_search();
  test_reduce_deletes_and_logs();
  test_reduce_flushes_new_root_units_before_selection();
  test_reduce_prepare_propagates_root_before_collection();
  test_reduce_prepare_reports_root_conflict();
  test_root_propagation_consumes_unflushed_marker();
  test_reduce_used_retention();
  test_reduce_equal_rank_preserves_arena_order();
  test_tier2_band_tracks_the_live_tier();
  test_retier_uses_kissat_truncation();
  test_solve_with_restarts_still_correct();
  puts("test_restart_reduce OK");
  return 0;
}
