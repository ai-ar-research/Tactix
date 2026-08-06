// S7 Reorder unit tests: the weight formula (clause-size decay, both-polarity
// reward, satisfied/redundant exclusions), the two rankings it can rewrite
// (VMTF queue order and EVSIDS heap order), the instance surface, the guard,
// and that a solve stays correct with the op applied.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "policies/ops/reorder/reorder.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace tactix;

static void addn(Kernel &k, std::vector<int> ds, bool redundant = false) {
  std::vector<Lit> l;
  for (const int d : ds)
    l.push_back(internalize(d));
  if (l.size() == 2)
    kernel_add_binary_clause(k, l[0], l[1]);
  else
    kernel_add_large_clause(k, l.data(), static_cast<uint32_t>(l.size()),
                            redundant, 0);
}

static bool near(double a, double b) { return std::fabs(a - b) < 1e-12; }

// THE FORMULA. A binary contributes 1 to each of its literals, a ternary 1/2, a
// quaternary 1/4; the variable weight is max(w+,w-) + 2·min(w+,w-), so a
// variable split evenly over both polarities outranks one with the same total
// mass on a single polarity.
static void test_weight_formula() {
  Kernel k;
  ScopedHeuristics _h(k);
  kernel_reserve_vars(k, 6);
  addn(k, {1, 2});        // binary: 1 to lit 1 and lit 2
  addn(k, {-1, 3, 4});    // ternary: 1/2 to lits -1, 3, 4
  addn(k, {5, 6, 3, 4});  // quaternary: 1/4 each
  std::vector<double> w;
  reorder_weights(k, w);
  // var 1: w+ = 1, w- = 1/2 -> max + 2·min = 1 + 1 = 2
  assert(near(w[var_of(internalize(1))], 2.0));
  // var 2: w+ = 1, w- = 0 -> 1
  assert(near(w[var_of(internalize(2))], 1.0));
  // var 3: w+ = 1/2 + 1/4 = 3/4, w- = 0
  assert(near(w[var_of(internalize(3))], 0.75));
  // var 5: w+ = 1/4, w- = 0
  assert(near(w[var_of(internalize(5))], 0.25));
  // the both-polarity reward: var 1 outranks var 2 though both carry 1.5 total
  assert(w[var_of(internalize(1))] > w[var_of(internalize(2))]);
}

// Redundant clauses are out of scope (they are search residue, not structure),
// and a clause satisfied under the current assignment contributes nothing.
static void test_exclusions() {
  Kernel k;
  ScopedHeuristics _h(k);
  kernel_reserve_vars(k, 5);
  addn(k, {1, 2, 3}, /*redundant=*/true);
  std::vector<double> w;
  reorder_weights(k, w);
  assert(near(w[var_of(internalize(1))], 0.0));

  Kernel k2;
  ScopedHeuristics _h2(k2);
  kernel_reserve_vars(k2, 5);
  addn(k2, {1, 2, 3});
  addn(k2, {4, 5, 3});
  kernel_assign_unit(k2, internalize(1)); // satisfies the first clause
  reorder_weights(k2, w);
  assert(near(w[var_of(internalize(2))], 0.0));
  assert(near(w[var_of(internalize(4))], 0.5)); // the second still counts
}

// FOCUSED: the heaviest variable must end up the next decision. Var 1 is the
// heaviest by construction, and starts NOT at the queue front.
static void test_queue_ranking() {
  Kernel k;
  ScopedHeuristics _h(k);
  kernel_reserve_vars(k, 4);
  addn(k, {1, 2});
  addn(k, {-1, 3});
  addn(k, {2, 3, 4});
  k.h->stable = false;
  // reserve_vars enqueues in index order, so var 3 is the front (last enqueued)
  assert(heuristics_queue_next_unassigned(k) == 3);
  assert(reorder_apply(k) == ApplyResult::Ok);
  assert(k.stats.reordered == 1);
  std::vector<double> w;
  reorder_weights(k, w);
  Var heaviest = 0;
  for (Var v = 1; v < k.nvars; v++)
    if (w[v] > w[heaviest])
      heaviest = v;
  assert(heaviest == var_of(internalize(1)));
  assert(heuristics_queue_next_unassigned(k) == heaviest);
  // and the whole queue is in ascending weight order from tail to front
  double prev = -1;
  for (Var v = k.h->queue_first; v != UINT32_MAX; v = k.h->links[v].next) {
    assert(w[v] >= prev);
    prev = w[v];
  }
}

// STABLE: scores are rescaled and the weight added, so the structural ranking
// dominates but a pre-existing score still breaks ties.
static void test_heap_ranking() {
  Kernel k;
  ScopedHeuristics _h(k);
  kernel_reserve_vars(k, 4);
  addn(k, {1, 2});
  addn(k, {-1, 3});
  addn(k, {2, 3, 4});
  k.h->stable = true;
  heuristics_update_scores(k);
  // bump var 4 (the lightest) hard: EVSIDS alone would decide it first
  for (int i = 0; i < 20; i++) {
    heuristics_score_bump(k, var_of(internalize(4)));
    heuristics_scores_decay(k);
  }
  assert(heuristics_heap_next_unassigned(k) == var_of(internalize(4)));
  assert(reorder_apply(k) == ApplyResult::Ok);
  assert(heuristics_heap_next_unassigned(k) == var_of(internalize(1)));
}

// The VMTF search cache survives a MID-TRAIL reorder — the invariant "every
// variable after search_idx is assigned", which the backward walk in
// queue_next_unassigned depends on. `queue_bump` maintains the cache only for
// variables it MOVES and early-returns on one already at the tail, so this is
// where a mid-trail application (N1) would break if the argument in reorder.cpp
// were wrong. Exercised at every trail depth, in both bump directions.
static void test_search_cache_survives_mid_trail() {
  for (int assign = 0; assign <= 3; assign++) {
    Kernel k;
    ScopedHeuristics _h(k);
    kernel_reserve_vars(k, 4);
    addn(k, {1, 2});
    addn(k, {-1, 3});
    addn(k, {2, 3, 4});
    k.h->stable = false;
    for (int i = 1; i <= assign; i++)
      kernel_assign_decision(k, internalize(i));
    assert(reorder_apply(k) == ApplyResult::Ok);
    // every variable strictly after the cache must be assigned
    bool past = false;
    for (Var v = k.h->queue_first; v != UINT32_MAX; v = k.h->links[v].next) {
      if (past)
        assert(k.values[make_lit(v, false)] != 0);
      if (v == k.h->search_idx)
        past = true;
    }
    assert(past && "the cache must point INTO the queue");
    if (assign < 4) // some variable is still open: the walk must find one
      assert(k.values[make_lit(heuristics_queue_next_unassigned(k), false)] ==
             0);
  }
}

// The instance surface: canonical first (the ranking the active mode reads),
// then the other one; both are always applicable.
static void test_instance_surface() {
  Kernel k;
  ScopedHeuristics _h(k);
  kernel_reserve_vars(k, 3);
  addn(k, {1, 2});
  const int id = tactix_rule_id("Reorder");
  assert(id >= 0);
  const RuleVTable &vt = tactix_rules()[id];
  assert(vt.kind == ActionKind::PolicyOp);
  assert(vt.guard(k));
  uint64_t inst[4];
  assert(vt.instances(k, inst, 4) == 2);
  assert(inst[0] == REORDER_QUEUE && inst[1] == REORDER_HEAP); // focused
  k.h->stable = true;
  assert(vt.instances(k, inst, 4) == 2);
  assert(inst[0] == REORDER_HEAP && inst[1] == REORDER_QUEUE);
  assert(vt.instances(k, inst, 1) == 1);
  // an unknown payload is a NoOp, never unsound
  assert(vt.apply_instance(k, 99) == ApplyResult::NoOp);
  assert(vt.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::Ok);
  assert(vt.apply_instance(k, REORDER_QUEUE) == ApplyResult::Ok);
}

// GUARD: `H` is a genuine precondition (the op rewrites H state); a refuted
// kernel is not reorderable. Nothing about the trail level is required — a
// policy may reorder mid-trail (N1), the weight is then read off the current
// assignment.
static void test_guard() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  assert(!reorder_guard(k)); // no H attached
  ScopedHeuristics _h(k);
  assert(reorder_guard(k));
  kernel_assign_decision(k, internalize(1));
  assert(k.level == 1 && reorder_guard(k));
  assert(reorder_apply(k) == ApplyResult::Ok);
  k.inconsistent = true;
  assert(!reorder_guard(k));
}

// End to end: reordering mid-solve leaves the verdict intact.
static void test_solve_unaffected() {
  Kernel k;
  ScopedHeuristics _h(k);
  kernel_reserve_vars(k, 3);
  addn(k, {1, 2, 3});
  addn(k, {1, 2, -3});
  addn(k, {1, -2, 3});
  addn(k, {1, -2, -3});
  addn(k, {-1, 2, 3});
  addn(k, {-1, 2, -3});
  addn(k, {-1, -2, 3});
  addn(k, {-1, -2, -3});
  assert(reorder_apply(k) == ApplyResult::Ok);
  assert(default_solve(k) == 20);
}

int main() {
  test_weight_formula();
  test_exclusions();
  test_queue_ranking();
  test_search_cache_survives_mid_trail();
  test_heap_ranking();
  test_instance_surface();
  test_guard();
  test_solve_unaffected();
  printf("test_reorder: all passed\n");
  return 0;
}
