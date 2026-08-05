// S6 Walk (plan §5.2 policy-operations table, moved to phase 3 in rev 71;
// design docs/design/s6-walk-design.md; kissat walk.c / cadical walk.cpp):
// WalkSAT/ProbSAT local search that IMPROVES THE SAVED PHASES. Pure `H`
// (proof delta: none, no calculus entry, no ledger row) — the walker never
// touches clauses, the trail or the proof; it computes an assignment out of
// band and, if that assignment falsifies fewer clauses than the one the phases
// already encode, copies it into `h->saved`. A bad walk is a bad phase guess,
// never an unsound step, which is why S6 needs no certificate.
//
// The application is kissat's whole walking rephase slot: local search, then
// the same tail every rephase type runs (target := saved, watermark reset,
// `stats.rephased++`). WHEN to walk is policy state  — DefaultPolicy
// puts it in the B,W,I,B,W,O rotation.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

// instance payload: the step budget for this application (walk steps, the
// rule's own currency — occurrence-list entries scanned, kissat's `walk_steps`).
// INSTANCE_CANONICAL takes the budget from `Kernel::work_budget`, and falls
// back to WALK_DEFAULT_STEPS when that is 0. Any payload is applicable: the
// budget only decides how long the search runs, never whether it is sound.
constexpr uint64_t WALK_DEFAULT_STEPS = 1000000;

bool walk_guard(const Kernel &);
ApplyResult walk_apply(Kernel &);                 // canonical: budgeted round
ApplyResult walk_apply_steps(Kernel &, uint64_t); // explicit step budget

} // namespace tactix
