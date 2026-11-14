// Instantiate, rule X4 (plan §5.2): for C = D ∨ l, assume l and ¬D; a conflict
// proves ¬l ∨ D, so D replaces C. Proof delta: add-RUP (D) + delete (C)
// (`docs/dev/calculus.md` §X4). Reference: cadical `instantiate.cpp`. It feeds
// BVE by making rare literals pure; judge it with X1/X2/X4.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root.
RuleEligibility instantiate_guard(const Kernel &);

// v1 instance space: payload = candidate Ref in arena order, trying its
// eligible literals in order; the semantic surface exposes each
// (clause,literal).
void instantiate_candidates(const Kernel &, std::vector<Ref> &out);

// One clause: the first successful literal rewrites and returns. Root-false
// literals stay in D.
ApplyResult instantiate_try(Kernel &, Ref candidate);
ApplyResult instantiate_try_literal(Kernel &, Ref candidate, Lit literal);

// Canonical: the full round. No steps budget in the rule — the round is
// budget_aware, reading `Kernel::work_budget` in propagation ticks.
ApplyResult instantiate_apply(Kernel &);

} // namespace tactix
