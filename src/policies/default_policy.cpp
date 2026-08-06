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

// Restart trigger: fast/slow glue EMAs with kissat's bias-corrected
// warm-up (smooth.c) and per-mode persistence (averages.h averages[2]).
// The kissat constants over-fire on PLAIN seeded EMAs; this is the faithful
// package they presuppose. (Constants live once, in default_policy.hpp.)

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
    // Growth key = kissat's `(statistics->switched + 1) / 2` (mode.c:90).
    // `switched` and `limits.mode.count` are incremented at the SAME site
    // (mode.c:204-205) from the same zero, so they are equal here, and this
    // branch runs only when mode.count is EVEN — for even c, (c+1)/2 == c/2.
    // Getting this key one AHEAD of the reference makes every focused interval
    // too long and starves stable mode. max(1) keeps nlogpown's count > 0
    // precondition if the key is ever read at count 0.
    const uint64_t n = mode.count / 2 ? mode.count / 2 : 1;
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
  // opts.rephase=1; the stable gate applies only at opts.rephase==2). We
  // follow kissat and rephase stable-only.
  static const bool both = std::getenv("TACTIX_REPHASE_BOTH_MODES") != nullptr;
  return (both || k.h->stable) && k.stats.conflicts > rephase.next_conflicts;
}

// Reorder is a POLICY OP, not a proof rule (no proof delta), so the
// "every implemented rule ships ON" baseline does not apply to it.
// TACTIX_REORDER mirrors kissat's `reorder` option: 0 = off (OUR default),
// 1 = stable mode only, 2 = both modes (kissat's default). Ablating kissat's
// own reordering does not resolve above the Tier C noise floor, so there is no
// measured warrant for scheduling it by default.
// kissat additionally requires `!solver->level`; that is kept here in the
// SCHEDULE (where the coupled root restart also lives, as with Rephase) rather
// than in the rule's guard, which stays a pure precondition (N1).
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

// kissat kimits.h UPDATE_CONFLICT_LIMIT(reorder, reordered, LINEAR, false):
// interval = reorderint · (number of reorders so far)
void DefaultPolicy::update_reorder_limit(const Kernel &k) {
  reorder.next_conflicts =
      k.stats.conflicts + REORDER_INTERVAL * reorder.count;
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

// --- effort ledger (docs/design/effort-scheduler-design.md) -----------------
// Each inprocessing rule is metered in its OWN currency, because one unit of
// work is not comparable across rules (a BVE resolvent, a subsumption check and
// a propagation tick cost different things). These readers are the rules' own
// work counters; kissat's statistics.eliminate_resolutions / forward_steps /
// probing_ticks are the same idea.
namespace {

// Scope the kernel's tick sink to ONE rule's apply. RAII because the apply
// paths return early — a leaked sink would silently charge the NEXT rule's
// propagation to this one, which is exactly the misattribution the private
// per-rule meters exist to remove.
struct TickSinkScope {
  Kernel &k;
  TickSinkScope(Kernel &kk, uint64_t Stats::*ctr) : k(kk) {
    k.tick_sink = ctr ? &(k.stats.*ctr) : nullptr;
  }
  ~TickSinkScope() { k.tick_sink = nullptr; }
  TickSinkScope(const TickSinkScope &) = delete;
  TickSinkScope &operator=(const TickSinkScope &) = delete;
};

// PRIVATE per-rule meters: each reads only its own charges, accumulated
// through `Kernel::tick_sink` while the policy has it scoped around that
// rule's apply. Reading the global `k.stats.ticks` instead lets an earlier
// slot entry spend the budget a later one was handed, and makes a starved
// entry indistinguishable from an idle one.
uint64_t meter_probe(const Kernel &k) { return k.stats.probe_ticks; }
uint64_t meter_backbone(const Kernel &k) { return k.stats.backbone_ticks; }
uint64_t meter_vivify(const Kernel &k) { return k.stats.vivify_ticks; }
uint64_t meter_instantiate(const Kernel &k) { return k.stats.instantiate_ticks; }
// The GLOBAL counter still exists and is still the right reference for
// SEARCH-relative windows (`search_ticks`, the mode limits) — it is only wrong
// as a per-rule SPEND meter.
uint64_t meter_ticks(const Kernel &k) { return k.stats.ticks; }
uint64_t meter_bve(const Kernel &k) { return k.stats.bve_resolutions; }
// Strengthen/SubsumeForward meter PARTNER VISITS, not candidates. cadical
// budgets subsumption in `stats.subchecks` (subsume.cpp:86,362,459) — one per
// partner compared — because the cost of "checking a clause" is its occurrence
// lists' length, not 1. Metering candidates under-prices precisely the
// expensive ones.
uint64_t meter_strengthen(const Kernel &k) { return k.stats.strengthen_visits; }
uint64_t meter_forward(const Kernel &k) { return k.stats.forward_visits; }
// TransitiveReduce's currency is TICKS (kissat transitive.c:320,
// `SET_EFFORT_LIMIT(limit, transitive, transitive_ticks)`). Metering CANDIDATES
// is a unit mismatch, not merely a generous one: one "checked" is a whole BFS
// walk, so the per-mille ledger cannot bind at all — a twentyfold rate cut on
// the candidate meter comes out bit-identical. Switching the meter alone is not
// enough either: without `Kernel::transitive_cursor` a truncated round replays
// its head next time and the tail is never reduced, so the honest budget buys
// starvation. TACTIX_V4_METER=checked restores the old currency for an A/B.
uint64_t meter_transitive(const Kernel &k) {
  static const bool checked = [] {
    const char *e = std::getenv("TACTIX_V4_METER");
    return e && !std::strcmp(e, "checked");
  }();
  return checked ? k.stats.transitive_checked : k.stats.transitive_ticks;
}
uint64_t meter_factor(const Kernel &k) { return k.stats.factor_ticks; }
uint64_t meter_ternary(const Kernel &k) { return k.stats.ternary_ticks; }
uint64_t meter_bce(const Kernel &k) { return k.stats.bce_ticks; }
uint64_t meter_sweep(const Kernel &k) { return k.stats.sweep_ticks; }

// name -> {effort per mille, currency, dosed}. Fractions are kissat's
// options.h defaults (eliminateeffort/forwardeffort/vivifyeffort 100,
// backboneeffort/transitiveeffort 20) unless a comment says otherwise.
// `dosed` marks the rules whose instance payloads survive formula mutation
// (literals and variables — immune to arena GC, cheaply re-validated), so the
// policy can enumerate once and apply candidates one at a time up to the
// budget. The Ref-payload rules (Vivify/Strengthen/SubsumeForward) cannot: the
// GC inside their first successful application relocates every clause, so a
// snapshot of Refs goes stale — they run as canonical rounds and are metered
// after the fact, exactly as kissat meters `substitute`.
// Congruence/Factor/Substitute expose only the canonical instance and have no
// meter at all.
// `retry_advice` marks a rule that maintains a per-variable "worth re-attempting"
// bit the POLICY consults (only BVE: Kernel::bve_retry). The rule deliberately
// does not filter its own instance enumeration with it — that would hide legal
// applications from RandomPolicy and from the phase-6 learned policy, whose
// action space IS the instance surface.
// `budget_aware` marks a BATCH rule whose canonical apply reads
// Kernel::work_budget (in its own currency) and stops mid-round — the policy
// hands such an entry its remaining ledger budget around r.apply(k) instead
// of only metering it after the fact (kissat SET_EFFORT_LIMIT).
struct EffortSpec {
  const char *name;
  uint32_t permille;
  uint64_t (*meter)(const Kernel &);
  bool dosed;
  bool retry_advice;
  bool budget_aware;
  // The private tick counter the policy scopes around this rule's apply, for
  // the entries whose currency is kernel propagation ticks. Null where the rule
  // charges its own counter (Factor, BCE, HyperTernaryRes, TransitiveReduce) or
  // where the currency is not ticks at all (BVE, Strengthen, SubsumeForward).
  uint64_t Stats::*tick_counter = nullptr;
  uint64_t thresh = 0; // cadical's NAMEthresh; see SlotEntry::thresh
};
// INVARIANT: merely registering a rule at RuleTier::Inprocess puts it in the
// slot, so a rule that can be metered needs an entry here — without one it runs
// with no meter and no budget on every inprocessing phase. The only deliberate
// absentees are the un-titratable closures (Congruence, Substitute: one
// canonical instance, nothing to dose), which the new-material watermark in the
// slot loop gates instead.
constexpr EffortSpec EFFORT_SPECS[] = {
    {"BVE", 100, meter_bve, true, true, false},
    {"Probe", 20, meter_probe, true, false, false, &Stats::probe_ticks},
    {"Backbone", 20, meter_backbone, true, false, false, &Stats::backbone_ticks},
    {"TransitiveReduce", 20, meter_transitive, true, false, false},
    {"Vivify", 100, meter_vivify, false, false, true, &Stats::vivify_ticks, 20},
    // Strengthen/SubsumeForward must be budget_aware: with dosed=false AND
    // budget_aware=false the per-mille bounds only how OFTEN a round starts,
    // never how much one round does, and a single round can then own most of a
    // solve. Both poll Kernel::work_budget in their own currency and stop.
    {"Strengthen", 100, meter_strengthen, false, false, true},
    {"SubsumeForward", 100, meter_forward, false, false, true},
    // kissat factoreffort 50 per mille (options.h:57 — half of
    // vivify/eliminate's 100), currency = factor_ticks. The first dose per
    // solve bypasses the permille formula with kissat's factoriniticks
    // allowance (see set_entry_limit).
    {"Factor", 50, meter_factor, false, false, true, nullptr, 7},
    // HyperTernaryRes: 8 per mille = cadical's `ternaryeffort`
    // (options.hpp:240), NOT the cheap-probing-family 20. It is the one rule
    // with no internal governor — its size limits (ternaryocclim /
    // ternarymaxadd) are inert at cadical's defaults, ~38x too loose to bind —
    // so this ledger share IS its bound.
    // BATCH + budget_aware, NOT per-instance `dosed`: it builds a round-local
    // occurrence index, so dosing one instance at a time would rebuild that
    // index per application. It reads Kernel::work_budget and stops mid-round
    // instead (Vivify's arrangement). Running it undosed is not a composition
    // either reference names — cadical runs `ternary` under a steps budget
    // inside its preprocessing rounds.
    {"HyperTernaryRes", 8, meter_ternary, false, false, true, nullptr, 6},
    // BCE: no reference schedule backs any of its constants — kissat 4.0.4 has
    // no blocked-clause elimination and cadical ships `block`=0, so 20 per
    // mille and the slot position are ours, not ported.
    {"BCE", 20, meter_bce, false, false, true},
    // Instantiate's probes ARE propagation, so the currency is ticks (Vivify's)
    // and the rate is the cheap-probing-family 20 (Probe/Backbone/
    // TransitiveReduce). BATCH + budget_aware because the round builds one
    // occurrence-count pass up front.
    {"Instantiate", 20, meter_instantiate, false, false, true, &Stats::instantiate_ticks},
    // kissat's `sweepeffort` is 100 per mille (options.h:143), the same share
    // as vivify/eliminate. Currency is the rule's own `sweep_ticks`. BATCH +
    // budget_aware: a round builds one environment per seed, so dosing it per
    // instance would rebuild that environment every time.
    {"Sweep", 100, meter_sweep, false, false, true},
};

const EffortSpec *effort_spec(const char *name) {
  for (const EffortSpec &s : EFFORT_SPECS)
    if (!std::strcmp(s.name, name))
      return &s;
  return nullptr; // all-or-nothing rule: no titration, no meter
}

// --- the authored inprocessing phase order (see SlotOrderRow) ---------------
// Merged from the references' argued pipelines, which agree wherever both own
// the technique:
//   kissat  probe.c:32-41    congruence, substitute, backbone, vivify, sweep,
//                            substitute, transitive_reduction, backbone, factor
//           eliminate.c:441  forward subsumption, then BVE, once per round
//   cadical probe.cpp:934-952  decompose (ELS), ternary, probe, extract_gates,
//                            backbone, sweep, vivify, transred, factor
//           elim.cpp:1044/1073/1090  subsume, then BVE rounds (instantiate sits
//                            inside the round at :947), then block (BCE)
// The invariants both references fix, each of which name-sorting broke:
//   1. ELS (Substitute) precedes every reader of the binary implication graph —
//      Backbone, Probe, TransitiveReduce, Factor.
//   2. Congruence feeds Substitute, so the two are adjacent and open the pass.
//   3. Vivify precedes TransitiveReduce.
//   4. Factor closes the probing pipeline.
//   5. Subsumption precedes elimination.
//   6. BCE closes the elimination pipeline (cadical block() runs after
//      elim_round); name-sorting ran it first of all.
// Where the references disagree — kissat vivifies then sweeps, cadical sweeps
// then vivifies — we take kissat's, so Sweep's environments are built over an
// already vivified database.
//
// The `group` column is the same partition, and it is the references' own:
// every PROBE row is called from kissat's `probe()` / cadical's `inprobe()`,
// every ELIMINATE row from kissat's `eliminate()` / cadical's `elim()`. Where
// only one reference owns a technique, that reference decides —
// HyperTernaryRes and Probe are cadical inprobe members (kissat has neither as
// a separate technique), Instantiate and BCE are cadical elim members
// (elim.cpp:947 inside the round, block() after it). BCE is the one row no
// reference schedules by default (kissat has no BCE, cadical ships block=0);
// cadical's placement inside `elim` is still the only evidence there is.
constexpr SlotOrderRow AUTHORED_SLOT_ORDER[] = {
    // probing pipeline (kissat probe.c:32-41, cadical probe.cpp:934-952)
    {"Congruence", SlotGroup::Probe},
    {"Substitute", SlotGroup::Probe},
    {"HyperTernaryRes", SlotGroup::Probe},
    {"Probe", SlotGroup::Probe},
    {"Backbone", SlotGroup::Probe},
    {"Vivify", SlotGroup::Probe},
    {"Sweep", SlotGroup::Probe},
    {"TransitiveReduce", SlotGroup::Probe},
    {"Factor", SlotGroup::Probe},
    // elimination pipeline (kissat eliminate.c:441, cadical elim.cpp:1044-1090)
    {"SubsumeForward", SlotGroup::Eliminate},
    {"Strengthen", SlotGroup::Eliminate},
    {"BVE", SlotGroup::Eliminate},
    {"Instantiate", SlotGroup::Eliminate},
    {"BCE", SlotGroup::Eliminate},
};

} // namespace

const SlotOrderRow *default_slot_order(size_t *count) {
  if (count)
    *count = sizeof(AUTHORED_SLOT_ORDER) / sizeof(AUTHORED_SLOT_ORDER[0]);
  return AUTHORED_SLOT_ORDER;
}

// --- candidate ORDER: the arms and the ranker (SCHED-E1/E2) -----------------

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

// The key a resumption cursor is expressed in: the payload's literal for the
// shared lit_instance encoding, the raw payload otherwise. Every DOSED rule
// uses the literal encoding today; the fallback keeps the cursor total rather
// than making it a precondition on the payload shape.
uint64_t instance_key(uint64_t inst) {
  Lit l;
  return lit_of_instance(inst, &l) ? static_cast<uint64_t>(l) : inst;
}

// OCCURRENCES of `l`, counted as watch ENTRIES (binary = 1 word, large = 2), so
// the arms key on how many clauses a candidate touches rather than on how many
// words its watch list happens to occupy.
uint32_t lit_occurrences(const Kernel &k, Lit l, bool binary_only) {
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
  const uint32_t sz = k.watches.heads[l].size;
  uint32_t n = 0;
  for (uint32_t i = 0; i < sz;) {
    if (Watch{ws[i]}.binary()) {
      n++;
      i += 1;
    } else {
      n += binary_only ? 0u : 1u;
      i += 2;
    }
  }
  return n;
}

// Sort key for the comparison arms; ASCENDING throughout (a descending arm
// returns the complement, so a stable sort still breaks ties by native
// position).
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
    // cadical probe.cpp:624/685/763: rank = noccs(-p) over BINARY clauses, and
    // probes are popped from the BACK of the sorted stack, so the literal that
    // occurs negated most often is probed FIRST.
    return ~static_cast<uint64_t>(lit_occurrences(k, negate(l), true));
  case CandidateOrder::Heuristic:
    // kissat transitive.c:222-260: stable mode sorts on the EVSIDS heap score,
    // focused mode on the VMTF stamp, both ASCENDING and both walked front to
    // back.
    if (k.h->stable) {
      // An EVSIDS score is never negative, and non-negative IEEE-754 doubles
      // compare as unsigned integers on their bit patterns.
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
    // splitmix64 Fisher-Yates. The generator is policy state seeded by a
    // constant, so the whole solve replays: a random order that is not
    // reproducible cannot be fuzzed or bisected.
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
  // H is OPTIONAL policy state (Track H): with none attached there is nothing
  // to rank by, and falling back to the native order is the only honest move.
  if (arm == CandidateOrder::Heuristic && !k.h)
    return;
  // Persistent scratch: this runs inside the dose loop, and a per-call
  // linear allocation there is a measured hazard.
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

// Resume, then rank, the instance list this occasion will walk.
//
// RESUME first, because the cursor is expressed in the RULE's own key space and
// is therefore only meaningful against the list as the rule enumerated it. A
// ranking arm then permutes whatever it is handed — so under a score arm the
// rotation survives only as a tie-break and the arm re-imposes its own head
// every occasion. That composition is deliberate and it is also the
// references': kissat's transitive sort puts the untried MARK ahead of the
// score (transitive.c:226-232) rather than rotating a scored list. Combining a
// per-item mark with a score is Stage F (ledger SP-REPEAT-*), not this item.
//
// Both steps are permutations. Nothing here can remove a candidate, so nothing
// here narrows what DefaultPolicy may apply (N1).
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
  // The built-in ranker is a no-op until SCHED-E2 wires the arms, so skip the
  // whole step — including its before-image copy — unless a policy has been
  // swapped in that can actually reorder.
  if (!candidate_order)
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
  // The interval is NECESSARY BUT NOT SUFFICIENT (see SlotClock's gate block).
  // The FIRST phase of each clock is exempt from both gates — it is the
  // preprocessing pass, there is no previous phase to be new relative to, and
  // both references exempt it too (cadical's `stats.inprobingphases &&`;
  // kissat's watermark starts at 0 while every variable starts marked).
  const bool gated =
      c.rounds != 0 &&
      (g == SlotGroup::Probe
           // cadical probe.cpp:23 — no inprobe phase unless a reduce has
           // happened since the last one.
           //
           // MEASURED INERT HERE, and provably so: our probe interval is
           // ~2500·n·log10(n+9) and our reduce interval 2000+300·r, so matching
           // cumulative conflicts gives r ≈ 2.9·n·√log n and a probe interval
           // ~2.9·√log n times the reduce interval — always > 1, so two probe
           // phases never fall inside one reduce interval. Kept because it is
           // the reference's gate and costs one compare: it becomes live the
           // moment the probing clock is given cadical's near-constant spacing
           // (probe.cpp:980), which is exactly where cadical needs it.
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

// Each clock's interval, on its own reference formula (see SlotGroup).
//   probe:     kissat kimits.h UPDATE_CONFLICT_LIMIT(probe, probings, NLOGN,
//              true) = probeint · n·log10(n+9), size-scaled.
//   eliminate: UPDATE_CONFLICT_LIMIT(eliminate, eliminations, NLOG2N, true)
//              = eliminateint · n·log10(n+9)², size-scaled.
// Both keys are the phase count of THAT clock, so the two schedules separate
// on their own and the ratio widens as the solve goes on — the reference
// behaviour SP-SLOT-1 records and one clock cannot express.
//
// The formulas are kissat's rather than cadical's on purpose: cadical's probe
// reschedule is essentially CONSTANT spacing (probe.cpp:980,
// 25·inprobeint·log10(phases+9) ≈ 2385 conflicts at phase 0 and ≈ 2500 at
// phase 1), which its own comment at probe.cpp:889-891 misdescribes as a sqrt
// schedule. Our whole slot is already denominated in kissat's size-scaled
// growing intervals, and mixing a constant-spacing probing clock into that
// would change two things at once.
void DefaultPolicy::update_slot_limit(const Kernel &k, SlotGroup g) {
  SlotClock &c = g == SlotGroup::Probe ? probe_clock : elim_clock;
  uint64_t n = c.rounds; // policy-local growth key
  if (!n)
    n = 1;
  const double lg = std::log10(static_cast<double>(n) + 9);
  const double base = g == SlotGroup::Probe
                          ? PROBE_INTERVAL * static_cast<double>(n) * lg
                          : SUBSUME_INTERVAL * static_cast<double>(n) * lg * lg;
  c.next_conflicts = k.stats.conflicts +
                     scale_inprocessing_delta(k, static_cast<uint64_t>(base));
}

// reduce cadence. Default: arithmetically growing interval.
// TACTIX_REDUCE_SQRT selects kissat's schedule instead — reduceint·√reductions
// (kimits.h UPDATE_CONFLICT_LIMIT(reduce, reductions, SQRT, false),
// reduceint = 1000). The two diverge fast: at 100 reductions kissat's next
// interval is 10 k conflicts and ours is 32 k, so our steady-state redundant
// database is several times larger and its `used` bits age several times
// slower — a candidate mechanism for our standing propagations/conflict gap.
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

// The shape of kissat/src/search.c's guard->apply chain: propagate,
// analyze+backjump on conflict, SAT when everything is assigned, else
// decide — with Reduce/SwitchMode/Restart/Rephase/inprocessing scheduled
// between conflicts (in kissat search.c's check order) by THIS policy's
// state. Rules carry no scheduling of their own.
int DefaultPolicy::solve(Kernel &k, volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  if (const char *cl = std::getenv("TACTIX_CHRONO_LEVELS")) {
    const long n = std::strtol(cl, nullptr, 10);
    if (n >= 0)
      chrono_levels = static_cast<uint32_t>(n);
  }
  if (k.inconsistent)
    return 20;
  // DefaultPolicy is a GUIDED policy — it reads `H` directly. The kernel does
  // not own `H`: if the caller (the session — API/main) attached one, use it
  // (so incremental state + the stepping surface persist); otherwise bring our
  // OWN for this solve, sized to the parsed formula, so that a bare
  // `default_solve(k)` works. This is the "policy owns its guidance" model
  // (plan §5). Sizing a fresh `H` to `k.nvars` here is bit-identical to having
  // had `H` since parse (same index-order VMTF queue, zero scores, positive
  // saved phases).
  std::optional<ScopedHeuristics> owned_h;
  if (!k.h)
    owned_h.emplace(k);
  // Per-SOLVE scheduling state: a reused policy object must not carry the
  // previous solve's first-dose consumption or factordelay decades into a new
  // formula.
  bve_rounds_run_ = 0;
  bve_level_ = 0;
  for (SlotEntry &e : inprocess_slot)
    e.doses = 0;
  // A/B knob: TACTIX_FORCE_MODE=focused|stable (0 = off; see the
  // SwitchMode decline below). Read once — the check sits on the CDCL
  // outer loop.
  force_mode_ = 0;
  if (const char *fm = std::getenv("TACTIX_FORCE_MODE")) {
    if (!std::strcmp(fm, "focused"))
      force_mode_ = 1;
    else if (!std::strcmp(fm, "stable"))
      force_mode_ = 2;
  }
  // Closure delay ladder, DEFAULT ON: a kissat-style delay ladder on
  // UNPRODUCTIVE unmetered closures (Congruence/Substitute), which otherwise
  // re-run their full closure every outer round for zero yield, because
  // ordinary learned binaries re-arm the new-material marks. Yield signal =
  // the registry contract itself (Ok vs NoOp). A delay, never a disable — no
  // implemented rule ships off. TACTIX_NO_CLOSURE_DELAY reverts for A/Bs.
  closure_delay_ = std::getenv("TACTIX_NO_CLOSURE_DELAY") == nullptr;
  // Reason-side bumping, DEFAULT ON, and it ships TOGETHER with the dynamic
  // tiers below because the pair is ONE mechanism: kissat's retention bands
  // are quantiles of the glue distribution of the clauses the search actually
  // uses, and reason-side bumping changes which clauses those are. Measured
  // accordingly — the pair wins while EITHER ALONE LOSES, so do not enable or
  // ablate one of them on its own. Both references ship them together.
  // TACTIX_NO_BUMP_REASONS / TACTIX_NO_DYNAMIC_TIERS revert independently.
  // The two sweep knobs exist because the references disagree 10x on the rate.
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
  bump_reasons_delay_ = {};
  // Reduce-cadence arm: kissat's √ interval + ramped fraction (reduceinit
  // 1000 is its first limit, against our 2000). The CADENCE and the RAMP are
  // separately settable on purpose — with one knob for both, a win cannot be
  // attributed to the schedule, the deletion ramp, or one paying for the
  // other. kissat couples them by design ("a frequent reducer must delete less
  // each time"), which may well be right, but that is a hypothesis to test.
  // TACTIX_REDUCE_SQRT is retained as the COMPOUND alias so numbers already
  // banked against it still reproduce.
  const bool both = std::getenv("TACTIX_REDUCE_SQRT") != nullptr;
  const char *cad = std::getenv("TACTIX_REDUCE_CADENCE"); // "sqrt" | "arith"
  const char *rmp = std::getenv("TACTIX_REDUCE_RAMP");    // "1" | "0"
  reduce_sqrt_ = cad ? !std::strcmp(cad, "sqrt") : both;
  default_reduction_.ramp = rmp ? (rmp[0] == '1') : both;
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
      // The composition (kissat's, effort-scheduler design §4.4): EVERY
      // Inprocess-tier rule is enabled, the probing family included, and BVE
      // is the only entry deferred out of the conflicts==0 preprocessing
      // phase (kissat fastel=0) so its doses land on a formula probing has
      // already simplified. Slot ORDER is the authored list
      // (AUTHORED_SLOT_ORDER); membership is still derived from the registry,
      // so a newly registered rule joins without editing this function.
      const std::vector<RuleVTable> &rules = tactix_rules();
      // TACTIX_NO_BVE / TACTIX_NO_FACTOR: the two named ablation knobs, kept
      // because each rule has a standing open investigation (BVE's bimodal
      // per-instance behaviour; Factor's effect on proof-checking cost).
      // Disabling a rule is always sound; unset, the slot is byte-identical
      // to the default composition.
      const bool bve_off = std::getenv("TACTIX_NO_BVE") != nullptr;
      const bool factor_off = std::getenv("TACTIX_NO_FACTOR") != nullptr;
      // TACTIX_SLOT_ENABLE / TACTIX_SLOT_DISABLE: comma-separated rule names
      // added to / removed from the default composition. Disabling a rule is
      // always sound; enabling one poses the same composition the phase-6
      // arm surface will pose via the API. Unset ⇒ byte-identical default.
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
          // BASELINE: every implemented proof rule ships scheduled. A rule-off
          // arm is a diagnostic for a SCHEDULING defect, never a shipped
          // default — so nothing here may default a rule to off. The two named
          // ablation knobs + the slot-surgery lists are the whole A/B surface
          // (the same overrides presets and the phase-6 learner use).
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
          // Which clock offers this entry its occasions. Read from the authored
          // table by NAME, so it is independent of the order arm below: the
          // legacy-order arm still runs the two clocks, which is what makes D1
          // and D2 separately priceable. An unauthored rule keeps SlotEntry's
          // default (the probing clock); test_scheduling fails the build before
          // that can happen silently.
          {
            size_t norder = 0;
            const SlotOrderRow *order = default_slot_order(&norder);
            for (size_t o = 0; o < norder; o++)
              if (!std::strcmp(order[o].name, nm)) {
                e.group = order[o].group;
                break;
              }
          }
          if (const EffortSpec *s = effort_spec(nm)) {
            e.permille = s->permille;
            e.meter = s->meter;
            e.tick_counter = s->tick_counter;
            e.thresh = s->thresh;
            e.dosed = s->dosed;
            e.retry_advice = s->retry_advice;
            e.budget_aware = s->budget_aware;
            // TACTIX_SLOT_PERMILLE=Name:rate[,Name:rate...] (DIAGNOSTIC sweep
            // knob, the shape TACTIX_WALK_PERMILLE has): override a slot
            // entry's effort rate without a rebuild. "How hard to run rule X"
            // is a plan §5.6 dosing question the phase-6 learner is meant to
            // subsume; sweeping one needs the whole tier in ONE window, which
            // is only affordable as an env arm.
            if (const char *pm = std::getenv("TACTIX_SLOT_PERMILLE")) {
              const size_t n = std::strlen(nm);
              for (const char *p = pm; (p = std::strstr(p, nm)); p += n)
                if ((p == pm || p[-1] == ',') && p[n] == ':') {
                  e.permille = static_cast<uint32_t>(strtoul(p + n + 1, nullptr, 10));
                  break;
                }
            }
          }
          // TACTIX_SLOT_MAX_APPLIES=Name:N (DIAGNOSTIC, same shape): cap how
          // many phases may schedule this entry. Parsed for EVERY entry, not
          // just those with an EffortSpec, because the cap is about rounds and
          // an unmetered entry has rounds too.
          if (const char *mx = std::getenv("TACTIX_SLOT_MAX_APPLIES")) {
            const size_t n = std::strlen(nm);
            for (const char *p = mx; (p = std::strstr(p, nm)); p += n)
              if ((p == mx || p[-1] == ',') && p[n] == ':') {
                e.max_applies =
                    static_cast<uint32_t>(strtoul(p + n + 1, nullptr, 10));
                break;
              }
          }
          // --- candidate order + resumption cursor (SCHED-E1/E2) ------------
          // Cursor default: ON for the DOSED entries whose enumeration is in
          // KEY order, OFF for BVE (see SlotEntry::cursor for both halves of
          // the argument). Order arm default: Native everywhere, and it stays
          // there until an A/B moves it.
          if (e.dosed)
            e.cursor_enabled = std::strcmp(nm, "BVE") != 0;
          // One knob PER RULE, spelled from the registry name so a newly dosed
          // rule gets its own without editing a list here:
          // TACTIX_CURSOR_<RULENAME> / TACTIX_ORDER_<RULENAME>, upper-cased.
          // Priced separately is the point — an ordering bundle cannot be
          // attributed to a rule.
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
          // A knob that CANNOT take effect aborts rather than being ignored:
          // an arm silently set on an entry the policy never orders is exactly
          // the vacuous-A/B failure mode.
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
          inprocess_slot.push_back(e);
        }
      };
      // PHASE ORDER. Default: the authored list, walked in order; any
      // Inprocess-tier rule it does not name is appended in registry order so a
      // newly registered rule is scheduled rather than silently dropped (give
      // it an authored position — the append is a safety net, not the design).
      //
      // TACTIX_SLOT_ORDER=legacy restores the pre-authored order — registry
      // (strcmp on rule names) order with BVE and then Factor moved to the
      // back. It is the A/B arm for the authored order and nothing else.
      const char *order_env = std::getenv("TACTIX_SLOT_ORDER");
      slot_order_legacy_ = order_env && !std::strcmp(order_env, "legacy");
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
        for (size_t o = 0; o < norder; o++)
          for (int i = 0; i < static_cast<int>(rules.size()); i++)
            if (rules[i].tier == RuleTier::Inprocess && !placed[i] &&
                !std::strcmp(rules[i].name, order[o].name)) {
              placed[i] = true;
              add_entry(i);
              break;
            }
        for (int i = 0; i < static_cast<int>(rules.size()); i++)
          if (rules[i].tier == RuleTier::Inprocess && !placed[i])
            add_entry(i);
      }
      // The built-in ranker holds one arm per registry rule — the shape the
      // §5.6 learner needs. Every arm is Native until SCHED-E2 wires the
      // per-rule knobs that select them.
      default_candidate_order_.arms.assign(rules.size(),
                                           CandidateOrder::Native);
    }
    if (!inprocess.initialized) {
      inprocess.initialized = true;
      // Preprocess: BOTH clocks fire at conflicts=0, before any search state
      // exists, so simplification never perturbs an in-progress SAT search (the
      // SAT-safety finding). The probing phase runs first and the elimination
      // phase immediately after it, which is the reference arrangement —
      // kissat probe_initially then eliminate, cadical inprobe then elim.
      probe_clock.next_conflicts = k.stats.conflicts;
      elim_clock.next_conflicts = k.stats.conflicts;
    }
  }
  bool lucky_tried = false;
  for (;;) {
    const ApplyResult r = propagate_apply(k);
    if (r == ApplyResult::Unsat)
      return 20;
    // Lucky: once, at search start after the initial root fixpoint
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
      // THE CONFLICT WAS REUSED AS ITS OWN DRIVING CLAUSE: analyze resolved it
      // in place (backtracked, assigned the forced literal with the conflict as
      // its reason) and learned NOTHING, so there is no pending record — no
      // glue sample, no clause to assert, no backjump target. Every line below
      // reads `k.pending`, whose fields still describe the PREVIOUS conflict,
      // so skipping this check runs the whole learn/backjump tail on a stale
      // record: backjump to a stale `jump_level`, undoing the reuse's own
      // assignment, then a `backjump_stale` on the already-assigned asserting
      // literal. kissat does the same thing structurally —
      // one_literal_on_conflict_level returning true makes kissat_analyze
      // return before kissat_learn_clause, so its glue sample, tier update and
      // restart tick are all skipped too (analyze.c:545 vs :567).
      if (!k.pending.valid) // `stats.conflict_reused` already counted it
        continue;
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
    // SAT when every ACTIVE variable is assigned. A retired variable occurs in
    // no clause, so every clause is already satisfied; ρ supplies its value in
    // kernel_extend_model. This is a POLICY decision, not a rule guard —
    // `decide_guard` is untouched and an external policy may still decide a
    // retired variable (N1). kissat does the same thing by decrementing its own
    // `unassigned` at deactivation (flags.c).
    if (k.retire_enabled ? k.unassigned_active == 0 : k.unassigned == 0)
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
    // TACTIX_FORCE_MODE (force_mode_, read once at solve entry): pin the search
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
    // kissat search.c order: reduce, switch, restart, REORDER, rephase, probe,
    // eliminate. Off unless TACTIX_REORDER is set (want_reorder).
    if (want_reorder(k) && reorder_guard(k)) {
      reorder_apply(k);
      reorder.count++;
      update_reorder_limit(k);
      continue;
    }
    if (want_rephase(k) && rephase_guard(k)) {
      // kissat couples a backtrack-to-root into kissat_rephase; here the
      // same effect is composed from the rule vocabulary (deviation
      // recorded in rephase.cpp) — the root restart IS a restart
      if (restart_guard(k))
        restart_apply_to(k, 0);
      // DEVIATION: our rotation is B,B+W,I,B,B+W,O — kissat rotates
      // B,W,I,B,W,O, i.e. Walk REPLACES Best at two slots. Replacing them
      // measured far worse than the walk gains, and the DRY-walk control
      // (TACTIX_WALK_DRY: walk runs, result discarded) isolates the cause to
      // the LOST BEST CONSUMPTION, not to the phases the walk writes. Our
      // `best` is load-bearing where kissat's is not, because kissat saves
      // phases at ASSIGN time (every propagation) while we save at unassign —
      // so kissat's saved phases already carry what our Best slot has to
      // restore. Walk therefore AUGMENTS Best here: consume best, then let
      // local search improve the result before it becomes target.
      static constexpr uint64_t WALK_SLOT = UINT64_MAX;
      static constexpr uint64_t ROTATION[6] = {
          REPHASE_BEST, WALK_SLOT, REPHASE_INVERTED,
          REPHASE_BEST, WALK_SLOT, REPHASE_ORIGINAL};
      const uint64_t type = ROTATION[rephase.count % 6];
      rephase.count++; // ONE rephase event, whatever it is composed of
      if (type == WALK_SLOT && walk_guard(k)) {
        rephase_apply_type(k, REPHASE_BEST); // the slot keeps its Best
        // Effort budget in Walk's own currency, denominated in SEARCH work
        // the way every other budget is (kissat SET_EFFORT_LIMIT(walk),
        // walkeffort = 50 per mille, floored at mineffort). DEVIATION: the
        // reference window is the search work since the LAST WALK; kissat
        // anchors on its last elimination instead (SET_EFFORT_LIMIT reads
        // last.ticks.eliminate), which measured worse here.
        uint64_t reference = search_ticks(k) - walk_anchor;
        if (reference < MIN_EFFORT_TICKS)
          reference = MIN_EFFORT_TICKS;
        // TACTIX_WALK_PERMILLE (DIAGNOSTIC sweep knob): the dose magnitude.
        // See WALK_PERMILLE for why the per-mille is not kissat's — the
        // budget is scaled from cache-line ticks but spent in per-entry walk
        // steps, so the reference rate does not transfer.
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
    // TWO CLOCKS (SlotGroup): the probing pipeline and the elimination
    // pipeline are scheduled independently, probing checked first — kissat
    // search.c:214-217 (kissat_probing then kissat_eliminating), cadical
    // internal.cpp:322-325 (inprobing then ineliminating).
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
// TACTIX_SLOT_TRACE: per-entry scheduling visibility (debug aid —
// "did rule X even get considered, and what gated it?")
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

// Pre-checks every occasion passes before its budget is even computed. False =
// declined; the decline is charged to the matching telemetry counter, which is
// what keeps the per-entry self-check (occasions == admitted + declines)
// balancing.
bool DefaultPolicy::slot_entry_admissible(SlotEntry &e, const Kernel &k,
                                          const SlotPhaseCtx &ctx) {
  if (!e.enabled || ctx.phase_unsat) {
    e.tm.dec_disabled++;
    return false;
  }
  if (e.max_applies && e.applies >= e.max_applies) {
    e.tm.dec_cap++;
    return false; // DIAGNOSTIC round cap (see SlotEntry::max_applies)
  }
  if (e.defer_preprocess && k.stats.conflicts == 0) {
    e.tm.dec_preprocess++;
    return false; // no elimination at preprocessing (kissat fastel=0)
  }
  e.applies++;
  return true;
}

// ACCUMULATE UNTIL WORTHWHILE (cadical limit.hpp:150). Decline while this
// occasion's delta is too small to repay the pass's setup, and crucially do NOT
// advance the anchor, so the entitlement accrues and a later occasion can
// afford a real pass. This is the only budget-based decline a batch entry has —
// the ceiling is re-baselined in set_entry_limit, which makes a `meter > limit`
// pre-check unfireable for one.
bool DefaultPolicy::slot_entry_thresh_declines(SlotEntry &e, const Kernel &k) {
  if (!(e.enabled && e.meter && e.thresh && e.doses > 0))
    return false;
  const uint64_t delta = e.limit - e.meter(k);
  if (delta >= e.thresh * kernel_irredundant_clauses(k))
    return false;
  e.tm.dec_budget++;
  return true; // anchor deliberately NOT advanced
}

// BVE ADMISSION. A during-search BVE dose fires only on binary-DENSE formulas
// — the same axis and threshold as Factor's density gate. Sparse formulas are
// where dosing measurably costs, and the cost is TRAJECTORY damage, not
// overhead. This is a hand-designed per-instance separator, a family that has
// measured badly more than once; its proper home is the phase-6 learned policy
// (plan §5.6 elimination-dosing exhibit). It survives only because removing
// admission entirely is much worse.
//
// TACTIX_BVE_ADMIT selects the mode (default `density` = shipped):
//   density — the binaries-per-variable proxy above.
//             TACTIX_BVE_DOSE_DENSITY overrides the threshold
//             (0 = dose everywhere).
//   cost    — decline once MEASURED cost-per-yield (resolutions per eliminated
//             variable) exceeds `Kernel::bve_cost_limit`. The FIRST dose always
//             runs: the ratio cannot be known before eliminating anything, so
//             this is learn-then-decide rather than a static separator.
//   none    — no admission at all. Measured clearly negative; kept as the A/B
//             floor, not shippable.
bool DefaultPolicy::bve_dose_declined(Kernel &k, SlotEntry &e) {
  if (!(e.retry_advice && k.stats.conflicts > 0))
    return false;
  static const char *const admit_env = std::getenv("TACTIX_BVE_ADMIT");
  const bool admit_cost = admit_env && !std::strcmp(admit_env, "cost");
  const bool admit_none = admit_env && !std::strcmp(admit_env, "none");
  if (admit_cost) {
    const uint64_t elim = k.stats.bve_eliminated;
    const uint64_t lim = k.bve_cost_limit;
    if (lim && elim && k.stats.bve_resolutions / elim > lim) {
      e.tm.dec_density++;
      return true; // this formula is paying too much per elimination
    }
  } else if (!admit_none) {
    uint64_t dose_density = 6;
    if (const char *dd = std::getenv("TACTIX_BVE_DOSE_DENSITY"))
      dose_density = std::strtoull(dd, nullptr, 10);
    if (dose_density &&
        k.binary_ids.size() < dose_density * kernel_num_output_vars(k)) {
      e.tm.dec_density++;
      return true;
    }
  }
  return false;
}

// One occasion of a DOSED entry: enumerate the rule's instances and apply them
// one at a time until the budget is spent. Payloads are literals/variables, so
// they survive the formula mutation each application causes.
DefaultPolicy::SlotStep
DefaultPolicy::run_dosed_entry(Kernel &k, SlotEntry &e, const RuleVTable &r,
                               SlotPhaseCtx &ctx) {
  const uint64_t limit = e.limit;
  if (!r.guard(k)) {
    e.tm.dec_guard++;
    return SlotStep::Next;
  }
  if (bve_dose_declined(k, e))
    return SlotStep::Next;
  // TACTIX_BVE_ONESHOT (DIAGNOSTIC): the BVE entry fires exactly ONE dose per
  // solve — full ramp, unbounded budget (see set_entry_limit) — then is
  // declined for the rest of the solve. Isolates "recurrence hurts" from "the
  // dose content hurts".
  const bool bve_oneshot = e.retry_advice && std::getenv("TACTIX_BVE_ONESHOT");
  if (bve_oneshot && e.doses > 0)
    return SlotStep::Next; // the one shot is consumed
  // TACTIX_BVE_CADENCE (DIAGNOSTIC): ONE completed call per phase — outer
  // rounds 1-2 are declined so the phase cannot re-drip what the completed
  // call finished.
  if (e.retry_advice && ctx.outer > 0 && std::getenv("TACTIX_BVE_CADENCE"))
    return SlotStep::Next;
  // BVE growth-slack RAMP (BveBoundPolicy): drive k.bve_bound cheap-first over
  // the sub-policy's passes (kissat eliminate.c set_next_elimination_bound), so
  // the non-growing eliminations happen first and later ones are measured
  // against an already-simplified formula — the adaptive version of a single
  // fixed bound. A null ramp (or a non-BVE dosed entry) = one pass at the
  // current bound. The dose is SHARED across passes.
  BveBoundPolicy *ramp = nullptr;
  if (e.retry_advice && !std::getenv("TACTIX_NO_BVE_RAMP"))
    ramp = bve_bound ? bve_bound : &default_bve_ramp_;
  // In a during-search dose, kissat's persistent completion-gated bound LEVEL
  // (see bve_level_ in the header) replaces the all-levels-per-dose ramp — one
  // pass per dose at the current level; the level doubles below only when the
  // dose completes.
  const bool level_mode = e.retry_advice && ramp && !bve_oneshot;
  const uint32_t passes = level_mode ? 1 : ramp ? ramp->num_passes() : 1;
  bool dose_cut = false;
  // kissat advances the bound only at a genuine FIXPOINT --
  // `complete = !remain && !last_round_eliminated` (eliminate.c:544).
  // "Finished within budget" is the weaker `!remain` half alone; the second
  // half matters because a pass that still eliminated something has not
  // exhausted the CURRENT bound, and advancing there moves to a more
  // aggressive bound while cheap work remains.
  uint64_t elim_at_last_pass = k.stats.bve_eliminated;
  // During a ramp the policy consults BOTH the retry bit AND the per-variable
  // needed-bound cache the rule fills (Kernel::bve_min_bound, enabled by
  // bve_track_min_bound below): a variable rejected for growth at a low slack
  // is skipped at every slack BELOW the one it actually needs, and re-attempted
  // only when its neighbourhood changes (retry bit set). This is what stops the
  // ramp re-resolving every rejected variable at all six levels. A bare retry
  // bit CANNOT do this: it is cleared on any attempt (including a growth-abort),
  // so it would skip a low-slack reject at every HIGHER slack too and collapse
  // the ramp to level 0. Without a ramp the retry bit alone is consulted
  // (cross-round dedup).
  const bool use_retry = e.retry_advice && !ramp;
  k.bve_track_min_bound = ramp != nullptr;
  for (uint32_t pass = 0; pass < passes && !ctx.phase_unsat; pass++) {
    // Skip remaining ramp levels once the dose is spent — but ONLY when
    // ramping. Without a ramp (passes==1) this must NOT short-circuit:
    // kernel_bve_retry_sync_fixed (persistent state) and r.instances must still
    // run before the inner-loop meter check, and a top-of-pass break would skip
    // that sync whenever the dose is already spent — a divergence visible only
    // across multiple phases.
    if (ramp && e.meter(k) > limit) {
      dose_cut = true;
      break;
    }
    if (ramp)
      k.bve_bound = level_mode ? bve_level_ : ramp->bound(pass);
    elim_at_last_pass = k.stats.bve_eliminated; // fixpoint test above
    // Fold newly root-fixed literals into the rule's retry advice before
    // reading it: fixing y changes what BVE computes for y's neighbours with no
    // clause add/delete to mark them. Needed by BOTH the retry and the ramp
    // needed-bound paths (a fixing invalidates a neighbour's cached bound), so
    // sync whenever the rule maintains the advice.
    if (e.retry_advice)
      kernel_bve_retry_sync_fixed(k);
    inprocess_payloads_.resize(2 * static_cast<size_t>(k.nvars) + 1);
    const uint32_t n =
        r.instances(k, inprocess_payloads_.data(),
                    static_cast<uint32_t>(inprocess_payloads_.size()));
    // WHERE THIS WALK STARTS AND IN WHAT ORDER IT PROCEEDS (SCHED-E1). Under a
    // cutting budget this is the scheduling decision: the dose below stops
    // part-way, so the head is what gets applied. Both steps are permutations
    // of what the rule enumerated — see order_candidates.
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
      // SCHEDULING (policy-side, kissat flags->eliminate): decline a variable
      // whose outcome this pass is already known. Unchanged neighbourhood
      // (retry bit clear) ⇒ the same attempt recomputes the same NoOp; DURING a
      // ramp, additionally, a clean variable known to need MORE slack than this
      // pass offers (bve_min_bound) cannot be eliminated here either. The rule
      // still OFFERS and ACCEPTS it — the policy declines, not the rule (N1).
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
      // a budget cut two lines above leaves the NEXT occasion resuming here
      // instead of replaying the head (SCHED-E1; SCHED-B3's rule-side
      // TransitiveReduce cursor, now policy state and shared by every dosed
      // entry that opts in). Written on the ATTEMPT, not on success: a
      // candidate that turned out non-viable has still been looked at.
      if (e.cursor_enabled) {
        e.cursor = instance_key(inprocess_payloads_[i]) + 1;
        e.ord.advanced++;
      }
      const ApplyResult ar = [&] {
        TickSinkScope sink(k, e.tick_counter);
        return apply_rule_instance(r, k, inprocess_payloads_[i]);
      }();
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
  // Level advance (kissat set_next_elimination_bound): only a dose that ran its
  // whole schedule within budget earns the doubling.
  const bool last_pass_eliminated = k.stats.bve_eliminated > elim_at_last_pass;
  if (level_mode && !dose_cut && !ctx.phase_unsat && !last_pass_eliminated) {
    const uint32_t ceiling = ramp->bound(ramp->num_passes() - 1);
    bve_level_ = bve_level_ ? std::min(bve_level_ * 2, ceiling) : 1;
  }
  // factordelay input: one more BVE round has run. Counted once per PHASE, not
  // per outer round — kissat INC(eliminations) fires once per kissat_eliminate
  // CALL (eliminate.c), whose analog is the phase; per-outer counting advances
  // the gate three decades too fast.
  if (e.retry_advice && ctx.outer == 0)
    bve_rounds_run_++;
  if (bve_oneshot)
    e.doses++; // consume the one shot (skipped hereafter)
  // The anchor advances HERE, on the ADMITTED path ONLY. kissat writes
  // `last.NAME.ticks` inside SET_EFFORT_LIMIT, i.e. only on a call that
  // actually runs, so a skipped occasion leaves the reference window growing
  // and the next one affordable. Advancing it for every enabled entry instead
  // makes a DECLINED occasion discard its accrued entitlement.
  e.anchor = search_ticks(k);
  e.tm.admitted++; // the dosed branch ran; it exits here, not below
  // A refutation found inside a dose does NOT leave the entry walk: the
  // remaining entries are declined by slot_entry_admissible's phase_unsat test,
  // which keeps their telemetry balanced.
  return SlotStep::Next;
}

// Factor's two scheduling gates, both kissat's (factor.c:1068-1085 / :1153).
bool DefaultPolicy::factor_entry_declines(const Kernel &k, SlotEntry &e) {
  // factordelay (kissat factor.c:1068-1085): a size-vs-progress warm-up gate,
  // NOT the kimits.c occasions delay. kissat's form TRUNCATES:
  // `size_t log_active = log10(active)` then `log_active <= eliminations + 4`,
  // so it admits active < 10^(eliminations + 5) — up to 99,999 vars at the very
  // first probe. Reading it as 10^(e+4) is one decade too strict.
  // Active ≈ unassigned − BVE-eliminated (decade granularity makes the
  // approximation safe).
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
  // Completion-gated new-material watermark (kissat's literals_factor analog):
  // re-fire only on new binaries / root-fixed vars since the last COMPLETED
  // round — the marks advance in run_budgeted_batch only when the dose ran to
  // natural exhaustion (factor.c:1153), so an incomplete (budget-cut) round
  // re-fires and resumes.
  if (k.stats.conflicts > 0 && k.binary_ids.size() <= e.binaries_mark &&
      k.arena.size() <= e.arena_mark &&
      kernel_root_fixed(k) <= e.root_fixed_mark) {
    e.tm.dec_material++;
    return true;
  }
  return false;
}

// Factor's per-occasion rule config: composition + the density advice. Both are
// set around the apply and cleared by the caller, so no other caller of the
// rule ever sees them.
void DefaultPolicy::factor_dose_config(Kernel &k, SlotEntry &e) {
  // COMPOSITION IS UNCONDITIONAL, matching kissat: `resize_factoring` is called
  // from `apply_factoring` (factor.c:838) with no gate, so the factoring
  // universe always grows over the fresh extension variables and later chains
  // in the SAME pass compose over them. Gating it on binary density instead
  // makes applied factorings SATURATE on sparse formulas, which loses instances
  // outright.
  //
  // WHAT BOUNDS COMPOSITION INSTEAD — kissat's governors, all of which we have:
  // (a) the factor_ticks effort budget, checked in its main loop
  // (`if (*ticks > limit) break`, factor.c:914) and here as the EffortSpec
  // 50-per-mille handoff; (b) a score-ordered candidate heap, so the budget is
  // spent best-first; (c) one attempt per literal per pass.
  //
  // kissat has a FOURTH governor we do not: its acceptance test is
  // `reduction > factoring.bound` where
  // `bound = solver->bounds.eliminate.additional_clauses` (factor.c:72/939) —
  // acceptance COUPLED to BVE's growth allowance, so factoring gets pickier as
  // elimination gets more permissive. Ours is `Kernel::factor_bound`, default 0
  // ("any positive gain"). If unconditional composition ever regresses the tier
  // pair, port that fourth governor; do NOT reinstate a density gate. (A
  // version of the coupling was measured negative, but only WITH composition
  // gated off, so it does not transfer.)
  //
  // TACTIX_COMPOSITE_DENSITY is retained as an A/B hatch only: a non-zero value
  // restores the old gate at that threshold.
  k.factor_composite = true;
  if (const char *cd = std::getenv("TACTIX_COMPOSITE_DENSITY")) {
    const uint64_t d = std::strtoull(cd, nullptr, 10);
    k.factor_composite =
        d ? k.binary_ids.size() >= d * kernel_num_output_vars(k) : false;
  }
  // Factor's binary-density admission. This is a SCHEDULING decision, so it
  // lives HERE next to BVE's identical gate, and reaches the rule as advice
  // that DEFAULTS TRUE. It must NOT move back inside `factor_apply`: as a
  // rule-side NoOp it refused every caller on a binary-sparse formula, not just
  // this policy, which breaks N1.
  //
  // Be honest about what this is: a hand-designed per-instance separator. The
  // family has been measured against no gate at all several times and lost, no
  // comparable axis exists in kissat or cadical, and on Tier B it is never the
  // best of {gated, always-admit, per-instance oracle}
  // (benchmarks/reports/2026-08-04-factor-oracle-ablation.md). It survives only
  // because Tier C wants it and nothing better exists yet. Its replacement is a
  // targeted admission mechanism, not a further threshold.
  //
  // The scope-percent UNION arm (`Kernel::factor_scope_percent`, A/B only,
  // default 0) is strictly additive: it can only ADMIT where the density gate
  // rejected, so today's beneficiaries cannot regress. Its arena scan
  // deliberately charges no factor_ticks — a rejected call must not consume the
  // once-per-solve first-dose allowance.
  uint32_t density = 6;
  if (const char *d = std::getenv("TACTIX_FACTOR_DENSITY"))
    density = static_cast<uint32_t>(std::atoi(d));
  // uint64 multiply: both operands are uint32, and a formula with tens of
  // millions of variables makes any density >= 89 WRAP, silently admitting
  // where it was meant to suppress.
  bool admit = k.binary_ids.size() >=
               static_cast<uint64_t>(density) * kernel_num_output_vars(k);
  if (!admit && k.factor_scope_percent) {
    uint64_t total = k.binary_ids.size();
    uint64_t in_scope = k.binary_ids.size();
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      const uint32_t words = clause_words(c->size);
      if (!c->garbage() && !c->redundant()) {
        total++;
        if (c->size <= k.factor_size)
          in_scope++;
      }
      ref += words;
    }
    if (total && in_scope * 100 >= static_cast<uint64_t>(k.factor_scope_percent) *
                                       total)
      admit = true;
  }
  k.factor_admit = admit;
  if (!admit) {
    e.tm.admitted--; // counted by the caller; the policy is declining it
    e.tm.dec_density++;
  }
}

// kissat vivify.c structure: the REDUNDANT (learned) tiers always run within
// their budget share; only the IRREDUNDANT pass is delay-gated (vivifyirr,
// kimits.c). Delaying the WHOLE round instead is measurably worse. Shares are
// kissat's option defaults: irr 3 / tiers 3+3+1 = 7.
ApplyResult DefaultPolicy::run_vivify_dose(Kernel &k, SlotEntry &e,
                                           const RuleVTable &r,
                                           uint64_t limit) {
  const uint64_t remaining0 = limit - e.meter(k);
  const uint64_t remaining = remaining0 ? remaining0 : 1;
  const bool irr_delayed = k.stats.conflicts > 0 && e.delay_count > 0;
  if (irr_delayed)
    e.delay_count--;
  // redundant pass (always; full budget when irr is delayed)
  ApplyResult ar;
  k.vivify_scope = 1;
  k.work_budget =
      irr_delayed ? remaining : std::max<uint64_t>(1, remaining * 7 / 10);
  {
    TickSinkScope sink(k, e.tick_counter);
    ar = apply_rule(r, k);
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
    const ApplyResult ar2 = apply_rule(r, k);
    if (ar2 == ApplyResult::Unsat)
      ar = ar2;
    else if (ar2 == ApplyResult::Ok && ar != ApplyResult::Ok)
      ar = ar2;
    if (k.stats.conflicts > 0) {
      // kissat vivify.c: vivified/tried < 1 % → bump, else halve;
      // tried == 0 counts unproductive (kissat_average(0,0)=0)
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
// phase's REMAINING ledger budget — in the rule's own currency — around its
// apply, so one canonical round cannot overrun the dose. Scoped to the apply;
// 0 leaves the rule unbounded for every other caller (API, RandomPolicy).
ApplyResult DefaultPolicy::run_budgeted_batch(Kernel &k, SlotEntry &e,
                                              const RuleVTable &r,
                                              uint64_t limit,
                                              bool factor_entry) {
  const uint64_t remaining0 = limit - e.meter(k);
  const uint64_t handed = remaining0 ? remaining0 : 1;
  const uint64_t spent_before = e.meter(k);
  // Factor doses run COMPOSITE: kissat's cross-round composition lives in
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
  // A zero-spend NoOp (Factor's admission short-circuit returns before the seed
  // scan) consumes NEITHER the first-dose allowance NOR the watermark —
  // kissat's skipped call likewise leaves limits.factor.marked untouched
  // (factor.c:1094-1101). The re-entry cost is one O(1) density check per phase.
  const uint64_t spent = e.meter(k) - spent_before;
  e.tm.handed += handed;
  e.tm.spent += spent;
  if (spent > 0)
    e.doses++;
  // Completion-gated watermark advance (kissat factor.c:1153): spend within the
  // handed budget ⇒ the round ran to natural exhaustion — advance the
  // new-material marks. A budget-cut round leaves them, so it re-fires next
  // phase and resumes.
  if (spent > 0 && spent <= handed) {
    e.tm.completed++;
    e.binaries_mark = k.binary_ids.size();
    e.arena_mark = k.arena.size();
    e.root_fixed_mark = kernel_root_fixed(k);
  }
  return ar;
}

// One occasion of a BATCH / UNMETERED entry: one canonical round, metered after
// the fact (or budget-handed, for the entries that poll work_budget).
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
  // In a during-search phase an UNMETERED closure (Congruence / Substitute)
  // re-runs only on new material since ITS last run. Repeating the full
  // closures every phase is a dominant diffuse Tier C cost; dropping them
  // entirely loses instances outright.
  if (!e.meter && k.stats.conflicts > 0 &&
      k.binary_ids.size() <= e.binaries_mark &&
      kernel_root_fixed(k) <= e.root_fixed_mark) {
    e.tm.dec_material++;
    return SlotStep::Next;
  }
  // Closure delay ladder (see solve entry): consume one delayed occasion AFTER
  // material admission, so the ladder counts real scheduling occasions (kissat
  // kimits delay semantics — same mechanics as vivify's irr ladder).
  if (closure_delay_ && !e.meter && k.stats.conflicts > 0 &&
      e.delay_count > 0) {
    e.delay_count--;
    e.tm.dec_delay++;
    return SlotStep::Next;
  }
  if (!r.guard(k)) {
    e.tm.dec_guard++;
    return SlotStep::Next;
  }
  // The anchor advances HERE, on the ADMITTED path ONLY — see run_dosed_entry
  // for why.
  e.anchor = search_ticks(k);
  e.tm.admitted++;
  // Vivify keeps its kissat scope/delay tier split; the other budget-aware
  // rules take the plain handoff.
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
  if (!e.meter) {
    e.binaries_mark = k.binary_ids.size();
    e.arena_mark = k.arena.size();
    e.root_fixed_mark = kernel_root_fixed(k);
    // Ladder maintenance (the vivify idiom above): a NoOp closure bumps the
    // delay, a productive one halves it.
    if (closure_delay_ && k.stats.conflicts > 0) {
      if (ar == ApplyResult::Ok)
        e.delay_current /= 2;
      else
        e.delay_current += (e.delay_current < UINT32_MAX);
      e.delay_count = e.delay_current;
    }
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

// One inprocessing phase on ONE clock (see SlotGroup / want_slot_phase). Only
// this group's entries are walked: an entry on the other clock is not offered
// an occasion here at all, so its telemetry counters are not touched and the
// per-entry self-check keeps balancing.
int DefaultPolicy::run_slot_phase(Kernel &k, SlotGroup group,
                                  volatile sig_atomic_t *stop) {
  SlotClock &clock = group == SlotGroup::Probe ? probe_clock : elim_clock;
  // Snapshot this clock's gate marks BEFORE the pipeline runs (SlotClock's gate
  // block): material the phase itself produces then re-admits the next one, so
  // a budget-cut pipeline is never gated out of its own follow-up.
  clock.reductions_mark = k.stats.reductions;
  clock.marks_mark = k.stats.bve_marks;
  clock.fixed_mark = kernel_root_fixed(k);
  clock.blocked = false; // a new blocking episode may now be counted
  // A simplification PHASE: descend to a quiescent root (a transient flush
  // that keeps saved/target phases — kernel_backtrack(update_phases=true) —
  // so search re-descends toward the same region afterward), then give each
  // enabled technique IN THIS GROUP one dose, sized as a per-mille fraction of
  // the search work that preceded it (kissat SET_EFFORT_LIMIT; the ledger is in
  // SlotEntry). Bounding matters — an unbounded fixpoint drowns even
  // trivial instances — but do NOT bound by APPLICATION COUNT: that is
  // blind to formula size and caps an O(applications × formula) loop
  // rather than capping simplification.
  const uint64_t phase_ticks_at_entry = k.stats.ticks;
  const uint64_t phase_props_at_entry = k.stats.propagations;
  // Wall clock for the WHOLE slot, so the `c timing` report can separate
  // the rules' own time (charged per id by apply_rule) from the phase
  // scaffolding around them — index build, garbage collection, the
  // scheduling itself. Diagnostic only; nothing reads it back. The phase
  // has exactly one exit (below), so one accumulate covers every path.
  const uint64_t phase_nanos_at_entry = now_nanos();
  k.stats.inprocess_phases++;
  if (k.level > 0 && restart_guard(k))
    restart_apply_to(k, 0);
  // Attach the shared occurrence index for the phase: the occurrence
  // consumers (BVE) read it instead of rebuilding a ~O(arena) index per
  // apply. Detached on EVERY exit below so k.dense never dangles.
  kernel_dense_connect(k, inprocess_occ);
  // Factor grid-selection policy for this phase: pure config, like
  // k.bve_bound — Factor is a batch entry reached via r.apply(k) below and
  // reads it only there, so once per phase suffices. Scoped set/clear
  // (mirrors the dense index) so the kernel never holds a dangling
  // pointer into this policy after solve returns.
  default_factor_match_.bound = k.factor_bound; // see DefaultFactorMatch
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
  //   per BVE *variable* instead costs hundreds of full subsumption sweeps
  //   per phase for no additional yield.
  //   UNMETERED rules (Congruence/Substitute) expose only the canonical
  //   instance: one pass per outer round, nothing to titrate.
  SlotPhaseCtx ctx{group, stop};
  const std::vector<RuleVTable> &rules = tactix_rules();
  // Budgets: the dose is `permille` of the search work done since this
  // entry last ran (kissat kimits.h SET_EFFORT_LIMIT), floored so a
  // technique with no search to amortize still gets a usable first dose.
  // Computed per entry at the point of use (set_entry_limit), and the
  // anchor advances only on the ADMITTED path, so a declined occasion does
  // not discard its accrued entitlement.
  //
  // §6 STOP RESPONSIVENESS: the slot polls `stop` at every entry boundary
  // and inside the dosed per-instance loop, not only in the outer search
  // loop — otherwise a long inprocessing phase leaves the solver deaf to
  // SIGTERM for its whole duration, and a competition harness escalates to
  // SIGKILL and loses the proof file. The phase has a single exit, so
  // breaking out here still runs its cleanup (dense disconnect, GC, ledger
  // accounting) and the proof stays well-formed.
  // KNOWN RESIDUAL: a single rule APPLICATION is still uninterruptible, so
  // this bounds latency to one application rather than one phase. An
  // unmetered closure (Congruence) can be minutes on its own. The real fix
  // is a termination flag the RULES poll — a rule-layer change.
  for (ctx.outer = 0;
       ctx.outer < INPROCESS_PHASE_ROUNDS && !ctx.phase_unsat &&
       !(stop && *stop);
       ctx.outer++) {
    ctx.round_changed = false;
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
      // Ceiling computed HERE, against the state the rule will actually
      // see, rather than frozen at phase entry for all entries.
      if (e.enabled && e.meter)
        set_entry_limit(e, k);
      if (slot_entry_thresh_declines(e, k))
        continue;
      const SlotStep step = (e.dosed && r.instances && r.apply_instance)
                                ? run_dosed_entry(k, e, r, ctx)
                                : run_batch_entry(k, e, r, ctx);
      if (step == SlotStep::Stop)
        break;
    }
    if (!ctx.round_changed)
      break; // outer fixpoint: nothing changed this round
  }
  // TACTIX_SWEEP_PROBE (DIAGNOSTIC, off by default): one EXTRA, UNBUDGETED
  // Sweep application per phase, on top of the ordinary scheduled Sweep
  // slot entry. It runs at the quiescent root with `k.dense` still
  // connected. Because it is unbudgeted it must never be set in a tier
  // run; it exists to drive the sweep + proof path hard in fuzz legs.
  // PROBING phases only — Sweep is a probing-clock entry, and firing the probe
  // on the elimination clock too would silently double its rate.
  if (group == SlotGroup::Probe && !ctx.phase_unsat &&
      std::getenv("TACTIX_SWEEP_PROBE")) {
    if (sweep_apply(k) == ApplyResult::Unsat)
      ctx.phase_unsat = true;
  }
  // Reclaim what the dosed applications garbaged. The canonical rounds
  // reclaim themselves; the per-instance path deliberately does not, so
  // that a dose costs its own work and not an O(arena) collection per
  // eliminated variable. Not collecting is always sound (deletes are lazy
  // and every reader skips garbage), so this is a cost choice, not a
  // correctness one — see docs/design/effort-scheduler-design.md §3.4.
  if (ctx.phase_dosed && !ctx.phase_unsat)
    kernel_collect_garbage(k);
  kernel_dense_disconnect(k);
  k.factor_match = nullptr; // scope the config to the phase (see entry)
  inprocess.ticks += k.stats.ticks - phase_ticks_at_entry;
  k.stats.inprocess_propagations +=
      k.stats.propagations - phase_props_at_entry;
  k.stats.inprocess_nanos += now_nanos() - phase_nanos_at_entry;
  if (ctx.phase_unsat)
    return 20;
  // The phase FIRED (whether or not it simplified anything): count it on THIS
  // clock — it is that clock's interval-growth key, so the two schedules
  // separate on their own.
  clock.rounds++;
  update_slot_limit(k, group);
  return 0;
}

void DefaultPolicy::set_entry_limit(SlotEntry &e, Kernel &k) {
  // Computed at the moment the entry is REACHED, not once for every entry at
  // phase entry. kissat reads OLD_LIMIT inside SET_EFFORT_LIMIT when the
  // technique begins (kimits.h:138), so its budget is a delta from where the
  // technique actually starts. Freezing all entries' ceilings up front hands an
  // entry late in the slot a ceiling computed against a formula and a tick
  // count that no longer exist by the time it runs.
  const std::vector<RuleVTable> &rules = tactix_rules();
        const uint64_t now = search_ticks(k);
        uint64_t reference = now - e.anchor;
        if (reference < MIN_EFFORT_TICKS)
          reference = MIN_EFFORT_TICKS;
        e.limit = e.meter(k) + reference / 1000 * e.permille;
        // The FIRST Factor dose per solve bypasses the permille formula with
        // kissat's factoriniticks allowance — an effectively unbounded first
        // round (kissat factor.c:1106-1117). See FACTOR_INIT_TICKS.
        if (e.doses == 0 && e.budget_aware &&
            !std::strcmp(rules[e.rule].name, "Factor"))
          e.limit = e.meter(k) + factor_init_ticks_;
        // TACTIX_BVE_ONESHOT (DIAGNOSTIC): give the first during-search BVE
        // dose Factor's first-dose allowance — effectively unbounded, so the
        // dose runs its whole schedule to completion. The dosed branch in
        // solve then retires the entry after this one dose.
        if (e.doses == 0 && e.retry_advice &&
            std::getenv("TACTIX_BVE_ONESHOT"))
          e.limit = e.meter(k) + 700000000ull;
        // TACTIX_BVE_CADENCE (DIAGNOSTIC): give EVERY admitted BVE dose the
        // completion allowance. Combined with the outer==0 gate in solve
        // this is one COMPLETED call per phase, which — since the phase
        // schedule already fires on the growing NLOG2N conflict interval —
        // reproduces kissat's shape: completion-gated calls at geometric
        // conflict spacing, ladder advancing per completion. It keeps
        // recurrence while removing the incomplete drip.
        if (e.retry_advice && std::getenv("TACTIX_BVE_CADENCE"))
          e.limit = e.meter(k) + 700000000ull;
}

void DefaultPolicy::report_slot(const Kernel &k) const {
  // One line per slot entry. DIAGNOSTIC ONLY — no scheduling decision reads any
  // of it. It answers the question `c stats` cannot: was an entry STARVED
  // (declined for budget) or IDLE (ran and found nothing)? Those are opposite
  // defects with opposite fixes and identical signatures otherwise.
  //
  // `spent/handed` is the one to read first: persistently << 1 means the entry
  // finishes early and its per-mille is not the binding constraint; ~1 with
  // `completed` far below `admitted` means it is being CUT mid-round and its
  // budget IS binding. `dec_budget` counts occasions it never started at all.
  //
  // The `c meters` line is a REACHABILITY check on the private per-rule
  // counters. If they read zero, the meters are inert and every rule budgeting
  // on them is silently unbudgeted — in which case a green run proves nothing.
  std::printf("c meters      probe %llu backbone %llu vivify %llu "
              "instantiate %llu | global ticks %llu\n",
              (unsigned long long)k.stats.probe_ticks,
              (unsigned long long)k.stats.backbone_ticks,
              (unsigned long long)k.stats.vivify_ticks,
              (unsigned long long)k.stats.instantiate_ticks,
              (unsigned long long)k.stats.ticks);
  const std::vector<RuleVTable> &rules = tactix_rules();
  // REACHABILITY readout for the authored order (SCHED-D1): the live sequence,
  // named, so an A/B arm cannot be banked without showing which order ran.
  std::printf("c slot-order  %s", slot_order_legacy_ ? "legacy  " : "authored");
  for (const SlotEntry &e : inprocess_slot)
    std::printf(" %s", rules[e.rule].name);
  std::printf("\n");
  // REACHABILITY readout for the two clocks (SCHED-D2) and their gates
  // (SCHED-D3). `probe phases` above `elim phases` is the split working; equal
  // counts mean it did not take. `blocked` is the gate: episodes in which an
  // ELAPSED interval was declined for want of a reduce (probe) or of new
  // material (eliminate). Zero across a run means the gate never fired and an
  // arm resting on it proved nothing.
  std::printf("c slot-clock  probe phases %llu next %llu blocked %llu | "
              "eliminate phases %llu next %llu blocked %llu | bve-marks %llu\n",
              (unsigned long long)probe_clock.rounds,
              (unsigned long long)probe_clock.next_conflicts,
              (unsigned long long)probe_clock.blocked_episodes,
              (unsigned long long)elim_clock.rounds,
              (unsigned long long)elim_clock.next_conflicts,
              (unsigned long long)elim_clock.blocked_episodes,
              (unsigned long long)k.stats.bve_marks);
  // REACHABILITY readout for the resumption cursors and (from SCHED-E2) the
  // candidate-ORDER arms. One line per DOSED entry — the only entries whose
  // candidate list the policy sees at all. Read it BEFORE any A/B verdict:
  //   `rotated 0` ⇒ no dose of this entry was ever truncated on this instance,
  //     so the cursor is inert here and an on/off arm proves nothing.
  //   `moved 0` under a non-native arm ⇒ the arm did not reorder anything, so
  //     the arm was not tested.
  for (const SlotEntry &e : inprocess_slot) {
    if (!e.dosed)
      continue;
    std::printf("c cand-order  %-18s cursor %s | lists %llu "
                "candidates %llu rotated %llu advanced %llu ranked %llu "
                "moved %llu\n",
                rules[e.rule].name,
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
    // SELF-CHECK: occasions must equal admitted + every decline reason. If it
    // does not, a `continue` was added without a counter and this entry's
    // numbers are silently incomplete — which is worse than no telemetry,
    // because it reads as a finding. Printed, not asserted, so a release build
    // still reports rather than dying mid-solve.
    const uint64_t declined =
        t.dec_disabled + t.dec_cap + t.dec_preprocess + t.dec_guard +
        t.dec_budget + t.dec_delay + t.dec_material + t.dec_warmup +
        t.dec_density + t.dec_oneshot + t.dec_nonviable;
    const char *balance = (t.admitted + declined == t.occasions) ? "" : "  ***UNACCOUNTED***";
    std::printf(
        "c slot %-18s occ %6llu adm %5llu ok %5llu | dec dis %llu cap %llu "
        "pre %llu grd %llu bud %llu dly %llu mat %llu warm %llu dns %llu "
        "one %llu nv %llu | handed %llu spent %llu completed %llu%s\n",
        rules[e.rule].name, (unsigned long long)t.occasions,
        (unsigned long long)t.admitted, (unsigned long long)t.ok,
        (unsigned long long)t.dec_disabled, (unsigned long long)t.dec_cap,
        (unsigned long long)t.dec_preprocess, (unsigned long long)t.dec_guard,
        (unsigned long long)t.dec_budget, (unsigned long long)t.dec_delay,
        (unsigned long long)t.dec_material, (unsigned long long)t.dec_warmup,
        (unsigned long long)t.dec_density, (unsigned long long)t.dec_oneshot,
        (unsigned long long)t.dec_nonviable, (unsigned long long)t.handed,
        (unsigned long long)t.spent, (unsigned long long)t.completed, balance);
  }
}

int default_solve(Kernel &k, volatile sig_atomic_t *stop) {
  DefaultPolicy policy;
  const int res = policy.solve(k, stop);
  // Opt-in via TACTIX_SLOT_STATS. Deliberately NOT on by default:
  // `default_solve` does not know main's `quiet`, and competition stdout must
  // stay exactly what the runner expects.
  if (std::getenv("TACTIX_SLOT_STATS"))
    policy.report_slot(k);
  return res;
}

} // namespace tactix
