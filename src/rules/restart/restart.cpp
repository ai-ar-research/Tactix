// Restart: abandon the assignment prefix (kissat restart.c). WHEN is
// policy state (focused: Glucose EMA scheme; stable: reluctant doubling —
// both in DefaultPolicy); HOW — including trail reuse — is here.
#include "rules/restart/restart.hpp"
#include "policies/heuristics.hpp"

namespace tactix {

// soundness: abandoning the assignment prefix is always sound; only a
// recorded-but-unanalyzed conflict pins the trail (stale conflict record).
// WHEN to restart is the policy's decision and state (plan §5) —
// this module only knows HOW.
bool restart_guard(const Kernel &k) {
  return !k.conflict_valid && !k.inconsistent && k.level > 0;
}

// trail reuse (kissat restart.c reuse_focused_trail/reuse_stable_trail):
// keep the decision levels whose decision variable ranks higher (VMTF
// stamp / EVSIDS score, per the active mode) than the next decision
// candidate — after a full restart the heuristic would re-decide exactly
// that prefix, so re-propagating it is pure waste.
uint32_t restart_reuse_level_for(Kernel &k, Var next) {
  assert(k.h && next < k.nvars);
  assert(k.values[make_lit(next, false)] == 0);
  uint32_t res = 0;
  if (k.h->stable) {
    const double limit = k.h->scores.score[next];
    while (res < k.level) {
      const Lit decision = k.trail[k.frames[res + 1]];
      if (k.h->scores.score[var_of(decision)] <= limit)
        break;
      res++;
    }
  } else {
    const uint32_t limit = k.h->links[next].stamp;
    while (res < k.level) {
      const Lit decision = k.trail[k.frames[res + 1]];
      if (k.h->links[var_of(decision)].stamp <= limit)
        break;
      res++;
    }
  }
  return res;
}

uint32_t restart_reuse_level(Kernel &k) {
  if (k.unassigned == 0)
    return 0; // nothing left to decide; abandon the whole trail
  if (!k.h)
    return 0; // unguided (no `H`, H5): no ranking to reuse — full restart
  const Var next = k.h->stable ? heuristics_heap_next_unassigned(k)
                               : heuristics_queue_next_unassigned(k);
  return restart_reuse_level_for(k, next);
}

ApplyResult restart_apply_to(Kernel &k, uint32_t keep_level) {
  assert(restart_guard(k));
  if (keep_level > k.level)
    keep_level = k.level; // keep at most the whole trail
  if (keep_level > 0)
    k.stats.restarts_reused++;
  if (keep_level < k.level) {
    // kissat `kissat_backtrack_in_consistent_state` (backtrack.c:150): snapshot
    // target/best FIRST, at the full trail, then unwind. No level-1 step here —
    // that is only on the conflict path.
    if (k.h)
      heuristics_update_target_phases(k);
    kernel_backtrack(k, keep_level);
  }
  k.stats.restarts++;
  return ApplyResult::Ok;
}

ApplyResult restart_apply(Kernel &k) {
  assert(restart_guard(k));
  return restart_apply_to(k, restart_reuse_level(k));
}

// generic instance surface (plan §5.1 v1): payload = keep-level.
// Ranked: the canonical (heuristic reuse) level first, then the rest of
// [0, level] descending — deeper keeps preserve more work
static uint32_t restart_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  if (cap == 0)
    return 0;
  const uint32_t canonical = restart_reuse_level(k);
  uint32_t n = 0;
  out[n++] = canonical;
  for (uint32_t lvl = k.level;; lvl--) {
    if (n >= cap)
      break;
    if (lvl != canonical)
      out[n++] = lvl;
    if (lvl == 0)
      break;
  }
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
