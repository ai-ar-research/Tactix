// C3 Vivify (plan §5.2): clause distillation by unit propagation — assume
// the negations of a candidate's literals one level at a time; an implied
// literal, an implied negation, or a probe conflict proves a strictly
// stronger clause, which replaces the candidate. Proof delta: add-RUP
// (the distilled clause, LRAT chain from a reason walk of the probe
// trail) + delete (the original); a root-satisfied candidate is deleted.
//
// References (plan §5; docs/design/independent-lineage-survey.md): kissat
// vivify.c / CaDiCaL vivify.cpp (MIT, transliterated in essence). SCOPE
// INCLUDES LEARNED TIERS (irredundant + redundant glue <= tier2): the Maple LCM
// discipline (Luo/Li/Xiao/Manyà/Lü IJCAI'17, SC17 winners onward), which
// CaDiCaL also adopted; Glucose core has no vivification (simp-only
// preprocessing), CMS runs `occ-vivif` in its schedule string.
//
// SCOPE: kissat's four bands, selected by Kernel::vivify_scope — irredundant,
// and the redundant tiers tier1/tier2/tier3 (vivify.c:236-256), each with its
// own budget share. Before SCHED-G3 the redundant side was ONE pass bounded by
// tier2, so tier3 was never vivified at all.
//
// Deviations recorded:
// (1) CLOSED by SCHED-G3 (docs/design/2026-08-05-vivify-trail-reuse.md): the
//     probing trail is carried across candidates whose assumption prefixes
//     share a head, over a count-descending global literal order with the
//     candidate list sorted lexicographically under it. What makes the reused
//     prefix valid for the next candidate is that EVERY database mutation is
//     preceded by a reset to the root, so a trail is never carried across a
//     change (invariant VP, design §3).
// (2) One rewrite per candidate per round (kissat/CaDiCaL iterate
//     within the candidate); rounds repeat via the slot schedule. STILL OPEN.
// (3) CLOSED by SCHED-G3 — probe order is now the global count order, and the
//     eager implied-literal scan after each propagation (kissat
//     vivify.c:1078-1090) comes with it, since under the count order the
//     implied literal is no longer usually the next position. What remains
//     open is kissat's `better_implied` refinement (vivify.c:1101-1117), which
//     re-picks the implied literal with the smallest trail index for a
//     shallower dependency; we take the first in trail order.
// (4) The candidate itself is excluded from probe propagation via
//     kernel_propagate's `ignore` parameter (CaDiCaL's ignore pointer —
//     the reviewed kernel API addition that V1 Probe also needs);
//     deriving through the candidate would be circular.
// (5) Redundant candidates keep their redundancy in the rewrite (glue
//     recomputed conservatively as min(old glue, new size)); an
//     irredundant rewrite stays irredundant. Strengtheners here are the
//     PROPAGATION ITSELF, so no partner-redundancy question arises (the
//     chain cites whatever clauses propagated — sound regardless of
//     their redundancy, since the rewrite is implied by F ∪ R and
//     weaker-than-original replacements keep the original derivable).
// (6) A unit rewrite asserts at the root and ends the round (values
//     changed — C2's deviation 4 verbatim); an empty rewrite is the
//     refutation (possible only in under-derived root states, corpus
//     002).
//
// Instance space (plan §5.1 v1): payload = candidate arena Ref;
// INSTANCE_CANONICAL = the full round. Guard: quiescent root.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

constexpr uint32_t VIVIFY_CLSLIM = 100; // probe-cost bound per candidate
// The STATIC fallback only. The live scope is `Kernel::tier2`, recomputed each
// reduce from the glue distribution (SCHED-C1); this value is that field's
// initial value, kept here because it is also kissat's `TIER2` constant.
constexpr uint32_t VIVIFY_TIER2 = 6;    // redundant scope (LCM): glue <= 6

bool vivify_guard(const Kernel &);
// canonical candidate order: irredundant first, then redundant tiers,
// size-ascending within each group
void vivify_candidates(const Kernel &, std::vector<Ref> &out);
ApplyResult vivify_try(Kernel &, Ref candidate); // one candidate
ApplyResult vivify_apply(Kernel &);              // canonical: full round

} // namespace tactix
