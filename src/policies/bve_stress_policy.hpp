#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// Temporary N1 stress policy (E1c, stress-policy skill): maximizes BVE at the
// root — before every decision it eliminates every schedulable variable it can,
// so gate detection + resolution + reconstruction run FAR more often per instance
// than the uniform `--random-policy` samples (which applies BVE only occasionally).
// A verdict is still reached via a simple random DPLL search so the model/proof
// oracles fire on the post-elimination formula. Per-step `kernel_check_invariants`
// (not the every-256 default) pins any culprit. The reusable analogue of
// AdversarialPolicy (off-fixpoint decide) for the inprocessing/elimination axis.
int bve_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
