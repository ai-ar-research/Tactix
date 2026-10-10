// Sub-policy surface (plan §5, §5.6): the current injection seams for selected
// scheduling heuristics. A policy can choose these before solving without
// touching a rule. Live switching is not implemented: the injected pointers
// are borrowed, and ownership, switch boundaries, and state migration still
// need a policy-layer contract before a learner can treat them as online arms.
// Uniform across these interfaces is identity, not the choice signature.
// Restart, mode, rephase, and inprocessing-slot effort scheduling remain in
// DefaultPolicy until that contract is implemented.
#pragma once

#include "kernel/kernel.hpp"
#include "rules/factor/factor.hpp"

namespace tactix {

struct SubPolicy {
  virtual ~SubPolicy() = default;
  // A stable name for introspection and the §5.6 arm space: the learner
  // enumerates sub-policies by name, as it does rules by id.
  virtual const char *name() const = 0;
  // The phase-6 reward hook lands here with the learner; kept out until there
  // is a consumer (§5.5).
};

// Branching: choose the next decision literal. Any unassigned literal is a
// legal instance (N1); rules validate, choosing never bypasses the guard.
struct BranchingPolicy : SubPolicy {
  virtual Lit pick(Kernel &) = 0;
  // Restart trail reuse asks for the same next-variable policy without first
  // creating a new decision frame.  Kissat's random-sequence admission sees
  // the current level in that context, whereas an ordinary decision sees the
  // level after it was incremented.  Policies without a contextual distinction
  // inherit their ordinary choice.
  virtual Lit pick_for_restart_reuse(Kernel &k) { return pick(k); }
  // Search-propagation conflicts can advance branching schedules differently
  // from conflicts synthesized inside analysis (for example OTFS restarts).
  virtual void on_search_propagation_conflict() {}
};

// Candidate order: in what order to try the instances a rule enumerated for one
// occasion. Under a cutting effort budget the order is the schedule, since the
// budget stops the walk part-way. N1: `rank` is a permutation, never a filter —
// dropping a candidate narrows what this policy can reach, invisibly, since the
// rule would still accept the payload. The resumption cursor
// (DefaultPolicy::SlotEntry::cursor) is the same kind of object: it moves where
// the walk starts, never which candidates exist.
struct CandidateOrderPolicy : SubPolicy {
  // Permute insts[0..n) in place. `rule` is the registry id, so one policy
  // object can serve every entry and the learner can hold one arm per rule.
  virtual void rank(const Kernel &k, uint32_t rule, uint64_t *insts,
                    uint32_t n) = 0;
};

struct ArenaOrderPolicy : SubPolicy, ArenaOrdering {};

// Reduction: given the ranked worst-first deletion candidates, already
// soundness-filtered by the rule, return how long a prefix to delete.
struct ReductionPolicy : SubPolicy {
  virtual size_t victims(const Kernel &, size_t candidates) = 0;
};

// BVE growth-slack schedule (kissat eliminate.c set_next_elimination_bound). A
// pass eliminates a variable iff its non-tautological resolvents are
// <= occ+ + occ- + bound; ramping the bound does the cheapest eliminations
// first, so each later one is measured against an already-simplified formula.
// `bound(p)` is the ascending slack at pass p, and drives `Kernel::bve_bound`.
struct BveBoundPolicy : SubPolicy {
  virtual uint32_t num_passes() const = 0;
  virtual uint32_t bound(uint32_t pass) const = 0;
};

} // namespace tactix
