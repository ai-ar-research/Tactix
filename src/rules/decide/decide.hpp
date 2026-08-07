// M2 Decide (plan §5.2, N1): branching is a first-class API surface.
// instances(σ) = every unassigned literal; the canonical instance is the
// active ranking's top pick (focused: VMTF + saved phase; stable: EVSIDS score
// heap + target-then-saved phase). N1: an external policy may apply Decide on
// any unassigned literal at any decision point, in every build.
// Proof delta: none (H only).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool decide_guard(const Kernel &);
ApplyResult decide_apply_canonical(Kernel &);

// N1 surface: apply Decide on a chosen literal (any unassigned literal is a
// legal instance). Returns NoOp if `lit` is out of range or assigned.
ApplyResult decide_apply(Kernel &, Lit lit, bool external);

// ranked instance enumeration: fills up to `cap` unassigned literals in the
// active heuristic's order (VMTF recency / EVSIDS score, with the mode's
// phase priority); returns the count
uint32_t decide_candidates(const Kernel &, Lit *out, uint32_t cap);

} // namespace tactix
