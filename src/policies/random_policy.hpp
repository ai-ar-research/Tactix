// RandomPolicy (plan §8.2 rule-order fuzzing): drives the solver purely
// through the rule registry, choosing uniformly at random among the rules
// whose SOUNDNESS guard holds — deliberately ignoring `advised` — and, for
// Decide, branching on a uniformly random unassigned literal with a random
// phase (the full theoretical instance space, not the heuristic ranking).
// Soundness and proof validity must survive any schedule this generates;
// this is the property that distinguishes the architecture (N1).
#pragma once

#include "kernel/kernel.hpp"

#include <csignal>

namespace tactix {

// returns 10 (SAT) / 20 (UNSAT) / 0 (interrupted); deterministic per seed
int random_solve(Kernel &, uint64_t seed,
                 volatile sig_atomic_t *stop = nullptr);

} // namespace tactix
