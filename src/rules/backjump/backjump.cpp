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
  // sound targets are [jump_level, current level]: below the assertion
  // level the clause stops being asserting (the stale check below would
  // just discard it), above the current level there is nothing to
  // abandon. Learned units always assert at the root.
  if (target < p.jump_level)
    target = p.jump_level;
  if (p.lits.size() == 1)
    target = 0;
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
    if (k.proof)
      k.proof->add_empty();
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

// generic instance surface (plan §5.1 v1): payload = target level.
// Ranked: the canonical (assertion-level) target first, then ascending
// toward the current level — chronological backtracking last
static uint32_t backjump_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  uint32_t n = 0;
  for (uint32_t lvl = k.pending.jump_level; lvl <= k.level && n < cap; lvl++)
    out[n++] = lvl;
  return n;
}

static ApplyResult backjump_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return backjump_apply(k);
  // backjump_apply_to clamps into [jump_level, current level]
  return backjump_apply_to(
      k, static_cast<uint32_t>(inst > UINT32_MAX ? UINT32_MAX : inst));
}

TACTIX_REGISTER_RULE(Backjump, RuleTier::Micro, backjump_guard,
                     backjump_apply, backjump_instances,
                     backjump_apply_instance);

} // namespace tactix
