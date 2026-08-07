// Instantiate, calculus rule X4 (plan §5.2): removal of a rare literal from a
// clause. For C = D ∨ l, assume l true and every literal of D false and
// propagate; a conflict proves F ⊨ ¬l ∨ D, which resolves against C to give D,
// so C may be replaced by the strictly shorter D.
// Proof delta: add-RUP (D) + delete (C), C2 Strengthen's pair — no witness, no
// new primitive. Precondition, effect and the reason D is RUP against F alone:
// `docs/calculus.md` §X4.
// Reference: cadical `instantiate.cpp` (kissat has no instantiation rule).
// Value: the removed literal's variable occurs at most INST_OCCLIM times in
// that polarity, so removing it tends to make the variable pure or nearly
// pure — this rule feeds BVE (rule E1) rather than paying by itself, and must
// be judged with its family (X1/X2/X4), never on a lone arm.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// cadical options.hpp defaults: instantiateclslim 3, instantiateocclim 1
constexpr uint32_t INST_CLSLIM = 3;  // minimum candidate clause size
constexpr uint32_t INST_OCCLIM = 1;  // maximum occurrences of the literal

// Guard: quiescent root (D-ROOT) — occurrence counts and the "unassigned"
// filters only mean anything once every level-0 unit is propagated into the
// clause set.
bool instantiate_guard(const Kernel &);

// Instance space (plan §5.1): payload = candidate arena Ref, in arena order.
// One instance = one clause = all of its eligible literals, since every removal
// a per-(clause, literal) surface could reach is reached by applying the
// clause. Candidates are irredundant, matching cadical's occurrence lists —
// distilling learned tiers is C3 Vivify's job here. There is deliberately no
// "already tried" flag (cadical's `instantiateonce`): that is a scheduling
// memo, and under N1 the same clause must stay applicable at any time.
void instantiate_candidates(const Kernel &, std::vector<Ref> &out);

// One clause: try its eligible literals in clause order; the first success
// rewrites and returns, where cadical keeps going within the clause. Rounds
// repeat through the slot schedule instead, which also keeps the arena stable
// for the duration of one probe. Root-falsified literals of C are kept in D;
// dropping them would mix two strengthenings into one delta for no new
// reachable state, and the next Vivify round removes them.
ApplyResult instantiate_try(Kernel &, Ref candidate);

// Canonical: the full round. No steps budget in the rule — the round is
// budget_aware, reading `Kernel::work_budget` in propagation ticks.
ApplyResult instantiate_apply(Kernel &);

} // namespace tactix
