// Analyze/Backjump unit test: forced 1-UIP scenario with a known learned
// clause and backjump level.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace tactix;

static void test_uip() {
  // c1 = (-1 -2 3), c2 = (-1 -2 -3); decide 1, decide 2:
  // c1 forces 3, c2 conflicts; 1-UIP learns (-2 -1) with UIP 2, jump to 1
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

  assert(analyze_guard(k));
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.pending.valid);
  assert(k.pending.lits.size() == 2);
  assert(k.pending.lits[0] == internalize(-2)); // negated UIP
  assert(k.pending.lits[1] == internalize(-1));
  assert(k.pending.jump_level == 1);

  assert(backjump_guard(k));
  assert(backjump_apply(k) == ApplyResult::Ok);
  assert(k.level == 1);
  assert(k.values[internalize(-2)] > 0); // asserted
  assert(!k.pending.valid);

  // and the search must now conclude SAT (model: 1, -2, any 3)
  assert(default_solve(k) == 10);
}

// D-SPLIT : Analyze+Backjump are fused into the single atomic
// ConflictAnalysis rule, so `pending` is consumed WITHIN one apply and never
// persists at a rule boundary. That makes the corpus-006 state
// (conflict_valid && pending.valid) unrepresentable — which is why
// propagate_guard no longer carries !pending.valid. This replaces the rev-35
// handoff-precondition test, whose state can no longer arise.
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

  // Analyze and Backjump are no longer separately registered
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
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Conflict);
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.pending.lits.size() == 1);
  assert(k.pending.lits[0] == internalize(-1));
  assert(k.pending.jump_level == 0);
  assert(backjump_apply(k) == ApplyResult::Ok);
  assert(k.level == 0);
  assert(k.values[internalize(-1)] > 0);
  assert(k.stats.learned_units == 1);
}

static void test_shrink() {
  // level 1: 1 forces 2 and 3; level 2: 4 forces 5 then 6. The conflict
  // clause (-6 -5 -2 -3) leaves the two-literal level-1 block {-2, -3} in
  // the 1-UIP clause; neither literal is removable by plain minimization
  // (their chains hit the decision 1, which is not in the clause), but the
  // block's walk converges to that decision, so shrinking replaces it by
  // -1: learned (-4 -1) instead of (-4 -2 -3).
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

static void test_otfs() {
  // r = (-1 -2 3) propagates 3 at level 2 (after deciding 1, then 2);
  // d = (-1 -2 -3) then conflicts. Resolving d with r on 3 yields (-1 -2),
  // which is smaller than both: OTFS strengthens r to the binary (-1 -2)
  // and, since this was the first resolution and the resolvent is smaller
  // than d, subsumes (deletes) d. Analysis restarts on (-1 -2) and learns
  // it as the driving clause with UIP 2.
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

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.otfs_strengthened == 1);
  assert(k.stats.otfs_subsumed == 1);
  assert(clause_at(k.arena, dref)->garbage()); // conflict subsumed
  assert(!clause_at(k.arena, rref)->garbage()); // reason kept (deviation)
  assert(k.pending.valid);
  assert(k.pending.lits.size() == 2);
  assert(k.pending.lits[0] == internalize(-2)); // negated UIP
  assert(k.pending.lits[1] == internalize(-1));
  assert(k.pending.jump_level == 1);
  assert(backjump_apply(k) == ApplyResult::Ok);
  assert(default_solve(k) == 10); // satisfiable: 1, -2
}

static void test_chrono_backjump_clamps_to_jump_level() {
  // c1 = (-1 -3 4), c2 = (-1 -3 -4); decide 1 (level 1), decide 2 (level
  // 2, unrelated), decide 3 (level 3): c1 forces 4, c2 conflicts. 1-UIP
  // learns (-3 -1) with jump level 1. Since (D-LEVEL-2) a CHRONOLOGICAL
  // target (2, above the assertion level) is HONORED: true-level assignment
  // stamps the UIP at its true level (jump_level 1) regardless of the target,
  // so backtracking only to level 2 keeps the unrelated level-2 decision and
  // does not strand the learned clause. (The name is historical; the function
  // formerly clamped to jump_level.)
  Kernel k;
  kernel_reserve_vars(k, 4);
  const Lit c1[3] = {internalize(-1), internalize(-3), internalize(4)};
  const Lit c2[3] = {internalize(-1), internalize(-3), internalize(-4)};
  kernel_add_large_clause(k, c1, 3, false, 0);
  kernel_add_large_clause(k, c2, 3, false, 0);

  kernel_assign_decision(k, internalize(1));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(2));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_assign_decision(k, internalize(3));
  assert(propagate_apply(k) == ApplyResult::Conflict);

  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.pending.lits.size() == 2);
  assert(k.pending.lits[0] == internalize(-3)); // negated UIP
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
  // targets below the assertion level clamp up to it: below jump_level
  // the learned clause would no longer be asserting
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
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.pending.jump_level == 1);
  assert(backjump_apply_to(k, 0) == ApplyResult::Ok); // clamps to 1
  assert(k.level == 1);
  assert(k.values[internalize(-2)] > 0); // asserted at the jump level
  assert(default_solve(k) == 10);
}

// --- TB4b: the conflict-reuse shortcut and its watch repair ----------------

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

// The shape the CRIPPLED version declined: exactly one conflict literal at the
// conflict level, but the clause's two watched literals are NOT the two
// highest-level ones. Reusing such a conflict as a reason without repairing the
// watches is what aborted regression corpus 002/005 — the clause is later left
// unit with no watch to fire on. Built white-box (decisions first, clause
// second) because ordinary propagation always leaves a conflict watched on the
// last two literals it falsified; the wrong pair arises from chronological
// backtracking, whose trail is not level-sorted.
static void test_conflict_reuse_repairs_watches() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  k.conflict_reuse = true;
  for (int v = 1; v <= 4; v++) { // levels 1..4, one decision each
    kernel_assign_decision(k, internalize(v));
    assert(propagate_apply(k) == ApplyResult::Ok);
  }
  // watches land on lits[0], lits[1] = -1 (level 1) and -2 (level 2); the two
  // HIGHEST levels are -4 (4, the unique conflict-level literal) and -3 (3)
  const Lit cl[4] = {internalize(-1), internalize(-2), internalize(-3),
                     internalize(-4)};
  const Ref ref = kernel_add_large_clause(k, cl, 4, false, 0);
  assert(watched_by(k, internalize(-1), ref));
  assert(watched_by(k, internalize(-2), ref));
  k.conflict_valid = true; // the clause is falsified: record it as the conflict
  k.conflict_ref = ref;

  assert(analyze_guard(k));
  assert(analyze_apply(k) == ApplyResult::Ok);
  assert(k.stats.conflict_reused == 1);        // the shortcut fired...
  assert(k.stats.conflict_watch_repaired == 1); // ...and needed the repair
  assert(!k.pending.valid);                     // nothing learned, nothing to assert

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

  // THE REGRESSION: drop below jump2 (unassigning the forced literal), then
  // re-falsify the level-3 literal. With the repaired watch pair the clause is
  // rescanned and propagates; with the ORIGINAL pair (-1, -2, both still false)
  // nothing would fire and it would sit unit at a claimed fixpoint.
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

// The shortcut resolves the conflict INSIDE Analyze and leaves no pending
// record, so DefaultPolicy's white-box micro-loop must skip the whole
// learn/backjump tail. It did not before TB4b's audit: it ran that tail on the
// PREVIOUS conflict's pending record, backtracking away the reuse's own
// assignment and counting a stale backjump. `backjump_stale` is TB4's standing
// regression signal and must stay 0 with the arm ON.
static void test_conflict_reuse_under_default_policy() {
  // Random 3-SAT above the threshold, solved with chrono ALWAYS on.
  //
  // The chrono setting is load-bearing, not decoration: under COMPLETE
  // propagation on a level-sorted trail "exactly one literal at the conflict
  // level" is IMPOSSIBLE — if the other literals were all below level M the
  // clause was already unit before level M and would have propagated. Only a
  // chronological backtrack (which keeps out-of-order literals and re-propagates
  // from the level boundary) produces the shape. At the shipped
  // `chrono_levels = 100` a 90-variable instance can never reach a backjump gap
  // of 100 levels, so a test written that way measures NOTHING and still
  // passes; the reachability assertions at the bottom are what keep it
  // honest.
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
    assert(k.stats.backjump_stale == 0); // TB4's standing invariant
    total_reused += k.stats.conflict_reused;
    total_repaired += k.stats.conflict_watch_repaired;
  }
  assert(total_reused > 0);   // the arm was actually exercised...
  assert(total_repaired > 0); // ...including the un-crippled (repair) path
}

// SP-EAGERSUB-1 / SCHED-C6. Drives `analyze_eager_subsume` directly on a
// hand-built register, because the discriminating cases are exactly the ones
// DefaultPolicy cannot produce. In particular the REASON guard: it is the
// LRAT-safety guard of the whole mechanism (a deleted id must never reappear
// in a chain), and under any legal backjump target the candidate's forced
// literal sits at or above the conflict level and is therefore always
// unassigned by the time we run — so DefaultPolicy reaches the guard zero
// times and a "green because it never fired" test would be vacuous.
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
  { // (c) THE LRAT-SAFETY GUARD: a subsumed candidate that is still a trail
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
    kernel_mark_garbage(k, clause_at(k.arena, dead));
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
  { // (f) REACHABILITY under DefaultPolicy — the arm is not vacuous where it
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

int main() {
  test_uip();
  test_conflictanalysis_atomic();
  test_learned_unit();
  test_shrink();
  test_otfs();
  test_chrono_backjump_clamps_to_jump_level();
  test_backjump_target_clamps();
  test_conflict_reuse_repairs_watches();
  test_conflict_reuse_under_default_policy();
  test_eager_subsume();
  puts("test_analyze OK");
  return 0;
}
