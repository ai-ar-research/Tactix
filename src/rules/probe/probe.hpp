// V1 Probe (plan §5.2): failed-literal probing producing ROOT UNITS.
// For each binary-implication-graph-head literal p (a literal whose
// negation carries a binary watch, so assuming p propagates something),
// assume p at a fresh level and propagate (FULL propagation); a conflict
// proves p cannot hold, so {¬p} is a RUP unit. Proof delta: add-RUP unit
// ONLY (the LRAT chain is a reason walk of the conflict — C3 Vivify's
// probe-conflict case with the emitted clause reduced to the singleton
// {¬p} and no candidate clause, no `ignore`). The derived units are
// asserted un-propagated at root and the round ends; M1 Propagate runs
// each unit's cascade (root-unit LRAT emission + refutation), exactly as
// C2 Strengthen / C3 Vivify leave a derived unit for the policy to
// propagate. This is the failed-literal machinery S5 Lucky deferred
// (lucky.cpp deviation 1).
//
// References (plan §5; rev-25 independent-lineage survey):
//  - kissat 4.0.4: the failed-literal loop, candidate schedule and
//    round/effort skeleton live in backbone.c (compute_backbone /
//    schedule_backbone_candidates), driven by probe.c; the propagation
//    engine is proprobe.c (kissat_probing_propagate) and unit emission is
//    assign.c kissat_learned_unit -> proof.c kissat_add_unit_to_proof.
//    Kissat keeps proprobe/backbone/substitute/transitive/congruence/
//    vivify as SEPARATE modules — the V1/V2/V3/V4/C3 split we mirror. V1
//    is that loop with FULL propagation; kissat's backbone.c restricts it
//    to binary-only propagation — that binary restriction IS V2 Backbone
//    (a subset of V1's units; V2's value is speed, not extra units).
//  - CaDiCaL probe.cpp (failed_literal / probe_dominator_lrat): confirms
//    the single-decision UIP degenerates to ¬p (no UIP analysis needed)
//    and the chain is a plain reason walk. Its default HBR (opts.probehbr)
//    and dominator machinery add binaries — OUT of the add-RUP-unit scope.
//  - CryptoMiniSat (probe.cpp full_probe / intree.cpp): monolithic —
//    bundles HBR + transitive reduction + SCC substitution + bothprop
//    necessary-assignment lifting in one pass; we take only the
//    failed-literal core and the BIG-head candidate idea (intree fill_roots).
//  - Glucose: no probing (simplify + LCM vivification only) — confirms
//    failed-literal probing is optional, not load-bearing, for a strong core.
//  MIT/GPL references; transliterated in essence, no code copied verbatim.
//  The reason walk (walk_reasons/emit_chain) is duplicated from Vivify per
//  §5.3 self-containment; a shared reason-walk utility is a
//  simplify-codebase candidate once V2 Backbone confirms the third consumer.
//
// Deviations recorded:
// (1) No tick/propagation budget — a round probes every BIG-head candidate
//     (kissat/CaDiCaL gate with SET_EFFORT_LIMIT ≈ 2%/8‰ of search ticks).
//     Acceptable while the inprocessing slot is deferred (no default-
//     schedule cost); a step budget joins when the slot gains one. Mirrors
//     Vivify deviation 1.
// (2) Candidate order = (var asc, positive-phase first); no RNG shuffle and
//     no usage-count / negated-occurrence ranking (kissat/CMS shuffle or
//     rank). Determinism over adversarial-order robustness — the Vivify
//     determinism discipline. Mirrors Vivify deviation 3.
// (3) BIG-head restriction (¬probe carries a binary watch), not true BIG
//     roots and not all free literals. Complete for single-literal probing
//     at a clean root: a lone assumption there can only cascade through a
//     binary implication (a large clause keeps ≥2 unassigned literals until
//     a binary forces one), so a probe whose negation has no binary watch
//     cannot fail. It does NOT subsume V2's units, whose binary-only walk
//     is a different candidate/propagation regime.
// (4) No hyper-binary resolution — V1 adds zero clauses of any kind
//     (CaDiCaL's default probehbr path is add-binary, out of scope).
// (5) No lifting / "necessary assignment" (a literal forced to the same
//     value on both p and ¬p probes) — add-RUP-unit-compatible but V2
//     Backbone's job; kept out to keep V1/V2 non-overlapping (CMS folds it
//     into probe_inter; we do not).
// (6) No UIP analysis — a single decision level means the derived unit is
//     always ¬p (kissat backbone_analyze / CaDiCaL probe_dominator omitted).
// (7) Both polarities of a candidate variable are probed independently; once
//     one polarity fails, the variable is skipped for the rest of the round
//     (a per-round `forced` mark). The both-fail refutation is caught when
//     M1 propagates the asserted batch — not by cross-probe reasoning (that
//     would be lifting, deviation 5).
// (8) A round collects all failed literals against the SAME clean root
//     (no re-propagation between probes — so no in-round cascade), then
//     asserts the batch and ends; further layers are found on the slot's
//     next pass (kissat's outer proberounds structure). This is what lets
//     the cascade + root-unit id emission stay entirely M1's (a bare
//     in-round re-propagation would leave cascade literals with unit_ids==0
//     and break the flush pre-pass's LRAT chains — strengthen.cpp act()).
//
// Instance space (plan §5.1 v1): payload = probe literal as a sign-extended
// DIMACS int (like Decide); INSTANCE_CANONICAL = the full round. Guard:
// quiescent root. The `ignore` parameter of kernel_propagate is NOT used
// (there is no candidate clause to exclude — that is C3 Vivify's device).
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

bool probe_guard(const Kernel &);
// canonical candidate order: BIG-head literals (¬probe has a binary watch),
// var-index ascending, positive phase before negative
void probe_candidates(const Kernel &, std::vector<Lit> &out);
ApplyResult probe_try(Kernel &, Lit p); // one probe literal
ApplyResult probe_apply(Kernel &);      // canonical: full round

} // namespace tactix
