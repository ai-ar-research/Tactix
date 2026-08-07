#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// N1 stress policy for V6 Congruence; shared contract in stress_registry.hpp.
// Targets the schedules the unit channel makes legal: congruence deriving root
// units and draining the root closure inside its own apply, so the rule mutates
// the trail; congruence returning Unsat; and congruence back to back off a
// propagation fixpoint on a formula its own previous round changed, legal since
// the D-LEVEL purification. Interleaves Substitute, where the unit channel
// bites. Knobs: TACTIX_CONG_STRESS_{ROUNDS,NOSUBST,BUDGET}.
int congruence_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
