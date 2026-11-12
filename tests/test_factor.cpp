// Factor (BVA) unit tests. Extension variables are assigned by search but
// projected out of the model: the v-line ranges over original variables only.
// Density admission is DefaultPolicy's, not the rule's, so these
// below-threshold grids factor through the rule's own surface.
#include "unit_test.hpp"
#include "rule_test_utils.hpp"
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/factor/factor.hpp"

#include <optional>

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
    ref += clause_actual_words(c);
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

// The 2x3 binary grid {base, base+1} x {base+2, base+3, base+4}, row by row.
static void grid_2x3(Kernel &k, Var base) {
  for (const Var f : {base, base + 1})
    for (const Var r : {base + 2, base + 3, base + 4})
      kernel_add_binary_clause(k, make_lit(f, false), make_lit(r, false),
                               ++k.next_clause_id);
}

// kernel_new_extension_vars grows all per-var state additively, records the
// extension boundary, and leaves the model-output count at the originals.
TEST(test_extension_var_boundary) {
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
  for (Var v = 8; v < 40; v++) {
    assert(kernel_new_extension_vars(k, 1) == v);
    assert(k.compact_import_order[v] == v);
    assert(k.compact_import_position[v] == v);
  }
  assert(k.internal_to_external.capacity() > k.nvars &&
         "single-variable extension growth must retain spare capacity");
}

// A 2×3 grid factors to 2 dividers + 3 quotients over a fresh x.
TEST(test_factoring_2x3) {
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
  assert((binary_partners(k, x) == std::vector<Lit>{b, a}));
  assert((binary_partners(k, nx) == std::vector<Lit>{p, q, r}));
  for (const Lit rr : R)
    assert(has_binary(k, nx, rr)); // quotients
  for (const Lit f : {a, b})       // every original deleted
    for (const Lit rr : R)
      assert(!has_binary(k, f, rr));
}

// Late heuristics attachment reconstructs a pre-attach Factor extension's
// activation rank.
TEST(test_late_heuristics_attach_keeps_factor_extension) {
  const auto make_grid = [](Kernel &k) {
    kernel_reserve_vars(k, 5);
    k.input_activation_order = {0, 1, 2, 3, 4};
    for (Var v = 0; v < 5; ++v)
      kernel_record_import(k, v);
    grid_2x3(k, 0);
  };

  Kernel early;
  make_grid(early);
  ScopedHeuristics early_h(early);
  assert(factor_apply(early) == ApplyResult::Ok);

  Kernel late;
  make_grid(late);
  assert(factor_apply(late) == ApplyResult::Ok);
  ScopedHeuristics late_h(late);
  assert(late.nvars == 6 && late.unassigned_active == 6);
  assert(late.compact_import_order == early.compact_import_order);
  assert(late.compact_import_order ==
         std::vector<Var>({0, 1, 2, 3, 4, 5}));
  assert(late.compact_import_position == early.compact_import_position);
  assert(late.h->activation_count == early.h->activation_count);
  assert(late.h->queue_stamp == early.h->queue_stamp);
  assert(late.h->scores.score == early.h->scores.score);
  for (Var v = 0; v < late.nvars; ++v)
    assert(late.h->links[v].stamp == early.h->links[v].stamp);
  late.h->stable = true;
  heuristics_update_target_phases(late); // the old compact population asserts
}

// A Factor extension survives every order of heuristics attachment ('A'),
// detachment ('D') and compaction of an eliminated, never-activated x0 ('C'):
// the compact import population ends as exactly the active variables in
// import order, and the target-phase update accepts it.
TEST(test_factor_extension_with_heuristics_and_compaction) {
  for (const char *ops : {"FCA", "ADFA", "ADCFA", "AFDCA"}) {
    const bool compact = std::strchr(ops, 'C');
    const Var first = compact ? 1 : 0; // x0 is absent from every grid clause
    Kernel k;
    kernel_reserve_vars(k, first + 5);
    for (Var v = first; v < k.nvars; ++v) {
      k.input_activation_order.push_back(v);
      kernel_record_import(k, v);
    }
    if (compact)
      kernel_record_import(k, 0); // input finalization records header-only IDs
    grid_2x3(k, first);
    std::optional<ScopedHeuristics> h;
    for (const char *op = ops; *op; ++op)
      switch (*op) {
      case 'A':
        h.emplace(k);
        assert(k.input_activation_order.empty());
        break;
      case 'D':
        h.reset();
        assert(!k.h);
        break;
      case 'F':
        assert(factor_apply(k) == ApplyResult::Ok);
        assert(k.first_extension_var == k.nvars - 1);
        break;
      case 'C':
        k.var_status.assign(k.nvars, VarStatus::Active);
        k.var_status[0] = VarStatus::Eliminated;
        --k.unassigned_active;
        assert(kernel_compact_variables(k));
        break;
      }
    assert(k.nvars == 6 && k.external_to_internal[first + 5] == 5);
    assert(k.compact_import_order == std::vector<Var>({0, 1, 2, 3, 4, 5}));
    assert(k.compact_import_position[5] == 5);
    assert(k.compact_import_order.size() == k.unassigned_active);
    k.h->stable = true;
    heuristics_update_target_phases(k);
  }
}

// Kissat's dense-watch scan currency: budget 4 pays 3 (first_factor), 2
// (quotient stack) and 2 (first remainder list), then declines the grid.
TEST(test_dense_scan_budget_boundary) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 5);
  grid_2x3(k, 0);
  k.work_budget = 4;
  assert(factor_apply(k) == ApplyResult::NoOp);
  assert(k.stats.factorings == 0);
  assert(k.stats.factor_ticks == 7);
}

// After a match Kissat charges one quotient occurrence before its next_factor
// budget checkpoint, then applies the best grid.
TEST(test_next_factor_budget_check_follows_occurrence) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 5);
  grid_2x3(k, 0);
  std::fill(k.factor_marks.begin(), k.factor_marks.end(), 0);
  k.factor_marks[make_lit(0, false)] = 1;
  k.work_budget = 18;
  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings == 1);
  assert(k.stats.factor_ticks == 23 &&
         "the cutoff follows the first re-entered quotient occurrence");
}

// Kissat new_quotient(next) marks next first: a clause already containing it is
// no remainder match.
TEST(test_next_factor_mark_precedes_match_scan) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 10);
  const Lit a = make_lit(0, false), b = make_lit(1, false);
  for (const auto &[p, q] :
       {std::pair{2u, 3u}, std::pair{4u, 5u}, std::pair{6u, 7u}}) {
    add_large(k, {a, make_lit(p, false), make_lit(q, false)});
    add_large(k, {b, make_lit(p, false), make_lit(q, false)});
  }
  const Lit x = make_lit(8, false), y = make_lit(9, false);
  add_large(k, {a, b, x});
  kernel_add_binary_clause(k, x, y, ++k.next_clause_id);

  std::fill(k.factor_marks.begin(), k.factor_marks.end(), 0);
  k.factor_marks[a] = 1;
  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings == 1);
  assert(k.stats.factor_ticks == 52 &&
         "an overlap row must not pay an impossible remainder-match scan");
}

// factoring.initial is the literal universe at entry: an earlier extension
// literal is a legal later factor.
TEST(test_prior_extension_literal_is_a_later_factor) {
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

// Kissat's Factor dense mode keeps the sparse binary watch order for quotient
// columns (no import-rank re-sort).
TEST(test_binary_quotients_preserve_sparse_watch_order) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 5);
  for (const Var v : {0u, 1u, 3u, 2u, 4u})
    kernel_record_import(k, v);
  grid_2x3(k, 0);
  assert(factor_apply(k) == ApplyResult::Ok);
  assert((binary_partners(k, make_lit(5, true)) ==
          std::vector<Lit>{make_lit(2, false), make_lit(3, false),
                           make_lit(4, false)}));
}

// Binaries created while dense follow the connected large occurrences: the
// later a/b grid emits its large columns before its new -x column.
TEST(test_dynamic_binary_follows_initial_large_occurrences) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 11);
  k.factor_composite = true;
  const Lit c = make_lit(0, false), d = make_lit(1, false);
  const Lit a = make_lit(2, false), b = make_lit(3, false);
  const Lit e = make_lit(4, false);

  for (const Lit factor : {c, d})
    for (const Lit remainder : {a, b, e})
      kernel_add_binary_clause(k, factor, remainder, ++k.next_clause_id);

  for (const auto &[u, v] :
       {std::pair{5u, 6u}, std::pair{7u, 8u}, std::pair{9u, 10u}}) {
    add_large(k, {a, make_lit(u, false), make_lit(v, false)});
    add_large(k, {b, make_lit(u, false), make_lit(v, false)});
  }

  // Make c the sole initial seed.  Clause additions from its factoring then
  // re-arm a and b for the second factoring in the same Factor application.
  std::fill(k.factor_marks.begin(), k.factor_marks.end(), 0);
  k.factor_marks[c] = 1;
  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings >= 2);

  const Lit nx = make_lit(11, true);
  const Lit ny = make_lit(12, true);
  assert(kernel_binary_live(k, ny, nx));
  const uint64_t binary_id = kernel_binary_id(k, ny, nx);
  uint32_t preceding_large_quotients = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (clause->size == 3 &&
        std::find(clause->begin(), clause->end(), ny) != clause->end() &&
        clause->id() < binary_id)
      preceding_large_quotients++;
    ref += clause_actual_words(clause);
  }
  assert(preceding_large_quotients == 3);
}

// Kissat Factor eligibility is per literal: opposite polarities can form the
// two rows of one grid.
TEST(test_opposite_polarities_can_share_a_grid) {
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

// No dimension ceiling (as in Kissat): a grid above 4,096 columns is reachable.
TEST(test_factoring_beyond_former_dimension_cap) {
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

// Kissat's null-stack dense path keeps root-satisfied binaries: root-true p
// stays a column.
TEST(test_satisfied_binary_remainder_is_retained) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 5);
  grid_2x3(k, 0);
  const Lit a = make_lit(0, false), b = make_lit(1, false);
  const Lit p = make_lit(2, false);
  kernel_assign_unit(k, p);

  assert(factor_apply(k) == ApplyResult::Ok);
  assert(k.stats.factorings == 1);
  assert(k.nvars == 6);
  assert(!has_binary(k, a, p) && !has_binary(k, b, p));
}

TEST(test_retired_literal_is_not_a_factor_candidate) {
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

TEST(test_assigned_large_remainder) {
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

// Kissat emits a large quotient in the last-factor clause's existing pose.
TEST(test_large_quotient_preserves_last_factor_pose) {
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

// Kissat's quotient stack is an occurrence multiset: equal large remainders
// form a 2x3 grid, each proof-addressable.
TEST(test_duplicate_large_remainder_occurrences) {
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
  assert(k.stats.factor_large_remainders > 0);
  assert(k.stats.factor_remainders_materialized > 0);
  assert(k.stats.factor_remainders_materialized +
             k.stats.factor_remainders_reused <=
         k.stats.factor_large_remainders);
}

// Kissat's dense transition flushes repeated binaries: copies make no grid.
TEST(test_duplicate_binary_occurrences_are_flushed) {
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

// factorcandrounds=2 plus Kissat's final connection check: C0, C1 and C2 drop
// in round one, round two and at connection.
TEST(test_candidate_reduction_final_connection_check) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 11);
  k.factor_size = 3;
  k.factor_cand_rounds = 2;
  const Lit u = make_lit(0, false), x = make_lit(1, false);
  const Lit y = make_lit(2, false), a = make_lit(3, false);
  const Lit z0 = make_lit(4, false), z1 = make_lit(5, false);
  const Lit z2 = make_lit(6, false);
  const Lit p0 = make_lit(7, false), p1 = make_lit(8, false);
  const Lit p2 = make_lit(9, false), pa = make_lit(10, false);

  add_large(k, {u, x, z0}); // round 1: unique `u` removes C0
  add_large(k, {x, y, z1}); // round 2: C0's loss removes C1 through `x`
  add_large(k, {y, a, z2}); // connection: C1's loss removes C2 through `y`
  kernel_add_binary_clause(k, z0, p0, ++k.next_clause_id);
  kernel_add_binary_clause(k, z1, p1, ++k.next_clause_id);
  kernel_add_binary_clause(k, z2, p2, ++k.next_clause_id);
  kernel_add_binary_clause(k, a, pa, ++k.next_clause_id);

  const int factor_id = tactix_rule_id("Factor");
  assert(factor_id >= 0);
  const RuleVTable &factor = tactix_rules()[factor_id];
  assert(factor.apply_instance(k, lit_instance(k, a)) == ApplyResult::NoOp);
  assert(k.stats.factor_large_remainders == 0 &&
         "the final peeled clause must not enter Factor's dense index");
}

// kissat_resume_sparse_mode rebuilds every large watch in arena order, even
// without a grid.
TEST(test_sparse_resume_rebuilds_large_watch_order) {
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

// Kissat keeps factoring.initial fixed: a same-pass extension literal is never
// a next factor.
TEST(test_fresh_large_quotients_do_not_become_factors) {
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

// Fresh extensions are demoted in creation order, newest least preferred
// (kissat factor.c:867-882, cadical factor.cpp:799-802).
TEST(test_empty_factor_restamps_only_when_advice_is_consumed) {
  for (const bool consume : {false, true}) {
    Kernel k;
    kernel_reserve_vars(k, 3);
    k.input_activation_order = {0, 1, 2};
    ScopedHeuristics h(k);
    kernel_add_binary_clause(k, internalize(1), internalize(2));
    kernel_add_binary_clause(k, internalize(2), internalize(3));
    heuristics_queue_bump(k, 0);
    k.h->search_idx = k.h->queue_first;
    k.h->search_stamp = k.h->links[k.h->queue_first].stamp;
    const auto stamp = k.h->queue_stamp;
    const auto cursor = k.h->search_idx;
    assert(factor_apply(k) == ApplyResult::NoOp);
    assert(k.nvars == 3 && k.stats.factorings == 0);
    assert(k.h->queue_stamp == stamp && k.h->search_idx == cursor);
    assert(k.fresh_var_events.size() == 1);
    assert(k.fresh_var_events[0].first == 3 && k.fresh_var_events[0].end == 3);
    k.factor_demote_fresh = consume;
    heuristics_consume_rule_advice(k);
    assert(k.fresh_var_events.empty());
    if (consume) {
      unsigned n = 0;
      for (Var v = k.h->queue_first; v != INVALID_VAR; v = k.h->links[v].next)
        assert(k.h->links[v].stamp == ++n);
      assert(k.h->queue_stamp == n && n == 3);
      assert(k.h->search_idx == k.h->queue_last);
      assert(k.h->search_stamp == n);
    } else {
      assert(k.h->queue_stamp == stamp && k.h->search_idx == cursor);
    }
    assert(k.stats.factor_demotions == 0 && k.stats.factor_demoted_vars == 0);
  }
}

TEST(test_demote_fresh_order) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 10); // two disjoint 2x3 grids over 0..4 and 5..9
  grid_2x3(k, 0);
  grid_2x3(k, 5);

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

// Kissat inserts a fresh variable with its cumulative score, then demotes it to
// zero; the transient sift shapes the heap.
TEST(test_demote_fresh_stable_heap_lifecycle) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  k.input_activation_order = {0, 1, 2, 3};
  ScopedHeuristics _hg_k(k);
  k.h->stable = true;
  heuristics_update_scores(k);

  kernel_reserve_vars(k, 5);
  assert(k.h->activation_count == 5);
  assert(k.h->scores.score[4] == 0.8);
  assert(k.h->scores.stack.front() == 4);

  k.fresh_var_events.push_back({4, 5});
  heuristics_consume_rule_advice(k);
  assert(k.h->scores.score[4] == 0.0);
  assert(k.h->scores.stack.front() == 3);
  assert((k.h->scores.stack == std::vector<Var>{3, 2, 1, 0, 4}));
}

TEST(test_demote_fresh_uses_recorded_end) {
  Kernel k;
  kernel_reserve_vars(k, 2);
  k.input_activation_order = {0, 1};
  ScopedHeuristics _hg_k(k);
  k.h->stable = true;
  heuristics_update_scores(k);

  kernel_reserve_vars(k, 3);
  k.fresh_var_events.push_back({2, 3});
  kernel_reserve_vars(k, 4); // unrelated later extension
  heuristics_consume_rule_advice(k);
  assert(k.h->scores.score[2] == 0.0);
  assert(k.h->scores.score[3] == 0.75);
  assert(k.stats.factor_demoted_vars == 1);
}

// The per-literal seed marks are policy and must not wedge: a watermark past
// everything suppresses the round, and the next binary add re-arms it.
TEST(test_touch_filter_is_policy_only) {
  Kernel k;
  ScopedHeuristics _hg_k(k);
  kernel_reserve_vars(k, 5);
  grid_2x3(k, 0);
  const Lit a = make_lit(0, false);
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

TEST(test_candidate_pruning_follows_reference_defaults) {
  Kernel k;
  assert(k.factor_cand_rounds == 2 &&
         "Factor candidate pruning follows both reference defaults");
}
