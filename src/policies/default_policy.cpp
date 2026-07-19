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
#include <cstring>

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
[[maybe_unused]] static uint64_t scale_inprocessing_delta(const Kernel &k,
                                                          uint64_t delta) {
  const double f =
      std::log10(static_cast<double>(kernel_irredundant_clauses(k)) + 1) - 5;
  return static_cast<uint64_t>((4.5 * f * f + 25) *
                               static_cast<double>(delta));
}

bool DefaultPolicy::want_inprocess(const Kernel &k) const {
  if (!inprocess.initialized || k.stats.conflicts < inprocess.next_conflicts)
    return false;
  // new-material gate (see default_policy.hpp): skip passes that cannot
  // find anything instead of paying the scan + root restart
  return !inprocess.ran || k.binary_ids.size() > inprocess.binaries_seen ||
         kernel_root_fixed(k) > inprocess.root_fixed_seen;
}

// kissat kimits.h UPDATE_CONFLICT_LIMIT(eliminate, eliminations, NLOG2N,
// true): interval grows as eliminateint · n·log10(n+9)², size-scaled
void DefaultPolicy::update_inprocess_limit(const Kernel &k) {
  // Increment 1 is PREPROCESSING-ONLY: one simplification phase at conflicts=0,
  // then never again. During-search BVE phases (each a root descent + hundreds
  // of eliminations) knock high-variance SAT searches off their trajectory
  // (Tier B: FmlaEquiv 27→198). They return in increment 2, once the shared
  // dense OccIndex + a real tick budget make a during-search phase cheap and
  // small enough to be SAT-safe. The growing periodic interval below (kissat
  // NLOG2N, phase-count-scaled) is what increment 2 restores.
  inprocess.next_conflicts = UINT64_MAX;
  (void)k;
#if 0 // increment 2:
  uint64_t n = inprocess.rounds; // policy-local growth key (rev 17)
  if (!n)
    n = 1;
  const double lg = std::log10(static_cast<double>(n) + 9);
  const uint64_t delta = static_cast<uint64_t>(
      SUBSUME_INTERVAL * static_cast<double>(n) * lg * lg);
  inprocess.next_conflicts =
      k.stats.conflicts + scale_inprocessing_delta(k, delta);
#endif
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
  if (SLOT_IN_DEFAULT) {
    // the slot itself: registry-driven (RuleTier::Inprocess), registry
    // order; default composition per default_policy.hpp. A preposed
    // (nonempty) slot is left untouched — to run nothing, prepose
    // disabled entries, not an empty vector
    if (inprocess_slot.empty()) {
      // Inprocessing-revamp increment 1: BVE only. BVE run to a bounded fixpoint
      // as a SAT-safe preprocessing pass (at conflicts=0) + rare, interval-grown
      // inprocessing is net-positive (Tier B, docs/inprocessing-revamp-design.md).
      // SubsumeForward/Strengthen join once the shared dense OccIndex makes them
      // affordable (increment 3); Probe/Vivify/Backbone with tick budgets
      // (increment 4). The slot stays registry-driven + posable (phase-6 arms).
      const std::vector<RuleVTable> &rules = tactix_rules();
      for (int i = 0; i < static_cast<int>(rules.size()); i++)
        if (rules[i].tier == RuleTier::Inprocess)
          inprocess_slot.push_back({i, std::strcmp(rules[i].name, "BVE") == 0});
    }
    if (!inprocess.initialized) {
      inprocess.initialized = true;
      // Preprocess: the first phase fires at conflicts=0, before any search
      // state exists, so simplification never perturbs an in-progress SAT
      // search (the SAT-safety finding). Subsequent phases are rare (the
      // interval grows super-linearly in phase count, update_inprocess_limit).
      inprocess.next_conflicts = k.stats.conflicts;
    }
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
    // the inprocessing SLOT in the kissat eliminate schedule (search.c
    // checks eliminate after rephase, before decide). Rules need a
    // quiescent root; kissat's coupled backtrack is composed from the
    // rule vocabulary as with Rephase — the root restart IS a restart.
    // Entries run in slot order through the registry vtable (dynamic
    // dispatch — macro tier, off the hot path by construction); a rule
    // that breaks quiescence (e.g. a strengthened unit) makes later
    // guards fail and the loop re-propagates
    if (want_inprocess(k)) {
      // A simplification PHASE: descend to a quiescent root (a transient flush
      // that keeps saved/target phases — kernel_backtrack(update_phases=true) —
      // so search re-descends toward the same region afterward), then run the
      // enabled techniques to a BOUNDED fixpoint. Bounding matters: each apply
      // rebuilds a rule-local occurrence index (~O(arena)), so an UNBOUNDED
      // fixpoint is ~O(n²) and drowns even trivial instances (square 2s→TO).
      // INPROCESS_PHASE_BUDGET caps the applies (a coarse effort proxy — becomes
      // a real tick budget in increment 2 once the shared OccIndex removes the
      // per-apply rebuild).
      if (k.level > 0 && restart_guard(k))
        restart_apply_to(k, 0);
      // Attach the shared occurrence index for the phase (increment 2): the
      // occurrence consumers (BVE) read it instead of rebuilding a ~O(arena)
      // index per apply. Detached on EVERY exit below so k.dense never dangles.
      kernel_dense_connect(k, inprocess_occ);
      uint32_t applied = 0;
      bool phase_unsat = false;
      const std::vector<RuleVTable> &rules = tactix_rules();
      for (;;) {
        bool round_ran = false; // did any rule make PROGRESS this round?
        for (const SlotEntry &e : inprocess_slot) {
          if (!e.enabled)
            continue;
          const RuleVTable &r = rules[e.rule];
          if (r.guard(k)) {
            const ApplyResult ar = r.apply(k);
            if (ar == ApplyResult::Unsat || k.inconsistent) {
              phase_unsat = true;
              break;
            }
            if (ar == ApplyResult::Ok) {
              round_ran = true;
              applied++;
            }
          }
        }
        if (phase_unsat || !round_ran)
          break; // UNSAT, or fixpoint: no enabled rule made progress
        // drain any root units the round asserted so the next round sees them
        const ApplyResult pr = propagate_apply(k);
        if (pr == ApplyResult::Unsat) {
          phase_unsat = true;
          break;
        }
        if (applied >= INPROCESS_PHASE_BUDGET)
          break; // effort bound
      }
      kernel_dense_disconnect(k);
      if (phase_unsat)
        return 20;
      // The phase FIRED (whether or not it simplified anything): count it (the
      // interval-growth key) and refresh the new-material watermarks so an
      // unproductive pass does not re-fire every interval — that repeated root
      // restart is exactly what disrupts a high-variance SAT search.
      inprocess.ran = true;
      inprocess.rounds++;
      inprocess.binaries_seen = k.binary_ids.size();
      inprocess.root_fixed_seen = kernel_root_fixed(k);
      update_inprocess_limit(k);
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
