// Policy interface (plan §5, load-bearing): rule-application order is decided
// by a Policy object, and DefaultPolicy is one instantiation with no special
// architectural status. Every scheduling heuristic and its state (restart
// triggers, reduce intervals, mode switching, classification, budgets) is owned
// by the policy; rules expose only pure soundness guards, instances, apply and
// statistics. Policies observe kernel state and rule results; rules never push
// scheduling state.
#pragma once

#include "kernel/kernel.hpp"
#include "rules/factor/factor.hpp"

#include <csignal>

namespace tactix {

class Policy {
public:
  virtual ~Policy() = default;
  // run to a verdict: 10 SAT / 20 UNSAT / 0 (interrupted via *stop)
  virtual int solve(Kernel &, volatile sig_atomic_t *stop = nullptr) = 0;
};

// Sub-policy surface (plan §5, §5.6): injection seams for scheduling heuristics
// chosen before solving. Injected pointers are borrowed; live switching needs a
// policy-layer contract for ownership, switch boundaries and state migration.
// Restart, mode, rephase and slot effort scheduling stay in DefaultPolicy.
struct SubPolicy {
  virtual ~SubPolicy() = default;
  // Stable name for introspection and the §5.6 arm space.
  virtual const char *name() const = 0;
  // The phase-6 reward hook lands here with a consumer (§5.5).
};

// Branching: pick the next decision literal; any unassigned one is legal (N1).
struct BranchingPolicy : SubPolicy {
  virtual Lit pick(Kernel &) = 0;
  // Restart trail reuse asks for the next variable without a new decision
  // frame, so Kissat's random-sequence admission sees the current level rather
  // than the incremented one.
  virtual Lit pick_for_restart_reuse(Kernel &k) { return pick(k); }
  // Search conflicts and analysis-synthesized ones may advance schedules
  // differently.
  virtual void on_search_propagation_conflict() {}
};

// Candidate order for a rule's enumerated instances: under a cutting budget the
// order is the schedule. N1: `rank` is a permutation, never a filter; the
// resumption cursor (DefaultPolicy::SlotEntry::cursor) likewise moves only the
// start.
struct CandidateOrderPolicy : SubPolicy {
  // Permute insts[0..n) in place; `rule` is the registry id.
  virtual void rank(const Kernel &k, uint32_t rule, uint64_t *insts,
                    uint32_t n) = 0;
};

struct ArenaOrderPolicy : SubPolicy, ArenaOrdering {};

// Reduction: prefix length of the ranked worst-first candidates to delete.
struct ReductionPolicy : SubPolicy {
  virtual size_t victims(const Kernel &, size_t candidates) = 0;
};

// BVE growth-slack schedule (kissat eliminate.c set_next_elimination_bound): a
// variable is eliminated iff its non-tautological resolvents are <= occ+ + occ-
// + bound, so ramping does the cheapest first. `bound(p)` drives
// `Kernel::bve_bound`.
struct BveBoundPolicy : SubPolicy {
  virtual uint32_t num_passes() const = 0;
  virtual uint32_t bound(uint32_t pass) const = 0;
};

} // namespace tactix
