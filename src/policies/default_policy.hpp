// DefaultPolicy (plan §5): the statically composed micro-loop — one
// instantiation of the Policy interface, no special status. Calls the
// SAME rule implementations the registry exposes, but through direct
// (inlinable) calls — the "static dispatch" side of the two-tier design.
// Owns every scheduling heuristic it uses (rev 17): the EMA-triggered
// restart schedule (Glucose scheme, Audemard–Simon; constants are the
// published defaults, not benchmark-tuned) and the arithmetic reduce
// interval. Reluctant doubling returns with stable mode (phase 2).
#pragma once

#include "policies/policy.hpp"

namespace tactix {

// canonical branching: the rule/kernel ranking's top pick (VMTF
// last-enqueued-unassigned, saved phase — §5.1)
class DefaultBranching final : public BranchingPolicy {
public:
  Lit pick(Kernel &) override;
};

// canonical reduction volume: kissat's default fraction of the ranked
// worst-first candidates
class GlueTieredReduction final : public ReductionPolicy {
public:
  size_t victims(const Kernel &, size_t candidates) override {
    return (3 * candidates) / 4;
  }
};

class DefaultPolicy final : public Policy {
public:
  int solve(Kernel &, volatile sig_atomic_t *stop = nullptr) override;

  // sub-policies (swappable without touching any rule; null = built-in)
  BranchingPolicy *branching = nullptr;
  ReductionPolicy *reduction = nullptr;

  // scheduling state — public so tests (and future presets) can pose
  // schedules directly; this is policy state, not kernel H-state
  struct {
    uint64_t floor_conflicts = 50; // earliest next restart
    double fast_glue = 0.0;        // ~ last 32 conflicts
    double slow_glue = 0.0;        // ~ last 16384 conflicts
    bool ema_initialized = false;
  } restart;
  struct {
    uint64_t next_conflicts = 2000;
  } reduce;

  // the scheduling predicates, exposed for tests/introspection
  bool want_restart(const Kernel &) const;
  bool want_reduce(const Kernel &) const;

private:
  void observe_glue(uint32_t glue); // per-conflict EMA update
  DefaultBranching default_branching_;
  GlueTieredReduction default_reduction_;
  std::vector<Ref> reduce_scratch_;
};

// convenience wrapper: solve under a fresh DefaultPolicy
int default_solve(Kernel &, volatile sig_atomic_t *stop = nullptr);

} // namespace tactix
