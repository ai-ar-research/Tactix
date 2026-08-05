// S7 Reorder (plan §5.2, kissat reorder.c): re-rank the decision heuristic by a
// STRUCTURAL weight read off the current formula, instead of by the search
// history EVSIDS/VMTF accumulated. Pure H (proof delta: none), so it is a policy
// operation, not a proof rule — no calculus entry (calculus.md "what belongs in
// this document"). Sound in any non-refuted state.
//
// The instance space is WHICH ranking to re-rank; the canonical instance is the
// one the active mode reads (heap while stable, queue while focused), so a
// policy that just wants "reorder now" gets kissat's behaviour. WHEN to reorder
// is policy state (rev 17): DefaultPolicy carries kissat's linearly-growing
// conflict interval, and it is OFF by default (Track R convention; ablating
// kissat's own `--reorder=0` moved Tier C −0.67 PAR-2 — a non-detection inside the
// 6.72 noise floor rather than a measured zero).
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
