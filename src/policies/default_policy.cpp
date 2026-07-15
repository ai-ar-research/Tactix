#include "policies/default_policy.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include "rules/rephase/rephase.hpp"
#include "rules/restart/restart.hpp"
#include "rules/subsume_forward/subsume_forward.hpp"
#include "rules/switchmode/switchmode.hpp"

#include <cmath>

namespace tactix {

// Restart trigger: fast/slow glue EMAs with kissat's bias-corrected
// warm-up (smooth.c) and per-mode persistence (averages.h averages[2]).
// The 07-13 experiment showed the kissat constants over-fire on PLAIN
// seeded EMAs; this is the faithful package the constants presuppose.
// (Constants live once, in default_policy.hpp.)

void DefaultPolicy::observe_glue(const Kernel &k, uint32_t glue) {
  ModeAverages &avg = averages[k.stable];
  if (!avg.initialized) { // lazy, once per mode (kissat init_averages)
    avg.fast_glue.init(EMA_FAST_WINDOW);
    avg.slow_glue.init(EMA_SLOW_WINDOW);
    avg.initialized = true;
  }
  avg.fast_glue.update(glue);
  avg.slow_glue.update(glue);
}

// blocking restarts (glucose Solver.cc:1496-1505, MIT, transliterated;
// recompositions recorded at the constants in the header): at each focused
// conflict, record the trail depth; once past BLOCK_LOWER_BOUND conflicts
// with a full window, a trail deeper than BLOCK_R times the window average
// means the search may be near a model — discard the fast glue signal so
// the restart trigger must re-diverge from parity (glucose fastclear)
void DefaultPolicy::observe_trail(const Kernel &k) {
  auto &b = block;
  if (b.ring.empty())
    b.ring.resize(BLOCK_TRAIL_QUEUE, 0);
  const uint32_t depth = static_cast<uint32_t>(k.trail.size());
  b.sum += depth;
  b.sum -= b.ring[b.pos];
  b.ring[b.pos] = depth;
  b.pos = (b.pos + 1) % BLOCK_TRAIL_QUEUE;
  if (b.filled < BLOCK_TRAIL_QUEUE) {
    b.filled++;
    return;
  }
  if (k.stats.conflicts <= BLOCK_LOWER_BOUND)
    return;
  const double avg_depth =
      static_cast<double>(b.sum) / static_cast<double>(BLOCK_TRAIL_QUEUE);
  if (depth > BLOCK_R * avg_depth) {
    ModeAverages &avg = averages[0]; // focused-only by construction
    avg.fast_glue.value = avg.slow_glue.value;
    avg.fast_glue.biased = avg.slow_glue.value * (1.0 - avg.fast_glue.exp);
    b.blocked++;
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

// kissat restart.c kissat_restarting: the conflicts limit gates both
// modes; stable defers to reluctant doubling, focused to the glue EMAs
bool DefaultPolicy::want_restart(const Kernel &k) const {
  if (k.stats.conflicts < restart.next_conflicts)
    return false;
  if (k.stable)
    return reluctant.trigger;
  const ModeAverages &avg = averages[0];
  return RESTART_MARGIN * avg.slow_glue.value <= avg.fast_glue.value;
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
    // kissat mode.c: only the focused entry refreshes the restart limit;
    // per-mode averages persist across switches (init_averages is
    // idempotent — no reset here)
    update_restart_limit(k);
  }
}

// kissat restart.c kissat_update_focused_restart_limit: interval =
// restartint + log10(restarts+9) - 1; called after focused restarts and
// on entering focused mode (the limit is left stale while stable —
// reluctant doubling governs there)
void DefaultPolicy::update_restart_limit(const Kernel &k) {
  uint64_t delta = RESTART_INTERVAL;
  if (k.stats.restarts)
    delta += static_cast<uint64_t>(
        std::log10(static_cast<double>(k.stats.restarts) + 9) - 1.0);
  restart.next_conflicts = k.stats.conflicts + delta;
}

bool DefaultPolicy::want_reduce(const Kernel &k) const {
  return k.stats.conflicts >= reduce.next_conflicts;
}

// kissat rephase.c kissat_rephasing: stable mode only
bool DefaultPolicy::want_rephase(const Kernel &k) const {
  return k.stable && k.stats.conflicts > rephase.next_conflicts;
}

// kissat kimits.h UPDATE_CONFLICT_LIMIT(rephase, rephased, NLOG3N):
// interval grows as rephaseint · n·log10(n+9)³ of the rephase count
void DefaultPolicy::update_rephase_limit(const Kernel &k) {
  const double lg = std::log10(static_cast<double>(k.stats.rephased) + 9);
  rephase.next_conflicts =
      k.stats.conflicts +
      static_cast<uint64_t>(REPHASE_INTERVAL *
                            static_cast<double>(k.stats.rephased) * lg * lg *
                            lg);
}

// kissat kimits.c kissat_scale_delta: inprocessing intervals scale with
// the formula size (factor 4.5·(log10(C+1)−5)² + 25, C = irredundant
// clause count) so the inprocessing share of runtime stays bounded on
// large instances; the eliminate-slot constants presuppose this scaling
// (constants travel with their machinery — the restart-EMA lesson)
static uint64_t scale_inprocessing_delta(const Kernel &k, uint64_t delta) {
  const double f =
      std::log10(static_cast<double>(kernel_irredundant_clauses(k)) + 1) - 5;
  return static_cast<uint64_t>((4.5 * f * f + 25) *
                               static_cast<double>(delta));
}

bool DefaultPolicy::want_subsume(const Kernel &k) const {
  if (!subsume.initialized || k.stats.conflicts < subsume.next_conflicts)
    return false;
  // new-material gate (see default_policy.hpp): skip rounds that cannot
  // find anything instead of paying the pass + root restart
  return !subsume.ran || k.binary_ids.size() > subsume.binaries_seen ||
         kernel_root_fixed(k) > subsume.root_fixed_seen;
}

// kissat kimits.h UPDATE_CONFLICT_LIMIT(eliminate, eliminations, NLOG2N,
// true): interval grows as eliminateint · n·log10(n+9)², size-scaled
void DefaultPolicy::update_subsume_limit(const Kernel &k) {
  uint64_t n = k.stats.forward_subsumptions;
  if (!n)
    n = 1;
  const double lg = std::log10(static_cast<double>(n) + 9);
  const uint64_t delta = static_cast<uint64_t>(
      SUBSUME_INTERVAL * static_cast<double>(n) * lg * lg);
  subsume.next_conflicts =
      k.stats.conflicts + scale_inprocessing_delta(k, delta);
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
  // the first inprocessing limit needs the ingested formula's size
  // (kissat inits limits after parsing; INIT_CONFLICT_LIMIT(eliminate,
  // true) is likewise size-scaled)
  if (SUBSUME_IN_DEFAULT && !subsume.initialized) {
    subsume.initialized = true;
    subsume.next_conflicts =
        k.stats.conflicts +
        scale_inprocessing_delta(k, SUBSUME_INTERVAL_INIT);
  }
  bool lucky_tried = false;
  for (;;) {
    const ApplyResult r = propagate_apply(k);
    if (r == ApplyResult::Unsat)
      return 20;
    // S5 Lucky: once, at search start after the initial root fixpoint
    // (kissat search.c luckyearly; luckylate joins with preprocessing)
    if (!lucky_tried && r == ApplyResult::Ok) {
      lucky_tried = true;
      if (lucky_guard(k) && lucky_apply(k) == ApplyResult::Sat)
        return 10;
    }
    if (r == ApplyResult::Conflict) {
      if (analyze_apply(k) == ApplyResult::Unsat)
        return 20;
      observe_glue(k, k.pending.glue); // policy observes; Analyze is pure
      if (k.stable)
        tick_reluctant(); // per learned clause (kissat learn.c)
      else
        observe_trail(k); // blocking restarts (glucose; focused regime)
      {
        // chrono (kissat learn.c): long backjumps waste the prefix —
        // step back one level instead when the gap exceeds the limit
        const uint32_t jump = k.pending.jump_level;
        const uint32_t back = k.level - 1;
        backjump_apply_to(k, back - jump > CHRONO_LEVELS ? back : jump);
      }
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
      if (!k.stable) // kissat restart.c: focused-only limit update
        update_restart_limit(k);
      continue;
    }
    if (want_rephase(k) && rephase_guard(k)) {
      // kissat couples a backtrack-to-root into kissat_rephase; here the
      // same effect is composed from the rule vocabulary (deviation
      // recorded in rephase.cpp) — the root restart IS a restart
      if (restart_guard(k))
        restart_apply_to(k, 0);
      // rotation B,B,I,B,B,O — kissat's B,W,I,B,W,O with Best standing in
      // for the phase-4 Walking slots: the destructive types (I/O wipe
      // every saved phase) keep kissat's 1/3 slot frequency; a plain
      // B,I,B,O rotation doubled their share and measurably hurt the
      // transition instances (s055) without Walk's repair after them
      static constexpr uint64_t ROTATION[6] = {
          REPHASE_BEST, REPHASE_BEST, REPHASE_INVERTED,
          REPHASE_BEST, REPHASE_BEST, REPHASE_ORIGINAL};
      rephase_apply_type(k, ROTATION[k.stats.rephased % 6]);
      update_rephase_limit(k);
      continue;
    }
    // C1 forward subsumption in the kissat eliminate slot (search.c
    // checks eliminate after rephase, before decide). The rule needs a
    // quiescent root; kissat's coupled backtrack is composed from the
    // rule vocabulary as with Rephase — the root restart IS a restart
    if (want_subsume(k)) {
      if (k.level > 0 && restart_guard(k))
        restart_apply_to(k, 0);
      if (subsume_forward_guard(k)) {
        subsume_forward_apply(k);
        // the new-material watermarks move only when a round actually ran
        subsume.ran = true;
        subsume.binaries_seen = k.binary_ids.size();
        subsume.root_fixed_seen = kernel_root_fixed(k);
      }
      update_subsume_limit(k);
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
