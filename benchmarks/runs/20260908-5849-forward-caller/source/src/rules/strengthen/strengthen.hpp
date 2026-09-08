// C2 Strengthen (plan §5.2, "SubsumeBackward + strengthening"): self-subsuming
// resolution — when a smaller clause d matches a candidate c on every literal
// except exactly one in opposite phase (f), resolving on f yields c ∖ {f},
// which replaces c. Proof delta: add-RUP of the shortened clause, LRAT chain
// [absorbed units..., d, c], plus delete of the original.
// Precondition, effect, soundness: `docs/calculus.md` §C2. The fused
// subsume-or-strengthen check is cadical `subsume.cpp subsume_check`
// transliterated: candidate literals are marked phase-aware, a partner with
// every literal marked subsumes and one with exactly one flipped strengthens.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

constexpr uint32_t STRENGTHEN_CLSLIM = 1000;
// Pinned Kissat options.h `subsumeocclim`: one limit for the complete mixed
// dense watch stream (binary and large entries alike).
constexpr uint32_t STRENGTHEN_OCCLIM = 1000;
constexpr uint32_t STRENGTHEN_BINLIM = 10000;

// Guard: quiescent root, C1's argument — value semantics plus no in-flight
// reasons; reaching the root is the policy's job.
RuleEligibility strengthen_guard(const Kernel &);

// Instance space (plan §5.1): payload = candidate arena Ref, as in C1, in
// canonical order (irredundant first, then size ascending). `complete` selects
// the instance surface's set — every clause `strengthen_try` accepts, any
// redundancy; the default is the canonical round's scope.
void strengthen_candidates(const Kernel &, std::vector<Ref> &out,
                           bool complete = false);

// Single-candidate instance surface. Partners are discovered through the watch
// lists of the candidate's free literals and their negations. A large partner
// must be irredundant — the subsumed branch needs it, since the partner
// justifies a deletion, and the check stays uniform because the branch is
// decided after the partner is found. A binary partner carries no redundancy
// check (subsume_forward.hpp; `docs/implementation-notes.md` §14). Victims are
// wider, any redundancy, and redundant survivors are never indexed as partners,
// so widening the victim set leaves the partner set alone.
ApplyResult strengthen_try(Kernel &, Ref candidate);

// Canonical: the full round, FUSED — it takes C1's inference whenever the check
// finds one, so the default slot schedules this rule instead of SubsumeForward
// while C1 stays registered and N1-exposed. Strengthened clauses are rebuilt
// fresh from the candidate's free literals minus the resolved one, with
// root-falsified literals stripped and their unit ids joining the chain.  As in
// Kissat's forward pass, a candidate gets at most one action per round; a
// strengthened large survivor is connected for later candidates and re-armed
// for a later round. A unit result is propagated through the current partial
// forward connection image before continuing; an empty result refutes, which is
// reachable because an N1 history can leave a root engine-quiescent yet
// semantically under-derived (corpus 002).
ApplyResult strengthen_apply(Kernel &);

// Eliminate's post-forward service, separate from canonical C2 and explicit
// actions: reconnect the full irredundant image, replay from the saved entry
// cursor, then flush satisfied large clauses before scheduling BVE. Uses the
// existing unit/refutation proof vocabulary; requires root dense ordered mode.
ApplyResult strengthen_reconnect_and_flush(Kernel &, uint32_t trail_from);

} // namespace tactix
