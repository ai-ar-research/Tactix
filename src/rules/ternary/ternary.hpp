// HyperTernaryRes, calculus rule X2 (plan §5.2): two ternary clauses
// (p ∨ a ∨ b) and (¬p ∨ c ∨ d) resolve on p, and the resolvent is kept only
// when it collapses to size <= 3, i.e. when the antecedents share literals.
// Adding it needs no witness — a resolvent is entailed by its two parents.
// Proof delta: add-RUP with chain [id(C), id(D)], the two-parent chain BVE also
// emits, so no new proof primitive. Precondition, effect and soundness:
// `docs/calculus.md` §X2. Reference: cadical `ternary.cpp`
// (`hyper_ternary_resolve`), the same resolvent walk with the same three
// rejections; kissat dropped hyper ternary resolution before 4.0.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: declared quiescent-root boundary — the occurrence index and the "already
// present" checks only mean anything once every level-0 unit is propagated into
// the clause set.
RuleEligibility ternary_guard(const Kernel &);

// Frozen v1 instance space (plan §5.1): payload = the pivot literal; one
// instance applies all resolutions of its positive ternaries against its
// negative ones. The additive semantic surface exposes each applicable parent
// pair/resolvent independently through an opaque snapshot payload. Canonical
// order: variables carrying at least one ternary occurrence in each polarity,
// ascending by literal code.
// `total_clauses` (optional out) is the live clause count from the same arena
// pass, the base for `Kernel::ternary_max_add_percent`.
void ternary_pivots(const Kernel &, std::vector<Lit> &out,
                    uint64_t *total_clauses = nullptr,
                    bool complete = false);

// Saturating floor(base * percent / 100), used by the caller-configurable
// output governor.  Saturation keeps the complete uint32 percentage domain
// valid without allowing multiplication to wrap into a smaller limit.
uint64_t ternary_percent_limit(uint64_t base, uint32_t percent);

// One pivot: add every collapsing resolvent. Resolvents are added redundant
// (glue = size) so Reduce can retire them, since adding an entailed consequence
// irredundant would grow the formula monotonically with no reclamation path;
// binary resolvents are the exception, having an irredundant lifecycle
// (`docs/implementation-notes.md` §14). Antecedents are not subsumed away — a
// rule owns one proof delta, and mixing `del` in would duplicate C1's
// occurrence bookkeeping (cadical does it at ternary.cpp:215).
ApplyResult ternary_try(Kernel &, Lit pivot);

// Canonical: the full round. No steps budget in the rule; it charges occurrence
// traffic to `stats.ternary_ticks` so a policy budget can be honest, and pairs
// are enumerated per pivot over the shared dense index rather than a
// per-literal ternary flag cache.
ApplyResult ternary_apply(Kernel &);

} // namespace tactix
