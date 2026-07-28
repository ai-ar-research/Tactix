#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include "policies/ops/rephase/rephase.hpp"
#include "policies/ops/walk/walk.hpp"
#include "rules/restart/restart.hpp"
#include "rules/registry.hpp"
#include "policies/ops/switchmode/switchmode.hpp"

#include <cmath>
#include <cstdio>
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
    avg.decision_rate.init(EMA_SLOW_WINDOW);
    avg.initialized = true;
  }
  avg.fast_glue.update(glue);
  avg.slow_glue.update(glue);
}

// decisions since the previous conflict IN THIS MODE (kissat analyze.c
// update_decision_rate_average, at the top of every non-probing analysis).
// The per-mode `saved_decisions` baseline is re-anchored at every mode switch
// (kissat mode.c) — without that, the first sample after a switch is the
// other mode's entire decision count and the EMA stays pinned high for
// ~emaslow conflicts, silently disabling the gate below.
void DefaultPolicy::observe_decision_rate(const Kernel &k) {
  ModeAverages &avg = averages[k.h->stable];
  if (!avg.initialized) {
    avg.fast_glue.init(EMA_FAST_WINDOW);
    avg.slow_glue.init(EMA_SLOW_WINDOW);
    avg.decision_rate.init(EMA_SLOW_WINDOW);
    avg.initialized = true;
  }
  const uint64_t cur = k.stats.decisions;
  const uint64_t prev = avg.saved_decisions;
  avg.saved_decisions = cur;
  avg.decision_rate.update(static_cast<double>(cur - prev));
}

// kissat's gate ladder for reason-side bumping, in its exact order: option,
// then the DESTRUCTIVE delay countdown, then the decision-rate test.
uint32_t DefaultPolicy::want_bump_reasons(const Kernel &k) {
  if (!bump_reasons_limit_)
    return 0;
  if (bump_reasons_delay_.count) {
    bump_reasons_delay_.count--;
    return 0;
  }
  if (averages[k.h->stable].decision_rate.value >= bump_reasons_rate_)
    return 0;
  return bump_reasons_limit_;
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
  // TACTIX_REPHASE_BOTH_MODES (DIAGNOSTIC, cadical-shaped arm): cadical
  // rephases -- and therefore walks -- in focused mode too (rephase.cpp,
  // opts.rephase=1; the stable gate applies only at opts.rephase==2), and is
  // 6.6x faster than kissat on the walk-decisive SCPC-1000-18. kissat and we
  // rephase stable-only.
  static const bool both = std::getenv("TACTIX_REPHASE_BOTH_MODES") != nullptr;
  return (both || k.h->stable) && k.stats.conflicts > rephase.next_conflicts;
}

// kissat kimits.h UPDATE_CONFLICT_LIMIT(rephase, rephased, NLOG3N):
// interval grows as rephaseint · n·log10(n+9)³ of the rephase count
// Counts rephase EVENTS, not ops (rephase.count): a B+W slot must not grow
// the interval twice. Identical to stats.rephased before Walk joined.
void DefaultPolicy::update_rephase_limit(const Kernel &k) {
  const double n = static_cast<double>(rephase.count);
  const double lg = std::log10(n + 9);
  rephase.next_conflicts =
      k.stats.conflicts +
      static_cast<uint64_t>(REPHASE_INTERVAL * n * lg * lg * lg);
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
// V4 meters CANDIDATES, and that under-prices its work on purpose — a
// "checked" is a whole BFS walk, and stats.transitive_ticks (charged by the
// rule) shows it running to 28 % of 9vliw's cycles (probe 1562920). Both
// honest-currency arms were measured and BANKED NEGATIVE 2026-07-25
// (same-window Tier B, windows `20260725-2022*`/`20260725-2028*`):
// ticks-meter @ kissat's 20‰ = 12.22 vs 10.67 (rbsat +17 s, mp1 out of
// envelope), + kissat's early exit = 13.92 vs 10.60 (FmlaEquiv conflicts
// 2×, 9vliw +43 %). The unbudgeted exhaustive reduction is load-bearing
// for trajectory on FmlaEquiv/9vliw-class instances; its 9vliw cost is a
// PAID, PRICED cost until a context-aware policy (phase-6 dosing exhibit)
// can dose it per instance.
uint64_t meter_transitive(const Kernel &k) { return k.stats.transitive_checked; }
uint64_t meter_factor(const Kernel &k) { return k.stats.factor_ticks; }
uint64_t meter_ternary(const Kernel &k) { return k.stats.ternary_ticks; }
uint64_t meter_bce(const Kernel &k) { return k.stats.bce_ticks; }

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
// `budget_aware` marks a BATCH rule whose canonical apply reads
// Kernel::work_budget (in its own currency) and stops mid-round — the policy
// hands such an entry its remaining ledger budget around r.apply(k) instead
// of only metering it after the fact (S4 dosing; kissat SET_EFFORT_LIMIT).
struct EffortSpec {
  const char *name;
  uint32_t permille;
  uint64_t (*meter)(const Kernel &);
  bool dosed;
  bool retry_advice;
  bool budget_aware;
};
constexpr EffortSpec EFFORT_SPECS[] = {
    {"BVE", 100, meter_bve, true, true, false},
    {"Probe", 20, meter_ticks, true, false, false},
    {"Backbone", 20, meter_ticks, true, false, false},
    {"TransitiveReduce", 20, meter_transitive, true, false, false},
    {"Vivify", 100, meter_ticks, false, false, true},
    {"Strengthen", 100, meter_strengthen, false, false, false},
    {"SubsumeForward", 100, meter_forward, false, false, false},
    // S4 Factor dosing (design §3.b): kissat factoreffort 50‰ (options.h:57
    // — half of vivify/eliminate's 100), currency = factor_ticks. The first
    // dose per solve bypasses the permille formula with kissat's
    // factoriniticks 700M allowance (set at phase entry below).
    {"Factor", 50, meter_factor, false, false, true},
    // Track R (rev 72). Both take LITERAL payloads, so both are `dosed` — the
    // policy enumerates once and applies candidates up to the budget. The
    // first A/B (2026-07-27) ran them UNDOSED, which is not the composition
    // either reference names: cadical runs `ternary` under a steps budget
    // inside its preprocessing rounds. 20‰ is the cheap-probing-family rate
    // (Probe/Backbone/TransitiveReduce) — a starting point to sweep, not a
    // ported constant, since neither reference exposes a comparable fraction.
    // BATCH + budget_aware, NOT per-instance `dosed`: both build a round-local
    // occurrence index, so dosing them one instance at a time would rebuild
    // that index per application. They read Kernel::work_budget and stop
    // mid-round instead (Vivify's arrangement).
    {"HyperTernaryRes", 20, meter_ternary, false, false, true},
    {"BCE", 20, meter_bce, false, false, true},
    // X4 Instantiate (R3): its probes ARE propagation, so the currency is
    // ticks (C3 Vivify's) and the rate is the cheap-probing-family 20 —
    // Probe/Backbone/TransitiveReduce. BATCH + budget_aware for the round-2
    // reason: the round builds one occurrence-count pass up front.
    {"Instantiate", 20, meter_ticks, false, false, true},
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
  // The during-search phases CARRY the elimination value (BVE is deferred
  // out of preprocessing), so they fire on the interval alone. (The legacy
  // composition's new-material gate left with the legacy path, 2026-07-25
  // negative-arm cleanup.)
  return true;
}

// kissat kimits.h UPDATE_CONFLICT_LIMIT(eliminate, eliminations, NLOG2N,
// true): interval grows as eliminateint · n·log10(n+9)², size-scaled
void DefaultPolicy::update_inprocess_limit(const Kernel &k) {
  // The during-search phases CARRY the elimination value (BVE is deferred
  // out of conflicts==0), so the growing interval is load-bearing: kissat
  // UPDATE_CONFLICT_LIMIT(eliminate, NLOG2N), size-scaled. A fixed interval
  // is the wrong control (L7, `775eefc`).
  {
    uint64_t n = inprocess.rounds; // policy-local growth key (rev 17)
    if (!n)
      n = 1;
    const double lg = std::log10(static_cast<double>(n) + 9);
    const uint64_t delta = static_cast<uint64_t>(
        SUBSUME_INTERVAL * static_cast<double>(n) * lg * lg);
    inprocess.next_conflicts =
        k.stats.conflicts + scale_inprocessing_delta(k, delta);
    return;
  }
}

// reduce cadence. Default: arithmetically growing interval (phase-1 choice).
// TACTIX_REDUCE_SQRT selects kissat's schedule instead — reduceint·√reductions
// (kimits.h UPDATE_CONFLICT_LIMIT(reduce, reductions, SQRT, false),
// reduceint = 1000). The two diverge fast: at 100 reductions kissat's next
// interval is 10 k conflicts and ours is 32 k, so our steady-state redundant
// database is several times larger and its `used` bits age several times
// slower. That is a candidate mechanism for the standing residual (s055
// props/conflict 1.43× kissat AT conflict parity).
void DefaultPolicy::update_reduce_limit(const Kernel &k) {
  if (reduce_sqrt_) {
    const double n = static_cast<double>(
        k.stats.reductions ? k.stats.reductions : 1);
    reduce.next_conflicts =
        k.stats.conflicts +
        static_cast<uint64_t>(REDUCE_INTERVAL_KISSAT * std::sqrt(n));
    return;
  }
  reduce.next_conflicts = k.stats.conflicts + REDUCE_INTERVAL_INIT +
                          REDUCE_INTERVAL_STEP * k.stats.reductions;
}

// how much of the ranked candidate list dies. Default: kissat's flat 75 %.
// Under TACTIX_REDUCE_SQRT, kissat's RAMP (reduce.c
// mark_less_useful_clauses_as_garbage): percent = high − (high−low)/log10(r+9)
// with reducelow/reducehigh = 50/90 per mille-of-ten, i.e. 50 % at the first
// reduction rising towards 90 %. The ramp belongs with the cadence — a
// frequent reducer must delete less each time.
size_t GlueTieredReduction::victims(const Kernel &k, size_t candidates) {
  if (!ramp)
    return reduce_canonical_victims(candidates);
  // kissat increments `reductions` at the top of its reduce; ours is still
  // the pre-increment count here, hence +1.
  const double r = static_cast<double>(k.stats.reductions) + 1.0;
  const double percent = 90.0 - 40.0 / std::log10(r + 9.0);
  return static_cast<size_t>(static_cast<double>(candidates) * percent /
                             100.0);
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
  // Per-SOLVE scheduling state (S4 dosing): a reused policy object must not
  // carry the previous solve's first-dose consumption or factordelay decades
  // into a new formula (adversarial-review note 2026-07-25).
  bve_rounds_run_ = 0;
  bve_level_ = 0;
  for (SlotEntry &e : inprocess_slot)
    e.doses = 0;
  // TB6 A/B knob: TACTIX_FORCE_MODE=focused|stable (0 = off; see the
  // SwitchMode decline below). Read once — the check sits on the CDCL
  // outer loop.
  force_mode_ = 0;
  if (const char *fm = std::getenv("TACTIX_FORCE_MODE")) {
    if (!std::strcmp(fm, "focused"))
      force_mode_ = 1;
    else if (!std::strcmp(fm, "stable"))
      force_mode_ = 2;
  }
  // Closure delay ladder — DEFAULT ON (2026-07-25 arrays: B 10.56 vs
  // 10.67, C 395.56 vs 398.94, equal solves, fuzz 10k/0 ×2 formats):
  // kissat-style delay ladder on UNPRODUCTIVE unmetered closures
  // (Congruence/Substitute) — probe 1562606: FmlaEquiv re-ran the full
  // congruence closure 24 times for zero yield (~14 % of cycles in gate
  // extraction) because ordinary learned binaries re-arm the new-material
  // marks every outer round. Yield signal = the registry contract itself
  // (Ok vs NoOp); a delay, never a disable (no-rule-off baseline).
  // TACTIX_NO_CLOSURE_DELAY reverts for A/Bs.
  closure_delay_ = std::getenv("TACTIX_NO_CLOSURE_DELAY") == nullptr;
  // Reason-side bumping — DEFAULT ON since 2026-07-27, TOGETHER with dynamic
  // tiers below: measured same-window Tier C (window `20260727-005552-C-pairs`)
  // 322.84 shipped -> **273.21** for the pair, against cadical 308.10, while
  // EITHER ALONE LOSES (bumping alone 391.90, tiers alone 424.26 in window
  // `20260727-003627`). That is not a coincidence to shrug at: kissat's
  // retention bands are quantiles of the glue distribution of clauses the
  // search actually uses, and reason-side bumping changes which clauses those
  // are — the two references ship them together because they are one
  // mechanism. TACTIX_NO_BUMP_REASONS / TACTIX_NO_DYNAMIC_TIERS revert
  // independently (both were measured). The two sweep knobs exist because the
  // references disagree 10x on the rate. Per-solve reset of the delay ladder,
  // like every other per-solve scheduling counter above.
  bump_reasons_limit_ =
      std::getenv("TACTIX_NO_BUMP_REASONS") ? 0 : BUMP_REASONS_LIMIT;
  if (const char *bl = std::getenv("TACTIX_BUMP_REASONS_LIMIT"))
    bump_reasons_limit_ = static_cast<uint32_t>(std::strtoul(bl, nullptr, 10));
  bump_reasons_rate_ = BUMP_REASONS_RATE;
  if (const char *br = std::getenv("TACTIX_BUMP_REASONS_RATE"))
    bump_reasons_rate_ = std::strtod(br, nullptr);
  bump_reasons_delay_ = {};
  // Reduce-cadence arm: kissat's √ interval + ramped fraction (reduceinit
  // 1000 is its first limit, against our 2000).
  reduce_sqrt_ = std::getenv("TACTIX_REDUCE_SQRT") != nullptr;
  default_reduction_.ramp = reduce_sqrt_;
  if (reduce_sqrt_)
    reduce.next_conflicts = k.stats.conflicts + REDUCE_INTERVAL_KISSAT;
  dynamic_tiers_ = std::getenv("TACTIX_NO_DYNAMIC_TIERS") == nullptr;
  retier_ = {};
  if (dynamic_tiers_)
    retier_.next_conflicts = k.stats.conflicts + retier_.interval;
  // the first inprocessing limit needs the ingested formula's size
  // (kissat inits limits after parsing; INIT_CONFLICT_LIMIT(eliminate,
  // true) is likewise size-scaled)
  {
    // the slot: registry-driven (RuleTier::Inprocess), registry order;
    // composition per default_policy.hpp. A preposed (nonempty) slot is
    // left untouched — to run nothing, prepose disabled entries, not an
    // empty vector
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
      // TACTIX_SLOT_ENABLE / TACTIX_SLOT_DISABLE: comma-separated rule names
      // added to / removed from the default composition (Track S composition
      // A/Bs — S2 enable-flips, S4 arrangement arms). Disabling a rule is
      // always sound; enabling one poses the same composition the phase-6
      // arm surface will pose via the API. Unset ⇒ byte-identical default.
      // Track S · S4: the target arrangement as ONE unit (effort-scheduler
      // design §4.4; kissat's composition): the probing family joins the
      // slot, and BVE leaves the conflicts==0 preprocessing phase (kissat
      // fastel=0) — its doses come from the during-search phases, which then
      // run on a formula probing has simplified.
      // **UNCONDITIONAL since 2026-07-25** — the Tier B+C one-unit gate
      // PASSED (report benchmarks/reports/2026-07-25-s4-gate-mp1.md: B
      // 17.45→10.62 vs legacy 43.86, mp1 5.5 s vs kissat 9.1) and the
      // legacy composition + its TACTIX_NO_ARRANGEMENT/KEEP_PREBVE reverts
      // were removed in the same day's negative-arm cleanup (user
      // directive); the pre-gate composition lives in git history.
      const auto env_list = [](const char *var, const char *nm) {
        const char *s = std::getenv(var);
        if (!s)
          return false;
        const size_t n = std::strlen(nm);
        for (const char *p = s; (p = std::strstr(p, nm)); p += n)
          if ((p == s || p[-1] == ',') && (p[n] == '\0' || p[n] == ','))
            return true;
        return false;
      };
      for (int i = 0; i < static_cast<int>(rules.size()); i++)
        if (rules[i].tier == RuleTier::Inprocess) {
          const char *nm = rules[i].name;
          // The S4 arrangement enables every Inprocess-tier rule; the two
          // named ablation knobs + the slot-surgery lists are the A/B
          // surface (same overrides presets/the phase-6 learner use).
          bool on = !(bve_off && !std::strcmp(nm, "BVE")) &&
                    !(factor_off && !std::strcmp(nm, "Factor")) &&
                    // Track R lands each new reference family N1-exposed and
                    // registry-visible FIRST, scheduled only once its own
                    // same-window A/B says it pays (`TACTIX_SLOT_ENABLE`
                    // is the measurement arm). Without this a newly
                    // registered Inprocess rule would silently join the S4
                    // arrangement and move every number in the same commit
                    // that introduces it.
                    std::strcmp(nm, "HyperTernaryRes") != 0 &&
                    std::strcmp(nm, "BCE") != 0 &&
                    std::strcmp(nm, "Instantiate") != 0;
          if (env_list("TACTIX_SLOT_ENABLE", nm))
            on = true;
          if (env_list("TACTIX_SLOT_DISABLE", nm))
            on = false;
          SlotEntry e;
          e.defer_preprocess = !std::strcmp(nm, "BVE");
          e.rule = i;
          e.enabled = on;
          if (const EffortSpec *s = effort_spec(nm)) {
            e.permille = s->permille;
            e.meter = s->meter;
            e.dosed = s->dosed;
            e.retry_advice = s->retry_advice;
            e.budget_aware = s->budget_aware;
          }
          inprocess_slot.push_back(e);
        }
      // Phase order (what works and why): name-sorted slot, then BVE moved
      // last (kissat eliminates on a formula its pipeline already
      // simplified — clique 222→4.2 s when BVE stopped running FIRST on raw
      // structure), then Factor last of all (kissat probe.c:41 — each phase
      // is probing-family → simplifiers → BVE → Factor, so the NEXT phase's
      // BVE sees the factored formula; the eliminate/factor alternation in
      // one-slot form, design §3.c).
      const auto move_to_back = [this](const char *name) {
        const std::vector<RuleVTable> &rr = tactix_rules();
        for (size_t j = 0; j < inprocess_slot.size(); j++)
          if (!std::strcmp(rr[inprocess_slot[j].rule].name, name)) {
            const SlotEntry e = inprocess_slot[j];
            inprocess_slot.erase(inprocess_slot.begin() + j);
            inprocess_slot.push_back(e);
            break;
          }
      };
      move_to_back("BVE");
      move_to_back("Factor");
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
      observe_decision_rate(k); // kissat: before the analysis, once per conflict
      // reason-side bumping is CONFIGURATION on the rule, scoped to this one
      // application so it never leaks to the API or RandomPolicy
      k.bump_reason_limit = want_bump_reasons(k);
      const uint64_t br_rounds = k.stats.reason_bump_rounds;
      const uint64_t br_aborted = k.stats.reason_bump_aborted;
      const ApplyResult ar = analyze_apply(k);
      k.bump_reason_limit = 0;
      if (k.stats.reason_bump_rounds != br_rounds) { // the pass actually ran
        if (k.stats.reason_bump_aborted != br_aborted) // over the mark limit
          bump_reasons_delay_.current +=
              (bump_reasons_delay_.current < UINT32_MAX);
        else if (bump_reasons_delay_.current)
          bump_reasons_delay_.current /= 2;
        bump_reasons_delay_.count = bump_reasons_delay_.current;
      }
      if (ar == ApplyResult::Unsat)
        return 20;
      observe_glue(k, k.pending.glue); // policy observes; Analyze is pure
      // second retier channel (kissat analyze.c update_tier_limits): a
      // doubling conflict interval starting at 2, capped at 2^16, so the
      // bands settle quickly at the start of a solve and then stop churning
      if (dynamic_tiers_ && k.stats.conflicts > retier_.next_conflicts) {
        reduce_retier(k);
        if (retier_.interval < (1u << 16))
          retier_.interval *= 2;
        retier_.next_conflicts = k.stats.conflicts + retier_.interval;
      }
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
      // kissat recomputes the tier limits at the TOP of every reduce, so the
      // selection below already sees the new bands (reduce.c:159 → :179)
      if (dynamic_tiers_)
        reduce_retier(k);
      reduce_candidates(k, reduce_scratch_);
      reduce_apply_refs(
          k, reduce_scratch_.data(),
          std::min(rp.victims(k, reduce_scratch_.size()),
                   reduce_scratch_.size()));
      update_reduce_limit(k);
      continue;
    }
    // TB6 A/B knob (force_mode_ read once at solve entry): pin the search
    // mode by declining every SwitchMode application (pure policy
    // scheduling — the rule stays guard-true and N1-appliable throughout).
    // The solve starts focused, so "focused" never switches; "stable"
    // lets exactly the first switch through, then pins.
    const bool pin_mode =
        force_mode_ == 1 || (force_mode_ == 2 && k.h->stable);
    if (!pin_mode && want_switch_mode(k) && switchmode_guard(k)) {
      switchmode_apply(k);
      mode.count++;
      // re-anchor the entered mode's decision-rate baseline (kissat mode.c)
      averages[k.h->stable].saved_decisions = k.stats.decisions;
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
      // rotation B,B+W,I,B,B+W,O — kissat rotates B,W,I,B,W,O, i.e. Walk
      // REPLACES Best at two slots. Measured 2026-07-26 (probes 1563472–84):
      // replacing them costs far more than the walk gains, and a DRY-walk
      // control (walk runs, result discarded — TACTIX_WALK_DRY) isolated the
      // cause to the lost Best consumption, not to the phases the walk writes:
      // battleship 110 k conflicts (B,B,I,B,B,O) → 927 k with walk-for-best →
      // 2.12 M dry. Our `best` is load-bearing where kissat's is not, because
      // kissat saves phases at ASSIGN time (every propagation) while we save
      // at unassign — so kissat's saved phases already carry what our Best
      // slot has to restore. Walk therefore AUGMENTS Best here: consume best,
      // then let local search improve the result before it becomes target.
      static constexpr uint64_t WALK_SLOT = UINT64_MAX;
      static constexpr uint64_t ROTATION[6] = {
          REPHASE_BEST, WALK_SLOT, REPHASE_INVERTED,
          REPHASE_BEST, WALK_SLOT, REPHASE_ORIGINAL};
      const uint64_t type = ROTATION[rephase.count % 6];
      rephase.count++; // ONE rephase event, whatever it is composed of
      if (type == WALK_SLOT && walk_guard(k)) {
        rephase_apply_type(k, REPHASE_BEST); // the slot keeps its Best
        // effort budget in Walk's own currency, denominated in SEARCH work
        // the way every other budget is (kissat SET_EFFORT_LIMIT(walk),
        // walkeffort = 50 per mille, floored at mineffort). The reference is
        // the search work since the LAST WALK. kissat anchors this on its
        // last elimination instead (SET_EFFORT_LIMIT reads
        // last.ticks.eliminate); porting that faithfully was measured WORSE
        // for us — same-window 3-arm Tier B, PAR-2 9.69 vs 8.97, all of it
        // rbsat (19.91 s vs 12.06 s) — so the anchor stays on the last walk
        // (windows `20260726-0144*`).
        uint64_t reference = search_ticks(k) - walk_anchor;
        if (reference < MIN_EFFORT_TICKS)
          reference = MIN_EFFORT_TICKS;
        // TACTIX_WALK_PERMILLE (DIAGNOSTIC sweep knob): the dose magnitude.
        // Ours and kissat's budgets are scaled from CACHE-LINE search ticks but
        // SPENT in per-entry walk steps, while cadical spends in cache-line
        // ticks too (1 tick ~ 16 entries, internal.hpp cache_lines) at 80 per
        // mille -- so cadical buys ~13x more walk scanning per unit of search
        // work than we do. Our effective dose is cadical-equivalent ~6 per
        // mille, and cadical's own dose-response on SCPC-1000-18 is 452.7 s at
        // walkeffort 8 vs 24.7 s at 80. Audit 2026-07-26.
        static const char *const pm_env = std::getenv("TACTIX_WALK_PERMILLE");
        const uint64_t permille =
            pm_env ? std::strtoull(pm_env, nullptr, 10) : WALK_PERMILLE;
        k.work_budget = reference / 1000 * permille;
        walk_apply(k);
        k.work_budget = 0;
        walk_anchor = search_ticks(k);
      } else {
        rephase_apply_type(k, type == WALK_SLOT ? REPHASE_BEST : type);
      }
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
      const uint64_t phase_props_at_entry = k.stats.propagations;
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
        // S4 (design §3.b): the FIRST Factor dose per solve bypasses the
        // permille formula with kissat's factoriniticks allowance — an
        // effectively unbounded first round (700M ticks; mp1's completed
        // first round spends ~1M). kissat factor.c:1106-1117.
        if (e.doses == 0 && e.budget_aware &&
            !std::strcmp(rules[e.rule].name, "Factor"))
          e.limit = e.meter(k) + 700000000ull;
        // I2 (bve-tolerance plan, TEMPORARY experiment knob): under
        // TACTIX_BVE_ONESHOT the first during-search BVE dose mirrors
        // Factor's first-dose allowance — effectively unbounded, so the
        // dose runs its whole schedule to completion (kissat-shape:
        // eliminate once, early, to completion). The dosed branch below
        // runs the full ramp and retires the entry after this one dose.
        if (e.doses == 0 && e.retry_advice &&
            std::getenv("TACTIX_BVE_ONESHOT"))
          e.limit = e.meter(k) + 700000000ull;
        // I3 cadence arm (bve-tolerance plan, report §5 routing): under
        // TACTIX_BVE_CADENCE every admitted BVE dose gets the completion
        // allowance — the phase schedule already fires on the growing
        // NLOG2N conflict interval (kissat's eliminate cadence analog),
        // so "one COMPLETED call per phase" (the outer==0 gate below)
        // reproduces kissat's profitable shape: completion-gated calls at
        // geometric conflict spacing, ladder advancing per completion.
        // The I2 decider showed the damage is drip-paced INCOMPLETE
        // recurrence; this arm removes the drip while keeping recurrence.
        if (e.retry_advice && std::getenv("TACTIX_BVE_CADENCE"))
          e.limit = e.meter(k) + 700000000ull;
        e.anchor = now;
      }
      for (int outer = 0; outer < INPROCESS_PHASE_ROUNDS && !phase_unsat;
           outer++) {
        bool round_changed = false;
        for (SlotEntry &e : inprocess_slot) {
          // TACTIX_SLOT_TRACE: per-entry scheduling visibility (debug aid —
          // "did rule X even get considered, and what gated it?")
          if (std::getenv("TACTIX_SLOT_TRACE"))
            std::fprintf(stderr,
                         "[slot] phase=%llu outer=%d %s enabled=%d defer=%d "
                         "guard=%d dosed=%d meter=%llu limit=%llu\n",
                         (unsigned long long)k.stats.inprocess_phases, outer,
                         rules[e.rule].name, (int)e.enabled,
                         (int)e.defer_preprocess, (int)rules[e.rule].guard(k),
                         (int)e.dosed,
                         (unsigned long long)(e.meter ? e.meter(k) : 0),
                         (unsigned long long)e.limit);
          if (!e.enabled || phase_unsat)
            continue;
          if (e.defer_preprocess && k.stats.conflicts == 0)
            continue; // S4: no elimination at preprocessing (kissat fastel=0)
          const RuleVTable &r = rules[e.rule];
          const uint64_t limit = e.limit;
          if (e.dosed && r.instances && r.apply_instance) {
            if (!r.guard(k))
              continue;
            // S4 residual fix (2026-07-25 morning, probe 1561900): a
            // during-search BVE dose fires only on binary-DENSE formulas —
            // the same axis and threshold as Factor's density short-circuit.
            // Every measured dose-loser is sparse (crypto1 ≈ 3.0
            // binaries/var: 70.8 → 15.95 s; mod4block ≈ 4.1: 60 → 13.19 s —
            // both L2-class trajectory damage), while post-dedup mp1
            // (≈ 103) holds 5.77 s with doses off entirely — its win is
            // dedup + dosed Factor + composite. Dense Tier C instances
            // (mp1-Nb5T15 10.0 s) keep their doses. A SELECTIVITY heuristic
            // in the density-gate family (Factor 6, composite 50, this) —
            // one shared axis, proper home the phase-6 learned policy.
            // TACTIX_BVE_DOSE_DENSITY overrides (0 = dose everywhere).
            // (Two earlier attempts banked in the gate report §2: the
            // adaptive delay — mp1's productivity is bursty — and
            // removal-only retry marking; neither recovered the pair.)
            if (e.retry_advice && k.stats.conflicts > 0) {
              uint64_t dose_density = 6;
              if (const char *dd = std::getenv("TACTIX_BVE_DOSE_DENSITY"))
                dose_density = std::strtoull(dd, nullptr, 10);
              if (dose_density &&
                  k.binary_ids.size() <
                      dose_density * kernel_num_output_vars(k))
                continue;
            }
            // I2 one-shot (TEMPORARY, see phase-entry carve-out above): the
            // BVE entry fires exactly ONE dose per solve — full ramp,
            // unbounded budget — then is declined for the rest of the solve
            // (recurrence-vs-content decider, bve-tolerance plan I2).
            const bool bve_oneshot =
                e.retry_advice && std::getenv("TACTIX_BVE_ONESHOT");
            if (bve_oneshot && e.doses > 0)
              continue; // the one shot is consumed
            // I3 cadence arm (see the phase-entry allowance above): ONE
            // completed call per phase — outer rounds 1-2 are declined so
            // the phase cannot re-drip what the completed call finished.
            if (e.retry_advice && outer > 0 &&
                std::getenv("TACTIX_BVE_CADENCE"))
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
            // S4 arrangement: kissat's persistent completion-gated bound
            // LEVEL (see bve_level_ in the header) replaces the
            // all-levels-per-dose ramp — one pass per dose at the current
            // level; the level doubles below only when the dose completes.
            // (The TACTIX_NO_BVE_LEVEL eager-ramp kill switch — the crypto1
            // bisect instrument, level pacing = 24 s of that 57 s regression
            // — was removed in the 2026-07-25 negative-arm cleanup; the
            // density gate below superseded it.)
            const bool level_mode = e.retry_advice && ramp && !bve_oneshot;
            const uint32_t passes = level_mode ? 1
                                    : ramp     ? ramp->num_passes()
                                               : 1;
            bool dose_cut = false;
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
              if (ramp && e.meter(k) > limit) {
                dose_cut = true;
                break;
              }
              if (ramp)
                k.bve_bound = level_mode ? bve_level_ : ramp->bound(pass);
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
              if (std::getenv("TACTIX_SLOT_TRACE"))
                std::fprintf(stderr,
                             "[slot]   %s pass=%u n=%u bve_bound=%u meter=%llu "
                             "limit=%llu\n",
                             r.name, pass, n, k.bve_bound,
                             (unsigned long long)e.meter(k),
                             (unsigned long long)limit);
              for (uint32_t i = 0; i < n; i++) {
                if (e.meter(k) > limit) {
                  dose_cut = true;
                  break; // dose spent
                }
                if (!r.guard(k)) {
                  dose_cut = true; // conservative: not a completed schedule
                  break;
                }
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
            // Level advance (kissat set_next_elimination_bound): only a dose
            // that ran its whole schedule within budget earns the doubling.
            if (level_mode && !dose_cut && !phase_unsat) {
              const uint32_t ceiling = ramp->bound(ramp->num_passes() - 1);
              bve_level_ = bve_level_ ? std::min(bve_level_ * 2, ceiling) : 1;
            }
            // S4 factordelay input: one more BVE round has run. Once per
            // PHASE, not per outer round — kissat INC(eliminations) fires
            // once per kissat_eliminate CALL (eliminate.c), whose analog is
            // the phase, and per-outer counting advanced the gate 3 decades
            // per phase (adversarial review 2026-07-25).
            if (e.retry_advice && outer == 0)
              bve_rounds_run_++;
            if (bve_oneshot)
              e.doses++; // consume the one shot (skipped hereafter)
            continue;
          }
          // batch / unmetered: one canonical round, metered post hoc
          if (e.meter && e.meter(k) > limit)
            continue;
          const bool factor_entry =
              e.budget_aware && !std::strcmp(r.name, "Factor");
          // S4 factordelay (kissat factor.c:1068-1085, arrangement-scoped):
          // a size-vs-progress warm-up gate, NOT the kimits.c occasions
          // delay. kissat's exact form TRUNCATES: `size_t log_active =
          // log10(active)` then `log_active <= eliminations + 4` — so it
          // admits active < 10^(eliminations + 5) (up to 99,999 vars at the
          // very first probe; adversarial review 2026-07-25 caught the
          // faithful-looking but one-decade-stricter 10^(e+4) reading).
          // Active ≈ unassigned − BVE-eliminated (decade granularity makes
          // the approximation safe).
          if (factor_entry) {
            const uint64_t elim = k.stats.bve_eliminated;
            const uint64_t active =
                k.unassigned > elim ? k.unassigned - elim : 0;
            uint64_t threshold = 1;
            const uint32_t decades =
                bve_rounds_run_ > 14 ? 19u : bve_rounds_run_ + 5;
            for (uint32_t d = 0; d < decades; d++)
              threshold *= 10;
            if (active >= threshold)
              continue;
            // Completion-gated new-material watermark (kissat's
            // literals_factor analog for a binary-only Factor, design
            // §3.b): re-fire only on new binaries / root-fixed vars since
            // the last COMPLETED round — marks advance below only when the
            // dose ran to natural exhaustion (factor.c:1153), so an
            // incomplete (budget-cut) round re-fires and resumes.
            if (k.stats.conflicts > 0 &&
                k.binary_ids.size() <= e.binaries_mark &&
                kernel_root_fixed(k) <= e.root_fixed_mark)
              continue;
          }
          // S4 dosing (attribution probes 2026-07-24): in a during-search
          // arrangement phase, an UNMETERED closure (Congruence/Factor/
          // Substitute) re-runs only on new material since ITS last run —
          // repeating the full closures every phase was a dominant diffuse
          // Tier C cost, while dropping them entirely breaks urq45.
          if (!e.meter && k.stats.conflicts > 0 &&
              k.binary_ids.size() <= e.binaries_mark &&
              kernel_root_fixed(k) <= e.root_fixed_mark)
            continue;
          // Closure delay ladder (see solve entry): consume one delayed
          // occasion AFTER material admission, so the ladder counts real
          // scheduling occasions (kissat kimits delay semantics — same
          // mechanics as vivify's irr ladder below).
          if (closure_delay_ && !e.meter && k.stats.conflicts > 0 &&
              e.delay_count > 0) {
            e.delay_count--;
            continue;
          }
          if (!r.guard(k))
            continue;
          // Budget handoff (kernel.hpp work_budget): a budget-aware batch
          // entry gets the phase's REMAINING ledger budget — in the rule's
          // own currency — around its apply, so one canonical round cannot
          // overrun the dose (a dominant diffuse Tier C cost). Scoped to the
          // apply; 0 restores the unbounded legacy path for all other
          // callers (API, RandomPolicy). Vivify keeps its kissat scope/delay
          // tier split inside the branch; other budget-aware rules (S4:
          // Factor) take the plain handoff.
          const bool budgeted = e.meter && !e.dosed && e.budget_aware;
          const bool vivify_dose =
              budgeted && !std::strcmp(r.name, "Vivify");
          ApplyResult ar;
          if (vivify_dose) {
            // kissat vivify.c structure: the REDUNDANT (learned) tiers always
            // run within their budget share; only the IRREDUNDANT pass is
            // delay-gated (vivifyirr, kimits.c) — delaying the whole round
            // measured as a Tier B loss (mod4block TO, arr4 2026-07-24).
            // Shares are kissat's option defaults: irr 3 / tiers 3+3+1 = 7.
            const uint64_t remaining0 = limit - e.meter(k);
            const uint64_t remaining = remaining0 ? remaining0 : 1;
            const bool irr_delayed =
                k.stats.conflicts > 0 && e.delay_count > 0;
            if (irr_delayed)
              e.delay_count--;
            // redundant pass (always; full budget when irr is delayed)
            k.vivify_scope = 1;
            k.work_budget =
                irr_delayed ? remaining : std::max<uint64_t>(1, remaining * 7 / 10);
            ar = r.apply(k);
            // irredundant pass (delay-gated, ratio-tracked on ITS deltas)
            if (!irr_delayed && ar != ApplyResult::Unsat && !k.inconsistent) {
              const uint64_t spent = e.meter(k);
              const uint64_t irr_budget =
                  spent < limit ? std::max<uint64_t>(1, limit - spent) : 1;
              const uint64_t tried_before = k.stats.vivify_checked;
              const uint64_t produced_before =
                  k.stats.vivified + k.stats.vivify_deleted;
              k.vivify_scope = 2;
              k.work_budget = irr_budget;
              const ApplyResult ar2 = r.apply(k);
              if (ar2 == ApplyResult::Unsat)
                ar = ar2;
              else if (ar2 == ApplyResult::Ok && ar != ApplyResult::Ok)
                ar = ar2;
              if (k.stats.conflicts > 0) {
                // kissat vivify.c: vivified/tried < 1 % → bump, else halve;
                // tried == 0 counts unproductive (kissat_average(0,0)=0)
                const uint64_t tried = k.stats.vivify_checked - tried_before;
                const uint64_t produced = k.stats.vivified +
                                          k.stats.vivify_deleted -
                                          produced_before;
                if (tried == 0 || produced * 100 < tried) {
                  e.delay_current += (e.delay_current < UINT32_MAX);
                  e.delay_count = e.delay_current;
                } else {
                  e.delay_current /= 2;
                  e.delay_count = e.delay_current;
                }
              }
            }
            k.vivify_scope = 0;
            k.work_budget = 0;
          } else if (budgeted) {
            // plain handoff (S4: Factor) — remaining budget, no tier split
            const uint64_t remaining0 = limit - e.meter(k);
            const uint64_t handed = remaining0 ? remaining0 : 1;
            const uint64_t spent_before = e.meter(k);
            // S4 increment 3 (design §3.d/§3.e): arrangement Factor doses
            // run COMPOSITE — kissat's cross-round composition lives in
            // exactly these small doses (resize_factoring; the single-blast
            // regression that keeps the default OFF is banked, S2b §7) —
            // and couple acceptance to the completed-BVE level (kissat
            // init_factoring: bound = bounds.eliminate.additional_clauses,
            // factor.c:72): as elimination gets more permissive, factoring
            // gets pickier. Scoped set/clear so no other caller sees them.
            if (factor_entry) {
              // Composite doses are DENSITY-SCOPED (probes 1561187/1561512
              // + build-race postmortem: composite ON is what takes
              // post-dedup mp1 from 15.5 to 5.4 s — 645k → 178k conflicts —
              // and simultaneously what breaks battleship 1.3 → 197 s).
              // The two sit far apart on the same axis the Factor density
              // short-circuit already uses: binaries-per-output-var ≈ 103
              // (mp1) vs ≈ 6 (battleship). Compose only on extremely
              // binary-dense formulas (≥ 50×): clique 17.5, x9 8.2,
              // battleship 6 stay non-composite. A SELECTIVITY heuristic
              // like the density gate itself — proper home is the learned
              // policy; TACTIX_COMPOSITE_DENSITY overrides the threshold
              // (0 disables, small values ≈ force-on).
              // (The eliminate-level BOUND COUPLING — design §3.e, measured
              // dead: it rejects the small-delta top-up grids the dosed
              // regime exists to add, mp1 5.3→14.9 s, probes 1561190/91 —
              // was removed in the 2026-07-25 negative-arm cleanup along
              // with its TACTIX_FACTOR_BOUND_COUPLE knob; Kernel::factor_bound
              // stays as plain N1 config, always 0 from this policy.)
              uint64_t comp_density = 50;
              if (const char *cd = std::getenv("TACTIX_COMPOSITE_DENSITY"))
                comp_density = std::strtoull(cd, nullptr, 10);
              if (comp_density &&
                  k.binary_ids.size() >=
                      comp_density * kernel_num_output_vars(k))
                k.factor_composite = true;
            }
            k.work_budget = handed;
            ar = r.apply(k);
            k.work_budget = 0;
            k.factor_composite = false;
            // A zero-spend NoOp (Factor's density short-circuit returns
            // before the seed scan) consumes NEITHER the first-dose
            // allowance NOR the watermark — kissat's skipped call leaves
            // limits.factor.marked untouched (factor.c:1094-1101); the
            // re-entry cost is one O(1) density check per phase.
            const uint64_t spent = e.meter(k) - spent_before;
            if (spent > 0)
              e.doses++;
            // Completion-gated watermark advance (kissat factor.c:1153):
            // spend within the handed budget ⇒ the round ran to natural
            // exhaustion — advance the new-material marks. A budget-cut
            // round leaves them, so it re-fires next phase and resumes.
            if (spent > 0 && spent <= handed) {
              e.binaries_mark = k.binary_ids.size();
              e.root_fixed_mark = kernel_root_fixed(k);
            }
          } else {
            ar = r.apply(k);
          }
          if (!e.meter) {
            e.binaries_mark = k.binary_ids.size();
            e.root_fixed_mark = kernel_root_fixed(k);
            // Ladder maintenance (vivify idiom, :872-876): a NoOp closure
            // bumps the delay, a productive one halves it.
            if (closure_delay_ && k.stats.conflicts > 0) {
              if (ar == ApplyResult::Ok)
                e.delay_current /= 2;
              else
                e.delay_current += (e.delay_current < UINT32_MAX);
              e.delay_count = e.delay_current;
            }
          }
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
      k.stats.inprocess_propagations +=
          k.stats.propagations - phase_props_at_entry;
      if (phase_unsat)
        return 20;
      // The phase FIRED (whether or not it simplified anything): count it —
      // the interval-growth key for the NLOG2N schedule.
      inprocess.rounds++;
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
