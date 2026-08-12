// CCE, calculus rule X3 (plan §5.2): covered clause elimination, the
// asymmetric variant the literature still calls CCE. It generalises rule X1:
// instead of testing a clause `C` for blockedness as written, grow it by
// asymmetric literal addition (ALA) and covered literal addition (CLA) until
// the extension is blocked — or falsified, the ATA exit — then delete `C`.
// Proof delta: `del` + one `push-witness` per CLA step, plus the blocked exit's
// final segment; an ATA exit with no CLA step in front of it pushes nothing.
// Nothing is added, so there is no chain and no format-specific hazard.
// Precondition, the ALA-closure lemma and the full reconstruction argument:
// `docs/calculus.md` §X3. Reference: cadical `cover.cpp` (kissat has no CCE);
// Heule-Järvisalo-Biere is the source result.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root (D-ROOT) — every test reads the live occurrence
// structure, which only means what it says once each level-0 unit is
// propagated.
RuleEligibility cce_guard(const Kernel &);

// Instance space (plan §5.1): payload = a literal. One instance = one literal =
// attempt CCE on every candidate clause containing it; per-clause instances
// would need clause identity in the payload, which the v1 encoding does not
// carry (and `Ref`s move under GC). Canonical order: every root-unassigned
// literal, ascending by code. INSTANCE_CANONICAL = the full round.
void cce_literals(const Kernel &, std::vector<Lit> &out);

// One literal: attempt CCE on every candidate clause containing it, witnessing
// each elimination. Ok = something deleted, NoOp = nothing.
//
// The two sides quantify differently, and that asymmetry is load-bearing.
// CLA/blockedness runs over every live non-root-satisfied clause, redundant
// ones included: over-quantifying only makes the test stricter, and filtering
// that side is what produced X1's fuzz-2204 wrong-SAT. ALA runs over
// irredundant clauses only, because the ALA-closure lemma needs the justifying
// clause satisfied when `ρ` replays, and S2 Reduce drops a redundant clause
// with no witness. Candidate scope and the rest of the within-rule choices:
// `docs/implementation-notes.md` §1 (X3 row) and ledger rows D-CCE-*.
ApplyResult cce_try(Kernel &, Lit l);
ApplyResult cce_apply(Kernel &); // canonical: the full round

} // namespace tactix
