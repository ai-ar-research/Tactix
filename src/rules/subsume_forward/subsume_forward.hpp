// C1 SubsumeForward (plan §5.2): forward subsumption — delete clauses
// that are implied literal-for-literal by a smaller clause (modulo
// root-falsified literals). Proof delta: delete only; strengthening
// (self-subsuming resolution) is C2's add-RUP + delete territory.
//
// References (plan §5 "evidence, not templates"; independent lineages per
// rev 25 — Glucose ships only MiniSat-style preprocessing backward
// subsumption (simp/SimpSolver.cc, same 1000 clause-size cap), and
// CryptoMiniSat's subsumeimplicit.cpp subsumes THROUGH the watch lists,
// the closest precedent for this rule's binary-subsumers-from-watches and
// per-instance discovery): the ROUND is a
// transliteration of CaDiCaL subsume.cpp subsume_round (MIT) — candidates
// sorted by size ascending, each checked against previously CONNECTED
// clauses through round-local one-literal occurrence lists, then
// connected at its least-occurring literal. kissat forward.c implements
// the same idea but presupposes dense mode (assert(!solver->watching):
// watches globally replaced by occurrence lists during elimination) plus
// per-var subsume flags persisting across rounds — a kernel-wide mode our
// watch-owning kernel does not have; CaDiCaL's self-contained round fits
// the §5.3 contract (everything rule-local, kernel untouched). Deviations
// recorded: (1) candidates and large subsumers are irredundant-only
// (CaDiCaL also subsumes/promotes redundant clauses; revisit with C2's
// redundant-aware strengthening). Binary subsumers need no redundancy
// check — see the V4 reconciliation below. (2) kissat's round-entry
// duplicate-binary / hyper-unary-resolution sweep is dropped: it deletes
// binaries by MULTISET position (one copy of a duplicated pair), which
// binary_ids — one id per literal PAIR — cannot name. V4 deletes only
// pairs with a single live copy; the sweep would need per-copy ids. (3)
// No steps/effort budget in
// the rule: WHEN and HOW MUCH are policy territory (rev 17) — a policy
// wanting bounded work applies per-candidate instances.
//
// V4 RECONCILIATION (rev 33) — why binary subsumers still need no
// redundancy check now that binaries ARE deletable (kernel_delete_binary):
// this rule deletes an irredundant candidate C on the strength of a
// subsuming binary D = (a ∨ b) ⊆ C, so C's constraint afterwards survives
// only through D. That is sound iff no rule can later delete D in a way
// that enlarges the model set. V4 TransitiveReduce — the only binary
// deleter — is EQUIVALENCE-PRESERVING: it drops D only when a path P of
// SURVIVING binaries already entails it (P ⊨ D), so F \ {D} ≡ F, hence
// F \ {D} ⊨ D ⊨ C. The surviving-path property is what V4's round-local
// tombstone set buys (it never deletes an edge justified by an edge it is
// deleting in the same round), and equivalence composes across rounds and
// across any N1 interleaving, so no model is ever gained. There is also
// nothing to check against: binaries carry no redundant flag and have
// nowhere to store one (kernel_add_binary_clause takes no `redundant`
// param; the binary watch word is full — watch.hpp:22-29). A check here
// would be dead code today.
//
// FORWARD-LOOKING CONDITION (not coded — the trigger to code it): if a
// future rule ever introduces genuinely redundant binaries (hyper-binary
// resolvents carrying a stored redundant bit) or deletes a binary
// NON-equivalence-preservingly (a V3b substitute-rewrite re-points edges
// at an SCC representative — model-preserving only modulo the witness
// pushed onto ρ, which is a strictly weaker property than the equivalence
// argued above), then a `binary_is_redundant(k, a, b)` guard becomes
// MANDATORY at all three binary-subsumer sites: subsumed_by_binary and
// subsume_forward_try below, and strengthen.cpp's phase-0 SUBSUMED branch
// (identical reliance). V4 introduces neither, so it stays deferred.
//
// Instance space (plan §5.1 v1 encoding): payload = the arena Ref of a
// candidate clause; apply_instance tries to forward-subsume exactly that
// clause, discovering subsumers through the WATCH lists of the
// candidate's free literals (complete for pure subsumption: any
// subsumer's two watched literals are its own literals, hence literals of
// the candidate). instances() yields candidates in the canonical
// processing order (size ascending). INSTANCE_CANONICAL = the full round.
//
// Guard: quiescent root, like Lucky — fully propagated, consistent, no
// conflict/pending handoff, level 0. Root quiescence is what makes the
// semantic check sound as stated ("false" literally means root-falsified,
// and any live clause has >= 2 free literals) and keeps deletion away
// from in-flight reasons; reaching the root is the policy's job
// (Restart keep-level 0 composes it).
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// kissat options.h defaults (subsumeclslim/subsumeocclim = 1e3); these
// bound the cost of one check, not the schedule — instance-selection
// limits like Reduce's tier thresholds, so they live rule-side
constexpr uint32_t SUBSUME_CLSLIM = 1000;
constexpr uint32_t SUBSUME_OCCLIM = 1000;

bool subsume_forward_guard(const Kernel &);
// candidate enumeration in canonical order (size ascending): irredundant,
// live, 3 <= size <= SUBSUME_CLSLIM, not root-satisfied
void subsume_forward_candidates(const Kernel &, std::vector<Ref> &out);
// single-candidate check (the instance surface): deletes + collects when
// subsumed; Ok = deleted, NoOp = kept (or payload not a candidate)
ApplyResult subsume_forward_try(Kernel &, Ref candidate);
ApplyResult subsume_forward_apply(Kernel &); // canonical: the full round

} // namespace tactix
