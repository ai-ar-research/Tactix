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

ApplyResult restart_apply(Kernel &k) {
  assert(restart_guard(k));
  kernel_backtrack(k, 0);
  k.stats.restarts++;
  k.restart.floor_conflicts = k.stats.conflicts + FLOOR_INTERVAL;
  return ApplyResult::Ok;
}

TACTIX_REGISTER_RULE(Restart, RuleTier::Macro, restart_guard,
                     restart_advised, restart_apply);

} // namespace tactix
