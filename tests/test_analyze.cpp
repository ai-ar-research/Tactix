// Analyze/Backjump: forced scenarios with a known 1-UIP clause and jump level.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/propagate/propagate.hpp"

#include <cassert>
#include <cstdio>

using namespace tactix;

static void test_uip() {
  // (-1 -2 3) and (-1 -2 -3): 1-UIP learns (-2 -1), UIP 2, jump level 1
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

  assert(default_solve(k) == 10); // still satisfiable: 1, -2, any 3
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

static Kernel singleton_minimization_fixture(bool enabled) {
  // At level 2, c follows from a through (-a c).  The conflict learns
  // (-d -c -a), where -c is the sole literal on its level and is redundant
  // through -a.  Kissat shrink=3 skips that singleton block; Tactix's
  // stronger arm minimizes it.
  Kernel k;
  kernel_reserve_vars(k, 4);
  k.minimize_singletons = enabled;
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

static void test_singleton_minimization_arm() {
  Kernel stronger = singleton_minimization_fixture(true);
  assert(stronger.pending.lits.size() == 2);
  assert(stronger.pending.lits[0] == internalize(-4));
  assert(stronger.pending.lits[1] == internalize(-1));
  assert(stronger.pending.jump_level == 1);

  Kernel kissat = singleton_minimization_fixture(false);
  assert(kissat.pending.lits.size() == 3);
  assert(kissat.pending.lits[0] == internalize(-4));
  assert(kissat.pending.jump_level == 2);
}

static Kernel learned_tie_fixture(bool trail_tie) {
  // The learned level-2 block is inserted as {-b,-c}, while c has the later
  // trail position.  Its shrink walk fails through the lower-level decision
  // a, leaving the block intact and isolating the within-level ordering rule.
  Kernel k;
  kernel_reserve_vars(k, 4);
  k.learned_trail_tie = trail_tie;
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

static void test_learned_level_tie_arm() {
  Kernel kissat = learned_tie_fixture(false);
  assert(kissat.pending.lits.size() == 3);
  assert(kissat.pending.lits[0] == internalize(-4));
  assert(kissat.pending.lits[1] == internalize(-2));
  assert(kissat.pending.lits[2] == internalize(-3));

  Kernel former = learned_tie_fixture(true);
  assert(former.pending.lits.size() == 3);
  assert(former.pending.lits[0] == internalize(-4));
  assert(former.pending.lits[1] == internalize(-3));
  assert(former.pending.lits[2] == internalize(-2));
}

static void test_otfs() {
  // Resolving d = (-1 -2 -3) with its reason r = (-1 -2 3) yields (-1 -2),
  // smaller than both: OTFS strengthens r to it, subsumes d, and relearns.
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
  // D-LEVEL-2: a chronological target above the assertion level is honored,
  // and true-level assignment stamps the UIP at jump_level whatever the target,
  // so stopping at level 2 keeps that decision without stranding the clause.
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
  // a target below the assertion level clamps up: the clause must stay
  // asserting
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
  kernel_reserve_vars(k, 4);
  k.conflict_reuse = true;
  for (int v = 1; v <= 4; v++) { // levels 1..4, one decision each
    kernel_assign_decision(k, internalize(v));
    assert(propagate_apply(k) == ApplyResult::Ok);
  }
  // watches land on -1 (level 1) and -2 (level 2); the two highest levels are
  // -4 (the unique conflict-level literal) and -3
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

int main() {
  test_uip();
  test_conflictanalysis_atomic();
  test_learned_unit();
  test_shrink();
  test_singleton_minimization_arm();
  test_learned_level_tie_arm();
  test_otfs();
  test_chrono_backjump_clamps_to_jump_level();
  test_backjump_target_clamps();
  test_conflict_reuse_repairs_watches();
  test_conflict_reuse_under_default_policy();
  test_eager_subsume();
  puts("test_analyze OK");
  return 0;
}
