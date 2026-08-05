// C1 SubsumeForward (plan §5.2): forward subsumption — delete clauses
// that are implied literal-for-literal by a smaller clause (modulo
// root-falsified literals). Proof delta: delete only; strengthening
// (self-subsuming resolution) is C2's add-RUP + delete territory.
//
// References (plan §5 "evidence, not templates"; independent lineages per
// — Glucose ships only MiniSat-style preprocessing backward
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
// check — see the binary-subsumer argument below. (2) kissat's round-entry
// duplicate-binary / hyper-unary-resolution sweep is dropped: it deletes
// binaries by MULTISET position (one copy of a duplicated pair), which
// binary_ids — one id per literal PAIR — cannot name. V4 deletes only
// pairs with a single live copy; the sweep would need per-copy ids. (3)
// No steps/effort budget in
// the rule: WHEN and HOW MUCH are policy territory  — a policy
// wanting bounded work applies per-candidate instances().
//
// WHY BINARY SUBSUMERS NEED NO REDUNDANCY CHECK. This rule deletes an
// irredundant candidate C on the strength of a subsumer D ⊆ C. The
// invariant to preserve is the inprocessing one — every model of the
// IRREDUNDANT formula F reconstructs through ρ to a model of F₀ — and
// deleting C preserves it whenever D ∈ F at that moment: m ⊨ F∖{C} ⟹ m ⊨ D
// ⟹ m ⊨ C. The argument CLOSES at the deletion and claims nothing about D's
// future, because every later F-rewrite carries the same obligation for the
// state it produces: it either leaves F ⊨ D, or pushes the ρ witness that
// repairs D. The one thing that would break it is D being FORGETTABLE — a
// member of the redundant set R, which may be dropped with neither
// entailment nor a witness. That is what the large-subsumer
// `d->redundant()` skips exist for.
//
// Binaries are never in R. kernel_add_binary_clause takes no `redundant`
// parameter and the binary watch word has no spare bit (watch.hpp), so
// every binary — a learned one from Analyze included — has an irredundant
// lifecycle, and the Forget paths (S2 Reduce; E1 BVE's redundant clauses on
// the pivot) act on arena Refs only and cannot reach one, as GC copies
// binary watch slots verbatim. Every rule that DELETES a binary leaves it
// entailed (V4 TransitiveReduce's surviving path; E2 Factor's aux-var
// definition; the root-satisfied/tautological binaries E1 BVE and V3
// Substitute drop) or pushes the ρ witness that restores it (V3's rewrite
// onto an SCC representative, X1 BCE, E1 BVE's pivot clauses). So
// `binary_is_redundant(k, a, b)` would be a constant false — dead code.
//
// THE ONE TRIGGER that would make it MANDATORY at all three binary-subsumer
// sites (subsumed_by_binary and subsume_forward_try below, and
// strengthen.cpp's phase-0 SUBSUMED branch — identical reliance): a rule
// that gives binaries a REDUNDANT lifecycle — a stored redundant bit, or
// any Forget that can reach a binary. A binary deletion that is merely
// non-equivalence-preserving is NOT a trigger: V3 Substitute, X1 BCE, E1
// BVE and E2 Factor all do that today. Argument and the enumeration behind
// it: docs/implementation-notes.md §14; the V3 case is pinned by
// tests/test_subsume.cpp test_binary_subsumer_then_substitute.
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
