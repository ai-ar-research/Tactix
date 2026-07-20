// V3 Substitute (plan §5.2): equivalent-literal substitution over the binary
// implication graph. Literals in one SCC of the BIG are logically equivalent;
// each non-representative is rewritten to its SCC's representative, and an SCC
// that contains a literal together with its own negation forces UNSAT
// (l ≡ ¬l). Proof delta: add-RUP + delete + push-witness.
//
// Scope (complete):
//   V3a — SCC DETECTION (guard + iterative Tarjan), unit-tested in isolation.
//   V3b-1 — the UNSAT path (x ≡ ¬x ⇒ refutation: two RUP units + the empty
//     clause via the rule-local BIG-path walk) + registration in the DEFERRED
//     slot. Landed first to de-risk big_path_chain (the design's #1 risk) with a
//     mutation-free, LRAT-checkable increment before the rewrite depended on it.
//   V3b-3 — the equivalent-literal rewrite: every non-representative SCC member
//     is rewritten to its representative across all large + binary occurrences
//     (add-before-delete), transient equivalence lemmas justify each rewrite in
//     LRAT, size-collapsed root units are asserted un-propagated, and a
//     reconstruction witness pair is pushed per eliminated variable (first ρ
//     consumer). See do_substitution in substitute.cpp.
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
// rev 34). INSTANCE_CANONICAL = the full round (every non-trivial SCC); a single
// instance substitutes the requested representative's class + its mirror.
// On x ≡ ¬x, apply refutes (Unsat).
ApplyResult substitute_apply(Kernel &); // canonical: full round

// One directed binary-implication-graph path `from → … → to` as its edge
// clause-ids in FORWARD order (LRAT antecedent chain; caller guards on lrat).
// Edge p→q is the binary (¬p∨q). Returns false if no path exists. Reused by V6
// Congruence to bridge two gate inputs proven equal via added equivalences.
bool big_path_chain(const Kernel &k, Lit from, Lit to,
                    std::vector<uint64_t> &chain);

} // namespace tactix
