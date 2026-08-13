// Restart: abandon the assignment prefix (kissat restart.c). WHEN is
// policy state (focused: Glucose EMA scheme; stable: reluctant doubling —
// both in DefaultPolicy); HOW — including trail reuse — is here.
#include "rules/restart/restart.hpp"

namespace tactix {

// soundness: abandoning the assignment prefix is always sound; only a
// recorded-but-unanalyzed conflict pins the trail (stale conflict record).
// WHEN to restart is the policy's decision and state (plan §5) —
// this module only knows HOW.
RuleEligibility restart_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("no-conflict", !k.conflict_valid)
      .fact("consistent", !k.inconsistent)
      .fact("nonroot-level", k.level > 0);
}

ApplyResult restart_apply_to(Kernel &k, uint32_t keep_level) {
  assert(restart_guard(k));
  if (keep_level > k.level)
    keep_level = k.level; // keep at most the whole trail
  if (keep_level > 0)
    k.stats.restarts_reused++;
  if (keep_level < k.level)
    kernel_backtrack(k, keep_level);
  k.stats.restarts++;
  return ApplyResult::Ok;
}

ApplyResult restart_apply(Kernel &k) {
  assert(restart_guard(k));
  return restart_apply_to(k, 0);
}

// generic instance surface (plan §5.1 v1): payload = keep-level.
// Canonical full restart first, then [level, 1] descending. Policies may rank
// the same complete legal set differently.
static uint32_t restart_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  if (cap == 0)
    return 0;
  uint32_t n = 0;
  out[n++] = 0;
  for (uint32_t lvl = k.level; lvl > 0 && n < cap; lvl--)
    out[n++] = lvl;
  return n;
}

static ApplyResult restart_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return restart_apply(k);
  // restart_apply_to clamps keep-levels above the current level
  return restart_apply_to(
      k, static_cast<uint32_t>(inst > UINT32_MAX ? UINT32_MAX : inst));
}

TACTIX_REGISTER_RULE(Restart, RuleTier::Macro, restart_guard,
                     restart_apply, restart_instances,
                     restart_apply_instance);

} // namespace tactix
