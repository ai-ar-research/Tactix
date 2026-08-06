// DefaultPolicy (plan §5): the statically composed micro-loop — one
// instantiation of the Policy interface, no special status. Calls the
// SAME rule implementations the registry exposes, but through direct
// (inlinable) calls — the "static dispatch" side of the two-tier design.
// Owns every scheduling heuristic it uses: the focused-mode restart
// schedule (kissat's bias-corrected per-mode glue EMAs), the stable-mode
// reluctant-doubling restart schedule, the arithmetic reduce interval, the
// mode-switch alternation (kissat mode.c: first switch after `modeinit`
// conflicts, stable phases last the preceding focused phase's ticks,
// focused intervals grow as modeint·n·log⁴n), the stable-only rephase
// schedule (kissat rephase.c: rephaseint·n·log³n interval, B,W,I,B,W,O
// rotation), and the inprocessing slot (every registry Inprocess-tier rule,
// split across kissat's two size-scaled clocks — see SlotGroup). Constants
// are the published kissat/cadical/Glucose defaults unless a comment says
// otherwise.
#pragma once

#include "policies/policy.hpp"
#include "policies/subpolicy.hpp"
// for ApplyResult (the slot helpers' return type) and RuleVTable's declaration
#include "rules/registry.hpp"

#include <cstdlib>

namespace tactix {

// canonical branching: the rule/kernel ranking's top pick (VMTF
// last-enqueued-unassigned, saved phase — §5.1)
class DefaultBranching final : public BranchingPolicy {
public:
  const char *name() const override { return "vmtf-evsids"; }
  Lit pick(Kernel &) override;

  // Random decisions (kissat decide.c `randec`). A pure H operation: it
  // changes WHICH variable is decided, never the formula, so there is no proof
  // delta and no calculus entry — it is policy, like Rephase or Reorder.
  // kissat ships randec=1 with randecfocused=1 and randecstable=0, so the
  // sequences run in FOCUSED mode only. Length is `randeclength * LOGN(count)`
  // conflicts (options.h:103-108).
  //
  // Deviation: the sequence is tracked as an END CONFLICT rather than a
  // countdown. Equivalent to kissat's per-conflict decrement, and it needs no
  // new hook on the conflict path.
  static constexpr uint64_t RANDEC_INIT = 500;   // randecinit
  static constexpr uint64_t RANDEC_LENGTH = 10;  // randeclength
  static constexpr uint64_t RANDEC_SEED = 0x9E3779B97F4A7C15ull;
  uint64_t randec_until = 0;      // conflict count at which the sequence ends
  uint64_t randec_next = RANDEC_INIT; // next conflict count that may start one
  uint64_t randec_sequences = 0;
  uint64_t randec_rng = RANDEC_SEED; // deterministic, seeded fixed
  bool randec_enabled = true; // A/B arm TACTIX_RANDEC=0 (see solve())
  // Per-solve re-initialisation, re-based on the current conflict count
  // (kissat search.c:70 re-seeds the RNG and kimits.c:107
  // INIT_CONFLICT_LIMIT(randec, false) re-bases the sequence limit at every
  // search start). `randec_enabled` is configuration and survives.
  void reset(uint64_t conflicts) {
    randec_until = 0;
    randec_next = conflicts + RANDEC_INIT;
    randec_sequences = 0;
    randec_rng = RANDEC_SEED;
  }
};

// canonical reduction volume: the rule's canonical fraction of the ranked
// worst-first candidates (reduce_canonical_victims — defined rule-side so
// the two cannot drift)
class GlueTieredReduction final : public ReductionPolicy {
public:
  const char *name() const override { return "glue-tiered"; }
  size_t victims(const Kernel &, size_t candidates) override;
  // kissat's ramped fraction instead of the flat canonical one; set by
  // DefaultPolicy together with its √-cadence arm (the two go together).
  bool ramp = false;
};

// canonical BVE growth-slack ramp: kissat's cheap-first doubling 0,1,2,4,8
// (eliminate.c set_next_elimination_bound), capped at slack-8 — see ceiling().
class DefaultBveBoundRamp final : public BveBoundPolicy {
public:
  const char *name() const override { return "kissat-ramp"; }
  // Growth-slack ceiling: 8, a deliberate deviation from kissat's 16. kissat
  // reaches slack-16 gradually across many interleaved eliminate calls and
  // reins in harmful growth with mode/effort feedback our single preprocessing
  // blast does not have. The empirical warrant is ONE Tier B A/B and Tier B has
  // no measured noise floor, so treat the exact value as unvalidated beyond one
  // tier; TACTIX_BVE_RAMP_MAX=N re-sweeps it.
  static uint32_t ceiling() {
    if (const char *s = std::getenv("TACTIX_BVE_RAMP_MAX"))
      return static_cast<uint32_t>(std::atoi(s));
    return 8;
  }
  uint32_t num_passes() const override {
    const uint32_t cap = ceiling();
    static constexpr uint32_t levels[6] = {0, 1, 2, 4, 8, 16};
    uint32_t n = 0;
    for (uint32_t i = 0; i < 6; i++)
      if (levels[i] <= cap)
        n = i + 1;
    return n ? n : 1;
  }
  uint32_t bound(uint32_t pass) const override {
    static constexpr uint32_t levels[6] = {0, 1, 2, 4, 8, 16};
    return levels[pass < 6 ? pass : 5];
  }
};

// canonical Factor grid selection: watch-list-size scoring. Watch sizes are
// ≤ 2^24, so the double→uint32_t round-trip at the rule's integer heap key /
// tie-break comparison is order-exact (no tie flips). accept() stays the
// minimum-gain floor and is deliberately NOT tightened — kissat's effective
// acceptance is ≈ delta>0 (docs/design/s2a-factor-matcher-design.md §2b). The
// knob exists for N1 / learned-policy arms, not for the default.
class DefaultFactorMatch final : public FactorMatchPolicy {
public:
  const char *name() const override { return "watches-size"; }
  double seed_score(const Kernel &k, Lit s) override {
    return k.watches.heads[s].size; // == the rule's watch_score, today's key
  }
  double next_factor_score(const Kernel &k, Lit c, uint32_t /*cofactor*/,
                           Lit /*seed*/) override {
    return k.watches.heads[c].size; // == today's tie-break
  }
  // Mirror of Kernel::factor_bound (kissat's `reduction > bound`,
  // factor.c:939), copied in at the attach site. The rule's own acceptance is
  // `fm ? fm->accept(...) : delta > k.factor_bound`, and DefaultPolicy ALWAYS
  // attaches this matcher — so without this member the kernel scalar would be
  // unreachable under the shipped policy. `bound = 0` is `delta >= 1`.
  uint32_t bound = 0;
  bool accept(uint32_t, uint32_t, int delta) override {
    return delta >= 1 && delta > static_cast<int>(bound);
  }
};

// --- candidate ORDER arms (SCHED-E1/E2) ------------------------------------
// The arms DefaultPolicy's built-in CandidateOrderPolicy offers, one setting
// per rule (TACTIX_ORDER_<RULENAME>). `Native` — the rule's own enumeration
// order — is the shipped default for every rule and stays so until an A/B says
// otherwise; the rest are priced individually, never as a bundle.
//
// Reference evidence, which is the reason these particular arms exist:
//   kissat transitive.c:270-294 sorts its transitive probes by the untried
//     mark FIRST, then (stable) EVSIDS score ascending / (focused) VMTF stamp
//     ascending — the `Heuristic` arm. Our enumeration is literal-index order.
//   cadical probe.cpp:621-624,685,763 ranks probes by noccs(-p) and pops from
//     the BACK, i.e. probes the most-negated-occurring literal first — the
//     `NegOccDesc` arm.
//   cadical elim.cpp:88-121 keeps its elimination schedule in a priority queue
//     keyed on occurrence count, cheapest first — the `OccAsc` arm. (Our BVE
//     enumeration is already occ⁺·occ⁻ ascending, a product where the
//     references use a sum, so the arm is a variation, not a port.)
// `Reverse` and `Random` are controls: the cheapest test that order matters at
// all, and the structureless floor any structured arm has to beat.
enum class CandidateOrder : uint8_t {
  Native,     // the rule's enumeration order, untouched
  Reverse,    // its exact inverse
  Random,     // seeded Fisher-Yates (splitmix64) — replayable, so testable
  OccAsc,     // fewest watch occurrences of the payload's variable first
  OccDesc,    // most first
  NegOccDesc, // most BINARY occurrences of the negated payload literal first
  Heuristic   // stable: EVSIDS score ascending; focused: VMTF stamp ascending
};
const char *candidate_order_name(CandidateOrder);
// Parse an arm name (the env-knob spelling); returns false on an unknown name,
// leaving the output untouched.
bool parse_candidate_order(const char *, CandidateOrder *);

// DefaultPolicy's built-in ranker: one arm per registry rule id, configured
// from the per-rule knobs when the slot is built. Every arm is a permutation
// (see CandidateOrderPolicy) — the scratch is persistent, because a ranking
// pass runs inside the inprocessing dose loop
// (docs: per-call linear scratch is a measured hazard).
class DefaultCandidateOrder final : public CandidateOrderPolicy {
public:
  const char *name() const override { return "per-rule-arms"; }
  void rank(const Kernel &, uint32_t rule, uint64_t *insts,
            uint32_t n) override;
  // `arms[rule]`; sized to the registry when the slot is built.
  std::vector<CandidateOrder> arms;
  CandidateOrder arm_of(uint32_t rule) const {
    return rule < arms.size() ? arms[rule] : CandidateOrder::Native;
  }
  // Per-solve: the Random arm's stream restarts from the seed, so solve n+1
  // draws the same permutations solve 1 did (kissat search.c:70 re-seeds
  // `solver->random` at every search start). `arms` is configuration and
  // `scratch_` is the persistent buffer the ranking pass reuses; neither is
  // per-solve state.
  void reset() { rng_ = SEED; }

private:
  static constexpr uint64_t SEED = 0x9e3779b97f4a7c15ull;
  std::vector<std::pair<uint64_t, uint64_t>> scratch_; // (key, payload)
  uint64_t rng_ = SEED;                                // replayable
};

// --- the two inprocessing CLOCKS (ledger SP-SLOT-1 / SP-SLOT-6) ------------
// Both references keep the probing pipeline and the elimination pipeline on
// INDEPENDENT conflict schedules, and the two diverge over the solve:
//   kissat  probe.c:85     UPDATE_CONFLICT_LIMIT(probe, probings, NLOGN)
//                          with probeint=100
//           eliminate.c:600 UPDATE_CONFLICT_LIMIT(eliminate, eliminations,
//                          NLOG2N) with eliminateint=500
//   cadical probe.cpp:980  delta = 25·inprobeint·log10(inprobingphases+9)
//           elim.cpp:1161  delta = scale(elimint·(elimphases+1))
// Under one clock everything rides the elimination cadence and the probing
// family fires ~5x less often than the references run it, with the gap
// widening. GROUP MEMBERSHIP IS POLICY (N1), like the order: it decides which
// clock offers a rule its occasions, never whether the rule may be applied.
enum class SlotGroup : uint8_t { Probe, Eliminate };

// --- the authored inprocessing phase order (ledger SP-SLOT-4, theme T6) -----
// The order in which DefaultPolicy offers inprocessing occasions is DATA, not
// an emergent property of rule names. ORDER IS A POLICY CHOICE (N1): no rule
// guard and no `instances()` may read this table, and it narrows nothing the
// public API can apply — it fixes only the sequence THIS policy walks.
struct SlotOrderRow {
  const char *name;
  SlotGroup group;
};
// The authored list, in order. Inprocess-tier rules absent from it are still
// scheduled (appended, registry order) so a newly registered rule is never
// silently dropped — but an authored position is what it should get.
const SlotOrderRow *default_slot_order(size_t *count);

class DefaultPolicy final : public Policy {
public:
  // scheduling constants (each exists exactly once; the struct
  // initializers below must use these, never repeat the values).
  // Restart: kissat's focused trigger — bias-corrected per-mode glue
  // EMAs (smooth.c warm-up), margin (100+restartmargin)/100, interval
  // restartint + log10(restarts+9)-1. The constants presuppose the smooth
  // warm-up + per-mode persistence: on plain seeded EMAs they measured
  // WORSE than Glucose's, so port the package, not the numbers. Mode:
  // kissat mode.c / options.h defaults — modeinit (first switch) and
  // modeint (growth factor) are distinct options that happen to share a
  // value.
  static constexpr double RESTART_MARGIN = 1.10;    // restartmargin 10
  static constexpr uint64_t RESTART_INTERVAL = 1;   // restartint
  static constexpr double EMA_FAST_WINDOW = 33;     // emafast
  static constexpr double EMA_SLOW_WINDOW = 1e5;    // emaslow
  static constexpr uint64_t REDUCE_INTERVAL_INIT = 2000;
  static constexpr uint64_t REDUCE_INTERVAL_STEP = 300;
  static constexpr uint64_t REDUCE_INTERVAL_KISSAT = 1000; // reduceint
  static constexpr uint64_t MODE_INITIAL_CONFLICTS = 1000; // modeinit
  static constexpr uint64_t MODE_INTERVAL = 1000;          // modeint
  static constexpr uint64_t RELUCTANT_PERIOD = 1024;    // reluctantint
  static constexpr uint64_t RELUCTANT_LIMIT = 1 << 20;  // reluctantlim
  static constexpr uint64_t REPHASE_INTERVAL_INIT = 1000; // rephaseinit
  static constexpr uint64_t REPHASE_INTERVAL = 1000;      // rephaseint
  static constexpr uint64_t REORDER_INTERVAL_INIT = 10000; // reorderinit
  static constexpr uint64_t REORDER_INTERVAL = 10000;      // reorderint
  // reason-side bumping (kissat bumpreasonslimit/bumpreasonsrate; cadical
  // agrees on the limit and says 100 for the rate — the one constant the
  // references disagree on, hence the sweep knob)
  static constexpr uint32_t BUMP_REASONS_LIMIT = 10;
  // Deviation: 20, neither kissat's 10 nor cadical's 100. Bumping fires when
  // the decision rate is BELOW this, so a larger value fires it in more
  // situations; the whole plateau {15, 20, 25} measured equivalent and clearly
  // better than 10. TACTIX_BUMP_REASONS_RATE re-sweeps it.
  static constexpr double BUMP_REASONS_RATE = 20.0;
  // Inprocessing-slot interval, in the kissat eliminate slot (eliminate.c
  // kissat_eliminating / kimits.h UPDATE_CONFLICT_LIMIT(eliminate, ...,
  // NLOG2N, true)): interval = eliminateint · n·log10(n+9)², init
  // eliminateinit, BOTH scaled by kissat_scale_delta's formula-size factor.
  // The constants presuppose that scaling, exactly as the restart constants
  // presuppose the EMA warm-up.
  static constexpr uint64_t SUBSUME_INTERVAL = 500;      // eliminateint
  // The PROBING clock's base interval — kissat `probeint` (options.h:98),
  // scaled by NLOGN rather than the eliminate clock's NLOG2N (probe.c:85).
  // 100 vs 500 with one fewer log factor is why the probing pipeline runs
  // several times per elimination phase in both references.
  static constexpr uint64_t PROBE_INTERVAL = 100;        // probeint
  // Effort budgets (docs/design/effort-scheduler-design.md). kissat kimits.h
  // SET_EFFORT_LIMIT: a technique's dose is a fixed per-mille fraction of the
  // SEARCH work done since that technique last ran, floored by an absolute
  // minimum — simplification effort is a bounded fraction of search effort,
  // which is self-tuning and pathology-proof. Bounding by APPLICATION COUNT
  // instead is size-blind and does not bound simplification at all.
  // kissat mineffort=10 ("in millions"); our tick unit is within 1.6× of
  // kissat's, so the constant transfers directly.
  static constexpr uint64_t MIN_EFFORT_TICKS = 10000000; // mineffort · 1e6
  // Outer rounds per simplification phase (kissat eliminaterounds): each round
  // runs BVE to its budget + the batch simplifiers (Subsume/Strengthen) once,
  // so the batch rules run a few times per phase, not once per BVE variable.
  static constexpr int INPROCESS_PHASE_ROUNDS = 3;
  // chronological backtracking (kissat learn.c kissat_determine_new_level,
  // options chrono=1/chronolevels=100): jumping over more than this many
  // levels is considered inefficient — backtrack one level instead. A pure
  // policy choice through backjump_apply_to (N1: no rule guard changes).
  // Chrono is only measurable once analyze backtracks to the conflict level
  // first; otherwise the gap that gates it is computed from the wrong level and
  // most chronological backtracks silently drop the conflict as a stale pending
  // clause. TACTIX_CHRONO_LEVELS=N overrides (N=0 = always chronological).
  static constexpr uint32_t CHRONO_LEVELS = 100;
  uint32_t chrono_levels = CHRONO_LEVELS;

  int solve(Kernel &, volatile sig_atomic_t *stop = nullptr) override;

  // --- per-solve reset (SCHED-H2) -------------------------------------------
  // Re-initialise every piece of SCHEDULING state at solve entry, re-basing
  // each limit and anchor on the kernel's CURRENT counters. Both references do
  // this at search entry — kissat search.c:45-70 start_search
  // (init_averages / init_reluctant / init_tiers / kissat_init_limits, plus a
  // re-seeded RNG) with kimits.c:99-136, and cadical internal.cpp:499-608
  // init_search_limits, whose restart / rephase / stabilize / reluctant /
  // elimbound family is reset "in any case". Re-basing (rather than zeroing) is
  // what makes ONE reset serve both a fresh formula and an incremental
  // continuation on the same kernel.
  //
  // WHY IT IS NOT HYGIENE: the §5.6 learned policy runs many solves from one
  // policy object, so state carried from solve n to n+1 makes a trajectory —
  // and therefore any RL reward — a function of episode ORDER. CONFIGURATION
  // (sub-policy pointers, the slot's composition/effort/order fields, the env
  // arms) is deliberately NOT touched: only state a solve derives.
  void reset_schedule(const Kernel &);
  // Escape hatch for the posing surface: the scheduling state below is public
  // precisely so a test or a preset can pose a schedule, and a reset at solve
  // entry would erase it. Clearing this makes solve() honour what it was given.
  // Nothing in the shipped path clears it.
  bool reset_schedule_per_solve = true;
  // Solves this object has run. Reachability readout for the reset (and the
  // thing a multi-solve harness reports); diagnostic only, never read by a
  // scheduling decision.
  uint64_t solves = 0;

  // sub-policies (swappable without touching any rule; null = built-in)
  BranchingPolicy *branching = nullptr;
  ReductionPolicy *reduction = nullptr;
  BveBoundPolicy *bve_bound = nullptr;       // BVE growth-slack ramp
  FactorMatchPolicy *factor_match = nullptr; // Factor grid selection
  // Candidate order for the DOSED slot entries (null = the built-in per-rule
  // arms below). Swapping this is how a preset — or the §5.6 learner — takes
  // over the order in which a rule's instances are tried, with no rule change
  // and no effect on what `apply_instance` accepts (N1).
  CandidateOrderPolicy *candidate_order = nullptr;

  // bias-corrected EMA (kissat smooth.c): value = biased / (1 - beta^n)
  // — the Adam-style warm-up makes early averages meaningful, which the
  // kissat restart constants presuppose
  struct Smooth {
    double value = 0, biased = 0, alpha = 0, beta = 0, exp = 1.0;
    void init(double window) {
      alpha = 1.0 / window;
      beta = 1.0 - alpha;
      value = biased = 0;
      exp = 1.0;
    }
    void update(double y) {
      biased += alpha * (y - biased);
      if (exp) {
        const double old_exp = exp;
        exp *= beta;
        if (exp == old_exp) { // beta^n underflow-stuck: correction done
          exp = 0;
          value = biased;
        } else {
          value = biased / (1.0 - exp);
        }
      } else {
        value = biased;
      }
    }
  };
  // per-mode glue averages (kissat averages.h averages[2]): initialized
  // lazily once per mode, PERSIST across mode switches
  struct ModeAverages {
    bool initialized = false;
    Smooth fast_glue, slow_glue;
    // decisions per conflict, per mode (kissat averages.h decision_rate,
    // window emaslow). Gates reason-side bumping: kissat only pays for it
    // while the search is averaging few decisions per conflict.
    Smooth decision_rate;
    uint64_t saved_decisions = 0;
  };

  // scheduling state — public so tests (and future presets) can pose
  // schedules directly; this is policy state, not kernel H-state
  ModeAverages averages[2]; // indexed by kernel.h->stable
  struct {
    // kissat limits.restart.conflicts: gates BOTH modes; updated after
    // focused restarts and on entering focused, stale-but-kept in stable
    uint64_t next_conflicts = RESTART_INTERVAL;
  } restart;
  struct {
    uint64_t next_conflicts = REDUCE_INTERVAL_INIT;
  } reduce;
  // mode-switch alternation (kissat mode.c/limits): even count = focused
  // (conflicts limit pending), odd = stable (ticks limit pending)
  struct {
    uint64_t count = 0;
    uint64_t conflicts_limit = MODE_INITIAL_CONFLICTS;
    uint64_t ticks_limit = 0;
    uint64_t ticks_at_focus_entry = 0;
  } mode;
  // reluctant doubling (Luby-style; kissat reluctant.c) — stable restarts
  struct {
    uint64_t u = 1, v = 1;
    uint64_t wait = RELUCTANT_PERIOD;
    bool trigger = false;
  } reluctant;
  // rephase schedule (kissat rephase.c: stable-only trigger, interval
  // grows rephaseint·n·log³n; rotation B,W,I,B,W,O — the W slots run Walk)
  struct {
    uint64_t next_conflicts = REPHASE_INTERVAL_INIT;
    // rephase EVENTS (rotation index + interval growth). Distinct from
    // stats.rephased, which counts rephase OPS: a B+W slot is one event and
    // two ops. Identical to stats.rephased on the pre-Walk path.
    uint64_t count = 0;
  } rephase;
  // Reorder schedule (kissat reorder.c/kimits.c: initial `reorderinit`,
  // then LINEAR growth `reorderint · count`). OFF unless TACTIX_REORDER is set
  // — see want_reorder.
  struct {
    uint64_t next_conflicts = REORDER_INTERVAL_INIT;
    uint64_t count = 0;
  } reorder;
  // Walk effort, per mille of the search work since the last walk (floor
  // MIN_EFFORT_TICKS). Deviation: NOT kissat's walkeffort = 50, because the
  // budget is a UNIT MISMATCH there — kissat and we scale from cache-line
  // search ticks but SPEND in per-entry walk steps, while cadical spends in
  // cache-line ticks too, so cadical's walkeffort = 80 buys ~13x more walk
  // scanning than 50 does here (docs/design/s6-walk-design.md §6). 560 is a
  // cadical-equivalent dose, chosen at the low end of a flat plateau; Tier C
  // shows no trend at all across 400..800, so do not read precision into it.
  // "How hard to walk" is a plan §5.6 dosing exhibit — TACTIX_WALK_PERMILLE.
  static constexpr uint64_t WALK_PERMILLE = 560;
  uint64_t walk_anchor = 0; // search_ticks when the last walk ran
  // --- the inprocessing SLOT (registry-driven scheduler) -----------------
  // Schedule-as-data: the slot is a plain vector of {rule id, enabled}
  // entries built from the registry (RuleTier::Inprocess — the tier's
  // reader, §5.3), in registry order (name-sorted, deterministic),
  // executed in order at a quiescent root on the kissat eliminate-slot
  // schedule below. PUBLIC and overridable: tests, presets, and the
  // phase-6 learner pose slots by editing entries — these choice points
  // are the arm space (§5.6). CMS's comma-string simplify schedule is the
  // design precedent for slot-as-ordered-list.
  //
  // The live composition: every Inprocess-tier rule enabled; phases =
  // probing family → simplifiers → dosed BVE (density-gated, deferred out
  // of conflicts=0 — kissat fastel=0) → Factor last; effort-ledger budgets
  // per entry; growing NLOG2N conflict interval. WHY it works: elimination
  // runs on a formula probing already simplified, doses are bounded in each
  // rule's own work currency, and the density gates keep during-search
  // elimination off the sparse instances it measurably harms (plan §5.6
  // elimination-dosing exhibit — the gates are the phase-6 learner's named
  // target). During-search phases are SAT-safe only because they are
  // budgeted and material-gated; the unbudgeted version was not. Every rule
  // stays registered, N1-exposed, and random-policy-fuzzed regardless of
  // its `enabled` bit.
  struct SlotEntry {
    int rule = -1;
    bool enabled = true;
    // Which clock offers this entry its occasions (see SlotGroup). Set from
    // the authored table when the slot is built; a POSED slot chooses its own,
    // and the default puts an unclassified entry on the probing clock.
    SlotGroup group = SlotGroup::Probe;
    // --- effort ledger ----------------------------------------------------
    // `permille` is this entry's share of the search work preceding its dose
    // (kissat options.h NAMEeffort); `meter` reads the rule's OWN work counter
    // — its currency (BVE resolutions, Strengthen candidate checks, ticks for
    // the propagating rules), since one unit of "work" is not comparable
    // across rules. kissat likewise budgets a resolvent count with a delta
    // derived from ticks: the ratio transfers, the unit does not. A null meter
    // means all-or-nothing (Congruence/Factor/Substitute expose only the
    // canonical instance, so there is nothing to titrate).
    uint32_t permille = 0;
    uint64_t (*meter)(const Kernel &) = nullptr;
    // This entry's PRIVATE tick counter inside Kernel::stats, or null when its
    // currency is not ticks (BVE resolutions, Strengthen/SubsumeForward
    // candidate checks, Factor's own factor_ticks, which the rule charges
    // itself). The policy scopes `Kernel::tick_sink` to this around the apply,
    // so kernel-side propagation done ON THIS RULE'S BEHALF lands here and
    // nowhere else.
    uint64_t Stats::*tick_counter = nullptr;
    // cadical's accumulate-until-worthwhile threshold (`opts.NAMEthresh`,
    // limit.hpp:150 — reached by TOKEN-PASTE, which is why it reads as dead
    // code to grep). If this occasion's DELTA is smaller than `thresh` per
    // irredundant clause, decline WITHOUT advancing the anchor, so the
    // entitlement accrues and a later occasion can afford a worthwhile pass.
    // 0 = no threshold. cadical ships factor 7, vivify 20, ternary 6.
    //
    // This is the ONLY budget-based decline a batch entry has: `limit` is
    // re-baselined at the point of use (set_entry_limit), so a `meter > limit`
    // pre-check can never fire for one.
    uint64_t thresh = 0;
    // cadical's `NAMEmaxeff`: an UPPER clamp on this occasion's DELTA, in this
    // entry's own currency (ledger SP-EFFORT-CEIL-1). `thresh` above is the
    // floor half and SCHED-B1 landed it; this is the ceiling half, and the row
    // was miscredited to B1 until SCHED-A3 separated them. 0 = uncapped.
    //
    // What it is FOR: our delta is a fixed per-mille of the search work since
    // this entry last ran, and nothing bounds that from above — an entry the
    // schedule reaches rarely accumulates an arbitrarily large window, and an
    // entry that is never cut makes its candidate ORDER irrelevant (SCHED-E1's
    // premise runs the other way too). `TACTIX_MAXEFF_<RULENAME>=N` overrides,
    // `TACTIX_EFFORT_CEIL=0` disables every ceiling.
    uint64_t max_delta = 0;
    // cadical applies `delta = max(delta, 2*active())` AFTER the ceiling for
    // elim/subsume (`elim.cpp:787`, `subsume.cpp:357`), so a large formula is
    // guaranteed a proportional pass even when the clamp bit. Faithful port:
    // the floor comes second and may lift the delta back above max_delta.
    bool active_floor = false;
    bool dosed = false;   // apply per-instance to a budget vs one canonical round
    bool retry_advice = false; // rule maintains a per-variable retry bit to consult
    // BATCH rule whose canonical apply reads Kernel::work_budget in its own
    // currency and stops mid-round; the policy hands it the remaining ledger
    // budget around r.apply(k) (see EffortSpec in the .cpp).
    bool budget_aware = false;
    // Whole-formula CLOSURE: no instance surface, so its REPETITION is
    // governed by the new-material watermark + the closure delay ladder below
    // rather than by how many candidates a dose may touch. Independent of
    // metering — Congruence carries both since SCHED-G1, and Substitute (no
    // EFFORT_SPECS row at all) reaches the same two declines through the
    // no-row default in the slot build.
    bool closure = false;
    uint64_t anchor = 0;  // search_ticks when this entry last ran (cadical last.NAME.ticks)
    uint64_t limit = 0;   // meter ceiling, recomputed per occasion (set_entry_limit)
    // Defer this entry out of the conflicts==0 preprocessing phase (kissat
    // fastel=0 — elimination only ever runs on a formula probing has already
    // simplified, in dosed during-search phases). BVE only.
    bool defer_preprocess = false;
    // New-material watermark for Factor's arena side. `binaries_mark` alone was
    // exact only while Factor was binary-only; once Kernel::factor_size > 2 its
    // material also includes small LARGE clauses, and a binary-only watermark
    // then silently skips re-runs that had new work to do. Arena size is a
    // conservative proxy: it grows whenever a clause is added, so the entry may
    // re-run needlessly but never wrongly declines.
    uint64_t arena_mark = 0;
    // DIAGNOSTIC cap: stop scheduling this entry after `max_applies` phases
    // (0 = uncapped, the default and the only shipped setting). Exists because
    // a rule's tier damage can be driven by how many ROUNDS it gets rather than
    // by its dose, and a rate change shrinks each round without ever removing
    // one. Set via TACTIX_SLOT_MAX_APPLIES=Name:N. Never read by a guard: this
    // is the POLICY declining to schedule, not the rule declining to apply (N1).
    uint32_t max_applies = 0;
    uint32_t applies = 0;
    // --- resumption cursor (SCHED-E1, generalizing SCHED-B3) -----------------
    // Where this entry's next walk over `instances()` STARTS. An effort budget
    // that binds cuts the walk part-way, and without a cursor the next occasion
    // re-enumerates the same set and replays the same head: the honest meter
    // then buys STARVATION of the tail rather than a bounded pass over the
    // whole set. That was measured on TransitiveReduce (SCHED-B3, where the
    // meter alone banked negative twice), and the mechanism is not specific to
    // it — every DOSED entry is truncated the same way.
    //
    // The value is the KEY (the payload's literal, or the raw payload when it
    // is not a literal encoding) one past the last candidate this entry
    // attempted; the walk resumes at the first candidate whose key is at least
    // that, wrapping to the head when none is. A candidate that has since
    // vanished therefore costs nothing — resumption never depends on the
    // previous element still existing.
    //
    // N1: this moves the START of DefaultPolicy's walk. It is not a
    // precondition, it is not visible to any guard, and `apply_instance` still
    // accepts every payload from every source (tests/test_scheduling.cpp
    // pins exactly that).
    uint64_t cursor = 0;
    // Cursor ON for the entries whose enumeration is in KEY order, where "the
    // first key at or after the cursor" is a meaningful resume point:
    // Probe/Backbone/TransitiveReduce all enumerate ascending by literal. BVE
    // is OFF by default because its schedule is occ⁺·occ⁻-ordered, not
    // key-ordered — a key cursor would rotate it to an arbitrary place — and
    // because it already owns the repetition device the others lack, the
    // per-variable `bve_retry` bit the policy consults. Per-item marks for the
    // rest are Stage F (ledger SP-REPEAT-*). TACTIX_CURSOR_<RULENAME>=0|1.
    bool cursor_enabled = false;
    // The candidate-order arm for this entry (TACTIX_ORDER_<RULENAME>), held
    // here as well as in DefaultCandidateOrder::arms so the `c cand-order`
    // report can name it per entry.
    CandidateOrder order = CandidateOrder::Native;
    // --- telemetry ----------------------------------------------------------
    // Diagnostic only: nothing here is ever READ by a scheduling decision, so
    // the numbers cannot feed back into behaviour. They exist because otherwise
    // a dose experiment cannot distinguish "this entry was STARVED of budget"
    // from "this entry ran and found nothing" — both look like a flat stat.
    struct Telem {
      uint64_t occasions = 0;   // times the slot loop reached this entry
      uint64_t admitted = 0;    // times it got as far as apply
      uint64_t dec_disabled = 0, dec_cap = 0, dec_preprocess = 0;
      uint64_t dec_guard = 0, dec_budget = 0, dec_delay = 0;
      uint64_t dec_material = 0, dec_warmup = 0, dec_density = 0;
      // A decline reason belongs here only while some `continue` increments it.
      // Two more (`oneshot`, `nonviable`) were printed as a permanent `0 0`
      // after the paths that fed them were removed, which reads as "this never
      // declines for that reason" rather than "that reason no longer exists" —
      // the opposite of what this block is for.
      uint64_t handed = 0;      // budget handed to budget-aware entries
      uint64_t spent = 0;       // meter delta actually consumed
      uint64_t ok = 0;          // applies returning Ok
      uint64_t completed = 0;   // rounds that finished inside their budget
      // SP-EFFORT-CEIL-1 reachability: occasions whose delta the UPPER clamp
      // cut, and occasions the `2*active()` floor lifted afterwards. A ceiling
      // that never clips is inert; one that clips EVERY occasion is a dose
      // change wearing a parity costume. Both readings must be reported.
      uint64_t clamped = 0;
      uint64_t floored = 0;
      // Occasions on which an effort subtraction would have gone NEGATIVE and
      // was saturated to zero instead. Every one of these was an unsigned
      // wraparound before, i.e. a budget of ~2^64 handed to a rule that reads
      // it — the ledger silently switching itself off. It must be REPORTED,
      // not merely guarded: a nonzero count means some anchor or ceiling is
      // still being read out of order and the dose it computes is fiction.
      uint64_t wrapped = 0;
    } tm;
    // Candidate-order reachability counters, the pre-registered readout for
    // every E1/E2 arm ([[ab-arm-identical-artefacts]]: an arm whose counter did
    // not move was not tested). Diagnostic only, like Telem.
    struct OrderTelem {
      uint64_t lists = 0;    // instance lists this entry ordered
      uint64_t candidates = 0; // payloads in them
      uint64_t rotated = 0;  // lists the cursor started away from the head —
                             // ZERO means no dose was ever truncated, i.e. the
                             // cursor is inert on this instance
      uint64_t ranked = 0;   // lists the arm actually permuted
      uint64_t moved = 0;    // positions whose payload the arm changed
      uint64_t advanced = 0; // candidates the walk consumed (cursor writes)
    } ord;

    // Per-entry new-material watermarks: an UNMETERED entry (Congruence /
    // Factor / Substitute — full canonical closure per run, nothing to
    // titrate) re-runs in a during-search phase only when the formula gained
    // material it can act on since ITS last run (new binaries / new root-fixed
    // variables). Re-running these closures every phase is a large diffuse
    // Tier C cost, but dropping them entirely loses instances that depend on
    // Congruence — so gate the repetition, keep the rule.
    uint64_t binaries_mark = 0;
    uint64_t root_fixed_mark = 0;
    // Budgeted applies of this entry this solve. Read for the first-dose
    // allowance (Factor's kissat factoriniticks — doses == 0 means the opening
    // round has not run yet).
    uint32_t doses = 0;
    // kissat delay ladder (kimits.c): after an UNPRODUCTIVE run in a
    // during-search phase — kissat vivifyirr criterion, vivified/tried < 1 % —
    // skip this entry's next `delay_current` scheduled occasions (bump:
    // current += 1; productive run: current /= 2; both reset count = current).
    // Backs a technique off the instances where its dose keeps buying nothing
    // while leaving it every-phase where it produces. Policy scheduling state
    // only — the rule stays N1-applicable via the API throughout.
    uint32_t delay_count = 0;
    uint32_t delay_current = 0;
  };
  std::vector<SlotEntry> inprocess_slot; // built lazily at solve() entry
  // Resume (SlotEntry::cursor) then rank (CandidateOrderPolicy) the instance
  // list one occasion will walk. Both are permutations of the list the rule
  // enumerated — see SlotEntry::cursor for the N1 argument. Public alongside
  // the scheduling predicates, so a test (or a preset) can pose an order
  // directly instead of inferring it from a solve.
  void order_candidates(const Kernel &, SlotEntry &, uint64_t *insts,
                        uint32_t n);
  // Compute this entry's meter ceiling at the point it is reached.
  void set_entry_limit(SlotEntry &, Kernel &);
  // One `c slot` line per entry. Diagnostic only — see the .cpp.
  void report_slot(const Kernel &) const;
  // factordelay analog (kissat factor.c:1068-1085,
  // docs/design/s4-factor-dosing-design.md §3.b): count of dosed BVE rounds
  // executed this solve. Factor's entry fires only when log10(active vars) ≤
  // this + 4 — instances up to 10^4 active vars factor from the very first
  // probe; each BVE round buys one more decade.
  uint32_t bve_rounds_run_ = 0;
  // A/B knob (TACTIX_FORCE_MODE, read once per solve): 0 = off,
  // 1 = pin focused (decline every switch), 2 = pin stable (allow only
  // the first switch). Pure scheduling state — SwitchMode stays
  // guard-true and N1-appliable.
  uint32_t force_mode_ = 0;
  // Slot-order arm (TACTIX_SLOT_ORDER=legacy, read once when the slot is
  // built): false = the authored order, true = the pre-authored registry
  // (name-sorted) order with BVE and Factor moved to the back. Reported on the
  // `c slot-order` line so an A/B arm can be shown to have taken effect.
  bool slot_order_legacy_ = false;
  // Closure delay ladder, default ON (TACTIX_NO_CLOSURE_DELAY reverts;
  // read once per solve): delay unproductive unmetered closures (see the
  // slot's unmetered branch).
  bool closure_delay_ = true;
  // Reason-side bumping (DEFAULT ON; TACTIX_NO_BUMP_REASONS reverts, read
  // once per solve). 0 = off; otherwise the per-conflict mark limit handed to
  // ConflictAnalysis. Ships together with dynamic_tiers_ — see the solve()
  // comment for why the pair is one mechanism.
  uint32_t bump_reasons_limit_ = 0;
  double bump_reasons_rate_ = BUMP_REASONS_RATE;
  // Factor's FIRST-DOSE allowance (docs/design/s4-factor-dosing-design.md
  // §3.b). We follow kissat `factoriniticks` 700 (options.h:59, millions); the
  // references disagree — cadical's `factoriniticks` is 300 (options.hpp:123),
  // i.e. 2.3x less room for the opening BVA burst. Arm:
  // TACTIX_FACTOR_INIT_TICKS (raw ticks).
  static constexpr uint64_t FACTOR_INIT_TICKS = 700000000ull;
  uint64_t factor_init_ticks_ = FACTOR_INIT_TICKS;
  // Congruence's OPENING round runs unbudgeted (SCHED-G1). Same shape as
  // Factor's first-dose allowance, and for a measured reason rather than by
  // analogy: the opening round is where essentially all of the rule's yield
  // is (`tierB/hwmcc17miters` — one 2.97 s round, 710,722 merges, instance
  // refuted; `tierC/T105.2.0` — 2,672 of 3,756 merges in round 1 of 17). The
  // ledger's job here is to bound REPETITION. Arm: TACTIX_CONG_INIT_TICKS.
  static constexpr uint64_t CONGRUENCE_INIT_TICKS = 1000000000000ull;
  uint64_t congruence_init_ticks_ = CONGRUENCE_INIT_TICKS;
  // kissat's delay ladder (kimits.c bump/reduce_delay): every round that
  // hits the mark limit doubles the number of conflicts the pass is skipped
  // for, every round that fits halves it. This is what keeps the extra
  // marking (and the wider per-conflict VMTF sort) off instances where it
  // would only cost throughput.
  struct {
    uint32_t count = 0, current = 0;
  } bump_reasons_delay_;
  // Reduce-cadence arm (TACTIX_REDUCE_SQRT, read once per solve): kissat's
  // reduceint·√reductions interval + ramped deletion fraction instead of our
  // arithmetic interval + flat 75 %.
  bool reduce_sqrt_ = false;
  // Dynamic retention tiers (DEFAULT ON; TACTIX_NO_DYNAMIC_TIERS reverts):
  // kissat 4.0 / cadical 3.0 both ship this ON and both use our fixed 2/6
  // only as the no-data fallback.
  bool dynamic_tiers_ = true;
  struct {
    uint64_t next_conflicts = 2;
    uint64_t interval = 2;
  } retier_;
  // During-search BVE bound level (kissat bounds.eliminate.additional_clauses,
  // eliminate.c set_next_elimination_bound): starts 0 (non-growing
  // eliminations only) and DOUBLES 0→1→2→4→8 only when a dose COMPLETES
  // (schedule exhausted within budget). Running the whole 0..8 ramp inside one
  // dose over-eliminates instead. The preprocessing-phase ramp is untouched.
  // Factor's acceptance bound reads this level too (s4-factor-dosing §3.e).
  uint32_t bve_level_ = 0;
  OccIndex inprocess_occ; // shared occurrence index, reused per phase
  // One inprocessing clock. Two live — `probe_clock` and `elim_clock` — on the
  // formulas in update_slot_limit; see SlotGroup for the reference evidence.
  struct SlotClock {
    uint64_t next_conflicts = 0;
    uint64_t rounds = 0; // phases run; also the interval growth key
    // --- the elapsed interval is NECESSARY BUT NOT SUFFICIENT (SCHED-D3) ----
    // Both references gate a phase on more than its conflict interval, and the
    // two clocks are gated differently:
    //   PROBE — cadical `probe.cpp:23`: after the first phase, a REDUCE must
    //   have happened since the last one (`last.inprobe.reductions ==
    //   stats.reductions` declines). Probing re-reads a clause database the
    //   reduce is what turns over.
    //   ELIMINATE — a NEW-MATERIAL watermark: kissat `eliminate.c:33-38`
    //   (`limits.eliminate.variables.{eliminate,subsume}` vs the live counters)
    //   and cadical `elim.cpp:75-80` (`last.elim.fixed < stats.all.fixed ||
    //   last.elim.marked < stats.mark.elim`). Without it a phase can fire
    //   having learned nothing it could act on.
    // Marks are snapshotted at phase ENTRY, so material produced INSIDE a phase
    // re-admits the next one. That is deliberately weaker than kissat's
    // completion-gated advance and equals cadical's restore-on-incomplete case
    // (elim.cpp:774-775/909): it can re-run needlessly, it can never starve a
    // budget-cut pipeline of its follow-up.
    uint64_t reductions_mark = 0; // probe gate: k.stats.reductions at entry
    uint64_t marks_mark = 0;      // elim gate: k.stats.bve_marks at entry
    uint64_t fixed_mark = 0;      // elim gate: kernel_root_fixed at entry
    // DIAGNOSTIC: episodes in which the gate turned an ELAPSED interval down.
    // Counted per episode, not per search step, so it reads as "times the phase
    // was declined". Zero across a tier means the gate is inert and any arm
    // resting on it is vacuous.
    bool blocked = false;
    uint64_t blocked_episodes = 0;
  };
  SlotClock probe_clock, elim_clock;
  // The two Stage-D scheduling mechanisms above are A/B arms like every other
  // one, so each has an off switch; both default to the shipped behaviour and
  // are read once, where the slot is composed.
  //   TACTIX_SLOT_CLOCKS=1  — collapse the split back onto ONE cadence: both
  //   groups keep their own phase, but both fire on the ELIMINATION formula and
  //   on a shared growth key, which is the pre-split rate.
  //   TACTIX_SLOT_WATERMARK=0 — drop the gates, so an elapsed interval is again
  //   sufficient on both clocks.
  bool slot_clocks_split_ = true;
  bool slot_watermark_ = true;
  // Slot timing: the first limit of each clock is size-scaled at solve entry
  // (kissat inits limits after parsing); after that each clock fires on its own
  // growing interval.
  struct {
    bool initialized = false;
    // Ticks spent inside simplification phases. The kernel keeps ONE global
    // propagation meter covering search and probing alike; kissat keeps them
    // apart (search_ticks vs probing_ticks) because every effort budget — and
    // the stable-phase length — is denominated in SEARCH work. Subtracting this
    // accumulator recovers the split policy-side, with no kernel change and no
    // scheduling identity leaking into the rules.
    uint64_t ticks = 0;
    // The accumulator above is credited at phase EXIT, so a reading taken
    // DURING a phase counted that phase's own work as search. It has to be
    // netted out live: an entry's anchor is read mid-phase, and a reading that
    // inflates during the phase and snaps back at its exit is not monotone —
    // the next phase's reading can fall below the anchor, and every effort
    // budget is an UNSIGNED delta from it.
    bool in_phase = false;
    uint64_t phase_entry_ticks = 0;
  } inprocess;
  // search work only (kissat statistics.search_ticks). MONOTONE by
  // construction: no search happens inside a phase, so the reading is frozen
  // for its duration.
  uint64_t search_ticks(const Kernel &k) const {
    const uint64_t net = k.stats.ticks - inprocess.ticks;
    if (!inprocess.in_phase)
      return net;
    const uint64_t in_flight = k.stats.ticks - inprocess.phase_entry_ticks;
    return net > in_flight ? net - in_flight : 0;
  }

  // the scheduling predicates and their transitions, exposed so tests
  // (and future presets) can pose schedules directly
  bool want_restart(const Kernel &) const;
  bool want_reduce(const Kernel &) const;
  bool want_switch_mode(const Kernel &) const;
  bool want_rephase(const Kernel &) const;
  bool want_reorder(const Kernel &) const;
  // One predicate per clock (see SlotGroup). Both are checked in the search
  // loop, probing first — kissat search.c:214-217, cadical internal.cpp:322-325.
  // Non-const: a declined-past-interval episode is counted (SlotClock::blocked).
  bool want_slot_phase(const Kernel &, SlotGroup);
  void tick_reluctant();                   // per-conflict in stable mode
  void update_mode_limits(const Kernel &); // after a switch
  void update_restart_limit(const Kernel &); // after a restart
  void update_reduce_limit(const Kernel &);  // after a reduction
  void update_rephase_limit(const Kernel &); // after a rephase
  void update_reorder_limit(const Kernel &); // after a reorder
  void update_slot_limit(const Kernel &, SlotGroup); // after a slot phase
  // One inprocessing phase on one clock. Returns 20 when the phase refuted the
  // formula, 0 otherwise. PUBLIC for the same reason as the predicates above: a
  // test poses the slot and runs ONE phase, instead of hoping a solve happens to
  // take a macro trigger — that hope is what makes a pose arm vacuous, since an
  // entry never reached reads exactly like an entry correctly declined
  // (tests/test_slot_pose.cpp).
  int run_slot_phase(Kernel &, SlotGroup, volatile sig_atomic_t *stop);

private:
  // --- one inprocessing phase, decomposed ----------------------------------
  // The phase is: enter (quiesce, attach the phase-scoped occurrence index),
  // walk this group's entries for INPROCESS_PHASE_ROUNDS outer rounds, exit
  // (reclaim, account). The per-entry work splits on the axis the effort
  // ledger already draws — DOSED entries, whose instances the policy
  // enumerates and applies one at a time, versus BATCH entries, which run one
  // canonical round metered around the call.
  //
  // Phase-local mutable state, so a helper's effect on the rest of the phase
  // is declared rather than captured. Nothing here is read by a rule.
  struct SlotPhaseCtx {
    SlotGroup group;
    volatile sig_atomic_t *stop;
    int outer = 0;              // current outer round
    bool phase_unsat = false;   // the phase refuted the formula
    bool phase_dosed = false;   // a dosed rule ran: the phase owes a reclaim
    bool round_changed = false; // this outer round changed something
  };
  // What the walk over entries does after an occasion. `Stop` leaves the entry
  // loop (the phase's single exit still runs); `Next` moves to the next entry.
  enum class SlotStep : uint8_t { Next, Stop };

  // Pre-checks reached before this entry's budget is computed; false = declined.
  bool slot_entry_admissible(SlotEntry &, const Kernel &, const SlotPhaseCtx &);
  // cadical's accumulate-until-worthwhile decline (SlotEntry::thresh).
  static uint64_t ledger_remaining(SlotEntry &, const Kernel &, uint64_t limit);
  bool slot_entry_thresh_declines(SlotEntry &, const Kernel &);
  SlotStep run_dosed_entry(Kernel &, SlotEntry &, const RuleVTable &,
                           SlotPhaseCtx &);
  SlotStep run_batch_entry(Kernel &, SlotEntry &, const RuleVTable &,
                           SlotPhaseCtx &);
  // BVE's during-search admission (TACTIX_BVE_ADMIT); true = declined.
  bool bve_dose_declined(Kernel &, SlotEntry &);
  // Factor's warm-up + new-material gates (kissat factordelay); true = declined.
  bool factor_entry_declines(const Kernel &, SlotEntry &);
  // Factor's per-occasion rule config (composition + the density advice).
  void factor_dose_config(Kernel &, SlotEntry &);
  // A budget-aware batch round: the remaining ledger budget handed to the rule.
  ApplyResult run_budgeted_batch(Kernel &, SlotEntry &, const RuleVTable &,
                                 uint64_t limit, bool factor_entry);
  // Vivify's two-pass dose (redundant tiers always, irredundant delay-gated).
  ApplyResult run_vivify_dose(Kernel &, SlotEntry &, const RuleVTable &,
                              uint64_t limit);
  // solve()'s two prologue units, in the order solve() calls them: the env arms
  // it owns, then the inprocessing slot they configure. Separated so the CDCL
  // loop can be read on its own; neither is callable in the other order.
  void configure_from_env();
  void build_inprocess_slot(Kernel &);
  void observe_glue(const Kernel &, uint32_t glue); // per-conflict EMAs
  void observe_decision_rate(const Kernel &);       // before each analysis
  uint32_t want_bump_reasons(const Kernel &);       // the kissat gate ladder
  DefaultBranching default_branching_;
  GlueTieredReduction default_reduction_;
  DefaultBveBoundRamp default_bve_ramp_;
  DefaultFactorMatch default_factor_match_;
  DefaultCandidateOrder default_candidate_order_;
  std::vector<Ref> reduce_scratch_;
  std::vector<uint64_t> inprocess_payloads_; // instance snapshot, reused
  std::vector<uint64_t> order_before_;       // ranking before-image, reused
};

// convenience wrapper: solve under a fresh DefaultPolicy
int default_solve(Kernel &, volatile sig_atomic_t *stop = nullptr);

} // namespace tactix
