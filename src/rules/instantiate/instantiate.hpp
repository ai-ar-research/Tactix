// Instantiate, calculus rule X4 (plan §5.2): removal of a rare literal from a
// clause. For C = D ∨ l, assume l true and every literal of D false and
// propagate; a conflict proves F ⊨ ¬l ∨ D, which resolves against C to give D,
// so C may be replaced by the strictly shorter D.
// Proof delta: add-RUP (D) + delete (C), C2 Strengthen's pair — no witness, no
// new primitive. Precondition, effect and the reason D is RUP against F alone:
// `docs/dev/calculus.md` §X4.
// Reference: cadical `instantiate.cpp` (kissat has no instantiation rule).
// Value: the removed literal's variable occurs at most INST_OCCLIM times in
// that polarity, so removing it tends to make the variable pure or nearly
// pure — this rule feeds BVE (rule E1) rather than paying by itself, and must
// be judged with its family (X1/X2/X4), never on a lone arm.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: declared quiescent-root boundary — occurrence counts and the "unassigned"
// filters only mean anything once every level-0 unit is propagated into the
// clause set.
RuleEligibility instantiate_guard(const Kernel &);

// Frozen v1 instance space: payload = candidate arena Ref, in arena order; an
// application tries its configured eligible literals in clause order. This
// grouped payload remains for replay compatibility. The registry's additive
// semantic surface exposes each (clause,literal) choice independently.
void instantiate_candidates(const Kernel &, std::vector<Ref> &out);

// One clause: try its eligible literals in clause order; the first success
// rewrites and returns, where cadical keeps going within the clause. Rounds
// repeat through the slot schedule instead, which also keeps the arena stable
// for the duration of one probe. Root-falsified literals of C are kept in D;
// dropping them would mix two strengthenings into one delta for no new
// reachable state, and the next Vivify round removes them.
ApplyResult instantiate_try(Kernel &, Ref candidate);
ApplyResult instantiate_try_literal(Kernel &, Ref candidate, Lit literal);

inline uint64_t instantiate_pair_instance(Ref candidate, Lit literal) {
  return (static_cast<uint64_t>(candidate) << 32) |
         static_cast<uint32_t>(literal);
}
inline void instantiate_pair_of_instance(uint64_t instance, Ref *candidate,
                                         Lit *literal) {
  *candidate = static_cast<Ref>(instance >> 32);
  *literal = static_cast<Lit>(static_cast<uint32_t>(instance));
}

// Canonical: the full round. No steps budget in the rule — the round is
// budget_aware, reading `Kernel::work_budget` in propagation ticks.
ApplyResult instantiate_apply(Kernel &);

} // namespace tactix
