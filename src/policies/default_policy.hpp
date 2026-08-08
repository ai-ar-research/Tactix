// DefaultPolicy (plan §5): the statically composed micro-loop, one
// instantiation of the Policy interface calling the registry's rule
// implementations through direct (inlinable) calls and owning every scheduling
// heuristic it uses.
#pragma once

#include "policies/policy.hpp"
#include "policies/subpolicy.hpp"
// for ApplyResult (the slot helpers' return type) and RuleVTable's declaration
#include "rules/registry.hpp"

#include <cstdlib>

namespace tactix {

// canonical branching: the ranking's top pick (VMTF, saved phase — §5.1)
class DefaultBranching final : public BranchingPolicy {
public:
  const char *name() const override { return "vmtf-evsids"; }
  Lit pick(Kernel &) override;

  // Random decisions (kissat decide.c `randec`, focused mode only). Pure H
  // policy: no proof delta, no calculus entry. Deviation: the sequence end is
  // an absolute conflict count, needing no conflict-path hook.
  static constexpr uint64_t RANDEC_INIT = 500;   // randecinit
  static constexpr uint64_t RANDEC_LENGTH = 10;  // randeclength
  static constexpr uint64_t RANDEC_SEED = 0x9E3779B97F4A7C15ull;
  uint64_t randec_until = 0;      // conflict count at which the sequence ends
  uint64_t randec_next = RANDEC_INIT; // next conflict count that may start one
  uint64_t randec_sequences = 0;
  uint64_t randec_rng = RANDEC_SEED; // deterministic, seeded fixed
  bool randec_enabled = true; // A/B arm TACTIX_RANDEC=0 (see solve())
  // Per-solve, re-based on the current conflict count (kissat search.c:70,
  // kimits.c:107). `randec_enabled` is configuration and survives.
  void reset(uint64_t conflicts) {
    randec_until = 0;
    randec_next = conflicts + RANDEC_INIT;
    randec_sequences = 0;
    randec_rng = RANDEC_SEED;
  }
};

// canonical reduction volume: the rule's reduce_canonical_victims fraction
class GlueTieredReduction final : public ReductionPolicy {
public:
  const char *name() const override { return "glue-tiered"; }
  size_t victims(const Kernel &, size_t candidates) override;
  // kissat's ramped fraction instead of the flat canonical one, set together
  // with the √-cadence arm.
  bool ramp = false;
};

// canonical BVE growth-slack ramp: kissat's cheap-first doubling 0,1,2,4,8
// (eliminate.c set_next_elimination_bound), capped at slack-8.
class DefaultBveBoundRamp final : public BveBoundPolicy {
public:
  const char *name() const override { return "kissat-ramp"; }
  // Growth-slack ceiling 8, deviating from kissat's 16, which it reaches
  // gradually with feedback a single preprocessing blast lacks.
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

// canonical Factor grid selection: watch-list-size scoring, order-exact through
// the rule's integer heap key. accept() stays the minimum-gain floor and is not
// tightened (docs/design/s2a-factor-matcher-design.md §2b).
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
  // Mirror of Kernel::factor_bound (kissat factor.c:939): DefaultPolicy always
  // attaches this matcher, so without it the kernel scalar is unreachable.
  uint32_t bound = 0; // 0 means `delta >= 1`
  bool accept(uint32_t, uint32_t, int delta) override {
    return delta >= 1 && delta > static_cast<int>(bound);
  }
};

// --- candidate ORDER arms ---------------------------------------------------
// One setting per rule (TACTIX_ORDER_<RULENAME>); `Native` is shipped and arms
// are priced individually. Evidence: kissat transitive.c:270-294, cadical
// probe.cpp:621-624 and elim.cpp:88-121.
enum class CandidateOrder : uint8_t {
  Native,     // the rule's enumeration order, untouched
  Reverse,    // its exact inverse — the cheapest test that order matters
  Random,     // seeded Fisher-Yates (splitmix64) — replayable, so testable
  OccAsc,     // fewest watch occurrences of the payload's variable first
  OccDesc,    // most first
  NegOccDesc, // most BINARY occurrences of the negated payload literal first
  Heuristic   // stable: EVSIDS score ascending; focused: VMTF stamp ascending
};
const char *candidate_order_name(CandidateOrder);
// Parse an arm name; returns false on an unknown one, leaving `out` untouched.
bool parse_candidate_order(const char *, CandidateOrder *);

// The built-in ranker: one arm per registry rule id, every arm a permutation.
// The scratch is persistent because ranking runs inside the dose loop.
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
  // Per-solve: the Random arm restarts from the seed (kissat search.c:70).
  void reset() { rng_ = SEED; }

private:
  static constexpr uint64_t SEED = 0x9e3779b97f4a7c15ull;
  std::vector<std::pair<uint64_t, uint64_t>> scratch_; // (key, payload)
  uint64_t rng_ = SEED;                                // replayable
};

// --- the two inprocessing CLOCKS --------------------------------------------
// Both references keep probing and elimination on independent, diverging
// schedules (kissat probe.c:85 against eliminate.c:600; cadical probe.cpp:980
// against elim.cpp:1161). Group membership is policy (N1): it picks which clock
// offers a rule occasions, never whether it may apply.
enum class SlotGroup : uint8_t { Probe, Eliminate };

// --- the authored inprocessing phase order ----------------------------------
// A policy choice (N1) held as data: no guard and no `instances()` reads it.
struct SlotOrderRow {
  const char *name;
  SlotGroup group;
};
// The authored list; a rule absent from it is appended in registry order.
const SlotOrderRow *default_slot_order(size_t *count);

class DefaultPolicy final : public Policy {
public:
  // Scheduling constants; each exists exactly once and the struct initializers
  // below must use these. Restart: kissat's focused trigger — bias-corrected
  // per-mode glue EMAs (smooth.c), margin (100+restartmargin)/100, interval
  // restartint + log10(restarts+9)-1 — which on plain seeded EMAs measure worse
  // than Glucose's, so port the machinery and not the numbers.
  static constexpr double RESTART_MARGIN = 1.10;    // restartmargin 10
  static constexpr uint64_t RESTART_INTERVAL = 1;   // restartint
  static constexpr double EMA_FAST_WINDOW = 33;     // emafast
  static constexpr double EMA_SLOW_WINDOW = 1e5;    // emaslow
  static constexpr uint64_t REDUCE_INTERVAL_INIT = 2000;
  static constexpr uint64_t REDUCE_INTERVAL_STEP = 300;
  static constexpr uint64_t REDUCE_INTERVAL_KISSAT = 1000; // reduceint
  // kissat mode.c / options.h: modeinit (first switch) and modeint (growth
  // factor) are distinct options that happen to share a value.
  static constexpr uint64_t MODE_INITIAL_CONFLICTS = 1000; // modeinit
  static constexpr uint64_t MODE_INTERVAL = 1000;          // modeint
  static constexpr uint64_t RELUCTANT_PERIOD = 1024;    // reluctantint
  static constexpr uint64_t RELUCTANT_LIMIT = 1 << 20;  // reluctantlim
  static constexpr uint64_t REPHASE_INTERVAL_INIT = 1000; // rephaseinit
  static constexpr uint64_t REPHASE_INTERVAL = 1000;      // rephaseint
  static constexpr uint64_t REORDER_INTERVAL_INIT = 10000; // reorderinit
  static constexpr uint64_t REORDER_INTERVAL = 10000;      // reorderint
  // reason-side bumping (kissat bumpreasonslimit/bumpreasonsrate; cadical says
  // 100 for the rate, hence the sweep knob)
  static constexpr uint32_t BUMP_REASONS_LIMIT = 10;
  // Deviation: 20, neither kissat's 10 nor cadical's 100 — bumping fires below
  // this decision rate, and {15, 20, 25} measured equivalent, beating 10.
  static constexpr double BUMP_REASONS_RATE = 20.0;
  // Eliminate-clock base interval: eliminateint · n·log10(n+9)², size-scaled.
  static constexpr uint64_t SUBSUME_INTERVAL = 500;      // eliminateint
  // Probing-clock base interval — kissat `probeint` (options.h:98), NLOGN
  // rather than NLOG2N, which is why probing runs several times as often.
  static constexpr uint64_t PROBE_INTERVAL = 100;        // probeint
  // Effort budgets (docs/design/effort-scheduler-design.md). kissat kimits.h
  // SET_EFFORT_LIMIT: a dose is a fixed per-mille of the search work since the
  // technique last ran, floored. This tick unit is within 1.6× of kissat's.
  static constexpr uint64_t MIN_EFFORT_TICKS = 10000000; // mineffort · 1e6
  // Outer rounds per simplification phase (kissat eliminaterounds).
  // kissat `proberounds` / `eliminaterounds`, both 2 (options.h:99, :48). The
  // loop is `round != max_rounds` from 0, so the body always runs at least once.
  static constexpr int INPROCESS_PHASE_ROUNDS = 2;
  // chronological backtracking (kissat learn.c, chronolevels=100): backtrack
  // one level rather than jump over more than this many — a policy choice
  // through backjump_apply_to (N1), measurable only once analyze backtracks to
  // the conflict level first.
  static constexpr uint32_t CHRONO_LEVELS = 100;
  uint32_t chrono_levels = CHRONO_LEVELS;
  // cadical `chronoreusetrail` (default 1): back-jump only as far as the
  // best-ranked variable above the jump level, keeping the rest of the trail.
  // `TACTIX_CHRONO_REUSE=0` is the off arm.
  bool chrono_reuse_trail_ = true;

  int solve(Kernel &, volatile sig_atomic_t *stop = nullptr) override;

  // Re-initialise every piece of scheduling state at solve entry, re-basing
  // each limit and anchor on the kernel's current counters (kissat
  // search.c:45-70 with kimits.c:99-136; cadical internal.cpp:499-608), so one
  // reset serves a fresh formula and an incremental continuation alike. A
  // correctness property: the §5.6 learner runs many solves from one object.
  void reset_schedule(const Kernel &);
  // Escape hatch: the state below is public so a test can pose a schedule.
  bool reset_schedule_per_solve = true;
  // Solves this object has run: a diagnostic readout for the reset.
  uint64_t solves = 0;

  // sub-policies (swappable without touching any rule; null = built-in)
  BranchingPolicy *branching = nullptr;
  ReductionPolicy *reduction = nullptr;
  BveBoundPolicy *bve_bound = nullptr;       // BVE growth-slack ramp
  FactorMatchPolicy *factor_match = nullptr; // Factor grid selection
  // Candidate order for the DOSED slot entries (null = the built-in arms): the
  // handle a preset or the §5.6 learner takes the walk order by (N1).
  CandidateOrderPolicy *candidate_order = nullptr;

  // bias-corrected EMA (kissat smooth.c): value = biased / (1 - beta^n), the
  // warm-up the kissat restart constants presuppose
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
  // per-mode glue averages (kissat averages.h averages[2]), initialized lazily
  // and persisting across mode switches
  struct ModeAverages {
    bool initialized = false;
    Smooth fast_glue, slow_glue;
    // decisions per conflict, per mode (kissat averages.h decision_rate); gates
    // reason-side bumping, paid for only at a low decision rate.
    Smooth decision_rate;
    uint64_t saved_decisions = 0;
  };

  // scheduling state — public so tests and presets can pose schedules directly
  ModeAverages averages[2]; // indexed by kernel.h->stable
  struct {
    // kissat limits.restart.conflicts: gates both modes, updated on focused
    // restarts and entry, left stale in stable
    uint64_t next_conflicts = RESTART_INTERVAL;
  } restart;
  struct {
    uint64_t next_conflicts = REDUCE_INTERVAL_INIT;
  } reduce;
  // mode-switch alternation (kissat mode.c): even count = focused, odd = stable
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
  // rephase schedule (kissat rephase.c): stable-only, growing interval,
  // rotation B,W,I,B,W,O where the W slots run Walk
  struct {
    uint64_t next_conflicts = REPHASE_INTERVAL_INIT;
    // rephase events, distinct from stats.rephased: a B+W slot is one event.
    uint64_t count = 0;
  } rephase;
  // Reorder schedule (kissat reorder.c/kimits.c), off unless TACTIX_REORDER.
  struct {
    uint64_t next_conflicts = REORDER_INTERVAL_INIT;
    uint64_t count = 0;
  } reorder;
  // Walk effort, per mille of the search work since the last walk (floor
  // MIN_EFFORT_TICKS). Deviation from kissat's walkeffort=50, a unit mismatch:
  // the budget scales from cache-line ticks but is spent in per-entry walk
  // steps (docs/design/s6-walk-design.md §6). TACTIX_WALK_PERMILLE sweeps it.
  static constexpr uint64_t WALK_PERMILLE = 560;
  uint64_t walk_anchor = 0; // search_ticks when the last walk ran
  // --- the inprocessing SLOT (registry-driven scheduler) ---------------------
  // Schedule-as-data: a vector of {rule id, enabled} entries built from the
  // registry (RuleTier::Inprocess, §5.3), executed in authored order at a
  // quiescent root on the clocks above. Public and overridable — tests, presets
  // and the §5.6 learner pose slots by editing entries. Live composition: every
  // Inprocess-tier rule enabled, probing family → simplifiers → dosed BVE →
  // Factor. During-search phases are SAT-safe only because they are budgeted.
  struct SlotEntry {
    int rule = -1;
    bool enabled = true;
    // Which clock offers this entry its occasions (see SlotGroup), set from the
    // authored table at build; unclassified defaults to probing.
    SlotGroup group = SlotGroup::Probe;
    // Effort ledger: `permille` is this entry's share of the search work
    // preceding its dose (kissat options.h NAMEeffort), and `meter` reads the
    // rule's own work counter, since work is not comparable across rules.
    uint32_t permille = 0;
    uint64_t (*meter)(const Kernel &) = nullptr;
    // This entry's private tick counter inside Kernel::stats, or null when its
    // currency is not ticks. `Kernel::tick_sink` is scoped to it around the
    // apply, so this rule's propagation lands nowhere else.
    uint64_t Stats::*tick_counter = nullptr;
    bool dosed = false;   // apply per-instance to a budget vs one canonical round
    // Batch rule whose apply reads Kernel::work_budget and stops mid-round.
    bool budget_aware = false;
    // Whole-formula closure: repetition is governed by the watermark below.
    bool closure = false;
    uint64_t limit = 0;   // meter ceiling, recomputed per occasion (set_entry_limit)
    // Defer out of the conflicts==0 preprocessing phase (kissat fastel=0).
    bool defer_preprocess = false;
    // New-material watermark for Factor's arena side: `binaries_mark` alone is
    // exact only while Factor is binary-only, and arena size never wrongly
    // declines.
    uint64_t arena_mark = 0;
    // Diagnostic cap: stop scheduling this entry after `max_applies` phases
    // (0 = uncapped, the only shipped setting), since tier damage can be driven
    // by round count. The policy declines to schedule, not the rule (N1).
    uint32_t max_applies = 0;
    uint32_t applies = 0;
    // Where this entry's next walk over `instances()` starts. A binding budget
    // cuts the walk part-way, and without a cursor the next occasion replays
    // the same head, so an honest meter buys starvation of the tail. The value
    // is one past the last candidate attempted. N1: no guard sees it.
    uint64_t cursor = 0;
    // Cursor on for the dosed entries enumerating in key order; BVE is off, its
    // schedule being occ⁺·occ⁻-ordered, and it owns `bve_retry` instead.
    bool cursor_enabled = false;
    // The candidate-order arm (TACTIX_ORDER_<RULENAME>), also in
    // DefaultCandidateOrder::arms so `c cand-order` can name it.
    CandidateOrder order = CandidateOrder::Native;
    // Telemetry, diagnostic only. Without it a dose experiment cannot tell an
    // entry starved of budget from one that ran and found nothing.
    struct Telem {
      uint64_t occasions = 0;   // times the slot loop reached this entry
      uint64_t admitted = 0;    // times it got as far as apply
      uint64_t dec_disabled = 0, dec_cap = 0, dec_preprocess = 0;
      uint64_t dec_guard = 0, dec_budget = 0, dec_delay = 0;
      uint64_t dec_material = 0, dec_warmup = 0;
      // A decline reason belongs here only while a `continue` increments it.
      uint64_t handed = 0;      // budget handed to budget-aware entries
      uint64_t spent = 0;       // meter delta actually consumed
      uint64_t ok = 0;          // applies returning Ok
      uint64_t completed = 0;   // rounds that finished inside their budget
      // Ceiling reachability: never clipping is inert, always clipping is a
      // dose change in disguise.

      // Effort subtractions saturated to zero rather than wrapping to ~2^64; a
      // nonzero count means an anchor is read out of order.
      uint64_t wrapped = 0;
    } tm;
    // Candidate-order reachability: an unmoved counter means an untested arm.
    struct OrderTelem {
      uint64_t lists = 0;    // instance lists this entry ordered
      uint64_t candidates = 0; // payloads in them
      uint64_t rotated = 0;  // lists the cursor started away from the head —
                             // zero means no dose was ever truncated, i.e. the
                             // cursor is inert on this instance
      uint64_t ranked = 0;   // lists the arm actually permuted
      uint64_t moved = 0;    // positions whose payload the arm changed
      uint64_t advanced = 0; // candidates the walk consumed (cursor writes)
    } ord;

    // Per-entry new-material watermarks: an unmetered entry re-runs in a
    // during-search phase only on material it can act on, since repeating those
    // closures every phase is a large diffuse cost.
    uint64_t binaries_mark = 0;
    uint64_t root_fixed_mark = 0;
    // Budgeted applies this solve; read for the first-dose allowance.
    uint32_t doses = 0;
    // kissat delay ladder (kimits.c): after an unproductive during-search run
    // (vivified/tried < 1 %), skip this entry's next `delay_current` occasions;
    // a productive run halves it. Policy state only.
    uint32_t delay_count = 0;
    uint32_t delay_current = 0;
  };
  std::vector<SlotEntry> inprocess_slot; // built lazily at solve() entry
  // Resume (SlotEntry::cursor) then rank (CandidateOrderPolicy) the instance
  // list one occasion walks. Public so a test can pose one.
  void order_candidates(const Kernel &, SlotEntry &, uint64_t *insts,
                        uint32_t n);
  // Compute this entry's meter ceiling at the point it is reached.
  void set_entry_limit(SlotEntry &, Kernel &);
  // One `c slot` line per entry. Diagnostic only — see the .cpp.
  void report_slot(const Kernel &) const;
  // factordelay analog (kissat factor.c:1068-1085, docs/design/
  // s4-factor-dosing-design.md §3.b): Factor fires only when the dosed BVE
  // round count satisfies log10(active vars) ≤ it + 4.
  uint32_t bve_rounds_run_ = 0;
  // TACTIX_FORCE_MODE (read once per solve): 0 = off, 1 = pin focused, 2 = pin
  // stable. SwitchMode stays guard-true and N1-appliable.
  uint32_t force_mode_ = 0;
  // Slot-order arm (TACTIX_SLOT_ORDER=legacy): registry order with BVE and
  // Factor at the back, instead of the authored one.
  bool slot_order_legacy_ = false;
  // Closure delay ladder, default on (TACTIX_NO_CLOSURE_DELAY reverts).
  bool closure_delay_ = true;
  // Reason-side bumping, default on (TACTIX_NO_BUMP_REASONS reverts): the
  // per-conflict mark limit handed to ConflictAnalysis, 0 = off. Ships with
  // dynamic_tiers_; see solve() for why the pair is one mechanism.
  uint32_t bump_reasons_limit_ = 0;
  double bump_reasons_rate_ = BUMP_REASONS_RATE;
  // Factor's first-dose allowance (docs/design/s4-factor-dosing-design.md
  // §3.b): kissat `factoriniticks` 700, where cadical's is 300.
  static constexpr uint64_t FACTOR_INIT_TICKS = 700000000ull;
  uint64_t factor_init_ticks_ = FACTOR_INIT_TICKS;
  // Congruence's opening round runs unbudgeted, like Factor's allowance:
  // essentially all its yield is there, so the ledger bounds repetition.
  static constexpr uint64_t CONGRUENCE_INIT_TICKS = 1000000000000ull;
  uint64_t congruence_init_ticks_ = CONGRUENCE_INIT_TICKS;
  // kissat's delay ladder (kimits.c): a round hitting the mark limit doubles
  // the conflicts skipped, a round that fits halves it.
  struct {
    uint32_t count = 0, current = 0;
  } bump_reasons_delay_;
  // Reduce-cadence arm (TACTIX_REDUCE_SQRT): kissat's reduceint·√reductions
  // interval + ramped deletion, instead of arithmetic and flat 75 %.
  bool reduce_sqrt_ = false;
  // Dynamic retention tiers, default on (TACTIX_NO_DYNAMIC_TIERS reverts): both
  // references ship it on and use a fixed 2/6 only as the no-data fallback.
  bool dynamic_tiers_ = true;
  struct {
    uint64_t next_conflicts = 2;
    uint64_t interval = 2;
  } retier_;
  // During-search BVE bound level (kissat eliminate.c
  // set_next_elimination_bound): 0, doubling only when a dose completes.
  uint32_t bve_level_ = 0;
  uint32_t bve_level_idx_ = 0; // position on the ramp's ladder (drives bve_level_)
  // kissat steps the elimination bound once per phase, so the flag records that
  // a dose ran and the step happens at phase exit.
  bool bve_ran_this_phase_ = false;
  // Factor's acceptance bound for this phase, snapshotted at entry from
  // `bve_level_` (kissat `init_factoring`, factor.c:72). `couple_factor_bound_`
  // ships true; TACTIX_FACTOR_COUPLE=0 is the off arm.
  uint32_t factor_bound_snapshot_ = 0;
  bool couple_factor_bound_ = true;
  OccIndex inprocess_occ; // shared occurrence index, reused per phase
  // One inprocessing clock; two live, on update_slot_limit's formulas.
  struct SlotClock {
    uint64_t next_conflicts = 0;
    uint64_t rounds = 0; // phases run; also the interval growth key
    // The elapsed interval is necessary but not sufficient, and the clocks are
    // gated differently. Probe (cadical probe.cpp:23): a reduce must have
    // happened since the last phase, since probing re-reads a clause database
    // the reduce turns over. Eliminate: a new-material watermark (kissat
    // eliminate.c:33-38, cadical elim.cpp:75-80), snapshotted at phase entry so
    // a budget-cut pipeline is never starved of its own follow-up.
    uint64_t reductions_mark = 0; // probe gate: k.stats.reductions at entry
    uint64_t marks_mark = 0;      // elim gate: k.stats.bve_marks at entry
    uint64_t fixed_mark = 0;      // elim gate: kernel_root_fixed at entry
    // Diagnostic: elapsed intervals the gate turned down; zero across a tier
    // means the gate is inert.
    bool blocked = false;
    uint64_t blocked_episodes = 0;
    // kissat's `last.ticks.probe` / `last.ticks.eliminate` (probe.c:86,
    // eliminate.c:601): search_ticks at this phase's exit, stamped
    // unconditionally and shared by every technique the phase runs, so one
    // phase's techniques all price against the same reference. The scope is
    // load-bearing, not incidental — it is what makes a technique unable to
    // earn or lose allowance by abstaining.
    uint64_t anchor = 0;
  };
  SlotClock probe_clock, elim_clock;
  // One allowance for the forward-subsumption pair (Strengthen and
  // SubsumeForward are two halves of kissat's single `forward` pass), fixed by
  // whichever is priced first in a phase and cleared at phase entry. 0 = unset.
  uint64_t forward_pair_limit_ = 0;
  // Off switches: TACTIX_SLOT_CLOCKS=1 collapses the split onto one cadence,
  // TACTIX_SLOT_WATERMARK=0 drops the gates.
  bool slot_clocks_split_ = true;
  bool slot_watermark_ = true;
  struct {
    bool initialized = false;
  } inprocess;
  // Search work only (kissat `statistics.search_ticks`). The kernel maintains
  // it, charging a tick to it exactly when `Kernel::inprocessing` is false, so
  // it is monotone by construction and a technique's own spend can never
  // re-enter the reference its next budget is a fraction of.
  uint64_t search_ticks(const Kernel &k) const { return k.stats.search_ticks; }

  // the scheduling predicates and their transitions, exposed for posing
  bool want_restart(const Kernel &) const;
  bool want_reduce(const Kernel &) const;
  bool want_switch_mode(const Kernel &) const;
  bool want_rephase(const Kernel &) const;
  bool want_reorder(const Kernel &) const;
  // One predicate per clock (see SlotGroup), both checked in the search loop,
  // probing first (kissat search.c:214-217). Non-const: it counts declines.
  bool want_slot_phase(const Kernel &, SlotGroup);
  void tick_reluctant();                   // per-conflict in stable mode
  void update_mode_limits(const Kernel &); // after a switch
  void update_restart_limit(const Kernel &); // after a restart
  void update_reduce_limit(const Kernel &);  // after a reduction
  void update_rephase_limit(const Kernel &); // after a rephase
  void update_reorder_limit(const Kernel &); // after a reorder
  void update_slot_limit(const Kernel &, SlotGroup); // after a slot phase
  // One inprocessing phase on one clock. Returns 20 when the phase refuted the
  // formula, 0 otherwise. Public so a test can pose the slot and run one phase
  // rather than hope a solve takes a macro trigger — an entry never reached
  // reads exactly like one correctly declined.
  int run_slot_phase(Kernel &, SlotGroup, volatile sig_atomic_t *stop);

private:
  // A phase is: enter (quiesce, attach the phase-scoped occurrence index), walk
  // this group's entries for INPROCESS_PHASE_ROUNDS outer rounds, exit
  // (reclaim, account). SlotPhaseCtx is phase-local state, declaring a helper's
  // effect on the rest of the phase rather than capturing it.
  struct SlotPhaseCtx {
    SlotGroup group;
    volatile sig_atomic_t *stop;
    int outer = 0;              // current outer round
    bool phase_unsat = false;   // the phase refuted the formula
    bool phase_dosed = false;   // a dosed rule ran: the phase owes a reclaim
    bool round_changed = false; // this outer round changed something
  };
  // What the walk does after an occasion: `Stop` leaves the entry loop (the
  // phase's single exit still runs), `Next` moves on.
  enum class SlotStep : uint8_t { Next, Stop };

  // Pre-checks reached before this entry's budget is computed (false =
  // declined).
  bool slot_entry_admissible(SlotEntry &, const Kernel &, const SlotPhaseCtx &);
  static uint64_t ledger_remaining(SlotEntry &, const Kernel &, uint64_t limit);
  SlotStep run_dosed_entry(Kernel &, SlotEntry &, const RuleVTable &,
                           SlotPhaseCtx &);
  SlotStep run_batch_entry(Kernel &, SlotEntry &, const RuleVTable &,
                           SlotPhaseCtx &);
  // Factor's warm-up and new-material gates (kissat factordelay).
  bool factor_entry_declines(const Kernel &, SlotEntry &);
  // Factor's per-occasion rule config (composition + the density advice).
  void factor_dose_config(Kernel &, SlotEntry &);
  // A budget-aware batch round: the remaining ledger budget handed to the rule.
  ApplyResult run_budgeted_batch(Kernel &, SlotEntry &, const RuleVTable &,
                                 uint64_t limit, bool factor_entry);
  // Vivify's two-pass dose (redundant tiers always, irredundant delay-gated).
  ApplyResult run_vivify_dose(Kernel &, SlotEntry &, const RuleVTable &,
                              uint64_t limit);
  // solve()'s two prologue units, in call order: the env arms it owns, then the
  // inprocessing slot they configure. Not callable in the other order.
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
