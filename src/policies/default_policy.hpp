// DefaultPolicy (plan §5): the statically composed micro-loop — one
// instantiation of the Policy interface, no special status. Calls the
// SAME rule implementations the registry exposes, but through direct
// (inlinable) calls — the "static dispatch" side of the two-tier design.
// Owns every scheduling heuristic it uses (rev 17): the focused-mode
// restart schedule (kissat's bias-corrected per-mode glue EMAs), the
// stable-mode reluctant-doubling restart schedule, the arithmetic reduce
// interval, the mode-switch alternation (kissat mode.c: first switch
// after `modeinit` conflicts, stable phases last the preceding focused
// phase's ticks, focused intervals grow as modeint·n·log⁴n), the
// stable-only rephase schedule (kissat rephase.c: rephaseint·n·log³n
// interval, B,B,I,B,B,O rotation), and the inprocessing slot (kissat
// eliminate.c intervals, size-scaled — C1 SubsumeForward for now; C2/C3/
// E1 join it this phase). Constants are the published kissat/Glucose
// defaults, not benchmark-tuned.
#pragma once

#include "policies/policy.hpp"
#include "policies/subpolicy.hpp"

#include <cstdlib>

namespace tactix {

// canonical branching: the rule/kernel ranking's top pick (VMTF
// last-enqueued-unassigned, saved phase — §5.1)
class DefaultBranching final : public BranchingPolicy {
public:
  const char *name() const override { return "vmtf-evsids"; }
  Lit pick(Kernel &) override;
};

// canonical reduction volume: the rule's canonical fraction of the ranked
// worst-first candidates (reduce_canonical_victims — defined rule-side so
// the two cannot drift)
class GlueTieredReduction final : public ReductionPolicy {
public:
  const char *name() const override { return "glue-tiered"; }
  size_t victims(const Kernel &, size_t candidates) override;
};

// canonical BVE growth-slack ramp: kissat's cheap-first doubling 0,1,2,4,8
// (eliminate.c set_next_elimination_bound), capped at slack-8 for our single
// preprocessing blast — see ceiling(). The first SubPolicy of a NEW category
// (Track S · S1b) — the schedule that dodges the crypto1 BVE U-shape.
class DefaultBveBoundRamp final : public BveBoundPolicy {
public:
  const char *name() const override { return "kissat-ramp"; }
  // Growth-slack ceiling. 8, NOT kissat's 16: kissat reaches slack-16 GRADUALLY
  // across many interleaved eliminate calls and reins in harmful growth with its
  // mode/effort feedback; our BVE runs as one preprocessing blast with no such
  // feedback, so an efficient ramp to 16 over-eliminates straight into the BVE
  // U-shape (crypto1 7→27 s, 71→385 eliminations, Tier B PAR-2 16.7→17.8). Slack-8
  // is where the single blast matches kissat's effective frontier — crypto1's L4
  // 71-elimination sweet spot — giving Tier B PAR-2 15.1, beating both slack-16 and
  // a flat fixed bound=8 (15.9) because the cheap-first ordering still helps.
  // (Before the S1b needed-bound cache the ceiling was moot: the dose cap stopped
  // the ramp near the slack-8 frontier by accident — the cache made the ceiling the
  // real lever.) TACTIX_BVE_RAMP_MAX=N re-sweeps it; Tier C/D validation owed.
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

// canonical Factor grid-selection arm (Track S · S2a): today's watch-list-size
// scoring, bit-exact to the pre-S2a built-in path. Watch sizes are ≤ 2^24, so
// the double→uint32_t round-trip at the rule's integer heap key / tie-break
// comparison is order-exact (no tie flips). accept() stays the FACTOR_MIN_GAIN
// floor — NOT tightened: measured on mp1, kissat's effective accept is ≈
// delta>0 (329 factorings with Δ≤2 vs our 149; design §2b) — the knob exists
// for N1/learning arms, not for the default.
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
  bool accept(uint32_t, uint32_t, int delta) override { return delta >= 1; }
};

// ---- S2a Increment-3: SHELVED experiment arm (env TACTIX_FACTOR_MATCH=paths).
// The mp1 grid-quality A/B (Log 2026-07-24) DISCONFIRMED the tie-break
// steering hypothesis: the 27x27 grid cap is invariant under three tie orders
// (default 1.37M / paths 1.31M / sparse-inverse 2.43M downstream-conflict
// proxy vs kissat-native 305k), so no tie-break ships as default (design §4
// bar: proxy must move materially). `paths` stays shelved for future arms;
// the width gap's live hypothesis is the extension-var composite-grid
// boundary (S2b in PROGRESS). ----

// kissat's structural coherence (factor.c distinct_paths, factorhops=3, its
// off-by-default factorstructural variant): score a tied candidate by the
// number of distinct binary-graph paths (length <= 3) connecting it to the
// chain's seed — cached per (seed, graph generation), one bounded BFS per
// seed instead of per call.
class PathsFactorMatch final : public FactorMatchPolicy {
public:
  const char *name() const override { return "distinct-paths"; }
  double seed_score(const Kernel &k, Lit s) override {
    return k.watches.heads[s].size;
  }
  double next_factor_score(const Kernel &k, Lit c, uint32_t, Lit seed) override;
  bool accept(uint32_t, uint32_t, int delta) override { return delta >= 1; }
  void end_sweep() override { paths_.clear(); cached_seed_ = ~0u; }

private:
  std::vector<uint32_t> paths_; // path counts to the cached seed, by literal
  Lit cached_seed_ = ~0u;
  uint64_t cached_gen_ = ~0ull;
};

class DefaultPolicy final : public Policy {
public:
  // scheduling constants (each exists exactly once; the struct
  // initializers below must use these, never repeat the values).
  // Restart: kissat's focused trigger — bias-corrected per-mode glue
  // EMAs (smooth.c warm-up), margin (100+restartmargin)/100, interval
  // restartint + log10(restarts+9)-1. The earlier Glucose-style plain
  // EMAs (margin 1.25, floor 50) are superseded 2026-07-14: the kissat
  // constants had measured WORSE without the warm-up (see the 07-13
  // note); with smooth warm-up + per-mode persistence they are the
  // faithful package. Mode: kissat mode.c / options.h defaults —
  // modeinit (first switch) and modeint (growth factor) are distinct
  // options that happen to share a value.
  static constexpr double RESTART_MARGIN = 1.10;    // restartmargin 10
  static constexpr uint64_t RESTART_INTERVAL = 1;   // restartint
  static constexpr double EMA_FAST_WINDOW = 33;     // emafast
  static constexpr double EMA_SLOW_WINDOW = 1e5;    // emaslow
  static constexpr uint64_t REDUCE_INTERVAL_INIT = 2000;
  static constexpr uint64_t REDUCE_INTERVAL_STEP = 300;
  static constexpr uint64_t MODE_INITIAL_CONFLICTS = 1000; // modeinit
  static constexpr uint64_t MODE_INTERVAL = 1000;          // modeint
  static constexpr uint64_t RELUCTANT_PERIOD = 1024;    // reluctantint
  static constexpr uint64_t RELUCTANT_LIMIT = 1 << 20;  // reluctantlim
  static constexpr uint64_t REPHASE_INTERVAL_INIT = 1000; // rephaseinit
  static constexpr uint64_t REPHASE_INTERVAL = 1000;      // rephaseint
  // C1 SubsumeForward occupies the kissat eliminate slot (eliminate.c
  // kissat_eliminating / kimits.h UPDATE_CONFLICT_LIMIT(eliminate, ...,
  // NLOG2N, true)) until C2/C3/E1 join it in this phase: interval =
  // eliminateint · n·log10(n+9)², init eliminateinit, BOTH scaled by
  // kissat_scale_delta's formula-size factor — the constants presuppose
  // the scaling exactly like the restart constants presuppose the EMA
  // warm-up (measured lesson, 2026-07-14 carried-items log)
  static constexpr uint64_t SUBSUME_INTERVAL_INIT = 500; // eliminateinit
  static constexpr uint64_t SUBSUME_INTERVAL = 500;      // eliminateint
  // Effort budgets (plan rev 53, docs/design/effort-scheduler-design.md).
  // kissat kimits.h SET_EFFORT_LIMIT: a technique's dose is a fixed per-mille
  // fraction of the SEARCH work done since that technique last ran, floored by
  // an absolute minimum — simplification effort is a bounded fraction of search
  // effort, which is self-tuning and pathology-proof. This REPLACES the former
  // INPROCESS_PHASE_BUDGET application count, which was size-blind (300
  // eliminations on a 1.45 M-variable formula) and whose real job was bounding
  // an O(applications × formula) loop rather than bounding simplification.
  // kissat mineffort=10 ("in millions"); our tick unit is within 1.6× of
  // kissat's (measured: tactix 26.6 M ticks/s vs kissat 42.1 M/s on 9vliw), so
  // the constant transfers directly.
  static constexpr uint64_t MIN_EFFORT_TICKS = 10000000; // mineffort · 1e6
  // Outer rounds per simplification phase (kissat eliminaterounds): each round
  // runs BVE to its budget + the batch simplifiers (Subsume/Strengthen) once,
  // so the batch rules run a few times per phase, not once per BVE variable.
  static constexpr int INPROCESS_PHASE_ROUNDS = 3;
  // chronological backtracking (kissat learn.c kissat_determine_new_level,
  // options chrono=1/chronolevels=100): jumping over more than this many
  // levels is considered inefficient — backtrack one level instead. A pure
  // policy choice through backjump_apply_to (the freeze's N1 story).
  // ENABLED 2026-07-22 (TB4) at kissat's own chronolevels=100, after the
  // go/no-go this comment used to defer. It was measured TWICE. The first run
  // said +13 % WORSE and was a lie: analyze did not backtrack to the conflict
  // level first, so the gap that gates chrono was measured from the wrong level
  // and 84 % of chronological backtracks silently DROPPED the conflict (stale
  // pending clause -> NoOp) and re-derived it, duplicating learned clauses and
  // corrupting the glue EMAs. With that fixed, clean interleaved Tier B, two
  // reps per arm: off 17.35/17.51, on 17.04/16.99 — both on-runs beat both
  // off-runs, mean 17.43 -> 17.02. DefaultPolicy is deliberately no longer
  // bit-exact against the old s055 = 932,762 pin; that pin belonged to the
  // chrono-off trajectory.
  static constexpr uint32_t CHRONO_LEVELS = 100;
  // Runtime override for the T4 go/no-go the comment above asks for:
  // TACTIX_CHRONO_LEVELS=N (kissat's `chronolevels`, default 100; N=0 means
  // always chronological). Unset keeps CHRONO_LEVELS, i.e. today's behaviour,
  // so the default path is untouched until a measurement says otherwise.
  uint32_t chrono_levels = CHRONO_LEVELS;

  int solve(Kernel &, volatile sig_atomic_t *stop = nullptr) override;

  // sub-policies (swappable without touching any rule; null = built-in)
  BranchingPolicy *branching = nullptr;
  ReductionPolicy *reduction = nullptr;
  BveBoundPolicy *bve_bound = nullptr; // BVE growth-slack ramp (Track S · S1b)
  FactorMatchPolicy *factor_match = nullptr; // Factor grid selection (S2a)

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
  // S3 rephase schedule (kissat rephase.c: stable-only trigger, interval
  // grows rephaseint·n·log³n; rotation B,B,I,B,B,O — kissat's B,W,I,B,W,O
  // with the Walking slots deferred to phase 4)
  struct {
    uint64_t next_conflicts = REPHASE_INTERVAL_INIT;
  } rephase;
  // --- the inprocessing SLOT (registry-driven scheduler, rev 26) --------
  // Schedule-as-data: the slot is a plain vector of {rule id, enabled}
  // entries built from the registry (RuleTier::Inprocess — the tier's
  // reader, §5.3), in registry order (name-sorted, deterministic),
  // executed in order at a quiescent root on the kissat eliminate-slot
  // schedule below. PUBLIC and overridable: tests, presets, and the
  // phase-6 learner pose slots by editing entries — these choice points
  // are the arm space (§5.6). Per-rule budgets join when a rule can
  // consume one (rev-20: no fields without readers). CMS's comma-string
  // simplify schedule is the design precedent for slot-as-ordered-list.
  //
  // Whether the slot is in the default schedule at all. ACTIVATED 2026-07-19 by
  // the inprocessing revamp (docs/design/inprocessing-revamp-design.md, report
  // benchmarks/reports/2026-07-19-inprocessing-revamp.md): the slot runs as a
  // SAT-safe bounded simplification phase — preprocessing at conflicts=0 (before
  // search state exists) + rare interval-grown inprocessing, each a bounded
  // fixpoint. Net-positive on Tier B and battleship-safe.
  //
  // WHY that shape: the earlier during-search slot was net-negative in EVERY
  // composition and interval (~157 PAR-2 vs 116), and the whole gap was one lost
  // SAT instance — battleship-14-26 solved in 14 s slot-off and timed out
  // slot-on at every setting, because the composed root-restart-to-0 that gives
  // the slot a quiescent root derails a productive SAT search (the classic
  // inprocessing-hurts-SAT hazard; report
  // benchmarks/reports/2026-07-18-e1-slot-activation.md). Preprocessing at
  // conflicts=0 removes the trajectory to derail. Keep that in mind before
  // moving any technique back into the search loop.
  //
  // Live composition is built in default_policy.cpp (see the comment there):
  // BVE (ordered LAST), Factor, Strengthen, SubsumeForward, Congruence,
  // Substitute enabled; Probe/Vivify/Backbone/TransitiveReduce disabled as
  // SAT-disruptive. Every rule stays registered, N1-exposed, and
  // random-policy-fuzzed regardless of its `enabled` bit.
  static constexpr bool SLOT_IN_DEFAULT = true;
  struct SlotEntry {
    int rule = -1;
    bool enabled = true;
    // --- effort ledger (rev 53) ------------------------------------------
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
    bool dosed = false;   // apply per-instance to a budget vs one canonical round
    bool retry_advice = false; // rule maintains a per-variable retry bit to consult
    // BATCH rule whose canonical apply reads Kernel::work_budget in its own
    // currency and stops mid-round; the policy hands it the remaining ledger
    // budget around r.apply(k) (S4 dosing — see EffortSpec in the .cpp).
    bool budget_aware = false;
    uint64_t anchor = 0;  // search_ticks when this entry last ran (cadical last.NAME.ticks)
    uint64_t limit = 0;   // meter ceiling for the CURRENT phase (set at phase entry)
    // S4 arrangement (TACTIX_ARRANGEMENT): defer this entry out of the
    // conflicts==0 preprocessing phase (kissat fastel=0 — elimination only
    // ever runs on a formula probing has already simplified, in dosed
    // during-search phases).
    bool arrangement_defer = false;
    // Per-entry new-material watermarks (S4 dosing increment 2): an UNMETERED
    // entry (Congruence/Factor/Substitute — full canonical closure per run,
    // nothing to titrate) re-runs in a during-search arrangement phase only
    // when the formula gained material it can act on since ITS last run (new
    // binaries / new root-fixed variables — the same trigger the slot-global
    // gate uses). Attribution probes 2026-07-24: re-running these closures
    // every phase is a top-two diffuse Tier C cost, but removing them
    // entirely breaks urq45 (Congruence is load-bearing) — so gate the
    // repetition, keep the rule.
    uint64_t binaries_mark = 0;
    uint64_t root_fixed_mark = 0;
    // S4 dosing: budgeted applies of this entry this solve. Read for the
    // first-dose allowance (Factor's kissat factoriniticks — doses == 0
    // means the 700M first round has not run yet).
    uint32_t doses = 0;
    // kissat delay (kimits.c, ported for S4 adaptive dosing): after an
    // UNPRODUCTIVE run in a during-search phase — kissat vivifyirr criterion,
    // vivified/tried < 1 % — skip this entry's next `delay_current` scheduled
    // occasions (bump: current += 1; productive run: current /= 2; both reset
    // count = current). Backs a technique off the instances where its dose
    // keeps buying nothing (the arr3 diffuse Tier C residual) while leaving
    // it every-phase where it produces. Policy scheduling state only — the
    // rule stays N1-applicable via the API throughout.
    uint32_t delay_count = 0;
    uint32_t delay_current = 0;
  };
  std::vector<SlotEntry> inprocess_slot; // built lazily at solve() entry
  bool arrangement_ = false; // S4 (TACTIX_ARRANGEMENT); set at slot build
  // S4 factordelay analog (kissat factor.c:1068-1085, design §3.b): count of
  // dosed BVE rounds executed this solve. Factor's entry fires only when
  // log10(active vars) ≤ this + 4 — instances up to 10^4 active vars factor
  // from the very first probe; each BVE round buys one more decade.
  uint32_t bve_rounds_run_ = 0;
  // S4 arrangement BVE bound level (kissat bounds.eliminate.additional_clauses,
  // eliminate.c set_next_elimination_bound): starts 0 (non-growing
  // eliminations only) and DOUBLES 0→1→2→4→8 only when a dose COMPLETES
  // (schedule exhausted within budget). Replaces the all-levels-per-dose S1c
  // ramp in arrangement doses — running 0..8 in the very first dose
  // over-eliminates (mp1: 1,223 eliminated vs kissat 661, conflicts 1.5M→3.5M,
  // the L4 U-shape; probe 1561169). The default preprocessing ramp is
  // untouched. Also Factor's acceptance bound (design §3.e) reads this level.
  uint32_t bve_level_ = 0;
  OccIndex inprocess_occ; // shared occurrence index, reused per phase (incr. 2)
  // Slot timing (kissat eliminate-slot schedule — C1-era machinery kept):
  // first limit size-scaled at solve entry (kissat inits limits after
  // parsing). After the first round, rounds are additionally gated on NEW
  // material (kissat eliminate.c kissat_eliminating's variables trigger):
  // under the slot's current scope new relations only come from new
  // binaries or new root-fixed variables (measured on tr-n300-s055: 8
  // ungated rounds, 0 found, +1.5 s solo before the gate)
  struct {
    bool initialized = false;
    bool ran = false; // the initial full round is never gated
    uint64_t next_conflicts = 0;
    uint64_t rounds = 0; // interval growth key (policy-local, rev 17)
    uint64_t binaries_seen = 0;
    uint64_t root_fixed_seen = 0;
    // Ticks spent inside simplification phases. The kernel keeps ONE global
    // propagation meter (kernel.cpp:426,456) covering search and probing alike;
    // kissat keeps them apart (search_ticks vs probing_ticks) because every
    // effort budget — and the stable-phase length — is denominated in SEARCH
    // work. Subtracting this accumulator recovers the split policy-side, with
    // no kernel change and no scheduling identity leaking into the rules.
    uint64_t ticks = 0;
  } inprocess;
  // search work only (kissat statistics.search_ticks)
  uint64_t search_ticks(const Kernel &k) const {
    return k.stats.ticks - inprocess.ticks;
  }

  // the scheduling predicates and their transitions, exposed so tests
  // (and future presets) can pose schedules directly
  bool want_restart(const Kernel &) const;
  bool want_reduce(const Kernel &) const;
  bool want_switch_mode(const Kernel &) const;
  bool want_rephase(const Kernel &) const;
  bool want_inprocess(const Kernel &) const;
  void tick_reluctant();                   // per-conflict in stable mode
  void update_mode_limits(const Kernel &); // after a switch
  void update_restart_limit(const Kernel &); // after a restart
  void update_reduce_limit(const Kernel &);  // after a reduction
  void update_rephase_limit(const Kernel &); // after a rephase
  void update_inprocess_limit(const Kernel &); // after a slot pass

private:
  void observe_glue(const Kernel &, uint32_t glue); // per-conflict EMAs
  DefaultBranching default_branching_;
  GlueTieredReduction default_reduction_;
  DefaultBveBoundRamp default_bve_ramp_;
  DefaultFactorMatch default_factor_match_;
  PathsFactorMatch paths_factor_match_; // shelved Inc-3 arm (TACTIX_FACTOR_MATCH)
  std::vector<Ref> reduce_scratch_;
  std::vector<uint64_t> inprocess_payloads_; // instance snapshot, reused
};

// convenience wrapper: solve under a fresh DefaultPolicy
int default_solve(Kernel &, volatile sig_atomic_t *stop = nullptr);

} // namespace tactix
