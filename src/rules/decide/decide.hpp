// M2 Decide (plan §5.2, N1): branching is a first-class API surface.
// instances(σ) = every unassigned literal. The proof rule enumerates a stable
// semantic order and its canonical instance is the lowest-index positive
// literal; policies own ranking and phase selection.
// Proof delta: none (H only).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

RuleEligibility decide_guard(const Kernel &);
ApplyResult decide_apply_canonical(Kernel &);

// N1 surface: apply Decide on a chosen literal (any unassigned literal is a
// legal instance). Returns NoOp if `lit` is out of range or assigned.
ApplyResult decide_apply(Kernel &, Lit lit, bool external);

// Stable semantic enumeration: one positive literal per unassigned variable,
// in variable order. The registry expands each to both legal phases.
uint32_t decide_candidates(const Kernel &, Lit *out, uint32_t cap);

} // namespace tactix
