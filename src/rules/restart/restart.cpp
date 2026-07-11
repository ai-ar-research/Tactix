// EMA-triggered restarts (Glucose scheme, Audemard–Simon; kissat's
// focused-mode restart.c is the same idea): restart when the short-term
// glue average exceeds the long-term one by a margin — search quality is
// degrading, so abandon the prefix. Constants are the published defaults
// (margin 1.25, floor 50 conflicts), not benchmark-tuned. Reluctant
// doubling returns with stable mode (SwitchMode, phase 2).
#include "rules/restart/restart.hpp"

namespace tactix {

static constexpr double MARGIN = 1.25;
static constexpr uint64_t FLOOR_INTERVAL = 50;

// soundness: abandoning the assignment prefix is always sound; only a
// recorded-but-unanalyzed conflict pins the trail (stale conflict record)
bool restart_guard(const Kernel &k) {
  return !k.conflict_valid && !k.inconsistent && k.level > 0;
}

bool restart_advised(const Kernel &k) {
  return k.restart.ema_initialized &&
         k.stats.conflicts >= k.restart.floor_conflicts &&
         k.restart.fast_glue > MARGIN * k.restart.slow_glue;
}

// trail reuse (kissat restart.c reuse_focused_trail): keep the decision
// levels whose decision variable has a higher VMTF stamp than the next
// decision candidate — after a full restart the heuristic would re-decide
// exactly that prefix, so re-propagating it is pure waste. The stable-mode
// (score-heap) variant arrives with SwitchMode in phase 2.
static uint32_t reuse_trail(Kernel &k) {
  if (k.unassigned == 0)
    return 0; // nothing left to decide; abandon the whole trail
  const Var next = kernel_queue_next_unassigned(k);
  const uint32_t limit = k.links[next].stamp;
  uint32_t res = 0;
  while (res < k.level) {
    const Lit decision = k.trail[k.frames[res + 1]];
    if (k.links[var_of(decision)].stamp <= limit)
      break;
    res++;
  }
  return res;
}

ApplyResult restart_apply(Kernel &k) {
  assert(restart_guard(k));
  const uint32_t keep = reuse_trail(k);
  if (keep > 0)
    k.stats.restarts_reused++;
  if (keep < k.level)
    kernel_backtrack(k, keep);
  k.stats.restarts++;
  k.restart.floor_conflicts = k.stats.conflicts + FLOOR_INTERVAL;
  return ApplyResult::Ok;
}

TACTIX_REGISTER_RULE(Restart, RuleTier::Macro, restart_guard,
                     restart_advised, restart_apply);

} // namespace tactix
