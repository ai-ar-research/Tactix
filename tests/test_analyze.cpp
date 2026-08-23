// Analyze/Backjump: forced scenarios with a known 1-UIP clause and jump level.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

static void test_uip() {
  // At level 2, 4 and 5 both follow decision 2.  The conflict
  // (-3,-4,-5) resolves through their binary reasons to (-2,-3).
  // Binary reasons keep this ordinary learn/backjump fixture separate from
  // the dedicated OTFS replacement-and-reuse test below.
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  kernel_add_binary_clause(k, internalize(-2), internalize(5));
  const Lit conflict[3] = {internalize(-3), internalize(-4),
                           internalize(-5)};
  const Ref cref = kernel_add_large_clause(k, conflict, 3, false, 0);
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
  assert(backjump_apply(k) == ApplyResult::Ok);
  assert(k.level == 1);
  assert(k.values[internalize(-2)] > 0); // asserted
  assert(k.assigned[var_of(internalize(-2))].jumped &&
         k.stats.jumped_reasons == 1 &&
         "Kissat's learned-binary assignment is not classification-gated");
  assert(!k.pending.valid);

  assert(default_solve(k) == 10); // still satisfiable: 1, -2, 3
}

// D-SPLIT: Analyze and Backjump are fused into the atomic ConflictAnalysis
// rule, so `pending` is consumed within one apply and (conflict_valid &&
// pending.valid) is unrepresentable — hence no !pending.valid in
// propagate_guard.
static void test_conflictanalysis_atomic() {
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit c1[3] = {internalize(-1), internalize(-2), internalize(3)};
  const Lit c2[3] = {internalize(-1), internalize(-2), internalize(-3)};
  kernel_add_large_clause(k, c1, 3, false, 0);
  kernel_add_large_clause(k, c2, 3, false, 0);
  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(2));
  assert(propagate_apply(k) == ApplyResult::Conflict);

  assert(tactix_rule_id("Analyze") < 0);
  assert(tactix_rule_id("Backjump") < 0);
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

static void test_learned_unit() {
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

static void test_bump_admission_order_is_not_cleanup_order() {
  Kernel k;
  ScopedHeuristics hg(k);
  kernel_reserve_vars(k, 4);
  k.h->stable = true;
  heuristics_update_scores(k);

  // Variable 0 models a literal touched first as minimization scratch and
  // admitted to the bump set only after 1 and 2.  Cleanup order must not move
  // that late admission ahead of them.  Equal EVSIDS increments make the
  // stable heap root an exact, witness-independent observation of the order.
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

// Kissat's level-one failed-literal analysis walks the implication graph past
// every singleton frontier, rather than stopping after one ordinary 1-UIP.
// Under decision 1, literal 2 implies both 3 and 4, whose binary conflict
// proves first -2 and then -1. Both become true root units before the analyzed
// variables are bumped, so neither can re-enter the focused decision queue.
static void test_level_one_failed_literal_batch() {
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

// Proof-producing twin of the batch fixture.  The first four clauses produce
// the two-unit failed-literal batch under decision 1; the final two make the
// opposite branch inconsistent, so root propagation closes a complete UNSAT
// proof that an external LRAT checker can replay.
static void write_level_one_failed_literal_lrat(const char *path) {
  Proof proof;
  assert(proof.open(path, ProofFormat::Lrat));
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

static void test_shrink() {
  // Neither literal of the level-1 block {-2, -3} is removable by plain
  // minimization, but the block's walk converges on the decision 1, so
  // shrinking learns (-4 -1).
  Kernel k;
  kernel_reserve_vars(k, 6);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  const Lit c3[4] = {internalize(-4), internalize(-2), internalize(-3),
                     internalize(5)};
  kernel_add_large_clause(k, c3, 4, false, 0);
  const Lit c4[3] = {internalize(-4), internalize(-5), internalize(6)};
  kernel_add_large_clause(k, c4, 3, false, 0);
  const Lit c5[4] = {internalize(-6), internalize(-5), internalize(-2),
                     internalize(-3)};
  kernel_add_large_clause(k, c5, 4, false, 0);

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
  assert(backjump_apply(k) == ApplyResult::Ok);
  assert(default_solve(k) == 10); // still satisfiable (1 2 3, -4)
}

static void test_shrink_accepts_lower_learned_literal() {
  // The level-2 block {-b,-c} converges to b.  Resolving c reaches -a on the
  // lower level, but -a is itself already in the learned clause and therefore
  // closes that branch.  Kissat marks all learned literals removable before
  // shrink=3; Tactix represents the same fact with SEEN.
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_assign_decision(k, internalize(1)); // a @ 1
  kernel_assign_decision(k, internalize(2)); // b @ 2
  const Lit reason[3] = {internalize(-2), internalize(-1), internalize(3)};
  const Ref reason_ref = kernel_add_large_clause(k, reason, 3, false, 0);
  kernel_assign(k, internalize(3), 2, reason_ref, false); // c @ 2
  kernel_assign_decision(k, internalize(4));              // d @ 3
  const Lit conflict[4] = {internalize(-4), internalize(-2),
                           internalize(-3), internalize(-1)};
  k.conflict_ref = kernel_add_large_clause(k, conflict, 4, false, 0);
  k.conflict_valid = true;

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.shrunken == 1);
  assert(k.pending.lits.size() == 3);
  assert(k.pending.lits[0] == internalize(-4));
  assert(k.pending.lits[1] == internalize(-2));
  assert(k.pending.lits[2] == internalize(-1));
  assert(k.pending.jump_level == 2);
}

static void test_shrink_crosses_unmarked_lower_level() {
  // The level-3 block {-b,-c} converges to b.  Resolving c first reaches -y
  // on level 2, which is not represented in the learned clause; y's binary
  // reason then reaches the learned anchor -a on level 1.  Kissat follows the
  // complete reason chain across that intermediate level.  Rejecting y just
  // because level 2 is absent keeps a strictly weaker learned clause.
  Kernel k;
  kernel_reserve_vars(k, 6);
  kernel_assign_decision(k, internalize(1)); // a @ 1
  kernel_assign_decision(k, internalize(2)); // unused level-2 decision
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_assign(k, internalize(3), 2, internalize(-1), true); // y @ 2
  kernel_assign_decision(k, internalize(4));                  // b @ 3
  const Lit reason[3] = {internalize(-4), internalize(-3),
                         internalize(5)};
  const Ref reason_ref = kernel_add_large_clause(k, reason, 3, false, 0);
  kernel_assign(k, internalize(5), 3, reason_ref, false); // c @ 3
  kernel_assign_decision(k, internalize(6));              // d @ 4
  const Lit conflict[4] = {internalize(-6), internalize(-4),
                           internalize(-5), internalize(-1)};
  k.conflict_ref = kernel_add_large_clause(k, conflict, 4, false, 0);
  k.conflict_valid = true;

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.shrunken == 1);
  assert(k.pending.lits.size() == 3);
  assert(k.pending.lits[0] == internalize(-6));
  assert(k.pending.lits[1] == internalize(-4));
  assert(k.pending.lits[2] == internalize(-1));
  assert(k.pending.jump_level == 3);
}

static void test_shrink_processes_lower_blocks_first() {
  // The lower block {-q,-r} converges to y and makes that walk removable.
  // Only then can the higher block {-b,-c}, whose c-reason reaches lower-level
  // y, converge to b.  Highest-first processing keeps -c; Kissat's
  // tail-to-head block order learns the strict subclause {-d,-b,-y}.
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
  const Lit reason[3] = {internalize(-5), internalize(-2),
                         internalize(6)};
  const Ref reason_ref = kernel_add_large_clause(k, reason, 3, false, 0);
  kernel_assign(k, internalize(6), 2, reason_ref, false); // c @ 2
  kernel_assign_decision(k, internalize(7));              // d @ 3
  const Lit conflict[5] = {internalize(-7), internalize(-5),
                           internalize(-6), internalize(-3),
                           internalize(-4)};
  k.conflict_ref = kernel_add_large_clause(k, conflict, 5, false, 0);
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
  // At level 2, c follows from a through (-a c).  The conflict learns
  // (-d -c -a), where -c is the sole literal on its level and is redundant
  // through -a.  Kissat shrink=3 skips that singleton block, so reference
  // parity requires retaining -c without dereferencing its reason.
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(2));
  // White-box N1 schedule: assign c at level 2 from the binary (-a c), even
  // though ordinary propagation would have assigned it at level 1.  Analyze
  // must remain sound for this public-state schedule.
  kernel_assign(k, internalize(3), 2, internalize(-1), true);
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_assign_decision(k, internalize(4));
  const Lit conflict[3] = {internalize(-4), internalize(-3),
                           internalize(-1)};
  k.conflict_ref = kernel_add_large_clause(k, conflict, 3, false, 0);
  k.conflict_valid = true;
  assert(analyze_apply(k) == ApplyResult::Ok);
  return k;
}

static void test_singleton_block_skips_minimization() {
  Kernel k = singleton_block_fixture();
  assert(k.pending.lits.size() == 3);
  assert(k.pending.lits[0] == internalize(-4));
  assert(k.pending.lits[1] == internalize(-3));
  assert(k.pending.lits[2] == internalize(-1));
  assert(k.pending.jump_level == 2);
  assert(k.stats.minimized == 0);
}

static Kernel minimization_depth_fixture(uint32_t chain, bool binary) {
  // a is the learned anchor, z is a root-false side literal in every large
  // reason, and x[0..chain) imply one another from a.  The second level-2
  // literal y depends on the absent lower-level decision c, which makes block
  // shrinking fail and admits literal-by-literal minimization.  Thus -x.back()
  // is redundant exactly when its reason walk reaches the already-learned -a
  // before the depth cap, while -y remains in the clause.
  assert(chain > 0);
  const uint32_t a = 1, z = 2, c = 3, b = 4, first_x = 5;
  const uint32_t y = first_x + chain, d = y + 1;
  Kernel k;
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

static void test_minimization_depth_matches_kissat() {
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
}

static Kernel learned_stable_order_fixture() {
  // The learned level-2 block is inserted as {-b,-c}, while c has the later
  // trail position.  Its shrink walk fails through the lower-level decision
  // a, leaving the block intact and isolating the within-level ordering rule.
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(2));
  kernel_assign(k, internalize(3), 2, internalize(-1), true);
  kernel_add_binary_clause(k, internalize(-1), internalize(3));
  kernel_assign_decision(k, internalize(4));
  const Lit conflict[3] = {internalize(-4), internalize(-2),
                           internalize(-3)};
  k.conflict_ref = kernel_add_large_clause(k, conflict, 3, false, 0);
  k.conflict_valid = true;
  assert(analyze_apply(k) == ApplyResult::Ok);
  return k;
}

static void test_learned_level_stable_order() {
  Kernel k = learned_stable_order_fixture();
  assert(k.pending.lits.size() == 3);
  assert(k.pending.lits[0] == internalize(-4));
  assert(k.pending.lits[1] == internalize(-2));
  assert(k.pending.lits[2] == internalize(-3));
}

static void test_otfs() {
  // Resolving d = (-1 -2 -3) with its reason r = (-1 -2 3) yields (-1 -2),
  // smaller than both: OTFS strengthens r, subsumes d, and reuses the
  // replacement as its own driver instead of learning an exact duplicate.
  Kernel k;
  kernel_reserve_vars(k, 3);
  const Lit r[3] = {internalize(-1), internalize(-2), internalize(3)};
  const Lit d[3] = {internalize(-1), internalize(-2), internalize(-3)};
  const Ref rref = kernel_add_large_clause(k, r, 3, false, 0);
  const Ref dref = kernel_add_large_clause(k, d, 3, false, 0);

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

static void test_otfs_reuse_honors_chronological_gap() {
  // The OTFS replacement is asserting at level 103 with jump level 1.  With
  // Kissat's 100-level threshold, reuse retains level 102 instead of jumping
  // directly to 1.  This is the deep-gap form reached by Fmla conflict 844.
  Kernel k;
  kernel_reserve_vars(k, 104);
  k.conflict_reuse_chrono_levels = 100;
  const Lit r[3] = {internalize(-1), internalize(-2), internalize(3)};
  const Lit d[3] = {internalize(-1), internalize(-2), internalize(-3)};
  kernel_add_large_clause(k, r, 3, false, 0);
  kernel_add_large_clause(k, d, 3, false, 0);

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

static void test_large_otfs_retires_replaced_reason() {
  // The five-literal reason is strengthened to four literals.  It must stay a
  // valid reason during Analyze's white-box half, but not survive the fused
  // backjump alongside its strict replacement.
  Kernel k;
  kernel_reserve_vars(k, 5);
  const Lit r[5] = {internalize(5), internalize(-2), internalize(-3),
                    internalize(-4), internalize(-1)};
  const Lit d[5] = {internalize(-1), internalize(-2), internalize(-3),
                    internalize(-4), internalize(-5)};
  const Ref rref = kernel_add_large_clause(k, r, 5, false, 0);
  const Ref dref = kernel_add_large_clause(k, d, 5, false, 0);

  // Keep three replacement literals on level 2, so the second analysis round
  // cannot reuse the strengthened conflict and the Analyze/Backjump lifetime
  // seam remains observable in this white-box fixture.
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(2));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  kernel_assign(k, internalize(3), 2, internalize(-2), true);
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  kernel_assign(k, internalize(4), 2, internalize(-2), true);
  kernel_assign(k, internalize(5), 2, rref, false);
  k.conflict_ref = dref;
  k.conflict_valid = true;
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.otfs_strengthened == 1);
  assert(k.stats.conflict_reused == 0);
  assert(k.pending.valid);
  assert(!clause_at(k.arena, rref)->garbage());
  assert(k.otfs_retired.size() == 1 && k.otfs_retired[0] == rref);
  kernel_check_invariants(k); // both old reason and replacement are valid

  assert(backjump_apply(k) == ApplyResult::Ok);
  assert(clause_at(k.arena, rref)->garbage());
  assert(k.otfs_retired.empty());
  kernel_check_invariants(k);
  assert(default_solve(k) == 10);
}

static void test_otfs_subsumption_inherits_better_conflict_glue() {
  // Kissat's on-the-fly subsumption transfers a better redundant-conflict LBD
  // to the strengthened redundant reason (strengthen.c:
  // kissat_on_the_fly_subsume).  The assignment levels deliberately leave the
  // strengthened reason with exact LBD 4, while the subsumed conflict already
  // carries glue 3, so only that transfer can produce the expected result.
  Kernel k;
  kernel_reserve_vars(k, 7);
  const Lit reason[7] = {internalize(7),  internalize(-4),
                         internalize(-6), internalize(-3),
                         internalize(-5), internalize(-2),
                         internalize(-1)};
  const Lit conflict[7] = {internalize(-7), internalize(-4),
                           internalize(-6), internalize(-3),
                           internalize(-5), internalize(-2),
                           internalize(-1)};
  const Ref reason_ref = kernel_add_large_clause(k, reason, 7, true, 4);
  const Ref conflict_ref = kernel_add_large_clause(k, conflict, 7, true, 3);

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
  assert(k.stats.promoted == 1);

  bool found = false;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && c->redundant() && c->size == 6) {
      assert(!found);
      found = true;
      assert(c->glue() == 3);
    }
    ref += clause_actual_words(c);
  }
  assert(found);
  assert(default_solve(k) == 10);
}

static void test_otfs_strengthening_preserves_better_reason_glue() {
  // Kissat's large_on_the_fly_strengthen mutates the reason in place.  Its
  // existing glue survives when it is already below the new size, even if the
  // current assignment levels would recompute to a worse LBD.  A packed
  // replacement must preserve that logical metadata before considering the
  // independent better-conflict transfer pinned above.
  Kernel k;
  kernel_reserve_vars(k, 7);
  const Lit reason[7] = {internalize(7),  internalize(-4),
                         internalize(-6), internalize(-3),
                         internalize(-5), internalize(-2),
                         internalize(-1)};
  const Lit conflict[7] = {internalize(-7), internalize(-4),
                           internalize(-6), internalize(-3),
                           internalize(-5), internalize(-2),
                           internalize(-1)};
  const Ref reason_ref = kernel_add_large_clause(k, reason, 7, true, 2);
  const Ref conflict_ref = kernel_add_large_clause(k, conflict, 7, true, 6);

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

  bool found = false;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && c->redundant() && c->size == 6) {
      assert(!found);
      found = true;
      assert(c->glue() == 2);
    }
    ref += clause_actual_words(c);
  }
  assert(found);
  assert(default_solve(k) == 10);
}

static void test_chrono_backjump_clamps_to_jump_level() {
  // D-LEVEL-2: a chronological target above the assertion level is honored,
  // and true-level assignment stamps the UIP at jump_level whatever the target,
  // so stopping at level 2 keeps that decision without stranding the clause.
  Kernel k;
  kernel_reserve_vars(k, 6);
  kernel_add_binary_clause(k, internalize(-1), internalize(6));
  kernel_add_binary_clause(k, internalize(-3), internalize(4));
  kernel_add_binary_clause(k, internalize(-3), internalize(5));
  const Lit conflict[3] = {internalize(-6), internalize(-4),
                           internalize(-5)};
  const Ref cref = kernel_add_large_clause(k, conflict, 3, false, 0);
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

static void test_backjump_target_clamps() {
  // a target below the assertion level clamps up: the clause must stay
  // asserting
  Kernel k;
  kernel_reserve_vars(k, 5);
  kernel_add_binary_clause(k, internalize(-1), internalize(5));
  kernel_add_binary_clause(k, internalize(-2), internalize(3));
  kernel_add_binary_clause(k, internalize(-2), internalize(4));
  const Lit conflict[3] = {internalize(-5), internalize(-3),
                           internalize(-4)};
  const Ref cref = kernel_add_large_clause(k, conflict, 3, false, 0);
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

static bool watched_by(const Kernel &k, Lit l, Ref ref) {
  const WatchHead &h = k.watches.heads[l];
  const uint32_t *ws = k.watches.arena.data() + h.offset;
  for (uint32_t i = 0; i < h.size;) {
    if (Watch{ws[i]}.binary()) {
      i += 1;
    } else {
      if (ws[i + 1] == ref)
        return true;
      i += 2;
    }
  }
  return false;
}

// One conflict literal at the conflict level, with the clause's watches not on
// the two highest-level literals: reusing that conflict as a reason without
// repairing the watches leaves it unit with no watch to fire on. Built
// white-box — only chronological backtracking produces the wrong pair.
static void test_conflict_reuse_repairs_watches() {
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
  const Lit cl[4] = {internalize(-1), internalize(-2), internalize(-3),
                     internalize(-4)};
  const Ref ref = kernel_add_large_clause(k, cl, 4, true, 2);
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

static void test_binary_conflict_reuse_jumps_reason() {
  // Reusing a binary conflict uses Kissat's learned/reused assignment path,
  // where reason jumping is unconditional (not classification-gated).
  // 1 implies 2; deciding 3 conflicts with (-3,-2).  Reuse asserts -3 at
  // level 1 and must record the virtual direct reason -1, resolving past -2.
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

static void test_nonreusable_conflict_repairs_watches() {
  // Kissat promotes the two highest-level literals into the watched pair even
  // when more than one literal lies at the conflict level and the conflict
  // therefore cannot be reused.  The surviving clause pose affects which
  // conflict propagation observes after the next backtrack.
  Kernel k;
  kernel_reserve_vars(k, 4);
  kernel_add_binary_clause(k, internalize(-3), internalize(4));
  kernel_assign_decision(k, internalize(1));
  kernel_assign_decision(k, internalize(2));
  kernel_assign_decision(k, internalize(3));
  kernel_assign(k, internalize(4), 3, internalize(-3), true);
  const Lit conflict[4] = {internalize(-1), internalize(-2),
                           internalize(-3), internalize(-4)};
  const Ref ref = kernel_add_large_clause(k, conflict, 4, false, 0);
  k.conflict_ref = ref;
  k.conflict_valid = true;
  k.conflict_reuse = false; // watch canonicalization is independent of reuse

  assert(analyze_apply(k) == ApplyResult::Ok);
  const Clause *c = clause_at(k.arena, ref);
  assert(c->lits[0] == internalize(-3));
  assert(c->lits[1] == internalize(-4));
  assert(k.stats.conflict_watch_repaired > 0);
}

static void test_forced_conflict_unwind_preserves_two_stage_heap_pose() {
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

  // The conflict lives at level 2 although the solver stands at level 5.
  // Kissat first unwinds level 5 (reinsert v4), snapshots, then unwinds to 2
  // (reinsert v3). A direct unwind would reverse this equal-score pose.
  k.conflict_bin[0] = negate(make_lit(1, false));
  k.conflict_bin[1] = negate(make_lit(1, false));
  k.conflict_ref = INVALID_REF;
  k.conflict_valid = true;
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.h->scores.pos[4] < k.h->scores.pos[3]);
}

// The shortcut leaves no pending record, so a white-box micro-loop must skip
// the learn/backjump tail; run on the previous conflict's record it backtracks
// away the reuse's own assignment. `backjump_stale` stays 0 with the arm on.
static void test_conflict_reuse_under_default_policy() {
  // Chrono is load-bearing here: on a level-sorted trail "one literal at the
  // conflict level" is impossible, and 90 variables never reach the shipped
  // 100-level gap, so the reachability assertions below keep this honest.
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

// Drives `analyze_eager_subsume` on a hand-built register: DefaultPolicy
// cannot produce the discriminating cases. The reason guard is the mechanism's
// LRAT-safety guard — a deleted id must never reappear in a chain.
static void test_eager_subsume() {
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
    const Lit same[3] = {internalize(1), internalize(2), internalize(3)};
    const Lit other[4] = {internalize(1), internalize(2), internalize(5),
                          internalize(6)};
    const Lit irr[4] = {internalize(1), internalize(2), internalize(3),
                        internalize(4)};
    const Ref r_same = kernel_add_large_clause(k, same, 3, true, 3, 101);
    const Ref r_other = kernel_add_large_clause(k, other, 4, true, 4, 102);
    const Ref r_irr = kernel_add_large_clause(k, irr, 4, false, 0, 103);
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
  { // (c) the LRAT-safety guard: a subsumed candidate that is still a trail
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
  { // (e) eager_subsume = 0 is byte-identical to not having the mechanism
    Kernel k;
    fresh(k, 8);
    k.eager_subsume = 0;
    const Lit big[4] = {internalize(1), internalize(2), internalize(3),
                        internalize(4)};
    const Ref victim = kernel_add_large_clause(k, big, 4, true, 4, 101);
    k.eager_recent[0] = victim;
    k.pending.valid = true;
    k.pending.ref = INVALID_REF;
    k.pending.lits = {internalize(1), internalize(2), internalize(3)};
    analyze_eager_subsume(k);
    assert(k.stats.eager_tried == 0 && k.stats.eager_subsumed == 0);
    assert(!clause_at(k.arena, victim)->garbage());
  }
  { // (f) reachability under DefaultPolicy, so the arm is not vacuous where it
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

int main(int argc, char **argv) {
  test_uip();
  test_conflictanalysis_atomic();
  test_learned_unit();
  test_bump_admission_order_is_not_cleanup_order();
  test_level_one_failed_literal_batch();
  if (argc == 2)
    write_level_one_failed_literal_lrat(argv[1]);
  test_shrink();
  test_shrink_accepts_lower_learned_literal();
  test_shrink_crosses_unmarked_lower_level();
  test_shrink_processes_lower_blocks_first();
  test_singleton_block_skips_minimization();
  test_minimization_depth_matches_kissat();
  test_learned_level_stable_order();
  test_otfs();
  test_otfs_reuse_honors_chronological_gap();
  test_large_otfs_retires_replaced_reason();
  test_otfs_subsumption_inherits_better_conflict_glue();
  test_otfs_strengthening_preserves_better_reason_glue();
  test_chrono_backjump_clamps_to_jump_level();
  test_backjump_target_clamps();
  test_conflict_reuse_repairs_watches();
  test_binary_conflict_reuse_jumps_reason();
  test_nonreusable_conflict_repairs_watches();
  test_forced_conflict_unwind_preserves_two_stage_heap_pose();
  test_conflict_reuse_under_default_policy();
  test_eager_subsume();
  puts("test_analyze OK");
  return 0;
}
