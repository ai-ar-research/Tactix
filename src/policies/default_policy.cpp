#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include "policies/ops/rephase/rephase.hpp"
#include "rules/restart/restart.hpp"
#include "rules/subsume_forward/subsume_forward.hpp"
#include "policies/ops/switchmode/switchmode.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <optional>

namespace tactix {

// Restart trigger: fast/slow glue EMAs with kissat's bias-corrected
// warm-up (smooth.c) and per-mode persistence (averages.h averages[2]).
// The 07-13 experiment showed the kissat constants over-fire on PLAIN
// seeded EMAs; this is the faithful package the constants presuppose.
// (Constants live once, in default_policy.hpp.)

void DefaultPolicy::observe_glue(const Kernel &k, uint32_t glue) {
  ModeAverages &avg = averages[k.h->stable];
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
  if (k.h->stable)
    return reluctant.trigger;
  const ModeAverages &avg = averages[0];
  return RESTART_MARGIN * avg.slow_glue.value <= avg.fast_glue.value;
}

bool DefaultPolicy::want_switch_mode(const Kernel &k) const {
  if (mode.count & 1) // in stable: leave after the ticks budget
    return search_ticks(k) >= mode.ticks_limit;
  return k.stats.conflicts >= mode.conflicts_limit;
}

// kissat mode.c update_mode_limit: stable phases get the ticks the
// preceding focused phase used (wall-time share stays balanced); focused
// intervals grow as modeint * n·log⁴n of the switch count
void DefaultPolicy::update_mode_limits(const Kernel &k) {
  if (mode.count & 1) { // just entered stable
    const uint64_t focused_ticks = search_ticks(k) - mode.ticks_at_focus_entry;
    mode.ticks_limit = search_ticks(k) + focused_ticks;
  } else { // just entered focused
    const uint64_t n = (mode.count / 2) + 1;
    const double log4 = std::pow(std::log10(static_cast<double>(n) + 9), 4);
    mode.conflicts_limit =
        k.stats.conflicts +
        static_cast<uint64_t>(MODE_INTERVAL * static_cast<double>(n) * log4);
    mode.ticks_at_focus_entry = search_ticks(k);
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
  return k.h->stable && k.stats.conflicts > rephase.next_conflicts;
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

// --- effort ledger (rev 53, docs/design/effort-scheduler-design.md) ---------
// Each inprocessing rule is metered in its OWN currency, because one unit of
// work is not comparable across rules (a BVE resolvent, a subsumption check and
// a propagation tick cost different things). These readers are the rules' own
// work counters; kissat's statistics.eliminate_resolutions / forward_steps /
// probing_ticks are the same idea.
namespace {

uint64_t meter_ticks(const Kernel &k) { return k.stats.ticks; }
uint64_t meter_bve(const Kernel &k) { return k.stats.bve_resolutions; }
uint64_t meter_strengthen(const Kernel &k) { return k.stats.strengthen_checked; }
uint64_t meter_forward(const Kernel &k) { return k.stats.forward_checked; }
uint64_t meter_transitive(const Kernel &k) { return k.stats.transitive_checked; }

// name -> {effort per mille, currency, dosed}. Fractions are kissat's
// options.h defaults (eliminateeffort/forwardeffort/vivifyeffort 100,
// backboneeffort/transitiveeffort 20). `dosed` marks the rules whose instance
// payloads survive formula mutation (literals and variables — immune to arena
// GC, cheaply re-validated), so the policy can enumerate once and apply
// candidates one at a time up to the budget. The Ref-payload rules
// (Vivify/Strengthen/SubsumeForward) cannot: the GC inside their first
// successful application relocates every clause, so a snapshot of Refs goes
// stale — they run as canonical rounds and are metered after the fact, exactly
// as kissat meters `substitute`. Congruence/Factor/Substitute expose only the
// canonical instance and have no meter at all.
// `retry_advice` marks a rule that maintains a per-variable "worth re-attempting"
// bit the POLICY consults (only E1 BVE: Kernel::bve_retry). The rule deliberately
// does not filter its own instance enumeration with it — that would hide legal
// applications from RandomPolicy and from the phase-6 learned policy, whose
// action space IS the instance surface.
struct EffortSpec {
  const char *name;
  uint32_t permille;
  uint64_t (*meter)(const Kernel &);
  bool dosed;
  bool retry_advice;
};
constexpr EffortSpec EFFORT_SPECS[] = {
    {"BVE", 100, meter_bve, true, true},
    {"Probe", 20, meter_ticks, true, false},
    {"Backbone", 20, meter_ticks, true, false},
    {"TransitiveReduce", 20, meter_transitive, true, false},
    {"Vivify", 100, meter_ticks, false, false},
    {"Strengthen", 100, meter_strengthen, false, false},
    {"SubsumeForward", 100, meter_forward, false, false},
};

const EffortSpec *effort_spec(const char *name) {
  for (const EffortSpec &s : EFFORT_SPECS)
    if (!std::strcmp(s.name, name))
      return &s;
  return nullptr; // all-or-nothing rule: no titration, no meter
}

} // namespace

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
  // TACTIX_INPROCESS_INTERVAL=N — the S1 experiment knob (L7's open question).
  // Re-enables during-search phases at a FIXED conflict interval, varying ONLY
  // the interval: no growth, no size scaling, nothing else moves. That isolation
  // is the whole point — L7 established that the cost of repeated BVE is NOT the
  // resolvent work (33 % fewer resolvents, identical wall time), leaving the
  // per-phase overhead as the suspect, and the way to test that is to see whether
  // time tracks phase COUNT. Unset = today's behaviour (preprocessing only), so
  // the default path is byte-identical.
  if (const char *iv = std::getenv("TACTIX_INPROCESS_INTERVAL")) {
    const long n = std::strtol(iv, nullptr, 10);
    if (n > 0) {
      inprocess.next_conflicts = k.stats.conflicts + static_cast<uint64_t>(n);
      return;
    }
  }
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
  const Var v = heuristics_next_decision_var(k);
  return heuristics_decide_lit(k, v);
}

// Phase-1 shape of kissat/src/search.c's guard->apply chain: propagate,
// analyze+backjump on conflict, SAT when everything is assigned, else
// decide — with Reduce/SwitchMode/Restart scheduled between conflicts
// (in kissat search.c's check order) by THIS policy's state (rev 17:
// rules carry no scheduling). S3/S5 join in phase 2.
int DefaultPolicy::solve(Kernel &k, volatile sig_atomic_t *stop) {
  if (const char *cl = std::getenv("TACTIX_CHRONO_LEVELS")) {
    const long n = std::strtol(cl, nullptr, 10);
    if (n >= 0)
      chrono_levels = static_cast<uint32_t>(n);
  }
  if (k.inconsistent)
    return 20;
  // DefaultPolicy is a GUIDED policy — it reads `H` directly. The kernel does not
  // own `H` (Track H · H7): if the caller (the session — API/main) attached one,
  // use it (so incremental state + the stepping surface persist); otherwise bring
  // our OWN for this solve, sized to the parsed formula. A bare `default_solve(k)`
  // thus works, and this IS the "policy owns its guidance" model (plan §5). Sizing
  // a fresh `H` to `k.nvars` here is bit-identical to having had `H` since parse
  // (same index-order VMTF queue, zero scores, positive saved phases).
  std::optional<ScopedHeuristics> owned_h;
  if (!k.h)
    owned_h.emplace(k);
  // the first inprocessing limit needs the ingested formula's size
  // (kissat inits limits after parsing; INIT_CONFLICT_LIMIT(eliminate,
  // true) is likewise size-scaled)
  if (SLOT_IN_DEFAULT) {
    // the slot itself: registry-driven (RuleTier::Inprocess), registry
    // order; default composition per default_policy.hpp. A preposed
    // (nonempty) slot is left untouched — to run nothing, prepose
    // disabled entries, not an empty vector
    if (inprocess_slot.empty()) {
      // Inprocessing-revamp (landed): BVE + Strengthen + SubsumeForward run as a
      // SAT-safe bounded preprocessing phase (at conflicts=0) in the kissat
      // eliminate-round structure — BVE (per-variable, budgeted, reads the shared
      // dense OccIndex) + the batch rules Strengthen/SubsumeForward (one pass per
      // outer round, keeping the index consistent by garbage+add). Net-positive:
      // Tier B 111.6 vs 116.2, Tier C 356 vs base (+3 solved). The probing V-rules
      // (Probe/Vivify/Backbone) are DEFERRED: they crack some UNSAT but disrupt
      // high-variance SAT search (battleship 18s→TO, and they even undo BVE's
      // mod4block win 8.7→267s) — Tier B 169.4, net-negative. Making them
      // SAT-safe needs per-technique effort/tick budgets + mode gating (a
      // follow-on). Substitute/TransitiveReduce also off. Slot stays
      // registry-driven + posable (phase-6 arms).
      // V6 Congruence (increment 1) joins: it ADDS gate-derived equivalence
      // binaries (a==b), and Substitute — enabled here for the collapse — runs
      // LATER in the name-sorted slot order (Congruence < Substitute) so it sees
      // those binaries in the binary graph and substitutes. Probe/Vivify/Backbone
      // stay off (SAT-disruptive); TransitiveReduce off.
      const std::vector<RuleVTable> &rules = tactix_rules();
      // TACTIX_NO_BVE: ablation knob for the bimodal-BVE investigation
      // (benchmarks/reports/2026-07-20-bve-bimodal-profiling.md — BVE is a
      // 1.84x Tier B PAR-2 loss overall but wins 2.3-2.8x on three instances,
      // so it needs a guard, not a switch). Disabling a rule is always sound;
      // unset, the slot is byte-identical to the default composition.
      const bool bve_off = std::getenv("TACTIX_NO_BVE") != nullptr;
      // TACTIX_NO_FACTOR: ablation knob to isolate Factor's effect on proof
      // verifiability (aes_decry Factor proof exceeds drat-trim's 2x-timeout
      // budget; the pre-Factor proof verified in ~77s). Disabling is always
      // sound; unset, the slot is byte-identical to the default composition.
      const bool factor_off = std::getenv("TACTIX_NO_FACTOR") != nullptr;
      for (int i = 0; i < static_cast<int>(rules.size()); i++)
        if (rules[i].tier == RuleTier::Inprocess) {
          const char *nm = rules[i].name;
          const bool on = (!std::strcmp(nm, "BVE") && !bve_off) ||
                          (!std::strcmp(nm, "Factor") && !factor_off) ||
                          !std::strcmp(nm, "Strengthen") ||
                          !std::strcmp(nm, "SubsumeForward") ||
                          !std::strcmp(nm, "Congruence") ||
                          // Factor (E2 BVA) ON: net-positive once the duplicate-
                          // binary bug was fixed (kernel_delete_binary_all_copies
                          // lets a factoring remove its whole grid even when grid
                          // binaries are duplicated; before, deletes refused →
                          // bloat → no benefit). x9 801k→453k conflicts (39s→23s),
                          // mp1 4.19M→3.66M, base search unchanged (only Factor's
                          // grid pairs are removed). Full kissat-level benefit
                          // still wants SBVA selection (S2) + large remainders (S4).
                          !std::strcmp(nm, "Substitute");
          SlotEntry e;
          e.rule = i;
          e.enabled = on;
          if (const EffortSpec *s = effort_spec(nm)) {
            e.permille = s->permille;
            e.meter = s->meter;
            e.dosed = s->dosed;
            e.retry_advice = s->retry_advice;
          }
          inprocess_slot.push_back(e);
        }
      // BVE-LAST ordering (the bimodal-BVE fix, 2026-07-21). kissat eliminates on
      // a formula ALREADY simplified by its probing/vivify/sweep/factor pipeline
      // (clique occ 36→5), making elimination benign; a name-sorted slot runs BVE
      // FIRST on the RAW formula, where partial elimination of symmetric structure
      // makes an intrinsically-hard formula (clique 222s). Running BVE LAST — after
      // Factor/Congruence/Substitute simplify — matches kissat: clique 222→4.2s
      // (53×, beats kissat), Tier B PAR-2 42.6→21.0, Tier C 28→29/30 (gains
      // aes_decry), Tier A neutral, fuzz sound. STOPGAP for the effort-budgeted
      // scheduler (plan rev 53) which supersedes it. `TACTIX_BVE_FIRST` reverts to
      // the old order for that A/B.
      if (!std::getenv("TACTIX_BVE_FIRST")) {
        const std::vector<RuleVTable> &rr = tactix_rules();
        for (size_t j = 0; j < inprocess_slot.size(); j++)
          if (!std::strcmp(rr[inprocess_slot[j].rule].name, "BVE")) {
            const SlotEntry e = inprocess_slot[j];
            inprocess_slot.erase(inprocess_slot.begin() + j);
            inprocess_slot.push_back(e);
            break;
          }
      }
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
      if (k.h->stable)
        tick_reluctant(); // per learned clause (kissat learn.c)
      {
        // chrono (kissat learn.c): long backjumps waste the prefix —
        // step back one level instead when the gap exceeds the limit
        const uint32_t jump = k.pending.jump_level;
        const uint32_t back = k.level - 1;
        const bool chrono = back - jump > chrono_levels;
        if (chrono)
          k.stats.chronological++;
        backjump_apply_to(k, chrono ? back : jump);
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
      if (k.h->stable) // entering stable: fresh reluctant sequence
        reluctant = {};
      update_mode_limits(k);
      continue;
    }
    if (want_restart(k) && restart_guard(k)) {
      restart_apply(k);
      reluctant.trigger = false; // consumed (kissat reluctant.h)
      if (!k.h->stable) // kissat restart.c: focused-only limit update
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
      // so search re-descends toward the same region afterward), then give each
      // enabled technique ONE DOSE, sized as a per-mille fraction of the search
      // work that preceded it (kissat SET_EFFORT_LIMIT; the ledger lives in
      // SlotEntry). Bounding matters — an unbounded fixpoint drowns even
      // trivial instances (square 2 s → TO) — but bounding by APPLICATIONS, as
      // this loop used to, bounds the wrong thing: it is blind to formula size
      // (300 eliminations on a 1.45 M-variable formula) and its real job was
      // capping an O(applications × formula) loop rather than capping
      // simplification.
      const uint64_t phase_ticks_at_entry = k.stats.ticks;
      k.stats.inprocess_phases++;
      if (k.level > 0 && restart_guard(k))
        restart_apply_to(k, 0);
      // Attach the shared occurrence index for the phase (increment 2): the
      // occurrence consumers (BVE) read it instead of rebuilding a ~O(arena)
      // index per apply. Detached on EVERY exit below so k.dense never dangles.
      kernel_dense_connect(k, inprocess_occ);
      // Factor grid-selection arm for this phase (S2a): pure config, like
      // k.bve_bound — Factor is a batch entry reached via r.apply(k) below and
      // reads it only there, so once per phase suffices. Scoped set/clear
      // (mirrors the dense index) so the kernel never holds a dangling
      // pointer into this policy after solve returns.
      k.factor_match = factor_match ? factor_match : &default_factor_match_;
      // Phase structure (kissat eliminate-round shape): a few OUTER rounds,
      // each giving every enabled technique one dose, so a technique can pick
      // up what the others exposed. Dose granularity follows the rule's
      // instance surface, not its name:
      //   DOSED rules (BVE, and the probing rules as they are enabled) take
      //   variable/literal payloads that survive formula mutation, so the
      //   policy enumerates candidates ONCE and applies them one at a time
      //   until the budget is spent. This also stops re-running BVE's
      //   O(formula) schedule per elimination, which the old canonical-apply
      //   loop did (bve_apply = schedule + eliminate one variable).
      //   BATCH rules (Strengthen/SubsumeForward/Vivify) take arena Refs, which
      //   the garbage collection inside their first successful application
      //   invalidates; they run as one canonical round and are metered after
      //   the fact (kissat meters `substitute` the same way). Running them once
      //   per BVE *variable* instead cost 299 full subsumption sweeps on
      //   `square` — 30 M pair-checks, 0 subsumed.
      //   UNMETERED rules (Congruence/Factor/Substitute) expose only the
      //   canonical instance: one pass per outer round, nothing to titrate.
      bool phase_unsat = false;
      bool phase_dosed = false; // a dosed rule ran: the phase owes a reclaim
      const std::vector<RuleVTable> &rules = tactix_rules();
      // kissat kimits.h SET_EFFORT_LIMIT, once per phase (eliminate.c:428
      // computes the resolution limit BEFORE the round loop, so all of a
      // technique's rounds share one budget): the dose is `permille` of the
      // search work done since this entry last ran, floored so a technique that
      // has had no search to amortize still gets a usable first dose.
      for (SlotEntry &e : inprocess_slot) {
        if (!e.enabled || !e.meter)
          continue;
        const uint64_t now = search_ticks(k);
        uint64_t reference = now - e.anchor;
        if (reference < MIN_EFFORT_TICKS)
          reference = MIN_EFFORT_TICKS;
        e.limit = e.meter(k) + reference / 1000 * e.permille;
        e.anchor = now;
      }
      for (int outer = 0; outer < INPROCESS_PHASE_ROUNDS && !phase_unsat;
           outer++) {
        bool round_changed = false;
        for (SlotEntry &e : inprocess_slot) {
          if (!e.enabled || phase_unsat)
            continue;
          const RuleVTable &r = rules[e.rule];
          const uint64_t limit = e.limit;
          if (e.dosed && r.instances && r.apply_instance) {
            if (!r.guard(k))
              continue;
            // BVE growth-slack RAMP (Track S · S1b, BveBoundPolicy): drive
            // k.bve_bound cheap-first over the sub-policy's passes (kissat
            // eliminate.c set_next_elimination_bound), so the non-growing
            // eliminations happen first and later ones are measured against an
            // already-simplified formula — the adaptive version of a single fixed
            // bound, dodging the crypto1 BVE U-shape (L4). A null ramp (or a
            // non-BVE dosed entry) = one pass at the current bound (the pre-S1b
            // behaviour, bit-exact). The dose is SHARED across passes.
            BveBoundPolicy *ramp = nullptr;
            if (e.retry_advice && !std::getenv("TACTIX_NO_BVE_RAMP"))
              ramp = bve_bound ? bve_bound : &default_bve_ramp_;
            const uint32_t passes = ramp ? ramp->num_passes() : 1;
            // During a ramp the policy consults BOTH the retry bit AND the
            // per-variable needed-bound cache the rule fills (Kernel::bve_min_bound,
            // enabled by bve_track_min_bound below): a variable rejected for growth
            // at a low slack is skipped at every slack BELOW the one it actually
            // needs, and re-attempted only when its neighbourhood changes (retry bit
            // set). This is what stops the ramp re-resolving every rejected variable
            // at all six levels — the S1b cost (crypto1 ~1.0M resolutions, hitting
            // the dose cap). A bare retry bit could NOT do this: it is cleared on any
            // attempt (incl. a growth-abort), so it would skip a low-slack reject at
            // every HIGHER slack too and collapse the ramp to level 0 — the exact
            // reason S1b turned it off; the needed-bound cache is the fix. Without a
            // ramp the retry bit alone is consulted, as before (cross-round dedup).
            const bool use_retry = e.retry_advice && !ramp;
            k.bve_track_min_bound = ramp != nullptr;
            for (uint32_t pass = 0; pass < passes && !phase_unsat; pass++) {
              // Skip remaining ramp levels once the dose is spent — but ONLY when
              // ramping. Without a ramp (passes==1) this must NOT short-circuit:
              // HEAD always ran kernel_bve_retry_sync_fixed (persistent state) +
              // r.instances before the inner-loop meter check, so a top-of-pass
              // break here would skip that sync when the dose is already spent (a
              // multi-phase-only divergence). Gating on `ramp` keeps the disabled
              // path byte-for-byte HEAD (S1b review).
              if (ramp && e.meter(k) > limit)
                break;
              if (ramp)
                k.bve_bound = ramp->bound(pass);
              // Fold newly root-fixed literals into the rule's retry advice before
              // reading it: fixing y changes what BVE computes for y's neighbours
              // with no clause add/delete to mark them. Needed by BOTH the retry
              // and the ramp needed-bound paths (a fixing invalidates a neighbour's
              // cached bound), so sync whenever the rule maintains the advice.
              if (e.retry_advice)
                kernel_bve_retry_sync_fixed(k);
              inprocess_payloads_.resize(2 * static_cast<size_t>(k.nvars) + 1);
              const uint32_t n = r.instances(
                  k, inprocess_payloads_.data(),
                  static_cast<uint32_t>(inprocess_payloads_.size()));
              for (uint32_t i = 0; i < n; i++) {
                if (e.meter(k) > limit)
                  break; // dose spent
                if (!r.guard(k))
                  break;
                // SCHEDULING (policy-side, kissat flags->eliminate): decline a
                // variable whose outcome this pass is already known. Unchanged
                // neighbourhood (retry bit clear) ⇒ the same attempt recomputes the
                // same NoOp; DURING a ramp, additionally, a clean variable known to
                // need MORE slack than this pass offers (bve_min_bound) cannot be
                // eliminated here either. The rule still OFFERS and ACCEPTS it — the
                // policy declines, not the rule (N1).
                if (use_retry || ramp) {
                  Lit rl;
                  if (lit_of_instance(inprocess_payloads_[i], &rl) &&
                      var_of(rl) < k.nvars && !k.bve_retry[var_of(rl)] &&
                      (!ramp || k.bve_min_bound[var_of(rl)] > k.bve_bound))
                    continue;
                }
                const ApplyResult ar =
                    r.apply_instance(k, inprocess_payloads_[i]);
                if (ar == ApplyResult::Unsat || k.inconsistent) {
                  phase_unsat = true;
                  break;
                }
                if (ar != ApplyResult::Ok)
                  continue; // this candidate was not viable; try the next
                round_changed = true;
                phase_dosed = true;
                // drain any root units the application asserted
                if (propagate_apply(k) == ApplyResult::Unsat) {
                  phase_unsat = true;
                  break;
                }
              }
            }
            k.bve_track_min_bound = false; // scope the caching to this ramp
            continue;
          }
          // batch / unmetered: one canonical round, metered post hoc
          if (e.meter && e.meter(k) > limit)
            continue;
          if (!r.guard(k))
            continue;
          const ApplyResult ar = r.apply(k);
          if (ar == ApplyResult::Unsat || k.inconsistent) {
            phase_unsat = true;
            break;
          }
          if (ar != ApplyResult::Ok)
            continue;
          round_changed = true;
          if (propagate_apply(k) == ApplyResult::Unsat) {
            phase_unsat = true;
            break;
          }
        }
        if (!round_changed)
          break; // outer fixpoint: nothing changed this round
      }
      // Reclaim what the dosed applications garbaged. The canonical rounds
      // reclaim themselves; the per-instance path deliberately does not, so
      // that a dose costs its own work and not an O(arena) collection per
      // eliminated variable. Not collecting is always sound (deletes are lazy
      // and every reader skips garbage), so this is a cost choice, not a
      // correctness one — see docs/design/effort-scheduler-design.md §3.4.
      if (phase_dosed && !phase_unsat)
        kernel_collect_garbage(k);
      kernel_dense_disconnect(k);
      k.factor_match = nullptr; // scope the arm to the phase (see entry)
      inprocess.ticks += k.stats.ticks - phase_ticks_at_entry;
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
