// S6 Walk (plan §5.2; design docs/design/s6-walk-design.md; kissat walk.c /
// cadical walk.cpp): WalkSAT/ProbSAT local search that improves the saved
// phases. The local search is `H`-only and has no proof delta or calculus
// entry. The full operation also reproduces Kissat's dense-entry/sparse-resume
// normalization: root-satisfied and duplicate binary representations are
// removed, and large watches are rebuilt. It computes an assignment out of
// band and copies it into `h->saved` only if that assignment falsifies fewer
// clauses than the phases already encode, so a bad walk is a bad phase guess
// and never an unsound step. The application is Kissat's whole walking rephase
// slot: local search, then the tail every rephase type runs (target := saved,
// watermark reset, `stats.rephased++`). When to walk is policy state.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

// Instance payload: the step budget for this application, counted in
// occurrence-list entries scanned (kissat's `walk_steps`). INSTANCE_CANONICAL
// takes the budget from `Kernel::work_budget`, falling back to
// WALK_DEFAULT_STEPS when that is 0. Any payload is applicable — the budget
// decides how long the search runs, never whether it is sound.
constexpr uint64_t WALK_DEFAULT_STEPS = 1000000;

RuleEligibility walk_guard(const Kernel &);
ApplyResult walk_apply(Kernel &);                 // canonical: budgeted round
ApplyResult walk_apply_steps(Kernel &, uint64_t); // explicit step budget

} // namespace tactix
