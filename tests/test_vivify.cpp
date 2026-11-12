// Vivify unit tests: the probe outcomes and terminal arms, root-satisfied
// deletion, tier scope, Kissat's schedule order, and exact state restoration.
#undef NDEBUG
#include "rule_test_utils.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/vivify/vivify.hpp"
#include "strategy/vivify_parameters.hpp"

#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <vector>

using namespace tactix;
using strategy::VivifyParameters;
using strategy::vivify_skip_global_order;

static void test_implied_literal_strengthens() {
  // assuming ¬1 propagates 2, so C distills to a fresh binary (1 2)
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {1, 2, 3});
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  assert(vivify_guard(k));
  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(k.stats.vivify_subsumed == 1);
  assert(live_large(k) == 0);
  assert(k.level == 0 && !k.conflict_valid); // state restored
  assert(default_solve(k) == 10);
}

// Reason jumping records the virtual resolvent (1 v 2), which subsumes; the
// resolved-away 4 only appears in the proof chain.
static void test_jumped_binary_reason_subsumes_candidate() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  k.jump_reasons_bigbig = true;
  add_external_clause(k, {1, 2, 3});
  kernel_add_binary_clause(k, internalize(1), internalize(4));
  kernel_add_binary_clause(k, internalize(-4), internalize(2));

  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(k.stats.vivify_subsumed == 1);
  assert(k.stats.vivify_shrunken == 0 && k.stats.vivified == 0);
  assert(live_large(k) == 0 && k.level == 0 && !k.conflict_valid);
  assert(default_solve(k) == 10);
}

static void test_implied_negation_removes() {
  // assuming ¬1 propagates ¬2, so literal 2 is redundant in C
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Ref candidate = add_external_clause(k, {1, 2, 3});
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  // The instance surface probes this clause pose (the round's Kissat order
  // would differ), isolating the implied-negation arm.
  assert(vivify_try(k, candidate) == ApplyResult::Ok);
  assert(k.stats.vivified == 1 && k.stats.vivify_shrunken == 1);
  assert(live_large(k) == 0); // (1 3) became a binary
  assert(default_solve(k) == 10);
}

static void test_probe_conflict_strengthens() {
  // assuming ¬1 and ¬2 conflicts, so the RUP clause (1 2) replaces C
  Kernel k;
  kernel_reserve_vars(k, 4);
  add_external_clause(k, {1, 2, 3});
  kernel_add_binary_clause(k, internalize(1), internalize(4));
  kernel_add_binary_clause(k, internalize(2), internalize(-4));
  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(k.stats.vivified == 1 && k.stats.vivify_shrunken == 1);
  assert(live_large(k) == 0);
  assert(k.level == 0 && !k.conflict_valid);
  assert(default_solve(k) == 10);
}

// One focused witness for each terminal arm in kissat vivify.c:1124-1200.
// All use the instance surface so candidate scheduling cannot hide an arm.
static void test_terminal_dispatch() {
  { // irredundant candidate, redundant large subsumer: promote then delete
    Kernel k;
    kernel_reserve_vars(k, 4);
    const Ref candidate = add_external_clause(k, {1, 2, 3});
    add_external_clause(k, {1, 2, 3}, true, 2);
    const uint64_t marks_before = k.stats.bve_marks;
    kernel_clear_subsume_material(k);
    const uint64_t material_before = k.subsume_material_count;
    assert(vivify_try(k, candidate) == ApplyResult::Ok);
    assert(k.stats.vivify_subsumed == 1 && k.stats.vivify_promoted == 1);
    // Deleting the candidate marks its three literals; promoting the subsumer
    // adds no second set (Kissat's `mark_added_literals`).
    assert(k.stats.bve_marks - marks_before == 3);
    assert(k.subsume_material_count == material_before + 3);
    for (Var v = 0; v < 4; ++v)
      assert(kernel_has_subsume_material(k, v) == (v < 3));
    bool saw_irredundant = false;
    for (Ref r = 0; r < k.arena.size();) {
      const Clause *c = clause_at(k.arena, r);
      saw_irredundant |= !c->garbage() && !c->redundant();
      r += clause_actual_words(c);
    }
    assert(saw_irredundant && default_solve(k) == 10);
  }
  { // no strict shrink: a redundant independently-implied candidate dies
    Kernel k;
    k.vivify_reuse = false;
    kernel_reserve_vars(k, 4);
    const Ref candidate = add_external_clause(k, {1, 2, 3}, true, 2);
    kernel_add_binary_clause(k, internalize(1), internalize(-4));
    add_external_clause(k, {2, 3, 4});
    assert(vivify_try(k, candidate) == ApplyResult::Ok);
    assert(k.stats.vivify_implied == 1 && default_solve(k) == 10);
  }
  { // the same implication graph without redundant reasons is asymmetric
    Kernel k;
    k.vivify_reuse = false;
    kernel_reserve_vars(k, 4);
    const Ref candidate = add_external_clause(k, {1, 2, 3});
    kernel_add_binary_clause(k, internalize(1), internalize(-4));
    add_external_clause(k, {2, 3, 4});
    assert(vivify_try(k, candidate) == ApplyResult::Ok);
    assert(k.stats.vivify_asymmetric == 1 && default_solve(k) == 10);
  }
  { // a redundant antecedent forbids the asymmetric deletion: explicit no-op
    Kernel k;
    k.vivify_reuse = false;
    kernel_reserve_vars(k, 4);
    const Ref candidate = add_external_clause(k, {1, 2, 3});
    kernel_add_binary_clause(k, internalize(1), internalize(-4));
    add_external_clause(k, {2, 3, 4}, true, 2);
    assert(vivify_try(k, candidate) == ApplyResult::NoOp);
    assert(k.stats.vivify_noop == 1 && default_solve(k) == 10);
  }
  { // reversing the final assumption conflicts and removes that literal
    Kernel k;
    k.vivify_reuse = false;
    kernel_reserve_vars(k, 4);
    const Ref candidate = add_external_clause(k, {1, 2, 3});
    kernel_add_binary_clause(k, internalize(-3), internalize(4));
    kernel_add_binary_clause(k, internalize(-3), internalize(-4));
    assert(vivify_try(k, candidate) == ApplyResult::Ok);
    assert(k.stats.vivify_instantiated == 1 && default_solve(k) == 10);
  }
  { // with no consequence the reversed final assumption is a terminal no-op
    Kernel k;
    k.vivify_reuse = false;
    kernel_reserve_vars(k, 3);
    const Ref candidate = add_external_clause(k, {1, 2, 3});
    assert(vivify_try(k, candidate) == ApplyResult::NoOp);
    assert(k.stats.vivify_noop == 1 && default_solve(k) == 10);
  }
}

static void test_lcm_scope_and_satisfied_delete() {
  // Scope 1 (the LCM band) admits tier-2 and nothing beyond.
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Ref r1 = add_external_clause(k, {1, 2, 3}, true, 2);
  add_external_clause(k, {1, 3, 4}, true, 9); // beyond tier2
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  std::vector<Ref> cands;
  k.vivify_scope = 1;
  vivify_candidates(k, cands);
  assert(cands.size() == 1 && cands[0] == r1); // LCM scope: tier2 only
  assert(vivify_apply(k) == ApplyResult::Ok);  // r1 distilled to (1 2)
  assert(k.stats.vivify_subsumed == 1);

  // satisfied deletion via the instance surface
  Kernel k2;
  kernel_reserve_vars(k2, 3);
  kernel_assign_unit(k2, internalize(1));
  const Ref r2 = add_external_clause(k2, {1, 2, 3});
  assert(propagate_apply(k2) == ApplyResult::Ok);
  assert(vivify_try(k2, r2) == ApplyResult::Ok);
  assert(k2.stats.vivify_deleted == 1);
  assert(live_large(k2) == 0);
}

// Each scope selects exactly its glue band, tracking the live tier bounds.
static void test_tier3_scope() {
  Kernel k;
  kernel_reserve_vars(k, 6);
  const Ref r1 = add_external_clause(k, {1, 2, 3}, true, 2);
  const Ref r2 = add_external_clause(k, {1, 4, 5}, true, 5);
  const Ref r3 = add_external_clause(k, {2, 4, 6}, true, 9);
  std::vector<Ref> cands;
  auto only = [&](uint8_t scope, Ref want) {
    k.vivify_scope = scope;
    vivify_candidates(k, cands);
    assert(cands.size() == 1 && cands[0] == want);
  };
  only(3, r1); // tier1: glue <= k.tier1 (2)
  only(4, r2); // tier2: tier1 < glue <= k.tier2 (6)
  only(5, r3);        // tier3: glue > tier2
  k.vivify_scope = 1; // redundant band stops at tier2
  vivify_candidates(k, cands);
  assert(cands.size() == 2);
  k.vivify_scope = 0;          // unscoped spans every band, so that no band
  vivify_candidates(k, cands); // is API-unreachable (N1)
  assert(cands.size() == 3);
  k.tier2 = 10; // the bands move with the live tier bound, emptying tier3
  k.vivify_scope = 5;
  vivify_candidates(k, cands);
  assert(cands.empty());
}

// The leftover bit selects the untried tail, and re-arms when empty.
static void test_leftover_priority() {
  Kernel k;
  kernel_reserve_vars(k, 9);
  std::vector<Ref> refs;
  for (int i = 0; i < 3; i++)
    refs.push_back(add_external_clause(k, {1 + 3 * i, 2 + 3 * i, 3 + 3 * i}));
  std::vector<Ref> cands;
  // nothing armed: the round arms everything, so the bit never suppresses
  vivify_apply(k);
  assert(k.stats.vivify_reprioritized == 1);
  assert(k.stats.vivify_prioritized == 3);
  for (const Ref r : refs)
    assert(!clause_at(k.arena, r)->vivify());
  clause_at(k.arena, refs[2])->set_vivify(true); // armed candidates come first
  k.vivify_scope = 2; // exercise the large-irredundant radix/reverse path
  vivify_candidates(k, cands);
  assert(cands.size() == 3 && cands[0] == refs[2]);
}

// Candidates sharing a leading literal reuse the probe assumption; with
// reuse off every candidate returns to the root instead.
static void test_trail_reuse() {
  Kernel k;
  kernel_reserve_vars(k, 8);
  // a shared head, so the global literal order puts 1 and 2 first in all three
  const auto shared_head = [](Kernel &kernel) {
    add_external_clause(kernel, {1, 2, 3, 4});
    add_external_clause(kernel, {1, 2, 5, 6});
    add_external_clause(kernel, {1, 2, 7, 8});
  };
  shared_head(k);
  assert(vivify_apply(k) == ApplyResult::NoOp); // nothing to derive here
  assert(k.stats.vivify_reused > 0);            // but the prefix was shared
  const uint64_t on = k.stats.vivify_reused;

  Kernel k2;
  kernel_reserve_vars(k2, 8);
  k2.vivify_reuse = false;
  shared_head(k2);
  assert(vivify_apply(k2) == ApplyResult::NoOp);
  assert(k2.stats.vivify_reused == 0); // the off arm returns to the root
  assert(on > 0 && k2.stats.vivify_probes > k.stats.vivify_probes);
  assert(k.level == 0 && k2.level == 0 && !k.conflict_valid); // root is clean
}

// Kissat shortens a large Vivify candidate in place: same Ref, same
// allocation boundary.
static void test_large_rewrite_stays_in_place() {
  Kernel k;
  kernel_reserve_vars(k, 7);
  const Ref rewritten = add_external_clause(k, {1, 2, 3, 4});
  const Ref tail_ref = add_external_clause(k, {5, 6, 7});
  kernel_add_binary_clause(k, internalize(1), internalize(-2));

  assert(vivify_try(k, rewritten) == ApplyResult::Ok);
  assert(k.stats.vivify_shrunken == 1);
  assert(live_large(k) == 2);
  const Clause *shortened = clause_at(k.arena, rewritten);
  assert(!shortened->garbage() && shortened->size == 3);
  assert(shortened->shrunken());
  assert(clause_actual_words(shortened) == clause_words(4));
  assert(rewritten + clause_actual_words(shortened) == tail_ref);
  assert(!clause_at(k.arena, tail_ref)->garbage());
  for (Lit lit = 0; lit < 2 * k.nvars; lit++)
    for (const WatchEntry watch : k.watches.entries(lit))
      if (!watch.binary()) {
        assert(watch.ref() < k.arena.size());
        assert(!clause_at(k.arena, watch.ref())->garbage());
      }
  assert(k.level == 0 && !k.conflict_valid);
  assert(default_solve(k) == 10);
}

static void test_large_learned_retry_advice() {
  for (const bool redundant : {false, true}) {
    Kernel k;
    kernel_reserve_vars(k, 5);
    const Ref ref = add_external_clause(k, {1, 2, 3, 4}, redundant, 2);
    kernel_add_binary_clause(k, internalize(1), internalize(-2));
    std::fill(k.bve_retry.begin(), k.bve_retry.end(), 0);
    const uint64_t marks = k.stats.bve_marks;
    assert(vivify_try(k, ref) == ApplyResult::Ok);
    assert(k.stats.vivify_shrunken == 1);
    assert(clause_at(k.arena, ref)->size == 3);
    for (Var v = 0; v < 4; ++v)
      assert(static_cast<bool>(k.bve_retry[v]) == !redundant);
    assert(!k.bve_retry[4]);
    assert(k.stats.bve_marks - marks == (redundant ? 0 : 4));
    assert(default_solve(k) == 10);
  }
  // Large reverse-last instantiation has selective removed-literal advice.
  Kernel k;
  k.vivify_reuse = false;
  kernel_reserve_vars(k, 5);
  const Ref ref = add_external_clause(k, {1, 2, 3, 4});
  kernel_add_binary_clause(k, internalize(-4), internalize(5));
  kernel_add_binary_clause(k, internalize(-4), internalize(-5));
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), 0);
  assert(vivify_try(k, ref) == ApplyResult::Ok);
  assert(k.stats.vivify_instantiated == 1);
  for (Var v = 0; v < 3; ++v) assert(!k.bve_retry[v]);
  assert(k.bve_retry[3] && !k.bve_retry[4]);
  assert(default_solve(k) == 10);
}

// Kissat's vivify_learn_large marks the rewritten clause before reducing its
// size, so Factor sees the stale old tail too.
static void test_large_rewrite_marks_stale_old_tail() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Ref ref = add_external_clause(k, {1, 3, 4, 2});
  kernel_add_binary_clause(k, internalize(1), internalize(-2));
  std::fill(k.factor_marks.begin(), k.factor_marks.end(), 0);
  k.factor_literals_marked = 0;

  kernel_clear_subsume_material(k);
  const uint64_t material_before = k.subsume_material_count;
  assert(vivify_try(k, ref) == ApplyResult::Ok);
  const Clause *rewritten = clause_at(k.arena, ref);
  assert(!rewritten->garbage() && rewritten->size == 3);
  assert(std::find(rewritten->begin(), rewritten->end(), internalize(2)) ==
         rewritten->end());
  assert(k.factor_marks[internalize(2)]);
  assert(k.subsume_material_count == material_before + 4);
  for (Var v = 0; v < 4; ++v)
    assert(kernel_has_subsume_material(k, v));
}

// Kissat's watch selector scans past the first unassigned literal: a later
// unassigned one, then a higher-level false one, wins.
static void test_replacement_watch_pose_scans_unassigned_tail() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  k.level = 1;
  kernel_assign(k, internalize(-1), 1, DECISION_REASON, false);
  k.level = 2;
  kernel_assign(k, internalize(-2), 2, DECISION_REASON, false);
  k.level = 3;
  kernel_assign(k, internalize(-5), 3, DECISION_REASON, false);
  std::vector<Lit> replacement = {
      internalize(1), internalize(2), internalize(3), internalize(4),
      internalize(5)};
  vivify_pose_replacement_watches(k, replacement);
  const std::vector<Lit> expected = {
      internalize(5), internalize(4), internalize(3), internalize(2),
      internalize(1)};
  assert(replacement == expected);
}

// Kissat rechecks the two watches after every Vivify terminal: false watches
// at levels 5 and 4 retain only the levels below 4.
static void test_replacement_watch_invariant_backtracks_below_false_pair() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  for (int i = 1; i <= 5; ++i)
    kernel_assign_decision(k, internalize(-i));
  vivify_reestablish_watch_invariant(k, lits_of({5, 4, 3, 2}).data(), 4);
  assert(k.level == 3 && k.trail.size() == 3);
}

// Kissat's large instantiation removes the reversed literal in physical
// order, retains the first size-2 assumption levels and poses watches there.
static void test_large_instantiation_retains_candidate_pose() {
  Kernel k;
  k.vivify_reuse = false;
  kernel_reserve_vars(k, 5);
  const Ref candidate = add_external_clause(k, {1, 2, 3, 4});
  kernel_add_binary_clause(k, internalize(-4), internalize(5));
  kernel_add_binary_clause(k, internalize(-4), internalize(-5));
  assert(vivify_try(k, candidate) == ApplyResult::Ok);
  assert(k.stats.vivify_instantiated == 1);
  const Clause *replacement = nullptr;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && c->size == 3)
      replacement = c;
    ref += clause_actual_words(c);
  }
  assert(replacement);
  assert(replacement->lits[0] == internalize(3));
  assert(replacement->lits[1] == internalize(2));
  assert(default_solve(k) == 10);
}

static void test_large_rewrite_trims_without_implied_literal() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  for (int i = 1; i <= 4; i++)
    kernel_assign_decision(k, internalize(-i));
  vivify_trim_replacement_prefix(k, INVALID_LIT, 5);
  assert(k.level == 3 && k.trail.size() == 3);

  // An implied-literal rewrite keeps the prefix (Kissat vivify_learn_large).
  vivify_trim_replacement_prefix(k, internalize(5), 4);
  assert(k.level == 3 && k.trail.size() == 3);
}

static void test_schedule_prefix_flush() {
  const std::vector<Lit> short_clause = lits_of({1, 2, 3});
  Kernel k;
  kernel_reserve_vars(k, 4);
  assert(!k.vivify_flush &&
         "production follows Kissat: watch flushing is not clause prefix "
         "subsumption");
  k.vivify_flush = true;
  add_external_clause(k, {1, 2, 3});
  add_external_clause(k, {1, 2, 3, 4});
  std::vector<Ref> cands;
  vivify_candidates(k, cands);
  assert(cands.size() == 2); // N1: scheduling never hides either instance
  volatile sig_atomic_t stop = 1;
  k.stop = &stop; // isolate the pre-propagation schedule flush
  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(k.stats.vivify_flushed == 1);
  assert(k.stats.vivify_checked == 0);
  assert(live_large(k, short_clause.data(), 3) == 1);

  Kernel off;
  off.vivify_flush = false;
  kernel_reserve_vars(off, 4);
  add_external_clause(off, {1, 2, 3});
  add_external_clause(off, {1, 2, 3, 4});
  off.stop = &stop;
  assert(vivify_apply(off) == ApplyResult::NoOp);
  assert(off.stats.vivify_flushed == 0 && live_large(off) == 2);
}

static void test_schedule_flush_partition() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  add_external_clause(k, {1, 2, 3}, true, 2);
  add_external_clause(k, {1, 2, 3, 4});
  add_external_clause(k, {1, 2, 3, 5}, true, 5);
  k.vivify_scope = 0;
  volatile sig_atomic_t stop = 1;
  k.stop = &stop;
  assert(vivify_apply(k) == ApplyResult::NoOp);
  assert(k.stats.vivify_flushed == 0);
  assert(live_large(k) == 3); // neither partitions nor tier bands are crossed
}

static void test_schedule_root_false_ternary_becomes_binary() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {-1, 2, 3}, false, 0, 1);
  kernel_assign_unit(k, internalize(1));
  assert(!kernel_propagate(k));
  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(k.stats.vivify_checked == 0); // simplified before candidate dispatch
  assert(live_large(k) == 0);
  assert(kernel_binary_live(k, internalize(2), internalize(3)));
}

// Kissat fixes tier membership before root simplification: a tier-2 clause
// shrunk to tier-1 glue stays in the tier-2 schedule.
static void test_schedule_root_shrink_preserves_pass_time_tier() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  k.tier1 = 2;
  k.tier2 = 6;
  k.vivify_scope = 4;
  add_external_clause(k, {-1, 2, 3, 4}, true, 3, 1);
  kernel_assign_unit(k, internalize(1));
  assert(!kernel_propagate(k));
  volatile sig_atomic_t stop = 1;
  k.stop = &stop; // inspect the committed schedule before candidate dispatch
  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(k.stats.vivify_checked == 0);
  assert(k.stats.vivify_reprioritized == 1);
  assert(k.stats.vivify_prioritized == 1);
  assert(k.large_watches_connected);
  bool found = false;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage()) {
      found = true;
      assert(c->size == 3 && c->glue() == 2 && c->vivify());
    }
    ref += clause_actual_words(c);
  }
  assert(found);
}

static void test_kissat_candidate_count_vector_ties() {
  { // Irredundant uses Kissat's one-count key; the stable radix tie is ref.
    Kernel k;
    kernel_reserve_vars(k, 6);
    for (const Var v : {5u, 0u, 4u, 1u, 3u, 2u})
      kernel_record_import(k, v);
    const Ref lo = add_external_clause(k, {6, 1, 5});
    const Ref hi = add_external_clause(k, {2, 4, 3});
    std::vector<Ref> cands;
    vivify_candidates(k, cands);
    assert(cands.size() == 2 && cands[0] == hi && cands[1] == lo);
  }
  { // Only the maximum count and inverse size participate; clause pose stays.
    Kernel k;
    kernel_reserve_vars(k, 7);
    const Ref lo = add_external_clause(k, {1, 2, 3});
    const Ref hi = add_external_clause(k, {4, 5, 6, 7});
    // Raise literal 7's count above every literal in the shorter clause.
    add_external_clause(k, {7, 1, 4}, true, 2);
    add_external_clause(k, {7, 2, 5}, true, 2);
    std::vector<Ref> cands;
    k.vivify_scope = 2;
    vivify_candidates(k, cands);
    assert(cands.size() == 2 && cands[0] == hi && cands[1] == lo);
    assert(clause_at(k.arena, hi)->lits[0] == internalize(4));
  }
  { // Fully identical keys: Kissat pops the larger arena reference first.
    Kernel k;
    kernel_reserve_vars(k, 3);
    const Ref first = add_external_clause(k, {1, 2, 3});
    const Ref second = add_external_clause(k, {1, 2, 3});
    std::vector<Ref> cands;
    vivify_candidates(k, cands);
    assert(cands.size() == 2 && cands[0] == second && cands[1] == first);
  }
  { // An appended packed-arena replacement retains its in-place source order.
    Kernel k;
    kernel_reserve_vars(k, 6);
    const Ref old = add_external_clause(k, {1, 2, 3}, false, 0, 1);
    const Ref later = add_external_clause(k, {4, 5, 6}, false, 0, 2);
    kernel_inherit_clause_position(k, 3, old);
    kernel_delete_clause(k, clause_at(k.arena, old));
    const Ref replacement = add_external_clause(k, {1, 2, 3}, false, 0, 3);
    assert(replacement > later); // physical append differs from logical pose
    std::vector<Ref> cands;
    k.vivify_scope = 2; // radix must reconstruct Kissat's old in-place order
    vivify_candidates(k, cands);
    assert(cands.size() == 2);
    assert(cands[0] == later && cands[1] == replacement);
  }
}

// Kissat has no per-candidate clause-size cutoff; the tick budget bounds the
// round.
static void test_large_candidate_has_no_invented_cutoff() {
  Kernel k;
  static constexpr uint32_t size = 101;
  kernel_reserve_vars(k, size);
  std::vector<Lit> lits;
  for (uint32_t i = 1; i <= size; i++)
    lits.push_back(internalize(static_cast<int>(i)));
  const Ref candidate =
      kernel_add_large_clause(k, lits.data(), size, false, 0);
  k.vivify_scope = 2;
  std::vector<Ref> candidates;
  vivify_candidates(k, candidates);
  assert(candidates.size() == 1 && candidates[0] == candidate);
  assert(vivify_try(k, candidate) == ApplyResult::NoOp);
  assert(k.stats.vivify_checked == 1 && k.level == 0);
}

// After an earlier tier exceeded the cumulative ceiling, Kissat builds a later
// tier's schedule but tries no candidate (zero is not "unbounded").
static void test_precharged_cumulative_budget_stops_before_candidate() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {1, 2, 3});
  k.vivify_scope = 2;
  k.work_budget_bounded = true;
  k.work_budget = 0;
  k.work_budget_offset = 1;
  assert(vivify_apply(k) == ApplyResult::NoOp);
  assert(k.stats.vivify_checked == 0 && k.level == 0);
}

// Every Kissat Vivify dose starts with the dense flush: duplicate binaries
// collapse and large clauses are rewatched (a first-candidate stop isolates it).
static void test_round_entry_flushes_large_and_deduplicates_binaries() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit a = internalize(1), b = internalize(-2);
  kernel_add_binary_clause(k, a, b);
  kernel_add_binary_clause(k, a, b);
  kernel_add_binary_clause(k, a, b);
  const std::vector<Lit> candidate = lits_of({3, 4, 5});
  kernel_add_large_clause(k, candidate.data(), 3, false, 0);
  assert(kernel_binary_copies(k, a, b) == 3);

  volatile sig_atomic_t stop = 1;
  k.stop = &stop;
  assert(vivify_apply(k) == ApplyResult::NoOp);
  k.stop = nullptr;

  assert(k.stats.vivify_checked == 0);
  assert(kernel_binary_copies(k, a, b) == 1);
  assert(live_large(k, candidate.data(), 3) == 1);
  kernel_check_invariants(k);
}

// Kissat propagates a Vivify-learned root unit and continues the built
// schedule; old records stay garbage in place, so candidate Refs stay valid.
static void test_unit_rewrite_continues_schedule() {
  Kernel k;
  kernel_reserve_vars(k, 10); // the fixture ranges through variable 10
  const std::vector<Lit> final_lits = lits_of({4, 5, 8});
  const std::vector<Lit> prefix_lits = lits_of({4, 5, 6});
  const Ref final = kernel_add_large_clause(k, final_lits.data(), 3, false, 0);
  const Ref satisfied = add_external_clause(k, {3, 9, 10});
  const Ref prefix = kernel_add_large_clause(k, prefix_lits.data(), 3, false, 0);
  const Ref unit = add_external_clause(k, {1, 2, 3});
  const size_t arena_words = k.arena.size();
  const uint64_t arena_generation = k.arena_generation;
  // Priority orders the transaction: learn a root unit, retain a prefix,
  // delete a root-satisfied clause, reuse the prefix on the final clause.
  clause_at(k.arena, unit)->set_vivify(true);
  clause_at(k.arena, prefix)->set_vivify(true);
  // Literal 3 probes first; ¬3 forces 7 and ¬7, so analysis learns unit 3.
  kernel_add_binary_clause(k, internalize(3), internalize(7));
  kernel_add_binary_clause(k, internalize(3), internalize(-7));
  std::vector<Ref> candidates;
  vivify_candidates(k, candidates);
  assert(candidates.size() == 4 && candidates[0] == unit &&
         candidates[1] == prefix && candidates[2] == satisfied &&
         candidates[3] == final);
  assert(vivify_apply(k) == ApplyResult::Ok);
  assert(kernel_root_value(k, internalize(3)) > 0);
  assert(k.stats.vivify_shrunken == 1);
  assert(k.stats.vivify_deleted == 1 && k.stats.vivify_checked == 3);
  assert(k.stats.vivify_reused == 1);
  // Retired records stay garbage in place; the no-op clauses stay live.
  assert(k.arena.size() == arena_words);
  assert(k.arena_generation == arena_generation);
  assert(clause_at(k.arena, unit)->garbage());
  assert(clause_at(k.arena, satisfied)->garbage());
  assert(live_large(k, prefix_lits.data(), 3) == 2);
  assert(live_large(k, final_lits.data(), 3) == 2);
  assert(k.level == 0 && kernel_propagation_fixpoint(k));
  assert(default_solve(k) == 10);
}

// The termination poll is cadence-free: a raised stop is both reached and
// honoured on the first candidate, before any work.
static void test_termination_poll() {
  Kernel k;
  kernel_reserve_vars(k, 9);
  for (int i = 0; i < 3; i++)
    add_external_clause(k, {1 + 3 * i, 2 + 3 * i, 3 + 3 * i});
  volatile sig_atomic_t stop = 1;
  k.stop = &stop;
  const ApplyResult r = vivify_apply(k);
  k.stop = nullptr;
  assert(r == ApplyResult::NoOp);
  assert(k.stats.vivify_polls == 1);      // reached on the first candidate
  assert(k.stats.vivify_terminated == 1); // and fired there
  assert(k.stats.vivify_checked == 0);    // before any work was done
  assert(k.level == 0);                   // and the root is clean
}

// Typed parameters: defaults, the 10:1 integer-division sort boundary, scoped
// installation and restore, and validation.
static void test_typed_parameters() {
  Kernel kernel;
  const VivifyParameters defaults = VivifyParameters::from_kernel(kernel);
  assert(defaults.scope == 0 && defaults.reuse && !defaults.flush);
  assert(defaults.irredundant_sort_ratio == 10 && defaults.valid());
  assert(!vivify_skip_global_order(109, 10, defaults));
  assert(vivify_skip_global_order(110, 10, defaults));
  VivifyParameters ratio = defaults;
  ratio.irredundant_sort_ratio = 2;
  assert(!vivify_skip_global_order(5, 2, ratio));
  assert(vivify_skip_global_order(6, 2, ratio));

  const VivifyParameters posed{5, false, true, 3};
  {
    strategy::ScopedVivifyParameters scope(kernel, posed);
    assert(VivifyParameters::from_kernel(kernel).scope == 5);
    assert(!kernel.vivify_reuse && kernel.vivify_flush);
    assert(kernel.vivify_irredundant_sort_ratio == 3);
  }
  const VivifyParameters restored = VivifyParameters::from_kernel(kernel);
  assert(restored.scope == defaults.scope && restored.reuse == defaults.reuse);
  assert(restored.flush == defaults.flush);
  assert(restored.irredundant_sort_ratio == defaults.irredundant_sort_ratio);

  VivifyParameters invalid = defaults;
  invalid.irredundant_sort_ratio = 0;
  assert(!invalid.valid());
  invalid = defaults;
  invalid.scope = 6;
  assert(!invalid.valid());
}

int main() {
  test_typed_parameters();
  test_implied_literal_strengthens();
  test_jumped_binary_reason_subsumes_candidate();
  test_implied_negation_removes();
  test_probe_conflict_strengthens();
  test_terminal_dispatch();
  test_lcm_scope_and_satisfied_delete();
  test_tier3_scope();
  test_leftover_priority();
  test_trail_reuse();
  test_large_rewrite_stays_in_place();
  test_large_learned_retry_advice();
  test_large_rewrite_marks_stale_old_tail();
  test_replacement_watch_pose_scans_unassigned_tail();
  test_replacement_watch_invariant_backtracks_below_false_pair();
  test_large_instantiation_retains_candidate_pose();
  test_large_rewrite_trims_without_implied_literal();
  test_schedule_prefix_flush();
  test_schedule_flush_partition();
  test_schedule_root_false_ternary_becomes_binary();
  test_schedule_root_shrink_preserves_pass_time_tier();
  test_kissat_candidate_count_vector_ties();
  test_large_candidate_has_no_invented_cutoff();
  test_precharged_cumulative_budget_stops_before_candidate();
  test_round_entry_flushes_large_and_deduplicates_binaries();
  test_unit_rewrite_continues_schedule();
  test_termination_poll();
  puts("test_vivify OK");
  return 0;
}
