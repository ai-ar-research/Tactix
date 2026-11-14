// C2 Strengthen (plan §5.2): self-subsuming resolution. A smaller d matching c
// except one opposite literal f gives c ∖ {f}, replacing c. Proof delta:
// add-RUP + delete (`docs/dev/calculus.md` §C2). The fused check is cadical
// `subsume.cpp subsume_check`.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root, C1's argument — value semantics plus no in-flight
// reasons; reaching the root is the policy's job.
RuleEligibility strengthen_guard(const Kernel &);

// Instance space (plan §5.1): payload = candidate arena Ref, irredundant first,
// then size ascending. `complete` gives the full surface (any redundancy).
void strengthen_candidates(const Kernel &, std::vector<Ref> &out,
                           bool complete = false);

// Single-candidate instance surface, partners from the free literals' watch
// lists and their negations. Large partners must be irredundant (they justify
// deletions); binary partners need no check (implementation-notes §14).
ApplyResult strengthen_try(Kernel &, Ref candidate);

// Semantic surface: one fused result per payload (victim plus literal, or
// INVALID_LIT for C1 deletion), `clause_literal_instance` encoding.

// Canonical: the fused full round (takes C1's inference too; C1 stays
// registered). Strengthened clauses drop root-false literals; as in Kissat's
// forward pass a candidate gets one action per round. A unit is propagated
// through the partial forward image; an empty result refutes (corpus 002).
ApplyResult strengthen_apply(Kernel &);

// Eliminate's post-forward service: reconnect the irredundant image, replay
// from the saved cursor, flush satisfied large clauses. Root dense ordered
// mode.
ApplyResult strengthen_reconnect_and_flush(Kernel &, uint32_t trail_from);

} // namespace tactix
