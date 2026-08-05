// C2 Strengthen (plan §5.2: "SubsumeBackward + strengthening"): self-
// subsuming resolution — when a smaller clause d matches a candidate c on
// every literal except exactly ONE in opposite phase (f), resolving on f
// yields c ∖ {f}, which replaces c. Proof delta: add-RUP (the shortened
// clause, LRAT chain [absorbed units..., d, c]) + delete (the original).
//
// References (plan §5; independent lineages per): the fused
// subsume-or-strengthen check is CaDiCaL subsume.cpp subsume_check (MIT,
// transliterated) — all candidate literals marked BY LITERAL (phase-
// aware); a clause with every literal marked subsumes, with exactly one
// flipped strengthens. The technique family is MiniSat SimpSolver's
// backward subsumption+strengthening, shipped by Glucose (simp/) and by
// CryptoMiniSat as the `occ-backw-sub-str` schedule step — all lineages
// carry it. kissat runs it inside eliminate's dense mode (backward.c),
// which presupposes the kernel-wide watch replacement we deliberately do
// not have (C1's recorded argument).
//
// Deviations recorded:
// (1) FUSED ROUND: the round acts on subsumption too when the check finds
//     it (not acting would waste the discovery) — the catalog's C2
//     "backward" half is delivered structurally by the size-ascending
//     round (every small/large pair meets, like C1). The default slot
//     therefore schedules Strengthen INSTEAD of SubsumeForward (no double
//     scan); C1 stays registered and N1-exposed. Stats attribute the two
//     effects separately.
// (2) Strengthened clauses are rebuilt FRESH from the candidate's free
//     literals minus f (root-falsified literals stripped as well — their
//     unit ids join the LRAT chain); the original becomes garbage after
//     the add (OTFS allocate-new precedent, N1 intermediate-state
//     cleanliness). Nothing is rewritten in place.
// (3) Candidates and large strengtheners are irredundant-only (C1
//     discipline; NOTE: strengthening is actually safe with redundant
//     strengtheners — c' implies c regardless — but redundant-aware
//     strengthening arrives with C3's tier-aware vivification, once, not
//     twice). ALL binaries subsume/strengthen, with no redundancy check —
//     since binaries ARE deletable (kernel_delete_binary), so the
//     justification is no longer "structurally permanent" but the V4
//     equivalence-preservation argument recorded in subsume_forward.hpp,
//     which covers this SUBSUMED branch verbatim (it, too, deletes an
//     irredundant candidate on a binary's strength). The STRENGTHEN
//     branches are unaffected either way: they emit an add-RUP clause
//     checked against the whole formula at emit time, so it stands on its
//     own no matter what happens to the binary it resolved against.
// (4) A strengthen that yields a UNIT asserts it at the root (unit id
//     recorded) and ENDS the round: values changed, the round's marks and
//     "root-falsified" reasoning are stale; the policy re-propagates.
//     An EMPTY result is handled as the refutation it is (add-RUP of the
//     empty clause, Unsat): under N1 histories a root can be engine-
//     quiescent yet semantically under-derived (corpus regression 002),
//     so "live candidates have >= 2 free literals" may not hold.
// (5) A strengthened result of size >= 3 is RE-SCANNED as the current
//     candidate before connecting (strengthening cascades — CaDiCaL
//     re-tries the shrunken clause the same way).
//
// Instance space (plan §5.1 v1): payload = candidate arena Ref, like C1;
// apply_instance subsumes-or-strengthens exactly that clause, discovering
// partners through the WATCH lists of the candidate's free literals and
// their negations. INSTANCE_CANONICAL = the full round.
//
// Guard: quiescent root (C1's argument verbatim: value semantics + no
// in-flight reasons; reaching the root is the policy's job).
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// same cost bounds as C1 (kissat subsumeclslim/subsumeocclim defaults)
constexpr uint32_t STRENGTHEN_CLSLIM = 1000;
constexpr uint32_t STRENGTHEN_OCCLIM = 1000;

bool strengthen_guard(const Kernel &);
// canonical candidate order (size ascending; C1's criteria)
void strengthen_candidates(const Kernel &, std::vector<Ref> &out);
// single-candidate instance surface: Ok = subsumed or strengthened
ApplyResult strengthen_try(Kernel &, Ref candidate);
ApplyResult strengthen_apply(Kernel &); // canonical: the full round

} // namespace tactix
