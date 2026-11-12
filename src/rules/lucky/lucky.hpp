// Lucky (kissat lucky.c): all-true, all-false and forward/backward constant
// sweeps with unit propagation. May conclude SAT; a first-level probe
// conflict commits the failed literal by RUP. Phases are never touched. The
// canonical instance is kissat's full cascade. Guard: a quiescent root state,
// which the policy reaches by Restart.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

// instance payloads (plan §5.1 v1 encoding for this rule)
constexpr uint64_t LUCKY_ALL_TRUE = 0;
constexpr uint64_t LUCKY_ALL_FALSE = 1;
constexpr uint64_t LUCKY_FORWARD_FALSE = 2;
constexpr uint64_t LUCKY_FORWARD_TRUE = 3;
constexpr uint64_t LUCKY_BACKWARD_FALSE = 4;
constexpr uint64_t LUCKY_BACKWARD_TRUE = 5;
constexpr uint64_t LUCKY_STARTUP_PARITY = 19;
constexpr uint64_t LUCKY_STARTUP_COUNT = 20;
constexpr uint64_t LUCKY_GRAPH_LABELING = 21;
constexpr uint64_t LUCKY_STARTUP_RAMSEY = 22;
// IDs 6-14, 17, 18 and 21 are SAT constructors; 15 and 16 are proof-capable;
// proof-only actions use 23/24. Never reuse or redirect a stable identity.
constexpr uint64_t LUCKY_PARITY_PROOF = 23;
constexpr uint64_t LUCKY_EXACT_ONE_PROOF = 24;
// Retired identity, kept reserved so old programs are rejected.
constexpr uint64_t LUCKY_NOL_HYPERGRAPH = 25;
constexpr uint64_t LUCKY_GRAPH_AUTHENTICATE = 26;
constexpr uint64_t LUCKY_GRAPH_CONTINUE = 27;
constexpr bool lucky_graph_instance(uint64_t instance) {
  return instance == LUCKY_GRAPH_LABELING || instance == LUCKY_GRAPH_AUTHENTICATE ||
         instance == LUCKY_GRAPH_CONTINUE;
}
constexpr uint32_t LUCKY_INSTANCE_COUNT = 28;
constexpr uint32_t LUCKY_ACTIVE_INSTANCE_COUNT = 27;

constexpr bool lucky_retired_constructor_instance(uint64_t instance) {
  return instance != INSTANCE_CANONICAL &&
         (instance == LUCKY_NOL_HYPERGRAPH || instance >= LUCKY_INSTANCE_COUNT);
}

RuleEligibility lucky_guard(const Kernel &);
ApplyResult lucky_apply_probe(Kernel &, uint64_t probe);
ApplyResult lucky_apply(Kernel &); // canonical: the full cascade

} // namespace tactix
