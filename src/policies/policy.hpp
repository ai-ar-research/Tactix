// Policy interface (plan §5 — load-bearing): rule-application
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

// Sub-policies (BranchingPolicy, ReductionPolicy, BveBoundPolicy, …) — the swappable
// per-decision heuristics a Policy composes — live in policies/subpolicy.hpp.

} // namespace tactix
