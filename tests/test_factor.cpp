// Factor (BVA) unit tests. Extension variables are assigned by search but
// projected out of the model: the v-line ranges over original variables only.
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
// extension boundary, and leaves the model-output count at the originals.
static void test_extension_var_boundary() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
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

// A 2×3 grid factors to 2 dividers + 3 quotients over a fresh x.
static void test_factoring_2x3() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
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

  assert(has_binary(k, x, a) && has_binary(k, x, b)); // dividers
  for (const Lit rr : R)
    assert(has_binary(k, nx, rr)); // quotients
  for (const Lit f : {a, b})       // every original deleted
    for (const Lit rr : R)
      assert(!has_binary(k, f, rr));
}

// Fresh extension vars are demoted to the bottom of the VMTF queue in
// creation order, so the newest is least preferred — matching kissat
// factor.c:867-882 and cadical factor.cpp:799-802. Two disjoint grids in one
// apply are what makes that order observable.
static void test_demote_fresh_order() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 10); // two disjoint 2x3 grids over 0..4 and 5..9
  for (Var base : {0u, 5u})
    for (const Var fv : {base + 0, base + 1})
      for (const Var rv : {base + 2, base + 3, base + 4})
        kernel_add_binary_clause(k, make_lit(fv, false), make_lit(rv, false),
                                 ++k.next_clause_id);

  assert(k.factor_demote_fresh); // the shipped default
  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings == 2);
  assert(k.nvars == 12 && k.first_extension_var == 10); // fresh = 10, 11
  assert(k.fresh_var_events.size() == 1);
  assert(k.stats.factor_demotions == 0); // rules emit facts, not H mutations
  heuristics_consume_rule_advice(k);

  // one demotion per application, not per factoring: the walk is O(#queue)
  assert(k.stats.factor_demotions == 1);
  assert(k.stats.factor_demoted_vars == 2);

  // newest fresh var at the queue front, older behind it, originals behind
  assert(k.h->queue_first == 11);
  assert(k.h->links[11].next == 10);
  assert(k.h->links[10].next < 10); // an original follows the demoted block
  for (Var v = 0; v < 10; v++)
    assert(k.h->links[v].stamp > k.h->links[10].stamp &&
           k.h->links[v].stamp > k.h->links[11].stamp);
}

// The per-literal seed marks are policy and must not wedge: a watermark past
// everything suppresses the round, and the next binary add re-arms it.
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

  // one more binary touches a and p, so both seeds come back
  kernel_add_binary_clause(k, a, make_lit(2, true), ++k.next_clause_id);
  const int id = tactix_rule_id("Factor");
  assert(id >= 0);
  const RuleVTable &v = tactix_rules()[id];
  assert(v.apply_instance(k, INSTANCE_CANONICAL) == ApplyResult::Ok);
  assert(k.stats.factorings == 1 && "a re-armed seed must be reachable again");
}

int main() {
  {
    Kernel k;
    assert(k.factor_cand_rounds == 2 &&
           "Factor candidate pruning follows both reference defaults");
  }
  // Density admission is DefaultPolicy's, not the rule's, so these
  // below-threshold grids factor through the rule's own surface.
  test_extension_var_boundary();
  test_factoring_2x3();
  test_touch_filter_is_policy_only();
  test_demote_fresh_order();
  std::printf("test_factor: all passed\n");
  return 0;
}
