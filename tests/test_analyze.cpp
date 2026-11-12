// Analyze/Backjump: forced scenarios with a known 1-UIP clause and jump level.
#include "unit_test.hpp"
#include "kernel/kernel.hpp"
#include "rule_test_utils.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/propagate/propagate.hpp"
#include "tactix_c_api.hpp"

TEST(test_uip) {
  // At level 2, 4 and 5 follow decision 2; (-3,-4,-5) resolves through their
  // binary reasons to (-2,-3).
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  kernel_add_binary_clause(k, internalize(-2), internalize(5));
  const Ref cref = add_external_clause(k, {-3, -4, -5});
  kernel_assign_decision(k, internalize(1));
  kernel_assign(k, internalize(3), 1, internalize(-1), true);
  kernel_assign_decision(k, internalize(2));
  kernel_assign(k, internalize(4), 2, internalize(-2), true);
  kernel_assign(k, internalize(5), 2, internalize(-2), true);
  k.conflict_ref = cref;
  k.conflict_valid = true;

  assert(analyze_guard(k));
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.pending.valid);
  assert(k.pending.lits.size() == 2);
  assert(k.pending.lits[0] == internalize(-2)); // negated UIP
  assert(k.pending.lits[1] == internalize(-3));
  assert(k.pending.jump_level == 1);

  assert(backjump_guard(k));
  assert(backjump_apply_to(k, k.pending.jump_level) == ApplyResult::Ok);
  assert(k.level == 1);
  assert(k.values[internalize(-2)] > 0); // asserted
  assert(k.assigned[var_of(internalize(-2))].jumped &&
         k.stats.jumped_reasons == 1 &&
         "Kissat's learned-binary assignment is not classification-gated");
  assert(!k.pending.valid);

  assert(default_solve(k) == 10); // still satisfiable: 1, -2, 3
}

TEST(test_detached_large_reason_in_conflict_analysis) {
  // The GF(2) reason (3, -1, -2) is not in the watched formula; resolving it with
  // the conflict (-3, -2, -1) learns (-2, -1).
  Kernel k;
  ScopedHeuristics hg(k);
  kernel_reserve_vars(k, 3);
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(2));

  const Lit reason_lits[3] = {internalize(3), internalize(-1),
                              internalize(-2)};
  const Ref reason_ref =
      kernel_add_detached_large_reason(k, reason_lits, 3, 41);
  kernel_assign_ref(k, internalize(3), reason_ref);
  const Clause *reason = clause_at(k.arena, reason_ref);
  assert(reason->garbage() && reason->reason() && reason->detached_reason());
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    for (const WatchEntry watch : k.watches.entries(lit))
      assert(watch.binary() || watch.ref() != reason_ref);

  k.conflict_ref =
      add_external_clause(k, {-3, -2, -1}, false, 0, 42);
  k.conflict_valid = true;
  assert(analyze_guard(k));
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.pending.valid);
  assert(k.pending.lits.size() == 2);
  assert(k.pending.lits[0] == internalize(-2));
  assert(k.pending.lits[1] == internalize(-1));
  assert(k.pending.jump_level == 1);

  // Analyze consumed the record without attaching or strengthening it.
  reason = clause_at(k.arena, reason_ref);
  assert(reason->garbage() && reason->reason() && reason->detached_reason() &&
         reason->size == 3);
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    for (const WatchEntry watch : k.watches.entries(lit))
      assert(watch.binary() || watch.ref() != reason_ref);

  assert(backjump_apply_to(k, k.pending.jump_level) == ApplyResult::Ok);
  assert(k.level == 1 && k.values[internalize(-2)] > 0);
  assert(!clause_at(k.arena, reason_ref)->reason());
  kernel_collect_garbage(k);
  kernel_check_invariants(k);
}

// D-SPLIT: ConflictAnalysis fuses Analyze and Backjump, so `pending` never
// outlives one apply.
TEST(test_conflictanalysis_atomic) {
  Kernel k;
  kernel_reserve_vars(k, 3);
  add_external_clause(k, {-1, -2, 3});
  add_external_clause(k, {-1, -2, -3});
  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(2));
  assert(propagate_apply(k) == ApplyResult::Conflict);

  const int id = tactix_rule_id("ConflictAnalysis");
  assert(id >= 0);
  const RuleVTable &vt = tactix_rules()[static_cast<size_t>(id)];
  assert(vt.guard(k)); // conflict recorded
  assert(vt.apply(k) == ApplyResult::Ok);
  // pending consumed inside the one apply — never observable at a boundary
  assert(!k.pending.valid);
  assert(k.values[internalize(-2)] > 0); // UIP asserted (jumped to level 1)
  assert(k.level == 1);
}

TEST(test_learned_unit) {
  // (-1 2), (-1 -2): deciding 1 conflicts and learns unit -1
  Kernel k;
  ScopedHeuristics hg(k);
  kernel_reserve_vars(k, 2);
  k.h->stable = true;
  heuristics_update_scores(k);
  assert(heuristics_heap_contains(k, 0));
  const double unit_score = k.h->scores.score[0];
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Conflict);
  assert(analyze_apply(k) == ApplyResult::Ok);
  // Level-one failed analysis owns its root-unit batch inside atomic M3; it
  // never exposes the ordinary Analyze -> Backjump pending handoff.
  assert(!k.pending.valid);
  assert(k.level == 0);
  assert(k.values[internalize(-1)] > 0);
  assert(k.stats.learned_units == 1);
  assert(!heuristics_heap_contains(k, 0));
  assert(k.h->scores.score[0] == unit_score);
}

TEST(test_bump_admission_order_is_not_cleanup_order) {
  Kernel k;
  ScopedHeuristics hg(k);
  kernel_reserve_vars(k, 4);
  for (Var v = 0; v < k.nvars; ++v)
    heuristics_score_set(k, v, 0.0);
  k.h->stable = true;
  heuristics_update_scores(k);

  // Variable 0 is minimization scratch admitted to the bump set after 1 and 2;
  // equal increments make the heap root observe the bump order.
  k.analyzed = {0, 1, 2};
  k.bump_order = {1, 2, 0};
  k.seen[0] = k.seen[1] = k.seen[2] = 1; // analysis-side bump mark
  k.analyzed_seen = 3;
  analyze_bump_after_backjump(k);

  assert(heuristics_heap_next_unassigned(k) == 1);
  assert(k.analyzed.empty());
  assert(k.bump_order.empty());
  assert(!k.seen[0] && !k.seen[1] && !k.seen[2]);
}

// Kissat's level-one failed-literal analysis walks past every singleton
// frontier: -2 and then -1 become root units before bumping.
TEST(test_level_one_failed_literal_batch) {
  Kernel k;
  ScopedHeuristics hg(k);
  kernel_reserve_vars(k, 5); // variable 5 remains as a decision witness
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  kernel_add_binary_clause(k, internalize(-3), internalize(-4));
  // Prove the no-pending path refreshes the external-policy observation.
  k.pending.glue = UINT32_MAX;

  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Conflict);
  assert(k.level == 1);
  assert(analyze_apply(k) == ApplyResult::Ok);

  assert(!k.pending.valid);
  assert(k.pending.glue == 0);
  assert(k.level == 0);
  assert(k.values[internalize(-2)] > 0);
  assert(k.values[internalize(-1)] > 0);
  assert(k.assigned[var_of(internalize(-2))].level == 0);
  assert(k.assigned[var_of(internalize(-1))].level == 0);
  assert(k.unit_ids[var_of(internalize(-2))] != 0);
  assert(k.unit_ids[var_of(internalize(-1))] != 0);
  assert(k.stats.learned == 1);
  assert(k.stats.learned_units == 2);
  assert(!heuristics_var_active(k, var_of(internalize(1))));
  assert(!heuristics_var_active(k, var_of(internalize(2))));
  const Var next = heuristics_queue_next_unassigned(k);
  assert(next != var_of(internalize(1)) && next != var_of(internalize(2)));
  assert(default_solve(k) == 10);
}

// The batch fixture plus two clauses that refute the opposite branch, as a
// complete UNSAT proof for an external checker.
static void write_level_one_failed_literal_proof(const char *path) {
  Proof proof;
  assert(proof.open(path, ProofFormat::DsrAscii));
  Kernel k;
  ScopedHeuristics hg(k);
  kernel_reserve_vars(k, 5);
  proof.bind_variable_map(&k.internal_to_external);
  k.proof = &proof;
  kernel_add_binary_clause(k, internalize(-1), internalize(2), 1);
  kernel_add_binary_clause(k, internalize(-2), internalize(3), 2);
  kernel_add_binary_clause(k, internalize(-2), internalize(4), 3);
  kernel_add_binary_clause(k, internalize(-3), internalize(-4), 4);
  kernel_add_binary_clause(k, internalize(1), internalize(5), 5);
  kernel_add_binary_clause(k, internalize(1), internalize(-5), 6);
  k.next_clause_id = 6;

  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Conflict);
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.learned_units == 2);
  assert(k.values[internalize(-2)] > 0);
  assert(k.values[internalize(-1)] > 0);
  assert(propagate_apply(k) == ApplyResult::Unsat);
  assert(k.inconsistent);
  proof.flush();
  k.proof = nullptr;
}

TEST(test_shrink) {
  // The block {-2, -3} is not minimizable but converges on decision 1, so
  // shrinking learns (-4 -1).
  Kernel k;
  kernel_reserve_vars(k, 6);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  add_external_clause(k, {-4, -2, -3, 5});
  add_external_clause(k, {-4, -5, 6});
  add_external_clause(k, {-6, -5, -2, -3});

  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(4));
  assert(propagate_apply(k) == ApplyResult::Conflict);

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.shrunken == 1);
  assert(k.pending.lits.size() == 2);
  assert(k.pending.lits[0] == internalize(-4)); // negated UIP
  assert(k.pending.lits[1] == internalize(-1)); // shrunken level-1 block
  assert(k.pending.jump_level == 1);
  assert(backjump_apply_to(k, k.pending.jump_level) == ApplyResult::Ok);
  assert(default_solve(k) == 10); // still satisfiable (1 2 3, -4)
}

TEST(test_shrink_accepts_lower_learned_literal) {
  // Block {-b,-c} converges to b; c's walk reaches the learned -a (Kissat marks
  // learned literals removable before shrink=3).
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_assign_decision(k, internalize(1)); // a @ 1
  kernel_assign_decision(k, internalize(2)); // b @ 2
  const Ref reason_ref = add_external_clause(k, {-2, -1, 3});
  kernel_assign(k, internalize(3), 2, reason_ref, false); // c @ 2
  kernel_assign_decision(k, internalize(4));              // d @ 3
  k.conflict_ref = add_external_clause(k, {-4, -2, -3, -1});
  k.conflict_valid = true;

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.shrunken == 1);
  assert(k.pending.lits.size() == 3);
  assert(k.pending.lits[0] == internalize(-4));
  assert(k.pending.lits[1] == internalize(-2));
  assert(k.pending.lits[2] == internalize(-1));
  assert(k.pending.jump_level == 2);
}

TEST(test_shrink_crosses_unmarked_lower_level) {
  // Block {-b,-c} converges to b through level-2 -y, whose reason reaches the
  // learned -a: Kissat follows the chain across the intermediate level.
  Kernel k;
  kernel_reserve_vars(k, 6);
  kernel_assign_decision(k, internalize(1)); // a @ 1
  kernel_assign_decision(k, internalize(2)); // unused level-2 decision
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_assign(k, internalize(3), 2, internalize(-1), true); // y @ 2
  kernel_assign_decision(k, internalize(4));                  // b @ 3
  const Ref reason_ref = add_external_clause(k, {-4, -3, 5});
  kernel_assign(k, internalize(5), 3, reason_ref, false); // c @ 3
  kernel_assign_decision(k, internalize(6));              // d @ 4
  k.conflict_ref = add_external_clause(k, {-6, -4, -5, -1});
  k.conflict_valid = true;

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.shrunken == 1);
  assert(k.pending.lits.size() == 3);
  assert(k.pending.lits[0] == internalize(-6));
  assert(k.pending.lits[1] == internalize(-4));
  assert(k.pending.lits[2] == internalize(-1));
  assert(k.pending.jump_level == 3);
}

TEST(test_shrink_processes_lower_blocks_first) {
  // The lower block {-q,-r} converges first, enabling the higher block; Kissat's
  // tail-to-head order learns {-d,-b,-y}.
  Kernel k;
  kernel_reserve_vars(k, 7);
  kernel_assign_decision(k, internalize(1)); // p @ 1
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_assign(k, internalize(2), 1, internalize(-1), true); // y @ 1
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  kernel_assign(k, internalize(3), 1, internalize(-2), true); // q @ 1
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  kernel_assign(k, internalize(4), 1, internalize(-2), true); // r @ 1

  kernel_assign_decision(k, internalize(5)); // b @ 2
  const Ref reason_ref = add_external_clause(k, {-5, -2, 6});
  kernel_assign(k, internalize(6), 2, reason_ref, false); // c @ 2
  kernel_assign_decision(k, internalize(7));              // d @ 3
  k.conflict_ref = add_external_clause(k, {-7, -5, -6, -3, -4});
  k.conflict_valid = true;

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.shrunken == 2);
  assert(k.pending.lits.size() == 3);
  assert(k.pending.lits[0] == internalize(-7));
  assert(k.pending.lits[1] == internalize(-5));
  assert(k.pending.lits[2] == internalize(-2));
  assert(k.pending.jump_level == 2);
}

static Kernel singleton_block_fixture() {
  // Kissat shrink=3 skips the singleton block -c (redundant through -a): -c stays
  // and its reason is not dereferenced.
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(2));
  // N1 schedule: c is assigned at level 2 from (-a c).
  kernel_assign(k, internalize(3), 2, internalize(-1), true);
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_assign_decision(k, internalize(4));
  k.conflict_ref = add_external_clause(k, {-4, -3, -1});
  k.conflict_valid = true;
  assert(analyze_apply(k) == ApplyResult::Ok);
  return k;
}

TEST(test_singleton_block_skips_minimization) {
  Kernel k = singleton_block_fixture();
  assert(k.pending.lits.size() == 3);
  assert(k.pending.lits[0] == internalize(-4));
  assert(k.pending.lits[1] == internalize(-3));
  assert(k.pending.lits[2] == internalize(-1));
  assert(k.pending.jump_level == 2);
  assert(k.stats.minimized == 0);
}

static Kernel minimization_depth_fixture(
    uint32_t chain, bool binary,
    uint32_t depth_limit =
        strategy::AnalyzeParameters::MINIMIZE_DEPTH_DEFAULT) {
  // Anchor a, root-false side literal z, and a chain x[0..chain) from a; y
  // fails block shrinking. -x.back() is removed exactly when its reason walk
  // reaches -a before the depth cap; -y stays.
  assert(chain > 0);
  const uint32_t a = 1, z = 2, c = 3, b = 4, first_x = 5;
  const uint32_t y = first_x + chain, d = y + 1;
  Kernel k;
  k.analyze_parameters.minimize_depth_limit = depth_limit;
  kernel_reserve_vars(k, d);
  kernel_assign_unit(k, internalize(z));
  kernel_assign_decision(k, internalize(a));
  // White-box N1 schedule: a second decision-reason assignment at level 1
  // gives y a valid but non-redundant lower-level endpoint.
  kernel_assign(k, internalize(c), 1, DECISION_REASON, false);
  kernel_assign_decision(k, internalize(b));
  uint32_t previous = a;
  for (uint32_t i = 0; i < chain; i++) {
    const uint32_t current = first_x + i;
    if (binary) {
      kernel_add_binary_clause(k, internalize(-static_cast<int>(previous)),
                               internalize(static_cast<int>(current)));
      kernel_assign(k, internalize(static_cast<int>(current)), 2,
                    internalize(-static_cast<int>(previous)), true);
    } else {
      const Lit reason[3] = {internalize(-static_cast<int>(previous)),
                             internalize(-static_cast<int>(z)),
                             internalize(static_cast<int>(current))};
      const Ref ref = kernel_add_large_clause(k, reason, 3, false, 0);
      kernel_assign(k, internalize(static_cast<int>(current)), 2, ref, false);
    }
    previous = current;
  }
  kernel_add_binary_clause(k, internalize(-static_cast<int>(c)),
                           internalize(static_cast<int>(y)));
  kernel_assign(k, internalize(static_cast<int>(y)), 2,
                internalize(-static_cast<int>(c)), true);
  kernel_assign_decision(k, internalize(static_cast<int>(d)));
  const Lit conflict[4] = {internalize(-static_cast<int>(d)),
                           internalize(-static_cast<int>(previous)),
                           internalize(-static_cast<int>(y)),
                           internalize(-static_cast<int>(a))};
  k.conflict_ref = kernel_add_large_clause(k, conflict, 4, false, 0);
  k.conflict_valid = true;
  assert(analyze_apply(k) == ApplyResult::Ok);
  return k;
}

TEST(test_minimization_depth_matches_kissat) {
  // Binary runs do not spend recursive depth in Kissat.  This exceeds the
  // recursive cap yet must still reach -a and remove the chain literal.
  Kernel binary = minimization_depth_fixture(1100, true);
  assert(binary.pending.lits.size() == 3);
  assert(binary.stats.minimized == 1);
  assert(binary.stats.minimize_depth_cutoffs == 0);

  // For large reasons the deepest x is entered at depth chain-1.  Kissat
  // admits depth 999 and rejects depth 1000 with minimizedepth=1000.
  Kernel admitted = minimization_depth_fixture(1000, false);
  assert(admitted.pending.lits.size() == 3);
  assert(admitted.stats.minimized == 1);
  assert(admitted.stats.minimize_depth_cutoffs == 0);
  Kernel rejected = minimization_depth_fixture(1001, false);
  assert(rejected.pending.lits.size() == 4);
  assert(rejected.stats.minimized == 0);
  assert(rejected.stats.minimize_depth_cutoffs == 1);

  Kernel disabled = minimization_depth_fixture(1, false, 0);
  assert(disabled.pending.lits.size() == 4);
  Kernel unbounded = minimization_depth_fixture(1100, false, UINT32_MAX);
  assert(unbounded.pending.lits.size() == 3);
  assert(unbounded.stats.minimize_depth_cutoffs == 0);
}

static Kernel learned_stable_order_fixture() {
  // Block {-b,-c} (c later on the trail) fails to shrink through decision a,
  // isolating the within-level ordering rule.
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(2));
  kernel_assign(k, internalize(3), 2, internalize(-1), true);
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_assign_decision(k, internalize(4));
  k.conflict_ref = add_external_clause(k, {-4, -2, -3});
  k.conflict_valid = true;
  assert(analyze_apply(k) == ApplyResult::Ok);
  return k;
}

TEST(test_learned_level_stable_order) {
  Kernel k = learned_stable_order_fixture();
  assert(k.pending.lits.size() == 3);
  assert(k.pending.lits[0] == internalize(-4));
  assert(k.pending.lits[1] == internalize(-2));
  assert(k.pending.lits[2] == internalize(-3));
}

TEST(test_otfs) {
  // d = (-1 -2 -3) and r = (-1 -2 3) resolve to (-1 -2): OTFS strengthens r,
  // subsumes d and reuses the replacement as driver.
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Ref rref = add_external_clause(k, {-1, -2, 3});
  const Ref dref = add_external_clause(k, {-1, -2, -3});

  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(2));
  assert(propagate_apply(k) == ApplyResult::Conflict);
  assert(k.conflict_ref == dref);
  assert(k.stats.conflicts == 1);

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.conflicts == 2); // OTFS reanalysis is a Kissat conflict
  assert(k.stats.otfs_strengthened == 1);
  assert(k.stats.otfs_subsumed == 1);
  assert(clause_at(k.arena, dref)->garbage()); // conflict subsumed
  assert(k.stats.conflict_reused == 1); // replacement drives immediately
  assert(!k.pending.valid);             // no duplicate learned clause
  assert(k.level == 1);
  assert(k.values[internalize(-2)] > 0);
  assert(clause_at(k.arena, rref)->garbage()); // retired after reuse backtrack
  assert(k.otfs_retired.empty());
  assert(default_solve(k) == 10); // satisfiable: 1, -2
}

TEST(test_otfs_reuse_honors_chronological_gap) {
  // Asserting at level 103 with jump level 1: Kissat's 100-level reuse
  // threshold retains level 102 (Fmla conflict 844).
  Kernel k;
  kernel_reserve_vars(k, 104);
  k.conflict_reuse_chrono_levels = 100;
  add_external_clause(k, {-1, -2, 3});
  add_external_clause(k, {-1, -2, -3});

  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  for (int v = 4; v <= 104; v++) {
    kernel_assign_decision(k, internalize(v));
    assert(propagate_apply(k) == ApplyResult::Ok);
  }
  kernel_assign_decision(k, internalize(2));
  assert(k.level == 103);
  assert(propagate_apply(k) == ApplyResult::Conflict);

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.conflict_reused == 1);
  assert(k.stats.chronological == 1);
  assert(k.level == 102);
  assert(k.values[internalize(104)] > 0); // retained chronological prefix
  assert(k.values[internalize(-2)] > 0); // replacement's forced literal
  assert(k.assigned[var_of(internalize(2))].level == 1);
}

TEST(test_large_otfs_shrinks_reason_in_place) {
  // In-place OTFS strengthening (kissat large_on_the_fly_strengthen): same record,
  // first watch retained; added material covers only the surviving tail.
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit d[5] = {internalize(-1), internalize(-2), internalize(-3),
                    internalize(-4), internalize(-5)};
  const Ref rref = add_external_clause(k, {5, -2, -3, -4, -1});
  const Ref dref = kernel_add_large_clause(k, d, 5, true, 3);

  // Three replacement literals on level 2 prevent reuse in the second round.
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  kernel_assign(k, internalize(3), 2, internalize(-2), true);
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  kernel_assign(k, internalize(4), 2, internalize(-2), true);
  kernel_assign(k, internalize(5), 2, rref, false);
  k.conflict_ref = dref;
  k.conflict_valid = true;
  std::fill(k.touched.begin(), k.touched.end(), 0);
  std::fill(k.removed.begin(), k.removed.end(), 0);
  std::fill(k.factor_marks.begin(), k.factor_marks.end(), 0);
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), 0);
  const uint64_t touch_before = k.touch_epoch;
  kernel_clear_subsume_material(k);
  const uint64_t material_before = k.subsume_material_count;
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.otfs_strengthened == 1);
  assert(k.stats.conflict_reused == 0);
  assert(k.pending.valid);
  assert(!clause_at(k.arena, rref)->garbage());
  assert(clause_at(k.arena, rref)->size == 4);
  assert(clause_at(k.arena, rref)->shrunken());
  assert(clause_at(k.arena, rref)->lits[0] == internalize(-2) &&
         "the surviving first watch keeps its slot");
  assert(k.otfs_retired.empty());
  // Touch epochs: +1 the removed literal, +2..+4 the source tail, then the
  // redundant retirement and the learned binary.
  assert(k.touched[var_of(internalize(-3))] == touch_before + 2);
  assert(k.touched[var_of(internalize(-4))] == touch_before + 3);
  uint64_t removal_end = 0;
  for (Lit literal : d) {
    const uint64_t mark = k.removed[var_of(literal)];
    assert(mark > touch_before + 4);
    removal_end = std::max(removal_end, mark);
  }
  assert(k.touched[var_of(internalize(-2))] == removal_end + 1);
  assert(k.touched[var_of(internalize(-1))] == removal_end + 2);
  assert(!kernel_touched_since(k, var_of(internalize(5)), touch_before));
  assert(k.subsume_material_count == material_before + 4);
  for (Var v = 0; v < 5; ++v)
    assert(kernel_has_subsume_material(k, v) == (v < 4));
  assert(k.factor_marks[internalize(-3)]);
  assert(k.factor_marks[internalize(-4)]);
  assert(k.factor_marks[internalize(-1)]);
  assert(!k.factor_marks[internalize(5)]);
  for (Var v = 0; v < k.nvars; ++v)
    assert(k.bve_retry[v] == (v == var_of(internalize(5))));
  kernel_check_invariants(k); // both old reason and replacement are valid

  assert(backjump_apply_to(k, k.pending.jump_level) == ApplyResult::Ok);
  assert(!clause_at(k.arena, rref)->garbage());
  assert(k.otfs_retired.empty());
  kernel_check_invariants(k);
  assert(default_solve(k) == 10);
}

TEST(test_large_otfs_retains_root_false_first_watch) {
  // Kissat's large OTFS path keeps a root-false first watch; only the binary
  // path drops root literals, chosen by the non-root antecedent size.
  Kernel k;
  kernel_reserve_vars(k, 6);
  kernel_assign_unit(k, internalize(1));
  const Lit reason[5] = {internalize(-1), internalize(5), internalize(-2),
                         internalize(-3), internalize(-4)};
  const Ref reason_ref = kernel_add_large_clause(k, reason, 5, false, 0);
  const Ref conflict_ref = add_external_clause(k, {-5, -2, -3, -4}, true, 3);

  kernel_assign_decision(k, internalize(6));
  kernel_assign_decision(k, internalize(2));
  kernel_assign(k, internalize(3), 2, internalize(-2), true);
  kernel_assign(k, internalize(4), 2, internalize(-2), true);
  kernel_assign(k, internalize(5), 2, reason_ref, false);
  k.conflict_ref = conflict_ref;
  k.conflict_valid = true;

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.otfs_strengthened == 1);
  bool found = false;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && c->size == 4) {
      bool have[5] = {};
      for (const Lit lit : *c) {
        assert(negated(lit));
        have[var_of(lit)] = true;
      }
      if (have[0] && have[1] && have[2] && have[3]) {
        assert(!found);
        found = true;
      }
    }
    ref += clause_actual_words(c);
  }
  assert(found);
  assert(clause_at(k.arena, reason_ref)->size == 4 &&
         "the strengthened image lives in the reason's own record");
  assert(backjump_apply_to(k, k.pending.jump_level) == ApplyResult::Ok);
  assert(!clause_at(k.arena, reason_ref)->garbage());
  kernel_check_invariants(k);
  assert(default_solve(k) == 10);
}

// A redundant reason (7 ...) and conflict (-7 ...) of the same seven-variable
// shape with the given glues; analysis strengthens and subsumes. Returns the
// single surviving size-6 redundant clause.
static const Clause *otfs_glue_fixture(Kernel &k, uint32_t reason_glue,
                                       uint32_t conflict_glue, bool vivify) {
  kernel_reserve_vars(k, 7);
  const Ref reason_ref =
      add_external_clause(k, {7, -4, -6, -3, -5, -2, -1}, true, reason_glue);
  const Ref conflict_ref =
      add_external_clause(k, {-7, -4, -6, -3, -5, -2, -1}, true, conflict_glue);
  if (vivify) clause_at(k.arena, reason_ref)->set_vivify(true);
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(2));
  kernel_assign(k, internalize(5), 2, internalize(-2), true);
  kernel_assign_decision(k, internalize(3));
  kernel_assign(k, internalize(6), 3, internalize(-3), true);
  kernel_assign_decision(k, internalize(4));
  kernel_assign(k, internalize(7), 4, reason_ref, false);
  k.conflict_ref = conflict_ref;
  k.conflict_valid = true;
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.otfs_strengthened == 1);
  assert(k.stats.otfs_subsumed == 1);
  const Clause *found = nullptr;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && c->redundant() && c->size == 6) {
      assert(!found);
      found = c;
    }
    ref += clause_actual_words(c);
  }
  assert(found);
  return found;
}

// kissat_on_the_fly_subsume (strengthen.c) transfers the conflict's better
// glue 3 to the reason whose exact LBD is 4.
TEST(test_otfs_subsumption_inherits_better_conflict_glue) {
  Kernel k;
  assert(otfs_glue_fixture(k, 4, 3, false)->glue() == 3);
  assert(k.stats.promoted == 1);
  assert(default_solve(k) == 10);
}

// kissat large_on_the_fly_strengthen keeps an existing glue below the new size
// even when the levels would recompute a worse LBD.
TEST(test_otfs_strengthening_preserves_better_reason_glue) {
  Kernel k;
  const Clause *c = otfs_glue_fixture(k, 2, 6, true);
  assert(c->glue() == 2 && c->vivify());
  assert(default_solve(k) == 10);
}

TEST(test_chrono_backjump_clamps_to_jump_level) {
  // D-LEVEL-2: a chronological target above the assertion level is honored and
  // the UIP is stamped at jump_level.
  Kernel k;
  kernel_reserve_vars(k, 6);
  kernel_add_binary_clause(k, internalize(-1), internalize(6));
  kernel_add_binary_clause(k, internalize(-3), internalize(4));
  kernel_add_binary_clause(k, internalize(-3), internalize(5));
  const Ref cref = add_external_clause(k, {-6, -4, -5});
  kernel_assign_decision(k, internalize(1));
  kernel_assign(k, internalize(6), 1, internalize(-1), true);
  kernel_assign_decision(k, internalize(2));
  kernel_assign_decision(k, internalize(3));
  kernel_assign(k, internalize(4), 3, internalize(-3), true);
  kernel_assign(k, internalize(5), 3, internalize(-3), true);
  k.conflict_ref = cref;
  k.conflict_valid = true;

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.pending.lits.size() == 2);
  assert(k.pending.lits[0] == internalize(-3)); // negated UIP
  assert(k.pending.lits[1] == internalize(-6));
  assert(k.pending.jump_level == 1);
  assert(backjump_apply_to(k, 2) == ApplyResult::Ok); // chrono target HONORED
  assert(k.level == 2);                    // backtracked to 2, not down to 1
  assert(k.values[internalize(2)] > 0);    // unrelated level-2 decision KEPT
  assert(k.values[internalize(-3)] > 0);   // UIP asserted...
  assert(k.assigned[var_of(internalize(3))].level == 1); // ...at its TRUE level
  assert(!k.pending.valid);
  assert(default_solve(k) == 10); // still satisfiable
}

TEST(test_backjump_target_clamps) {
  // a target below the assertion level clamps up: the clause must stay
  // asserting
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(-1), internalize(5));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  const Ref cref = add_external_clause(k, {-5, -3, -4});
  kernel_assign_decision(k, internalize(1));
  kernel_assign(k, internalize(5), 1, internalize(-1), true);
  kernel_assign_decision(k, internalize(2));
  kernel_assign(k, internalize(3), 2, internalize(-2), true);
  kernel_assign(k, internalize(4), 2, internalize(-2), true);
  k.conflict_ref = cref;
  k.conflict_valid = true;
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.pending.jump_level == 1);
  assert(backjump_apply_to(k, 0) == ApplyResult::Ok); // clamps to 1
  assert(k.level == 1);
  assert(k.values[internalize(-2)] > 0); // asserted at the jump level
  assert(default_solve(k) == 10);
}

// --- the conflict-reuse shortcut and its watch repair ----------------------


// A reused conflict whose watches are not on its two highest-level literals
// has its watches repaired (built white-box, as only chrono produces it).
TEST(test_conflict_reuse_repairs_watches) {
  Kernel k;
  ScopedHeuristics hg(k);
  kernel_reserve_vars(k, 4);
  k.h->stable = true;
  const double scinc_before = k.h->scinc;
  k.conflict_reuse = true;
  for (int v = 1; v <= 4; v++) { // levels 1..4, one decision each
    kernel_assign_decision(k, internalize(v));
    assert(propagate_apply(k) == ApplyResult::Ok);
  }
  // watches land on -1 (level 1) and -2 (level 2); the two highest levels are
  // -4 (the unique conflict-level literal) and -3
  const Ref ref = add_external_clause(k, {-1, -2, -3, -4}, true, 2);
  assert(watched_by(k, internalize(-1), ref));
  assert(watched_by(k, internalize(-2), ref));
  k.conflict_valid = true; // the clause is falsified: record it as the conflict
  k.conflict_ref = ref;

  assert(analyze_guard(k));
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.conflict_reused == 1);        // the shortcut fired...
  assert(k.stats.conflict_watch_repaired == 1); // ...and needed the repair
  assert(k.stats.used_glue[1][2] == 0 &&
         "an already-asserting conflict bypasses Kissat Deduce/mark-used");
  assert(!k.pending.valid);                     // nothing learned, nothing to assert
  assert(k.h->scinc == scinc_before &&
         "an empty Kissat analyzed stack must not advance EVSIDS decay");

  const Clause *c = clause_at(k.arena, ref);
  assert(c->lits[0] == internalize(-4));                 // the forced literal
  assert(k.var_level(var_of(c->lits[1])) == 3);          // ...paired at jump2
  assert(watched_by(k, internalize(-4), ref));
  assert(watched_by(k, c->lits[1], ref));
  assert(!watched_by(k, internalize(-1), ref)); // old watches transferred
  assert(!watched_by(k, internalize(-2), ref));

  assert(k.level == 3);                       // backtracked to jump2
  assert(k.values[internalize(-4)] > 0);      // forced, with the conflict...
  assert(k.assigned[var_of(internalize(4))].reason == ref); // ...as its reason
  assert(k.assigned[var_of(internalize(4))].level == 3);
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_check_invariants(k);

  // Dropping below jump2 and re-falsifying the level-3 literal must propagate
  // through the repaired pair; the original pair fires on nothing.
  kernel_backtrack(k, 2);
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_check_invariants(k);
  assert(k.values[internalize(-4)] == 0);
  kernel_assign_decision(k, internalize(3));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.values[internalize(-4)] > 0); // derived through the repaired watch
  kernel_check_invariants(k);
  assert(k.stats.backjump_stale == 0);
}

TEST(test_conflict_repair_after_fixed_prefix_watch_flush) {
  // Sparse collection drops the root-fixed watch; N1 decisions before
  // propagation then need the absent watch promoted.
  for (unsigned fixed_slot = 0; fixed_slot < 2; ++fixed_slot) {
    Kernel k;
    k.conflict_reuse = true;
    kernel_reserve_vars(k, 6);
    Lit prefix[3] = {internalize(-1), internalize(2), internalize(3)};
    if (fixed_slot) std::swap(prefix[0], prefix[1]);
    const Ref ref = kernel_add_large_clause(k, prefix, 3, false, 0);
    const Ref start = add_external_clause(k, {4, 5, 6}, true, 3);
    kernel_assign_unit(k, internalize(1));
    kernel_delete_clause(k, clause_at(k.arena, start));
    kernel_collect_garbage(k, start);
    assert(!watched_by(k, internalize(-1), ref));
    assert(watched_by(k, internalize(2), ref));
    kernel_check_invariants(k); // this singly watched prefix is legal

    kernel_assign_decision(k, internalize(-2));
    kernel_assign_decision(k, internalize(-3));
    assert(propagate_apply(k) == ApplyResult::Conflict);
    assert(k.conflict_ref == ref);
    assert(analyze_apply(k) == ApplyResult::Ok);
    assert(k.stats.conflict_reused == 1);
    assert(k.stats.conflict_watch_repaired == 1);
    assert(k.level == 1 && k.values[internalize(3)] > 0);
    assert(watched_by(k, internalize(2), ref));
    assert(watched_by(k, internalize(3), ref));
    assert(!watched_by(k, internalize(-1), ref));
    assert(propagate_apply(k) == ApplyResult::Ok);
    kernel_check_invariants(k);

    kernel_backtrack(k, 0);
    assert(propagate_apply(k) == ApplyResult::Ok);
    kernel_assign_decision(k, internalize(-2));
    assert(propagate_apply(k) == ApplyResult::Ok);
    assert(k.values[internalize(3)] > 0);
    kernel_check_invariants(k);
  }
}

TEST(test_binary_conflict_reuse_jumps_reason) {
  // Binary conflict reuse jumps reasons unconditionally (Kissat): asserting -3
  // at level 1 records the direct reason -1.
  Kernel k;
  kernel_reserve_vars(k, 3);
  k.conflict_reuse = true;
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(k.values[internalize(2)] > 0);
  kernel_assign_decision(k, internalize(3));
  kernel_add_binary_clause(k, internalize(-3), internalize(-2));
  k.conflict_ref = INVALID_REF;
  k.conflict_bin[0] = internalize(-3);
  k.conflict_bin[1] = internalize(-2);
  k.conflict_valid = true;
  assert(analyze_apply(k) == ApplyResult::Ok);
  const Var v = var_of(internalize(3));
  assert(k.stats.conflict_reused == 1);
  assert(k.values[internalize(-3)] > 0);
  assert(k.assigned[v].binary && k.assigned[v].jumped);
  assert(k.assigned[v].reason == internalize(-1));
  assert(k.jumped_over[v] == internalize(-2));
}

TEST(test_nonreusable_conflict_repairs_watches) {
  // Kissat promotes the two highest-level literals even when the conflict
  // cannot be reused.
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(-3), internalize(4));
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(2));
  kernel_assign_decision(k, internalize(3));
  kernel_assign(k, internalize(4), 3, internalize(-3), true);
  const Ref ref = add_external_clause(k, {-1, -2, -3, -4});
  k.conflict_ref = ref;
  k.conflict_valid = true;
  k.conflict_reuse = false; // watch canonicalization is independent of reuse

  assert(analyze_apply(k) == ApplyResult::Ok);
  const Clause *c = clause_at(k.arena, ref);
  assert(c->lits[0] == internalize(-3));
  assert(c->lits[1] == internalize(-4));
  assert(k.stats.conflict_watch_repaired > 0);
}

TEST(test_forced_conflict_unwind_preserves_two_stage_heap_pose) {
  Kernel k;
  ScopedHeuristics hg(k);
  kernel_reserve_vars(k, 6);
  k.h->stable = true;
  heuristics_update_scores(k);

  // Remove each assigned variable lazily from the stable heap. Variables 4
  // and 5 retain equal scores, so their reinsertion order is observable.
  for (Var v = 0; v < 5; ++v) {
    heuristics_score_set(k, v, v >= 3 ? 100.0 : 10.0 + v);
    kernel_assign_decision(k, make_lit(v, false));
    (void) heuristics_heap_next_unassigned(k);
  }
  assert(!heuristics_heap_contains(k, 3));
  assert(!heuristics_heap_contains(k, 4));

  // Conflict at level 2 from level 5: Kissat unwinds level 5, snapshots, then
  // unwinds to 2, keeping the equal-score pose.
  k.conflict_bin[0] = negate(make_lit(1, false));
  k.conflict_bin[1] = negate(make_lit(1, false));
  k.conflict_ref = INVALID_REF;
  k.conflict_valid = true;
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.h->scores.pos[4] < k.h->scores.pos[3]);
}

// The reuse shortcut leaves no pending record; `backjump_stale` stays 0.
TEST(test_conflict_reuse_under_default_policy) {
  // Chrono is required to reach reuse; the reachability asserts below check it.
  uint64_t total_reused = 0, total_repaired = 0;
  for (int seed = 1; seed <= 6; seed++) {
    Kernel k;
    ScopedHeuristics hg(k);
    const uint32_t N = 90;
    kernel_reserve_vars(k, N);
    uint64_t rng = static_cast<uint64_t>(seed) * 0x9E3779B97F4A7C15ull + 1;
    auto next = [&]() {
      rng ^= rng >> 12;
      rng ^= rng << 25;
      rng ^= rng >> 27;
      return rng * 0x2545F4914F6CDD1Dull;
    };
    for (int i = 0; i < 430; i++) { // m/n ~ 4.8: UNSAT side, still quick
      Lit ls[3];
      for (int j = 0; j < 3; j++) {
        const Var v = static_cast<Var>(next() % N);
        ls[j] = make_lit(v, (next() & 1) != 0);
      }
      if (var_of(ls[0]) == var_of(ls[1]) || var_of(ls[0]) == var_of(ls[2]) ||
          var_of(ls[1]) == var_of(ls[2]))
        continue; // no duplicate/tautological literals
      kernel_add_large_clause(k, ls, 3, false, 0);
    }
    k.conflict_reuse = true;
    DefaultPolicy policy;
    policy.chrono_levels = 0; // always chronological
    const int r = policy.solve(k);
    assert(r == 10 || r == 20);
    assert(k.stats.backjump_stale == 0); // the standing invariant
    total_reused += k.stats.conflict_reused;
    total_repaired += k.stats.conflict_watch_repaired;
  }
  assert(total_reused > 0);   // the arm was actually exercised...
  assert(total_repaired > 0); // ...including the un-crippled (repair) path
}

// analyze_eager_subsume on a hand-built register: a clause naming a trail
// reason is not deleted.
TEST(test_eager_subsume) {
  auto fresh = [](Kernel &k, uint32_t nvars) { kernel_reserve_vars(k, nvars); };

  { // (a) a strictly smaller learned clause deletes the register entry
    Kernel k;
    fresh(k, 8);
    const Lit big[4] = {internalize(1), internalize(2), internalize(3),
                        internalize(4)};
    const Ref victim = kernel_add_large_clause(k, big, 4, true, 4, 101);
    const Lit small[3] = {internalize(1), internalize(2), internalize(3)};
    const Ref fresh_ref = kernel_add_large_clause(k, small, 3, true, 3, 102);
    k.eager_recent[0] = victim;
    k.pending.valid = true;
    k.pending.ref = fresh_ref;
    k.pending.lits.assign(small, small + 3);
    analyze_eager_subsume(k);
    assert(k.stats.eager_tried == 1);
    assert(k.stats.eager_subsumed == 1);
    assert(clause_at(k.arena, victim)->garbage());
    assert(k.eager_recent[0] == fresh_ref); // dead slot compacted, then insert
    for (const uint8_t m : k.eager_marks)
      assert(m == 0); // touched-restore left the scratch clean
  }
  { // (b) equal size / not contained / irredundant are all KEPT
    Kernel k;
    fresh(k, 8);
    const Lit other[4] = {internalize(1), internalize(2), internalize(5),
                          internalize(6)};
    const Ref r_same = add_external_clause(k, {1, 2, 3}, true, 3, 101);
    const Ref r_other = kernel_add_large_clause(k, other, 4, true, 4, 102);
    const Ref r_irr = add_external_clause(k, {1, 2, 3, 4}, false, 0, 103);
    k.eager_recent[0] = r_same;
    k.eager_recent[1] = r_other;
    k.eager_recent[2] = r_irr;
    k.pending.valid = true;
    k.pending.ref = INVALID_REF; // a learned BINARY subsumes too
    k.pending.lits = {internalize(1), internalize(2), internalize(3)};
    analyze_eager_subsume(k);
    assert(k.stats.eager_subsumed == 0);
    assert(k.stats.eager_tried == 1); // only `other` is larger AND redundant
    assert(!clause_at(k.arena, r_same)->garbage());
    assert(!clause_at(k.arena, r_other)->garbage());
    assert(!clause_at(k.arena, r_irr)->garbage());
  }
  { // (c) the reason guard: a subsumed candidate that is still a trail
    // reason is kept, and the deviation from kissat is counted
    Kernel k;
    fresh(k, 8);
    const Lit big[4] = {internalize(1), internalize(2), internalize(3),
                        internalize(4)};
    const Ref victim = kernel_add_large_clause(k, big, 4, true, 4, 101);
    kernel_assign_decision(k, internalize(-1));      // level 1
    kernel_assign(k, internalize(4), 1, victim, false); // forced BY the victim
    k.eager_recent[0] = victim;
    k.pending.valid = true;
    k.pending.ref = INVALID_REF;
    k.pending.lits = {internalize(1), internalize(2), internalize(3)};
    analyze_eager_subsume(k);
    assert(k.stats.eager_tried == 1);
    assert(k.stats.eager_reason_skipped == 1);
    assert(k.stats.eager_subsumed == 0);
    assert(!clause_at(k.arena, victim)->garbage()); // reason survived
    assert(k.eager_recent[0] == victim);
  }
  { // (d) the register is reset by garbage collection — it holds raw Refs and
    // the collect relocates every one (kissat_reset_last_learned)
    Kernel k;
    fresh(k, 8);
    const Lit a[3] = {internalize(1), internalize(2), internalize(3)};
    const Lit b[3] = {internalize(4), internalize(5), internalize(6)};
    const Ref dead = kernel_add_large_clause(k, a, 3, true, 3, 101);
    const Ref live = kernel_add_large_clause(k, b, 3, true, 3, 102);
    k.eager_recent[0] = live;
    kernel_delete_clause(k, clause_at(k.arena, dead));
    kernel_collect_garbage(k); // `live` moves from offset 8 to offset 0
    assert(k.eager_recent[0] == INVALID_REF);
    kernel_check_invariants(k); // the stale-register assertion
  }
  { // (e) an implementation-only packing pass forwards live entries and
    // invalidates garbage ones, matching a reference operation with no GC
    Kernel k;
    fresh(k, 8);
    const Ref dead = add_external_clause(k, {1, 2, 3}, true, 3, 101);
    const Ref live = add_external_clause(k, {4, 5, 6}, true, 3, 102);
    k.eager_recent[0] = live;
    k.eager_recent[1] = dead;
    kernel_delete_clause(k, clause_at(k.arena, dead));
    kernel_collect_garbage(k, 0, /*sparse_collection=*/false,
                           /*preserve_eager_recent=*/true);
    assert(k.eager_recent[0] == 0); // `live` moved from offset 8 to offset 0
    assert(k.eager_recent[1] == INVALID_REF);
    kernel_check_invariants(k);
  }
  { // (f) eager_subsume = 0 is byte-identical to not having the mechanism
    Kernel k;
    fresh(k, 8);
    k.eager_subsume = 0;
    const Ref victim = add_external_clause(k, {1, 2, 3, 4}, true, 4, 101);
    k.eager_recent[0] = victim;
    k.pending.valid = true;
    k.pending.ref = INVALID_REF;
    k.pending.lits = {internalize(1), internalize(2), internalize(3)};
    analyze_eager_subsume(k);
    assert(k.stats.eager_tried == 0 && k.stats.eager_subsumed == 0);
    assert(!clause_at(k.arena, victim)->garbage());
  }
  { // (g) reachability under DefaultPolicy, so the arm is not vacuous where it
    // actually ships. Same random-3-SAT bed as the conflict-reuse test.
    uint64_t subsumed = 0, tried = 0;
    for (int seed = 1; seed <= 6; seed++) {
      Kernel k;
      ScopedHeuristics hg(k);
      const uint32_t N = 90;
      kernel_reserve_vars(k, N);
      uint64_t rng = static_cast<uint64_t>(seed) * 0x9E3779B97F4A7C15ull + 1;
      auto next = [&]() {
        rng ^= rng >> 12;
        rng ^= rng << 25;
        rng ^= rng >> 27;
        return rng * 0x2545F4914F6CDD1Dull;
      };
      for (int i = 0; i < 430; i++) {
        Lit ls[3];
        for (int j = 0; j < 3; j++) {
          const Var v = static_cast<Var>(next() % N);
          ls[j] = make_lit(v, (next() & 1) != 0);
        }
        if (var_of(ls[0]) == var_of(ls[1]) || var_of(ls[0]) == var_of(ls[2]) ||
            var_of(ls[1]) == var_of(ls[2]))
          continue;
        kernel_add_large_clause(k, ls, 3, false, 0);
      }
      DefaultPolicy policy;
      const int r = policy.solve(k);
      assert(r == 10 || r == 20);
      subsumed += k.stats.eager_subsumed;
      tried += k.stats.eager_tried;
    }
    assert(tried > 0);
    assert(subsumed > 0);
  }
}

// The public per-call minimization depth: Kissat's minimizedepth default 1e3.
TEST(test_minimize_depth_public_config) {
  tactix_c_handle *solver = tactix_init();
  assert(tactix_config(solver, TACTIX_CFG_ANALYZE_MINIMIZE_DEPTH) == 1000);
  for (uint32_t value : {0u, 1001u, UINT32_MAX}) {
    tactix_set_analyze_minimize_depth(solver, value);
    assert(tactix_config(solver, TACTIX_CFG_ANALYZE_MINIMIZE_DEPTH) == value);
  }
  tactix_release(solver);
}

int main(int argc, char **argv) {
  run_tests();
  if (argc == 2)
    write_level_one_failed_literal_proof(argv[1]);
  puts("test_analyze OK");
  return 0;
}
