// Policy interface (plan §5, rev 17 — load-bearing): rule-application
// ORDER is decided by a Policy object; DefaultPolicy is one instantiation
// with no special architectural status. ALL scheduling heuristics and
// their state (restart triggers, reduce intervals, mode switching,
// classification, budgets) are implemented in and owned by the policy —
// rules expose pure soundness guards, instances, apply, and statistics.
// Policies observe kernel state/stats and rule results; rules never push
// scheduling state.
#pragma once

#include "kernel/kernel.hpp"

#include <csignal>

namespace tactix {

class Policy {
public:
  virtual ~Policy() = default;
  // run to a verdict: 10 SAT / 20 UNSAT / 0 (interrupted via *stop)
  virtual int solve(Kernel &, volatile sig_atomic_t *stop = nullptr) = 0;
};

// Sub-policies: complex per-application choices are policy decisions too.
// The rule/kernel supplies the instance space and its RANKING (VMTF order
// for Decide, glue-tiered worst-first order for Reduce — §5.1); the
// sub-policy makes the CHOICE. A Policy composes sub-policies and may
// swap them (presets, MAB arms, external/RL branching) without touching
// any rule.

class BranchingPolicy {
public:
  virtual ~BranchingPolicy() = default;
  // choose the next decision literal; any unassigned literal is a legal
  // instance (N1). Rules validate; choosing never bypasses the guard.
  virtual Lit pick(Kernel &) = 0;
};

class ReductionPolicy {
public:
  virtual ~ReductionPolicy() = default;
  // given the ranked deletion candidates (worst first, soundness-filtered
  // by the rule), return how long a prefix to delete
  virtual size_t victims(const Kernel &, size_t candidates) = 0;
};

} // namespace tactix
