// Restart: abandon the assignment prefix (kissat restart.c). When is the
// policy's (DefaultPolicy); how, including trail reuse, is here.
#include "rules/restart/restart.hpp"
#include "rules/analyze/analyze.hpp"

namespace tactix {

// Always sound; only a recorded unanalyzed conflict pins the trail.
RuleEligibility restart_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent);
}

static bool recorded_conflict_still_holds(const Kernel &k) {
  if (!k.conflict_valid)
    return false;
  if (k.conflict_ref == INVALID_REF)
    return k.value(k.conflict_bin[0]) < 0 && k.value(k.conflict_bin[1]) < 0;
  const Clause *conflict = clause_at(k.arena, k.conflict_ref);
  for (const Lit lit : *conflict)
    if (k.value(lit) >= 0)
      return false;
  return true;
}

static ApplyResult refresh_recorded_conflict(Kernel &k) {
  if (recorded_conflict_still_holds(k)) {
    // If every conflict literal survives a full restart, the root is refuted:
    // discharge it through Analyze before a root rewrite retires its
    // antecedent.
    if (k.level == 0)
      return analyze_apply(k);
    return ApplyResult::Ok;
  }
  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  k.conflict_bin[0] = k.conflict_bin[1] = INVALID_LIT;
  return ApplyResult::Ok;
}

ApplyResult restart_backtrack_to(Kernel &k, uint32_t keep_level) {
  assert(restart_guard(k));
  if (keep_level > k.level)
    keep_level = k.level; // keep at most the whole trail
  if (keep_level < k.level)
    kernel_backtrack(k, keep_level);
  return refresh_recorded_conflict(k);
}

ApplyResult restart_apply_to(Kernel &k, uint32_t keep_level) {
  assert(restart_guard(k));
  if (keep_level > k.level)
    keep_level = k.level;
  if (keep_level > 0)
    k.stats.restarts_reused++;
  const ApplyResult result = restart_backtrack_to(k, keep_level);
  k.stats.restarts++;
  return result;
}

ApplyResult restart_apply(Kernel &k) {
  assert(restart_guard(k));
  return restart_apply_to(k, 0);
}

// Instance surface (plan §5.1 v1): payload = keep-level; full restart first,
// then [level, 1] descending.
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
