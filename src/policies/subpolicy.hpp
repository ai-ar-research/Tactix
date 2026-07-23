// Sub-policy surface (plan §5/§5.6) — the uniform shape for a swappable scheduling
// heuristic. A `Policy` composes sub-policies and may swap ANY of them without
// touching a rule (presets, MAB arms, external/RL); the phase-6 learner treats each
// as an arm over which it learns. The uniform part is IDENTITY + swappability (and,
// later, a reward hook); the CHOICE signature is category-specific — branching
// returns a decision literal, a bound schedule returns a growth bound — because
// those decisions are genuinely different shapes. This umbrella un-defers Track H ·
// H6's sub-policy super-class now that a 2nd class of arm (the BVE bound ramp) exists.
//
// Existing scheduling that is NOT yet a SubPolicy (restart triggers, mode switching,
// rephase, the inprocessing-slot effort budgets) is reparented incrementally; this
// header is where each lands as it is formalized.
#pragma once

#include "kernel/kernel.hpp"

namespace tactix {

struct SubPolicy {
  virtual ~SubPolicy() = default;
  // a stable name for introspection and the §5.6 arm space (the learner enumerates
  // sub-policies by name, like rules by id).
  virtual const char *name() const = 0;
  // (phase-6 reward hook lands here when the learner does: observe(reward). Kept out
  // now — no consumer yet, §5.5.)
};

// Branching: choose the next decision literal. Any unassigned literal is a legal
// instance (N1); rules validate, choosing never bypasses the guard.
struct BranchingPolicy : SubPolicy {
  virtual Lit pick(Kernel &) = 0;
};

// Reduction: given the ranked worst-first deletion candidates (soundness-filtered by
// the rule), return how long a prefix to delete.
struct ReductionPolicy : SubPolicy {
  virtual size_t victims(const Kernel &, size_t candidates) = 0;
};

// BVE growth-slack schedule (kissat eliminate.c set_next_elimination_bound). A
// preprocessing BVE pass eliminates a variable iff its non-tautological resolvents
// ≤ occ⁺ + occ⁻ + bound; RAMPING the bound over successive passes does the cheapest
// (non-growing) eliminations first, so each is measured against a formula already
// simplified by the cheaper ones — the adaptive, per-instance version of a single
// fixed bound. This dodges the BVE U-shape (too many eliminations triple the
// conflicts on high-variance SAT — L4/crypto1). `num_passes()` levels, `bound(p)` the
// growth slack at pass p (ascending). The policy drives `Kernel::bve_bound` from it.
struct BveBoundPolicy : SubPolicy {
  virtual uint32_t num_passes() const = 0;
  virtual uint32_t bound(uint32_t pass) const = 0;
};

} // namespace tactix
