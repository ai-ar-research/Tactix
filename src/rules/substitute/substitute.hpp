// V3 Substitute (plan §5.2): equivalent-literal substitution over the binary
// implication graph. Literals in one SCC of the graph are logically equivalent;
// each non-representative is rewritten to its SCC's representative, and an SCC
// holding a literal together with its own negation refutes the formula.
// Proof delta: add-RUP + delete + push-witness. Precondition, effect and
// soundness: `docs/calculus.md` §V3; engineering invariants:
// `docs/implementation-notes.md`. Three parts: SCC detection (iterative
// Tarjan, unit-tested in isolation), the refutation path for l ≡ ¬l (two RUP
// units and the empty clause via the rule-local graph-path walk), and the
// rewrite itself (`do_substitution` in substitute.cpp).
#pragma once

#include "rules/registry.hpp"

#include <unordered_map>
#include <utility>
#include <vector>

namespace tactix {

// Guard: quiescent root, shared by every inprocessing rule — the binary
// implication graph must already reflect all level-0 units before its SCCs are
// meaningful.
RuleEligibility substitute_guard(const Kernel &k);

// SCCs of the active binary implication graph: (¬p ∨ q) gives p → q.
//   repr[l]  = the representative with the smallest DIMACS import order in
//              l's SCC (Kissat's compact internal-literal order);
//              repr[l]==l for a representative or a singleton; INVALID_LIT for
//              an inactive (root-fixed) literal that is not a node.
//   nontrivial_reps = requested representatives of SCCs with more than one
//              literal, in ascending import order — the public instances;
//   nontrivial_count = their count even when not materialized.
//   contradiction   = some SCC contains a literal and its own negation.
//   contradiction_lit = a witness x with x and ¬x in one SCC (INVALID_LIT if
//              none); both directed paths x→¬x and ¬x→x then exist, which the
//              refutation walks.
struct SccResult {
  std::vector<Lit> repr;            // by lit
  std::vector<Lit> nontrivial_reps; // representatives of |SCC| > 1, ascending
  uint64_t nontrivial_count = 0;
  // Kissat's integrated failed-root channel: if one DFS root reaches a literal
  // and its negation in distinct SCCs, learn its negation as a root unit.
  std::vector<Lit> failed_units;
  std::vector<Lit> failed_witness;
  bool contradiction = false;
  Lit contradiction_lit = INVALID_LIT;
};
SccResult substitute_find_sccs(const Kernel &k, bool collect_instances = true);

// Instance space (plan §5.1): payload = an SCC representative literal via
// lit_instance/lit_of_instance. A single instance substitutes the requested
// representative's class and its mirror; INSTANCE_CANONICAL = the full round
// (every non-trivial SCC). On x ≡ ¬x, apply refutes.
ApplyResult substitute_apply(Kernel &); // canonical: full round

// Initial probing uses Kissat's `complete=true` contract: repeat productive
// rounds so equivalences exposed by one rewrite are consumed by the next.
// The shipped reference cap is two rounds (`substituterounds=2`).
ApplyResult substitute_apply_complete(Kernel &, uint32_t max_rounds = 2);

// Mechanism helper for Sweep's reference-inline application: rewrite only the
// SCC classes named by equivalence binaries that the caller has just proved
// and added. This is not a scheduling call to the Substitute rule; it is the
// same proof-safe add-before-delete kernel used by that rule, restricted to
// the supplied classes.
ApplyResult substitute_apply_equivalences(
    Kernel &, const std::vector<std::pair<Lit, Lit>> &);

// Tactix's variable-width arena cannot shrink a clause in place.  Sweep keeps
// one of these ledgers across all productive environments in a call: each
// connected replacement inherits its source pose, garbage is collected once
// after the reference-ordered consequence sequence, and the resulting arena
// has the same logical clause order it had before the call.
struct SweepSubstitutionSession final : ArenaOrdering {
  struct DenseOccurrence {
    bool binary = false;
    uint32_t payload = 0; // other literal for binary, arena Ref otherwise
  };

  Ref original_words = 0;
  bool changed = false;
  std::unordered_map<Ref, Ref> origin;
  std::unordered_map<Ref, Ref> latest;
  // Kissat's dense watch lists are one ordered stream containing binaries and
  // every irredundant large occurrence. Inline substitution can append either
  // kind, so two separately ordered Tactix indexes are not enough.
  std::vector<std::vector<DenseOccurrence>> dense_occurrences;

  void begin(const Kernel &);
  void sync_new_binaries(
      const Kernel &, const std::vector<std::pair<Lit, Lit>> &equivalences);
  void rewrite_binary_occurrences(const Kernel &, const std::vector<Lit> &repr,
                                  const std::vector<uint8_t> &sub);
  void remove_large(Kernel &, Ref source);
  void collapse_large_to_binary(Kernel &, Ref source, Lit a, Lit b,
                                bool append_a, bool append_b);
  void inherit(Kernel &, Ref replacement, Ref source);
  void order(const Kernel &, std::vector<Ref> &) override;
  uint64_t collect(Kernel &);
};

// Sweep commits consequences while its dense pass is still in flight.  Kissat
// rewrites the proved class immediately but does not propagate newly created
// root units until the Sweep call ends.  This entry point preserves that
// lifecycle: the caller must be at level zero and owns the eventual root
// closure.
ApplyResult substitute_apply_sweep_equivalences(
    Kernel &, const std::vector<std::pair<Lit, Lit>> &,
    SweepSubstitutionSession &);

// One directed binary-implication-graph path `from → … → to` as its edge
// clause-ids in FORWARD order (LRAT antecedent chain; caller guards on lrat).
// Edge p→q is the binary (¬p∨q). Returns false if no path exists. Reused by V6
// Congruence to bridge two gate inputs proven equal via added equivalences.
bool big_path_chain(const Kernel &k, Lit from, Lit to,
                    std::vector<uint64_t> &chain);

} // namespace tactix
