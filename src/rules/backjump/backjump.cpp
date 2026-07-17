#include "rules/backjump/backjump.hpp"

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
  // Assign the asserting literal at its TRUE level — the assertion level.
  // This kernel stamps every implication with the CURRENT decision level,
  // so backtracking to a target ABOVE jump_level would stamp the asserting
  // literal too deep; a later partial Restart (or Backjump) to a level
  // between jump_level and target then unassigns it while its reason clause
  // stays unit — an escaped unit at a claimed fixpoint (corpus 005; the
  // corpus-004 current-level-assignment hazard, reached here via
  // chronological backtracking). Genuine chronological backtracking (keeping
  // decision levels above the assertion level) needs true-level assignment
  // and is deferred to the §5.4 kernel candidate; until then jump_level is
  // the only sound target. `target` is clamped, not asserted, so every legal
  // instance stays total per the rule contract (N1).
  target = p.jump_level;
  if (p.lits.size() == 1)
    target = 0; // learned units assert at the root
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
  if (p.lits.size() == 1 && k.level == 0 && k.values[asserting] < 0) {
    // learned unit contradicts a root assignment: refutation complete
    // (the unit is in the proof; the empty clause is RUP from it)
    k.inconsistent = true;
    p.valid = false;
    if (k.proof) {
      if (k.proof->lrat()) { // chain: the learned unit + its opposite
        const uint64_t chain[2] = {p.id, k.unit_ids[var_of(asserting)]};
        k.proof->add_empty(++k.next_clause_id, chain, 2);
      } else {
        k.proof->add_empty(0, nullptr, 0);
      }
    }
    return ApplyResult::Unsat;
  }
  if (stale) {
    p.valid = false;
    return ApplyResult::NoOp;
  }

  if (p.lits.size() == 1) {
    // learned unit: asserted at the root (target was forced to 0 above)
    assert(k.level == 0);
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
