// S6 Walk (plan §5.2; design docs/design/s6-walk-design.md; kissat walk.c /
// cadical walk.cpp): WalkSAT/ProbSAT local search that improves the saved
// phases. The local search has no proof delta or calculus entry. The full
// root-phase operation also reproduces Kissat's dense-entry/sparse-resume
// normalization: root-satisfied and duplicate binary representations are
// removed, and large watches are rebuilt. It computes an assignment out of
// band and copies it into `h->saved` only if that assignment falsifies fewer
// clauses than the phases already encode, so a bad walk is a bad phase guess
// and never an unsound step. The application is Kissat's whole walking rephase
// slot: local search, then the tail every rephase type runs (target := saved,
// watermark reset, `stats.rephased++`). A caller in search applies Restart
// before Walk; when to do so remains policy state.
#pragma once

#include "rules/registry.hpp"
#include "strategy/rule_parameters.hpp"

namespace tactix {

// Instance payload: the step budget for this application, counted in
// occurrence-list entries scanned (kissat's `walk_steps`). INSTANCE_CANONICAL
// takes a bounded budget from `Kernel::work_budget`, otherwise the typed
// reference default. Every explicit payload, including zero, is exact.
// A typed seed_candidate names an exact retained publication. Seeded calls
// additionally charge phase/identity visits and two evaluations of the same
// current Walk CNF; export requires improvement over ordinary starting phases,
// even when the candidate itself required zero flips. An incomplete import,
// stale identity or zero seed allowance leaves all phase memories untouched.

RuleEligibility walk_guard(const Kernel &);
ApplyResult walk_apply(Kernel &);                 // canonical: budgeted round
ApplyResult walk_apply_steps(Kernel &, uint64_t); // explicit step budget

// Pure scoring seam used by validation/tests and by the walker. `walk_count`
// is one-based, matching Kernel::stats.walks after the application starts.
double walk_break_base(const strategy::WalkParameters &, double average_size,
                       uint64_t walk_count);

} // namespace tactix
