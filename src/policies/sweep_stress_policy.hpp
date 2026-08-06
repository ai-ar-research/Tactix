#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// Temporary N1 stress policy (V5 S-e, stress-policy skill): maximizes SWEEP
// EMISSION, and in particular the general core->chain path.
//
// Why this exists. The S-e N2 breach (an unverifiable refutation on
// `asconhashv12`) was live at HEAD while generic fuzz was green on 4000 native
// LRAT and 10000 DRAT cases. The oracle was right and the schedule was never
// reached: the defect is in S-e, the general path, and S-e only fires for an
// equivalence the S-d RUP fast path DECLINES. On the Tier C instance that was
// 1 pair out of 9; on fuzz-scale random formulas the project's own oracle finds
// 8 non-RUP claims in 4380 environments. A leg that emits zero general-path
// lemmas says nothing about the general path, and that is exactly how the
// breach shipped ([[ab-arm-identical-artefacts]]).
//
// So this policy attacks both halves of the reachability problem:
//  (1) SWEEP EVERY ROOT, repeatedly, to a fixpoint, and interleave V3
//      Substitute so each round sees the formula the previous round changed
//      (the Sweep analogue of the C3 congruence stress) — no effort budget is
//      set, so `sweep_apply` runs the whole round;
//  (2) with `TACTIX_SWEEP_FORCE_GENERAL=1`, route EVERY proved equivalence
//      through S-e by making S-d decline (sweep.cpp). That turns "8 in 4380"
//      into "every one of them", which is what makes a green leg mean
//      something. Both settings are legal N1 schedules and neither weakens an
//      oracle: what is emitted still has to be a checkable proof.
//
// A verdict is still reached via a simple random DPLL search so the model and
// proof oracles fire. Per-step `kernel_check_invariants` (not the every-256
// default) pins any culprit. Knobs: TACTIX_SWEEP_STRESS_ROUNDS (max sweep
// rounds per root visit, default 4), TACTIX_SWEEP_STRESS_NOSUBST (drop the
// Substitute interleave), TACTIX_TRACE (per-step trace), plus the rule's own
// TACTIX_SWEEP_* limits and TACTIX_SWEEP_FORCE_GENERAL.
int sweep_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
