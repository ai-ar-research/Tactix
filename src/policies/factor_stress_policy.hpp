#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// TEMPORARY N1 stress policy (S2a Increment 2, stress-policy skill): attaches
// an ADVERSARIAL FactorMatchPolicy — scores uncorrelated with watch sizes
// (seeded hash / all-tied / reversed, drawn from the seed) and the maximizing
// accept ≡ delta≥1 — then maximizes Factor at the root, so grid growth is
// steered over seed/tie-break chains DefaultFactorMatch would never produce.
// Every resulting (F,R) must still yield a sound, checker-valid proof
// (apply_factoring is grid-agnostic — that is the property under test).
// Verdicts are reached via a random DPLL search so the model/proof oracles
// fire; per-step kernel_check_invariants (not the every-256 default).
// Delete after Increment 2 unless kept as a durable stress axis.
int factor_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
