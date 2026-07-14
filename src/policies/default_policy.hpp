// DefaultPolicy (plan §5): the statically composed micro-loop — one
// instantiation of the Policy interface, no special status. Calls the
// SAME rule implementations the registry exposes, but through direct
// (inlinable) calls — the "static dispatch" side of the two-tier design.
// Owns every scheduling heuristic it uses (rev 17): the focused-mode
// EMA-triggered restart schedule (Glucose scheme, Audemard–Simon), the
// stable-mode reluctant-doubling restart schedule, the arithmetic reduce
// interval, the mode-switch alternation (kissat mode.c: first switch
// after `modeinit` conflicts, stable phases last the preceding focused
// phase's ticks, focused intervals grow as modeint·n·log⁴n), and the
// stable-only rephase schedule (kissat rephase.c: rephaseint·n·log³n
// interval, B,I,B,O rotation). Constants are the published
// kissat/Glucose defaults, not benchmark-tuned.
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
  // Restart: Glucose-style EMA trigger (see default_policy.cpp for the
  // evaluated-and-reverted kissat constants). Mode: kissat mode.c /
  // options.h defaults — modeinit (first switch) and modeint (interval
  // growth factor) are distinct options that happen to share a value.
  static constexpr double RESTART_MARGIN = 1.25;
  static constexpr uint64_t RESTART_FLOOR_INTERVAL = 50;
  static constexpr uint64_t REDUCE_INTERVAL_INIT = 2000;
  static constexpr uint64_t REDUCE_INTERVAL_STEP = 300;
  static constexpr uint64_t MODE_INITIAL_CONFLICTS = 1000; // modeinit
  static constexpr uint64_t MODE_INTERVAL = 1000;          // modeint
  static constexpr uint64_t RELUCTANT_PERIOD = 1024;    // reluctantint
  static constexpr uint64_t RELUCTANT_LIMIT = 1 << 20;  // reluctantlim
  static constexpr uint64_t REPHASE_INTERVAL_INIT = 1000; // rephaseinit
  static constexpr uint64_t REPHASE_INTERVAL = 1000;      // rephaseint

  int solve(Kernel &, volatile sig_atomic_t *stop = nullptr) override;

  // sub-policies (swappable without touching any rule; null = built-in)
  BranchingPolicy *branching = nullptr;
  ReductionPolicy *reduction = nullptr;

  // scheduling state — public so tests (and future presets) can pose
  // schedules directly; this is policy state, not kernel H-state
  struct {
    uint64_t floor_conflicts = RESTART_FLOOR_INTERVAL; // earliest restart
    double fast_glue = 0.0;       // ~ last 32 conflicts
    double slow_glue = 0.0;       // ~ last 16384 conflicts
    bool ema_initialized = false;
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
  // grows rephaseint·n·log³n; rotation B,I,B,O — kissat's B,W,I,B,W,O
  // with the Walking slots deferred to phase 4)
  struct {
    uint64_t next_conflicts = REPHASE_INTERVAL_INIT;
  } rephase;

  // the scheduling predicates and their transitions, exposed so tests
  // (and future presets) can pose schedules directly
  bool want_restart(const Kernel &) const;
  bool want_reduce(const Kernel &) const;
  bool want_switch_mode(const Kernel &) const;
  bool want_rephase(const Kernel &) const;
  void tick_reluctant();                   // per-conflict in stable mode
  void update_mode_limits(const Kernel &); // after a switch
  void update_restart_limit(const Kernel &); // after a restart
  void update_reduce_limit(const Kernel &);  // after a reduction
  void update_rephase_limit(const Kernel &); // after a rephase

private:
  void observe_glue(uint32_t glue); // per-conflict EMA update
  DefaultBranching default_branching_;
  GlueTieredReduction default_reduction_;
  std::vector<Ref> reduce_scratch_;
};

// convenience wrapper: solve under a fresh DefaultPolicy
int default_solve(Kernel &, volatile sig_atomic_t *stop = nullptr);

} // namespace tactix
