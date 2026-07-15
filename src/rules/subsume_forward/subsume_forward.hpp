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
// check: this kernel never deletes binaries, so a subsuming binary can
// never disappear later. (2) kissat's round-entry duplicate-binary /
// hyper-unary-resolution sweep is dropped — it deletes binaries, which
// this kernel cannot do yet (revisit with V4 TransitiveReduce, the rule
// whose whole delta is binary deletion). (3) No steps/effort budget in
// the rule: WHEN and HOW MUCH are policy territory (rev 17) — a policy
// wanting bounded work applies per-candidate instances.
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
