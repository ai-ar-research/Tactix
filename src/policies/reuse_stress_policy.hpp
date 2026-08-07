#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// N1 stress policy for the conflict-reuse shortcut and its watch repair;
// shared contract in stress_registry.hpp. The shortcut's precondition — exactly
// one conflict literal at the conflict level — is impossible under complete
// propagation on a level-sorted trail, so it exists only after a chronological
// backjump, which neither DefaultPolicy nor a uniform draw reaches usefully.
// This forces `conflict_reuse` on, makes every backjump chronological, and
// restarts often to random levels, each followed by propagation to a fixpoint
// and a full `kernel_check_invariants` — the oracle, since a conflict clause
// reused as a reason without its watches repaired is left unit at a claimed
// fixpoint (regression corpus 002 and 005).
int reuse_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
