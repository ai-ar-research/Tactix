// V3 Substitute (plan §5.2): equivalent-literal substitution over the binary
// implication graph. Each non-representative SCC literal is rewritten to its
// representative; an SCC holding l and ¬l refutes. Proof delta: add-RUP +
// delete + push-witness (`docs/dev/calculus.md` §V3).
#pragma once

#include "rules/registry.hpp"

#include <unordered_map>
#include <utility>
#include <vector>

namespace tactix {

// Guard: quiescent root (SCCs need every level-0 unit reflected).
RuleEligibility substitute_guard(const Kernel &k);

// SCCs of the active binary implication graph ((¬p ∨ q) gives p → q).
//   repr[l] = the SCC member with the smallest import order (Kissat's compact
//             order); repr[l]==l for representatives and singletons;
//             INVALID_LIT for an inactive (root-fixed) literal.
//   nontrivial_reps = requested representatives of non-trivial SCCs, ascending
//             import order (the public instances); nontrivial_count their count.
//   contradiction / contradiction_lit = some x with x and ¬x in one SCC.
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
SccResult substitute_find_sccs(const Kernel &k, bool collect_instances = true,
                               bool account_work = true);

// Instance space (plan §5.1): payload = an SCC representative literal; one
// instance substitutes that class and its mirror. INSTANCE_CANONICAL = the
// full round. On x ≡ ¬x, apply refutes. As in Kissat (`substitute_binaries`,
// `substitute_clauses`, `kissat_watch_large_clauses`), large watches are
// flushed after the SCC pass and reconnected before returning when
// `reconnect_large_watches` holds; multi-round drivers pass false and call
// `substitute_reconnect_large_watches` after the last round.
ApplyResult substitute_apply(Kernel &);
ApplyResult substitute_apply_round(Kernel &, bool reconnect_large_watches);
// Kissat's `kissat_watch_large_clauses` + `kissat_reset_propagate` + probing
// propagation: reconnect in arena order and close the root over large clauses
// for units since `trail_begin`. Unsat iff that refutes.
ApplyResult substitute_reconnect_large_watches(Kernel &, uint32_t trail_begin);

// Kissat's `complete=true` probing: repeat productive rounds, capped by
// `substituterounds=2`.
ApplyResult substitute_apply_complete(Kernel &, uint32_t max_rounds);

// Sweep keeps the proved bridges and leaves class retirement to canonical
// Substitute; one session spans a call. Connected replacements keep pose and
// span; the mixed-occurrence view gives Kissat's stream order for reached
// literals.
struct SweepSubstitutionSession final {
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

  bool changed = false;
  // Kissat's dense watch list is one stream of binaries and irredundant large
  // occurrences. Invariant at every entry point: per literal, the binary part
  // of the ledger equals the physical binary watch multiset and the large part
  // names the same records as `Kernel::dense`; the ledger's binary order is
  // published to the watches. Watches and `Kernel::dense` own membership.
  struct DenseOccurrenceList {
    std::vector<DenseOccurrence> entries;
    bool initialized = false;
  };
  // Streams only for literals reached by a Sweep environment.
  std::unordered_map<Lit, DenseOccurrenceList> dense_occurrences;
  // Lazily materialized binary-then-large view of touched literals; storage is
  // kept across calls.
  std::vector<Lit> dense_initialized_lits;
  // Literals whose stream may have changed since the last reconcile (marked by
  // the non-const `occurrences`).
  std::vector<Lit> ledger_dirty;
  std::vector<uint8_t> ledger_dirty_mark;
  bool dense_enabled = false;
  // Owners whose binary substream the current transaction changed; only there
  // may `sync_new_binaries` meet a ledger-only binary.
  std::vector<Lit> dirty_binary_owners;

  // Kissat's `substitute_connected_clauses` pushes each moved occurrence of one
  // retiring literal onto a `delayed` stack in encounter order and flushes it
  // before the next literal. A move records its source stream and position so
  // the transaction replays that order.
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
  // Adopt Sweep's published binaries; required for every batch, even fully
  // declined ones.
  void sync_new_binaries(
      Kernel &, const std::vector<std::pair<Lit, Lit>> &equivalences);
  // Pose-only: move the last copy of each fresh bridge to this pair's commit
  // boundary.
  void repose_emitted_equivalence(const Kernel &, Lit a, Lit b);
  // `map` is the sparse literal-to-representative map; members the rewrite
  // declines (root-fixed or retired) are skipped the same way. `sources` lists
  // losing literals in `sweep_equivalence` order and opens the transaction
  // `flush_moves` closes.
  void rewrite_binary_occurrences(
      const Kernel &, const std::vector<std::pair<Lit, Lit>> &map,
      const std::vector<Lit> &sources,
      const std::vector<EquivalenceBridge> &bridges);
  const std::vector<Lit> &retiring_sources() const { return retiring; }
  bool substitutes(Lit) const;
  Lit representative(Lit) const;
  uint32_t retiring_index(Lit) const;
  // If (a, b) is a fresh equivalence binary, write its preserved endpoints
  // oriented to the query and return true (Kissat exempts it only from its own
  // pair's `substitute_connected_clauses`). False means an ordinary binary.
  bool equivalence_bridge_targets(Lit a, Lit b, Lit *target_a,
                                  Lit *target_b) const;
  // Materialize the streams of an irredundant clause's literals before the
  // kernel rewrites, collapses or retires it.
  void prime_large(const Kernel &, Ref source);
  void remove_large(Kernel &, Ref source);
  void collapse_large_to_binary(Kernel &, Ref source, Lit a, Lit b,
                                bool append_a, bool append_b);
  // Mirror an in-place large overwrite in Sweep's ledger only.
  void rewrite_large_in_place(const Kernel &, Ref source, const Lit *image,
                              uint32_t size);
  // Append moved occurrences to their representatives, one losing literal at a
  // time, once per batch after the large rewrite.
  void flush_moves(const Kernel &);
  uint64_t collect(Kernel &);

private:
  // Republishes only `lits`, the streams reconciled by this batch.
  void publish_dense_order(Kernel &, const std::vector<Lit> &lits);
  void mark_ledger_dirty(Lit);
  bool sync_publication_is_complete(const Kernel &);
  DenseOccurrence *find_large(const Kernel &, Lit, Ref);
  void erase_large(const Kernel &, Lit, Ref);
  void purge_stale_binaries(Lit);
  // Epoch-stamped neighbour scratch for the binary-image pre-pass.
  std::vector<uint32_t> live_epoch;  // by lit
  uint32_t live_generation = 0;

public:
  // Mark which (key, owner) images already existed before the first push.
  void classify_existing_binaries(const Kernel &, const std::vector<Lit> &owners,
                                  const std::vector<Lit> &others,
                                  std::vector<uint8_t> &out);

private:
  // Epoch-stamped multiset scratch for `sync_new_binaries`.
  std::vector<uint32_t> sync_epoch;  // by lit
  std::vector<size_t> sync_count;    // by lit, valid when the epoch matches
  std::vector<Lit> sync_touched;
  uint32_t sync_generation = 0;
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

// Sweep commits consequences mid-pass: like Kissat, rewrite the class through
// connected occurrences but keep the bridges and members for the later
// Substitute pass. Caller is at level zero and owns the root closure.
ApplyResult substitute_apply_sweep_equivalences(
    Kernel &, const std::vector<std::pair<Lit, Lit>> &,
    SweepSubstitutionSession &,
    std::vector<std::pair<Lit, Lit>> *applied_images = nullptr);

} // namespace tactix
