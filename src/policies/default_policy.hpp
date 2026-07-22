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

namespace tactix {

// canonical branching: the rule/kernel ranking's top pick (VMTF
// last-enqueued-unassigned, saved phase — §5.1)
class DefaultBranching final : public BranchingPolicy {
public:
  Lit pick(Kernel &) override;
};

// canonical reduction volume: the rule's canonical fraction of the ranked
// worst-first candidates (reduce_canonical_victims — defined rule-side so
// the two cannot drift)
class GlueTieredReduction final : public ReductionPolicy {
public:
  size_t victims(const Kernel &, size_t candidates) override;
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
  // DISABLED for DefaultPolicy (UINT32_MAX) since rev 38: D-LEVEL-2 made the
  // backjump target genuinely chronological (backjump_apply_to no longer
  // clamps to jump_level), so a finite threshold would change DefaultPolicy's
  // trajectory. Keeping it at UINT32_MAX forces target == jump_level, so
  // DefaultPolicy stays bit-exact (s055 = 932,762). Turning on chronological
  // backtracking in DefaultPolicy is a separate, measured go/no-go.
  static constexpr uint32_t CHRONO_LEVELS = UINT32_MAX;

  int solve(Kernel &, volatile sig_atomic_t *stop = nullptr) override;

  // sub-policies (swappable without touching any rule; null = built-in)
  BranchingPolicy *branching = nullptr;
  ReductionPolicy *reduction = nullptr;

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
  ModeAverages averages[2]; // indexed by kernel.stable
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
  // Default composition: Strengthen enabled, SubsumeForward DISABLED —
  // C2's fused round already performs C1's subsumption check on every
  // candidate pair (strengthen.hpp deviation 1); scheduling both would
  // scan the arena twice for the same discoveries. C1 stays registered,
  // N1-exposed, and random-policy-fuzzed.
  //
  // Whether the slot is in the default schedule at all. STILL DEFERRED
  // (re-measured after E1/BVE landed, 2026-07-18 — the trigger this comment
  // used to name; report benchmarks/reports/2026-07-18-e1-slot-activation.md).
  // Verdict: net-negative on Tier B in EVERY composition (full suite, BVE-only)
  // and interval tried (~157 PAR-2 vs baseline 116). The gap is one lost SAT
  // instance — battleship-14-26 solves in 14 s with the slot off, times out
  // with it on, at every interval — because the composed root-restart-to-0
  // that gives the slot a quiescent root derails a productive SAT search (the
  // classic inprocessing-hurts-SAT hazard). The wins are real (circuit 65->9
  // full suite; mod4block 46->5 BVE-only) but BVE alone is NOT the cross-class
  // lever (it doesn't crack the circuit), and two hard UNSAT miters time out in
  // all configs. Activation is a phase-4 item now: SAT-safe inprocessing
  // scheduling (effort budget / stable-mode gate / reconstruct-not-restart) is
  // the blocker, not a knob. Everything below stays N1-exposed and
  // random-policy-fuzzed meanwhile.
  // ACTIVATED (inprocessing-revamp increment 1, docs/inprocessing-revamp-design.md):
  // BVE runs as a SAT-safe bounded simplification phase — preprocessing at
  // conflicts=0 (before search state exists) + rare interval-grown inprocessing,
  // each a bounded fixpoint. Net-positive on Tier B (114 vs 116, battleship-safe).
  // Composition is BVE-only for now; SubsumeForward/Strengthen (increment 3) and
  // Probe/Vivify/Backbone (increment 4) join as the dense OccIndex + tick budgets
  // make them affordable and SAT-safe.
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
    uint64_t anchor = 0;  // search_ticks when this entry last ran (cadical last.NAME.ticks)
    uint64_t limit = 0;   // meter ceiling for the CURRENT phase (set at phase entry)
  };
  std::vector<SlotEntry> inprocess_slot; // built lazily at solve() entry
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
  std::vector<Ref> reduce_scratch_;
  std::vector<uint64_t> inprocess_payloads_; // instance snapshot, reused
};

// convenience wrapper: solve under a fresh DefaultPolicy
int default_solve(Kernel &, volatile sig_atomic_t *stop = nullptr);

} // namespace tactix
