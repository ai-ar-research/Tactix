#pragma once

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

namespace tactix {

// SAT-only model construction for an independent-set formula with a layered
// exact-count encoding (LymphoSAT's independent-set owner): a variable prefix
// of N graph vertices whose edges are negative binary clauses, followed by an
// exact-count structure of sum_{i<=N} (min(i, K) + 1) state variables that
// requires at least K selected vertices, K + 1 being the largest clause size.
// The owner constructs family-specific optima; this strategy finds a K-vertex
// independent set generically (minimum-degree greedy, then swap local search),
// completes the state variables by propagation and validates the whole model.
RuleEligibility independent_set_model_guard(const Kernel &);
ApplyResult independent_set_model_apply(Kernel &);

} // namespace tactix
