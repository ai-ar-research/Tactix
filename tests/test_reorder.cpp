// S7 Reorder: the weight formula and its exclusions, the two rankings it
// rewrites (VMTF queue, EVSIDS heap), the instance surface, and the guard.
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

// Weight formula: a binary contributes 1 to each literal, a ternary 1/2, a
// quaternary 1/4; the variable weight is max(w+,w-) + 2·min(w+,w-), so a
// both-polarity split outranks the same total mass on one polarity.
static void test_weight_formula() {
  Kernel k;
  ScopedHeuristics _h(k);
  kernel_reserve_vars(k, 6);
  addn(k, {1, 2});
  addn(k, {-1, 3, 4});
  addn(k, {5, 6, 3, 4});
  std::vector<double> w;
  reorder_weights(k, w);
  // (w+, w-) per var: 1 -> (1, 1/2); 2 -> (1, 0); 3 -> (3/4, 0); 5 -> (1/4, 0)
  assert(near(w[var_of(internalize(1))], 2.0));
  assert(near(w[var_of(internalize(2))], 1.0));
  assert(near(w[var_of(internalize(3))], 0.75));
  assert(near(w[var_of(internalize(5))], 0.25));
  // the both-polarity reward: var 1 outranks var 2 though both carry 1.5 total
  assert(w[var_of(internalize(1))] > w[var_of(internalize(2))]);
}

// Redundant clauses and clauses satisfied under the assignment contribute 0.
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

// Kissat deliberately stops inspecting a large clause when the 100th open
// literal is reached.  A satisfying root literal after that physical cutoff
// is not observed, and the clause contributes table[100].  Continuing through
// the tail looks equivalent mathematically but changes Reorder's exact heap.
static void test_max_size_stops_before_late_satisfaction() {
  Kernel k;
  ScopedHeuristics _h(k);
  kernel_reserve_vars(k, 101);
  std::vector<int> clause;
  for (int i = 1; i <= 101; i++)
    clause.push_back(i);
  addn(k, clause);
  kernel_assign_unit(k, internalize(101));

  std::vector<double> w;
  reorder_weights(k, w);
  assert(w[var_of(internalize(1))] == std::ldexp(1.0, -98));
}

// Focused: the heaviest variable must become the next decision.
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
  double prev = -1; // the whole queue is ascending in weight, tail to front
  for (Var v = k.h->queue_first; v != UINT32_MAX; v = k.h->links[v].next) {
    assert(w[v] >= prev);
    prev = w[v];
  }
}

// Stable: scores are rescaled and the weight added, so a prior score only ties.
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

// Kissat rescales before sorting stable Reorder candidates.  A large inactive
// score can make two distinct active scores underflow to the same value; the
// resulting internal-index tie then determines the equal-weight update order
// and the physical heap pose.  Sorting the pre-rescale values reverses it.
static void test_heap_sort_observes_rescaled_scores() {
  Kernel k;
  ScopedHeuristics _h(k);
  kernel_reserve_vars(k, 3);
  addn(k, {2, 3}); // equal structural weight for variables 2 and 3
  k.h->stable = true;
  heuristics_update_scores(k);
  heuristics_score_set(k, var_of(internalize(1)), 1e150);
  heuristics_score_set(k, var_of(internalize(2)), 1e-200);
  heuristics_score_set(k, var_of(internalize(3)), 2e-200);

  k.var_status.assign(k.nvars, VarStatus::Active);
  k.var_status[var_of(internalize(1))] = VarStatus::Eliminated;
  heuristics_deactivate_var(k, var_of(internalize(1)));
  assert(k.h->scores.stack.size() == 2);
  assert(k.h->scores.stack[0] == var_of(internalize(3)));

  assert(reorder_apply(k) == ApplyResult::Ok);
  assert(k.h->scores.score[var_of(internalize(2))] == 1.0);
  assert(k.h->scores.score[var_of(internalize(3))] == 1.0);
  assert(k.h->scores.stack[0] == var_of(internalize(2)) &&
         k.h->scores.stack[1] == var_of(internalize(3)));
}

// Stable Reorder's final tie is Kissat's current internal index (first-import
// order), not Tactix's stable variable identity or activation permutation.
// With equal weights and scores, the lowest import index is updated first and
// therefore reaches the heap root first.
static void test_heap_tie_uses_import_order() {
  Kernel k;
  ScopedHeuristics _h(k);
  kernel_reserve_vars(k, 3);
  kernel_record_import(k, 2);
  kernel_record_import(k, 0);
  kernel_record_import(k, 1);
  heuristics_seed_activation_order(k, {0, 1, 2});
  addn(k, {1, 2, 3});
  k.h->stable = true;
  k.h->scores.stack = {0, 1, 2};
  k.h->scores.pos = {0, 1, 2};
  k.h->scores.score.assign(3, 0.0);

  assert(reorder_apply(k) == ApplyResult::Ok);
  assert(k.h->scores.stack[0] == 2);
}

// A variable imported from a later-discarded input clause can exist as a
// stable Tactix identity without ever becoming Kissat ACTIVE. The activation
// image defines Reorder's candidate population; such an identity keeps its
// score even though the public decision surface remains complete.
static void test_inactive_import_is_not_reordered() {
  Kernel k;
  ScopedHeuristics _h(k);
  kernel_reserve_vars(k, 3);
  kernel_record_import(k, 0);
  kernel_record_import(k, 1);
  kernel_record_import(k, 2);
  heuristics_seed_activation_order(k, {2, 0}); // variable 1 never activates
  addn(k, {1, 2, 3});
  k.h->stable = true;
  k.h->scores.score.assign(3, 0.0);

  assert(reorder_apply(k) == ApplyResult::Ok);
  assert(k.h->scores.score[1] == 0.0);
}

// Kissat reorders ACTIVE variables only. Retired variables are disconnected
// from both rankings; feeding them back through queue_bump follows stale links
// and used to corrupt the focused queue on the first 10k-conflict Reorder.
static void test_retired_variables_stay_disconnected() {
  for (const bool stable : {false, true}) {
    Kernel k;
    ScopedHeuristics _h(k);
    kernel_reserve_vars(k, 6);
    addn(k, {1, 2});
    addn(k, {-1, 3});
    k.var_status.assign(k.nvars, VarStatus::Active);
    k.var_status[1] = VarStatus::Eliminated;
    k.var_status[4] = VarStatus::Substituted;
    heuristics_deactivate_var(k, 1);
    heuristics_deactivate_var(k, 4);
    // Overlapping retirement reports are idempotent.  A second dequeue must
    // not splice the queue through the disconnected sentinel links.
    heuristics_deactivate_var(k, 1);
    heuristics_deactivate_var(k, 4);
    assert(k.stats.deactivation_repeats == 2);
    k.h->stable = stable;
    if (stable)
      heuristics_update_scores(k);
    const Var queue_first = k.h->queue_first;
    const Var queue_last = k.h->queue_last;
    const double retired_score = k.h->scores.score[4];
    // Conflict analysis can still encounter a retired identity in stale
    // reason-side material.  Kissat filters those bumps through ACTIVE: the
    // disconnected variable must not be dequeued again or gain heap score.
    heuristics_queue_bump(k, 1);
    heuristics_score_bump(k, 4);
    assert(k.h->queue_first == queue_first && k.h->queue_last == queue_last);
    assert(k.h->links[1].prev == INVALID_VAR &&
           k.h->links[1].next == INVALID_VAR);
    assert(k.h->scores.score[4] == retired_score);
    // A later Substitute demotion batch can overlap identities retired by an
    // earlier rule. It must demote its still-connected members without
    // splicing those already-disconnected identities back into the queue.
    std::vector<uint8_t> marked(k.nvars, 0);
    marked[1] = marked[3] = marked[4] = 1;
    heuristics_demote_marked_vars(k, marked);
    assert(!heuristics_queue_contains(k, 1));
    assert(!heuristics_queue_contains(k, 4));
    assert(reorder_apply(k) == ApplyResult::Ok);
    assert(k.stats.reactivations == 0);
    uint32_t queued = 0;
    for (Var v = k.h->queue_first; v != INVALID_VAR; v = k.h->links[v].next)
      assert(++queued <= 4 && v != 1 && v != 4);
    assert(queued == 4);
    for (const Var v : k.h->scores.stack)
      assert(v != 1 && v != 4);
  }
}

// The VMTF search cache survives a mid-trail reorder: every variable after
// `search_idx` must stay assigned, the invariant `queue_next_unassigned`'s
// backward walk needs and `queue_bump` maintains only for what it moves.
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

// Instance surface: the active mode's ranking first; both always applicable.
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
  assert(vt.apply_instance(k, 99) == ApplyResult::NoOp); // unknown payload
  assert(vt.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::Ok);
  assert(vt.apply_instance(k, REORDER_QUEUE) == ApplyResult::Ok);
}

// Guard: `H` is a genuine precondition (the op rewrites H state) and a refuted
// kernel is not reorderable; the trail level is not constrained (N1).
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
  test_max_size_stops_before_late_satisfaction();
  test_queue_ranking();
  test_search_cache_survives_mid_trail();
  test_heap_ranking();
  test_heap_sort_observes_rescaled_scores();
  test_heap_tie_uses_import_order();
  test_inactive_import_is_not_reordered();
  test_retired_variables_stay_disconnected();
  test_instance_surface();
  test_guard();
  test_solve_unaffected();
  printf("test_reorder: all passed\n");
  return 0;
}
