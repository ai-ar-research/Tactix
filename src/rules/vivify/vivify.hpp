// C3 Vivify (plan §5.2): clause distillation by unit propagation — assume
// the negations of a candidate's literals one level at a time; an implied
// literal, an implied negation, or a probe conflict proves a strictly
// stronger clause, which replaces the candidate. Proof delta: add-RUP
// (the distilled clause, LRAT chain from a reason walk of the probe
// trail) + delete (the original); a root-satisfied candidate is deleted.
//
// References (plan §5; rev 25 lineages): kissat vivify.c / CaDiCaL
// vivify.cpp (MIT, transliterated in essence). SCOPE INCLUDES LEARNED
// TIERS (irredundant + redundant glue <= tier2): the Maple-lineage LCM
// discipline (Luo/Li/Xiao/Manyà/Lü IJCAI'17, SC17 winners onward), which
// CaDiCaL also adopted; Glucose core has no vivification (simp-only
// preprocessing), CMS runs `occ-vivif` in its schedule string.
//
// Deviations recorded:
// (1) No trail reuse between candidates and no count-based candidate
//     scheduling (kissat's vivifier sorts by usage counts and reuses the
//     assumed prefix across sorted candidates): each candidate probes
//     from and returns to the root (kernel_backtrack update_phases=false,
//     the Lucky primitive). Cost, not soundness; revisit when the slot
//     gains tick budgets.
// (2) One rewrite per candidate per round (kissat/CaDiCaL iterate
//     within the candidate); rounds repeat via the slot schedule.
// (3) Probe order = clause literal order (kissat sorts by inverse
//     trail/counts).
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
constexpr uint32_t VIVIFY_TIER2 = 6;    // redundant scope (LCM): glue <= 6

bool vivify_guard(const Kernel &);
// canonical candidate order: irredundant first, then redundant tiers,
// size-ascending within each group
void vivify_candidates(const Kernel &, std::vector<Ref> &out);
ApplyResult vivify_try(Kernel &, Ref candidate); // one candidate
ApplyResult vivify_apply(Kernel &);              // canonical: full round

} // namespace tactix
