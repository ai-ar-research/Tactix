#include "rules/backjump/backjump.hpp"

#include "diagnostics/event_trace.hpp"
#include "policies/heuristics.hpp" // target/best phase snapshot
#include "proof/proof.hpp"
#include "rules/analyze/analyze.hpp" // analyze_eager_subsume

namespace tactix {

bool backjump_guard(const Kernel &k) {
  // !conflict_valid: backtracking with an unanalyzed conflict recorded
  // would leave Analyze a stale conflict over unassigned variables
  return k.pending.valid && !k.conflict_valid && !k.inconsistent;
}

void backjump_assign_learned_unit(Kernel &k, Lit unit) {
  kernel_assign_unit(k, unit);
  // Kissat marks a newly learned unit's variable fixed inside assignment,
  // which immediately removes it from both decision structures before the
  // analyzed stack is bumped.  Tactix retains stable kernel identities for
  // reconstruction, so this policy-owned sub-step performs the equivalent
  // decision-structure transition explicitly.
  heuristics_deactivate_var(k, var_of(unit));
  k.stats.learned_units++;
}

ApplyResult backjump_apply_to(Kernel &k, uint32_t target) {
  assert(backjump_guard(k));
  k.stats.backjumps++; // public observation of a learned-clause transaction
  PendingLearned &p = k.pending;
  event_trace_detail(k, "analysis_tail_backjump_begin", target);
  // Chronological target (D-LEVEL-2): true-level assignment stamps the
  // asserting literal at jump_level however far the trail is cut, so any level
  // in [jump_level, level-1] is sound. The clamp keeps every legal instance
  // total (N1).
  if (target < p.jump_level)
    target = p.jump_level;
  if (target + 1 > k.level) // target >= level (guard against underflow)
    target = k.level - 1;
  // A learned unit goes through the same chrono determination as any other
  // learned clause (kissat learn.c:46), not forced to target 0: a unit is a
  // root fact whatever level the solver stands at, `kernel_assign_unit`
  // stamping it at level 0 explicitly (D-LEVEL-2).
  //
  // Phase-snapshot order, kissat `kissat_backtrack_after_conflict`
  // (backtrack.c:156): step back one level, snapshot, then backtrack to the
  // target. The one-level step must come first, or the snapshot counts the
  // conflict level's own literals into the target/best high-water mark and
  // later longer trails fail to beat it.
  if (k.level) {
    kernel_backtrack(k, k.level - 1);
    if (k.h)
      heuristics_update_target_phases(k);
  }
  event_trace_detail(k, "analysis_tail_first_unwind", k.level);
  if (target < k.level)
    kernel_backtrack(k, target);
  event_trace_detail(k, "analysis_tail_target_unwind", k.level);

  // OTFS's old clause had to remain a valid reason until the analysis-level
  // literal was removed.  It is now redundant with the fresh stronger clause;
  // retire it before any later rule or propagation can observe the duplicate.
  analyze_retire_otfs_reasons(k);
  event_trace_detail(k, "analysis_tail_otfs_retired", k.level);

  // Eager subsumption belongs after the backtrack: before it, every subsumable
  // candidate is still a trail reason and gets skipped. Running it before the
  // assert is equivalent to kissat's position, since asserting the UIP can
  // only make the pending clause a reason and that clause is not a candidate.
  // Mechanism in analyze.cpp; rationale in docs/implementation-notes.md §15.
  event_trace_detail(k, "analysis_tail_eager_begin", p.lits.size());
  analyze_eager_subsume(k);
  event_trace_detail(k, "analysis_tail_eager_end", p.lits.size());

  const Lit asserting = p.lits[0];

  // The pending clause's assertion metadata is only valid in the trail episode
  // Analyze computed it in; a Restart or Decide in between can stale it.
  // Asserting with the clause as reason is sound only if every other literal
  // is false and the asserting literal unassigned — otherwise discard the
  // assertion and let propagation take over, the clause being stored, watched
  // and in the proof already. apply() stays total for any history.
  bool stale = k.values[asserting] != 0;
  for (size_t i = 1; !stale && i < p.lits.size(); i++)
    if (k.values[p.lits[i]] >= 0)
      stale = true;
  // Level-aware, not "at the root": unit chrono routing can leave the solver
  // above level 0, and what makes a learned unit a refutation is that its
  // literal is false at level 0.
  if (p.lits.size() == 1 && k.values[asserting] < 0 &&
      k.assigned[var_of(asserting)].level == 0) {
    // learned unit contradicts a root assignment: refutation complete
    // (the unit is in the proof; the empty clause is RUP from it)
    p.valid = false;
    // chain: the learned unit + its opposite (the shared kernel idiom;
    // sets `inconsistent`)
    kernel_emit_empty_from_pair(k, p.id, k.unit_ids[var_of(asserting)]);
    analyze_bump_after_backjump(k);
    return ApplyResult::Unsat;
  }
  if (stale) {
    k.stats.backjump_stale++;
    p.valid = false;
    analyze_bump_after_backjump(k);
    return ApplyResult::NoOp;
  }

  if (p.lits.size() == 1) {
    // Asserted as a root fact even when the solver stands higher (see the
    // unit chrono note above), so `k.level == 0` is not an invariant here.
    backjump_assign_learned_unit(k, asserting);
  } else if (p.lits.size() == 2) {
    // Kissat's learned-binary assertion uses assign.c, whose jump-reason path
    // is deliberately not gated by `classification.bigbig`.
    kernel_assign_binary(k, asserting, p.lits[1], /*require_bigbig=*/false);
  } else {
    assert(p.ref != INVALID_REF);
    kernel_assign_ref(k, asserting, p.ref);
  }
  event_trace_detail(k, "analysis_tail_asserted", k.level);
  // `ConflictAnalysis` is one public atomic rule, but its internal order
  // follows Kissat's learn.c/analyze.c boundary: learn, backtrack/assert, then
  // bump the analyzed variables. Reinsertion before equal-score bumps is
  // decision-observable even though the score arithmetic is commutative.
  event_trace_detail(k, "analysis_tail_bump_begin", k.analyzed.size());
  analyze_bump_after_backjump(k);
  event_trace_detail(k, "analysis_tail_bump_end", k.analyzed.size());
  p.valid = false;
  return ApplyResult::Ok;
}

ApplyResult backjump_apply(Kernel &k) {
  return backjump_apply_to(k, k.pending.jump_level);
}

// Not a registered rule: Analyze + Backjump are fused into the atomic
// ConflictAnalysis rule (docs/implementation-notes.md §4). These stay as
// internal step functions, driven by conflictanalysis_apply and, white-box, by
// DefaultPolicy's micro-loop and the analyze unit tests.

} // namespace tactix
