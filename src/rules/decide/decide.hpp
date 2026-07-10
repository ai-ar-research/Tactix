// M2 Decide (plan §5.2, N1): branching is a first-class API surface.
// instances(σ) = every unassigned literal; the canonical instance is the
// VMTF pick with the saved phase. External policies (script/RL/LLM) may
// apply Decide on ANY unassigned literal at any decision point — this is
// non-negotiable N1 and exists in every build. Proof delta: none (H only).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool decide_guard(const Kernel &);
ApplyResult decide_apply_canonical(Kernel &);

// N1 surface: apply Decide on a chosen literal (any unassigned literal is a
// legal instance). Returns NoOp if `lit` is out of range or assigned.
ApplyResult decide_apply(Kernel &, Lit lit, bool external);

// ranked instance enumeration: fills up to `cap` unassigned literals in
// heuristic order (VMTF recency, saved phase); returns the count
uint32_t decide_candidates(const Kernel &, Lit *out, uint32_t cap);

} // namespace tactix
