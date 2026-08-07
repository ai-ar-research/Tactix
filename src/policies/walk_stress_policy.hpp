#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// N1 stress policy for S6 Walk; shared contract in stress_registry.hpp.
// DefaultPolicy only walks at a quiescent stable root, but the walker is legal
// at any decision level, holding trail-assigned literals fixed and touching
// nothing outside `H`. This covers that surface: Walk at random decision
// levels, off propagation fixpoints, with tiny step budgets and back-to-back
// walks with no search between them.
int walk_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
