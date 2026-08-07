// Transliterated from kissat/src/rephase.c (MIT): each type rewrites the saved
// phases, the target phases then restart from them, and consuming best resets
// its watermark. Deviations, ledgered in docs/implementation-notes.md: (1)
// kissat couples a backtrack-to-root plus trail flush into kissat_rephase, but
// that composition belongs to the policy, keeping this rule pure `H` so any
// legal schedule may apply it mid-trail; (2) kissat's W slots replace Best with
// a walk, while DefaultPolicy's rotation augments Best with one.
#include "policies/ops/rephase/rephase.hpp"
#include "policies/heuristics.hpp"

#include <algorithm>

namespace tactix {

// Requires `H`: Rephase rewrites the saved/target/best phase arrays, which are
// `H` state — a pure precondition of a policy operation that needs `H` (§5).
bool rephase_guard(const Kernel &k) {
  return !k.inconsistent && k.h != nullptr;
}

ApplyResult rephase_apply_type(Kernel &k, uint64_t type) {
  assert(rephase_guard(k));
  switch (type) {
  case REPHASE_BEST:
    for (Var v = 0; v < k.nvars; v++)
      if (k.h->best[v])
        k.h->saved[v] = k.h->best[v];
    k.h->best_assigned = 0; // consumed: start collecting a fresh best
    break;
  case REPHASE_INVERTED:
    std::fill(k.h->saved.begin(), k.h->saved.end(), static_cast<int8_t>(-1));
    break;
  case REPHASE_ORIGINAL:
    std::fill(k.h->saved.begin(), k.h->saved.end(), static_cast<int8_t>(1));
    break;
  default:
    return ApplyResult::NoOp; // unknown payload: never unsound
  }
  // target phases restart from the fresh saved ones (kissat reset_phases)
  std::copy(k.h->saved.begin(), k.h->saved.end(), k.h->target.begin());
  k.h->target_assigned = 0;
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

// Policy operation (plan §4): H-only, no proof delta.
TACTIX_REGISTER_POLICY_OP(Rephase, RuleTier::Macro, rephase_guard, rephase_apply,
                          rephase_instances, rephase_apply_instance);

} // namespace tactix
