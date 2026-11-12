// Sweep's resumable schedule: retained admission is scheduling advice, never a
// public Sweep precondition; small environments still own the initial solve,
// model and exact effort under every enclosing budget.
#include "unit_test.hpp"
#include "kernel/kernel.hpp"
#include "rule_test_utils.hpp"
#include "rules/substitute/substitute.hpp"
#include "rules/sweep/sweep.hpp"

static void binary(Kernel &k, int a, int b) {
  kernel_add_binary_clause(k, internalize(a), internalize(b),
                           ++k.next_clause_id);
}

static void build(Kernel &k, uint32_t nvars,
                  const std::vector<std::vector<int>> &binaries) {
  kernel_reserve_vars(k, nvars);
  for (const auto &c : binaries)
    binary(k, c[0], c[1]);
}

// Next seed of the resumable schedule (reschedule front before the tail).
static Var next_scheduled(const Kernel &k) {
  for (size_t j = k.sweep_front.size(); j-- > 0;)
    if (k.sweep_front[j] != INVALID_VAR)
      return k.sweep_front[j];
  for (size_t i = k.sweep_tail_next; i < k.sweep_tail.size(); i++)
    if (k.sweep_tail[i] != INVALID_VAR)
      return k.sweep_tail[i];
  return INVALID_VAR;
}

static SweepLimits bounded() {
  SweepLimits lim;
  lim.max_seeds = 1;
  lim.env_ticks = 1;
  return lim;
}

static void mark_retained(Kernel &k) {
  k.sweep_incomplete = true;
  k.sweep_marked.resize(k.external_to_internal.size(), 0);
  for (const Var external : k.sweep_tail)
    if (external < k.sweep_marked.size())
      k.sweep_marked[external] = 1;
  for (const Var external : k.sweep_front)
    if (external < k.sweep_marked.size())
      k.sweep_marked[external] = 1;
}

TEST(pure_seed) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  binary(k, 1, 6);
  binary(k, 2, 7);
  binary(k, -2, 8); // A fresh eligible seed must not restart this call.
  k.sweep_tail = {0};
  mark_retained(k);
  assert(sweep_apply_with_limits(k, bounded()) == ApplyResult::NoOp);
  assert(k.stats.sweep_swept == 0 && k.stats.sweep_resumed == 0);
  assert(k.stats.sweep_scheduled == 0);
  assert(k.sweep_live == 0 && k.sweep_slot.empty());
  assert(k.sweep_passes_completed == 2 && k.stats.sweep_completed == 2);
  std::vector<Var> seeds;
  sweep_seeds(k, seeds);
  assert(std::find(seeds.begin(), seeds.end(), 0) != seeds.end());
  const auto before = k.stats.sweep_swept;
  (void)sweep_try(k, 0);
  assert(k.stats.sweep_swept == before + 1); // N1 still exposes pure seeds.
}

static void completed_tail_admits_fresh(bool filtered) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  binary(k, 1, 6);
  if (!filtered)
    binary(k, -1, 7);
  binary(k, 2, 7);
  binary(k, -2, 8);
  // The saved representative was already swept in the preceding pass. Its
  // unmarked tail is not evidence that the pass remains incomplete.
  k.sweep_tail = {0};
  (void)sweep_apply_with_limits(k, bounded());
  assert(k.stats.sweep_scheduled == 1);
  assert(k.stats.sweep_swept == 1);
  if (filtered) {
    assert(k.stats.sweep_resumed == 0);
    assert(k.sweep_passes_completed == 1 && !k.sweep_incomplete);
  } else {
    assert(k.stats.sweep_resumed == 1);
    assert(k.sweep_live == 1 && next_scheduled(k) == 1);
    assert(k.sweep_passes_completed == 0 && k.sweep_incomplete);
    assert(k.sweep_marked[1] == 1);
  }
}

TEST(completed_pass_keeps_unmarked_representative) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  binary(k, 1, 2);
  binary(k, -1, 3);
  binary(k, -2, 4);
  binary(k, -3, -4);
  k.sweep_tail = {0};
  k.sweep_incomplete = true;
  k.sweep_marked.resize(k.external_to_internal.size(), 0);
  k.sweep_marked[0] = 1;
  SweepLimits lim;
  lim.max_seeds = 1;
  (void)sweep_apply_with_limits(k, lim);
  assert(k.stats.sweep_rescheduled > 0);
  assert(k.sweep_live > 0 && next_scheduled(k) == 0);
  assert(!k.sweep_incomplete && k.sweep_passes_completed == 1);
  assert(k.sweep_marked[0] == 0);
  const uint64_t scheduled = k.stats.sweep_scheduled;
  (void)sweep_apply_with_limits(k, lim);
  assert(k.stats.sweep_scheduled > scheduled);
}

TEST(front_tail_order) {
  Kernel k;
  kernel_reserve_vars(k, 16);
  binary(k, 1, 12); // pure front
  binary(k, -3, 13); // pure tail
  for (int seed : {2, 4, 5}) {
    binary(k, seed, 14);
    binary(k, -seed, 15);
  }
  k.sweep_tail = {INVALID_VAR, 2, 3, 4};
  k.sweep_tail_next = 1;
  k.sweep_front = {1, INVALID_VAR, 0};
  k.sweep_slot.resize(16, 0);
  k.sweep_slot[1] = (1u << 31) + 1;
  k.sweep_slot[0] = (1u << 31) + 3;
  k.sweep_live = 2;
  mark_retained(k);
  (void)sweep_apply_with_limits(k, bounded());
  assert(k.stats.sweep_swept == 1 && k.stats.sweep_resumed == 1);
  assert(k.sweep_front.empty());
  assert(k.sweep_tail[1] == INVALID_VAR);
  assert(k.sweep_slot[0] == 0 && k.sweep_slot[1] == 0 &&
         k.sweep_slot[2] == 0);
  assert(k.sweep_live == 2 && next_scheduled(k) == 3);
  assert(k.sweep_slot[3] == 3 && k.sweep_slot[4] == 4);
  (void)sweep_apply_with_limits(k, bounded());
  assert(k.sweep_live == 1 && next_scheduled(k) == 4);
  assert(k.sweep_slot[3] == 0 && k.sweep_slot[4] == 4);
}

TEST(occurrence_boundary) {
  for (bool negative : {false, true})
    for (uint32_t count : {1u, 2u, 3u})
      for (uint32_t completed : {0u, 1u}) {
        Kernel k;
        kernel_reserve_vars(k, 16);
        binary(k, negative ? 1 : -1, 12);
        for (uint32_t j = 0; j < count; j++)
          binary(k, negative ? -1 : 1, 8 + j);
        k.sweep_tail = {0};
        mark_retained(k);
        k.sweep_passes_completed = completed;
        auto lim = bounded();
        lim.max_env_clauses = 2;
        (void)sweep_apply_with_limits(k, lim);
        const uint32_t cap = 2u << completed;
        assert(k.stats.sweep_limit_clauses == cap);
        assert(k.stats.sweep_swept == (count <= cap ? 1u : 0u));
      }
}

TEST(invalid_and_duplicate_entries) {
  Kernel k;
  kernel_reserve_vars(k, 8);
  binary(k, 1, 7);
  // All pure duplicates and unresolved external identities must drain once.
  k.sweep_tail = {0, INVALID_VAR, 0, 99};
  mark_retained(k);
  (void)sweep_apply_with_limits(k, bounded());
  assert(k.stats.sweep_swept == 0 && k.sweep_live == 0);
  assert(k.sweep_slot.empty() && k.sweep_passes_completed == 2);
}

TEST(entry_not_pop) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  binary(k, 1, 2); binary(k, -1, 3);
  binary(k, -2, 4); binary(k, -3, -4);
  OccIndex occ;
  kernel_dense_connect(k, occ);
  k.sweep_tail = {0};
  mark_retained(k);
  SweepLimits lim;
  lim.max_seeds = 2;
  lim.max_env_clauses = 4;
  (void)sweep_apply_with_limits(k, lim);
  // The first seed proves the cycle equivalent and moves its representative
  // to the front. Its new bridge occurrences exceed the entry cap, but this
  // same-call representative must not be rejected by a second admission test.
  assert(k.stats.sweep_rescheduled > 0 && k.stats.sweep_swept == 2);
  assert(!kernel_var_retired(k, 0));
  assert(k.watches.size(internalize(1)) > lim.max_env_clauses &&
         k.watches.size(internalize(-1)) > lim.max_env_clauses);
  kernel_dense_disconnect(k);
}

TEST(fixed_and_retired) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  k.unit_ids[0] = ++k.next_clause_id;
  kernel_assign_unit(k, internalize(1));
  k.var_status.resize(k.nvars, VarStatus::Active);
  k.var_status[1] = VarStatus::Eliminated;
  k.external_to_internal[2] = INVALID_VAR;
  k.sweep_tail = {0, 1, 2};
  mark_retained(k);
  (void)sweep_apply_with_limits(k, bounded());
  assert(k.stats.sweep_swept == 0 && k.sweep_live == 0);
  assert(k.sweep_slot.empty() && k.stats.sweep_completed == 2);
}

TEST(large_occurrences) {
  for (bool dense : {false, true})
    for (uint32_t count : {1u, 2u}) {
      Kernel k;
      kernel_reserve_vars(k, 12);
      for (bool neg : {false, true})
        for (uint32_t j = 0; j < count; j++) {
          const Lit seed = make_lit(0, neg);
          const Lit a = make_lit(2 + 2 * j, false);
          const Lit b = make_lit(3 + 2 * j, false);
          const Lit lits[] = {dense ? a : seed, b, dense ? seed : a};
          kernel_add_large_clause(k, lits, 3, false, 0, ++k.next_clause_id);
        }
      OccIndex occ;
      if (dense)
        kernel_dense_connect(k, occ);
      k.sweep_tail = {0};
      mark_retained(k);
      auto lim = bounded();
      // Existing fresh admission uses raw sparse watch words (two per large
      // watch), versus one occurrence in the connected dense representation.
      // Retained admission must use the same predicate, not invent another.
      lim.max_env_clauses = dense ? 1 : 2;
      (void)sweep_apply_with_limits(k, lim);
      assert(k.stats.sweep_swept == (count == 1 ? 1u : 0u));
      if (dense)
        kernel_dense_disconnect(k);
    }
}

static void clause(Kernel &k, const std::vector<Lit> &lits) {
  const uint64_t id = ++k.next_clause_id;
  if (lits.size() == 2)
    kernel_add_binary_clause(k, lits[0], lits[1], id);
  else
    kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
}

static void positive_clause(Kernel &k, uint32_t size) {
  kernel_reserve_vars(k, size + 2);
  std::vector<Lit> lits;
  for (Var v = 0; v < size; v++)
    lits.push_back(make_lit(v, false));
  clause(k, lits);
}

static uint64_t single_clause(uint32_t size, uint64_t budget = 0) {
  Kernel k;
  positive_clause(k, size);
  SweepLimits lim;
  lim.env_ticks = budget;
  SweepRound round;
  sweep_round_begin(k, round);
  const auto next_id = k.next_clause_id;
  assert(sweep_detect_seed(k, 0, lim, round) == 0);
  assert(round.env.vars.size() == size && round.env.nclauses() == 1);
  assert(k.stats.sweep_environments == 1);
  assert(k.stats.sweep_solves >= 1 && k.stats.sweep_ticks_sub > 0);
  assert(k.stats.sweep_ticks_sub == round.env.solver.kitten_ticks());
  if (!budget) {
    assert(round.env.solver.has_model());
    assert(std::any_of(round.env.solver.model().begin(),
                       round.env.solver.model().begin() + size,
                       [](int8_t value) { return value > 0; }));
  }
  assert(round.found.empty() && round.proofs.empty() && round.units.empty());
  assert(!k.inconsistent && k.trail.empty() && k.next_clause_id == next_id);
  return k.stats.sweep_ticks_sub;
}

TEST(empty_and_singleton) {
  Kernel k;
  kernel_reserve_vars(k, 1);
  SweepRound round;
  SweepLimits lim;
  sweep_round_begin(k, round);
  assert(sweep_detect_seed(k, 0, lim, round) == 0);
  assert(round.env.vars.size() == 1 && round.env.nclauses() == 0);
  assert(k.stats.sweep_environments == 1 && k.stats.sweep_solves >= 1);
  assert(round.found.empty() && round.units.empty() && !k.inconsistent);
  assert(k.trail.empty());
  // Empty kernel and invalid public identity still decline before detection.
  Kernel empty;
  assert(sweep_try(empty, 0) == ApplyResult::NoOp);
  assert(empty.stats.sweep_solves == 0);
}

TEST(root_and_retired_still_decline) {
  Kernel k;
  kernel_reserve_vars(k, 2);
  k.unit_ids[0] = ++k.next_clause_id;
  kernel_assign_unit(k, make_lit(0, false));
  k.var_status.resize(k.nvars, VarStatus::Active);
  k.var_status[1] = VarStatus::Eliminated;
  SweepRound round;
  SweepLimits lim;
  sweep_round_begin(k, round);
  assert(sweep_detect_seed(k, 0, lim, round) == 0);
  assert(sweep_detect_seed(k, 1, lim, round) == 0);
  assert(k.stats.sweep_solves == 0 && k.stats.sweep_environments == 0);
}

TEST(enclosing_budget) {
  Kernel k;
  positive_clause(k, 270);
  clause(k, {make_lit(0, true), make_lit(270, false)});
  // All three seeds have two polarities at entry. The large first clause
  // crosses the variable cap before either negative binary is imported.
  clause(k, {make_lit(1, true), make_lit(271, false)});
  clause(k, {make_lit(2, true), make_lit(270, false)});
  OccIndex occ;
  kernel_dense_connect(k, occ); // Include the third, unwatched large occurrence.
  k.sweep_tail = {0, 1, 2};
  mark_retained(k);
  SweepLimits lim;
  lim.round_ticks = 1;
  lim.env_ticks = 0;
  (void)sweep_apply_with_limits(k, lim);
  // Import costs one real Kitten tick; the inclusive inner poll then returns
  // Budget. Equality still admits the second seed under the strict outer
  // limit, but two charged attempts leave the third retained.
  assert(k.stats.sweep_swept == 2 && k.stats.sweep_solves == 2);
  assert(k.stats.sweep_ticks_sub == 2 && k.stats.sweep_budget == 2);
  assert(k.sweep_live == 1 && next_scheduled(k) == 2);
  assert(!k.inconsistent && k.trail.empty());
  kernel_dense_disconnect(k);
}

TEST(repeated_session) {
  Kernel k;
  positive_clause(k, 270);
  SweepRound round;
  SweepLimits lim;
  sweep_round_begin(k, round);
  for (uint32_t call = 0; call < 3; call++) {
    const auto before = k.stats.sweep_ticks_sub;
    assert(sweep_detect_seed(k, 0, lim, round) == 0);
    assert(round.env.nclauses() == 1 && round.env.solver.has_model());
    assert(k.stats.sweep_ticks_sub - before == round.env.solver.kitten_ticks());
    assert(k.stats.sweep_environments == call + 1);
    assert(round.found.empty() && round.units.empty());
  }
}

TEST(selected_seed_budget) {
  for (bool selected : {false, true})
    for (bool bounded : {false, true}) {
      Kernel k;
      positive_clause(k, 3);
      // Keep a canonical seed eligible in the unbounded comparison.
      clause(k, {make_lit(0, true), make_lit(1, false)});
      k.work_budget = 0;
      k.work_budget_bounded = bounded;
      if (selected) (void)sweep_try(k, 0);
      else (void)sweep_apply(k);
      assert((k.stats.sweep_solves == 0) == bounded);
      assert(!k.inconsistent && k.trail.empty());
    }
  for (unsigned mode = 0; mode < 4; ++mode) {
    Kernel k;
    positive_clause(k, 3);
    SweepLimits lim;
    // Both the explicit round limit and the enclosing public rule limit
    // must reach the selected seed's embedded solver. Keep a tighter
    // environment limit when both are supplied.
    if (mode == 0) lim.round_ticks = 1;
    if (mode == 1) {
      k.work_budget = 1;
      k.work_budget_bounded = true;
    }
    if (mode == 2) { lim.round_ticks = 8; lim.env_ticks = 1; }
    if (mode == 3) { lim.round_ticks = 1; lim.env_ticks = 8; }
    assert(sweep_try_with_limits(k, 0, lim) == ApplyResult::NoOp);
    assert(k.stats.sweep_solves == 1 && k.stats.sweep_budget == 1);
    assert(k.stats.sweep_ticks_sub == 1);
    assert(!k.inconsistent && k.trail.empty());
  }
}

TEST(test_resumable_schedule) {
  const std::vector<std::vector<int>> cls = {
      {1, 2}, {-1, 3}, {-2, 4}, {-3, -4}};
  SweepLimits one_seed;
  one_seed.max_seeds = 1;

  Kernel off;
  build(off, 4, cls);
  SweepLimits off_limits = one_seed;
  off_limits.resume = false;
  sweep_apply_with_limits(off, off_limits);
  sweep_apply_with_limits(off, off_limits);
  assert(off.sweep_tail.empty());
  assert(off.stats.sweep_resumed == 0 && off.stats.sweep_completed == 0);

  Kernel filtered;
  build(filtered, 3, {{1, 2}, {-1, 2}});
  sweep_apply_with_limits(filtered, one_seed);
  assert(filtered.stats.sweep_scheduled == 1);
  assert(filtered.stats.sweep_completed == 1);
  std::vector<Var> complete_surface;
  sweep_seeds(filtered, complete_surface);
  assert(filtered.stats.sweep_units == 1);
  assert(complete_surface.size() == 2); // only the newly fixed var leaves scope

  Kernel growth;
  build(growth, 3, {{1, 2}, {1, 3}, {-1, 2}, {-1, 3}});
  SweepLimits growth_limits = one_seed;
  growth_limits.max_env_clauses = 1;
  sweep_apply_with_limits(growth, growth_limits);
  assert(growth.stats.sweep_scheduled == 0 &&
         growth.stats.sweep_completed == 1);
  sweep_apply_with_limits(growth, growth_limits);
  assert(growth.stats.sweep_limit_clauses == 2);
  assert(growth.stats.sweep_scheduled == 1);
  Kernel on;
  build(on, 4, cls);
  sweep_apply_with_limits(on, one_seed);
  // Productive seeds move surviving representatives to the pop end, as
  // Kissat's schedule_inner does.
  assert(on.sweep_tail.size() == 4 && on.sweep_tail_next == 1 &&
         on.sweep_live >= 3);
  assert(on.stats.sweep_scheduled == 4 && on.stats.sweep_resumed == 0);
  (void)substitute_apply(on); // mandatory post-Sweep phase owner
  const uint32_t live_before = on.sweep_live;
  // Entry revalidation drains the isolated representative left by Substitute.
  assert(!kernel_var_retired(on, 0));
  assert(on.watches.size(make_lit(0, false)) == 0 &&
         on.watches.size(make_lit(0, true)) == 0);
  for (Var v = 1; v < 4; v++)
    assert(kernel_var_retired(on, v));
  sweep_apply_with_limits(on, one_seed);
  assert(on.sweep_live < live_before && on.sweep_live == 0 &&
         on.stats.sweep_resumed == 0);
  (void)substitute_apply(on);
  for (uint32_t calls = 0; !on.sweep_passes_completed && calls < 16; calls++) {
    sweep_apply_with_limits(on, one_seed);
    (void)substitute_apply(on);
  }
  assert(on.sweep_tail.empty() && on.sweep_tail_next == 0);
  assert(on.sweep_passes_completed == 2 && on.stats.sweep_completed == 2);
  assert(on.stats.sweep_limit_vars == 256 &&
         on.stats.sweep_limit_clauses == 1024 &&
         on.stats.sweep_limit_depth == 2);

  sweep_apply_with_limits(on, one_seed);
  assert(on.stats.sweep_limit_vars == 1024 &&
         on.stats.sweep_limit_clauses == 4096 &&
         on.stats.sweep_limit_depth == 3);
}

// Kissat's `sweep_equivalence_candidates` reschedules the pair's
// representative, the smaller internal (import-order) index; both import
// orders are exercised.
TEST(test_reschedules_pair_representative_by_import_order) {
  SweepLimits limits;
  limits.max_seeds = 1;
  for (uint32_t flipped = 0; flipped < 2; flipped++) {
    Kernel k;
    kernel_reserve_vars(k, 3);
    kernel_record_import(k, flipped ? 1 : 0);
    kernel_record_import(k, flipped ? 0 : 1);
    kernel_record_import(k, 2);
    kernel_add_binary_clause(k, internalize(-1), internalize(2),
                             ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(-2), internalize(1),
                             ++k.next_clause_id);
    assert(sweep_apply_with_limits(k, limits) == ApplyResult::Ok);

    const Var representative = flipped ? 1u : 0u;
    assert(!kernel_var_retired(k, representative));
    assert(!kernel_var_retired(k, flipped ? 0u : 1u));
    assert(k.stats.sweep_rescheduled == 1);
    assert(next_scheduled(k) == k.internal_to_external[representative]);
    assert(substitute_apply(k) == ApplyResult::Ok);
    assert(kernel_var_retired(k, flipped ? 0u : 1u));
  }
}

// The chain x1 = x2 = x3 from one environment: Kissat's `schedule_inner`
// declines the intermediate representative x2, retired by the same batch.
TEST(test_reschedule_declines_a_retired_representative) {
  SweepLimits limits;
  limits.max_seeds = 1;
  Kernel k;
  kernel_reserve_vars(k, 5);
  // Four ternaries state the equivalence without a binary to propagate.
  const auto equate = [&](int left, int right, int helper) {
    for (uint32_t mask = 0; mask < 4; mask++) {
      const int sign = (mask & 1) ? 1 : -1;
      const int h = (mask & 2) ? helper : -helper;
      add_external_clause(k, {-sign * left, sign * right, h}, false, 0,
                          ++k.next_clause_id);
    }
  };
  equate(1, 2, 4);
  equate(2, 3, 5);

  assert(sweep_apply_with_limits(k, limits) == ApplyResult::Ok);
  const Var minimum = var_of(internalize(1));
  const Var intermediate = var_of(internalize(2));
  assert(k.stats.sweep_equivalences == 2);
  assert(!kernel_var_retired(k, minimum));
  assert(!kernel_var_retired(k, intermediate));
  assert(!kernel_var_retired(k, var_of(internalize(3))));
  assert(k.stats.sweep_rescheduled == 1);
  // The retired identity does not take the pop end.
  assert(next_scheduled(k) == k.internal_to_external[minimum]);
  assert(substitute_apply(k) == ApplyResult::Ok);
  assert(kernel_var_retired(k, intermediate));
  assert(kernel_var_retired(k, var_of(internalize(3))));
}

// Kissat propagates a productive environment's units before building the
// next seed, so variables they fix never become later environments.
TEST(test_resumable_closes_root_between_environments) {
  Kernel k;
  build(k, 6,
        {{1, 2}, {-1, 2}, {-2, 3}, {-3, 4}, {-4, 5}, {-5, 6}});
  SweepLimits limits;
  limits.max_depth = 0;
  assert(sweep_apply_with_limits(k, limits) == ApplyResult::Ok);
  for (Var v = 1; v < 6; v++)
    assert(kernel_root_value(k, make_lit(v, false)) > 0);
  assert(k.stats.sweep_environments == 1 &&
         "root consequences must close before the next environment");
}

int main() {
  run_tests();
  completed_tail_admits_fresh(false);
  completed_tail_admits_fresh(true);
  for (uint32_t size : {2u, 3u, 17u, 270u}) {
    (void)single_clause(size);
    (void)single_clause(size, 1);
  }
  std::puts("Sweep resumable schedule and small environments: PASS");
}
