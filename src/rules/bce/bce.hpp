// BCE, calculus rule X1 (plan §5.2): blocked clause elimination. A clause `C`
// blocked on a literal `l ∈ C` — every resolvent on `l` with a `¬l`-clause is
// tautological — may be deleted. Not equivalent, only equisatisfiable: a model
// of `F \ {C}` is repaired by setting `l` true, which is exactly the
// reconstruction witness, and `ρ` must record it or SAT answers go wrong.
// Proof delta: `del` + `push-witness`. Nothing is added, so there is no chain
// and no format-specific hazard; blockedness is a solver-soundness obligation
// discharged by the test, not a proof obligation.
// Precondition, effect, soundness: `docs/calculus.md` §X1. Reference: cadical
// `block.cpp` (kissat 4 has no BCE); Eén-Biere is the source result.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: declared quiescent-root boundary — the tautology test reads the live occurrence
// structure, which only means what it says once every level-0 unit is
// propagated.
RuleEligibility bce_guard(const Kernel &);

// Frozen v1 instance space (plan §5.1): payload = the blocking literal. One
// instance deletes every irredundant clause blocked on it. The additive
// semantic surface names each (clause, blocking-literal) choice independently,
// including binary pairs; its opaque payloads are valid until the next formula
// mutation. Canonical order: literals with at least one irredundant occurrence,
// ascending by code. INSTANCE_CANONICAL = the full round.
void bce_literals(const Kernel &, std::vector<Lit> &out);

// One literal: delete every irredundant clause blocked on it, pushing a witness
// per deletion. Ok = something deleted, NoOp = nothing.
//
// Candidates are irredundant (S2 Reduce may drop a redundant clause for free,
// so blocking one buys nothing and costs a `ρ` segment) and never units (a root
// unit is already propagated into the formula). A pure literal needs no fast
// path: with no `¬l`-occurrence the "for every `¬l`-clause" quantifier is
// vacuous, so BCE subsumes pure-literal elimination here. Within-rule scope
// choices and the ledger rows: `docs/implementation-notes.md` §1 (X1 row).
ApplyResult bce_try(Kernel &, Lit blocking);
ApplyResult bce_apply(Kernel &); // canonical: the full round

} // namespace tactix
