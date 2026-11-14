// S6 Walk (plan §5.2; docs/design/s6-walk-design.md; kissat walk.c / cadical
// walk.cpp): ProbSAT local search improving saved phases, with no proof delta.
// The root-phase operation also reproduces Kissat's dense-entry/sparse-resume
// normalization (removing root-satisfied and duplicate binaries, rebuilding
// large watches). The assignment is copied into `h->saved` only if it falsifies
// fewer clauses than the current phases. One application is Kissat's walking
// rephase slot: search, then target := saved, watermark reset,
// `stats.rephased++`. Search callers apply Restart first.
#pragma once

#include "rules/registry.hpp"
#include "strategy/rule_parameters.hpp"

namespace tactix {

// Explicit payloads, including zero, give exact occurrence-scan allowances;
// seeded imports must improve the same objective, and incomplete or interrupted
// work keeps phase memories. FullCurrentCnf splits preparation and search doses
// (docs/dev/controller-interface.md, Walk).

RuleEligibility walk_guard(const Kernel &);
ApplyResult walk_apply(Kernel &);                 // canonical: budgeted round
ApplyResult walk_apply_steps(Kernel &, uint64_t); // explicit step budget

// Pure scoring seam; `walk_count` is one-based like Kernel::stats.walks.
double walk_break_base(const strategy::WalkParameters &, double average_size,
                       uint64_t walk_count);

// Per-mille of live irredundant ternaries with an exact complement.
uint32_t walk_complementary_ternary_fraction_permille(const Kernel &);

} // namespace tactix
