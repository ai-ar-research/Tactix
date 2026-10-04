// S5 Lucky (plan §5.2, kissat lucky.c): cheap complete-assignment probes
// before real search — all-true, all-false, and forward/backward
// constant sweeps with unit propagation. May conclude SAT; a first-level
// probe conflict commits the failed-assumption unit by RUP, while an abandoned
// deeper probe retains all such prior root consequences. Phases are never
// touched. The instance space is the PROBE variant; the canonical
// instance is kissat's full cascade. WHEN to probe is policy state
// (DefaultPolicy: once, at search start — kissat luckyearly/luckylate).
//
// Guard: probes are defined from a quiescent state — fully propagated,
// no conflict or pending handoff in flight, at the root. Reaching the
// root is the policy's job (Restart keep-level 0 composes it).
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
// IDs 6--18 and 21 retain their historical SAT-constructor meanings.
// Proof-only parity/exact-one actions use 23/24 so constructor replays never
// acquire UNSAT semantics. Never reuse or redirect a stable identity.
constexpr uint64_t LUCKY_PARITY_PROOF = 23;
constexpr uint64_t LUCKY_EXACT_ONE_PROOF = 24;
constexpr uint32_t LUCKY_INSTANCE_COUNT = 25;
constexpr uint32_t LUCKY_ACTIVE_INSTANCE_COUNT = 25;

constexpr bool lucky_retired_constructor_instance(uint64_t instance) {
  return instance != INSTANCE_CANONICAL && instance >= LUCKY_INSTANCE_COUNT;
}

RuleEligibility lucky_guard(const Kernel &);
ApplyResult lucky_apply_probe(Kernel &, uint64_t probe);
ApplyResult lucky_apply(Kernel &); // canonical: the full cascade

} // namespace tactix
