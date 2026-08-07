#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// N1 stress policy for V5 Sweep; shared contract in stress_registry.hpp.
// Targets the general core->chain path S-e, which fires only for an equivalence
// the S-d RUP fast path declines — rare enough (order 8 in 4380 environments)
// that a generic leg emits zero general-path lemmas. Sweeps every root to a
// fixpoint with no effort budget, interleaving Substitute so each round sees
// the formula the previous one changed; TACTIX_SWEEP_FORCE_GENERAL=1 makes S-d
// decline (sweep.cpp) so every proved equivalence routes through S-e. Both are
// legal N1 schedules and neither weakens an oracle. Knobs:
// TACTIX_SWEEP_STRESS_{ROUNDS,NOSUBST}, plus the rule's own TACTIX_SWEEP_*.
int sweep_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
