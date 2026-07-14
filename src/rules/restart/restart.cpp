// Restart: abandon the assignment prefix (kissat restart.c). WHEN is
// policy state (focused: Glucose EMA scheme; stable: reluctant doubling —
// both in DefaultPolicy, rev 17); HOW — including trail reuse — is here.
#include "rules/restart/restart.hpp"

namespace tactix {

// soundness: abandoning the assignment prefix is always sound; only a
// recorded-but-unanalyzed conflict pins the trail (stale conflict record).
// WHEN to restart is the policy's decision and state (plan §5 rev 17) —
// this module only knows HOW.
bool restart_guard(const Kernel &k) {
  return !k.conflict_valid && !k.inconsistent && k.level > 0;
}

// trail reuse (kissat restart.c reuse_focused_trail/reuse_stable_trail):
// keep the decision levels whose decision variable ranks higher (VMTF
// stamp / EVSIDS score, per the active mode) than the next decision
// candidate — after a full restart the heuristic would re-decide exactly
// that prefix, so re-propagating it is pure waste.
uint32_t restart_reuse_level(Kernel &k) {
  if (k.unassigned == 0)
    return 0; // nothing left to decide; abandon the whole trail
  uint32_t res = 0;
  if (k.stable) {
    const Var next = kernel_heap_next_unassigned(k);
    const double limit = k.scores.score[next];
    while (res < k.level) {
      const Lit decision = k.trail[k.frames[res + 1]];
      if (k.scores.score[var_of(decision)] <= limit)
        break;
      res++;
    }
  } else {
    const Var next = kernel_queue_next_unassigned(k);
    const uint32_t limit = k.links[next].stamp;
    while (res < k.level) {
      const Lit decision = k.trail[k.frames[res + 1]];
      if (k.links[var_of(decision)].stamp <= limit)
        break;
      res++;
    }
  }
  return res;
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
  return restart_apply_to(k, restart_reuse_level(k));
}

TACTIX_REGISTER_RULE(Restart, RuleTier::Macro, restart_guard,
                     restart_apply);

} // namespace tactix
