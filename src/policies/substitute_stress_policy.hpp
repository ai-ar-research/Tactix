#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// Temporary N1 stress policy (V3 Substitute, stress-policy skill): maximizes
// EQUIVALENT-LITERAL SUBSTITUTION — its rewrite, its witness pair per
// eliminated variable, and its x ≡ ¬x refutation.
//
// Why this exists. Substitute is the second witness-emitting rule with no
// targeted driver (`RV-S22` in
// benchmarks/reports/2026-08-06-review-post-track-sched.md), and it is the
// heavier of the two: per eliminated variable it pushes a witness PAIR, and its
// rewrite also ADDS clauses (transient equivalence lemmas justify each rewrite
// in LRAT), so both oracles are live — a wrong witness is a wrong SAT answer, a
// wrong lemma or chain is a rejected proof. In the shipped schedule it only ever
// runs on the binary implication graph DefaultPolicy hands it; the two other
// stress policies that mention it (`congruence_stress`, `sweep_stress`) only
// ever turn it OFF.
//
// The schedules it drives, all legal, none reached often by chance:
//  (1) SUBSTITUTE AT EVERY ROOT, to a fixpoint. A round's rewrite changes the
//      very graph the next round's Tarjan walks, and collapsed clauses can
//      produce root units that must be asserted un-propagated.
//  (2) THE PER-SCC INSTANCE SURFACE, drawn from the rule's own `instances()`
//      enumeration — one representative at a time, in random order, rather than
//      the whole round. `apply_instance` re-derives the SCCs from the CURRENT
//      graph, so a representative enumerated before an earlier instance's
//      rewrite may no longer be one: that stale-payload path is reachable only
//      this way, and it is the path a learned policy will use.
//  (3) INTERLEAVED WITH THE RULES THAT REWRITE THE BINARY IMPLICATION GRAPH
//      UNDER IT — V6 Congruence and Sweep ADD equivalence binaries (new SCCs),
//      V4 TransitiveReduce DELETES binaries (splits them) — and with the other
//      witness pushers, X1 BCE and E1 BVE, so ρ interleaves witness pairs with
//      single segments in both orders.
//  (4) SUBSTITUTE OFF A PROPAGATION FIXPOINT (the root branch sits before the
//      propagate branch): its guard purification means `apply` must complete the
//      root closure itself, and a root conflict during that drain refutes.
//
// Verdicts come from a random DPLL search with real conflict analysis, so the
// model oracle fires on SAT and the learned clauses give the rewrite large
// redundant clauses to touch. Per-step `kernel_check_invariants` (not the
// every-256 default) pins any culprit. Knobs:
// TACTIX_SUBSTITUTE_STRESS_ROUNDS (canonical rounds per root visit, default 4),
// TACTIX_SUBSTITUTE_STRESS_INSTANCES (per-representative draws per root visit,
// default 4), TACTIX_SUBSTITUTE_STRESS_SOLO (drop the interleave),
// TACTIX_TRACE (per-step trace).
int substitute_stress_solve(Kernel &k, uint64_t seed,
                           volatile sig_atomic_t *stop);

} // namespace tactix
