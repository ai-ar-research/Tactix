#pragma once

// HyperBinaryRes, rule X6 (plan §5.2): assume `p` and close under binaries; if
// a long clause has every lᵢ (i >= 1) falsified, emit (¬d ∨ l₀) for the closest
// common dominator d of ¬l₁ … ¬lₙ in the implication tree. Proof delta: add-RUP
// (`docs/dev/calculus.md` §X6). Reference: cadical `probe.cpp`
// (`hyper_binary_resolve` :218, `probe_dominator` :149).

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root, the probing family's precondition (as in V1).
RuleEligibility hyperbinary_guard(const Kernel &k);

// Canonical instance: one round over the probing schedule. Resolution is
// post-hoc off the finished trail, reproducing cadical's tree (a long-clause
// literal takes its dominator as parent); resolvents are invisible to the rest
// of the same probe, which is weaker and sound.
ApplyResult hyperbinary_apply(Kernel &k);

// One root literal: close it and resolve every long clause it makes unit. NoOp
// when `p` is fixed, out of range, or yields nothing. A failed RUP replay of
// `d ∧ ¬l₀` declines the resolvent. Resolvents are permanent binaries
// (implementation-notes §14).
ApplyResult hyperbinary_try(Kernel &k, Lit p);

// The round's candidate roots, in the policy-visible order (see
// `CandidateOrderPolicy`); shared with the instance surface.
void hyperbinary_candidates(const Kernel &k, std::vector<Lit> &out);

// Semantic payload: dominator plus the arena word of the output literal's
// occurrence (identifying C and l0), stable until collection. Applies at most
// one resolvent.
uint64_t hyperbinary_semantic_instance(Lit dominator,
                                       uint32_t literal_word);

} // namespace tactix
