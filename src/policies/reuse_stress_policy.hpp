#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// N1 stress policy (TB4b, stress-policy skill): hammers the conflict-reuse
// shortcut and its WATCH REPAIR, the two things `--random-policy` samples far
// too rarely to trust.
//
// Why a dedicated axis. "Exactly one conflict literal at the conflict level" is
// IMPOSSIBLE under complete propagation on a level-sorted trail — if all the
// other literals sat below level M the clause was already unit before M and
// would have propagated. The shape exists only after a CHRONOLOGICAL backjump,
// which keeps out-of-order literals and re-propagates from the level boundary.
// DefaultPolicy reaches it only when the backjump gap exceeds `chrono_levels`
// (shipped 100), and `--random-policy` only when its uniform draw happens to
// pick a high backjump target for ConflictAnalysis AND the resulting trail
// produces such a conflict: measured 0 reuses over 2,105 conflicts on random
// 3-SAT at the shipped setting.
//
// So this policy forces the ingredients: `conflict_reuse` ON regardless of the
// env knob, EVERY backjump chronological (target drawn from the top of the
// legal range), and — because the shortcut's hazard only materialises LATER —
// frequent restarts to random levels, each followed by propagation to a
// fixpoint and a full `kernel_check_invariants`. That last combination is the
// oracle: a conflict clause reused as a reason without its watches repaired is
// left unit at a claimed fixpoint, which is exactly the
// "unit large clause escaped propagation" assertion regression corpus 002 and
// 005 tripped on the first version of the shortcut.
int reuse_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
