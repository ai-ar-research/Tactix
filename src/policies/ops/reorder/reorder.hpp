// Reorder (plan §5.2, kissat reorder.c): re-rank the decision heuristic by a
// structural weight read off the current formula rather than by accumulated
// search history. Pure `H`, no proof delta, so a policy operation and not a
// proof rule; sound in any non-refuted state. The instance space is which
// ranking to re-rank, the canonical instance being the one the active mode
// reads. When to reorder is policy state, and it is off by default: ablating
// kissat's own reordering does not resolve above the Tier C noise floor.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

// instance payloads (plan §5.1 v1 encoding for this rule)
constexpr uint64_t REORDER_QUEUE = 0; // VMTF queue: move-to-front in weight order
constexpr uint64_t REORDER_HEAP = 1;  // EVSIDS heap: score += weight

bool reorder_guard(const Kernel &);
ApplyResult reorder_apply_target(Kernel &, uint64_t target);
ApplyResult reorder_apply(Kernel &); // canonical: whichever the active mode reads

// exposed for unit tests: the per-variable structural weight
// max(w+,w-) + 2·min(w+,w-), with each literal weighted 2^-(size-2) per
// irredundant clause it occurs in (binaries counted from the watch lists).
void reorder_weights(const Kernel &, std::vector<double> &var_weight);

} // namespace tactix
