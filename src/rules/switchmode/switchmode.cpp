// S4 SwitchMode: flip the active instance-ranking mode between focused
// (VMTF queue) and stable (EVSIDS score heap). Transliterated from
// kissat/src/mode.c switch_to_stable_mode / switch_to_focused_mode (MIT).
// Pure H-state (proof delta: none) — flipping is sound in any state, so
// the guard only excludes a refuted kernel. WHEN to switch (kissat's
// conflicts-in / ticks-out alternation) is policy state (plan §5 rev 17),
// as are the per-mode restart regimes (EMA vs reluctant doubling).
#include "rules/switchmode/switchmode.hpp"
#include "policies/heuristics.hpp"

namespace tactix {

bool switchmode_guard(const Kernel &k) { return !k.inconsistent; }

ApplyResult switchmode_apply(Kernel &k) {
  assert(switchmode_guard(k));
  k.h->stable = !k.h->stable;
  k.stats.switched++;
  if (k.h->stable) {
    // entering stable: every var must be reachable via the score heap
    // (kissat bump.c kissat_update_scores)
    heuristics_update_scores(k);
  } else {
    // entering focused: the VMTF search cache must point at the queue
    // tail again (kissat queue.c kissat_reset_search_of_queue)
    heuristics_reset_queue_search(k);
  }
  return ApplyResult::Ok;
}

// Policy operation (plan §4 rev 59): H-only (flips the decision-ranking
// heuristic — the VMTF-vs-EVSIDS MAB arm), no proof delta. Registered in-place
// with the same name/tier/fns, so its id is unchanged (Track H · H1).
TACTIX_REGISTER_POLICY_OP(SwitchMode, RuleTier::Macro, switchmode_guard,
                          switchmode_apply);

} // namespace tactix
