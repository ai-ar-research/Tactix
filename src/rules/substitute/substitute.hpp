// V3 Substitute (plan §5.2): equivalent-literal substitution over the binary
// implication graph. Literals in one SCC of the BIG are logically equivalent;
// each non-representative is rewritten to its SCC's representative, and an SCC
// that contains a literal together with its own negation forces UNSAT
// (l ≡ ¬l). Proof delta: add-RUP + delete + push-witness.
//
// Scope so far:
//   V3a — SCC DETECTION (guard + iterative Tarjan), unit-tested in isolation.
//   V3b-1 (this commit) — the UNSAT path + registration: an SCC containing a
//     literal and its own negation means x ≡ ¬x, so the formula is UNSAT; emit
//     the refutation (two RUP units + the empty clause, chains via the
//     rule-local BIG-path walk) and register the rule in the DEFERRED slot.
//     No clause rewrite yet — a non-contradictory round is a NoOp. Landing the
//     UNSAT path first de-risks big_path_chain (the design's #1 risk) with a
//     fully LRAT-checkable, mutation-free increment before the rewrite depends
//     on it.
//   V3b-3 (next) — the equivalent-literal rewrite + reconstruction witnesses +
//     size-collapse units.
#pragma once

#include "rules/registry.hpp"

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
//   contradiction_lit = a witness x with x and ¬x in one SCC (INVALID_LIT if
//              none); both directed paths x→¬x and ¬x→x then exist (the SCC is
//              strongly connected), which the UNSAT proof walks.
struct SccResult {
  std::vector<Lit> repr;            // by lit
  std::vector<Lit> nontrivial_reps; // representatives of |SCC| > 1, ascending
  bool contradiction = false;
  Lit contradiction_lit = INVALID_LIT;
};
SccResult substitute_find_sccs(const Kernel &k);

// Rule surface (registered in the DEFERRED inprocessing slot, so DefaultPolicy
// never runs it; N1-exposed + random-policy-fuzzed). Payload = an SCC
// representative literal via lit_instance/lit_of_instance (registry.hpp;
// rev 34). INSTANCE_CANONICAL = the full round. V3b-1: apply detects and, on
// x ≡ ¬x, refutes; otherwise NoOp (no rewrite until V3b-3).
ApplyResult substitute_apply(Kernel &); // canonical: full round

} // namespace tactix
