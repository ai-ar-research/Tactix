// V3 Substitute (plan §5.2): equivalent-literal substitution over the binary
// implication graph. Literals in one SCC of the graph are logically equivalent;
// each non-representative is rewritten to its SCC's representative, and an SCC
// holding a literal together with its own negation refutes the formula.
// Proof delta: add-RUP + delete + push-witness. Precondition, effect and
// soundness: `docs/calculus.md` §V3; design record
// `docs/design/v3-substitute-design.md`. Three parts: SCC detection (iterative
// Tarjan, unit-tested in isolation), the refutation path for l ≡ ¬l (two RUP
// units and the empty clause via the rule-local graph-path walk), and the
// rewrite itself (`do_substitution` in substitute.cpp).
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root, shared by every inprocessing rule — the binary
// implication graph must already reflect all level-0 units before its SCCs are
// meaningful.
bool substitute_guard(const Kernel &k);

// SCC decomposition of the binary implication graph (edges p → q for every
// binary clause (¬p ∨ q); only active — unassigned — literals are nodes).
//   repr[l]  = the representative (numerically smallest) literal of l's SCC;
//              repr[l]==l for a representative or a singleton; INVALID_LIT for
//              an inactive (root-fixed) literal that is not a node.
//   nontrivial_reps = representatives of SCCs with more than one literal, in
//              ascending order — the substitution instances.
//   contradiction   = some SCC contains a literal and its own negation.
//   contradiction_lit = a witness x with x and ¬x in one SCC (INVALID_LIT if
//              none); both directed paths x→¬x and ¬x→x then exist, which the
//              refutation walks.
struct SccResult {
  std::vector<Lit> repr;            // by lit
  std::vector<Lit> nontrivial_reps; // representatives of |SCC| > 1, ascending
  bool contradiction = false;
  Lit contradiction_lit = INVALID_LIT;
};
SccResult substitute_find_sccs(const Kernel &k);

// Instance space (plan §5.1): payload = an SCC representative literal via
// lit_instance/lit_of_instance. A single instance substitutes the requested
// representative's class and its mirror; INSTANCE_CANONICAL = the full round
// (every non-trivial SCC). On x ≡ ¬x, apply refutes.
ApplyResult substitute_apply(Kernel &); // canonical: full round

// One directed binary-implication-graph path `from → … → to` as its edge
// clause-ids in FORWARD order (LRAT antecedent chain; caller guards on lrat).
// Edge p→q is the binary (¬p∨q). Returns false if no path exists. Reused by V6
// Congruence to bridge two gate inputs proven equal via added equivalences.
bool big_path_chain(const Kernel &k, Lit from, Lit to,
                    std::vector<uint64_t> &chain);

} // namespace tactix
