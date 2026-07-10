// DefaultPolicy (plan §5): the statically composed micro-loop. Calls the
// SAME rule implementations the registry exposes, but through direct
// (inlinable) calls — this is the "static dispatch" side of the two-tier
// design. Policies own order only; no rule calls another rule.
#pragma once

#include "kernel/kernel.hpp"

#include <csignal>

namespace tactix {

// returns 10 (SAT) / 20 (UNSAT) / 0 (interrupted via *stop)
int default_solve(Kernel &, volatile sig_atomic_t *stop = nullptr);

} // namespace tactix
