#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// N1 stress policy for the probing snapshot/restore scope
// (`kernel_probing_begin`/`end`: watch relocation and `searched` rollback);
// shared contract in stress_registry.hpp. At every root visit it applies the
// scope's consumers in a seed-random order, so open/close cycles interleave
// densely with search that then reuses the restored watch lists.
int probing_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
