// V3 Substitute (plan §5.2): equivalent-literal substitution over the binary
// implication graph. Literals in one SCC of the BIG are logically equivalent;
// each non-representative is rewritten to its SCC's representative, and an SCC
// that contains a literal together with its own negation forces UNSAT
// (l ≡ ¬l). Proof delta: add-RUP + delete + push-witness.
//
// V3a scope (this file): SCC DETECTION only — the guard and the iterative
// Tarjan pass. The clause rewrite, LRAT chains, witness pushes, UNSAT path,
// and rule registration land in V3b; keeping detection separate lets the
// graph algorithm be unit-tested in isolation before the proof machinery.
#pragma once

#include "kernel/kernel.hpp"

#include <vector>

namespace tactix {

// Quiescent-root fixpoint precondition, shared by every inprocessing rule
// (C1/C2/C3/V1/V2): the binary-implication graph must already reflect all
// level-0 units before its SCCs are meaningful.
bool substitute_guard(const Kernel &k);

// SCC decomposition of the binary implication graph (edges p → q for every
// binary clause (¬p ∨ q); only active — unassigned — literals are nodes).
//   repr[l]  = the representative (numerically smallest) literal of l's SCC;
//              repr[l]==l for a representative or a singleton; INVALID_LIT for
//              an inactive (root-fixed) literal that is not a node.
//   nontrivial_reps = representatives of SCCs with more than one literal, in
//              ascending order — the substitution instances (each collapses a
//              whole SCC onto its representative).
//   contradiction   = some SCC contains a literal and its own negation, i.e.
//              l ≡ ¬l, so the formula is UNSAT.
struct SccResult {
  std::vector<Lit> repr;            // by lit
  std::vector<Lit> nontrivial_reps; // representatives of |SCC| > 1, ascending
  bool contradiction = false;
};
SccResult substitute_find_sccs(const Kernel &k);

} // namespace tactix
