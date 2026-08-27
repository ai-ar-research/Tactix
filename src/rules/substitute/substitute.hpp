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

// Mechanism helper for callers that consume named equivalence classes
// immediately. Sweep uses the session-aware entry point below instead so it
// can retain the proved bridges and defer class retirement to canonical
// Substitute.
ApplyResult substitute_apply_equivalences(
    Kernel &, const std::vector<std::pair<Lit, Lit>> &);

// Sweep keeps one session across all productive environments in a call. Its
// connected replacements retain source pose and allocation span, while the
// output-sensitive mixed-occurrence view supplies Kissat's stream order only
// for literals reached by the environment or rewrite.
struct SweepSubstitutionSession final : ArenaOrdering {
  struct DenseOccurrence {
    bool binary = false;
    uint32_t payload = 0; // other literal for binary, arena Ref otherwise
  };
  struct EquivalenceBridge {
    Lit a = INVALID_LIT; // the emitted equivalence binary, as Sweep proved it
    Lit b = INVALID_LIT;
    // The endpoints the binary keeps until canonical Substitute: the emitted
    // pair rewritten by every LATER merge of the batch, never by its own.
    Lit target_a = INVALID_LIT;
    Lit target_b = INVALID_LIT;
  };

  Ref original_words = 0;
  bool changed = false;
  std::unordered_map<Ref, Ref> origin;
  std::unordered_map<Ref, Ref> latest;
  // Kissat's dense watch lists are one ordered stream containing binaries and
  // every irredundant large occurrence. Inline substitution can append either
  // kind, so two separately ordered Tactix indexes are not enough.
  //
  // Invariant, restored at every entry point below: for each literal the
  // binary sub-multiset of this ledger equals that literal's physical binary
  // watch multiset, and its large sub-multiset names the same irredundant
  // records as `Kernel::dense`. The ledger owns only the ORDER; the watch lists
  // and `Kernel::dense` own membership. A ledger that claims a clause the
  // formula does not hold puts a phantom clause into the next environment.
  struct DenseOccurrenceList {
    std::vector<DenseOccurrence> entries;
    bool initialized = false;
  };
  // Only literals reached by a Sweep environment need a unified stream.
  // A dense vector-of-vectors committed 24 bytes for every physical literal
  // before one stream was touched; sparse ownership retains the same per-list
  // order and capacity without that formula-wide header image.
  std::unordered_map<Lit, DenseOccurrenceList> dense_occurrences;
  // Kissat has no all-literals shadow build: it reads the one connected watch
  // stream when a literal is reached.  Tactix materializes the equivalent
  // binary-then-large view lazily and retains it only for literals touched in
  // this Sweep call.  The vector storage is retained across calls so giant
  // formulas do not repeatedly construct/destroy millions of empty vectors.
  std::vector<Lit> dense_initialized_lits;
  bool dense_enabled = false;
  // Directed owners whose ordered binary substream was changed by the current
  // inline transaction. Reconciled against physical watches before the next
  // Sweep seed consumes the ledger: a dirty substream is a prediction the
  // physical rewrite may supersede, so only there may `sync_new_binaries`
  // meet a ledger-only binary; on any other owner that residue is a defect.
  std::vector<Lit> dirty_binary_owners;

  // Kissat retires one literal at a time: `substitute_connected_clauses` walks
  // that literal's single unified watch stream and pushes every occurrence it
  // moves — binary or large, in encounter order — onto one `delayed` stack,
  // which it flushes to the representative before the next retiring literal is
  // walked. Tactix rewrites binaries and large clauses in two separate batch
  // passes, so a move instead records which stream released it and where it sat
  // in that stream, and the transaction replays all of them in that order.
  struct DelayedMove {
    Lit representative = INVALID_LIT;
    uint32_t source = 0;   // retiring literal's rank in Kissat's call order
    uint32_t position = 0; // its index in that literal's stream
    DenseOccurrence image;
  };

  void begin(const Kernel &);
  bool has_dense_occurrences() const { return dense_enabled; }
  std::vector<DenseOccurrence> &occurrences(const Kernel &, Lit);
  // Must precede publication of a fresh equivalence pair so a newly appended
  // binary remains after the pre-existing mixed stream, as in Kissat.
  void prime_equivalence(const Kernel &, Lit a, Lit b);
  // Adopt the binaries Sweep published for this batch. Must run for every
  // emitted batch, including one whose classes are all declined below, because
  // the binaries are already in the watch lists by then.
  void sync_new_binaries(
      const Kernel &, const std::vector<std::pair<Lit, Lit>> &equivalences);
  // `map` is the sparse requested literal-to-representative map. The rewrite
  // this mirrors declines a member that is already root-fixed or retired, so
  // this applies the same
  // predicate rather than the map; mirroring the map instead moves ledger
  // entries the kernel leaves in place. `sources` is every losing literal in
  // the order `sweep_equivalence` hands it to `substitute_connected_clauses`;
  // it opens the transaction that `flush_moves` closes.
  void rewrite_binary_occurrences(
      const Kernel &, const std::vector<std::pair<Lit, Lit>> &map,
      const std::vector<Lit> &sources,
      const std::vector<EquivalenceBridge> &bridges);
  const std::vector<Lit> &retiring_sources() const { return retiring; }
  bool substitutes(Lit) const;
  Lit representative(Lit) const;
  uint32_t retiring_index(Lit) const;
  // If (a, b) is a freshly proved equivalence binary, write its preserved
  // endpoints oriented to the query and return true. Kissat exempts such a
  // binary only from its own pair's `substitute_connected_clauses` call, so
  // the preserved image is the emitted pair under every later merge of the
  // batch; the transformed bridges let canonical Substitute rediscover every
  // still-active class member. False means an ordinary binary.
  bool equivalence_bridge_targets(Lit a, Lit b, Lit *target_a,
                                  Lit *target_b) const;
  // Materialize the streams of every literal of an irredundant connected
  // clause before the kernel rewrites, collapses or retires it. The ledger
  // entry points below locate the source by its pre-rewrite pose; a stream
  // first built after the kernel has marked the record garbage, dropped it
  // from `Kernel::dense`, or published its binary image cannot contain it.
  void prime_large(const Kernel &, Ref source);
  void remove_large(Kernel &, Ref source);
  void collapse_large_to_binary(Kernel &, Ref source, Lit a, Lit b,
                                bool append_a, bool append_b);
  // Mirror the ordered mixed-occurrence effect before the kernel overwrites a
  // surviving large clause at the same Ref.  This changes only Sweep's
  // traversal ledger; the general kernel primitive owns physical indexes.
  void rewrite_large_in_place(const Kernel &, Ref source, const Lit *image,
                              uint32_t size);
  void inherit(Kernel &, Ref replacement, Ref source);
  // Append the transaction's moved occurrences to their representatives, one
  // losing literal's stream at a time. Runs once per emission batch, after
  // the large-clause rewrite has decided what each large occurrence became.
  void flush_moves(const Kernel &);
  void order(const Kernel &, std::vector<Ref> &) override;
  uint64_t collect(Kernel &);

private:
  DenseOccurrence *find_large(const Kernel &, Lit, Ref);
  void erase_large(const Kernel &, Lit, Ref);
  void purge_stale_binaries(Lit, std::unordered_map<Lit, size_t> &);
  void reset_transaction();
  bool move_origin(const Clause &, Lit representative, Ref source,
                   bool final_binary,
                   uint32_t &rank, uint32_t &position) const;
  void push_move(const Kernel &, const Clause &, Lit representative, Ref source,
                 DenseOccurrence image);

  std::vector<DelayedMove> delayed_moves;
  std::vector<Lit> retiring;      // by rank
  std::vector<Lit> retiring_repr; // by rank
  std::unordered_map<Lit, uint32_t> retiring_rank;
  std::unordered_map<uint64_t, EquivalenceBridge> equivalence_bridges;
  // (retiring literal, source ref) -> that occurrence's stream index.
  std::unordered_map<uint64_t, uint32_t> retiring_position;
};

// Sweep commits consequences while its dense pass is still in flight. Kissat
// rewrites the proved class through connected binary/irredundant occurrences,
// but retains the equivalence binaries and active members for the following
// whole-formula Substitute pass. This entry point preserves that two-stage
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
