#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// N1 stress policy for BCE (rule X1); shared contract in stress_registry.hpp.
// The governing oracle is the model check, not the proof check: every checker
// accepts a deletion line unconditionally, so a missing or mis-ordered witness
// segment shows up only as a wrong SAT answer. Drives whole BCE rounds at every
// root to a fixpoint, the per-literal `apply_instance` surface in random order,
// an interleave with the other witness pushers in both orders, and BCE off a
// propagation fixpoint. Knobs: TACTIX_BCE_STRESS_{ROUNDS,INSTANCES,SOLO}.
int bce_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
