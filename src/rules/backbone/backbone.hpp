// V2 Backbone (plan §5.2): the binary-implication backbone — failed-literal
// probing restricted to BINARY-ONLY propagation. For each binary-implication-
// graph-head literal p, assume p and propagate through the binary implication
// graph only (`kernel_propagate_binary`); a binary conflict proves p cannot
// hold, so {¬p} is a RUP root unit. Same shape and proof delta as V1 Probe
// (add-RUP unit only; collect-then-batch round; M1 owns the cascade), the ONE
// difference being the propagation engine: V1 uses full propagation, V2 uses
// the binary implication graph. So V2's derived units are a SUBSET of V1's —
// V2's value is speed/incrementality (binary propagation is far cheaper and
// touches no large clauses), which makes it a distinct, cheaper scheduling
// arm for the phase-6 learner, not a source of extra units.
//
// References (plan §5; rev-25 independent-lineage survey):
//  - kissat 4.0.4 backbone.c (compute_backbone / backbone_propagate, which
//    `stop_early`-restricts propagation to binary watches) + proprobe.c: this
//    IS the binary-only loop; V1 Probe is the same loop with full propagation
//    (probe.hpp records the split). Unit emission follows assign.c
//    kissat_learned_unit -> proof.c kissat_add_unit_to_proof.
//  - CryptoMiniSat intree.cpp: a monolithic BIG DFS that ALSO folds in
//    hyper-binary resolution, transitive reduction and SCC substitution — all
//    out of the add-RUP-unit scope (V3/V4/E-family own those); we take only
//    the failed-literal-over-the-BIG core.
//  - Glucose: no backbone/probing (simp + LCM only).
//  MIT/GPL references; transliterated in essence, no code copied verbatim.
//  The proof-critical LRAT reason walk is the shared kernel helper
//  (kernel_reason_walk / kernel_conflict_reason_walk); Vivify/Probe/Backbone
//  all route their chains through it.
//
// Deviations recorded (V1 Probe's list applies verbatim — see probe.hpp — the
// two rules share every deviation but the propagation engine):
// (1) STALE as written — corrected 2026-08-02: DefaultPolicy DOES budget both
//     (EFFORT_SPECS `Probe`/`Backbone`, 20 permille of search ticks) and the
//     inprocessing slot was activated 2026-07-19. What is still absent is a
//     RULE-side budget, which only matters for a policy that is not
//     DefaultPolicy; (2) candidate order
//     (var asc, positive-first), no RNG/ranking; (3) BIG-head candidates;
//     (4) no HBR / (5) no lifting / (6) no UIP analysis; (7) both polarities
//     probed independently, once-forced-per-round `forced` mark; (8) a round
//     collects all failed literals against the same clean root then asserts
//     the batch un-propagated and ends — M1 Propagate runs the cascade + root-
//     unit id emission + refutation.
//
// Instance space (plan §5.1 v1): payload = probe literal as a sign-extended
// DIMACS int (like Decide / V1 Probe); INSTANCE_CANONICAL = the full round.
// Guard: quiescent root.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

bool backbone_guard(const Kernel &);
// canonical candidate order: BIG-head literals (¬probe has a binary watch),
// var-index ascending, positive phase before negative (same set as V1 Probe)
void backbone_candidates(const Kernel &, std::vector<Lit> &out);
ApplyResult backbone_try(Kernel &, Lit p); // one binary probe
ApplyResult backbone_apply(Kernel &);      // canonical: full round

} // namespace tactix
