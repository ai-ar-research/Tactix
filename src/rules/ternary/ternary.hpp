// HyperTernaryRes, rule X2 (plan §5.2): resolve (p ∨ a ∨ b) with (¬p ∨ c ∨ d)
// and keep the resolvent only at size <= 3. Proof delta: add-RUP from the
// parents (`docs/dev/calculus.md` §X2). Reference: cadical `ternary.cpp`
// (`hyper_ternary_resolve`); kissat dropped it before 4.0.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root.
RuleEligibility ternary_guard(const Kernel &);

// v1 instance space (plan §5.1): payload = pivot literal, applying all its
// resolutions; the semantic surface exposes each pair. Canonical order:
// variables with ternaries in both polarities, ascending literal code.
// `total_clauses` (optional) is the base for `Kernel::ternary_max_add_percent`.
void ternary_pivots(const Kernel &, std::vector<Lit> &out,
                    uint64_t *total_clauses = nullptr,
                    bool complete = false);

// Saturating floor(base * percent / 100).
uint64_t ternary_percent_limit(uint64_t base, uint32_t percent);

// One pivot: add every collapsing resolvent, redundant (glue = size) so Reduce
// can retire it; binaries are irredundant (implementation-notes §14).
// Antecedents are not subsumed (cadical does, ternary.cpp:215).
ApplyResult ternary_try(Kernel &, Lit pivot);

// Canonical: the full round, charging `stats.ternary_ticks`; no steps budget.
ApplyResult ternary_apply(Kernel &);

} // namespace tactix
