#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// Temporary N1 stress policy (C3, stress-policy skill): maximizes the schedules
// that V6 Congruence's unit channel newly makes legal.
//
// Before C3, V6 could only ADD equivalence binaries: it never assigned, never
// propagated, and had never once returned `Unsat` — so no fuzz case in the
// project's history has exercised a V6 refutation. C3 makes three patterns
// reachable that `--random-policy` draws far too rarely to trust:
//   (1) congruence DERIVES ROOT UNITS, assigns them, and drains the root closure
//       inside its own apply — the rule now mutates the trail;
//   (2) congruence REFUTES, returning Unsat from a rule that previously could not;
//   (3) congruence applied BACK-TO-BACK and OFF A PROPAGATION FIXPOINT, on a
//       formula its own previous round changed (its guard permits this since the
//       D-LEVEL purification; `inprocess_reach_root` drains at entry).
// It also interleaves V3 Substitute, because that is the composition where the
// unit channel bites: Substitute collapses the equivalences congruence added,
// which makes further gates congruent and further XORs cancel.
//
// A verdict is still reached via a simple random DPLL search so the model/proof
// oracles fire. Per-step `kernel_check_invariants` (not the every-256 default)
// pins any culprit. Knobs: TACTIX_CONG_STRESS_ROUNDS (max congruence rounds per
// root visit, default 8), TACTIX_CONG_STRESS_NOSUBST (drop the Substitute
// interleave), TACTIX_TRACE (per-step trace).
int congruence_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
