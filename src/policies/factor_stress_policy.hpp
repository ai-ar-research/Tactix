#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// N1 stress policy for S2 Factor; shared contract in stress_registry.hpp.
// Attaches an adversarial FactorMatchPolicy — scores uncorrelated with watch
// sizes and the maximizing accept rule delta >= 1 — then maximizes Factor at
// the root, so grid growth follows tie-break chains DefaultFactorMatch would
// never produce. Under test: `apply_factoring` is grid-agnostic, so every
// resulting (F,R) must still yield a checker-valid proof.
int factor_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
