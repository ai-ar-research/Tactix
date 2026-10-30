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

// Explicit payloads, including zero, give exact occurrence-scan allowances.
// Seeded imports must improve the same current-CNF objective; incomplete,
// stale or interrupted work preserves phase memories. FullCurrentCnf separates
// preparation and search doses; see docs/dev/controller-interface.md (Walk).

RuleEligibility walk_guard(const Kernel &);
ApplyResult walk_apply(Kernel &);                 // canonical: budgeted round
ApplyResult walk_apply_steps(Kernel &, uint64_t); // explicit step budget

// Pure scoring seam used by validation/tests and by the walker. `walk_count`
// is one-based, matching Kernel::stats.walks after the application starts.
double walk_break_base(const strategy::WalkParameters &, double average_size,
                       uint64_t walk_count);

// Structural census used by strategy policies. Returns the per-mille share of
// live irredundant clauses that are ternary clauses with an exact literalwise
// complement in the current formula. No benchmark identity is consulted.
uint32_t walk_complementary_ternary_fraction_permille(const Kernel &);

struct UniformNegativeWalkCensus {
  uint64_t clauses = 0;
  uint64_t unsatisfied = 0;
};

// Structural census for a configuration-checking startup profile. Root-true
// clauses are already satisfied; root-false literals are ignored; every
// remaining variable is assigned false. Each physical irredundant clause is
// counted once. No formula identity or semantic naming is consulted.
UniformNegativeWalkCensus walk_uniform_negative_census(const Kernel &);

} // namespace tactix
