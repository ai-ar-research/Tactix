#include "rules/backjump/backjump.hpp"

namespace tactix {

bool backjump_guard(const Kernel &k) {
  return k.pending.valid && !k.inconsistent;
}

ApplyResult backjump_apply(Kernel &k) {
  assert(backjump_guard(k));
  PendingLearned &p = k.pending;
  if (p.jump_level < k.level)
    kernel_backtrack(k, p.jump_level);
  const Lit asserting = p.lits[0];
  // Under DefaultPolicy the asserting literal is always unassigned here.
  // Under other legal schedules (rules interleaved between Analyze and
  // Backjump) it may have been assigned meanwhile; the learned clause is
  // already stored and watched, so skipping the assertion stays sound.
  if (k.values[asserting] != 0) {
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
