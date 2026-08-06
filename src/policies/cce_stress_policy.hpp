#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// Temporary N1 stress policy (X3 CCE R6, stress-policy skill): maximizes
// COVERED CLAUSE ELIMINATION, and in particular the WITNESSED exits.
//
// Why this exists. CCE's shipped rate is 4 per mille — cadical's `covereffort`,
// the lowest number in its option set — so under DefaultPolicy the rule barely
// runs, and `--random-policy` draws one rule of two dozen uniformly. A green
// fuzz leg over zero CCE eliminations is vacuous ([[ab-arm-identical-artefacts]]),
// and the failure mode this rule can produce is not a rejected proof but a WRONG
// SAT ANSWER: the deletion is only satisfiability-preserving, so a mis-ordered or
// mis-scoped witness segment silently corrupts model reconstruction.
//
// The three schedules it drives, all legal, none reached often by chance:
//  (1) CCE AT EVERY ROOT, to a fixpoint, with `work_budget` left at 0 so each
//      apply runs the WHOLE round. A round's deletions change which clauses the
//      next round's ALA and CLA may quantify over, which is exactly the
//      invariant the ALA-closure lemma rests on.
//  (2) INTERLEAVED WITH THE OTHER WITNESS-PUSHING RULES — X1 BCE and E1 BVE —
//      so `ρ` carries CCE's segment sequences stacked between other rules'
//      segments. CCE is the only rule here that pushes MORE THAN ONE segment
//      per deletion, so segment ORDER across rules is a live question.
//  (3) THE PER-LITERAL INSTANCE SURFACE, drawn at random, not just the
//      canonical round: `apply_instance` is the N1 path a learned policy will
//      use, and it enters through `inprocess_reach_root` where the round does
//      not.
// It also enters CCE OFF A PROPAGATION FIXPOINT (the branch sits before the
// propagate branch), which the guard permits and DefaultPolicy never does.
//
// A verdict is still reached via a simple random DPLL search so the model and
// proof oracles fire; conflict analysis runs, so redundant clauses exist for the
// CLA side to quantify over. Per-step `kernel_check_invariants` (not the
// every-256 default) pins any culprit. Knobs: TACTIX_CCE_STRESS_ROUNDS (max CCE
// rounds per root visit, default 4), TACTIX_CCE_STRESS_SOLO (drop the BCE/BVE
// interleave), TACTIX_CCE_STRESS_INSTANCES (per-literal instance draws per root
// visit, default 3), TACTIX_TRACE (per-step trace).
int cce_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
