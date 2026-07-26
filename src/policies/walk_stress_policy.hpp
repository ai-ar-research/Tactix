#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// N1 stress policy (Track W · W4, stress-policy skill): hammers S6 Walk on the
// schedules DefaultPolicy never takes. DefaultPolicy only ever walks at a
// quiescent ROOT in stable mode, once per rephase event; the walker is however
// legal at any decision level (it holds trail-assigned literals fixed and
// touches nothing outside `H`), and that is precisely the surface no other
// harness exercises. Here Walk is applied at random decision levels, off
// propagation fixpoints, with random tiny step budgets (including 1, and
// including back-to-back walks with no search in between), while a random DPLL
// reaches the verdict. Per-step kernel_check_invariants; the fuzz oracles then
// check the verdict and the proof, so any walk that corrupted kernel state or
// leaked into the proof shows up. Kept as the durable Walk stress axis
// (probing/bve/congruence/factor precedent).
int walk_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
