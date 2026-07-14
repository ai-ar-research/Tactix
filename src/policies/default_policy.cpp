#include "policies/default_policy.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include "rules/restart/restart.hpp"
#include "rules/switchmode/switchmode.hpp"

#include <cmath>

namespace tactix {

// Glucose-style EMA restart trigger (Audemard–Simon): restart when the
// short-term glue average exceeds the long-term one by a margin — search
// quality is degrading, so abandon the prefix. Focused mode only.
// NOTE (2026-07-13, evaluated and reverted): kissat's defaults (margin
// 1.10, interval 1+log) were tried and measured WORSE here (s055 focused-
// only 6.9M → 7.9M conflicts, restart interval 18 vs kissat's own 41 at
// the same constants) — our plain seeded EMAs over-fire at that margin;
// kissat's constants presuppose its bias-corrected EMA warm-up
// (smooth.c). Revisit together with per-mode bias-corrected averages.
// (The constants themselves live once, in default_policy.hpp.)

void DefaultPolicy::observe_glue(uint32_t glue) {
  if (restart.ema_initialized) {
    restart.fast_glue += (glue - restart.fast_glue) / 32.0;
    restart.slow_glue += (glue - restart.slow_glue) / 16384.0;
  } else {
    restart.fast_glue = restart.slow_glue = glue;
    restart.ema_initialized = true;
  }
}

// reluctant doubling (Knuth's formulation of Luby; kissat reluctant.c):
// ticked once per learned clause while stable, arms `trigger` when the
// current wait elapses
void DefaultPolicy::tick_reluctant() {
  if (reluctant.trigger)
    return;
  if (--reluctant.wait)
    return;
  uint64_t u = reluctant.u, v = reluctant.v;
  if ((u & -u) == v) {
    u++;
    v = 1;
  } else {
    v *= 2;
  }
  uint64_t wait = v * RELUCTANT_PERIOD;
  if (wait > RELUCTANT_LIMIT) {
    u = v = 1;
    wait = RELUCTANT_PERIOD;
  }
  reluctant.trigger = true;
  reluctant.wait = wait;
  reluctant.u = u;
  reluctant.v = v;
}

bool DefaultPolicy::want_restart(const Kernel &k) const {
  if (k.stable)
    return reluctant.trigger;
  return restart.ema_initialized &&
         k.stats.conflicts >= restart.floor_conflicts &&
         restart.fast_glue > RESTART_MARGIN * restart.slow_glue;
}

bool DefaultPolicy::want_switch_mode(const Kernel &k) const {
  if (mode.count & 1) // in stable: leave after the ticks budget
    return k.stats.ticks >= mode.ticks_limit;
  return k.stats.conflicts >= mode.conflicts_limit;
}

// kissat mode.c update_mode_limit: stable phases get the ticks the
// preceding focused phase used (wall-time share stays balanced); focused
// intervals grow as modeint * n·log⁴n of the switch count
void DefaultPolicy::update_mode_limits(const Kernel &k) {
  if (mode.count & 1) { // just entered stable
    const uint64_t focused_ticks = k.stats.ticks - mode.ticks_at_focus_entry;
    mode.ticks_limit = k.stats.ticks + focused_ticks;
  } else { // just entered focused
    const uint64_t n = (mode.count / 2) + 1;
    const double log4 = std::pow(std::log10(static_cast<double>(n) + 9), 4);
    mode.conflicts_limit =
        k.stats.conflicts +
        static_cast<uint64_t>(MODE_INTERVAL * static_cast<double>(n) * log4);
    mode.ticks_at_focus_entry = k.stats.ticks;
  }
  // per-mode averages restart fresh (kissat init_averages on switch)
  restart.ema_initialized = false;
  update_restart_limit(k);
}

void DefaultPolicy::update_restart_limit(const Kernel &k) {
  restart.floor_conflicts = k.stats.conflicts + RESTART_FLOOR_INTERVAL;
}

bool DefaultPolicy::want_reduce(const Kernel &k) const {
  return k.stats.conflicts >= reduce.next_conflicts;
}

// arithmetically growing reduce interval
void DefaultPolicy::update_reduce_limit(const Kernel &k) {
  reduce.next_conflicts = k.stats.conflicts + REDUCE_INTERVAL_INIT +
                          REDUCE_INTERVAL_STEP * k.stats.reductions;
}

size_t GlueTieredReduction::victims(const Kernel &, size_t candidates) {
  return reduce_canonical_victims(candidates);
}

Lit DefaultBranching::pick(Kernel &k) {
  assert(k.unassigned > 0);
  const Var v = kernel_next_decision_var(k);
  return kernel_decide_lit(k, v);
}

// Phase-1 shape of kissat/src/search.c's guard->apply chain: propagate,
// analyze+backjump on conflict, SAT when everything is assigned, else
// decide — with Reduce/SwitchMode/Restart scheduled between conflicts
// (in kissat search.c's check order) by THIS policy's state (rev 17:
// rules carry no scheduling). S3/S5 join in phase 2.
int DefaultPolicy::solve(Kernel &k, volatile sig_atomic_t *stop) {
  if (k.inconsistent)
    return 20;
  for (;;) {
    const ApplyResult r = propagate_apply(k);
    if (r == ApplyResult::Unsat)
      return 20;
    if (r == ApplyResult::Conflict) {
      if (analyze_apply(k) == ApplyResult::Unsat)
        return 20;
      observe_glue(k.pending.glue); // policy observes; Analyze stays pure
      if (k.stable)
        tick_reluctant(); // per learned clause (kissat learn.c)
      backjump_apply(k);
      continue;
    }
    if (k.unassigned == 0)
      return 10;
    if (want_reduce(k) && reduce_guard(k)) {
      // sub-policy chooses HOW MUCH of the rule's ranked candidates die
      ReductionPolicy &rp = reduction ? *reduction : default_reduction_;
      reduce_candidates(k, reduce_scratch_);
      reduce_apply_refs(
          k, reduce_scratch_.data(),
          std::min(rp.victims(k, reduce_scratch_.size()),
                   reduce_scratch_.size()));
      update_reduce_limit(k);
      continue;
    }
    if (want_switch_mode(k) && switchmode_guard(k)) {
      switchmode_apply(k);
      mode.count++;
      if (k.stable) // entering stable: fresh reluctant sequence
        reluctant = {};
      update_mode_limits(k);
      continue;
    }
    if (want_restart(k) && restart_guard(k)) {
      restart_apply(k);
      reluctant.trigger = false; // consumed (kissat reluctant.h)
      update_restart_limit(k);
      continue;
    }
    if (stop && *stop)
      return 0;
    // sub-policy chooses the branch (any unassigned literal is legal, N1)
    BranchingPolicy &bp = branching ? *branching : default_branching_;
    decide_apply(k, bp.pick(k), false);
  }
}

int default_solve(Kernel &k, volatile sig_atomic_t *stop) {
  DefaultPolicy policy;
  return policy.solve(k, stop);
}

} // namespace tactix
