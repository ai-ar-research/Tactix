#include "rules/backjump/backjump.hpp"

#include "proof/proof.hpp"

namespace tactix {

bool backjump_guard(const Kernel &k) {
  // !conflict_valid: backtracking with an unanalyzed conflict recorded
  // would leave Analyze a stale conflict over unassigned variables
  return k.pending.valid && !k.conflict_valid && !k.inconsistent;
}

ApplyResult backjump_apply(Kernel &k) {
  assert(backjump_guard(k));
  PendingLearned &p = k.pending;
  if (p.jump_level < k.level)
    kernel_backtrack(k, p.jump_level);
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
    if (k.proof)
      k.proof->add_empty();
    return ApplyResult::Unsat;
  }
  if (stale) {
    p.valid = false;
    return ApplyResult::NoOp;
  }

  if (p.lits.size() == 1) {
    // learned unit: asserted at the root
    if (k.level > 0)
      kernel_backtrack(k, 0);
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

TACTIX_REGISTER_RULE(Backjump, RuleTier::Micro, backjump_guard,
                     backjump_apply);

} // namespace tactix
