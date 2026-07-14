// S4 SwitchMode: flip the active instance-ranking mode between focused
// (VMTF queue) and stable (EVSIDS score heap). Transliterated from
// kissat/src/mode.c switch_to_stable_mode / switch_to_focused_mode (MIT).
// Pure H-state (proof delta: none) — flipping is sound in any state, so
// the guard only excludes a refuted kernel. WHEN to switch (kissat's
// conflicts-in / ticks-out alternation) is policy state (plan §5 rev 17),
// as are the per-mode restart regimes (EMA vs reluctant doubling).
#include "rules/switchmode/switchmode.hpp"

namespace tactix {

bool switchmode_guard(const Kernel &k) { return !k.inconsistent; }

ApplyResult switchmode_apply(Kernel &k) {
  assert(switchmode_guard(k));
  k.stable = !k.stable;
  k.stats.switched++;
  if (k.stable) {
    // entering stable: every var must be reachable via the score heap
    // (kissat bump.c kissat_update_scores)
    kernel_update_scores(k);
  } else {
    // entering focused: the VMTF search cache must point at the queue
    // tail again (kissat queue.c kissat_reset_search_of_queue)
    kernel_reset_queue_search(k);
  }
  return ApplyResult::Ok;
}

TACTIX_REGISTER_RULE(SwitchMode, RuleTier::Macro, switchmode_guard,
                     switchmode_apply);

} // namespace tactix
