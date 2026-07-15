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
  // Adaptive reduce interval (glucose Solver.cc reduceDB, MIT,
  // transliterated — independent-lineage campaign experiment 3; absent
  // from kissat/CaDiCaL): when the ranked clause DB looks healthy, the
  // next reduction is postponed by specialIncReduceDB per signal —
  // (a) glue ≤ 3 at the deletion boundary (the worst clause the canonical
  // volume would keep; glucose learnts[size/RATIOREMOVECLAUSES]) and
  // (b) glue ≤ 5 for the best-ranked candidate. Signals are published by
  // the Reduce rule's canonical ranking as stats (reduce.cpp — the §5.6
  // observables pattern; deviations recorded there). The interval reacts
  // to DB quality, not just the reduction count.
  static constexpr uint64_t REDUCE_SPECIAL_INCREMENT = 1000;
  static constexpr uint64_t REDUCE_BOUNDARY_GLUE_HEALTHY = 3;
  static constexpr uint64_t REDUCE_BEST_GLUE_HEALTHY = 5;
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
  // chronological backtracking (kissat learn.c kissat_determine_new_level,
  // options chrono=1/chronolevels=100): jumping over more than this many
  // levels is considered inefficient — backtrack one level instead. A
  // pure policy choice through backjump_apply_to (the freeze's N1 story).
  static constexpr uint32_t CHRONO_LEVELS = 100;

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
  // C1 forward-subsumption schedule (the inprocessing slot); the first
  // limit needs the ingested formula's size, so it is set lazily at
  // solve() entry (kissat inits limits after parsing for the same reason).
  // DEFERRED from the default schedule 2026-07-15 (see PROGRESS): with C1
  // as the slot's only rule, the composed root restarts perturb
  // trajectories without the slot's payoff (kissat's identical rounds are
  // carried by BVE — 49 vars eliminated on the s055 reference), and
  // tr-n300-s055 flipped Tier A at the 30 s line (3× 199/200 after two
  // passes). Re-enable with V1/E1 when the slot earns its restarts; the
  // rule stays fully N1-exposed and random-policy-fuzzed meanwhile.
  static constexpr bool SUBSUME_IN_DEFAULT = false;
  // After the first round, rounds are additionally gated on NEW material
  // (kissat eliminate.c kissat_eliminating's variables trigger): under
  // C1's scope new subsumption relations only come from new binaries or
  // new root-fixed variables — a round without either finds nothing and
  // still costs a full pass plus the composed root restart (measured on
  // tr-n300-s055: 8 rounds, 0 subsumed, +1.5 s solo before the gate)
  struct {
    bool initialized = false;
    bool ran = false; // the initial full round is never gated
    uint64_t next_conflicts = 0;
    uint64_t binaries_seen = 0;
    uint64_t root_fixed_seen = 0;
  } subsume;

  // the scheduling predicates and their transitions, exposed so tests
  // (and future presets) can pose schedules directly
  bool want_restart(const Kernel &) const;
  bool want_reduce(const Kernel &) const;
  bool want_switch_mode(const Kernel &) const;
  bool want_rephase(const Kernel &) const;
  bool want_subsume(const Kernel &) const;
  void tick_reluctant();                   // per-conflict in stable mode
  void update_mode_limits(const Kernel &); // after a switch
  void update_restart_limit(const Kernel &); // after a restart
  void update_reduce_limit(const Kernel &);  // after a reduction
  void update_rephase_limit(const Kernel &); // after a rephase
  void update_subsume_limit(const Kernel &); // after a subsume round

private:
  void observe_glue(const Kernel &, uint32_t glue); // per-conflict EMAs
  DefaultBranching default_branching_;
  GlueTieredReduction default_reduction_;
  std::vector<Ref> reduce_scratch_;
};

// convenience wrapper: solve under a fresh DefaultPolicy
int default_solve(Kernel &, volatile sig_atomic_t *stop = nullptr);

} // namespace tactix
