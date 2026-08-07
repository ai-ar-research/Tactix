#pragma once

// X6 `HyperBinaryRes` — hyper binary resolution over the binary implication
// tree (plan §5.2; calculus §6 X6; parity ledger `SP-PROBE-2`).
//
// Reference: cadical `probe.cpp` (`probehbr`=1, `hyper_binary_resolve` at :218,
// `probe_dominator` at :149). Transliterated, not copied.
//
// THE INFERENCE. Assume a literal `p` and close it under the BINARY implication
// graph alone. If a long clause `C = (l₀ ∨ l₁ ∨ … ∨ lₙ)` has every `lᵢ` (i ≥ 1)
// falsified by that closure, then `p → l₀`. The useful form is not `(¬p ∨ l₀)`
// but the TIGHTEST one: the closure restricted to binaries is a TREE rooted at
// `p`, so the negated literals `¬l₁ … ¬lₙ` have a unique closest common
// dominator `d`, and `d → l₀` already. We emit `(¬d ∨ l₀)`.
//
// WHY THE RESOLVENT IS RUP, which is what licenses `add-RUP`: assume its
// negation, `d ∧ ¬l₀`. Propagating `d` reaches every `¬lᵢ` (i ≥ 1) by
// dominance, so `C` becomes unit on `l₀`, contradicting `¬l₀`. The chain is
// therefore constructed by REPLAY rather than derived — see below.
//
// DEVIATIONS from cadical, each with its reason.
//
//  1. **We resolve POST-HOC, off the finished probing trail; cadical resolves
//     INSIDE propagation.** Its resolvent becomes the forced literal's reason,
//     so the implication graph stays a tree even where a long clause forced.
//     We reproduce exactly that tree without touching propagation: walking the
//     trail in order, a literal forced by a binary takes the forcing literal as
//     its parent, and a literal forced by a LONG clause takes its own dominator
//     `d` — the same parent cadical would have installed. Dominators therefore
//     agree. What differs is that our resolvents are not visible to the rest of
//     the same probe, so a later clause cannot resolve against one. That is
//     strictly weaker and never unsound. The reason to accept it is the §5.3
//     contract: intercepting propagation would put rule logic in the kernel.
//
//  2. **Chains come from the shared RUP replay, not from a dominator-specific
//     LRAT walk** (cadical `probe_dominator_lrat`). After unwinding we assume
//     `d ∧ ¬l₀` at the clean root and replay; the replay both TESTS RUP and
//     builds the chain, and a replay that fails DECLINES the resolvent. This is
//     V5 Sweep's discipline and reuses the one proof-critical helper the
//     project already has. It runs in DRAT mode too, where it is the test
//     rather than the chain construction.
//
//  3. **The subsumed original is deleted, and case (A) is skipped**, both as in
//     cadical. If `¬d ∈ C` the resolvent subsumes `C`, so `C` is deleted
//     (`delete`). If every `lᵢ` (i ≥ 1) is ROOT-false the clause is already
//     binary in effect and no resolvent is worth adding — cadical's case (A),
//     which its garbage collector handles anyway.
//
//  4. **No redundant/irredundant split.** cadical marks the resolvent redundant
//     unless it subsumes an irredundant reason. Our binaries are watch entries
//     plus a proof id and carry no redundancy bit, so the distinction has
//     nowhere to live; every resolvent is a permanent binary. This is the
//     stricter side — we keep less freedom to delete, never more.

#include "kernel/kernel.hpp"
#include "rules/registry.hpp"

#include <vector>

namespace tactix {

bool hyperbinary_guard(const Kernel &k);

// canonical instance: one full round over the probing-candidate schedule
ApplyResult hyperbinary_apply(Kernel &k);

// one root literal: close it under binaries, resolve every long clause it
// makes unit. NoOp when `p` is fixed, out of range, or yields nothing.
ApplyResult hyperbinary_try(Kernel &k, Lit p);

// the round's candidate roots, in the policy-visible order (see
// `CandidateOrderPolicy`); shared with the instance surface.
void hyperbinary_candidates(const Kernel &k, std::vector<Lit> &out);

} // namespace tactix
