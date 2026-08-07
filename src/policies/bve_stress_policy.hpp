#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// N1 stress policy for BVE (rule E1); contract in stress_registry.hpp. Before
// every decision it eliminates every schedulable variable it can, so gate
// detection, resolution and reconstruction run far more often per instance than
// a uniform draw reaches them.
int bve_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
