#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// N1 stress policy for CCE (rule X3); shared contract in stress_registry.hpp.
// CCE's shipped rate is 4 per mille, so DefaultPolicy barely runs it and a
// uniform draw reaches it almost never. As with BCE the governing oracle is the
// model check. Drives whole CCE rounds at every root to a fixpoint, the
// per-literal `apply_instance` surface, CCE off a propagation fixpoint, and an
// interleave with the other witness pushers — CCE alone pushes more than one
// segment per deletion, so cross-rule segment order matters. Knobs:
// TACTIX_CCE_STRESS_{ROUNDS,INSTANCES,SOLO}.
int cce_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
