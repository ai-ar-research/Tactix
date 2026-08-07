// Policy interface (plan §5, load-bearing): rule-application order is decided
// by a Policy object, and DefaultPolicy is one instantiation with no special
// architectural status. Every scheduling heuristic and its state (restart
// triggers, reduce intervals, mode switching, classification, budgets) is owned
// by the policy; rules expose only pure soundness guards, instances, apply and
// statistics. Policies observe kernel state and rule results; rules never push
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

// The swappable per-decision heuristics a Policy composes (BranchingPolicy,
// ReductionPolicy, BveBoundPolicy, ...) live in policies/subpolicy.hpp.

} // namespace tactix
