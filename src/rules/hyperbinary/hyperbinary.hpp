#pragma once

// HyperBinaryRes, calculus rule X6 (plan §5.2): hyper binary resolution over
// the binary implication tree. Assume a literal `p` and close it under binaries
// alone; if a long clause `C = (l₀ ∨ … ∨ lₙ)` has every `lᵢ` (i >= 1) falsified
// by that closure then `p → l₀`, and the tightest form is emitted rather than
// `(¬p ∨ l₀)`: the binary closure is a tree rooted at `p`, so `¬l₁ … ¬lₙ` have
// a unique closest common dominator `d` and `d → l₀` already, giving
// `(¬d ∨ l₀)`. Proof delta: add-RUP — negating the resolvent, `d` propagates
// every `¬lᵢ` by dominance, so `C` becomes unit on `l₀`.
// Precondition, effect, soundness: `docs/calculus.md` §X6. Reference: cadical
// `probe.cpp` (`hyper_binary_resolve` :218, `probe_dominator` :149).

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root, the probing family's precondition (as in V1).
bool hyperbinary_guard(const Kernel &k);

// Canonical instance: one full round over the probing-candidate schedule.
// Resolution happens post-hoc, off the finished probing trail, where cadical
// resolves inside propagation so its resolvent becomes the forced literal's
// reason. The same tree is reproduced without touching propagation: walking the
// trail in order, a literal forced by a binary takes the forcing literal as its
// parent and one forced by a long clause takes its own dominator — the parent
// cadical would have installed — so dominators agree. What differs is that a
// resolvent is invisible to the rest of the same probe, strictly weaker and
// never unsound; intercepting propagation would put rule logic in the kernel.
ApplyResult hyperbinary_apply(Kernel &k);

// One root literal: close it under binaries, resolve every long clause it makes
// unit. NoOp when `p` is fixed, out of range, or yields nothing. Chains come
// from the shared RUP replay, not a dominator-specific LRAT walk (cadical
// `probe_dominator_lrat`): `d ∧ ¬l₀` is assumed at the clean root and replayed,
// so the replay both tests RUP and builds the chain, and a replay that fails
// DECLINES the resolvent — V5 Sweep's discipline. Every resolvent is a
// permanent binary, since binaries carry no redundancy bit
// (`docs/implementation-notes.md` §14), the stricter side of cadical's split.
ApplyResult hyperbinary_try(Kernel &k, Lit p);

// The round's candidate roots, in the policy-visible order (see
// `CandidateOrderPolicy`); shared with the instance surface.
void hyperbinary_candidates(const Kernel &k, std::vector<Lit> &out);

} // namespace tactix
