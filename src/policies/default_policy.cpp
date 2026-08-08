#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include "policies/ops/rephase/rephase.hpp"
#include "policies/ops/reorder/reorder.hpp"
#include "policies/ops/walk/walk.hpp"
#include "rules/restart/restart.hpp"
#include "rules/registry.hpp"
#include "rules/sweep/sweep.hpp"
#include "policies/ops/switchmode/switchmode.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <utility>

namespace tactix {

// Restart trigger: fast/slow glue EMAs with kissat's warm-up (smooth.c) and
// per-mode persistence, the package its constants presuppose.

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

// Decisions since the previous conflict, per mode (kissat analyze.c); the
// baseline is re-anchored at every mode switch, or the EMA pins high.
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

// kissat's gate ladder for reason-side bumping: option, delay, rate test.
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

// Reluctant doubling (Knuth's Luby; kissat reluctant.c), per learned clause.
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

// kissat restart.c kissat_restarting: the conflicts limit gates both modes.
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

// kissat mode.c update_mode_limit: a stable phase gets the preceding focused
// phase's ticks, keeping the wall-time share balanced.
void DefaultPolicy::update_mode_limits(const Kernel &k) {
  if (mode.count & 1) { // just entered stable
    const uint64_t focused_ticks = search_ticks(k) - mode.ticks_at_focus_entry;
    mode.ticks_limit = search_ticks(k) + focused_ticks;
  } else { // just entered focused
    // Growth key = kissat's `(switched + 1) / 2` (mode.c:90).
    const uint64_t n = mode.count / 2 ? mode.count / 2 : 1;
    const double log4 = std::pow(std::log10(static_cast<double>(n) + 9), 4);
    mode.conflicts_limit =
        k.stats.conflicts +
        static_cast<uint64_t>(MODE_INTERVAL * static_cast<double>(n) * log4);
    mode.ticks_at_focus_entry = search_ticks(k);
    // kissat mode.c: only the focused entry refreshes the restart limit, and
    // per-mode averages persist across switches.
    update_restart_limit(k);
  }
}

// kissat restart.c kissat_update_focused_restart_limit; stale while stable.
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
  // TACTIX_REPHASE_BOTH_MODES (diagnostic): cadical rephases, and so walks, in
  // focused mode too. The default follows kissat: stable-only.
  static const bool both = std::getenv("TACTIX_REPHASE_BOTH_MODES") != nullptr;
  return (both || k.h->stable) && k.stats.conflicts > rephase.next_conflicts;
}

// Reorder is a policy op, not a proof rule, so the all-rules-on baseline does
// not apply. TACTIX_REORDER mirrors kissat's `reorder` and defaults off.
bool DefaultPolicy::want_reorder(const Kernel &k) const {
  static const char *const env = std::getenv("TACTIX_REORDER");
  static const int mode = env ? std::atoi(env) : 0;
  if (mode <= 0)
    return false;
  if (!k.h->stable && mode < 2)
    return false;
  if (k.level != 0)
    return false; // kissat's own trigger: reorder only at the root
  return k.stats.conflicts >= reorder.next_conflicts;
}

// kissat kimits.h UPDATE_CONFLICT_LIMIT(reorder, reordered, LINEAR, false).
void DefaultPolicy::update_reorder_limit(const Kernel &k) {
  reorder.next_conflicts =
      k.stats.conflicts + REORDER_INTERVAL * reorder.count;
}

// kissat kimits.h UPDATE_CONFLICT_LIMIT(rephase, rephased, NLOG3N), keyed on
// events not ops — a B+W slot must not grow the interval twice.
void DefaultPolicy::update_rephase_limit(const Kernel &k) {
  const double n = static_cast<double>(rephase.count);
  const double lg = std::log10(n + 9);
  rephase.next_conflicts =
      k.stats.conflicts +
      static_cast<uint64_t>(REPHASE_INTERVAL * n * lg * lg * lg);
}

// Every piece of state a solve derives is re-based on the kernel's current
// counters; configuration is untouched. See the header for why it matters.
void DefaultPolicy::reset_schedule(const Kernel &k) {
  const uint64_t conflicts = k.stats.conflicts;
  // The tick accumulator zeroes before any anchor re-bases.
  inprocess = {};
  const uint64_t ticks = search_ticks(k);

  // per-mode glue/decision EMAs (kissat search.c:56; cadical internal.cpp:582)
  averages[0] = averages[1] = {};
  // restart (kissat kimits.c:120; cadical internal.cpp:568-570)
  update_restart_limit(k);
  // reduce (kissat kimits.c:110; cadical internal.cpp:508)
  reduce.next_conflicts =
      conflicts +
      (reduce_sqrt_ ? REDUCE_INTERVAL_KISSAT : REDUCE_INTERVAL_INIT);
  // mode alternation (kissat mode.c:19-40; cadical internal.cpp:596-601)
  mode = {};
  mode.conflicts_limit = conflicts + MODE_INITIAL_CONFLICTS;
  mode.ticks_at_focus_entry = ticks;
  // reluctant doubling (kissat reluctant.c:4-14; cadical internal.cpp:603-608)
  reluctant = {};
  // rephase: limit and rotation counters (kissat kimits.c:116; cadical
  // internal.cpp:559-562)
  rephase = {};
  rephase.next_conflicts = conflicts + REPHASE_INTERVAL_INIT;
  // reorder (kissat kimits.c:113 INIT_CONFLICT_LIMIT(reorder, false))
  reorder = {};
  reorder.next_conflicts = conflicts + REORDER_INTERVAL_INIT;
  // walk effort anchor (cadical last.walk.ticks): the window starts now
  walk_anchor = ticks;
  // Both clocks and their watermarks (kissat kimits.c:126-135).
  probe_clock = elim_clock = {};
  // The phase effort anchors start the window HERE, not at zero: kissat gets a
  // fresh solver per solve, this policy may be reused (§5.6), and a zero anchor
  // on solve #2 would hand the first phase every tick the last solve spent.
  probe_clock.anchor = elim_clock.anchor = ticks;
  // BVE's growth-slack ramp and decade counter (kissat kimits.c:128-130)
  bve_rounds_run_ = 0;
  bve_level_ = 0;
  bump_reasons_delay_ = {};
  retier_ = {};
  if (dynamic_tiers_)
    retier_.next_conflicts = conflicts + retier_.interval;
  default_branching_.reset(conflicts);
  default_candidate_order_.reset();
  // Runtime state only: configuration is what a posed slot carries.
  for (SlotEntry &e : inprocess_slot) {
    e.limit = 0;
    e.applies = 0;
    e.doses = 0;
    e.cursor = 0;
    e.tm = {};
    e.ord = {};
    // Watermarks zero rather than take the live counters, so the unmetered
    // closures get one guaranteed run.
    e.binaries_mark = 0;
    e.root_fixed_mark = 0;
    e.arena_mark = 0;
    e.delay_count = 0;
    e.delay_current = 0;
  }
  forward_pair_limit_ = 0;
}

// kissat kimits.c kissat_scale_delta: inprocessing intervals scale with formula
// size, which the slot constants assume.
// kissat's `solver->active` (internal.h:105, flags.c): variables that are
// neither root-fixed, BVE-eliminated nor substituted away — plus the extension
// variables Factor introduces, which move it UP. The round loop tests it for
// CHANGE, not for shrinkage, so an extending round counts as progress too.
static uint64_t active_variables(const Kernel &k) {
  const uint64_t retired = static_cast<uint64_t>(kernel_root_fixed(k)) +
                           k.stats.bve_eliminated + k.stats.substitute_vars;
  const uint64_t n = k.nvars;
  return n > retired ? n - retired : 0;
}

static uint64_t scale_inprocessing_delta(const Kernel &k, uint64_t delta) {
  const double f =
      std::log10(static_cast<double>(kernel_irredundant_clauses(k)) + 1) - 5;
  return static_cast<uint64_t>((4.5 * f * f + 25) *
                               static_cast<double>(delta));
}

// --- effort ledger (docs/design/effort-scheduler-design.md) -----------------
// Each rule is metered in its own currency: work is not comparable.
namespace {

// cadical `chronoreusetrail` (analyze.cpp:675-716), which kissat has no
// counterpart for on the backjump side — its trail reuse is at RESTART only
// (restart.c:86). Rather than dropping the whole suffix above `jump`, keep the
// prefix up to the best-ranked variable in it: that part of the trail is what
// the next decisions would most likely rebuild. Ranking follows the mode, as
// cadical's does — EVSIDS score while stable, VMTF stamp while focused.
// N1: the target is an ordinary ConflictAnalysis instance and stays inside
// [jump_level, level-1], so this narrows nothing.
uint32_t reuse_trail_level(const Kernel &k, uint32_t jump) {
  const uint32_t begin = k.frames[jump + 1];
  const uint32_t end = static_cast<uint32_t>(k.trail.size());
  if (begin >= end || !k.h)
    return jump;
  uint32_t best_pos = begin;
  Var best = 0;
  bool have_best = false;
  for (uint32_t i = begin; i < end; i++) {
    const Var v = var_of(k.trail[i]);
    // A retired variable can sit on this kernel's trail (kernel.cpp backtrack
    // note) but never on cadical's, and it is never decided again — ranking it would
    // keep levels the next decisions cannot rebuild. Skip it.
    if (kernel_var_retired(k, v))
      continue;
    if (!have_best) {
      best = v;
      best_pos = i;
      have_best = true;
      continue;
    }
    // Score ties break toward the SMALLER variable index (cadical
    // score_smaller, internal.hpp:1816) — with strict > the earliest trail
    // position wins instead, and on an all-unbumped suffix (every score 0.0)
    // the walk degenerates to jump+1 rather than cadical's pick. VMTF stamps
    // are unique, so focused mode needs no tie-break.
    const bool better =
        k.h->stable
            ? (k.h->scores.score[v] > k.h->scores.score[best] ||
               (k.h->scores.score[v] == k.h->scores.score[best] && v < best))
            : k.h->links[v].stamp > k.h->links[best].stamp;
    if (better) {
      best = v;
      best_pos = i;
    }
  }
  if (!have_best) // the whole suffix is retired variables: nothing to keep
    return jump;
  uint32_t res = jump;
  while (res + 1 < k.level && k.frames[res + 1] <= best_pos)
    res++;
  return res;
}


// Scope the kernel's tick sink to one rule's apply; RAII because the apply
// paths return early and a leaked sink misattributes the next rule's work.
struct TickSinkScope {
  Kernel &k;
  TickSinkScope(Kernel &kk, uint64_t Stats::*ctr) : k(kk) {
    k.tick_sink = ctr ? &(k.stats.*ctr) : nullptr;
  }
  ~TickSinkScope() { k.tick_sink = nullptr; }
  TickSinkScope(const TickSinkScope &) = delete;
  TickSinkScope &operator=(const TickSinkScope &) = delete;
};

// Bracket a simplification phase, so its ticks are charged to the phase rather
// than to `stats.search_ticks`. RAII for the same reason: an early return that
// left the flag set would freeze the search counter for the rest of the solve,
// and every effort limit is a fraction of it.
struct InprocessScope {
  Kernel &k;
  explicit InprocessScope(Kernel &kk) : k(kk) { k.inprocessing = true; }
  ~InprocessScope() { k.inprocessing = false; }
  InprocessScope(const InprocessScope &) = delete;
  InprocessScope &operator=(const InprocessScope &) = delete;
};

// Private per-rule meters: no EffortSpec meter may read the global
// `k.stats.ticks`, which lets one entry spend another's budget.
uint64_t meter_probe(const Kernel &k) { return k.stats.probe_ticks; }
uint64_t meter_backbone(const Kernel &k) { return k.stats.backbone_ticks; }
uint64_t meter_vivify(const Kernel &k) { return k.stats.vivify_ticks; }
uint64_t meter_instantiate(const Kernel &k) { return k.stats.instantiate_ticks; }
uint64_t meter_bve(const Kernel &k) { return k.stats.bve_resolutions; }
// Strengthen/SubsumeForward meter partner visits (cadical `stats.subchecks`),
// since checking a clause costs its occurrence lists' length.
// kissat's forward subsumption is ONE technique with one counter: a candidate
// is either subsumed or strengthened inside the same pass
// (`not_subsumed_but_strengthened`, forward.c:554), and every step of it is
// charged to `forward_steps` under one `forwardeffort` limit (forward.c:539).
// The split into two rules is local, so both price against the sum and share a
// single limit; billing each 100 per mille would draw twice the reference's.
uint64_t meter_forward_pair(const Kernel &k) {
  return k.stats.strengthen_visits + k.stats.forward_visits;
}
// TransitiveReduce's currency is ticks (kissat transitive.c:320): a "checked"
// is a whole breadth-first walk, so a candidate meter cannot bind.
uint64_t meter_transitive(const Kernel &k) {
  static const bool checked = [] {
    const char *e = std::getenv("TACTIX_V4_METER");
    return e && !std::strcmp(e, "checked");
  }();
  return checked ? k.stats.transitive_checked : k.stats.transitive_ticks;
}
uint64_t meter_factor(const Kernel &k) { return k.stats.factor_ticks; }
uint64_t meter_ternary(const Kernel &k) { return k.stats.ternary_ticks; }
uint64_t meter_hbr(const Kernel &k) { return k.stats.hbr_ticks; }
uint64_t meter_bce(const Kernel &k) { return k.stats.bce_ticks; }
uint64_t meter_cce(const Kernel &k) { return k.stats.cce_ticks; }
uint64_t meter_sweep(const Kernel &k) { return k.stats.sweep_ticks; }
uint64_t meter_congruence(const Kernel &k) { return k.stats.congruence_ticks; }

// name -> {effort per mille, currency, dosed, budget_aware}. Fractions are
// kissat's options.h defaults unless a row says otherwise; a Ref-payload rule,
// or one whose round builds a shared index, cannot be `dosed`.
//
// The two shape bits are not a dosing taxonomy — they are what a scheduler
// generic over the rule interface has to be told, where kissat hand-writes each
// technique's loop and so never states it. `dosed` drives the rule candidate by
// candidate against a budget; `budget_aware` calls its canonical round and lets
// the round poll `Kernel::work_budget` at a candidate boundary, which is what
// both references do inside their techniques.
struct EffortSpec {
  const char *name;
  uint32_t permille;
  uint64_t (*meter)(const Kernel &);
  bool dosed;
  bool budget_aware;
  // Null where the rule charges its own counter, or the currency is not ticks.
  uint64_t Stats::*tick_counter = nullptr;
  // Whole-formula closure, orthogonal to metering. See SlotEntry::closure.
  bool closure = false;
};
// Invariant: a rule registered at RuleTier::Inprocess is in the slot, so one
// that can be metered needs a row here. There is no upper clamp: kissat ships
// no `maxeffort` option at all, and the floor is on the REFERENCE
// (MIN_EFFORT_TICKS = mineffort x 1e6), which is the only one it has.

// kissat's delay ladder is driven by a yield rate rather than by whether the
// round did anything: congruence.c:4629 bumps when merged/active < 0.001,
// sweep.c:1719 when eliminated/swept < 0.001, vivify.c:1454 when
// vivified/tried < 0.01. A round that merges one variable out of a hundred
// thousand is unproductive at that scale and must still back off, which a
// no-op test cannot see.
struct DelaySpec {
  const char *name;
  uint64_t (*yield)(const Kernel &);
  uint64_t (*denom)(const Kernel &);
  uint32_t permille; // bump the delay when yield * 1000 < permille * denom
  // Whether the denominator is this application's delta or its running value:
  // kissat's congruence divides by the CURRENT active count (congruence.c:4629)
  // while sweep divides by the pairs THIS call swept (sweep.c:1719).
  bool denom_delta;
};
constexpr DelaySpec DELAY_SPECS[] = {
    // congruence.c:4629, denominator `solver->active`.
    {"Congruence", [](const Kernel &k) { return k.stats.congruence_merges; },
     active_variables, 1, false},
    // sweep.c:1719, denominator the candidate pairs actually swept.
    {"Sweep", [](const Kernel &k) { return k.stats.sweep_equivalences; },
     [](const Kernel &k) { return k.stats.sweep_candidates; }, 1, true},
};
static const DelaySpec *delay_spec(const char *name) {
  for (const DelaySpec &d : DELAY_SPECS)
    if (!std::strcmp(d.name, name))
      return &d;
  return nullptr;
}

constexpr EffortSpec EFFORT_SPECS[] = {
    // kissat `eliminateeffort` 100 per mille (options.h:44).
    {"BVE", 100, meter_bve, true, false, nullptr},
    // cadical `probeeffort` 8 per mille (options.hpp:164) — the only reference
    // rate for this technique. kissat has no `probeeffort`: its probing phase is
    // bounded by the conflict INTERVAL alone. The previous 20 was neither
    // reference's number.
    {"Probe", 8, meter_probe, true, false, &Stats::probe_ticks},
    // Rides probing's rate: cadical runs HBR inside the probing propagation
    // under `probehbr` with no `hbreffort` of its own, and kissat has no hyper
    // binary resolution at all, so there is no reference share to port and no
    // structural per-candidate cap to copy. `budget_aware`, so one root's
    // resolvent loop stops at the ledger residual rather than at the closure —
    // the sweep/kitten shape. Accepted deviation `D-HBR-STANDALONE`
    // (docs/implementation-notes.md §13.3, plan rev 91).
    {"HyperBinaryRes", 8, meter_hbr, true, true, &Stats::hbr_ticks},
    {"Backbone", 20, meter_backbone, true, false, &Stats::backbone_ticks},
    {"TransitiveReduce", 20, meter_transitive, true, false},
    // kissat `vivifyeffort` 100 per mille (options.h:161).
    {"Vivify", 100, meter_vivify, false, true, &Stats::vivify_ticks},
    // Strengthen/SubsumeForward must be budget_aware: unbudgeted, the per-mille
    // bounds only how often a round starts. kissat `forwardeffort` 100
    // (options.h:72); its currency there is `forward_steps`, here partner
    // visits.
    {"Strengthen", 100, meter_forward_pair, false, true, nullptr},
    {"SubsumeForward", 100, meter_forward_pair, false, true, nullptr},
    // kissat factoreffort 50 per mille (options.h:57); the first dose per solve
    // takes the factoriniticks allowance instead (see set_entry_limit).
    {"Factor", 50, meter_factor, false, true, nullptr},
    // 8 per mille is cadical's `ternaryeffort` (options.hpp:240); kissat has no
    // ternary resolution, so this rate is cadical's and has no kissat analogue.
    {"HyperTernaryRes", 8, meter_ternary, false, true, nullptr},
    // No reference rate exists: kissat has no BCE and cadical ships `block`=0,
    // bounding it structurally (`blockocclim` 1e2 -> `Kernel::bce_occ_limit`).
    // Shipping it ON is the standing baseline, so this rate is a local choice —
    // an accepted deviation, not a ported constant.
    {"BCE", 20, meter_bce, false, true},
    // cadical's `covereffort` is 4 per mille (options.hpp:73); `covermaxeff` is
    // not ported, being in propagations where `cce_ticks` counts literals.
    {"CCE", 4, meter_cce, false, true},
    // Instantiate's probes are propagation: ticks, at the cheap-probing rate.
    // Same shape as BCE: cadical ships `instantiate`=0 and bounds it
    // structurally (`instantiateclslim`); kissat has no counterpart. The rate
    // is a local choice.
    {"Instantiate", 20, meter_instantiate, false, true, &Stats::instantiate_ticks},
    // kissat's `sweepeffort` is 100 per mille (options.h:143).
    {"Sweep", 100, meter_sweep, false, true},
    // kissat gives congruence no effort budget at all — it is gated by the
    // delay ladder and `congruenceonce` instead. This budget is deliberately
    // stricter than the reference, an accepted deviation carrying its own
    // measurement in docs/implementation-notes.md §13.3. The first round per
    // solve bypasses it (set_entry_limit).
    {"Congruence", 100, meter_congruence, false, true, nullptr, true},
};

const EffortSpec *effort_spec(const char *name) {
  for (const EffortSpec &s : EFFORT_SPECS)
    if (!std::strcmp(s.name, name))
      return &s;
  return nullptr; // all-or-nothing rule: no titration, no meter
}

// --- the authored inprocessing phase order (see SlotOrderRow) ---------------
// Merged from the references' pipelines (kissat probe.c:32-41, eliminate.c:441;
// cadical probe.cpp:934-952, elim.cpp:1044-1098): Substitute precedes every
// reader of the binary implication graph, Congruence feeds Substitute, Vivify
// precedes TransitiveReduce, Factor closes probing, subsumption precedes BVE.
constexpr SlotOrderRow AUTHORED_SLOT_ORDER[] = {
    // kissat's `probe()` body verbatim (probe.c:33-41), repeats included:
    //   congruence, substitute, backbone, vivify, sweep, substitute,
    //   transitive_reduction, backbone, factor
    // The repeats are load-bearing rather than cosmetic. Congruence and Sweep
    // do not retire variables themselves — they compute representatives, and
    // the `substitute` that FOLLOWS each of them is what marks the variables
    // eliminated (substitute.c:259). Drop either pairing and the round loop's
    // fixpoint test, which reads the active-variable count, sees no progress
    // from them at all.
    {"Congruence", SlotGroup::Probe},
    {"Substitute", SlotGroup::Probe},
    // The extra rules, placed without disturbing kissat's relative order. X6 sits before
    // the failed-literal pair because its resolvents densify the binary graph
    // they close over; V1 Probe is full-propagation failed literals, which is
    // what kissat's binary-only `backbone` is the cheap half of.
    {"HyperBinaryRes", SlotGroup::Probe},
    {"Backbone", SlotGroup::Probe},
    {"Probe", SlotGroup::Probe},
    {"Vivify", SlotGroup::Probe},
    {"Sweep", SlotGroup::Probe},
    {"Substitute", SlotGroup::Probe}, // kissat probe.c:38 — retires Sweep's work
    {"TransitiveReduce", SlotGroup::Probe},
    {"HyperTernaryRes", SlotGroup::Probe}, // cadical's ternary; kissat has none
    {"Backbone", SlotGroup::Probe},        // kissat probe.c:40
    {"Factor", SlotGroup::Probe},
    // kissat's `eliminate` round is forward subsumption then the elimination
    // sweep (eliminate.c:439-441, :468); the rest are local or cadical's.
    {"SubsumeForward", SlotGroup::Eliminate},
    {"Strengthen", SlotGroup::Eliminate},
    {"BVE", SlotGroup::Eliminate},
    {"Instantiate", SlotGroup::Eliminate},
    {"BCE", SlotGroup::Eliminate},
    {"CCE", SlotGroup::Eliminate},
};


} // namespace

const SlotOrderRow *default_slot_order(size_t *count) {
  if (count)
    *count = sizeof(AUTHORED_SLOT_ORDER) / sizeof(AUTHORED_SLOT_ORDER[0]);
  return AUTHORED_SLOT_ORDER;
}

// --- candidate ORDER: the arms and the ranker -------------------------------

const char *candidate_order_name(CandidateOrder o) {
  switch (o) {
  case CandidateOrder::Native:
    return "native";
  case CandidateOrder::Reverse:
    return "reverse";
  case CandidateOrder::Random:
    return "random";
  case CandidateOrder::OccAsc:
    return "occ-asc";
  case CandidateOrder::OccDesc:
    return "occ-desc";
  case CandidateOrder::NegOccDesc:
    return "negocc-desc";
  case CandidateOrder::Heuristic:
    return "heuristic";
  }
  return "native";
}

bool parse_candidate_order(const char *s, CandidateOrder *out) {
  static const struct {
    const char *n;
    CandidateOrder o;
  } table[] = {
      {"native", CandidateOrder::Native},
      {"reverse", CandidateOrder::Reverse},
      {"random", CandidateOrder::Random},
      {"occ-asc", CandidateOrder::OccAsc},
      {"occ-desc", CandidateOrder::OccDesc},
      {"negocc-desc", CandidateOrder::NegOccDesc},
      {"heuristic", CandidateOrder::Heuristic},
  };
  for (const auto &row : table)
    if (!std::strcmp(s, row.n)) {
      *out = row.o;
      return true;
    }
  return false;
}

namespace {

// The key a resumption cursor is expressed in: the payload's literal, or the
// raw payload, so the cursor never constrains the payload shape.
uint64_t instance_key(uint64_t inst) {
  Lit l;
  return lit_of_instance(inst, &l) ? static_cast<uint64_t>(l) : inst;
}

// Occurrences of `l` as watch entries, not words.
uint32_t lit_occurrences(const Kernel &k, Lit l, bool binary_only) {
  uint32_t n = 0;
  for (const WatchEntry w : k.watches.entries(l))
    n += w.binary() ? 1u : (binary_only ? 0u : 1u);
  return n;
}

// Sort key for the comparison arms, ascending throughout: a descending arm
// returns the complement, so a stable sort keeps native rank as the tie-break.
uint64_t order_key(const Kernel &k, CandidateOrder arm, uint64_t inst) {
  Lit l;
  if (!lit_of_instance(inst, &l) || var_of(l) >= k.nvars)
    return 0; // not a literal payload: leave it where the native order had it
  const Var v = var_of(l);
  switch (arm) {
  case CandidateOrder::OccAsc:
  case CandidateOrder::OccDesc: {
    const uint64_t occ = lit_occurrences(k, make_lit(v, false), false) +
                         lit_occurrences(k, make_lit(v, true), false);
    return arm == CandidateOrder::OccAsc ? occ : ~occ;
  }
  case CandidateOrder::NegOccDesc:
    // cadical probe.cpp:624/685/763: rank = noccs(-p) over binaries, popped
    // from the back, so the most-negated literal is probed first.
    return ~static_cast<uint64_t>(lit_occurrences(k, negate(l), true));
  case CandidateOrder::Heuristic:
    // kissat transitive.c:222-260: stable sorts on the EVSIDS heap score,
    // focused on the VMTF stamp, both ascending and walked front to back.
    if (k.h->stable) {
      // Non-negative IEEE-754 doubles compare as unsigned bit patterns.
      const double s = k.h->scores.score[v];
      uint64_t bits = 0;
      std::memcpy(&bits, &s, sizeof bits);
      return bits;
    }
    return k.h->links[v].stamp;
  default:
    return 0;
  }
}

} // namespace

void DefaultCandidateOrder::rank(const Kernel &k, uint32_t rule,
                                 uint64_t *insts, uint32_t n) {
  const CandidateOrder arm = arm_of(rule);
  if (arm == CandidateOrder::Native || n < 2)
    return;
  if (arm == CandidateOrder::Reverse) {
    std::reverse(insts, insts + n);
    return;
  }
  if (arm == CandidateOrder::Random) {
    // splitmix64 Fisher-Yates, seeded by a constant so the whole solve replays:
    // an unreproducible order cannot be fuzzed or bisected.
    for (uint32_t i = n; i > 1; i--) {
      rng_ += 0x9e3779b97f4a7c15ull;
      uint64_t z = rng_;
      z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
      z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
      z ^= z >> 31;
      std::swap(insts[i - 1], insts[z % i]);
    }
    return;
  }
  // H is optional policy state: with none, fall back to the native order.
  if (arm == CandidateOrder::Heuristic && !k.h)
    return;
  // Persistent scratch: inside the dose loop a per-call linear allocation is a
  // measured hazard.
  scratch_.resize(n);
  for (uint32_t i = 0; i < n; i++)
    scratch_[i] = {order_key(k, arm, insts[i]), insts[i]};
  std::stable_sort(
      scratch_.begin(), scratch_.end(),
      [](const std::pair<uint64_t, uint64_t> &a,
         const std::pair<uint64_t, uint64_t> &b) { return a.first < b.first; });
  for (uint32_t i = 0; i < n; i++)
    insts[i] = scratch_[i].second;
}

// Resume, then rank, the instance list this occasion will walk — resume first,
// since the cursor lives in the rule's own key space. Both are permutations
// (N1).
void DefaultPolicy::order_candidates(const Kernel &k, SlotEntry &e,
                                     uint64_t *insts, uint32_t n) {
  if (n == 0)
    return;
  e.ord.lists++;
  e.ord.candidates += n;
  if (e.cursor_enabled) {
    uint32_t start = 0;
    while (start < n && instance_key(insts[start]) < e.cursor)
      start++;
    if (start >= n)
      start = 0; // the cursor ran past the set: begin again at the head
    if (start) {
      std::rotate(insts, insts + start, insts + n);
      e.ord.rotated++;
    }
  }
  // The default ranker is a no-op at the Native arm, so skip the step then.
  if (!candidate_order && e.order == CandidateOrder::Native)
    return;
  CandidateOrderPolicy &cp =
      candidate_order ? *candidate_order : default_candidate_order_;
  order_before_.assign(insts, insts + n);
  cp.rank(k, static_cast<uint32_t>(e.rule), insts, n);
  uint64_t moved = 0;
  for (uint32_t i = 0; i < n; i++)
    moved += (order_before_[i] != insts[i]);
  if (moved) {
    e.ord.ranked++;
    e.ord.moved += moved;
  }
}

bool DefaultPolicy::want_slot_phase(const Kernel &k, SlotGroup g) {
  if (!inprocess.initialized)
    return false;
  SlotClock &c = g == SlotGroup::Probe ? probe_clock : elim_clock;
  if (k.stats.conflicts < c.next_conflicts)
    return false;
  // The interval is necessary but not sufficient (see SlotClock); each clock's
  // first phase is exempt from both gates, as in both references.
  const bool gated =
      slot_watermark_ && c.rounds != 0 &&
      (g == SlotGroup::Probe
           // cadical probe.cpp:23 — no inprobe phase unless a reduce has
           // happened since the last. Provably inert at the shipped intervals.
           ? k.stats.reductions == c.reductions_mark
           // kissat eliminate.c:33-38 / cadical elim.cpp:75-80 — wait for new
           // root units or newly marked elimination candidates.
           : (k.stats.bve_marks <= c.marks_mark &&
              kernel_root_fixed(k) <= c.fixed_mark));
  if (gated) {
    if (!c.blocked) { // count episodes, not search steps
      c.blocked = true;
      c.blocked_episodes++;
    }
    return false;
  }
  return true;
}

// Each clock's interval, on its own reference formula (see SlotGroup): probeint
// or eliminateint times n·log10(n+9) to the first or second power, size-scaled.
void DefaultPolicy::update_slot_limit(const Kernel &k, SlotGroup g) {
  SlotClock &c = g == SlotGroup::Probe ? probe_clock : elim_clock;
  // TACTIX_SLOT_CLOCKS=1: one cadence, both clocks on the elimination formula
  // and the larger phase count. Only the clock that just fired advances.
  const bool split = slot_clocks_split_;
  uint64_t n = split ? c.rounds
                     : std::max(probe_clock.rounds, elim_clock.rounds);
  if (!n)
    n = 1;
  const double lg = std::log10(static_cast<double>(n) + 9);
  const double base = split && g == SlotGroup::Probe
                          ? PROBE_INTERVAL * static_cast<double>(n) * lg
                          : SUBSUME_INTERVAL * static_cast<double>(n) * lg * lg;
  c.next_conflicts = k.stats.conflicts +
                     scale_inprocessing_delta(k, static_cast<uint64_t>(base));
}

// Reduce cadence. Default: arithmetically growing interval; TACTIX_REDUCE_SQRT
// selects kissat's reduceint·√reductions, which diverges fast.
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

// How much of the ranked candidate list dies. Default: kissat's flat 75 %;
// under TACTIX_REDUCE_SQRT its ramp (reduce.c), which belongs with the cadence.
size_t GlueTieredReduction::victims(const Kernel &k, size_t candidates) {
  if (!ramp)
    return reduce_canonical_victims(candidates);
  // kissat increments `reductions` at the top of its reduce; the count here is
  // still pre-increment, hence +1.
  const double r = static_cast<double>(k.stats.reductions) + 1.0;
  const double percent = 90.0 - 40.0 / std::log10(r + 9.0);
  return static_cast<size_t>(static_cast<double>(candidates) * percent /
                             100.0);
}

Lit DefaultBranching::pick(Kernel &k) {
  assert(k.unassigned > 0);
  // Random decisions: focused mode only (kissat randecstable defaults to 0),
  // and a new sequence may only begin at level <= 1, matching decide.c:103.
  if (randec_enabled && !k.h->stable) {
    const uint64_t c = k.stats.conflicts;
    if (c >= randec_until && k.level <= 1 && c >= randec_next) {
      randec_sequences++;
      const double n = static_cast<double>(randec_sequences);
      const double logn = std::log10(n + 9.0);
      randec_until = c + static_cast<uint64_t>(RANDEC_LENGTH * logn) + 1;
      randec_next = c + static_cast<uint64_t>(RANDEC_INIT * logn);
      k.stats.random_sequences++;
    }
    if (c < randec_until) {
      // uniform over UNASSIGNED variables; splitmix64 so the schedule stays
      // reproducible (a random policy that is not replayable is not testable)
      for (uint32_t tries = 0; tries < 64; tries++) {
        randec_rng += 0x9E3779B97F4A7C15ull;
        uint64_t z = randec_rng;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        const Var v = static_cast<Var>(z % k.nvars);
        if (k.values[make_lit(v, true)] == 0 && !kernel_var_retired(k, v)) {
          k.stats.random_decisions++;
          return heuristics_decide_lit(k, v);
        }
      }
      // dense assignment: fall through to the heuristic rather than spin
    }
  }
  const Var v = heuristics_next_decision_var(k);
  return heuristics_decide_lit(k, v);
}

// The env-arm ingestion half of solve(): every A/B knob DefaultPolicy owns,
// read once per solve, before reset_schedule re-bases the limits.
void DefaultPolicy::configure_from_env() {
  // A/B knob: TACTIX_FORCE_MODE=focused|stable (0 = off; see the SwitchMode
  // decline below). Read once — the check sits on the CDCL outer loop.
  force_mode_ = 0;
  if (const char *fm = std::getenv("TACTIX_FORCE_MODE")) {
    if (!std::strcmp(fm, "focused"))
      force_mode_ = 1;
    else if (!std::strcmp(fm, "stable"))
      force_mode_ = 2;
  }
  // Closure delay ladder, default on: an unproductive unmetered closure would
  // otherwise re-run in full every outer round. A delay, never a disable.
  closure_delay_ = std::getenv("TACTIX_NO_CLOSURE_DELAY") == nullptr;
  // Reason-side bumping ships with the dynamic tiers below because the pair is
  // one mechanism, and measuring one half of it is meaningless.
  bump_reasons_limit_ =
      std::getenv("TACTIX_NO_BUMP_REASONS") ? 0 : BUMP_REASONS_LIMIT;
  if (const char *bl = std::getenv("TACTIX_BUMP_REASONS_LIMIT"))
    bump_reasons_limit_ = static_cast<uint32_t>(std::strtoul(bl, nullptr, 10));
  bump_reasons_rate_ = BUMP_REASONS_RATE;
  if (const char *br = std::getenv("TACTIX_BUMP_REASONS_RATE"))
    bump_reasons_rate_ = std::strtod(br, nullptr);
  factor_init_ticks_ = FACTOR_INIT_TICKS;
  if (const char *fi = std::getenv("TACTIX_FACTOR_INIT_TICKS"))
    factor_init_ticks_ = std::strtoull(fi, nullptr, 10);
  congruence_init_ticks_ = CONGRUENCE_INIT_TICKS;
  if (const char *ci = std::getenv("TACTIX_CONG_INIT_TICKS"))
    congruence_init_ticks_ = std::strtoull(ci, nullptr, 10);
  // Reduce-cadence arm: kissat's √ interval + ramped fraction, separately
  // settable so a win can be attributed; TACTIX_REDUCE_SQRT sets both.
  const bool both = std::getenv("TACTIX_REDUCE_SQRT") != nullptr;
  const char *cad = std::getenv("TACTIX_REDUCE_CADENCE"); // "sqrt" | "arith"
  const char *rmp = std::getenv("TACTIX_REDUCE_RAMP");    // "1" | "0"
  reduce_sqrt_ = cad ? !std::strcmp(cad, "sqrt") : both;
  default_reduction_.ramp = rmp ? (rmp[0] == '1') : both;
  dynamic_tiers_ = std::getenv("TACTIX_NO_DYNAMIC_TIERS") == nullptr;
}

// The inprocessing-slot construction half of solve(). Must run after
// configure_from_env + reset_schedule.
void DefaultPolicy::build_inprocess_slot(Kernel &k) {
  // A preposed (nonempty) slot is left untouched: to run nothing, prepose
  // disabled entries, not an empty vector.
  if (inprocess_slot.empty()) {
    // Composition (kissat's, effort-scheduler design §4.4): every
    // Inprocess-tier rule enabled, BVE alone deferred out of preprocessing
    // (kissat fastel=0).
    const std::vector<RuleVTable> &rules = tactix_rules();
    // TACTIX_NO_BVE / TACTIX_NO_FACTOR: the two named ablation knobs, kept
    // because each rule has a standing open investigation.
    const bool bve_off = std::getenv("TACTIX_NO_BVE") != nullptr;
    const bool factor_off = std::getenv("TACTIX_NO_FACTOR") != nullptr;
    // TACTIX_SLOT_ENABLE / TACTIX_SLOT_DISABLE: comma-separated rule names
    // added to or removed from the composition, as the §5.6 arm surface will
    // via API.
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
    const auto add_entry = [&](int i) {
      {
        const char *nm = rules[i].name;
        // Baseline: every implemented proof rule ships scheduled. A rule-off
        // arm is a diagnostic for a scheduling defect, never a default.
        bool on = !(bve_off && !std::strcmp(nm, "BVE")) &&
                  !(factor_off && !std::strcmp(nm, "Factor"));
        if (env_list("TACTIX_SLOT_ENABLE", nm))
          on = true;
        if (env_list("TACTIX_SLOT_DISABLE", nm))
          on = false;
        SlotEntry e;
        e.defer_preprocess = !std::strcmp(nm, "BVE");
        e.rule = i;
        e.enabled = on;
        // Which clock offers this entry its occasions, read by name so clock
        // and order stay separately priceable.
        {
          size_t norder = 0;
          const SlotOrderRow *order = default_slot_order(&norder);
          for (size_t o = 0; o < norder; o++)
            if (!std::strcmp(order[o].name, nm)) {
              e.group = order[o].group;
              break;
            }
        }
        // A rule with no ledger row is un-titratable, so closure-gated.
        e.closure = true;
        if (const EffortSpec *s = effort_spec(nm)) {
          e.closure = s->closure;
          e.permille = s->permille;
          e.meter = s->meter;
          e.tick_counter = s->tick_counter;
          e.dosed = s->dosed;
          e.budget_aware = s->budget_aware;
          // TACTIX_SLOT_PERMILLE=Name:rate[,...] (diagnostic): override an
          // entry's effort rate without a rebuild, so a sweep fits one window.
          if (const char *pm = std::getenv("TACTIX_SLOT_PERMILLE")) {
            const size_t n = std::strlen(nm);
            for (const char *p = pm; (p = std::strstr(p, nm)); p += n)
              if ((p == pm || p[-1] == ',') && p[n] == ':') {
                e.permille = static_cast<uint32_t>(strtoul(p + n + 1, nullptr, 10));
                break;
              }
          }
        }
        // TACTIX_SLOT_MAX_APPLIES=Name:N (diagnostic): cap scheduling phases.
        if (const char *mx = std::getenv("TACTIX_SLOT_MAX_APPLIES")) {
          const size_t n = std::strlen(nm);
          for (const char *p = mx; (p = std::strstr(p, nm)); p += n)
            if ((p == mx || p[-1] == ',') && p[n] == ':') {
              e.max_applies =
                  static_cast<uint32_t>(strtoul(p + n + 1, nullptr, 10));
              break;
            }
        }
        // Cursor default: on for the dosed entries enumerating in key order,
        // off for BVE (see SlotEntry::cursor). Order arm: Native.
        if (e.dosed)
          e.cursor_enabled = std::strcmp(nm, "BVE") != 0;
        // One knob per rule, spelled from the registry name so a newly dosed
        // rule gets its own: TACTIX_CURSOR_<RULENAME> /
        // TACTIX_ORDER_<RULENAME>.
        char knob[96];
        const auto knob_name = [&knob](const char *prefix,
                                       const char *name) {
          size_t p = 0;
          for (const char *s = prefix; *s && p + 1 < sizeof knob; s++)
            knob[p++] = *s;
          for (const char *s = name; *s && p + 1 < sizeof knob; s++)
            knob[p++] = static_cast<char>(
                std::toupper(static_cast<unsigned char>(*s)));
          knob[p] = '\0';
        };
        // A knob that cannot take effect aborts rather than being ignored: an
        // arm set on an unordered entry is the vacuous-A/B failure mode.
        const auto reject = [&knob](const char *why) {
          std::fprintf(stderr, "c ERROR: %s: %s\n", knob, why);
          std::abort();
        };
        knob_name("TACTIX_CURSOR_", nm);
        if (const char *cv = std::getenv(knob)) {
          if (!e.dosed)
            reject("this entry runs as a canonical batch round, so the "
                   "policy never walks its candidates and a cursor cannot "
                   "take effect");
          e.cursor_enabled = std::strtoul(cv, nullptr, 10) != 0;
        }
        knob_name("TACTIX_ORDER_", nm);
        if (const char *ov = std::getenv(knob)) {
          if (!e.dosed)
            reject("this entry runs as a canonical batch round, so the "
                   "policy never sees its candidate list and an order arm "
                   "cannot take effect");
          if (!parse_candidate_order(ov, &e.order))
            reject("unknown candidate-order arm (native, reverse, random, "
                   "occ-asc, occ-desc, negocc-desc, heuristic)");
        }
        inprocess_slot.push_back(e);
      }
    };
    // Phase order: the authored list, with an unnamed Inprocess-tier rule
    // appended in registry order as a safety net rather than by design.
    const char *order_env = std::getenv("TACTIX_SLOT_ORDER");
    slot_order_legacy_ = order_env && !std::strcmp(order_env, "legacy");
    // The two clock arms (see SlotClock), read once, here.
    if (const char *cl = std::getenv("TACTIX_SLOT_CLOCKS"))
      slot_clocks_split_ = std::strtoul(cl, nullptr, 10) != 1;
    if (const char *wm = std::getenv("TACTIX_SLOT_WATERMARK"))
      slot_watermark_ = std::strtoul(wm, nullptr, 10) != 0;
    if (const char *fc = std::getenv("TACTIX_FACTOR_COUPLE"))
      couple_factor_bound_ = std::strtoul(fc, nullptr, 10) != 0;
    // TACTIX_CHRONO_REUSE is read in solve(): this block runs under the
    // one-shot slot guard, which a preposed slot or a reused policy skips.
    if (slot_order_legacy_) {
      for (int i = 0; i < static_cast<int>(rules.size()); i++)
        if (rules[i].tier == RuleTier::Inprocess)
          add_entry(i);
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
    } else {
      size_t norder = 0;
      const SlotOrderRow *order = default_slot_order(&norder);
      std::vector<bool> placed(rules.size(), false);
      // One entry per occurrence, so a rule the order names twice is visited
      // twice — kissat runs substitute and backbone twice per probing round.
      // Each occurrence carries its own SlotEntry, hence its own limit and
      // telemetry, which is what kissat does too: both calls share the
      // technique's cumulative counter but each recomputes `counter + delta`.
      for (size_t o = 0; o < norder; o++)
        for (int i = 0; i < static_cast<int>(rules.size()); i++)
          if (rules[i].tier == RuleTier::Inprocess &&
              !std::strcmp(rules[i].name, order[o].name)) {
            placed[i] = true;
            add_entry(i);
            break;
          }
      // An Inprocess rule the authored order does not name still gets one
      // occasion, appended in registry order — a safety net, not a design.
      for (int i = 0; i < static_cast<int>(rules.size()); i++)
        if (rules[i].tier == RuleTier::Inprocess && !placed[i])
          add_entry(i);
    }
    // Hand the built-in ranker the arms the knobs selected, indexed by registry
    // id so one object serves every entry — the shape the §5.6 learner needs.
    default_candidate_order_.arms.assign(rules.size(),
                                         CandidateOrder::Native);
    for (const SlotEntry &se : inprocess_slot)
      default_candidate_order_.arms[se.rule] = se.order;
  }
  if (!inprocess.initialized) {
    inprocess.initialized = true;
    // Preprocess: both clocks fire at conflicts=0, before any search state
    // exists, so simplification never perturbs an in-progress search.
    probe_clock.next_conflicts = k.stats.conflicts;
    elim_clock.next_conflicts = k.stats.conflicts;
  }
}

// The shape of kissat search.c's guard->apply chain: propagate, analyze +
// backjump on conflict, SAT when all assigned, else decide, with the policy ops
// between conflicts in kissat's check order. Rules never schedule.
int DefaultPolicy::solve(Kernel &k, volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  if (const char *cl = std::getenv("TACTIX_CHRONO_LEVELS")) {
    const long n = std::strtol(cl, nullptr, 10);
    if (n >= 0)
      chrono_levels = static_cast<uint32_t>(n);
  }
  // Read beside its sibling knob: the old home in build_inprocess_slot ran
  // under that function's one-shot guard, so a preposed inprocess slot or a
  // second solve() on the same policy object silently ignored the arm.
  if (const char *cr = std::getenv("TACTIX_CHRONO_REUSE"))
    chrono_reuse_trail_ = std::strtoul(cr, nullptr, 10) != 0;
  // Random-decision sequences ship on (kissat `randec=1`); `=0` is the off arm.
  if (const char *rd = std::getenv("TACTIX_RANDEC"))
    default_branching_.randec_enabled = (*rd != '0');
  if (k.inconsistent)
    return 20;
  // DefaultPolicy reads `H` directly and the kernel does not own it: use the
  // session's if attached, else own one so a bare `default_solve(k)` works.
  std::optional<ScopedHeuristics> owned_h;
  if (!k.h)
    owned_h.emplace(k);
  solves++;
  configure_from_env();
  // Per-solve scheduling state, after the env arms above because two re-based
  // limits depend on them. A reused policy object must look fresh.
  if (reset_schedule_per_solve)
    reset_schedule(k);
  build_inprocess_slot(k);
  bool lucky_tried = false;
  for (;;) {
    const ApplyResult r = propagate_apply(k);
    if (r == ApplyResult::Unsat)
      return 20;
    // Lucky: once, at search start after the initial root fixpoint (kissat
    // search.c luckyearly; luckylate joins with preprocessing)
    if (!lucky_tried && r == ApplyResult::Ok) {
      lucky_tried = true;
      if (lucky_guard(k) && lucky_apply(k) == ApplyResult::Sat)
        return 10;
    }
    if (r == ApplyResult::Conflict) {
      observe_decision_rate(k); // kissat: before the analysis, once per conflict
      // reason-side bumping is rule configuration, scoped to this apply
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
      // An invalid pending record means the conflict was reused as its own
      // driving clause: analyze learned nothing, so `k.pending` is stale and
      // the learn/backjump tail must be skipped (kissat analyze.c).
      if (!k.pending.valid) // `stats.conflict_reused` already counted it
        continue;
      observe_glue(k, k.pending.glue); // policy observes; Analyze is pure
      // second retier channel (kissat analyze.c update_tier_limits), a doubling
      // conflict interval capped at 2^16 so the bands settle early
      if (dynamic_tiers_ && k.stats.conflicts > retier_.next_conflicts) {
        reduce_retier(k);
        if (retier_.interval < (1u << 16))
          retier_.interval *= 2;
        retier_.next_conflicts = k.stats.conflicts + retier_.interval;
      }
      if (k.h->stable)
        tick_reluctant(); // per learned clause (kissat learn.c)
      {
        // chrono (kissat learn.c): a long backjump wastes the prefix, so step
        // back one level when the gap exceeds the limit
        const uint32_t jump = k.pending.jump_level;
        const uint32_t back = k.level - 1;
        uint32_t target = jump;
        if (back - jump > chrono_levels) {
          k.stats.chronological++;
          target = back;
        } else if (chrono_reuse_trail_ && chrono_levels != UINT32_MAX &&
                   jump < back) {
          // The UINT32_MAX conjunct is the chrono master switch: kissat spells
          // "chrono off" as a huge level limit (learn.c backjump_limit), and
          // cadical's opts.chrono=0 disables reuse outright (analyze.cpp:655)
          // — without it the off arm still emits chronological targets here.
          target = reuse_trail_level(k, jump);
          if (target != jump)
            k.stats.chronological++;
        }
        backjump_apply_to(k, target);
      }
      continue;
    }
    // SAT when every active variable is assigned: a retired variable occurs in
    // no clause, and ρ supplies its value in kernel_extend_model. A policy
    // decision, not a guard — an external policy may still decide one (N1).
    if (k.retire_enabled ? k.unassigned_active == 0 : k.unassigned == 0)
      return 10;
    if (want_reduce(k) && reduce_guard(k)) {
      // sub-policy chooses how much of the ranked candidate list dies
      ReductionPolicy &rp = reduction ? *reduction : default_reduction_;
      // kissat recomputes the tier limits at the top of every reduce, so the
      // selection below sees the new bands (reduce.c:159)
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
    // TACTIX_FORCE_MODE: pin the search mode by declining every SwitchMode
    // application; the rule stays guard-true and N1-appliable.
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
    // kissat search.c order: reduce, switch, restart, reorder, rephase, probe,
    // eliminate. Off unless TACTIX_REORDER is set.
    if (want_reorder(k) && reorder_guard(k)) {
      reorder_apply(k);
      reorder.count++;
      update_reorder_limit(k);
      continue;
    }
    if (want_rephase(k) && rephase_guard(k)) {
      // kissat couples a backtrack-to-root into kissat_rephase; here the same
      // effect is composed from the rule vocabulary (see rephase.cpp).
      if (restart_guard(k))
        restart_apply_to(k, 0);
      // Deviation: the rotation is B,B+W,I,B,B+W,O where kissat rotates
      // B,W,I,B,W,O, i.e. Walk augments Best rather than replacing it, because
      // this kernel saves phases at unassign where kissat saves at assign.
      static constexpr uint64_t WALK_SLOT = UINT64_MAX;
      static constexpr uint64_t ROTATION[6] = {
          REPHASE_BEST, WALK_SLOT, REPHASE_INVERTED,
          REPHASE_BEST, WALK_SLOT, REPHASE_ORIGINAL};
      const uint64_t type = ROTATION[rephase.count % 6];
      rephase.count++; // ONE rephase event, whatever it is composed of
      if (type == WALK_SLOT && walk_guard(k)) {
        rephase_apply_type(k, REPHASE_BEST); // the slot keeps its Best
        // Effort budget in Walk's own currency (kissat SET_EFFORT_LIMIT(walk)).
        // Deviation: the window runs from the last walk, not the last elim.
        uint64_t reference = search_ticks(k) - walk_anchor;
        if (reference < MIN_EFFORT_TICKS)
          reference = MIN_EFFORT_TICKS;
        // TACTIX_WALK_PERMILLE (diagnostic): see WALK_PERMILLE for why the
        // per-mille is not kissat's.
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
    // The inprocessing slot, in the kissat eliminate schedule (search.c checks
    // eliminate after rephase, before decide). Rules need a quiescent root, so
    // the coupled backtrack is composed from the rule vocabulary.
    if (want_slot_phase(k, SlotGroup::Probe)) {
      if (run_slot_phase(k, SlotGroup::Probe, stop) == 20)
        return 20;
      continue;
    }
    if (want_slot_phase(k, SlotGroup::Eliminate)) {
      if (run_slot_phase(k, SlotGroup::Eliminate, stop) == 20)
        return 20;
      continue;
    }
    if (stop && *stop)
      return 0;
    // sub-policy chooses the branch (any unassigned literal is legal, N1)
    BranchingPolicy &bp = branching ? *branching : default_branching_;
    decide_apply(k, bp.pick(k), false);
  }
}


namespace {
// TACTIX_SLOT_TRACE: per-entry scheduling visibility, for debugging.
void slot_trace_entry(const Kernel &k, const DefaultPolicy::SlotEntry &e,
                      const RuleVTable &r, int outer) {
  if (!std::getenv("TACTIX_SLOT_TRACE"))
    return;
  std::fprintf(stderr,
               "[slot] phase=%llu outer=%d %s enabled=%d defer=%d "
               "guard=%d dosed=%d meter=%llu limit=%llu\n",
               (unsigned long long)k.stats.inprocess_phases, outer, r.name,
               (int)e.enabled, (int)e.defer_preprocess, (int)r.guard(k),
               (int)e.dosed, (unsigned long long)(e.meter ? e.meter(k) : 0),
               (unsigned long long)e.limit);
}
} // namespace

// Pre-checks every occasion passes before its budget is computed; a decline is
// charged to a counter, keeping occasions == admitted + declines.
bool DefaultPolicy::slot_entry_admissible(SlotEntry &e, const Kernel &k,
                                          const SlotPhaseCtx &ctx) {
  if (!e.enabled || ctx.phase_unsat) {
    e.tm.dec_disabled++;
    return false;
  }
  if (e.max_applies && e.applies >= e.max_applies) {
    e.tm.dec_cap++;
    return false; // diagnostic round cap (see SlotEntry::max_applies)
  }
  if (e.defer_preprocess && k.stats.conflicts == 0) {
    e.tm.dec_preprocess++;
    return false; // no elimination at preprocessing (kissat fastel=0)
  }
  e.applies++;
  return true;
}

// How much of this entry's ceiling is left, in its own currency. Saturating:
// the meter can stand above the ceiling, and the difference would wrap to
// ~2^64.
uint64_t DefaultPolicy::ledger_remaining(SlotEntry &e, const Kernel &k,
                                        uint64_t limit) {
  const uint64_t spent = e.meter(k);
  if (limit >= spent)
    return limit - spent;
  e.tm.wrapped++;
  return 0;
}

// One occasion of a dosed entry: apply instances until the budget is spent.
DefaultPolicy::SlotStep
DefaultPolicy::run_dosed_entry(Kernel &k, SlotEntry &e, const RuleVTable &r,
                               SlotPhaseCtx &ctx) {
  const uint64_t limit = e.limit;
  if (!r.guard(k)) {
    e.tm.dec_guard++;
    return SlotStep::Next;
  }
  // The growth-bound ramp, the per-variable retry bit and the three diagnostic
  // hatches below are BVE mechanisms, so they are selected by name here, as
  // Vivify's tier split and Factor's initial allowance already are. A generic
  // per-entry flag set for exactly one rule read as a rule-shape category and
  // was not one.
  const bool bve_entry = !std::strcmp(r.name, "BVE");
  // TACTIX_BVE_ONESHOT (diagnostic): one dose per solve, full ramp and
  // unbounded budget, separating recurrence cost from dose content.
  const bool bve_oneshot = bve_entry && std::getenv("TACTIX_BVE_ONESHOT");
  if (bve_oneshot && e.doses > 0)
    return SlotStep::Next; // the one shot is consumed
  // TACTIX_BVE_CADENCE (diagnostic): one completed call per phase.
  if (bve_entry && ctx.outer > 0 && std::getenv("TACTIX_BVE_CADENCE"))
    return SlotStep::Next;
  // BVE growth-slack ramp (BveBoundPolicy): drive k.bve_bound cheap-first over
  // the sub-policy's passes (kissat eliminate.c set_next_elimination_bound), so
  // later eliminations see a simplified formula. One dose, all passes.
  BveBoundPolicy *ramp = nullptr;
  if (bve_entry && !std::getenv("TACTIX_NO_BVE_RAMP"))
    ramp = bve_bound ? bve_bound : &default_bve_ramp_;
  // In a during-search dose, the completion-gated bound level (bve_level_)
  // replaces the all-levels-per-dose ramp: one pass per dose.
  const bool level_mode = bve_entry && ramp && !bve_oneshot;
  const uint32_t passes = level_mode ? 1 : ramp ? ramp->num_passes() : 1;
  bool dose_cut = false;
  // kissat advances the bound only at a genuine fixpoint (eliminate.c:544): a
  // pass that still eliminated something has not exhausted the current bound.
  uint64_t elim_at_last_pass = k.stats.bve_eliminated;
  // During a ramp the retry bit and the per-variable needed-bound cache
  // (Kernel::bve_min_bound) are consulted together: a bare retry bit, cleared
  // on any attempt, would collapse the ramp to level 0.
  const bool use_retry = bve_entry && !ramp;
  k.bve_track_min_bound = ramp != nullptr;
  for (uint32_t pass = 0; pass < passes && !ctx.phase_unsat; pass++) {
    // Skip remaining ramp levels once the dose is spent, but only when ramping:
    // without one, kernel_bve_retry_sync_fixed and r.instances must still run.
    if (ramp && e.meter(k) > limit) {
      dose_cut = true;
      break;
    }
    if (ramp)
      k.bve_bound = level_mode ? bve_level_ : ramp->bound(pass);
    elim_at_last_pass = k.stats.bve_eliminated; // fixpoint test above
    // Fold newly root-fixed literals into the retry advice before reading it:
    // fixing y changes what BVE computes for y's unmarked neighbours.
    if (bve_entry)
      kernel_bve_retry_sync_fixed(k);
    inprocess_payloads_.resize(2 * static_cast<size_t>(k.nvars) + 1);
    const uint32_t n =
        r.instances(k, inprocess_payloads_.data(),
                    static_cast<uint32_t>(inprocess_payloads_.size()));
    // Where the walk starts and in what order it proceeds: under a cutting
    // budget that is the scheduling decision, since only the head is applied.
    order_candidates(k, e, inprocess_payloads_.data(), n);
    if (std::getenv("TACTIX_SLOT_TRACE"))
      std::fprintf(stderr,
                   "[slot]   %s pass=%u n=%u bve_bound=%u meter=%llu "
                   "limit=%llu\n",
                   r.name, pass, n, k.bve_bound,
                   (unsigned long long)e.meter(k), (unsigned long long)limit);
    for (uint32_t i = 0; i < n; i++) {
      if (e.meter(k) > limit) {
        dose_cut = true;
        break; // dose spent
      }
      if (!r.guard(k)) {
        dose_cut = true; // conservative: not a completed schedule
        break;
      }
      // Policy-side scheduling (kissat flags->eliminate): decline a variable
      // whose outcome for this pass is known. The rule still accepts it (N1).
      if (use_retry || ramp) {
        Lit rl;
        if (lit_of_instance(inprocess_payloads_[i], &rl) &&
            var_of(rl) < k.nvars && !k.bve_retry[var_of(rl)] &&
            (!ramp || k.bve_min_bound[var_of(rl)] > k.bve_bound))
          continue;
      }
      if (ctx.stop && *ctx.stop)
        break; // §6: a dose can enumerate thousands of candidates
      // Advance the resumption cursor over the candidate about to be tried, so
      // a budget cut above resumes here. Written on the attempt, not on
      // success.
      if (e.cursor_enabled) {
        e.cursor = instance_key(inprocess_payloads_[i]) + 1;
        e.ord.advanced++;
      }
      // Residual handoff for a dosed entry whose single instance is unbounded.
      // The loop above polls the ledger between instances, the references' own
      // granularity (kissat eliminate.c:478, cadical probe.cpp:832); they can
      // stop there because a structural cap bounds one candidate
      // (eliminateocclim, eliminateclslim). An entry with no such cap needs the
      // budget pushed into the instance, as kissat's sweep bounds its kitten
      // sub-solve: compute the ceiling once, hand kitten the remainder
      // (sweep.c:53-59). The rule polls what it was handed; the amount stays
      // the policy's decision, so `instances()` is untouched (N1).
      const bool hand_residual = e.budget_aware && e.meter;
      if (hand_residual) {
        const uint64_t remaining = ledger_remaining(e, k, limit);
        k.work_budget = remaining ? remaining : 1;
      }
      const ApplyResult ar = [&] {
        TickSinkScope sink(k, e.tick_counter);
        return apply_rule_instance(r, k, inprocess_payloads_[i]);
      }();
      if (hand_residual)
        k.work_budget = 0;
      if (ar == ApplyResult::Unsat || k.inconsistent) {
        ctx.phase_unsat = true;
        break;
      }
      if (ar != ApplyResult::Ok)
        continue; // this candidate was not viable; try the next
      ctx.round_changed = true;
      ctx.phase_dosed = true;
      // drain any root units the application asserted
      if (propagate_apply(k) == ApplyResult::Unsat) {
        ctx.phase_unsat = true;
        break;
      }
    }
  }
  k.bve_track_min_bound = false; // scope the caching to this ramp
  // The level advance is kissat's `set_next_elimination_bound`, which is called
  // once per `kissat_eliminate` and steps UNCONDITIONALLY: its `complete`
  // predicate reads two variables that a shadowing bug leaves at their
  // initialisers, and a counter-instrumented kissat 4.0.4 takes the completed
  // branch on 5 of 5 elimination phases. Advancing only on an exhausted dose
  // instead — the shape this replaced — saturated the level at the ceiling
  // exactly where BVE is useless, making Factor pickiest where the coupling
  // intends it laxest. Stepped once per PHASE in `run_slot_phase`, since
  // the reference granularity is the phase, not the entry.
  if (level_mode && !ctx.phase_unsat)
    bve_ran_this_phase_ = true;
  (void)elim_at_last_pass;
  // factordelay input: one more BVE round has run. Counted per phase, not per
  // outer round, which would advance the gate three decades too fast.
  if (bve_entry && ctx.outer == 0)
    bve_rounds_run_++;
  if (bve_oneshot)
    e.doses++; // consume the one shot (skipped hereafter)
  e.tm.admitted++; // the dosed branch ran; it exits here, not below
  // A refutation inside a dose does not leave the entry walk: the rest are
  // declined by slot_entry_admissible, keeping telemetry balanced.
  return SlotStep::Next;
}

// Factor's two scheduling gates, both kissat's (factor.c:1068-1085, :1153).
bool DefaultPolicy::factor_entry_declines(const Kernel &k, SlotEntry &e) {
  // factordelay (kissat factor.c:1068-1085) is a size-vs-progress warm-up gate:
  // its truncating form admits active < 10^(eliminations + 5).
  const uint64_t elim = k.stats.bve_eliminated;
  const uint64_t active = k.unassigned > elim ? k.unassigned - elim : 0;
  uint64_t threshold = 1;
  const uint32_t decades = bve_rounds_run_ > 14 ? 19u : bve_rounds_run_ + 5;
  for (uint32_t d = 0; d < decades; d++)
    threshold *= 10;
  if (active >= threshold) {
    e.tm.dec_warmup++;
    return true;
  }
  // Completion-gated new-material watermark (kissat factor.c:1153): re-fire
  // only on material new since the last completed round, so a cut round
  // resumes.
  if (k.stats.conflicts > 0 && k.binary_ids.size() <= e.binaries_mark &&
      k.arena.size() <= e.arena_mark &&
      kernel_root_fixed(k) <= e.root_fixed_mark) {
    e.tm.dec_material++;
    return true;
  }
  return false;
}

// Factor's per-occasion rule config, set around the apply and cleared by the
// caller, so no other caller sees it.
void DefaultPolicy::factor_dose_config(Kernel &k, SlotEntry &) {
  // Composition is unconditional, matching kissat (`resize_factoring` is called
  // from `apply_factoring`, factor.c:838, with no gate). kissat's governors
  // bound it instead, bar the acceptance coupling to BVE's growth allowance
  // (factor.c:72/939).
  k.factor_composite = true;
  // Admission is unconditional. `kissat_factoring` (factor.c:1068-1085) gates
  // on exactly two things — the `factordelay` warm-up and the marked watermark
  // — and both are ported above; a binary-density term appears in neither
  // reference. The gate that used to sit here also read `Kernel::binary_ids`,
  // a proof-id map the kernel documents as size-monotone because deletion keeps
  // the key, so its numerator counted binaries ever derived rather than the
  // live population.
  k.factor_admit = true;
}

// kissat vivify.c structure: the redundant tiers always run within their budget
// share, only the irredundant pass is delay-gated (vivifyirr), and the shares
// are spent as three separately budgeted passes (vivify.c:236).
ApplyResult DefaultPolicy::run_vivify_dose(Kernel &k, SlotEntry &e,
                                           const RuleVTable &r,
                                           uint64_t limit) {
  const uint64_t remaining0 = ledger_remaining(e, k, limit);
  const uint64_t remaining = remaining0 ? remaining0 : 1;
  const bool irr_delayed = k.stats.conflicts > 0 && e.delay_count > 0;
  if (irr_delayed)
    e.delay_count--;
  // TACTIX_VIVIFY_TIER3=0: one tier2-bounded redundant pass holding the 7/10.
  const char *no_t3 = std::getenv("TACTIX_VIVIFY_TIER3");
  const bool split = !(no_t3 && *no_t3 == '0');
  // redundant passes (always; full budget when irr is delayed)
  ApplyResult ar = ApplyResult::NoOp;
  const uint64_t red = irr_delayed ? remaining : remaining * 7 / 10;
  // Kernel::vivify_scope: 1 = the whole tier2-bounded band (the un-split arm),
  // 3/4/5 = kissat's tier1 / tier2 / tier3 bands.
  const uint8_t scopes[3] = {3, 4, 5};
  const uint64_t shares[3] = {red * 3 / 7, red * 3 / 7, red / 7};
  const uint32_t passes = split ? 3u : 1u;
  for (uint32_t p = 0; p < passes && ar != ApplyResult::Unsat && !k.inconsistent;
       p++) {
    k.vivify_scope = split ? scopes[p] : uint8_t{1};
    k.work_budget = std::max<uint64_t>(1, split ? shares[p] : red);
    TickSinkScope sink(k, e.tick_counter);
    const ApplyResult a = apply_rule(r, k);
    if (a == ApplyResult::Unsat ||
        (a == ApplyResult::Ok && ar != ApplyResult::Unsat))
      ar = a;
  }
  // irredundant pass (delay-gated, ratio-tracked on ITS deltas)
  if (!irr_delayed && ar != ApplyResult::Unsat && !k.inconsistent) {
    const uint64_t spent = e.meter(k);
    const uint64_t irr_budget =
        spent < limit ? std::max<uint64_t>(1, limit - spent) : 1;
    const uint64_t tried_before = k.stats.vivify_checked;
    const uint64_t produced_before = k.stats.vivified + k.stats.vivify_deleted;
    k.vivify_scope = 2;
    k.work_budget = irr_budget;
    // The tick sink must scope THIS apply too: without it the irredundant
    // pass's spend never reaches `vivify_ticks`, so the ledger under-counts
    // Vivify by a whole pass and the next dose's limit is anchored below what
    // it actually spent.
    const ApplyResult ar2 = [&] {
      TickSinkScope sink(k, e.tick_counter);
      return apply_rule(r, k);
    }();
    if (ar2 == ApplyResult::Unsat)
      ar = ar2;
    else if (ar2 == ApplyResult::Ok && ar != ApplyResult::Ok)
      ar = ar2;
    if (k.stats.conflicts > 0) {
      // kissat vivify.c: vivified/tried < 1 % bumps, else halve; tried == 0
      // counts unproductive.
      const uint64_t tried = k.stats.vivify_checked - tried_before;
      const uint64_t produced =
          k.stats.vivified + k.stats.vivify_deleted - produced_before;
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
  return ar;
}

// Budget handoff (kernel.hpp work_budget): a budget-aware batch entry gets the
// remaining ledger budget around its apply; 0 leaves other callers unbounded.
ApplyResult DefaultPolicy::run_budgeted_batch(Kernel &k, SlotEntry &e,
                                              const RuleVTable &r,
                                              uint64_t limit,
                                              bool factor_entry) {
  const uint64_t remaining0 = ledger_remaining(e, k, limit);
  const uint64_t handed = remaining0 ? remaining0 : 1;
  const uint64_t spent_before = e.meter(k);
  // Factor doses run composite: kissat's cross-round composition lives in
  // exactly these small doses (resize_factoring).
  if (factor_entry)
    factor_dose_config(k, e);
  k.work_budget = handed;
  ApplyResult ar;
  {
    TickSinkScope sink(k, e.tick_counter);
    ar = apply_rule(r, k);
  }
  k.work_budget = 0;
  k.factor_admit = true; // advice is per-occasion; default is NO restriction
  k.factor_composite = false;
  // A zero-spend NoOp consumes neither the first-dose allowance nor the
  // watermark, as kissat's skipped call leaves limits.factor.marked untouched.
  const uint64_t spent = e.meter(k) - spent_before;
  e.tm.handed += handed;
  e.tm.spent += spent;
  if (spent > 0)
    e.doses++;
  // Completion-gated watermark advance (kissat factor.c:1153): spend within the
  // handed budget means natural exhaustion, so the marks advance.
  if (spent > 0 && spent <= handed) {
    e.tm.completed++;
    e.binaries_mark = k.binary_ids.size();
    e.arena_mark = k.arena.size();
    e.root_fixed_mark = kernel_root_fixed(k);
  }
  return ar;
}

// One occasion of a batch or unmetered entry: one canonical round, metered
// after the fact, or budget-handed for the entries that poll work_budget.
DefaultPolicy::SlotStep
DefaultPolicy::run_batch_entry(Kernel &k, SlotEntry &e, const RuleVTable &r,
                               SlotPhaseCtx &ctx) {
  const uint64_t limit = e.limit;
  if (e.meter && e.meter(k) > limit) {
    e.tm.dec_budget++;
    return SlotStep::Next;
  }
  const bool factor_entry = e.budget_aware && !std::strcmp(r.name, "Factor");
  if (factor_entry && factor_entry_declines(k, e))
    return SlotStep::Next;
  // In a during-search phase a closure entry re-runs only on new material since
  // its own last run: repeating them every phase is a dominant diffuse cost.
  if (e.closure && k.stats.conflicts > 0 &&
      k.binary_ids.size() <= e.binaries_mark &&
      kernel_root_fixed(k) <= e.root_fixed_mark) {
    e.tm.dec_material++;
    return SlotStep::Next;
  }
  // Delay ladder: consume one delayed occasion after material admission, so the
  // ladder counts real scheduling occasions. Not restricted to closures — the
  // rules kissat delays this way are congruence and sweep, and sweep is metered
  // here, so a closure-only gate meant a metered repeater could never back off.
  if (closure_delay_ && (e.closure || delay_spec(r.name)) &&
      k.stats.conflicts > 0 && e.delay_count > 0) {
    e.delay_count--;
    e.tm.dec_delay++;
    return SlotStep::Next;
  }
  if (!r.guard(k)) {
    e.tm.dec_guard++;
    return SlotStep::Next;
  }
  e.tm.admitted++;
  // Ladder inputs, read before the apply: the kernel counters are cumulative,
  // so the rate kissat computes over one call is a delta here.
  const DelaySpec *const dspec = delay_spec(r.name);
  const uint64_t yield_before = dspec ? dspec->yield(k) : 0;
  const uint64_t denom_before = dspec ? dspec->denom(k) : 0;
  // Vivify keeps its scope/delay tier split; the rest take a plain handoff.
  const bool budgeted = e.meter && !e.dosed && e.budget_aware;
  const bool vivify_dose = budgeted && !std::strcmp(r.name, "Vivify");
  ApplyResult ar;
  if (vivify_dose) {
    ar = run_vivify_dose(k, e, r, limit);
  } else if (budgeted) {
    ar = run_budgeted_batch(k, e, r, limit, factor_entry);
  } else {
    TickSinkScope sink(k, e.tick_counter);
    ar = apply_rule(r, k);
  }
  if (e.closure) {
    e.binaries_mark = k.binary_ids.size();
    e.arena_mark = k.arena.size();
    e.root_fixed_mark = kernel_root_fixed(k);
  }
  // Ladder maintenance. Where kissat drives the ladder off a yield rate, use
  // its rate and its denominator; the deltas are taken across this application
  // because the counters are cumulative. A rule kissat does not delay this way
  // keeps the local NoOp test, which is why the two branches differ.
  if (closure_delay_ && k.stats.conflicts > 0 && (e.closure || dspec)) {
    bool unproductive;
    if (dspec) {
      const uint64_t yield = dspec->yield(k) - yield_before;
      const uint64_t denom =
          dspec->denom_delta ? dspec->denom(k) - denom_before : dspec->denom(k);
      unproductive = denom == 0 || yield * 1000 < uint64_t(dspec->permille) * denom;
    } else {
      unproductive = ar != ApplyResult::Ok;
    }
    if (unproductive)
      e.delay_current += (e.delay_current < UINT32_MAX);
    else
      e.delay_current /= 2;
    e.delay_count = e.delay_current;
  }
  if (ar == ApplyResult::Ok)
    e.tm.ok++;
  if (ar == ApplyResult::Unsat || k.inconsistent) {
    ctx.phase_unsat = true;
    return SlotStep::Stop;
  }
  if (ar != ApplyResult::Ok)
    return SlotStep::Next;
  ctx.round_changed = true;
  if (propagate_apply(k) == ApplyResult::Unsat) {
    ctx.phase_unsat = true;
    return SlotStep::Stop;
  }
  return SlotStep::Next;
}

// One inprocessing phase on one clock (see SlotGroup / want_slot_phase). Only
// this group's entries are walked, so the other clock's telemetry is untouched.
int DefaultPolicy::run_slot_phase(Kernel &k, SlotGroup group,
                                  volatile sig_atomic_t *stop) {
  SlotClock &clock = group == SlotGroup::Probe ? probe_clock : elim_clock;
  // Snapshot this clock's gate marks before the pipeline runs (see SlotClock),
  // so material the phase produces re-admits the next one.
  clock.reductions_mark = k.stats.reductions;
  clock.marks_mark = k.stats.bve_marks;
  clock.fixed_mark = kernel_root_fixed(k);
  clock.blocked = false; // a new blocking episode may now be counted
  forward_pair_limit_ = 0; // the pair's shared allowance is per phase
  bve_ran_this_phase_ = false;
  // kissat reads the bound ONCE per factorization (`init_factoring`,
  // factor.c:72) and judges every grid in that round — composite chain links
  // included — against that one number. Snapshot it here so a dose cannot see
  // the level move underneath it.
  factor_bound_snapshot_ = couple_factor_bound_ ? bve_level_ : 0;
  // A simplification phase: descend to a quiescent root (a transient flush that
  // keeps saved/target phases), then dose each enabled technique in this group.
  // Route this phase's ticks away from `stats.search_ticks` for its duration:
  // an effort limit is a fraction of recent SEARCH work, so the phase's own
  // spend must not enter the reference (kissat gets the same split by calling a
  // different propagator; see `Kernel::inprocessing`).
  InprocessScope inprocess_scope(k);
  const uint64_t phase_props_at_entry = k.stats.propagations;
  // Wall clock for the whole slot, so `c timing` can separate rule time from
  // phase scaffolding. Diagnostic only.
  const uint64_t phase_nanos_at_entry = now_nanos();
  k.stats.inprocess_phases++;
  if (k.level > 0 && restart_guard(k))
    restart_apply_to(k, 0);
  // Attach the shared occurrence index for the phase, so BVE reads it instead
  // of rebuilding an O(arena) index per apply. Detached on every exit below.
  kernel_dense_connect(k, inprocess_occ);
  // Factor grid-selection policy for this phase: pure config, like k.bve_bound,
  // read only inside r.apply(k) and scoped so no pointer dangles after.
  // kissat: Factor accepts a grid iff `reduction > bound`, where `bound` IS
  // BVE's growth allowance (factor.c:72/939) — so BVA only introduces a
  // variable BVE would not immediately eliminate again. Max keeps the
  // TACTIX_FACTOR_BOUND / tactix_set_factor_bound surface authoritative.
  default_factor_match_.bound = std::max(k.factor_bound, factor_bound_snapshot_); // see DefaultFactorMatch
  k.factor_match = factor_match ? factor_match : &default_factor_match_;
  // Phase structure (kissat eliminate-round shape): a few outer rounds, each
  // giving every enabled technique one dose, so it can pick up what the others
  // exposed. Dose granularity follows the instance surface.
  SlotPhaseCtx ctx{group, stop};
  const std::vector<RuleVTable> &rules = tactix_rules();
  // Budgets: the dose is `permille` of the search work since this entry last
  // ran (kissat SET_EFFORT_LIMIT), floored so a technique with no search to
  // amortize still gets a usable first dose. Stop responsiveness (plan §6): the
  // slot polls `stop` at every entry boundary and inside the dosed loop, or a
  // long phase leaves the solver deaf to SIGTERM and a harness escalates to
  // SIGKILL, losing the proof file. One rule application is still
  // uninterruptible.
  uint64_t active_before = active_variables(k);
  for (ctx.outer = 0;
       ctx.outer < INPROCESS_PHASE_ROUNDS && !ctx.phase_unsat &&
       !(stop && *stop);
       ctx.outer++) {
    ctx.round_changed = false;
    // Reset here so the completeness wire below reads this round's verdict; a
    // declined entry keeps its last real round's flag, hence the true default.
    k.subsume_complete = true;
    k.strengthen_complete = true;
    for (SlotEntry &e : inprocess_slot) {
      if (e.group != group)
        continue; // the other clock's pipeline: not offered here at all
      if (stop && *stop)
        break; // §6: leave the phase promptly; cleanup runs at its exit
      const RuleVTable &r = rules[e.rule];
      slot_trace_entry(k, e, r, ctx.outer);
      e.tm.occasions++;
      if (!slot_entry_admissible(e, k, ctx))
        continue;
      // WHEN the ceiling is computed differs by phase, and kissat's two phases
      // genuinely differ. Probing recomputes each technique's limit on every
      // entry, so a technique called twice in one round takes two deltas
      // (probe.c:33-42 calls substitute and backbone twice each, and each call
      // runs SET_EFFORT_LIMIT). Elimination computes `resolution_limit` ONCE,
      // seven lines before its round loop (eliminate.c:428), and that single
      // budget spans every round — enforced as a round-loop break on
      // `eliminate_resolutions > resolution_limit` (eliminate.c:523).
      if (e.enabled && e.meter &&
          (group == SlotGroup::Probe || ctx.outer == 0))
        set_entry_limit(e, k);
      const SlotStep step = (e.dosed && r.instances && r.apply_instance)
                                ? run_dosed_entry(k, e, r, ctx)
                                : run_batch_entry(k, e, r, ctx);
      if (step == SlotStep::Stop)
        break;
    }
    // The subsumption completeness wire (kissat eliminate.c:519-520): a
    // half-checked candidate list buys another round instead of letting the
    // pipeline advance on material subsumption has not finished. It only adds
    // rounds, capped by INPROCESS_PHASE_ROUNDS — an admission that can decline
    // forever must never gate a clock. Predicted inert; `subsume_holds` says
    // so.
    const bool subsume_pending =
        group == SlotGroup::Eliminate && k.subsume_holds_elim &&
        (!k.subsume_complete || !k.strengthen_complete);
    if (!ctx.round_changed && subsume_pending)
      k.stats.subsume_holds++;
    // kissat's round-loop fixpoint is `before == solver->active` (probe.c:81):
    // the count of variables neither fixed, eliminated nor substituted. It is an
    // EQUALITY test, not a shrinkage test, because Factor ADDS variables
    // (factor.c:822) — so a round that only extends still counts as progress.
    // Note congruence and sweep do not retire variables themselves; the
    // `substitute` that follows each of them does, which is why the sequence
    // pairs them (probe.c:33-38). Without those pairings this test reads zero.
    const uint64_t active_now = active_variables(k);
    if (active_now == active_before && !subsume_pending)
      break; // outer fixpoint: the round retired and created nothing
    active_before = active_now;
  }
  // Reclaim what the dosed applications garbaged; canonical rounds reclaim
  // themselves. Not collecting is always sound, so this is a cost choice — see
  // docs/design/effort-scheduler-design.md §3.4.
  if (ctx.phase_dosed && !ctx.phase_unsat)
    kernel_collect_garbage(k);
  // Reference parity: redundant clauses mentioning a variable this phase
  // retired must die with it (cadical elim.cpp:736-756), or the variable keeps
  // propagating through learned clauses the dense index never showed BVE.
  if (!ctx.phase_unsat)
    kernel_flush_retired_redundant(k);
  // Rules record retirement as kernel facts and push an event; updating THIS
  // policy's ranking structures is this policy's job (§5 layering — rules
  // never call into policy code).
  if (k.retire_enabled)
    for (const Var v : k.retired_events)
      heuristics_deactivate_var(k, v);
  k.retired_events.clear();
  kernel_dense_disconnect(k);
  k.factor_match = nullptr; // scope the config to the phase (see entry)
  k.stats.inprocess_propagations +=
      k.stats.propagations - phase_props_at_entry;
  k.stats.inprocess_nanos += now_nanos() - phase_nanos_at_entry;
  // kissat `set_next_elimination_bound` (eliminate.c:339-372): once per
  // elimination phase, unconditionally when one ran.
  if (group == SlotGroup::Eliminate && bve_ran_this_phase_ && !ctx.phase_unsat)
    bve_level_ = bve_level_ ? std::min(bve_level_ * 2u, 8u) : 1u;
  // kissat's `last.ticks.probe` / `last.ticks.eliminate` (probe.c:86,
  // eliminate.c:601): stamp the phase's effort anchor on the way out, AFTER
  // `in_phase` is cleared so `search_ticks` reads the netted value, and
  // UNCONDITIONALLY — a phase in which every entry declined still consumed the
  // search that preceded it, so the next phase must price against this point.
  clock.anchor = search_ticks(k);
  if (ctx.phase_unsat)
    return 20;
  // The phase fired, simplifying or not: count it on this clock, whose
  // interval-growth key it is.
  clock.rounds++;
  update_slot_limit(k, group);
  return 0;
}

void DefaultPolicy::set_entry_limit(SlotEntry &e, Kernel &k) {
  // kissat `SET_EFFORT_LIMIT` (kimits.h:135-170), verbatim:
  //   REFERENCE = max(search_ticks - <phase anchor>, mineffort * 1e6)
  //   LIMIT     = <this technique's own counter, now> + effort/1000 * REFERENCE
  // The anchor is the PHASE's, not the entry's, so every technique in one phase
  // prices against the same reference and none can earn or lose allowance by
  // abstaining. There is no upper clamp and no active floor: kissat has
  // neither, and the clamps that used to sit here were cadical's.
  const std::vector<RuleVTable> &rules = tactix_rules();
  const SlotClock &clock =
      e.group == SlotGroup::Probe ? probe_clock : elim_clock;
  const uint64_t now = search_ticks(k);
  uint64_t reference = 0;
  if (now >= clock.anchor)
    reference = now - clock.anchor;
  else
    e.tm.wrapped++; // see Telem::wrapped
  if (reference < MIN_EFFORT_TICKS)
    reference = MIN_EFFORT_TICKS;
  const uint64_t delta = reference / 1000 * e.permille;
  e.limit = e.meter(k) + delta;
  // The forward-subsumption pair shares one allowance, because kissat's single
  // pass does. The first of the two to be priced in a phase fixes it; the
  // second adopts it rather than opening a second budget of the same size.
  if (e.meter == meter_forward_pair) {
    if (forward_pair_limit_)
      e.limit = forward_pair_limit_;
    else
      forward_pair_limit_ = e.limit;
  }
  // kissat gives factor's FIRST call an absolute allowance instead, because at
  // preprocessing time the reference is meaningless (factor.c:1106-1117).
  if (e.doses == 0 && e.budget_aware &&
      !std::strcmp(rules[e.rule].name, "Factor"))
    e.limit = e.meter(k) + factor_init_ticks_;
  // Congruence's opening round takes the same shape, `search_ticks` being
  // near-zero at conflicts == 0.
  if (e.doses == 0 && e.budget_aware &&
      !std::strcmp(rules[e.rule].name, "Congruence"))
    e.limit = e.meter(k) + congruence_init_ticks_;
}

void DefaultPolicy::report_slot(const Kernel &k) const {
  // One line per slot entry, diagnostic only, answering what `c stats` cannot:
  // was an entry starved (declined for budget) or idle (ran and found nothing)?
  // `c meters` is the reachability check — zero there means a budget is inert.
  std::printf("c meters      probe %llu backbone %llu vivify %llu "
              "instantiate %llu | global ticks %llu\n",
              (unsigned long long)k.stats.probe_ticks,
              (unsigned long long)k.stats.backbone_ticks,
              (unsigned long long)k.stats.vivify_ticks,
              (unsigned long long)k.stats.instantiate_ticks,
              (unsigned long long)k.stats.ticks);
  const std::vector<RuleVTable> &rules = tactix_rules();
  // Reachability readout for the authored order: the live sequence, named, so
  // an A/B arm cannot be banked without showing which order ran.
  std::printf("c slot-order  %s", slot_order_legacy_ ? "legacy  " : "authored");
  for (const SlotEntry &e : inprocess_slot)
    std::printf(" %s", rules[e.rule].name);
  std::printf("\n");
  // Reachability readout for the two clocks and their gates: `probe phases`
  // above `elim phases` is the split working, and `blocked` zero means an arm
  // resting on the gate proves nothing.
  std::printf("c slot-clock  probe phases %llu next %llu blocked %llu | "
              "eliminate phases %llu next %llu blocked %llu | bve-marks %llu\n",
              (unsigned long long)probe_clock.rounds,
              (unsigned long long)probe_clock.next_conflicts,
              (unsigned long long)probe_clock.blocked_episodes,
              (unsigned long long)elim_clock.rounds,
              (unsigned long long)elim_clock.next_conflicts,
              (unsigned long long)elim_clock.blocked_episodes,
              (unsigned long long)k.stats.bve_marks);
  // Reachability readout for the candidate-order arms and cursors: `rotated 0`
  // means the cursor is inert here, `moved 0` that a non-native arm was
  // untested.
  for (const SlotEntry &e : inprocess_slot) {
    if (!e.dosed)
      continue;
    std::printf("c cand-order  %-18s arm %-12s cursor %s | lists %llu "
                "candidates %llu rotated %llu advanced %llu ranked %llu "
                "moved %llu\n",
                rules[e.rule].name, candidate_order_name(e.order),
                e.cursor_enabled ? "on " : "off",
                (unsigned long long)e.ord.lists,
                (unsigned long long)e.ord.candidates,
                (unsigned long long)e.ord.rotated,
                (unsigned long long)e.ord.advanced,
                (unsigned long long)e.ord.ranked,
                (unsigned long long)e.ord.moved);
  }
  for (const SlotEntry &e : inprocess_slot) {
    const SlotEntry::Telem &t = e.tm;
    if (!t.occasions)
      continue;
    // Self-check: occasions must equal admitted + every decline reason, or a
    // `continue` was added without a counter. Printed, not asserted.
    const uint64_t declined =
        t.dec_disabled + t.dec_cap + t.dec_preprocess + t.dec_guard +
        t.dec_budget + t.dec_delay + t.dec_material + t.dec_warmup;
    const char *balance = (t.admitted + declined == t.occasions) ? "" : "  ***UNACCOUNTED***";
    std::printf(
        "c slot %-18s occ %6llu adm %5llu ok %5llu | dec dis %llu cap %llu "
        "pre %llu grd %llu bud %llu dly %llu mat %llu warm %llu "
        "| handed %llu spent %llu completed %llu | "
        "wrapped %llu%s\n",
        rules[e.rule].name, (unsigned long long)t.occasions,
        (unsigned long long)t.admitted, (unsigned long long)t.ok,
        (unsigned long long)t.dec_disabled, (unsigned long long)t.dec_cap,
        (unsigned long long)t.dec_preprocess, (unsigned long long)t.dec_guard,
        (unsigned long long)t.dec_budget, (unsigned long long)t.dec_delay,
        (unsigned long long)t.dec_material, (unsigned long long)t.dec_warmup,
        (unsigned long long)t.handed,
        (unsigned long long)t.spent, (unsigned long long)t.completed,
        (unsigned long long)t.wrapped, balance);
  }
}

int default_solve(Kernel &k, volatile sig_atomic_t *stop) {
  DefaultPolicy policy;
  const int res = policy.solve(k, stop);
  // Opt-in via TACTIX_SLOT_STATS: `default_solve` does not know main's `quiet`,
  // and competition stdout must stay exactly what the runner expects.
  if (std::getenv("TACTIX_SLOT_STATS"))
    policy.report_slot(k);
  return res;
}

} // namespace tactix
