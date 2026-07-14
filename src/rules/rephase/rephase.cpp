// Transliterated from kissat/src/rephase.c (MIT): each type rewrites the
// saved phases, then the target phases restart from the fresh saved ones
// (target = saved, watermark reset); consuming best resets its watermark
// too. Deviations (recorded): (1) kissat couples a backtrack-to-root +
// trail flush into kissat_rephase — under our contract that composition
// belongs to the policy (Restart keep-level 0 + Propagate), keeping this
// rule pure H so any legal schedule may apply it mid-trail; (2) kissat's
// schedule slots for Walking are deferred to phase 4 (S6 Walk) — the
// DefaultPolicy rotation is B,I,B,O.
#include "rules/rephase/rephase.hpp"

#include <algorithm>

namespace tactix {

bool rephase_guard(const Kernel &k) { return !k.inconsistent; }

ApplyResult rephase_apply_type(Kernel &k, uint64_t type) {
  assert(rephase_guard(k));
  switch (type) {
  case REPHASE_BEST:
    for (Var v = 0; v < k.nvars; v++)
      if (k.best[v])
        k.saved[v] = k.best[v];
    k.best_assigned = 0; // consumed: start collecting a fresh best
    break;
  case REPHASE_INVERTED:
    std::fill(k.saved.begin(), k.saved.end(), static_cast<int8_t>(-1));
    break;
  case REPHASE_ORIGINAL:
    std::fill(k.saved.begin(), k.saved.end(), static_cast<int8_t>(1));
    break;
  default:
    return ApplyResult::NoOp; // unknown payload: never unsound
  }
  // target phases restart from the fresh saved ones (kissat reset_phases)
  std::copy(k.saved.begin(), k.saved.end(), k.target.begin());
  k.target_assigned = 0;
  k.stats.rephased++;
  return ApplyResult::Ok;
}

ApplyResult rephase_apply(Kernel &k) {
  return rephase_apply_type(k, REPHASE_BEST);
}

static uint32_t rephase_instances(Kernel &, uint64_t *out, uint32_t cap) {
  static constexpr uint64_t all[3] = {REPHASE_BEST, REPHASE_INVERTED,
                                      REPHASE_ORIGINAL};
  const uint32_t n = cap < 3 ? cap : 3;
  for (uint32_t i = 0; i < n; i++)
    out[i] = all[i];
  return n;
}

static ApplyResult rephase_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return rephase_apply(k);
  return rephase_apply_type(k, inst);
}

TACTIX_REGISTER_RULE(Rephase, RuleTier::Macro, rephase_guard, rephase_apply,
                     rephase_instances, rephase_apply_instance);

} // namespace tactix
