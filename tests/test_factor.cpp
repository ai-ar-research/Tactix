// E2 Factor (BVA). S0: extension-variable allocation + model projection.
// Extension variables (added mid-solve by Factor) are assigned by search but
// projected out of the reported model — the v-line ranges over original vars only.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/factor/factor.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

static bool has_binary(const Kernel &k, Lit a, Lit b) {
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[a].offset;
  for (uint32_t i = 0; i < k.watches.heads[a].size;) {
    const Watch w{ws[i]};
    if (w.binary()) {
      if (w.other() == b)
        return true;
      i += 1;
    } else
      i += 2;
  }
  return false;
}

// kernel_new_extension_vars grows all per-var state additively, records the
// extension boundary, and keeps the model-output count at the original variables.
static void test_extension_var_boundary() {
  Kernel k;
  ScopedHeuristics _hg_k(k); // attach H (the session owns it)
  kernel_reserve_vars(k, 5); // 5 original vars: 0..4
  assert(k.nvars == 5);
  assert(k.first_extension_var == UINT32_MAX);
  assert(kernel_num_output_vars(k) == 5); // all original

  const Var first = kernel_new_extension_vars(k, 2); // add 2 extension vars
  assert(first == 5);
  assert(k.nvars == 7);
  assert(k.first_extension_var == 5);          // boundary set to first extension
  assert(kernel_num_output_vars(k) == 5);      // extension vars projected out
  assert(k.values.size() == 2 * 7);            // per-var state grew (by lit)
  assert(k.h->saved.size() == 7 && k.h->scores.score.size() == 7); // and others

  const Var f2 = kernel_new_extension_vars(k, 1); // add one more
  assert(f2 == 7);
  assert(k.nvars == 8);
  assert(k.first_extension_var == 5);          // boundary unchanged
  assert(kernel_num_output_vars(k) == 5);
}

// A 2×3 binary grid F={a,b} × R={p,q,r} (6 clauses) factors to 2 dividers + 3
// quotients (5 clauses) over a fresh x; originals are deleted.
static void test_factoring_2x3() {
  Kernel k;
  ScopedHeuristics _hg_k(k); // attach H (the session owns it)
  kernel_reserve_vars(k, 5); // a=0 b=1 p=2 q=3 r=4
  const Lit a = make_lit(0, false), b = make_lit(1, false);
  const Lit p = make_lit(2, false), q = make_lit(3, false), r = make_lit(4, false);
  const Lit R[3] = {p, q, r};
  for (const Lit f : {a, b})
    for (const Lit rr : R)
      kernel_add_binary_clause(k, f, rr, ++k.next_clause_id);

  assert(factor_guard(k));
  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings == 1);
  assert(k.nvars == 6 && k.first_extension_var == 5); // one fresh var x = 5
  const Lit x = make_lit(5, false), nx = make_lit(5, true);

  // dividers (x∨a),(x∨b) present
  assert(has_binary(k, x, a) && has_binary(k, x, b));
  // quotients (¬x∨p),(¬x∨q),(¬x∨r) present
  for (const Lit rr : R)
    assert(has_binary(k, nx, rr));
  // every original (f∨r) deleted
  for (const Lit f : {a, b})
    for (const Lit rr : R)
      assert(!has_binary(k, f, rr));
}

// SCHED-C5: fresh extension vars are demoted to the BOTTOM of the VMTF queue,
// and the ORDER inside the demoted block matters. Both references prepend in
// CREATION order (kissat factor.c:867-882 walks `fresh` begin->end; cadical
// factor.cpp:799-802 `queue.bury`s in the same direction), so the var created
// LAST ends up at `queue_first` = least preferred. Ours ran the loop backwards
// for weeks, which is undetectable from any aggregate and cost `rbsat` 4.7x the
// conflicts. Two disjoint grids in ONE apply are what makes the order testable.
static void test_demote_fresh_order() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 10); // two disjoint 2x3 grids over 0..4 and 5..9
  for (Var base : {0u, 5u})
    for (const Var fv : {base + 0, base + 1})
      for (const Var rv : {base + 2, base + 3, base + 4})
        kernel_add_binary_clause(k, make_lit(fv, false), make_lit(rv, false),
                                 ++k.next_clause_id);

  assert(k.factor_demote_fresh); // the shipped default since SCHED-C5
  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings == 2);
  assert(k.nvars == 12 && k.first_extension_var == 10); // fresh = 10, 11

  // Fired exactly once for the whole application (never once per factoring —
  // the O(#queue) per-call cost), and counted both fresh vars.
  assert(k.stats.factor_demotions == 1);
  assert(k.stats.factor_demoted_vars == 2);

  // NEWEST fresh var is least preferred, i.e. at the queue front, with the
  // older one immediately behind it and every original behind both.
  assert(k.h->queue_first == 11);
  assert(k.h->links[11].next == 10);
  assert(k.h->links[10].next < 10); // an original follows the demoted block
  for (Var v = 0; v < 10; v++)
    assert(k.h->links[v].stamp > k.h->links[10].stamp &&
           k.h->links[v].stamp > k.h->links[11].stamp);
}

// SCHED-F4 · the per-literal seed marks are POLICY, and they must not wedge.
// (a) with the watermark advanced past everything the round seeds nothing and
//     NoOps — the suppression is real;
// (b) the very next binary add re-arms it and the SAME factoring is applied,
//     through the registry surface. A build whose mark could not be re-armed
//     would fail (b), which is the wedge D3 measured.
static void test_touch_filter_is_policy_only() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 5);
  const Lit a = make_lit(0, false), b = make_lit(1, false);
  const Lit p = make_lit(2, false), q = make_lit(3, false),
            r = make_lit(4, false);
  for (const Lit f : {a, b})
    for (const Lit rr : {p, q, r})
      kernel_add_binary_clause(k, f, rr, ++k.next_clause_id);

  k.factor_touch_mark = k.touch_epoch; // "already seeded everything"
  assert(factor_guard(k));
  assert(factor_apply(k) == ApplyResult::NoOp);
  assert(k.stats.factorings == 0);
  assert(k.stats.factor_mark_skipped >= 10);
  assert(k.stats.factor_rearmed == 0 && "nothing was seeded, nothing to resume");

  // one more binary over a and p: both variables are touched, so both seeds
  // come back and the grid is found again.
  kernel_add_binary_clause(k, a, make_lit(2, true), ++k.next_clause_id);
  const int id = tactix_rule_id("Factor");
  assert(id >= 0);
  const RuleVTable &v = tactix_rules()[id];
  assert(v.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::Ok);
  assert(k.stats.factorings == 1 && "a re-armed seed must be reachable again");
}

int main() {
  // No TACTIX_FACTOR_FORCE needed: the density admission heuristic lives in
  // DefaultPolicy, not in the rule, so these tiny below-threshold grids factor
  // through the rule's own surface (see tests/test_factor_n1.cpp).
  test_extension_var_boundary();
  test_factoring_2x3();
  test_touch_filter_is_policy_only();
  test_demote_fresh_order();
  std::printf("test_factor: all passed\n");
  return 0;
}
