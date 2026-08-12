// Factor (BVA) unit tests. Extension variables are assigned by search but
// projected out of the model: the v-line ranges over original variables only.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/factor/factor.hpp"

#include <algorithm>
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

static std::vector<Lit> binary_watch_partners(const Kernel &k, Lit lit) {
  std::vector<Lit> result;
  for (const WatchEntry watch : k.watches.entries(lit))
    if (watch.binary())
      result.push_back(watch.other());
  return result;
}

static void add_large(Kernel &k, std::initializer_list<Lit> lits) {
  const std::vector<Lit> clause(lits);
  kernel_add_large_clause(k, clause.data(), static_cast<uint32_t>(clause.size()),
                          /*redundant=*/false, /*glue=*/0,
                          ++k.next_clause_id);
}

static uint32_t count_live_large(Kernel &k, std::initializer_list<Lit> lits) {
  std::vector<Lit> expected(lits);
  std::sort(expected.begin(), expected.end());
  uint32_t result = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && c->size == expected.size()) {
      std::vector<Lit> actual(c->begin(), c->end());
      std::sort(actual.begin(), actual.end());
      result += actual == expected;
    }
    ref += clause_words(c->size);
  }
  return result;
}

static std::vector<Ref> large_watch_refs(const Kernel &k, Lit lit) {
  std::vector<Ref> result;
  for (const WatchEntry watch : k.watches.entries(lit))
    if (!watch.binary())
      result.push_back(watch.ref());
  return result;
}

static std::vector<Lit> large_watch_blockers(const Kernel &k, Lit lit) {
  std::vector<Lit> result;
  for (const WatchEntry watch : k.watches.entries(lit))
    if (!watch.binary())
      result.push_back(watch.other());
  return result;
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
  // Kissat applies the selected quotient chain backwards: the last selected
  // factor is the first divider in the fresh literal's watch list.
  assert((binary_watch_partners(k, x) == std::vector<Lit>{b, a}));
  assert((binary_watch_partners(k, nx) == std::vector<Lit>{p, q, r}));
  for (const Lit rr : R)
    assert(has_binary(k, nx, rr)); // quotients
  for (const Lit f : {a, b})       // every original deleted
    for (const Lit rr : R)
      assert(!has_binary(k, f, rr));
}

// Factor's effort currency is Kissat's dense-watch scan currency, and its
// second quit point is checked after each current quotient occurrence.  A
// budget of four therefore pays 3 ticks for first_factor, 2 for the quotient
// stack, and 2 for the first remainder watch list before declining the grid.
static void test_dense_scan_budget_boundary() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 5);
  const Lit a = make_lit(0, false), b = make_lit(1, false);
  const Lit p = make_lit(2, false), q = make_lit(3, false),
            r = make_lit(4, false);
  for (const Lit f : {a, b})
    for (const Lit remainder : {p, q, r})
      kernel_add_binary_clause(k, f, remainder, ++k.next_clause_id);

  k.work_budget = 4;
  assert(factor_apply(k) == ApplyResult::NoOp);
  assert(k.stats.factorings == 0);
  assert(k.stats.factor_ticks == 7);
}

// `factoring.initial` is the current literal universe at entry, not the
// original-variable boundary.  An extension literal created by an earlier
// Factor application is therefore a legal factor in a later application.
static void test_prior_extension_literal_is_a_later_factor() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 4); // a=0, p=1, q=2, r=3
  const Var extension = kernel_new_extension_vars(k, 1);
  const Lit a = make_lit(0, false), nx = make_lit(extension, true);
  const Lit p = make_lit(1, false), q = make_lit(2, false),
            r = make_lit(3, false);
  for (const Lit factor : {a, nx})
    for (const Lit remainder : {p, q, r})
      kernel_add_binary_clause(k, factor, remainder, ++k.next_clause_id);

  // Force `a` to be the only first-factor seed.  The old original-variable
  // bound then rejected `nx` as the next factor and incorrectly no-oped.
  std::fill(k.factor_marks.begin(), k.factor_marks.end(), 0);
  k.factor_marks[a] = 1;
  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings == 1);
  assert(k.nvars == 6);
}

// Kissat enters Factor dense mode without saving/reconnecting binaries.  The
// current sparse watch order therefore owns quotient-column order; import rank
// must not silently re-sort it.  Give q an earlier import rank than p while
// inserting the grid columns as p,q,r to distinguish those two contracts.
static void test_binary_quotients_preserve_sparse_watch_order() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 5);
  for (const Var v : {0u, 1u, 3u, 2u, 4u})
    kernel_record_import(k, v);
  const Lit a = make_lit(0, false), b = make_lit(1, false);
  const Lit p = make_lit(2, false), q = make_lit(3, false),
            r = make_lit(4, false);
  for (const Lit f : {a, b})
    for (const Lit remainder : {p, q, r})
      kernel_add_binary_clause(k, f, remainder, ++k.next_clause_id);

  assert(factor_apply(k) == ApplyResult::Ok);
  const Lit nx = make_lit(5, true);
  assert((binary_watch_partners(k, nx) == std::vector<Lit>{p, q, r}));
}

// Factor eligibility is per literal in Kissat.  Opposite polarities of the
// same variable can therefore form the two rows of a profitable grid; treating
// factor identity as per-variable skipped this exact shape on sted and shifted
// all later extension-variable definitions.
static void test_opposite_polarities_can_share_a_grid() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 4); // a=0 p=1 q=2 r=3
  const Lit a = make_lit(0, false), na = negate(a);
  const Lit p = make_lit(1, false), q = make_lit(2, false),
            r = make_lit(3, false);
  for (const Lit f : {a, na})
    for (const Lit remainder : {p, q, r})
      kernel_add_binary_clause(k, f, remainder, ++k.next_clause_id);

  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings == 1);
  assert(k.nvars == 5 && k.first_extension_var == 4);
  const Lit x = make_lit(4, false), nx = negate(x);
  assert(has_binary(k, x, a) && has_binary(k, x, na));
  for (const Lit remainder : {p, q, r})
    assert(has_binary(k, nx, remainder));
}

// Factor has no reference-side dimension ceiling: acceptance is controlled by
// profitability and the policy effort budget.  This grid crosses Tactix's
// former 4,096-column implementation cap and must still be reachable through
// the rule surface.
static void test_factoring_beyond_former_dimension_cap() {
  constexpr uint32_t columns = 4097;
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 2 + columns);
  const Lit a = make_lit(0, false), b = make_lit(1, false);
  for (uint32_t i = 0; i < columns; i++) {
    const Lit remainder = make_lit(2 + i, false);
    kernel_add_binary_clause(k, a, remainder, ++k.next_clause_id);
    kernel_add_binary_clause(k, b, remainder, ++k.next_clause_id);
  }

  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings == 1);
  assert(k.nvars == 3 + columns);
}

// Factor enters Kissat's null-stack dense path, which retains root-satisfied
// binaries.  Thus root-true p remains a structural column in this 2x3 grid.
// Congruence's distinct non-null dense path is the one that collects it.
static void test_satisfied_binary_remainder_is_retained() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 5);
  const Lit a = make_lit(0, false), b = make_lit(1, false);
  const Lit p = make_lit(2, false), q = make_lit(3, false),
            r = make_lit(4, false);
  for (const Lit f : {a, b})
    for (const Lit remainder : {p, q, r})
      kernel_add_binary_clause(k, f, remainder, ++k.next_clause_id);
  kernel_assign_unit(k, p);

  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings == 1);
  assert(k.nvars == 6);
  assert(!has_binary(k, a, p) && !has_binary(k, b, p));
}

static void test_retired_literal_is_not_a_factor_candidate() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 5);
  const Lit retired = make_lit(0, false), active = make_lit(1, false);
  for (const Lit remainder : {make_lit(2, false), make_lit(3, false),
                              make_lit(4, false)}) {
    kernel_add_binary_clause(k, retired, remainder, ++k.next_clause_id);
    kernel_add_binary_clause(k, active, remainder, ++k.next_clause_id);
  }
  k.var_status.resize(k.nvars, VarStatus::Active);
  k.var_status[var_of(retired)] = VarStatus::Substituted;

  (void)factor_apply(k); // the transposed grid may still factor active p/q/r
  for (Var v = 5; v < k.nvars; v++)
    assert(!has_binary(k, make_lit(v, false), retired) &&
           "a retired literal must never become a divider candidate");
}

static void test_assigned_large_remainder() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 8);
  k.factor_size = 3;
  const Lit a = make_lit(0, false), b = make_lit(1, false);
  const Lit p = make_lit(2, false), u = make_lit(3, false);
  const Lit q = make_lit(4, false), v = make_lit(5, false);
  const Lit r = make_lit(6, false), w = make_lit(7, false);
  for (const Lit f : {a, b}) {
    add_large(k, {f, p, u});
    add_large(k, {f, q, v});
    add_large(k, {f, r, w});
  }
  kernel_assign_unit(k, p);

  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings == 1);
  const Lit nx = make_lit(8, true);
  assert(count_live_large(k, {nx, p, u}) == 1);
  assert(count_live_large(k, {a, p, u}) == 0);
  assert(count_live_large(k, {b, p, u}) == 0);
}

// Remainder interning canonicalizes content only for matching.  Kissat emits
// each large quotient by copying the selected last-factor clause in its
// existing pose and removing that factor.  Sorting the emitted remainder
// instead changes the quotient's initial watched pair after sparse resume.
static void test_large_quotient_preserves_last_factor_pose() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 8);
  k.factor_size = 3;
  const Lit a = make_lit(0, false), b = make_lit(1, false);
  const Lit p = make_lit(2, false), u = make_lit(3, false);
  const Lit q = make_lit(4, false), v = make_lit(5, false);
  const Lit r = make_lit(6, false), w = make_lit(7, false);
  for (const auto &[lo, hi] :
       {std::pair<Lit, Lit>{p, u}, {q, v}, {r, w}}) {
    add_large(k, {a, lo, hi});
    add_large(k, {b, hi, lo});
  }

  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings == 1);
  const Lit nx = make_lit(8, true);
  assert((large_watch_blockers(k, nx) == std::vector<Lit>{u, v, w}));
}

// Kissat's quotient stack is a clause-occurrence multiset.  Three equal large
// remainders in each of two rows are therefore a profitable 2x3 grid, not a
// deduplicated 2x1 set.  Every original Ref and every new quotient occurrence
// remains independently proof-addressable.
static void test_duplicate_large_remainder_occurrences() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 4); // a=0 b=1 p=2 q=3
  const Lit a = make_lit(0, false), b = make_lit(1, false);
  const Lit p = make_lit(2, false), q = make_lit(3, false);
  for (unsigned copy = 0; copy < 3; copy++) {
    add_large(k, {a, p, q});
    add_large(k, {b, p, q});
  }
  assert(count_live_large(k, {a, p, q}) == 3);
  assert(count_live_large(k, {b, p, q}) == 3);

  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings == 1);
  assert(k.nvars == 5 && k.first_extension_var == 4);
  const Lit nx = make_lit(4, true);
  assert(count_live_large(k, {a, p, q}) == 0);
  assert(count_live_large(k, {b, p, q}) == 0);
  assert(count_live_large(k, {nx, p, q}) == 3);
}

// Kissat's dense-mode transition flushes repeated binary watch pairs before
// Factor connects its occurrence lists. Copies therefore cannot manufacture a
// profitable 2x3 grid from only two logical pairs.
static void test_duplicate_binary_occurrences_are_flushed() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 3); // a=0 b=1 p=2
  const Lit a = make_lit(0, false), b = make_lit(1, false);
  const Lit p = make_lit(2, false);
  for (unsigned copy = 0; copy < 3; copy++) {
    kernel_add_binary_clause(k, a, p, ++k.next_clause_id);
    kernel_add_binary_clause(k, b, p, ++k.next_clause_id);
  }

  assert(factor_apply(k) == ApplyResult::NoOp);
  assert(k.stats.factorings == 0);
  assert(kernel_binary_copies(k, a, p) == 1);
  assert(kernel_binary_copies(k, b, p) == 1);
}

// Factor's dense-mode lifecycle ends by rebuilding every live large watch in
// arena order, even when no profitable grid was found.  Kissat does this in
// `kissat_resume_sparse_mode`; retaining the incoming sparse order changes
// propagation/reason order after an otherwise identical Factor pass.
static void test_sparse_resume_rebuilds_large_watch_order() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 5);
  k.factor_size = 3;
  const Lit a = make_lit(0, false), b = make_lit(1, false),
            c = make_lit(2, false), d = make_lit(3, false),
            e = make_lit(4, false);
  const Lit first_lits[3] = {a, b, c};
  const Lit second_lits[3] = {a, d, e};
  const Ref first =
      kernel_add_large_clause(k, first_lits, 3, false, 0, ++k.next_clause_id);
  const Ref second =
      kernel_add_large_clause(k, second_lits, 3, false, 0, ++k.next_clause_id);
  assert((large_watch_refs(k, a) == std::vector<Ref>{first, second}));

  kernel_swap_large_watch_refs(k, a, first, second, b, d);
  assert((large_watch_refs(k, a) == std::vector<Ref>{second, first}));

  assert(factor_apply(k) == ApplyResult::NoOp);
  assert((large_watch_refs(k, a) == std::vector<Ref>{first, second}));
}

// The first 3x3 grid leaves two partial rows.  Its fresh quotient would
// complete them only by using the new extension literal as a third factor.
// Kissat grows its dense remainder universe but keeps `factoring.initial`
// fixed, so a same-pass extension literal is never a next factor.
static void test_fresh_large_quotients_do_not_become_factors() {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 11);
  k.factor_size = 3;
  k.factor_composite = true;
  k.touch_filter = false;

  const Lit a = make_lit(0, false), b = make_lit(1, false),
            c = make_lit(2, false), h = make_lit(3, false),
            j = make_lit(4, false);
  const Lit p = make_lit(5, false), u = make_lit(6, false),
            q = make_lit(7, false), v = make_lit(8, false),
            r = make_lit(9, false), w = make_lit(10, false);
  const std::pair<Lit, Lit> remainders[3] = {{p, u}, {q, v}, {r, w}};
  for (const Lit f : {a, b, c})
    for (const auto &[x, y] : remainders)
      add_large(k, {f, x, y});
  const std::pair<Lit, Lit> partial[2] = {remainders[0], remainders[1]};
  for (const Lit f : {h, j})
    for (const auto &[x, y] : partial)
      add_large(k, {f, x, y});

  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings == 1);
  assert(k.nvars == 12 && k.first_extension_var == 11);
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

  std::fill(k.factor_marks.begin(), k.factor_marks.end(), 0);
  // "already seeded everything": Factor has cleared every literal bit.
  assert(factor_guard(k));
  assert(factor_apply(k) == ApplyResult::NoOp);
  assert(k.stats.factorings == 0);
  assert(k.stats.factor_mark_skipped >= 10);
  assert(k.stats.factor_rearmed == 0);

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
  test_dense_scan_budget_boundary();
  test_prior_extension_literal_is_a_later_factor();
  test_binary_quotients_preserve_sparse_watch_order();
  test_opposite_polarities_can_share_a_grid();
  test_factoring_beyond_former_dimension_cap();
  test_satisfied_binary_remainder_is_retained();
  test_retired_literal_is_not_a_factor_candidate();
  test_assigned_large_remainder();
  test_large_quotient_preserves_last_factor_pose();
  test_duplicate_large_remainder_occurrences();
  test_duplicate_binary_occurrences_are_flushed();
  test_sparse_resume_rebuilds_large_watch_order();
  test_fresh_large_quotients_do_not_become_factors();
  test_touch_filter_is_policy_only();
  test_demote_fresh_order();
  std::printf("test_factor: all passed\n");
  return 0;
}
