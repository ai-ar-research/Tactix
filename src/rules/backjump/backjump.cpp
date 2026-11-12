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
  // Kissat fixes a learned unit inside assignment, removing it from the
  // decision structures before the bump; do the same explicitly.
  heuristics_deactivate_var(k, var_of(unit));
  k.stats.learned_units++;
}

ApplyResult backjump_apply_to(Kernel &k, uint32_t target) {
  assert(backjump_guard(k));
  k.stats.backjumps++; // public observation of a learned-clause transaction
  PendingLearned &p = k.pending;
  event_trace_detail(k, "analysis_tail_backjump_begin", target);
  // Chronological target (D-LEVEL-2): any level in [jump_level, level-1] is
  // sound; the clamp keeps every instance total (N1).
  if (target < p.jump_level)
    target = p.jump_level;
  if (target + 1 > k.level) // target >= level (guard against underflow)
    target = k.level - 1;
  // A learned unit uses the same chrono target (kissat learn.c:46);
  // `kernel_assign_unit` stamps it at level 0 (D-LEVEL-2). Phase snapshot as
  // kissat `kissat_backtrack_after_conflict` (backtrack.c:156): one level back,
  // snapshot, then the target.
  if (k.level) {
    kernel_backtrack(k, k.level - 1);
    if (k.h)
      heuristics_update_target_phases(k);
  }
  event_trace_detail(k, "analysis_tail_first_unwind", k.level);
  if (target < k.level)
    kernel_backtrack(k, target);
  event_trace_detail(k, "analysis_tail_target_unwind", k.level);

  // Retire OTFS's old clause now that the stronger one replaces it.
  analyze_retire_otfs_reasons(k);
  event_trace_detail(k, "analysis_tail_otfs_retired", k.level);

  // Eager subsumption after the backtrack (earlier, candidates are still
  // reasons), equivalent to kissat's position (implementation-notes §15).
  event_trace_detail(k, "analysis_tail_eager_begin", p.lits.size());
  analyze_eager_subsume(k);
  event_trace_detail(k, "analysis_tail_eager_end", p.lits.size());

  const Lit asserting = p.lits[0];

  // Pending metadata can go stale across a Restart or Decide: assert only if
  // every other literal is false and the asserting literal unassigned, else let
  // propagation handle the stored clause. apply() stays total.
  bool stale = k.values[asserting] != 0;
  for (size_t i = 1; !stale && i < p.lits.size(); i++)
    if (k.values[p.lits[i]] >= 0)
      stale = true;
  // A learned unit refutes when its literal is false at level 0.
  if (p.lits.size() == 1 && k.values[asserting] < 0 &&
      k.assigned[var_of(asserting)].level == 0) {
    // learned unit contradicts a root assignment: refutation complete
    // (the unit is in the proof; the empty clause is RUP from it)
    p.valid = false;
    // the shared kernel idiom; sets `inconsistent`
    kernel_emit_empty(k);
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
  // Kissat's learn.c/analyze.c order: learn, backtrack/assert, then bump.
  event_trace_detail(k, "analysis_tail_bump_begin", k.analyzed.size());
  analyze_bump_after_backjump(k);
  event_trace_detail(k, "analysis_tail_bump_end", k.analyzed.size());
  p.valid = false;
  return ApplyResult::Ok;
}

ApplyResult backjump_apply(Kernel &k) {
  return backjump_apply_to(k, k.pending.jump_level);
}

// Internal step functions of the fused ConflictAnalysis rule
// (implementation-notes §4).

} // namespace tactix
