#include "rules/backjump/backjump.hpp"

#include "policies/heuristics.hpp" // TB4b snapshot timing (target/best phases)
#include "proof/proof.hpp"

namespace tactix {

bool backjump_guard(const Kernel &k) {
  // !conflict_valid: backtracking with an unanalyzed conflict recorded
  // would leave Analyze a stale conflict over unassigned variables
  return k.pending.valid && !k.conflict_valid && !k.inconsistent;
}

ApplyResult backjump_apply_to(Kernel &k, uint32_t target) {
  assert(backjump_guard(k));
  PendingLearned &p = k.pending;
  // CHRONOLOGICAL target (rev 38, D-LEVEL-2): any level in [jump_level, level-1]
  // is now a sound backjump target. True-level assignment stamps the asserting
  // literal at its true level (jump_level) regardless of how far we backtrack,
  // so a higher (chronological) target keeps the intervening decisions without
  // stranding the assertion — the corpus-004/005 hazard is gone. Clamp into the
  // sound range (below jump_level would strand the assertion; >= level makes no
  // progress); the clamp keeps every legal instance total (N1).
  if (target < p.jump_level)
    target = p.jump_level;
  if (target + 1 > k.level) // target >= level (guard against underflow)
    target = k.level - 1;
  // TB4b · UNIT CHRONO ROUTING (2026-08-04). A learned unit used to be forced
  // to `target = 0`, discarding the whole descent. kissat routes it through the
  // SAME chrono determination as any other learned clause — `learn_unit` calls
  // `kissat_determine_new_level (solver, 0)` (learn.c:46), so with jump == 0 and
  // a deep trail `back - 0 > chronolevels` fires and it steps back ONE level,
  // keeping the prefix. Sound here because `kernel_assign_unit` stamps the
  // literal at level 0 explicitly (true-level assignment, D-LEVEL-2) regardless
  // of the current decision level — the unit is a root fact whatever level the
  // solver sits at, which is the whole point of that rev-38 change.
  //
  // Measured motive (`veer_axi`): 14,088 of our 18,635 conflicts learn a unit,
  // and the caller COUNTED 14,108 chronological backtracks while this line then
  // overrode 14,088 of them to 0 — so we genuinely backtracked chronologically
  // on 20 conflicts (0.1 %) where kissat does so on 2,821 of 2,967 (95 %). The
  // `c chronological` stat was reporting behaviour we did not perform.
  // TB4b · SNAPSHOT TIMING — kissat `kissat_backtrack_after_conflict`
  // (backtrack.c:156) in its exact order:
  //     if (level) backtrack_without_updating_phases(level - 1);
  //     update_target_and_best_phases();
  //     backtrack_without_updating_phases(new_level);
  // The one-level step FIRST is the point: the snapshot must NOT see the
  // conflict level's own literals. Taking it at the full trail (what we did)
  // inflates the `target_assigned`/`best_assigned` high-water mark by the
  // conflicting assignments, so later genuinely-longer trails fail to beat it
  // and the target phases stop tracking the best trail found.
  if (k.level) {
    if (k.level - 1 < k.level)
      kernel_backtrack(k, k.level - 1);
    if (k.h)
      heuristics_update_target_phases(k);
  }
  if (target < k.level)
    kernel_backtrack(k, target);
  const Lit asserting = p.lits[0];

  // The pending clause's assertion metadata is only valid in the trail
  // episode Analyze computed it in. Under DefaultPolicy that is always
  // the case; under other legal schedules (e.g. Restart or Decide between
  // Analyze and Backjump) it may be stale. Asserting with the clause as
  // reason is sound ONLY if every other literal is currently false and
  // the asserting literal is unassigned — otherwise discard the pending
  // assertion: the clause itself is already stored, watched, and in the
  // proof, so normal propagation takes over. (Rule contract: apply() is
  // total and sound whenever the guard holds, for any history.)
  bool stale = k.values[asserting] != 0;
  for (size_t i = 1; !stale && i < p.lits.size(); i++)
    if (k.values[p.lits[i]] >= 0)
      stale = true;
  // Refutation test is now LEVEL-AWARE, not "we are at the root": under unit
  // chrono routing we may sit above level 0, and what makes a learned unit a
  // refutation is that its literal is false AT LEVEL 0, not where we happen to
  // be standing.
  if (p.lits.size() == 1 && k.values[asserting] < 0 &&
      k.assigned[var_of(asserting)].level == 0) {
    // learned unit contradicts a root assignment: refutation complete
    // (the unit is in the proof; the empty clause is RUP from it)
    p.valid = false;
    // chain: the learned unit + its opposite (the shared kernel idiom;
    // sets `inconsistent`)
    kernel_emit_empty_from_pair(k, p.id, k.unit_ids[var_of(asserting)]);
    return ApplyResult::Unsat;
  }
  if (stale) {
    k.stats.backjump_stale++;
    p.valid = false;
    return ApplyResult::NoOp;
  }

  if (p.lits.size() == 1) {
    // Asserted AS a root fact (level 0) even when the solver stands higher —
    // see the unit-chrono-routing note above. No `k.level == 0` assert: that
    // was an artefact of the forced target, not an invariant of the assignment.
    kernel_assign_unit(k, asserting);
    k.stats.learned_units++;
  } else if (p.lits.size() == 2) {
    kernel_assign_binary(k, asserting, p.lits[1]);
  } else {
    assert(p.ref != INVALID_REF);
    kernel_assign_ref(k, asserting, p.ref);
  }
  p.valid = false;
  return ApplyResult::Ok;
}

ApplyResult backjump_apply(Kernel &k) {
  return backjump_apply_to(k, k.pending.jump_level);
}

// NOT a registered rule since rev 37: Analyze + Backjump are fused into the
// single atomic ConflictAnalysis rule (calculus §8; D-SPLIT). backjump_apply /
// backjump_apply_to / backjump_guard remain as internal step functions —
// called by conflictanalysis_apply (analyze.cpp) and, white-box, by
// DefaultPolicy's micro-loop and the analyze unit tests — but there is no
// `pending` control state observable at a rule boundary, so the guards that
// used to carry `¬pending.valid` no longer need it.

} // namespace tactix
