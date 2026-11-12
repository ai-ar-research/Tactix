// C2 Strengthen: fused one-action-per-candidate rounds, unit-producing
// strengthen, the instance surfaces, budgets, root flushes and interrupts.
#undef NDEBUG
#include "rule_test_utils.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/strengthen/strengthen.hpp"

#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdio>

using namespace tactix;

// Pinned occurrence limits (Kissat's subsumeocclim is 1000).
static constexpr uint32_t TEST_STRENGTHEN_OCCLIM =
    strategy::StrengthenParameters::OCCURRENCE_DEFAULT;
static_assert(TEST_STRENGTHEN_OCCLIM == 1000 &&
              strategy::SubsumeForwardParameters::BINARY_OCCURRENCE_DEFAULT ==
                  10000);

static void test_round_entry_deduplicates_binaries() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit a = internalize(1), b = internalize(2);
  kernel_add_binary_clause(k, a, internalize(3));
  kernel_add_binary_clause(k, a, b);
  kernel_add_binary_clause(k, a, b);
  kernel_add_binary_clause(k, a, b);
  assert(kernel_binary_copies(k, a, b) == 3);
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(kernel_binary_copies(k, a, b) == 1);
  assert(k.stats.binaries_deleted == 2);
  assert(strengthen_apply(k) == ApplyResult::NoOp);
  kernel_check_invariants(k);
}

static void test_round_entry_hyper_unary_binary_pair() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  const Lit x = internalize(1), a = internalize(2);
  kernel_add_binary_clause(k, x, a);
  kernel_add_binary_clause(k, x, negate(a));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.value(x) > 0 && k.assigned[var_of(x)].level == 0);
  assert(!k.inconsistent);
  kernel_check_invariants(k);
}

static void test_binary_strengthens_large() {
  // resolving c = (1 2 3) on 1 against (-1 2) leaves the binary (2 3)
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {1, 2, 3});
  const uint64_t arena_generation = k.arena_generation;
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  assert(strengthen_guard(k));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthened == 1);
  assert(k.stats.strengthen_subsumed == 0);
  assert(live_large(k) == 0); // (2 3) became a binary
  assert(k.arena.empty());
  assert(k.arena_generation == arena_generation + 1);
  assert(default_solve(k) == 10);
}

static void test_kissat_mixed_occurrence_limit_admits_binary_strengthener() {
  // Kissat's subsumeocclim (1000) bounds the whole mixed watch stream, so a
  // stream above CaDiCaL's 100-entry default is still scanned.
  Kernel k;
  kernel_reserve_vars(k, 110);
  add_external_clause(k, {1, 2, 3});
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  for (int var = 4; var <= 105; ++var)
    kernel_add_binary_clause(k, internalize(1), internalize(var));
  assert(k.watches.heads[internalize(1)].size > 100 &&
         k.watches.heads[internalize(1)].size <= TEST_STRENGTHEN_OCCLIM);
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthened == 1);
  assert(has_binary(k, internalize(1), internalize(3)));
}

static void test_binary_partner_uses_kissat_last_strengthener() {
  // Kissat carries `remove` across the scan: the later literal-3 walk
  // overwrites the literal-2 result and removes 1.
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {1, 2, 3});
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_add_binary_clause(k, internalize(2), internalize(-3));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(has_binary(k, internalize(2), internalize(3)));
  assert(!has_binary(k, internalize(1), internalize(2)));
}

static void test_opposite_phase_binary_does_not_override_later_result() {
  // (-1,2) removes 1, the later (-2,3) removes 2; Kissat does not rescan
  // opposite-phase binaries at literal 1, so the later result stands.
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {2, 3, 1});
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(has_binary(k, internalize(1), internalize(3)));
  assert(!has_binary(k, internalize(2), internalize(3)));
}

static void test_fused_subsumption() {
  // c = (1 2 3) subsumed by the binary (1 2): fused round deletes it
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {1, 2, 3});
  const uint64_t arena_generation = k.arena_generation;
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthen_subsumed == 1);
  assert(k.stats.strengthened == 0);
  assert(live_large(k) == 0);
  assert(k.arena.empty());
  assert(k.arena_generation == arena_generation + 1);
}

static void test_large_partner_without_same_round_cascade() {
  // Kissat checks each candidate once: (-1 2 3) becomes (-1 3), the size-4
  // candidate meets the older (-2 3) first and becomes (1 3 4), and the new
  // (-1 3) opportunity waits for the next round.
  Kernel k;
  kernel_reserve_vars(k, 4);
  add_external_clause(k, {1, 2, 3, 4});
  add_external_clause(k, {-1, 2, 3});
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthened == 2);
  live_large(k, lits_of({1, 3, 4}).data(), 3);
  assert(!k.strengthen_complete);
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthened == 3);
  assert(live_large(k) == 0); // second round produces binary (3 4)
  assert(default_solve(k) == 10);
}

static void test_equal_size_candidates_use_inherited_arena_position() {
  // An appended replacement inherits the shrunk record's position (Kissat
  // shrinks in place), so the source becomes (1,3) and subsumes the
  // equal-size victim (1,3,4) in the same pass.
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Ref original_ref =
      add_external_clause(k, {1, 2, 3, 5}, false, 0, ++k.next_clause_id);
  add_external_clause(k, {1, 3, 4}, false, 0, ++k.next_clause_id);
  Ref source_ref = INVALID_REF;
  assert(kernel_replace_clause(k, clause_at(k.arena, original_ref),
                               ++k.next_clause_id, lits_of({1, 2, 3}).data(), 3,
                               false, 0, &source_ref) == ReplaceOutcome::Large);
  assert(source_ref > original_ref);
  assert(kernel_clause_position(k, source_ref) == original_ref);
  kernel_add_binary_clause(k, internalize(1), internalize(-2),
                           ++k.next_clause_id);

  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthened == 1);
  assert(k.stats.strengthen_subsumed == 1);
  assert(kernel_binary_copies(k, internalize(1), internalize(3)) == 1);
  assert(live_large(k) == 0);
  kernel_check_invariants(k);
}

static void test_connection_uses_growing_forward_population() {
  Kernel k;
  kernel_reserve_vars(k, 12 * (TEST_STRENGTHEN_OCCLIM + 1) + 4);
  add_external_clause(k, {1, 2, 3});
  add_external_clause(k, {1, 2, 3, 4});
  int next = 5;
  for (int anchor = 1; anchor <= 3; ++anchor)
    for (uint32_t i = 0; i <= TEST_STRENGTHEN_OCCLIM; ++i, next += 4)
      add_external_clause(k, {anchor, next, next + 1, next + 2, next + 3});
  OccIndex dense;
  kernel_dense_connect(k, dense);
  // Kissat connects the partner against the growing forward index, before the
  // later fillers make its literals expensive (not the BVE population).
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.strengthen_subsumed == 1);
  assert(live_large(k) == 1 + 3 * (TEST_STRENGTHEN_OCCLIM + 1));
  kernel_dense_disconnect(k);
}

static void test_binary_watches_count_toward_connection_cap() {
  Kernel k;
  kernel_reserve_vars(k, 3 * (TEST_STRENGTHEN_OCCLIM + 1) + 4);
  add_external_clause(k, {1, 2, 3});
  add_external_clause(k, {1, 2, 3, 4});
  int next = 5;
  for (int anchor = 1; anchor <= 3; ++anchor)
    for (uint32_t i = 0; i <= TEST_STRENGTHEN_OCCLIM; ++i)
      kernel_add_binary_clause(k, internalize(anchor), internalize(next++));
  OccIndex dense;
  kernel_dense_connect(k, dense);
  // Kissat's SIZE_WATCHES population includes binaries: no partner literal is
  // eligible for the round-local subsumer index.
  assert(strengthen_apply(k) == ApplyResult::NoOp);
  assert(k.stats.strengthen_subsumed == 0);
  assert(live_large(k) == 2);
  kernel_dense_disconnect(k);
}

// Kissat schedules clauses with at least two armed variables, and
// connect_subsuming needs every active variable of the partner armed.
static void test_kissat_touch_candidate_and_connection_predicates() {
  for (int armed = 1; armed <= 3; ++armed) {
    Kernel k;
    kernel_reserve_vars(k, 4);
    add_external_clause(k, {1, 2, 3});
    add_external_clause(k, {1, 2, 3, 4});
    k.strengthen_touch_mark = k.touch_epoch;
    kernel_clear_subsume_material(k);
    for (int v = 1; v <= armed; ++v)
      kernel_mark_subsume_material(k, var_of(internalize(v)));
    assert(strengthen_apply(k) ==
           (armed == 3 ? ApplyResult::Ok : ApplyResult::NoOp));
    if (armed == 1)
      assert(k.stats.strengthen_checked == 0 &&
             k.stats.strengthen_mark_skipped == 2);
    if (armed == 2)
      assert(k.stats.strengthen_checked == 2 &&
             k.stats.strengthen_subsumed == 0);
    if (armed == 3)
      assert(k.stats.strengthen_subsumed == 1);
    assert(live_large(k) == (armed == 3 ? 1u : 2u));
  }
}

static void test_root_entry_unit_then_satisfied_deletion() {
  // Root propagation derives the unit first; discovery then retires the
  // satisfied clause.
  Kernel k;
  kernel_reserve_vars(k, 3);
  kernel_assign_unit(k, internalize(-1));
  kernel_assign_unit(k, internalize(-2));
  add_external_clause(k, {1, 2, 3});
  assert(propagate_apply(k) == ApplyResult::Ok); // quiesce the units
  assert(strengthen_guard(k));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.flushed == 1);
  assert(k.stats.strengthened == 0);
  assert(k.value(internalize(3)) > 0); // unit asserted at root
  assert(k.assigned[var_of(internalize(3))].level == 0);
  assert(live_large(k) == 0);
}

static void test_forward_step_budget_cuts_between_candidates() {
  Kernel k;
  kernel_reserve_vars(k, 12);
  add_external_clause(k, {1, 2, 3});
  add_external_clause(k, {9, 10, 11, 12});
  // c's first mixed stream costs a candidate step, a stream step and one
  // 128-byte cache line (3); a budget of one finishes c and stops before d.
  for (int v = 4; v <= 8; ++v)
    kernel_add_binary_clause(k, internalize(1), internalize(v));
  k.midround_budget = true;
  k.work_budget = 1;
  assert(strengthen_apply(k) == ApplyResult::NoOp);
  assert(k.stats.strengthen_checked == 1);
  assert(k.stats.strengthen_visits == 3);
  assert(k.stats.budget_stops_c2 == 1);
  assert(!k.strengthen_complete);
  assert(k.stats.strengthen_rearmed > 0);
  assert(live_large(k) == 2);

  k.midround_budget = false;
  k.work_budget = 0;
  assert(strengthen_apply(k) == ApplyResult::NoOp);
  assert(k.strengthen_complete);
  assert(k.stats.strengthen_checked == 2);
  kernel_check_invariants(k);
}

static void test_instance_surface() {
  // instances() lists candidates size-ascending; apply_instance does one step
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Ref cref = add_external_clause(k, {1, 2, 3, 4});
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  const std::vector<RuleVTable> &rules = tactix_rules();
  const int id = tactix_rule_id("Strengthen");
  assert(id >= 0);
  uint64_t insts[4];
  const uint32_t n = rules[id].instances(k, insts, 4);
  assert(n == 1 && insts[0] == cref);
  assert(rules[id].apply_instance(k, insts[0]) == ApplyResult::Ok);
  assert(k.stats.strengthened == 1);
  live_large(k, lits_of({2, 3, 4}).data(), 3); // the strengthened clause
  assert(rules[id].apply_instance(k, 999999) == ApplyResult::NoOp); // junk
}

static void test_semantic_outcomes_include_every_removed_literal() {
  const int id = tactix_rule_id("Strengthen");
  assert(id >= 0);
  const RuleVTable &rule = tactix_rules()[id];
  assert(rule.semantic_instances && rule.apply_semantic_instance);
  for (int removed = 1; removed <= 3; ++removed) {
    Kernel k;
    kernel_reserve_vars(k, 3);
    const Ref candidate = add_external_clause(k, {1, 2, 3});
    kernel_add_binary_clause(k, internalize(-1), internalize(2));
    kernel_add_binary_clause(k, internalize(1), internalize(-2));
    add_external_clause(k, {1, 2, -3}, true, 3);

    uint64_t instances[16];
    const uint32_t count = rule.semantic_instances(k, instances, 16);
    const uint64_t wanted = clause_literal_instance(
        candidate, internalize(removed));
    assert(std::find(instances, instances + count, wanted) !=
           instances + count);
    assert(rule.apply_semantic_instance(k, wanted) == ApplyResult::Ok);
    Lit first = INVALID_LIT, second = INVALID_LIT;
    for (int literal = 1; literal <= 3; ++literal) {
      if (literal == removed)
        continue;
      (first == INVALID_LIT ? first : second) = internalize(literal);
    }
    assert(kernel_binary_live(k, first, second));
    assert(k.stats.strengthened == 1);
    kernel_check_invariants(k);
  }
}

static void test_root_flush() {
  // Kissat's forward discovery retires a root-satisfied clause but keeps a
  // root-false literal of another clause for the next BVE round.
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_assign_unit(k, internalize(1));
  add_external_clause(k, {1, 2, 3}); // root-satisfied: dies
  add_external_clause(k, {-1, 2, 4}); // candidate, but unchanged
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(strengthen_guard(k));
  assert(strengthen_apply(k) == ApplyResult::Ok);
  assert(k.stats.flushed == 1);
  assert(k.stats.strengthened == 0); // attributed to flushed only
  assert(k.stats.strengthen_checked == 1);
  assert(live_large(k) == 1); // sat died; (-1 2 4) stays physically large
  assert(kernel_binary_copies(k, internalize(2), internalize(4)) == 0);
  assert(default_solve(k) == 10);
}

// C2 polls for interrupt at entry, in the candidate loop and on the
// single-instance surface, with no cadence; the loop site needs the stress
// cadence and `strengthen_checked > 0` witnesses it.
static void fill_interrupt_case(Kernel &k, int n) {
  kernel_reserve_vars(k, 3 * n + 4);
  for (int i = 0; i < n; i++)
    add_external_clause(k, {3 * i + 1, 3 * i + 2, 3 * i + 3});
}

// (1 2 3) and its subsumer (1 2); returns the large clause.
static Ref subsumed_case(Kernel &k) {
  kernel_reserve_vars(k, 3);
  const Ref ref = add_external_clause(k, {1, 2, 3});
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  return ref;
}

static void test_interrupt() {
  volatile sig_atomic_t stop = 1;
  { // (1) the round-entry poll
    Kernel k;
    subsumed_case(k);
    KernelStopPublisher pub(k, &stop);
    assert(strengthen_apply(k) == ApplyResult::NoOp);
    assert(k.stats.strengthen_subsumed == 0);
    assert(k.stats.strengthen_checked == 0); // never reached the candidates
    assert(k.stats.strengthen_interrupted == 1);
    assert(k.stats.rule_interrupted == 1);
    assert(!k.strengthen_complete);
    // An entry abort has no tail to re-arm and keeps the watermark.
    assert(k.strengthen_touch_mark == 0 &&
           k.stats.strengthen_mark_advances == 0);
    assert((k.subsume_material == std::vector<uint8_t>{1, 1, 1}));
    kernel_check_invariants(k);
  }
  { // (2) the single-instance entry
    Kernel k;
    const Ref cref = subsumed_case(k);
    KernelStopPublisher pub(k, &stop);
    assert(strengthen_try(k, cref) == ApplyResult::NoOp);
    assert(k.stats.strengthen_subsumed == 0);
    assert(k.stats.strengthen_interrupted == 1);
  }
  { // negative control for both: same formula, no interrupt, the clause dies
    Kernel k;
    subsumed_case(k);
    assert(strengthen_apply(k) == ApplyResult::Ok);
    assert(k.stats.strengthen_subsumed == 1 &&
           k.stats.strengthen_interrupted == 0);
    Kernel t;
    const Ref cref = subsumed_case(t);
    assert(strengthen_try(t, cref) == ApplyResult::Ok);
    assert(t.stats.strengthen_subsumed == 1 &&
           t.stats.strengthen_interrupted == 0);
  }
  { // (3) inside the candidate loop: sweeping the deterministic 1-in-m
    // stress cadence walks the first fire past the flush pass.
    bool past_entry = false;
    for (uint32_t m = 100; m <= 6000 && !past_entry; m += 100) {
      Kernel k;
      fill_interrupt_case(k, 200);
      const auto material = k.subsume_material;
      const uint64_t material_count = k.subsume_material_count;
      k.stop_fuzz = m;
      strengthen_apply(k);
      past_entry = k.stats.strengthen_interrupted >= 1 &&
                   k.stats.strengthen_checked > 0;
      if (past_entry) {
        assert(!k.strengthen_complete);
        assert(k.strengthen_touch_mark == 0);
        assert(k.subsume_material == material);
        assert(k.subsume_material_count == material_count);
        kernel_check_invariants(k);
      }
    }
    assert(past_entry);
    Kernel c; // negative control: same formula, no knob, round completes
    fill_interrupt_case(c, 200);
    assert(strengthen_apply(c) == ApplyResult::NoOp);
    assert(c.stats.strengthen_interrupted == 0 && c.strengthen_complete);
  }
}

// Kissat's generic root flush (collect.c) drops a root-false literal without
// touching glue; only OTFS, Sweep and Vivify shrinks clamp to size-1.
static void test_root_flush_preserves_glue() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  add_external_clause(k, {1, 2, 3, 4}, true, 4);
  kernel_assign_unit(k, negate(internalize(4)));
  assert(propagate_apply(k) == ApplyResult::Ok);
  Ref ref = 0;
  uint64_t changed = 0;
  assert(kernel_flush_root_clause(k, &ref, &changed) ==
         RootFlushOutcome::Rewritten);
  bool found = false;
  for (Ref scan = 0; scan < k.arena.size();) {
    const Clause *cl = clause_at(k.arena, scan);
    if (!cl->garbage() && cl->redundant()) {
      found = true;
      assert(cl->size == 3);
      assert(cl->glue() == 4); // preserved, not clamped to size-1
    }
    scan += clause_actual_words(cl);
  }
  assert(found);
  kernel_check_invariants(k);
}

int main() {
  test_root_flush_preserves_glue();
  test_round_entry_deduplicates_binaries();
  test_round_entry_hyper_unary_binary_pair();
  test_binary_strengthens_large();
  test_kissat_mixed_occurrence_limit_admits_binary_strengthener();
  test_binary_partner_uses_kissat_last_strengthener();
  test_opposite_phase_binary_does_not_override_later_result();
  test_fused_subsumption();
  test_large_partner_without_same_round_cascade();
  test_equal_size_candidates_use_inherited_arena_position();
  test_connection_uses_growing_forward_population();
  test_binary_watches_count_toward_connection_cap();
  test_kissat_touch_candidate_and_connection_predicates();
  test_root_entry_unit_then_satisfied_deletion();
  test_forward_step_budget_cuts_between_candidates();
  test_instance_surface();
  test_semantic_outcomes_include_every_removed_literal();
  test_root_flush();
  test_interrupt();
  puts("test_strengthen OK");
  return 0;
}
