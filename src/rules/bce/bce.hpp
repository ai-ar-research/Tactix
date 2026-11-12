// BCE, rule X1 (plan §5.2): delete a clause blocked on l ∈ C (every resolvent
// on l tautological). Equisatisfiable: the witness sets l true. Proof delta:
// `del` + `push-witness` (`docs/dev/calculus.md` §X1). Reference: cadical
// `block.cpp` (Eén-Biere).
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root.
RuleEligibility bce_guard(const Kernel &);

// v1 instance space (plan §5.1): payload = blocking literal, deleting every
// irredundant clause blocked on it; the semantic surface names each choice.
// Canonical order: literals with an irredundant occurrence, ascending.
// INSTANCE_CANONICAL = the full round.
void bce_literals(const Kernel &, std::vector<Lit> &out);

// One literal: delete every irredundant clause blocked on it with a witness.
// Ok = something deleted. Candidates are irredundant and never units; a pure
// literal is the vacuous case (implementation-notes §1, X1).
ApplyResult bce_try(Kernel &, Lit blocking);
ApplyResult bce_apply(Kernel &); // canonical: the profile-selected round

} // namespace tactix
