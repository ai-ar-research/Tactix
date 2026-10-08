#pragma once

#include "kernel/clause.hpp"
#include "kernel/connected_occurrence.hpp"
#include "kernel/occ.hpp"
#include "kernel/types.hpp"
#include "kernel/watch.hpp"
#include "proof/proof.hpp"
#include "rules/lucky/graph_labeling_session.hpp"
#include "strategy/analyze_parameters.hpp"
#include "strategy/congruence_parameters.hpp"
#include "strategy/parity_consequence_parameters.hpp"
#include "strategy/forward_parameters.hpp"
#include "strategy/failed_literal_parameters.hpp"
#include "strategy/instantiate_parameters.hpp"
#include "strategy/local_search.hpp"
#include "strategy/rule_parameters.hpp"
#include "strategy/semantic_variable.hpp"
#include "strategy/theory_action_parameters.hpp"
#include "strategy/vivify_parameters.hpp"

#include <cassert>
#include <array>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace tactix {

struct Kernel;
struct AffineBasisActionSession;
struct ExactCoverExtensionActionSession;
struct IndependentSetCoverActionSession;
class Gf3AffineActionSession;
class DirectCspSupportActionSession;
class DirectCspGuardedCoverActionSession;
class NaryCspRelationOwnerSession;
class NaryCspEqualitySession;
class NaryCspEqualityTransitivitySession;
class NaryCspEqualityAmoSession;
class DirectPhpActionSession;
class Gf3GaussianActionSession;
class Gf3GeneralRowActionSession;
class TseitinCycleBreakingSession;
class AffineMatrixSearchSession;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
void rule_journal_note_clause_transition(const Kernel &, const char *,
                                         const uint32_t *, uint32_t,
                                         uint64_t);
void rule_journal_note_root_assignment(const Kernel &, uint32_t, uint32_t,
                                       bool);
void rule_journal_note_variable_status(const Kernel &, uint32_t,
                                       const char *);
#endif

// Binary clauses are numerous enough that std::unordered_map's one allocation
// per entry is both a parse-time and a memory bottleneck.  This table has the
// deliberately narrow interface the kernel needs.  Native LRAT retains exact
// keys and latest proof ids; DRAT file mode uses the watch lists as its exact
// membership authority and keeps only the size-monotone addition count.
// Open addressing in the identity-bearing mode also matches the dense
// representation used by the reference solver more closely than a node-based
// container.
class BinaryIdMap {
  static constexpr uint64_t EMPTY = UINT64_MAX;
  // Keep keys and proof ids in separate arrays.  Native LRAT needs both;
  // DRAT names deletions by literals and ignores antecedent ids, so file
  // parsing can omit the value array while retaining the identical key table
  // used for binary deduplication and membership.
  std::vector<uint64_t> keys_;
  std::vector<uint64_t> values_;
  size_t size_ = 0;
  bool track_keys_ = true;
  bool track_values_ = true;

  static uint64_t hash(uint64_t x) {
    x += UINT64_C(0x9e3779b97f4a7c15);
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
  }

  size_t locate(uint64_t key) const {
    assert(!keys_.empty());
    const size_t mask = keys_.size() - 1;
    size_t pos = static_cast<size_t>(hash(key)) & mask;
    while (keys_[pos] != EMPTY && keys_[pos] != key)
      pos = (pos + 1) & mask;
    return pos;
  }

  void rehash(size_t capacity) {
    std::vector<uint64_t> old_keys, old_values;
    old_keys.swap(keys_);
    old_values.swap(values_);
    keys_.assign(capacity, EMPTY);
    if (track_values_)
      values_.assign(capacity, 0);
    size_ = 0;
    for (size_t i = 0; i < old_keys.size(); i++)
      if (old_keys[i] != EMPTY)
        insert_or_assign(old_keys[i],
                         track_values_ ? old_values[i] : UINT64_C(1));
  }

public:
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  bool tracks_keys() const { return track_keys_; }
  bool tracks_values() const { return track_values_; }
#ifdef TACTIX_ENABLE_EVENT_TRACE
  size_t diagnostic_storage_bytes() const {
    return (keys_.capacity() + values_.capacity()) * sizeof(uint64_t);
  }
#endif

  void discard_storage() {
    // The exact pair count is a monotone scheduling watermark independent of
    // the optional key/id archive.  DRAT can therefore release the import-time
    // deduplication table after the input is complete while preserving size_.
    track_keys_ = false;
    track_values_ = false;
    keys_.clear();
    keys_.shrink_to_fit();
    values_.clear();
    values_.shrink_to_fit();
  }

  void discard_values() {
    assert(track_keys_);
    assert(size_ == 0 && "proof-id mode is fixed before binary import");
    track_values_ = false;
    values_.clear();
    values_.shrink_to_fit();
  }

  // Linear probing remains well behaved for this one-shot archive at 7/8:
  // keys are never erased and the split key/value layout keeps probes to one
  // cache stream in DRAT mode.  The former 0.7 target rounded common
  // hundred-million-key formulas up by an otherwise unnecessary power of two.
  void reserve(size_t entries) {
    if (!track_keys_)
      return;
    size_t capacity = 16;
    while (entries > (capacity * 7) / 8)
      capacity <<= 1;
    if (capacity > keys_.size())
      rehash(capacity);
  }

  size_t count(uint64_t key) const {
    if (!track_keys_)
      return 0;
    return keys_.empty() ? 0 : keys_[locate(key)] == key;
  }

  uint64_t get(uint64_t key) const {
    if (!track_keys_)
      return UINT64_C(1);
    if (keys_.empty())
      return 0;
    const size_t pos = locate(key);
    if (keys_[pos] != key)
      return 0;
    return track_values_ ? values_[pos] : UINT64_C(1);
  }

  uint64_t at(uint64_t key) const {
    if (!track_keys_)
      return UINT64_C(1);
    assert(!keys_.empty());
    const size_t pos = locate(key);
    assert(keys_[pos] == key && "missing binary clause id");
    return track_values_ ? values_[pos] : UINT64_C(1);
  }

  void insert_or_assign(uint64_t key, uint64_t value) {
    insert_or_assign_known(key, value, true);
  }

  void insert_or_assign_known(uint64_t key, uint64_t value, bool is_new) {
    assert(key != EMPTY);
    if (!track_keys_) {
      size_ += is_new;
      return;
    }
    if (keys_.empty() || (size_ + 1) * 8 > keys_.size() * 7)
      rehash(keys_.empty() ? 16 : 2 * keys_.size());
    const size_t pos = locate(key);
    if (keys_[pos] == EMPTY) {
      keys_[pos] = key;
      size_++;
    }
    if (track_values_)
      values_[pos] = value;
  }

  template <class F> void for_each(F &&visit) const {
    if (!track_keys_)
      return;
    for (size_t i = 0; i < keys_.size(); i++)
      if (keys_[i] != EMPTY)
        visit(keys_[i], track_values_ ? values_[i] : UINT64_C(1));
  }

  void swap(BinaryIdMap &other) {
    keys_.swap(other.keys_);
    values_.swap(other.values_);
    std::swap(size_, other.size_);
    std::swap(track_keys_, other.track_keys_);
    std::swap(track_values_, other.track_values_);
  }
};

// Optional no-false-negative prefilter for phases that make many binary
// membership queries. The watch lists remain the exact liveness authority;
// stale bits after deletion can only cause an exact fallback scan. This avoids
// duplicating every live pair in another full-key hash table.
class BinaryLivenessIndex {
  // Blocked layout: a key's three probe bits all lie inside one 512-bit block
  // (one cache line), so a membership test or an insertion touches a single
  // line rather than three random ones. The block count is a power of two.
  static constexpr uint64_t BLOCK_BITS = 512;
  static constexpr uint64_t BLOCK_WORDS = BLOCK_BITS / 64;
  std::vector<uint64_t> bits_;
  uint64_t block_mask_ = 0;
  size_t entry_capacity_ = 0;
  uint32_t exact_literals_ = 0;

  static uint64_t hash(uint64_t x) {
    x += UINT64_C(0x9e3779b97f4a7c15);
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
  }
  struct Probe {
    size_t base; // first word of the block
    uint64_t p0, p1, p2; // bit positions inside the block
  };
  Probe probe(uint64_t key) const {
    const uint64_t h1 = hash(key);
    const uint64_t h2 = hash(key ^ UINT64_C(0xd6e8feb86659fd93));
    return {static_cast<size_t>((h1 >> 9) & block_mask_) * BLOCK_WORDS,
            h1 & (BLOCK_BITS - 1), h2 & (BLOCK_BITS - 1),
            (h2 >> 9) & (BLOCK_BITS - 1)};
  }
  bool bit(size_t base, uint64_t position) const {
    return bits_[base + (position >> 6)] & (UINT64_C(1) << (position & 63));
  }
  void set_bit(size_t base, uint64_t position) {
    bits_[base + (position >> 6)] |= UINT64_C(1) << (position & 63);
  }
  uint64_t exact_position(uint64_t key) const {
    assert(exact_literals_);
    const uint64_t lo = key >> 32;
    const uint64_t hi = static_cast<uint32_t>(key);
    assert(lo < hi && hi < exact_literals_);
    return lo * (2 * uint64_t{exact_literals_} - lo - 1) / 2 + hi - lo - 1;
  }

public:
#ifdef TACTIX_ENABLE_EVENT_TRACE
  size_t diagnostic_storage_bytes() const {
    return bits_.capacity() * sizeof(uint64_t);
  }
#endif
  // Four bits per entry, rounded up to a power of two of whole blocks.
  void reserve(size_t entries) {
    exact_literals_ = 0;
    uint64_t target = BLOCK_BITS;
    assert(entries <= UINT64_MAX / 4);
    const uint64_t requested = 4 * static_cast<uint64_t>(entries);
    while (target < requested) {
      assert(target <= UINT64_MAX / 2);
      target <<= 1;
    }
    bits_.assign(static_cast<size_t>(target >> 6), 0);
    block_mask_ = (target / BLOCK_BITS) - 1;
    entry_capacity_ = static_cast<size_t>(target >> 2);
  }
  // A compact literal universe can use its pair bitmap as the exact authority.
  // Select it only when the complete bitmap is bounded to 256 MiB and no
  // larger than the minimum two-word watch payload represented by
  // 'storage_entries'. Larger or sparse universes retain the blocked Bloom
  // representation and its exact-watch fallback.
  void reserve_bounded_exact(size_t entries, uint32_t literals,
                             uint64_t storage_entries) {
    constexpr uint64_t MAX_EXACT_WORDS = (UINT64_C(256) << 20) / 8;
    const uint64_t pairs =
        uint64_t{literals} * (uint64_t{literals} - (literals != 0)) / 2;
    const uint64_t words = (pairs + 63) / 64;
    const bool within_watch_payload =
        storage_entries <= UINT64_MAX / 64 && pairs <= storage_entries * 64;
    if (literals >= 2 && words <= MAX_EXACT_WORDS && within_watch_payload) {
      bits_.assign(static_cast<size_t>(words), 0);
      block_mask_ = 0;
      entry_capacity_ = static_cast<size_t>(pairs);
      exact_literals_ = literals;
    } else {
      reserve(entries);
    }
  }
  bool exact() const { return exact_literals_ != 0; }
  bool needs_growth(size_t entries) const { return entries > entry_capacity_; }
  bool maybe(uint64_t key) const {
    if (exact_literals_) {
      const uint64_t position = exact_position(key);
      return bits_[position >> 6] & (UINT64_C(1) << (position & 63));
    }
    if (bits_.empty())
      return true;
    const Probe q = probe(key);
    return bit(q.base, q.p0) && bit(q.base, q.p1) && bit(q.base, q.p2);
  }
  void add(uint64_t key) {
    if (exact_literals_) {
      const uint64_t position = exact_position(key);
      bits_[position >> 6] |= UINT64_C(1) << (position & 63);
      return;
    }
    if (bits_.empty())
      return;
    const Probe q = probe(key);
    set_bit(q.base, q.p0);
    set_bit(q.base, q.p1);
    set_bit(q.base, q.p2);
  }
  void remove(uint64_t key) {
    if (exact_literals_) {
      const uint64_t position = exact_position(key);
      bits_[position >> 6] &= ~(UINT64_C(1) << (position & 63));
    }
  }
};

// Direct lookup from a packed replacement's proof ID to its source clause's
// current arena position. Aliases are generation-local: collection
// materializes every replacement at that position, then clears this image.
class ClausePositionAliases {
  static constexpr unsigned PAGE_SHIFT = 12;
  static constexpr size_t PAGE_SIZE = size_t{1} << PAGE_SHIFT;
  static constexpr size_t PAGE_MASK = PAGE_SIZE - 1;
  std::vector<std::vector<uint64_t>> pages_;

public:
#ifdef TACTIX_ENABLE_EVENT_TRACE
  size_t diagnostic_storage_bytes() const {
    size_t result = pages_.capacity() * sizeof(std::vector<uint64_t>);
    for (const auto &page : pages_)
      result += page.capacity() * sizeof(uint64_t);
    return result;
  }
#endif
  // No alias recorded since the last collection means every clause sits at its
  // own reference, which is the ordinary state between two in-place rewrites.
  bool empty() const { return pages_.empty(); }
  uint64_t get(uint64_t id) const {
    if (pages_.empty())
      return 0;
    const uint64_t page64 = id >> PAGE_SHIFT;
    if (page64 >= pages_.size())
      return 0;
    const auto &page = pages_[static_cast<size_t>(page64)];
    return page.empty() ? 0 : page[static_cast<size_t>(id) & PAGE_MASK];
  }
  void set(uint64_t id, uint64_t value) {
    const uint64_t page64 = id >> PAGE_SHIFT;
    assert(page64 < SIZE_MAX);
    const size_t page_index = static_cast<size_t>(page64);
    if (page_index >= pages_.size())
      pages_.resize(page_index + 1);
    auto &page = pages_[page_index];
    if (page.empty())
      page.resize(PAGE_SIZE, 0);
    page[static_cast<size_t>(id) & PAGE_MASK] = value;
  }
  void clear() {
    pages_.clear();
  }
};

#ifdef TACTIX_ENABLE_EVENT_TRACE
void event_trace_detail(const Kernel &, const char *, int64_t);
void event_trace_binary_assignment(const Kernel &, Lit assigned,
                                   Lit immediate_reason, Lit stored_reason,
                                   bool jumped);
void event_trace_phase_save(const Kernel &, Lit);
void event_trace_root_assign(const Kernel &, Lit);
void event_trace_sweep_schedule(const Kernel &, Var, uint64_t, uint64_t,
                                uint64_t, uint64_t);
void event_trace_sweep_seed(const Kernel &, Var);
void event_trace_sweep_environment(const Kernel &, Var, uint32_t, uint32_t,
                                   uint64_t);
void event_trace_sweep_environment_clause(const Kernel &, Var, uint32_t,
                                          const Lit *, uint32_t);
void event_trace_sweep_query(const Kernel &, Var, uint32_t, const char *, Lit,
                             Lit, int, uint64_t, uint64_t);
void event_trace_sweep_model(const Kernel &, Var, uint32_t, const Var *,
                             const int8_t *, uint32_t);
void event_trace_sweep_flip(const Kernel &, Var, const char *, Lit, bool,
                            uint64_t);
void event_trace_sweep_effort(const Kernel &, uint32_t, uint64_t, uint64_t,
                              uint64_t);
void event_trace_sweep_environment_var(const Kernel &, Var, uint32_t, Var,
                                       uint32_t);
void event_trace_sweep_binary_attempt(const Kernel &, Var, uint32_t, Lit, Lit,
                                      int32_t, int32_t, const char *);
void event_trace_sweep_watch(const Kernel &, Var, uint32_t, Lit);
void event_trace_sweep_equivalence(const Kernel &, Lit, Lit);
void event_trace_sweep_commit(const Kernel &, Lit, Lit, int8_t, int8_t,
                              const char *);
void event_trace_sweep_rewrite_binary(const Kernel &, Lit, Lit, uint32_t,
                                      Lit);
void event_trace_sweep_unit(const Kernel &, Lit);
void event_trace_substitute_map(const Kernel &, Lit, Lit);
void event_trace_factor_seed(const Kernel &, Lit, uint32_t, uint32_t);
void event_trace_factor_remainder(const Kernel &, Lit, uint32_t, bool,
                                  const Lit *, uint32_t);
void event_trace_factor_candidate(const Kernel &, Lit, uint32_t, uint32_t,
                                  Lit, uint32_t, uint32_t);
void event_trace_factor_score_entry(const Kernel &, Lit, uint32_t, Lit,
                                    uint32_t, bool, Lit, uint64_t,
                                    const Clause *);
void event_trace_factor_literal_state(const Kernel &, Lit, bool, bool, int,
                                      uint32_t);
void event_trace_factor_literal_entry(const Kernel &, Lit, uint32_t, bool, Lit,
                                      uint64_t, const Clause *);
void event_trace_factor_candidate_source(const Kernel &, Lit, uint32_t,
                                         uint32_t, Lit, Lit, const Clause *,
                                         const Clause *);
void event_trace_factor_candidate_binary_source(const Kernel &, Lit, uint32_t,
                                                uint32_t, Lit, Lit);
void event_trace_factor_pair(const Kernel &, Lit, const char *);
void event_trace_factor_heap(const Kernel &, Lit, const char *, Lit, uint32_t,
                             uint32_t, Lit, uint32_t, uint32_t, uint32_t);
void event_trace_factor_heap_digest(const Kernel &, Lit, uint32_t, uint64_t,
                                    Lit, uint32_t);
void event_trace_factor_schedule(const Kernel &, uint32_t, Lit, uint32_t);
void event_trace_factor_mark_transition(const Kernel &, Lit, bool,
                                        const char *);
void event_trace_bve_retry_mark(const Kernel &, Var, const char *, bool, bool,
                                const Clause *);
void event_trace_factor_update(const Kernel &, Lit, uint32_t, Lit, uint32_t);
void event_trace_factor_grid(const Kernel &, Lit, uint32_t, uint32_t, int);
void event_trace_factor_grid_member(const Kernel &, uint32_t, Lit);
void event_trace_factor_fresh(const Kernel &, Lit, Var, const Lit *, uint32_t,
                              uint32_t, int, uint64_t ticks, uint64_t pops,
                              uint64_t first_ticks, uint64_t next_ticks,
                              uint64_t match_ticks);
#endif

struct FactorSelection;
struct Heuristics;

struct Assigned {
  uint32_t level;
  uint32_t trail; // position on the trail
  uint32_t reason; // Ref, or other literal if binary, or sentinels
  bool binary;
  bool jumped;
};

struct Link {
  uint32_t prev, next, stamp;
};

struct Stats {
  uint64_t decisions = 0;
  uint64_t conflicts = 0;
  uint64_t propagations = 0;
  uint64_t ticks = 0; // propagation memory-traffic proxy (see propagate.cpp)
  uint64_t search_ticks = 0;
  uint64_t inprocess_propagations = 0; // of those, inside a simplification
  // Monotonic policy-observation epoch.  `unflushed_root` is a transient
  // prefix marker and can return to zero inside Propagate, so it cannot tell
  // DefaultPolicy whether this application created a true-level-zero fact.
  // Counting at the assignment primitive lets the policy skip a second walk
  // over every ordinary non-root implication while retaining its rule-boundary
  // fixed-variable handoff.
  uint64_t root_assignments = 0;
  // clause additions that landed behind the propagation cursor and rewound it
  uint64_t propagation_rearms = 0;
  static constexpr uint32_t MAX_TIMED_RULES = 64;
  uint64_t rule_nanos[MAX_TIMED_RULES] = {}; // per registry rule id
  uint64_t rule_calls[MAX_TIMED_RULES] = {};
  uint64_t inprocess_nanos = 0; // whole slot, incl. scheduling not in a rule
  // Inclusive phase-boundary attribution. These four buckets partition the
  // ordinary Probe/Eliminate slot; structured-root startup is separate.
  uint64_t inprocess_entry_nanos = 0;
  uint64_t inprocess_dense_connect_nanos = 0;
  uint64_t inprocess_slot_loop_nanos = 0;
  uint64_t inprocess_exit_nanos = 0;
  uint64_t structured_root_nanos = 0;
  uint64_t solve_nanos = 0;     // wall time of solve(), set by the frontend
  uint64_t sr_applications = 0;
  uint64_t sr_applied = 0;
  // Generated unique-literal AMO certificates which committed a binary.
  // This provenance is a monotonic scheduling signal; ordinary SR binaries
  // and caller-authored payloads do not contribute.
  uint64_t sr_unique_literal_amo_binaries = 0;
  // Non-unique-literal consequences committed after generated helper binaries.
  // This monotonic provenance lets DefaultPolicy distinguish consumed
  // scaffolding from a helper-only result without assuming producer order.
  uint64_t sr_post_amo_nonhelper_applied = 0;
  // The subset of those consequences where generic graph symmetry committed
  // a root unit.  This shows that the completed BIG was consumed by a later
  // group computation, rather than merely preceding an independent structured
  // fixing.
  uint64_t sr_post_amo_graph_root_units = 0;
  uint64_t sr_nonforcing_cut_batches_declined = 0;
  uint64_t sr_nonforcing_cuts_declined = 0;
  uint64_t sr_singleton_forcing_batches_declined = 0;
  uint64_t sr_singleton_forcing_rows_declined = 0;
  uint64_t residual_handoff_calls = 0;
  uint64_t residual_handoff_variables = 0;
  uint64_t residual_handoff_nanos = 0;
  uint64_t sr_validation_ticks = 0;
  uint64_t sr_witness_literals = 0;
  // Witness-free SR expansion (docs/calculus.md): expanded lines, lines kept
  // native because the stream was witnessed or the expansion was out of scope
  // or over budget, and the clause additions the expansions wrote.
  uint64_t sr_expanded = 0;
  uint64_t sr_expansion_native = 0;
  uint64_t sr_expansion_additions = 0;
  uint64_t switched = 0; // mode switches (S4)
  uint64_t target_saved = 0; // target-phase snapshots
  uint64_t best_saved = 0;   // best-phase snapshots
  uint64_t rephased = 0;     // Rephase applications (S3)
  uint64_t reorder_eligible = 0; // scheduled occasions past the policy gate
  uint64_t reordered = 0;    // Reorder applications (S7, policy op)
  uint64_t lucky_early_calls = 0; // DefaultPolicy startup opportunities
  uint64_t lucky_late_calls = 0;  // ... after initial preprocessing
  uint64_t lucky_late_sat = 0;    // late opportunities proving SAT
  uint64_t lucky_probes = 0; // Lucky probes run (S5)
  uint64_t parity_model_calls = 0;
  uint64_t parity_model_equations = 0;
  uint64_t parity_model_rank = 0;
  uint64_t parity_model_candidates = 0;
  uint64_t parity_model_sat = 0;
  uint64_t parity_model_interrupted = 0;
  uint64_t parity_refutation_probe_declines = 0;
  uint64_t parity_refutation_recognized = 0;
  uint64_t parity_refutation_unsat = 0;
  uint64_t parity_refutation_proof_additions = 0;
  uint64_t parity_refutation_proof_deletions = 0;
  uint64_t counting_calls = 0, counting_stars = 0, counting_unsat = 0;
  uint64_t counting_work = 0, counting_interrupted = 0, counting_additions = 0;
  uint64_t affine_model_calls = 0, affine_model_equations = 0;
  uint64_t affine_model_projection = 0, affine_model_entries = 0;
  uint64_t affine_model_candidates = 0, affine_model_sat = 0;
  uint64_t affine_model_work = 0, affine_model_interrupted = 0;
  uint64_t affine_model_budget = 0, affine_model_unavailable = 0;
  uint64_t onehot_model_calls = 0;
  uint64_t onehot_model_domains = 0;
  uint64_t onehot_model_equations = 0;
  uint64_t onehot_model_rank = 0;
  uint64_t onehot_model_candidates = 0;
  uint64_t onehot_model_sat = 0;
  uint64_t onehot_model_interrupted = 0;
  uint64_t exact_one_refutation_calls = 0;
  uint64_t exact_one_refutation_recognized = 0;
  uint64_t exact_one_refutation_equations = 0;
  uint64_t exact_one_refutation_unsat = 0;
  uint64_t exact_one_refutation_interrupted = 0;
  uint64_t exact_one_refutation_proof_additions = 0;
  uint64_t exact_one_refutation_proof_deletions = 0;
  uint64_t direct_csp_model_calls = 0;
  uint64_t direct_csp_model_domains = 0;
  uint64_t direct_csp_model_edges = 0;
  uint64_t direct_csp_model_restarts = 0;
  uint64_t direct_csp_model_infeasible = 0;
  uint64_t direct_csp_model_work = 0;
  uint64_t direct_csp_model_best = UINT64_MAX;
  uint64_t direct_csp_model_sat = 0;
  uint64_t balanced_signature_calls = 0;
  uint64_t balanced_signature_recognized = 0;
  uint64_t balanced_signature_forced_ttf = 0;
  uint64_t balanced_signature_candidates = 0;
  uint64_t balanced_signature_restarts = 0;
  uint64_t balanced_signature_steps = 0;
  uint64_t balanced_signature_completion_rounds = 0;
  uint64_t balanced_signature_sat = 0;
  uint64_t balanced_signature_interrupted = 0;
  uint64_t fermat_model_calls = 0;
  uint64_t fermat_model_recognized = 0;
  uint64_t fermat_model_candidates = 0;
  uint64_t fermat_model_factor_iterations = 0;
  uint64_t fermat_model_completion_rounds = 0;
  uint64_t fermat_model_sat = 0;
  uint64_t fermat_model_interrupted = 0;
  uint64_t cerny_model_calls = 0;
  uint64_t cerny_model_recognized = 0;
  uint64_t cerny_model_states = 0;
  uint64_t cerny_model_length = 0;
  uint64_t cerny_model_sat = 0;
  uint64_t cerny_model_interrupted = 0;
  uint64_t exact_cover_model_calls = 0;
  uint64_t exact_cover_model_recognized = 0;
  uint64_t exact_cover_model_blocks = 0;
  uint64_t exact_cover_model_nodes = 0;
  uint64_t exact_cover_model_restarts = 0;
  uint64_t exact_cover_model_budget_exhausted = 0;
  uint64_t exact_cover_model_sat = 0;
  uint64_t exact_cover_model_interrupted = 0;
  uint64_t ramsey_startup_attempts = 0;
  uint64_t ramsey_startup_recognized = 0;
  uint64_t ramsey_startup_recognition_work = 0;
  uint64_t ramsey_startup_certificate_work = 0;
  uint64_t ramsey_startup_nodes = 0;
  uint64_t ramsey_startup_additions = 0;
  uint64_t ramsey_startup_deletions = 0;
  uint64_t ramsey_startup_helpers = 0;
  uint64_t ramsey_startup_interrupted = 0;
  uint64_t ramsey_startup_exhausted = 0;
  uint64_t ramsey_startup_unsat = 0;
  uint64_t graph_labeling_calls = 0;
  uint64_t graph_labeling_recognized = 0;
  uint64_t graph_labeling_vertices = 0;
  uint64_t graph_labeling_separation = 0;
  uint64_t graph_labeling_seeds = 0;
  uint64_t graph_labeling_nodes = 0;
  uint64_t graph_labeling_repairs = 0;
  uint64_t graph_labeling_anneal_attempts = 0;
  uint64_t graph_labeling_work = 0;
  uint64_t graph_labeling_budget = 0;
  uint64_t graph_labeling_interrupted = 0;
  uint64_t graph_labeling_models_rejected = 0;
  uint64_t graph_labeling_sat = 0;
  uint64_t graph_interval_equivalence_calls = 0;
  uint64_t graph_interval_equivalence_recognized = 0;
  uint64_t graph_interval_equivalence_vertices = 0;
  uint64_t graph_interval_equivalence_gates = 0;
  uint64_t graph_interval_equivalence_intervals = 0;
  uint64_t graph_interval_equivalence_groups = 0;
  uint64_t graph_interval_equivalence_selected = 0;
  uint64_t graph_interval_equivalence_work = 0;
  uint64_t graph_interval_equivalence_source_clauses = 0;
  uint64_t graph_interval_equivalence_source_literals = 0;
  uint64_t graph_interval_equivalence_source_workspace = 0;
  uint64_t graph_interval_equivalence_resource_exhausted = 0;
  uint64_t graph_interval_equivalence_interrupted = 0;
  uint64_t graph_interval_equivalence_rup_ticks = 0;
  uint64_t graph_interval_equivalence_rup_rejected = 0;
  uint64_t graph_interval_equivalence_duplicates = 0;
  uint64_t graph_interval_equivalence_published = 0;
  uint64_t graph_interval_equivalence_proof_literal_occurrences = 0;
  uint64_t independent_set_model_calls = 0;
  uint64_t independent_set_model_recognized = 0;
  uint64_t independent_set_model_vertices = 0;
  uint64_t independent_set_model_target = 0;
  uint64_t independent_set_model_greedy_size = 0;
  uint64_t independent_set_model_size = 0;
  uint64_t independent_set_model_steps = 0;
  uint64_t independent_set_model_completion_rounds = 0;
  uint64_t independent_set_model_sat = 0;
  uint64_t independent_set_model_interrupted = 0;
  uint64_t lookup_circuit_model_calls = 0;
  uint64_t lookup_circuit_model_recognized = 0;
  uint64_t lookup_circuit_model_gates = 0;
  uint64_t lookup_circuit_model_support = 0;
  uint64_t lookup_circuit_model_vector_evaluations = 0;
  uint64_t lookup_circuit_model_csp_work = 0;
  uint64_t lookup_circuit_model_sat = 0;
  uint64_t lookup_circuit_model_interrupted = 0;
  uint64_t simon_model_calls = 0;
  uint64_t simon_model_recognized = 0;
  uint64_t simon_model_rounds = 0;
  uint64_t simon_model_candidates = 0;
  uint64_t simon_model_sat = 0;
  uint64_t simon_model_interrupted = 0;
  uint64_t stconn_proof_calls = 0;
  uint64_t stconn_proof_recognized = 0;
  uint64_t stconn_proof_width = 0;
  uint64_t stconn_proof_columns = 0;
  uint64_t stconn_proof_rup_clauses = 0;
  uint64_t stconn_proof_budget_exhausted = 0;
  uint64_t stconn_proof_unsat = 0;
  uint64_t stconn_proof_interrupted = 0;
  uint64_t signed_cardinality_proof_calls = 0;
  uint64_t signed_cardinality_proof_recognized = 0;
  uint64_t signed_cardinality_proof_groups = 0;
  uint64_t signed_cardinality_proof_bdd_nodes = 0;
  uint64_t signed_cardinality_proof_additions = 0;
  uint64_t signed_cardinality_proof_budget_exhausted = 0;
  uint64_t signed_cardinality_proof_unsat = 0;
  uint64_t signed_cardinality_proof_interrupted = 0;
  uint64_t local_search_hints_offered = 0;
  uint64_t local_search_hints_replaced = 0;
  uint64_t local_search_hint_phases_offered = 0;
  uint64_t local_search_hints_applied = 0;
  uint64_t local_search_hints_stale = 0;
  uint64_t local_search_target_install_attempts = 0;
  uint64_t local_search_target_installs = 0;
  uint64_t local_search_target_eligible_phases = 0;
  uint64_t local_search_target_changed_phases = 0;
  uint64_t local_search_target_matching_phases = 0;
  uint64_t local_search_target_replaced_phases = 0;
  uint64_t local_search_target_overwritten_native = 0;
  uint64_t local_search_target_overwritten_rephase = 0;
  uint64_t local_search_target_overwritten_walk = 0;
  uint64_t local_search_target_decisions = 0;
  uint64_t local_search_target_decision_matches = 0;
  uint64_t local_search_target_decision_overrides = 0;
  uint64_t semantic_variable_candidates_published = 0;
  uint64_t semantic_variable_candidate_existing = 0;
  uint64_t semantic_variable_candidate_introduced = 0;
  uint64_t semantic_variable_advice_attempts = 0;
  uint64_t semantic_variable_advice_applied = 0;
  uint64_t semantic_variable_advice_invalid = 0;
  uint64_t semantic_variable_advice_stale = 0;
  uint64_t semantic_variable_advice_resource_exhausted = 0;
  uint64_t semantic_variable_advice_inapplicable = 0;
  uint64_t semantic_variable_advice_variables = 0;
  uint64_t semantic_variable_advice_priority = 0;
  uint64_t walk_seed_imports = 0;
  uint64_t walk_seed_imported_phases = 0;
  uint64_t walk_seed_steps = 0;
  uint64_t walk_seed_budget_exhausted = 0;
  uint64_t walk_seed_interrupted = 0;
  uint64_t walk_seed_ordinary_unsat = UINT64_MAX;
  uint64_t walk_seed_candidate_unsat = UINT64_MAX;
  uint64_t walk_seed_improved = 0;
  uint64_t walk_seed_exported = 0;
  uint64_t walk_preparation_steps = 0, walk_search_steps = 0;
  uint64_t walk_preparation_budget_exhausted = 0;
  uint64_t walk_search_budget_exhausted = 0;
  uint64_t walk_seed_admitted = 0, walk_seed_rejected = 0;
  uint64_t walks = 0;         // S6 Walk applications (policy op)
  uint64_t walk_flips = 0;    // literal flips
  uint64_t walk_steps = 0;    // native occurrence visits plus explicit seed work
  uint64_t walk_improved = 0; // walks that lowered the unsatisfied count
  uint64_t walk_minimum = UINT64_MAX;
  uint64_t warmups = 0; // root decision/propagation sweeps before Walk
  uint64_t warming_decisions = 0;
  uint64_t warming_conflicts = 0;
  uint64_t warming_propagations = 0;
  uint64_t promoted = 0;     // glue promotions on use (kissat deduce.c)
  uint64_t learned = 0;
  uint64_t learned_units = 0;
  uint64_t learned_lits = 0;      // sum of FINAL learned clause sizes
  uint64_t learned_glue_sum = 0;  // sum of learned glues
  uint64_t minimized = 0; // literals removed by clause minimization
  uint64_t minimize_depth_cutoffs = 0; // recursive walks stopped at Kissat cap
  uint64_t shrunken = 0;  // literals removed by all-UIP block shrinking
  uint64_t otfs_strengthened = 0; // reasons strengthened on the fly
  uint64_t otfs_subsumed = 0;     // conflicts subsumed by the resolvent
  uint64_t eager_tried = 0;     // containment tests run (kissat eagertried)
  uint64_t eager_subsumed = 0;  // deletions (kissat eagerly_subsumed)
  uint64_t eager_reason_skipped = 0; // subsumed but kept — still a trail reason
  uint64_t shrink_promoted = 0;
  uint64_t tumble_seeded = 0;
  static constexpr uint32_t MAX_GLUE_USED = 127;
  uint64_t used_glue[2][MAX_GLUE_USED + 1] = {};
  uint64_t retiered = 0;           // dynamic tier recomputations
  uint64_t tier2_band_dynamic = 0;
  uint64_t tier2_band_static = 0;
  uint64_t reduce_scans = 0;
  uint64_t reduce_scan_words = 0;
  uint64_t reduce_skippable_words = 0;
  uint64_t reason_bumped = 0;      // extra vars marked by reason-side bumping
  uint64_t reason_bump_rounds = 0; // conflicts the reason-side pass ran on
  uint64_t reason_bump_depth2 = 0; // rounds allowed to recurse in stable mode
  uint64_t reason_bump_aborted = 0; // rounds rolled back at the mark limit
  uint64_t api_decisions = 0; // externally chosen branches (N1)
  uint64_t input_restated = 0; // normalized input clauses restated by fresh id
  uint64_t decisions_extension = 0; // decisions on a Factor extension variable
  uint64_t decisions_substituted = 0; // ... on one Substitute rewrote away
  mutable uint64_t rule_interrupted = 0; // applications aborted on the
  mutable uint64_t stop_fuzz_fires = 0;  // stops reported by the stress knob,
  uint64_t bumps = 0;                // bump events (both modes, both channels)
  uint64_t bumps_extension = 0;      // ...of which landed on an extension var
  uint64_t bumps_ext_reason_only = 0; // ...of those, reason-side ONLY
  uint64_t restarts = 0;
  uint64_t restarts_reused = 0; // restarts that kept a trail prefix
  uint64_t reductions = 0;
  uint64_t deleted = 0; // learned clauses deleted by Reduce
  uint64_t variable_compactions = 0;
  uint64_t compacted_variables = 0;
  uint64_t binaries_deleted = 0; // binary clauses deleted (kernel_delete_binary)
  uint64_t forward_subsumptions = 0; // SubsumeForward rounds
  uint64_t forward_checked = 0;  // SubsumeForward candidates checked
  uint64_t forward_visits = 0;
  uint64_t forward_subsumed = 0; // clauses deleted by SubsumeForward
  uint64_t strengthen_rounds = 0;  // Strengthen rounds
  uint64_t strengthen_checked = 0; // Strengthen candidate checks
  uint64_t strengthen_visits = 0;  // Kissat-compatible fused forward_steps
  uint64_t strengthened = 0;       // clauses strengthened (add+delete)
  uint64_t strengthen_subsumed = 0; // deletions by Strengthen's fused round
  uint64_t budget_stops_c1 = 0; // SubsumeForward rounds cut by `work_budget`
  uint64_t budget_stops_c2 = 0; // Strengthen rounds cut between candidates
  uint64_t subsume_interrupted = 0;
  uint64_t strengthen_interrupted = 0;
  uint64_t flushed = 0; // root-satisfied deletions + root-falsified literal
  uint64_t sparse_collections = 0; // startup whole-arena root collections
  uint64_t sparse_collected = 0;   // clauses deleted or rewritten by them
  uint64_t vivify_rounds = 0;  // Vivify rounds
  uint64_t vivify_checked = 0; // Vivify candidates probed
  uint64_t vivified = 0;       // clauses distilled by Vivify (add+delete)
  uint64_t vivify_deleted = 0; // root-satisfied deletions by Vivify
  uint64_t vivify_flushed = 0; // scheduled clauses removed by prefix subsumption
  uint64_t vivify_reused = 0;     // levels taken from the retained trail
  uint64_t vivify_probes = 0;     // assumptions made; reuse rate = reused/both
  uint64_t vivify_reuse_full = 0; // candidates wholly covered by the prefix
  uint64_t vivify_tier3 = 0;      // tier3 candidates probed; 0 = that half of
  uint64_t vivify_prioritized = 0;   // candidates carrying the leftover bit
  uint64_t vivify_reprioritized = 0; // rounds whose leftover set was empty
  uint64_t vivify_polls = 0;      // kernel_terminating evaluations at the site
  uint64_t vivify_terminated = 0; // rounds cut short by one
  uint64_t vivify_subsumed = 0;   // candidates deleted by a reason clause
  uint64_t vivify_promoted = 0;   // redundant subsumers made irredundant
  uint64_t vivify_shrunken = 0;   // strict deduction rewrites
  uint64_t vivify_implied = 0;    // redundant independently-implied deletes
  uint64_t vivify_asymmetric = 0; // irredundant asymmetric-tautology deletes
  uint64_t vivify_instantiated = 0; // successful last-literal instantiations
  uint64_t vivify_noop = 0;       // terminal implied/instantiation declines
  uint64_t probe_rounds = 0;   // Probe rounds
  uint64_t probed = 0;         // probe assumptions made
  uint64_t probe_failed = 0;   // failed literals -> root units derived
  uint64_t backbone_computations = 0; // canonical batch applications
  uint64_t backbone_rounds = 0;  // Backbone rounds
  uint64_t backbone_probed = 0;  // binary probe assumptions made
  uint64_t backbone_failed = 0;  // failed literals -> root units derived
  uint64_t transitive_rounds = 0;  // TransitiveReduce rounds
  uint64_t transitive_checked = 0; // candidate edges walked
  uint64_t bce_rounds = 0;         // BCE rounds that deleted something
  uint64_t bce_deleted = 0;        // blocked clauses deleted (witnessed)
  uint64_t probe_ticks = 0;        // Probe's own propagation ticks
  uint64_t backbone_ticks = 0;     // Backbone's own
  uint64_t vivify_ticks = 0;       // Vivify's own
  uint64_t instantiate_ticks = 0;  // Instantiate's own
  uint64_t bce_ticks = 0;          // occurrences + pairs scanned — BCE's budget
  uint64_t instantiate_rounds = 0;  // Instantiate rounds that strengthened
  uint64_t instantiate_checked = 0; // (clause, literal) pairs probed
  uint64_t instantiated = 0;        // literals removed by Instantiate

  uint64_t cce_rounds = 0;   // rounds attempted; nothing may read it — a
  uint64_t cce_tried = 0;    // candidate clauses extended
  uint64_t cce_deleted = 0;  // clauses eliminated
  uint64_t cce_blocked = 0;  // ... of which by the blocked exit (witnessed)
  uint64_t cce_ata = 0;      // ... of which by the asymmetric-tautology exit
  uint64_t cce_ala = 0;      // asymmetric literals added
  uint64_t cce_cla = 0;      // covered literals added (CCE's own mechanism)
  uint64_t cce_ticks = 0;    // clause literals scanned — CCE's budget currency

  uint64_t sweep_rounds = 0;       // detection rounds run
  uint64_t sweep_environments = 0; // environments built AND handed to the solver
  uint64_t sweep_env_clauses = 0;  // clauses encoded into them
  uint64_t sweep_solves = 0;       // sub-solver solve() calls
  uint64_t sweep_candidates = 0;   // candidate-pair directions actually solved
  uint64_t sweep_swept = 0;        // scheduled variables swept (kissat `swept`)
  uint64_t sweep_equivalences = 0; // pairs PROVED equivalent
  uint64_t sweep_units = 0;        // root units proved by local environments
  uint64_t sweep_backbone_candidates = 0;
  uint64_t sweep_backbone_flipped = 0;
  uint64_t sweep_binaries = 0;     // equivalence binaries ADDED with a proof
  uint64_t sweep_not_rup = 0;      // equivalences declined (not RUP)
  uint64_t sweep_general = 0;      // pairs emitted via the general proof path
  uint64_t random_sequences = 0;   // random-decision sequences started
  uint64_t random_decisions = 0;   // decisions actually taken at random
  uint64_t sweep_known = 0;        // pairs the formula already states as binaries
  uint64_t sweep_env_unsat = 0;    // environments refuted outright (F is UNSAT)
  uint64_t sweep_budget = 0;       // solves abandoned on the tick budget
  uint64_t sweep_ticks = 0;        // Sweep scan+sub-solver diagnostic
  uint64_t sweep_env_lits = 0;      // literal occurrences encoded
  uint64_t sweep_env_false = 0;     // of those, root-FALSE occurrences
  uint64_t sweep_env_freefalse = 0; // of those, with no unit id => free var
  uint64_t sweep_env_stripped = 0;  // root-false occurrences REMOVED
  uint64_t sweep_strip_cited = 0;   // ... with their root units cited in an
  uint64_t sweep_env_units = 0;     // unit clauses added alongside (off arm)
  uint64_t sweep_ticks_dedup = 0;   // the environment's clause-key scan
  uint64_t sweep_ticks_sub = 0;     // Sweep's scheduled effort currency
  uint64_t sweep_ticks_repr = 0;    // Tactix arena words rebuilt after emissions
  uint64_t sweep_occ_f = 0;         // env vars' large-clause degree in F
  uint64_t sweep_occ_env = 0;       // ... and inside the environment
  uint64_t sweep_vars_closed = 0;   // env vars holding their WHOLE degree
  uint64_t sweep_vars_total = 0;    // env vars
  uint64_t sweep_cand_seeded = 0;   // candidates entering the loop
  uint64_t sweep_cand_refined = 0;  // candidates dropped by model refinement
  uint64_t sweep_flip_attempts = 0; // partition literals tested by model flip
  uint64_t sweep_flipped = 0;       // successful model flips
  uint64_t sweep_dir_sat = 0;       // candidate directions that MODELLED
  uint64_t sweep_dir_unsat = 0;     // candidate directions that REFUTED
  uint64_t sweep_scheduled = 0;     // eligible seeds placed in canonical tails
  uint64_t sweep_rescheduled = 0;   // equivalence representatives moved next
  uint64_t sweep_resumed = 0;       // seeds consumed from a retained tail
  uint64_t sweep_completed = 0;     // canonical tails exhausted
  uint32_t sweep_limit_vars = 0;    // last canonical environment limits
  uint32_t sweep_limit_clauses = 0;
  uint32_t sweep_limit_depth = 0;

  uint64_t ternary_rounds = 0;     // HyperTernaryRes rounds that added clauses
  uint64_t ternary_resolvents = 0; // ternary resolvents added (redundant)
  uint64_t hbr_rounds = 0;      // X6 HyperBinaryRes: canonical rounds entered
  uint64_t hbr_probes = 0;      // root literals closed under propagation
  uint64_t hbr_binaries = 0;    // resolvents emitted
  uint64_t hbr_subsumed = 0;    // reasons deleted because the resolvent subsumed them
  uint64_t hbr_declined = 0;    // resolvents the RUP replay could not re-derive
  uint64_t hbr_interrupts = 0;  // termination polls that fired mid-round
  uint64_t hbr_ticks = 0;       // private meter (see Kernel::tick_sink)
  uint64_t ternary_binaries = 0;   // binary resolvents added (permanent)
  uint64_t ternary_ticks = 0;      // occurrence entries + pairs examined
  uint64_t transitive_ticks = 0;   // cache lines scanned by the walks (kissat
  uint64_t transitive_deleted = 0; // transitively redundant binaries deleted
  uint64_t transitive_units = 0;   // failed-source units committed by V4
  uint64_t transitive_polls = 0;   // kernel_terminating evaluations at the site
  uint64_t transitive_terminated = 0; // dst walks cut by an interrupt
  uint64_t substitute_rounds = 0;  // Substitute rounds — detection passes
  mutable uint64_t substitute_ticks = 0; // binary-watch scan cache lines
  uint64_t substitute_vars = 0;    // variables substituted away
  uint64_t substitute_sccs = 0;
  uint64_t substitute_scc_lits = 0;
  uint64_t substitute_hazard_skips = 0;
  uint64_t substitute_hazard_partial = 0;
  uint64_t substitute_hazard_lits = 0;
  uint64_t deactivations = 0;  // counted at the site, so 0 means "never fired"
  uint64_t deactivation_repeats = 0; // already disconnected retirement reports
  uint64_t reactivations = 0;  // ... of those, later re-inserted into a ranking
  uint64_t conflict_reused = 0; // conflicts resolved by reusing the conflict as
  uint64_t jumped_reasons = 0;  // binary reasons replaced by their resolvent
  uint64_t jump_reason_classifications = 0;
  uint64_t jump_reason_bigbig = 0;
  uint64_t conflict_watch_repaired = 0; // ... of those, needing watch repair
  uint64_t binprop_scans = 0;
  uint64_t binprop_ordered = 0;
  // Dense occurrence arena: slices moved to the arena tail because a push
  // outgrew their capacity, and defragmentations that reclaimed the abandoned
  // images. Both are reachability readouts for the arena's growth policy.
  uint64_t dense_slice_moves = 0;
  uint64_t dense_defrags = 0;
  uint64_t binprop_flagged = 0;
  uint64_t binprop_words = 0;
  uint64_t binprop_skipped = 0;
  uint64_t binprop_large = 0;
  uint64_t chronological = 0;  // chrono backtrack targets taken
  uint64_t backjump_stale = 0;
  uint64_t inprocess_phases = 0;   // policy inprocessing phases (a root descent
  uint64_t parity_consequence_attempts = 0;
  uint64_t parity_consequence_clauses = 0, parity_consequence_equations = 0;
  uint64_t parity_consequence_selection_work = 0;
  uint64_t parity_consequence_preflight_additions = 0;
  uint64_t parity_consequence_additions = 0, parity_consequence_deletions = 0;
  uint64_t parity_consequence_helpers = 0, parity_consequence_published = 0;
  uint64_t affine_consequence_attempts = 0, affine_consequence_applied = 0;
  uint64_t affine_consequence_no_structure = 0;
  uint64_t affine_consequence_invalid = 0, affine_consequence_inapplicable = 0;
  uint64_t affine_consequence_resource_exhausted = 0;
  uint64_t affine_consequence_interrupted = 0;
  uint64_t affine_consequence_unsupported_proof = 0;
  uint64_t affine_consequence_stale = 0;
  uint64_t affine_consequence_already_published = 0;
  uint64_t affine_consequence_snapshot_clauses = 0;
  uint64_t affine_consequence_snapshot_literals = 0;
  uint64_t affine_consequence_snapshot_work = 0;
  uint64_t affine_consequence_equations = 0;
  uint64_t affine_consequence_consequences = 0;
  uint64_t affine_consequence_recognition_work = 0;
  uint64_t affine_consequence_selected_sources = 0;
  uint64_t affine_consequence_selected_premises = 0;
  uint64_t affine_consequence_proof_work = 0;
  uint64_t affine_consequence_preflight_additions = 0;
  uint64_t affine_consequence_preflight_deletions = 0;
  uint64_t affine_consequence_additions = 0;
  uint64_t affine_consequence_deletions = 0;
  uint64_t affine_consequence_published = 0;
  uint64_t affine_consequence_journal_work = 0;
  uint64_t affine_consequence_journal_bytes = 0;
  uint64_t affine_consequence_candidate_variables = 0;
  uint64_t affine_consequence_basis_work = 0;
  uint64_t affine_consequence_rows = 0;
  uint64_t affine_consequence_definitions = 0;
  uint64_t affine_consequence_extent_growth = 0;
  uint64_t affine_consequence_session_work = 0;
  uint64_t affine_consequence_excluded_rows = 0;
  uint64_t affine_consequence_transaction_keys = 0;
  uint64_t affine_consequence_committed_keys = 0;
  uint64_t affine_matrix_init_attempts = 0;
  uint64_t affine_matrix_init_applied = 0;
  uint64_t affine_matrix_init_applied_partial = 0;
  uint64_t affine_matrix_init_incomplete_image = 0;
  uint64_t affine_matrix_init_incomplete_matrix = 0;
  uint64_t affine_matrix_init_exhausted = 0;
  uint64_t affine_matrix_init_interrupted = 0;
  uint64_t affine_matrix_init_snapshot_work = 0;
  uint64_t affine_matrix_init_recognition_work = 0;
  uint64_t affine_matrix_init_matrix_work = 0;
  uint64_t affine_matrix_init_premise_retention_work = 0;
  uint64_t affine_matrix_init_root_unit_work = 0;
  uint64_t affine_matrix_alias_work = 0;
  uint64_t affine_matrix_alias_proof_additions = 0;
  uint64_t affine_matrix_service_calls = 0;
  uint64_t affine_matrix_service_lifecycle_work = 0;
  uint64_t affine_matrix_service_applied = 0;
  uint64_t affine_matrix_service_no_event = 0;
  uint64_t affine_matrix_service_duplicate = 0;
  uint64_t affine_matrix_service_exhausted = 0;
  uint64_t affine_matrix_service_interrupted = 0;
  uint64_t affine_matrix_service_stale = 0;
  uint64_t affine_matrix_service_trail_capture_work = 0;
  uint64_t affine_matrix_service_sync_work = 0;
  uint64_t affine_matrix_service_extension_hits = 0;
  uint64_t affine_matrix_service_rebuilds = 0;
  uint64_t affine_matrix_service_rebuild_backtrack = 0;
  uint64_t affine_matrix_service_rebuild_reassignment = 0;
  uint64_t affine_matrix_service_rebuild_partial = 0;
  uint64_t affine_matrix_service_retained_assignments = 0;
  uint64_t affine_matrix_service_conditioned_assignments = 0;
  uint64_t affine_matrix_service_session_work = 0;
  uint64_t affine_matrix_service_planner_work = 0;
  uint64_t affine_matrix_service_revalidation_work = 0;
  uint64_t affine_matrix_service_proof_additions = 0;
  uint64_t affine_matrix_service_proof_deletions = 0;
  uint64_t affine_matrix_service_proof_cache_hits = 0;
  uint64_t affine_matrix_service_proof_cache_inserts = 0;
  uint64_t affine_matrix_service_helper_extent = 0;
  uint64_t affine_matrix_service_journal_bytes = 0;
  uint64_t exact_cover_extension_attempts = 0;
  uint64_t exact_cover_extension_applied = 0;
  uint64_t exact_cover_extension_no_structure = 0;
  uint64_t exact_cover_extension_already_published = 0;
  uint64_t exact_cover_extension_invalid = 0;
  uint64_t exact_cover_extension_inapplicable = 0;
  uint64_t exact_cover_extension_resource_exhausted = 0;
  uint64_t exact_cover_extension_interrupted = 0;
  uint64_t exact_cover_extension_unsupported_proof = 0;
  uint64_t exact_cover_extension_stale = 0;
  uint64_t exact_cover_extension_source_clauses = 0;
  uint64_t exact_cover_extension_source_literals = 0;
  uint64_t exact_cover_extension_recognition_work = 0;
  uint64_t exact_cover_extension_revalidation_work = 0;
  uint64_t exact_cover_extension_definitions = 0;
  uint64_t exact_cover_extension_additions = 0;
  uint64_t exact_cover_extension_literal_occurrences = 0;
  uint64_t exact_cover_extension_journal_bytes = 0;
  uint64_t exact_cover_extension_extent_growth = 0;
  uint64_t exact_cover_extension_candidate_variables = 0;
  uint64_t independent_set_cover_attempts = 0;
  uint64_t independent_set_cover_applied = 0;
  uint64_t independent_set_cover_no_structure = 0;
  uint64_t independent_set_cover_duplicate = 0;
  uint64_t independent_set_cover_invalid = 0;
  uint64_t independent_set_cover_inapplicable = 0;
  uint64_t independent_set_cover_resource_exhausted = 0;
  uint64_t independent_set_cover_interrupted = 0;
  uint64_t independent_set_cover_unsupported_proof = 0;
  uint64_t independent_set_cover_stale = 0;
  uint64_t independent_set_cover_source_clauses = 0;
  uint64_t independent_set_cover_source_literals = 0;
  uint64_t independent_set_cover_recognition_work = 0;
  uint64_t independent_set_cover_source_scan_work = 0;
  uint64_t independent_set_cover_theory_recognition_work = 0;
  uint64_t independent_set_cover_proof_work = 0;
  uint64_t independent_set_cover_planned_proof_additions = 0;
  uint64_t independent_set_cover_proof_additions = 0;
  uint64_t independent_set_cover_proof_deletions = 0;
  uint64_t independent_set_cover_proof_literal_occurrences = 0;
  uint64_t independent_set_cover_journal_work = 0;
  uint64_t independent_set_cover_published = 0;
  uint64_t independent_set_cover_candidate_variables = 0;
  uint64_t gf3_affine_attempts = 0;
  uint64_t gf3_affine_applied = 0;
  uint64_t gf3_affine_no_structure = 0;
  uint64_t gf3_affine_no_consequence = 0;
  uint64_t gf3_affine_already_published = 0;
  uint64_t gf3_affine_invalid = 0;
  uint64_t gf3_affine_inapplicable = 0;
  uint64_t gf3_affine_resource_exhausted = 0;
  uint64_t gf3_affine_interrupted = 0;
  uint64_t gf3_affine_unsupported_proof = 0;
  uint64_t gf3_affine_stale = 0;
  uint64_t gf3_affine_source_clauses = 0;
  uint64_t gf3_affine_source_literals = 0;
  uint64_t gf3_affine_equations = 0;
  uint64_t gf3_affine_owner_capture_work = 0;
  uint64_t gf3_affine_recognition_work = 0;
  uint64_t gf3_affine_planning_work = 0;
  uint64_t gf3_affine_duplicate_scan_work = 0;
  uint64_t gf3_affine_revalidation_work = 0;
  uint64_t gf3_affine_proof_work = 0;
  uint64_t gf3_affine_planned_additions = 0;
  uint64_t gf3_affine_duplicate_additions = 0;
  uint64_t gf3_affine_emitted_additions = 0;
  uint64_t gf3_affine_proof_literal_occurrences = 0;
  uint64_t gf3_affine_arena_words = 0;
  uint64_t gf3_affine_journal_work = 0;
  uint64_t gf3_affine_journal_bytes = 0;
  uint64_t gf3_affine_session_work = 0;
  uint64_t gf3_affine_session_commits = 0;
  uint64_t gf3_affine_candidate_variables = 0;
  uint64_t direct_csp_support_attempts = 0;
  uint64_t direct_csp_support_applied = 0;
  uint64_t direct_csp_support_no_structure = 0;
  uint64_t direct_csp_support_no_consequence = 0;
  uint64_t direct_csp_support_already_published = 0;
  uint64_t direct_csp_support_invalid = 0;
  uint64_t direct_csp_support_inapplicable = 0;
  uint64_t direct_csp_support_resource_exhausted = 0;
  uint64_t direct_csp_support_interrupted = 0;
  uint64_t direct_csp_support_unsupported_proof = 0;
  uint64_t direct_csp_support_stale = 0;
  uint64_t direct_csp_support_source_clauses = 0;
  uint64_t direct_csp_support_source_literals = 0;
  uint64_t direct_csp_support_owner_capture_work = 0;
  uint64_t direct_csp_support_recognition_work = 0;
  uint64_t direct_csp_support_source_scan_work = 0;
  uint64_t direct_csp_support_planning_work = 0;
  uint64_t direct_csp_support_revalidation_work = 0;
  uint64_t direct_csp_support_proof_work = 0;
  uint64_t direct_csp_support_planned_additions = 0;
  uint64_t direct_csp_support_emitted_additions = 0;
  uint64_t direct_csp_support_proof_literal_occurrences = 0;
  uint64_t direct_csp_support_journal_work = 0;
  uint64_t direct_csp_support_journal_bytes = 0;
  uint64_t direct_csp_support_extent_growth = 0;
  uint64_t direct_csp_support_session_work = 0;
  uint64_t direct_csp_support_session_commits = 0;
  uint64_t direct_csp_support_owner_commits = 0;
  uint64_t direct_csp_support_candidate_variables = 0;
  uint64_t direct_csp_support_introduced_variables = 0;
  uint64_t nary_csp_equality_attempts = 0;
  uint64_t nary_csp_equality_applied = 0;
  uint64_t nary_csp_equality_no_structure = 0;
  uint64_t nary_csp_equality_no_consequence = 0;
  uint64_t nary_csp_equality_already_published = 0;
  uint64_t nary_csp_equality_invalid = 0;
  uint64_t nary_csp_equality_inapplicable = 0;
  uint64_t nary_csp_equality_resource_exhausted = 0;
  uint64_t nary_csp_equality_interrupted = 0;
  uint64_t nary_csp_equality_unsupported_proof = 0;
  uint64_t nary_csp_equality_stale = 0;
  uint64_t nary_csp_equality_source_clauses = 0;
  uint64_t nary_csp_equality_source_literals = 0;
  uint64_t nary_csp_equality_recognition_work = 0;
  uint64_t nary_csp_equality_planning_work = 0;
  uint64_t nary_csp_equality_proof_work = 0;
  uint64_t nary_csp_equality_planned_additions = 0;
  uint64_t nary_csp_equality_emitted_additions = 0;
  uint64_t nary_csp_equality_proof_literal_occurrences = 0;
  uint64_t nary_csp_equality_journal_bytes = 0;
  uint64_t nary_csp_equality_extent_growth = 0;
  uint64_t nary_csp_equality_session_commits = 0;
  uint64_t nary_csp_equality_candidate_variables = 0;
  uint64_t nary_csp_equality_introduced_variables = 0;
  uint64_t nary_csp_equality_amo_attempts = 0;
  uint64_t nary_csp_equality_amo_applied = 0;
  uint64_t nary_csp_equality_amo_missing_equality = 0;
  uint64_t nary_csp_equality_amo_stale = 0;
  uint64_t nary_csp_equality_amo_duplicate = 0;
  uint64_t nary_csp_equality_amo_declined = 0;
  uint64_t nary_csp_equality_amo_recognition_work = 0;
  uint64_t nary_csp_equality_amo_proof_work = 0;
  uint64_t nary_csp_equality_amo_proof_additions = 0;
  uint64_t nary_csp_equality_amo_proof_deletions = 0;
  uint64_t nary_csp_equality_amo_proof_literal_occurrences = 0;
  uint64_t nary_csp_equality_amo_journal_bytes = 0;
  uint64_t nary_csp_equality_amo_binary_clauses = 0;
  uint64_t nary_csp_equality_amo_session_commits = 0;
  uint64_t nary_csp_equality_transitivity_invalid = 0;
  uint64_t nary_csp_equality_transitivity_inapplicable = 0;
  uint64_t nary_csp_equality_transitivity_resource_exhausted = 0;
  uint64_t nary_csp_equality_transitivity_interrupted = 0;
  uint64_t nary_csp_equality_transitivity_unsupported_proof = 0;
  uint64_t nary_csp_equality_transitivity_attempts = 0;
  uint64_t nary_csp_equality_transitivity_applied = 0;
  uint64_t nary_csp_equality_transitivity_missing_equality = 0;
  uint64_t nary_csp_equality_transitivity_stale = 0;
  uint64_t nary_csp_equality_transitivity_duplicate = 0;
  uint64_t nary_csp_equality_transitivity_declined = 0;
  uint64_t nary_csp_equality_transitivity_recognition_work = 0;
  uint64_t nary_csp_equality_transitivity_proof_work = 0;
  uint64_t nary_csp_equality_transitivity_proof_additions = 0;
  uint64_t nary_csp_equality_transitivity_proof_deletions = 0;
  uint64_t nary_csp_equality_transitivity_proof_literal_occurrences = 0;
  uint64_t nary_csp_equality_transitivity_journal_bytes = 0;
  uint64_t nary_csp_equality_transitivity_ternary_clauses = 0;
  uint64_t nary_csp_equality_transitivity_session_commits = 0;
  uint64_t nary_csp_equality_transitivity_planning_work = 0;
  uint64_t nary_csp_equality_transitivity_preparation_work = 0;
  uint64_t nary_csp_equality_transitivity_source_clauses = 0;
  uint64_t nary_csp_equality_transitivity_source_literals = 0;
  uint64_t nary_csp_equality_transitivity_session_bytes = 0;

  uint64_t direct_php_attempts = 0, direct_php_applied = 0;
  uint64_t direct_php_invalid = 0, direct_php_inapplicable = 0;
  uint64_t direct_php_not_recognized = 0, direct_php_malformed = 0;
  uint64_t direct_php_ambiguous = 0, direct_php_resource_exhausted = 0;
  uint64_t direct_php_interrupted = 0, direct_php_unsupported_proof = 0;
  uint64_t direct_php_unsupported_proof_split = 0, direct_php_stale = 0;
  uint64_t direct_php_already_applied = 0, direct_php_replay_mismatch = 0;
  uint64_t direct_php_prepared = 0;
  uint64_t direct_php_snapshot_clauses = 0, direct_php_snapshot_literals = 0;
  uint64_t direct_php_snapshot_work = 0, direct_php_recognition_work = 0;
  uint64_t direct_php_replay_work = 0;
  uint64_t direct_php_proof_additions = 0, direct_php_proof_rup_additions = 0;
  uint64_t direct_php_proof_rat_additions = 0;
  uint64_t direct_php_proof_deletions = 0;
  uint64_t direct_php_proof_addition_literals = 0;
  uint64_t direct_php_proof_deletion_literals = 0;
  uint64_t direct_php_proof_fresh_variables = 0, direct_php_proof_events = 0;
  uint64_t direct_php_proof_maximum_event_width = 0;
  uint64_t gf3_gaussian_attempts = 0, gf3_gaussian_applied = 0;
  uint64_t gf3_gaussian_invalid = 0, gf3_gaussian_inapplicable = 0;
  uint64_t gf3_gaussian_no_structure = 0;
  uint64_t gf3_gaussian_no_supported_outcome = 0;
  uint64_t gf3_gaussian_already_published = 0;
  uint64_t gf3_gaussian_exhausted = 0, gf3_gaussian_interrupted = 0;
  uint64_t gf3_gaussian_stale = 0, gf3_gaussian_unsupported_proof = 0;
  uint64_t gf3_gaussian_source_clauses = 0;
  uint64_t gf3_gaussian_source_literals = 0;
  uint64_t gf3_gaussian_capture_work = 0, gf3_gaussian_bridge_work = 0;
  uint64_t gf3_gaussian_certificate_work = 0;
  uint64_t gf3_gaussian_validation_work = 0;
  uint64_t gf3_gaussian_revalidation_work = 0;
  uint64_t gf3_gaussian_proof_additions = 0;
  uint64_t gf3_gaussian_proof_literals = 0;
  uint64_t gf3_gaussian_installed_clauses = 0;
  uint64_t gf3_gaussian_duplicate_clauses = 0;
  uint64_t gf3_gaussian_journal_work = 0;
  uint64_t gf3_gaussian_journal_bytes = 0;
  uint64_t gf3_gaussian_session_work = 0;
  uint64_t gf3_gaussian_identity_work = 0;
  uint64_t gf3_gaussian_identity_bytes = 0;
  uint64_t gf3_gaussian_session_identity_bytes = 0;
  uint64_t gf3_gaussian_units = 0, gf3_gaussian_conflicts = 0;
  uint64_t gf3_general_row_attempts = 0, gf3_general_row_applied = 0;
  uint64_t gf3_general_row_invalid = 0, gf3_general_row_inapplicable = 0;
  uint64_t gf3_general_row_no_structure = 0;
  uint64_t gf3_general_row_no_consequence = 0;
  uint64_t gf3_general_row_already_published = 0;
  uint64_t gf3_general_row_exhausted = 0;
  uint64_t gf3_general_row_interrupted = 0;
  uint64_t gf3_general_row_stale = 0;
  uint64_t gf3_general_row_unsupported_proof = 0;
  uint64_t gf3_general_row_source_clauses = 0;
  uint64_t gf3_general_row_source_literals = 0;
  uint64_t gf3_general_row_capture_work = 0;
  uint64_t gf3_general_row_bridge_work = 0;
  uint64_t gf3_general_row_planning_work = 0;
  uint64_t gf3_general_row_validation_work = 0;
  uint64_t gf3_general_row_duplicate_scan_work = 0;
  uint64_t gf3_general_row_revalidation_work = 0;
  uint64_t gf3_general_row_proof_additions = 0;
  uint64_t gf3_general_row_proof_literals = 0;
  uint64_t gf3_general_row_arena_words = 0;
  uint64_t gf3_general_row_journal_bytes = 0;
  uint64_t gf3_general_row_identity_bytes = 0;
  uint64_t gf3_general_row_session_work = 0;
  uint64_t gf3_general_row_session_identity_bytes = 0;
  uint64_t gf3_general_row_installed_clauses = 0;
  uint64_t gf3_general_row_semantic_variables = 0;
  uint64_t tseitin_cycle_attempts = 0, tseitin_cycle_applied = 0;
  uint64_t tseitin_cycle_invalid = 0, tseitin_cycle_inapplicable = 0;
  uint64_t tseitin_cycle_no_structure = 0;
  uint64_t tseitin_cycle_already_applied = 0;
  uint64_t tseitin_cycle_exhausted = 0, tseitin_cycle_interrupted = 0;
  uint64_t tseitin_cycle_stale = 0, tseitin_cycle_unsupported_proof = 0;
  uint64_t tseitin_cycle_snapshot_clauses = 0;
  uint64_t tseitin_cycle_snapshot_literals = 0;
  uint64_t tseitin_cycle_snapshot_work = 0;
  uint64_t tseitin_cycle_recapture_work = 0;
  uint64_t tseitin_cycle_discovery_work = 0;
  uint64_t tseitin_cycle_retained_bytes = 0;
  uint64_t tseitin_cycle_admission_work = 0;
  uint64_t tseitin_cycle_planning_work = 0;
  uint64_t tseitin_cycle_transaction_work = 0;
  uint64_t tseitin_cycle_validation_ticks = 0;
  uint64_t tseitin_cycle_candidates = 0, tseitin_cycle_offers = 0;
  uint64_t tseitin_cycle_identity_bytes = 0;
  uint64_t tseitin_cycle_identity_work = 0;
  uint64_t tseitin_cycle_journal_work = 0;
  uint64_t tseitin_cycle_journal_bytes = 0;
  uint64_t tseitin_cycle_session_work = 0;
  uint64_t tseitin_cycle_session_identity_bytes = 0;
  uint64_t tseitin_cycle_proof_additions = 0;
  uint64_t tseitin_cycle_proof_deletions = 0;
  uint64_t tseitin_cycle_proof_literal_occurrences = 0;
  uint64_t tseitin_cycle_proof_helpers = 0;
  uint64_t tseitin_cycle_proof_only_variables = 0;
  uint64_t tseitin_cycle_proof_witness_literals = 0;
  uint64_t tseitin_cycle_proof_routes = 0;
  uint64_t tseitin_cycle_units = 0;
  uint64_t congruence_rounds = 0;  // Congruence rounds — gate-closure passes
  uint64_t congruence_binary_rebuilds = 0;
  uint64_t congruence_unjustified = 0;
  uint64_t congruence_units = 0; // root units from degenerate gates
  uint64_t congruence_y1_found = 0; // detections
  uint64_t congruence_y1_units = 0; // ... emitted and batched
  uint64_t congruence_y1_ticks = 0; // one complete binary-watch pass + replay
  uint64_t congruence_binary_equivalences = 0; // paired binaries imported
  mutable uint64_t congruence_gates_and = 0;
  mutable uint64_t congruence_gates_xor = 0;
  mutable uint64_t congruence_gates_ite = 0;
  uint64_t congruence_gates = 0;   // gates EXTRACTED, over all applications
  uint64_t congruence_merges = 0;  // equivalences MERGED, over all applications
  mutable uint64_t congruence_ticks = 0;
  mutable uint64_t congruence_budget_stops = 0; // rounds cut by `work_budget`
  uint64_t factorings = 0;         // Factor/BVA applications — grids replaced
  uint64_t factor_large_remainders = 0; // remainders offered by the widened
  uint64_t factor_remainders_materialized = 0; // immutable clause remainders
  uint64_t factor_remainders_reused = 0; // ... served from the dense watch
  uint64_t factor_demotions = 0;    // demotion calls that moved something
  uint64_t factor_demoted_vars = 0; // extension vars pushed to the queue front
  uint64_t factor_ticks = 0;   // Factor's budget currency (kissat
  uint64_t bve_rounds = 0;         // BVE canonical-apply calls (a policy that
  uint64_t bve_eliminated = 0;     // variables eliminated by BVE resolution
  uint64_t bve_resolutions = 0;    // resolvents attempted — BVE's unit of work
  uint64_t fastel_rounds = 0;      // startup owner-pass admission checks
  uint64_t fastel_candidates = 0;  // candidates in those checked domains
  uint64_t fastel_active = 0;      // active variables at those checks
  uint64_t fastel_discovery_ticks = 0; // live variables inspected by admission
  uint64_t fastel_budget_stops = 0; // domains exceeding the shared effort dose
  uint64_t fastel_declined = 0;    // domains where the owner pass was minority
  uint64_t fastel_root_suppressed = 0; // producer-zero: startup FastEliminate is never suppressed
  uint64_t fastel_eliminated = 0;  // eliminations committed by startup fastel
  uint64_t fastel_resolutions = 0; // BVE work committed by startup fastel
  uint64_t fastel_nanos = 0;       // complete startup fastel phase time
  uint64_t bve_bwd_queued = 0;      // fresh resolvents used as subsumers
  uint64_t bve_bwd_subsumed = 0;    // victims deleted (cadical elimbwsub)
  uint64_t bve_bwd_strengthened = 0; // victims shortened (cadical elimbwstr)
  uint64_t bve_bwd_units = 0;       // strengthenings that collapsed to a unit
  uint64_t flushed_retired_redundant = 0; // reference-parity flush at phase exit
  uint64_t factor_rejected_by_bound = 0; // grids the coupled bound declined
  uint64_t bve_marks = 0;
  uint64_t touch_marks = 0;
  uint64_t touch_kept_marks = 0;
  uint64_t forward_mark_skipped = 0;
  uint64_t strengthen_mark_skipped = 0;
  uint64_t forward_mark_advances = 0;
  uint64_t strengthen_mark_advances = 0;
  uint64_t forward_rearmed = 0;
  uint64_t strengthen_rearmed = 0;
  uint64_t bce_mark_skipped = 0, bce_rearmed = 0;
  uint64_t ternary_mark_skipped = 0, ternary_rearmed = 0;
  uint64_t instantiate_mark_skipped = 0, instantiate_rearmed = 0;
  uint64_t bce_interrupted = 0;
  uint64_t ternary_interrupted = 0;
  uint64_t instantiate_interrupted = 0;
  uint64_t factor_mark_skipped = 0, factor_rearmed = 0;
  uint64_t subsume_holds = 0; // outer elimination rounds held open because
  uint64_t touch_redundant_dynamic = 0;
  uint64_t touch_redundant_static6 = 0;
  uint64_t strengthen_red_checked = 0;
  uint64_t strengthen_red_skipped = 0;
  uint64_t strengthen_red_subsumed = 0;
  uint64_t strengthen_red_strengthened = 0;
  uint64_t cce_mark_skipped = 0, cce_rearmed = 0, cce_mark_advances = 0;
  uint64_t bve_gates_equiv = 0;
  uint64_t bve_gates_and_or = 0;
  uint64_t bve_gates_ite = 0;
  uint64_t bve_gates_xor = 0;
  uint64_t bve_definitions_checked = 0; // attempts
  uint64_t bve_gates_definition = 0;    // ... that found a definition
  uint64_t bve_definition_ticks = 0;
  uint64_t bve_definition_budget = 0;   // attempts the per-attempt tick ceiling
  uint64_t bve_definition_units = 0;    // one-sided cores / failed pivots
  uint64_t bve_gate_pairs_saved = 0; // resolvent pairs the gate reduction
  // Appended so extending the public semantic HBR surface does not perturb the
  // layout of existing hot counters.
  uint64_t hbr_units = 0; // explicit semantic resolvents collapsed to units
  // Appended diagnostics: completed Prepared normalization certificates, and
  // actual committed shortened RUP premises; work remains recognition_work.
  uint64_t affine_matrix_init_normalization_rup_checks = 0;
  uint64_t affine_matrix_init_shortening_additions = 0;
  uint64_t affine_matrix_init_maintenance_service_work = 0;
  uint64_t affine_matrix_init_discovery_recognized_groups = 0;
  uint64_t affine_matrix_init_discovery_skipped_groups = 0;
  uint64_t affine_matrix_init_discovery_recognizer_work = 0;
  uint64_t affine_matrix_service_pending_slices = 0;
  uint64_t affine_matrix_service_resumed_slices = 0;
  uint64_t affine_matrix_service_slice_steps = 0;
  uint64_t affine_matrix_service_slice_work = 0;
  uint64_t affine_matrix_service_slice_invalidations = 0;
  uint64_t affine_matrix_service_minimum_slice_work_max = 0;
};

struct PendingLearned {
  bool valid = false;
  std::vector<Lit> lits; // lits[0] = asserting literal
  Ref ref = INVALID_REF; // arena ref for size >= 3, INVALID otherwise
  uint32_t glue = 0;
  uint32_t jump_level = 0;
  uint64_t id = 0; // proof clause id (LRAT)
};

struct OccIndex {
  FlatLists<Ref> large; // Probe-only owner; may hold stale garbage Refs
  // Kissat dense watches are one mixed connected stream: the binary prefix
  // present at dense entry, then arena-order large occurrences, followed by
  // every binary or large clause created while dense. Other simplifiers use
  // its large-only projection; resolvent insertion order is search-visible.
  using Entry = ConnectedOccurrences::Entry;
  static_assert(sizeof(Entry) == sizeof(uint32_t),
                "dense mixed entries must match Kissat's one-word watch");
  ConnectedOccurrences ordered; // sole payload owner in mixed mode
  // Only Eliminate/BVE consumes the mixed stream. Probe's Congruence,
  // Substitute, Sweep, and Strengthen consumers use `large` and must not pay
  // another formula-wide vector header and payload image.
  bool track_order = false;

  size_t large_lits() const { return track_order ? ordered.lits() : large.lits(); }
  uint32_t large_size(Lit lit) const {
    return track_order ? ordered.large_size(lit) : large.size(lit);
  }
  // Dispatch once outside the consumer's loop. Probe retains its direct
  // contiguous Ref iteration; free N1 callers can use the mixed projection.
  template <class Visit> decltype(auto) with_large(Lit lit, Visit visit) const {
    if (track_order)
      return visit(ordered.large_refs(lit));
    return visit(large[lit]);
  }
  void copy_large(Lit lit, std::vector<Ref> &out) const {
    with_large(lit, [&](auto refs) { out.assign(refs.begin(), refs.end()); });
  }
  void push_large(Lit lit, Ref ref) {
    if (track_order) ordered.push(lit, Entry::from_large(ref));
    else large.push(lit, ref);
  }
  void clear_large(Lit lit) {
    if (track_order) ordered.compact_large(lit, [](Ref) { return false; });
    else large.clear(lit);
  }
  template <class Keep> void compact_large(Lit lit, Keep keep) {
    if (track_order) ordered.compact_large(lit, keep);
    else {
      const auto refs = large[lit];
      size_t out = 0;
      for (const Ref ref : refs)
        if (keep(ref)) refs[out++] = ref;
      large.shrink(lit, out);
    }
  }
};

struct ArenaOrdering {
  virtual ~ArenaOrdering() = default;
  virtual void order(const Kernel &, std::vector<Ref> &) = 0;
};

struct ProbingLog {
  bool recording = false;
  bool rollback_pose = true;
  uint32_t base_trail = 0, base_head = 0, base_level = 0;
  struct ListSnap {
    Lit lit;
    uint32_t off, size; // offset into `words`, and the list's original length
    uint32_t binaries;
    bool has_large, binaries_first;
  };
  std::vector<ListSnap> lists;
  std::vector<uint32_t> words; // flat backing store for the snapshots above
  std::vector<uint32_t> stamp; // by lit: epoch this literal was snapshotted in
  uint32_t epoch = 0;          // bumped per scope; 0 is "never snapshotted"
  struct ClauseSnap {
    Ref ref;
    uint32_t rpos;             // index the replacement literal came from (>= 2)
    Lit lit0, lit1;            // the watched pair before the relocation
    uint32_t searched;
  };
  std::vector<ClauseSnap> clauses;
};

enum class VarStatus : uint8_t { Active = 0, Substituted, Eliminated };

// Analyze is atomic at the rule boundary, but Kissat updates a policy clock
// between OTFS re-analysis rounds. Preserve the exact observation made at
// that internal boundary so DefaultPolicy can consume it after the rule
// returns without moving strategy logic into the proof rule.
struct AnalysisConflictCheckpoint {
  uint64_t conflicts = 0;
  uint32_t mode = 0;
  uint64_t used_glue[Stats::MAX_GLUE_USED + 1] = {};
};

struct Kernel;
using AnalysisConflictHook = void (*)(Kernel &, void *);
// Policy-only observer for Kissat's mutable BVE schedule. Clause mutations
// remain kernel operations; while a BVE dose owns a live heap it installs this
// non-owning hook so each affected variable is rescored at the same mutation
// boundary. `reinsert` distinguishes removed material (which rearms a popped
// candidate) from added material (which only changes an entry still present).
using BveScoreUpdateHook = void (*)(Kernel &, void *, Var, bool reinsert);

// Search assigns at most one literal per variable, and every variable-growth
// path reserves the trail before an assignment can mention the new identity.
// Keep that invariant explicit instead of paying std::vector's generic
// grow-capable push call for every propagation.  The backing vector is sized to
// the reservation while `used_` is the logical end, matching Kissat's reserved
// raw trail stack without exposing allocation policy to search or rules.
class ReservedTrail {
public:
  using iterator = std::vector<Lit>::iterator;
  using const_iterator = std::vector<Lit>::const_iterator;

  size_t size() const { return used_; }
  size_t capacity() const { return storage_.size(); }
  bool empty() const { return !used_; }

  iterator begin() { return storage_.begin(); }
  const_iterator begin() const { return storage_.begin(); }
  iterator end() { return storage_.begin() + used_; }
  const_iterator end() const { return storage_.begin() + used_; }

  Lit &operator[](size_t i) {
    assert(i < used_);
    return storage_[i];
  }
  const Lit &operator[](size_t i) const {
    assert(i < used_);
    return storage_[i];
  }
  Lit &back() {
    assert(used_);
    return storage_[used_ - 1];
  }
  const Lit &back() const {
    assert(used_);
    return storage_[used_ - 1];
  }

  void reserve(size_t n) {
    if (storage_.size() < n)
      storage_.resize(n);
  }
  [[gnu::always_inline]] void push_back(Lit lit) {
    assert(used_ < storage_.size());
    storage_[used_++] = lit;
  }
  void pop_back() {
    assert(used_);
    --used_;
  }
  void resize(size_t n) {
    assert(n <= used_);
    used_ = n;
  }

  // Variable compaction already builds the replacement trail in a temporary
  // std::vector.  Adopt its allocation, then expose its spare reservation as
  // backing size so the direct push invariant remains true afterward.
  void swap(std::vector<Lit> &other) {
    const size_t next_used = other.size();
    storage_.swap(other);
    used_ = next_used;
    storage_.resize(storage_.capacity());
  }

  operator std::vector<Lit>() const {
    return std::vector<Lit>(begin(), end());
  }
  bool operator==(const std::vector<Lit> &other) const {
    return size() == other.size() &&
           std::equal(begin(), end(), other.begin());
  }

private:
  std::vector<Lit> storage_;
  size_t used_ = 0;
};

// Provenance of symmetry-breaking output (plan §9.5, per-run tags): which
// published clauses search actually consumed; no decision reads symmetry runs.
// The AFFINE class (per affine run epoch) feeds DefaultPolicy's affine delay
// (docs/implementation-notes.md). Large clauses are identified by proof id,
// retired exactly by kernel_delete_clause; binaries and units keep a stable
// external identity and an internal filter that compaction rebuilds.
// `Kernel::symuse_active` stays false until the first tag. Propagation is
// observed by rescanning the trail segment a propagation call assigned, after
// the call returns, so the propagation loop itself carries no bookkeeping;
// analysis pays one predictable branch per resolved antecedent.
struct SymmetryUseRun {
  uint64_t clauses = 0, helpers = 0, units = 0, binaries = 0, large = 0;
  // Assignments whose reason is a tagged binary/large clause, made by search
  // propagation (propagate_service), probing, warm-up, binary-only
  // propagation or a conflict-reuse assertion: helper share, and the share
  // made outside inprocessing, warm-up and probing.
  // For AFFINE runs, search_propagations counts only assignments above the
  // root: a root-published affine clause's own root propagation is not use.
  uint64_t propagations = 0, helper_propagations = 0, search_propagations = 0;
  // Conflict clauses and reasons resolved by ConflictAnalysis (1UIP and the
  // failed-level-one walk; not minimization or shrinking).
  uint64_t analysis_uses = 0, helper_analysis_uses = 0;
  // Root implications whose reason contains the negation of a tagged unit
  // (one level: the unit's own consequences, not their descendants). When the
  // run tagged no binary and no tagged large clause is live, only propagation
  // at the root is inspected for this. A reason that is itself an AFFINE
  // clause is not attributed: Gaussian output consumed by Gaussian output.
  uint64_t root_forced = 0;
  uint64_t large_deleted = 0; // retired by any path, in-place shrink included
};

struct SymmetryUse {
  static constexpr uint64_t HELPER = 1u << 8; // tag value: (run + 1) | HELPER
  // Affine tag value: AFFINE | (epoch << AFFINE_SHIFT), epoch saturating at
  // AFFINE_EPOCHS - 1; counted in `affine_runs[epoch]`, never in `runs`.
  static constexpr uint64_t AFFINE = 1u << 9;
  static constexpr uint32_t AFFINE_SHIFT = 10;
  static constexpr uint32_t AFFINE_EPOCHS = 1u << (32 - AFFINE_SHIFT);
  static constexpr uint8_t BINARY_MARK = 1;   // lit occurs in a tagged binary
  static constexpr uint8_t UNIT_MARK = 2;     // lit is a tagged unit's negation
  struct StableBinary {
    Lit a, b; // external literals
    uint32_t tag;
  };
  struct StableUnit {
    Lit lit; // external literal
    uint32_t tag;
  };
  uint8_t run = 0; // tag for new publications
  std::vector<SymmetryUseRun> runs;
  std::vector<SymmetryUseRun> affine_runs; // indexed by affine run epoch
  std::vector<StableBinary> binaries;
  std::vector<StableUnit> units;
  // Internal-space filters, rebuilt by kernel_compact_variables.
  std::vector<uint8_t> lit_marks;
  BinaryIdMap binary_tags; // internal binary key -> tag
  BinaryIdMap unit_tags;   // internal unit literal -> tag
  BinaryIdMap large_tags;  // proof id -> tag, zero once retired
  uint64_t large_min = UINT64_MAX, large_max = 0; // empty range: no large tag
  uint64_t live_large = 0; // tagged large clauses not yet retired
  bool has_units = false;
  // SR symmetry output is tagged only when set (diagnostic `c symuse` report;
  // DefaultStrategyConfig::symmetry_use_observation). Off, no symmetry tag
  // arms `symuse_active`, so the trail/analysis/delete hooks cost nothing.
  bool record_symmetry = false;
};

struct Kernel {
  uint32_t nvars = 0;
  // Keep the Kernel ABI identical when the standalone event-trace test links
  // the production object library (`tests/CMakeLists.txt`).
  size_t diagnostic_policy_reduce_bytes = 0;
  size_t diagnostic_policy_inprocess_bytes = 0;
  // Stable external identities are separate from the dense indices used by
  // search.  Before the first compaction both maps are the identity.  A dead
  // external variable maps to INVALID_VAR; every live internal variable maps
  // back to the permanent DIMACS/proof variable that names it.
  std::vector<Var> internal_to_external;
  std::vector<Var> external_to_internal;
  // Kissat-style live compact identity, represented as a permutation over
  // stable kernel variables plus its inverse.  This is formula representation
  // state rather than search guidance: proof rules may compare these identities
  // without importing the policy-owned heuristic object.
  std::vector<Var> compact_import_order;
  std::vector<Var> compact_import_position;
  uint32_t original_external_vars = UINT32_MAX;
  uint32_t first_extension_var = UINT32_MAX;
  bool inconsistent = false; // root-level conflict happened

  std::vector<int8_t> values;     // by lit: 1 true, -1 false, 0 unassigned
  std::vector<Assigned> assigned; // by var
  std::vector<Lit> jumped_over;

  ReservedTrail trail;
  uint32_t propagate_head = 0;
  // Lowest trail position that BINARY-ONLY propagation moved `propagate_head`
  // past and full propagation has not re-covered; UINT32_MAX means none.
  // `propagate_head` means full propagation has consumed the literal, either by
  // completing its watch scan or by finding the current conflict. The binary
  // propagator cannot establish that contract because it skips every large
  // watch. A literal it covered that STAYS on the trail is otherwise credited
  // forever, so the trail can reach a claimed fixpoint that still hides a large
  // unit or conflict, and a total assignment built on it falsifies a clause.
  // The next full propagation pulls the cursor back to this debt and rescans.
  // A speculative caller that retracts everything it scanned leaves the debt
  // equal to the cursor, so the pull-back is inert for it.
  uint32_t binary_only_head = UINT32_MAX;
  bool trail_tainted = false;
  std::vector<uint32_t> frames = {0};
  uint32_t level = 0;
  uint32_t unassigned = 0;
  uint32_t unassigned_active = 0;
  bool warming = false; // Kissat walk warm-up: suppress random branching.
  bool symuse_active = false; // some clause carries a SymmetryUse tag
  uint64_t walk_random_seed = 0; // policy RNG snapshot for a faithful Walk

  Arena arena;
  // A packed rewrite is appended while Kissat shortens in place, so its new
  // proof id stores source-position+1 here (zero means no alias). Collection
  // materializes this generation-local position and clears the image. This
  // keeps later sparse-collection permutations authoritative.
  ClausePositionAliases clause_position_alias;
  // Historical logical arena position of the first reducible record. Promotion
  // to irredundant does not advance this collection boundary. Reduce captures
  // it before advancing the next-call marker; genuine collection recomputes it.
  Ref first_reducible = INVALID_REF;
  WatchLists watches;
  // Kissat's `watching` is false while a dense/Vivify phase has disconnected
  // every large watch. Binary watches remain live in both states.
  bool large_watches_connected = true;
  ArenaOrdering *arena_ordering = nullptr;
  OccIndex *dense = nullptr;
  // Literals whose whole dense large-occurrence list the current caller will
  // clear in one pass. An in-place rewrite then skips the per-clause search
  // and erase for them: substitution retires a literal from every clause at
  // once, so removing its occurrences one at a time is quadratic in its
  // degree, and kissat's substitution walks each literal's list once instead.
  const std::vector<uint8_t> *dense_bulk_cleared = nullptr;
  // Reusable marks for the above; never sized per call.
  std::vector<uint8_t> dense_bulk_scratch;
  // Collection's survivor list. Kernel-owned so a collection does not
  // reallocate one clause-sized vector per call.
  std::vector<Ref> collect_order_scratch;
  // The dense occurrence rebuild's reference list, kept separate because
  // collection calls that rebuild while its own survivor list is still live.
  std::vector<Ref> dense_order_scratch;
  // Scratch capacity for delayed large-watch relocations.  `delayed_used`,
  // not vector::size(), is the live stack length: propagation grows this
  // backing store once per largest watch list and writes triples directly,
  // avoiding three checked vector pushes on the hottest relocation path.
  std::vector<uint32_t> delayed;
  size_t delayed_used = 0;
  ProbingLog probing;

  Heuristics *h = nullptr;

  std::vector<Lit> reconstruction;
  std::vector<int8_t> model_values; // by stable external literal, after extend
  bool model_extended = false; // kernel_extend_model idempotency guard

  bool conflict_valid = false;
  Ref conflict_ref = INVALID_REF; // INVALID means binary conflict
  Lit conflict_bin[2] = {INVALID_LIT, INVALID_LIT};

  PendingLearned pending;

  std::vector<uint8_t> seen;   // by var
  std::vector<Var> analyzed;   // every var whose analysis scratch needs reset
  // Exact admission order of the SEEN | REASON_BUMP set.  Kissat keeps this
  // order on a stack separate from minimization/removability scratch; stable
  // EVSIDS heap topology is observable when equal increments are applied.
  std::vector<Var> bump_order;
  std::vector<Var> shrinkable; // vars marked during a block-shrink walk
  uint32_t analyzed_seen = 0;
  std::vector<Var> reason_bumped; // REASON_BUMP marks, for rollback
  std::vector<Var> minimize_scratch; // iterative binary-reason chain
  std::vector<Lit> learned_scratch; // the clause under construction
  std::vector<Lit> otfs_scratch;    // OTFS resolvent assembly
  // OTFS materializes a replacement because variable-size arena records can
  // not shrink in place.  The old reason stays valid until Backjump unassigns
  // its propagated literal, then is retired before the fused rule returns.
  std::vector<Ref> otfs_retired;
  std::vector<AnalysisConflictCheckpoint> analysis_conflict_checkpoints;
  AnalysisConflictHook analysis_conflict_hook = nullptr;
  void *analysis_conflict_hook_state = nullptr;
  // Policy observation after the final learned clause has joined the formula
  // and while the analyzed set still names that conflict's variables.
  std::vector<Var> radix_scratch;   // radix-sort double buffer
  std::vector<uint8_t> marked_levels;
  std::vector<uint32_t> touched_levels;

  static constexpr uint32_t EAGER_SUBSUME_SLOTS =
      strategy::AnalyzeParameters::EAGER_SUBSUME_CAPACITY;
  Ref eager_recent[EAGER_SUBSUME_SLOTS] = {INVALID_REF, INVALID_REF,
                                           INVALID_REF, INVALID_REF};
  std::vector<uint8_t> eager_marks;

  uint64_t next_clause_id = 0;
  // Live irredundant large clauses. kissat maintains the same quantity as
  // `statistics.clauses_irredundant` and reads it in constant time from
  // `kissat_classify` and `BINIRR_CLAUSES`; walking the whole arena for it was
  // 4.4% of Rank 6 across 86 calls. The population changes only where a large
  // record is created, retired, reused in place, or flips redundancy.
  uint64_t live_irredundant_large = 0;
  std::vector<uint64_t> unit_ids; // by var
  // DSR must publish propagation-derived root facts without turning them into
  // solver-side asserted units.  Keep that proof-only deduplication state
  // separate from `unit_ids`, whose presence is observable by simplification
  // rules and therefore affects the search trajectory.
  // Kernel root units already written to the native-DSR proof, by variable:
  // emitted polarity, 0 if none.  Never a unit the kernel does not hold.
  std::vector<int8_t> dsr_root_values;
  const Proof *dsr_root_proof = nullptr;
  uint64_t dsr_root_stream_identity = 0;
  BinaryIdMap binary_ids;
  // Eager input for later native-LRAT replay retains exact binary identities
  // until the proof is attached; clause admission and root work remain eager.
  bool replay_lrat_input = false;
  BinaryLivenessIndex *binary_liveness = nullptr; // scoped dense-rule index
  std::vector<std::pair<uint32_t, uint64_t>> lrat_entries;
  std::vector<uint64_t> lrat_chain; // built by kernel_lrat_chain below
  std::vector<Var> reason_walk_stack; // reusable proof-reason DFS stack
  std::vector<Var> jump_pending;

  struct InputRestatement {
    uint64_t id;    // the file-position id; final antecedent of the lemma
    uint32_t begin; // slice of input_restate_lits (the normalized clause)
    uint32_t size;
    uint32_t unit_begin; // root-true units used to remove false literals
    uint32_t unit_size;
    Ref ref; // large clauses only: the header to re-point
  };
  std::vector<InputRestatement> input_restatements;
  std::vector<Lit> input_restate_lits;
  std::vector<Lit> input_restate_units;
  std::vector<Var> input_activation_order;
  // Bounded parse-order image for startup structural model recovery.  It is
  // populated only for small formulas within the direct-CSP admission limits,
  // preserved through the proof-producing structured root pass, and released
  // after the subsequent model-search opportunity.
  std::vector<uint64_t> input_binary_order;
  bool input_binary_order_captured = false;
  // The Fermat owner inspects the two long input blocks and the final 80 raw
  // clauses.  Preserve only the sufficient structural summary: exact long
  // clause count/shape plus a fixed-size ring of raw binary DIMACS pairs.
  // This avoids retaining the input formula or scanning it again at startup.
  static constexpr uint32_t INPUT_CLAUSE_TAIL_CAPACITY = 80;
  std::vector<std::array<int32_t, 2>> input_clause_tail;
  uint32_t input_clause_tail_next = 0;
  uint32_t input_long_clause_count = 0;
  uint64_t input_positive_binary_count = 0;
  uint64_t input_binary_clause_count = 0;
  uint64_t input_ternary_clause_count = 0;
  uint64_t input_unit_clause_count = 0;
  // Original-input terminal-certificate boundary. A policy or dispatcher
  // closes it before its first ordinary rule; root restarts do not reopen it.
  bool startup_certificate_open = true;
  // Original DIMACS literals, zero-delimited, only until the first ordinary
  // action. Count-preservation recognition needs clauses lost to input units.
  std::vector<int32_t> startup_input;
  void close_startup_certificate() {
    if (!startup_certificate_open)
      return;
    startup_certificate_open = false;
    std::vector<int32_t>().swap(startup_input);
  }
  uint64_t input_nonunit_clause_count = 0;
  uint32_t input_uniform_nonunit_width = 0;
  bool input_nonunit_uniform = true;
  uint32_t input_fermat_width = 0;
  bool input_fermat_long_shape = true;
  std::vector<uint8_t> input_activated; // per-var membership, parse-scoped
  std::vector<uint8_t> input_marks;     // per-lit membership, parse-scoped
  // Permanent external-variable import ranks. Kissat's tumble=1 assigns
  // internal variables on first input occurrence; Tactix keeps public internal
  // identities stable and consults this equivalent order at reference-ordered
  // traversals instead.
  std::vector<uint32_t> external_import_rank;
  // Direct image of the same permanent rank in current compact identities.
  // UINT32_MAX means the variable has not occurred in input yet. Variable
  // compaction remaps this image with every other per-current-variable field;
  // the external rank remains the permanent proof/reconstruction authority.
  std::vector<uint32_t> internal_import_rank;
  // Inverse of `external_import_rank` in permanent first-occurrence order.
  // Retired external identities remain here as holes and are resolved through
  // `external_to_internal`, allowing live import order to be reconstructed in
  // linear time after variable compaction.
  std::vector<Var> import_rank_to_external;
  uint32_t next_import_rank = 0;
  bool input_refuted = false;
  uint64_t input_refutation_id = 0; // the refuting input clause
  std::vector<Lit> input_refutation_units; // root-true antecedent units

  Proof *proof = nullptr;
  // Always-compiled, nullable Stage-0 population seam. The observer is
  // non-owning; ScopedPopulationObserverBinding settles Proof before unbind.
  PopulationObserver *population_observer = nullptr;
  bool population_observe_raw_input = true;
  bool population_observe_solver = true;
  uint64_t next_population_transaction = 0;
  Stats stats;

  // The shipped DefaultPolicy can finish at startup propagation or early
  // Lucky, before any inprocessing/search consumer exists. Its frontend may
  // defer this cold per-variable state until that opportunity fails. Bare
  // kernels, API clients, random/replay policies and stress policies retain
  // eager construction, so the public rule surface keeps its old contract.
  bool cold_state_deferred = false;
  std::vector<uint8_t> bve_retry;

  // Exact persistent Kissat `flags->factor` bit, by literal polarity. Clause
  // addition sets it; Factor clears it when that literal is popped. Unlike the
  // shared per-variable touch epochs, this does not spuriously arm the opposite
  // polarity.
  std::vector<uint8_t> factor_marks;
  // Kissat's cumulative `literals_factor` watermark.  It advances only on a
  // zero-to-one factor-bit transition and is intentionally not decremented
  // when Factor consumes the bit.
  uint64_t factor_literals_marked = 0;
  // Result of the most recent Factor application.  DefaultPolicy copies the
  // cumulative watermark only after a complete schedule drain, matching
  // `factor.c::run_factorization` / `limits.factor.marked`.
  bool factor_completed = false;

  // Kissat retains the surviving Backbone candidate polarities between
  // computations and schedules that priority set before all other literals.
  // Bit 0 is the positive literal and bit 1 the negative literal.
  std::vector<uint8_t> backbone_priority;

  // Bumped by every `kernel_collect_garbage`. A Ref recorded before the bump
  // names the pre-collection arena, so anything holding one across a possible
  // collection compares this instead of reasoning about the call graph.
  uint64_t arena_generation = 1;
  // Bumped by every in-place rewrite of a large record's literal set.
  uint64_t large_clause_rewrites = 0;
  // Large-record references of the variables witness-free SR expansion has
  // asked for, valid for one arena generation and rewrite count; the arena
  // prefix [0, covered) is indexed. Records are filtered when read.
  struct SrExpansionOccurrences {
    uint64_t arena_generation = 0;
    uint64_t rewrites = 0;
    size_t covered = 0;
    std::vector<uint8_t> interest;
    std::vector<std::vector<Ref>> refs;
  } sr_expansion_occurrences;

  // Cold shared-strategy boundary epoch. Registry dispatch and public API
  // mutations advance it; DefaultPolicy's static hot path never reads or
  // writes it. Online selections bind to this value so an intervening action
  // cannot become invisible by restoring the same arena/trail shape.
  uint64_t strategy_epoch = 0;

  uint64_t touch_epoch = 0;
  // Separate native subsumption-material lifecycle. Shared touch epochs also
  // carry removal/root/retry advice and must not be used as this counter.
  // During deferred startup every activated variable is implicitly armed.
  std::vector<uint8_t> subsume_material;
  uint64_t subsume_material_count = 0;
  std::vector<uint64_t> touched; // by var
  std::vector<uint64_t> removed; // by var
  std::vector<uint64_t> touched_kept; // by var
  uint32_t touch_fixed_head = 0;
  // Kissat keeps newly derived root units in an `unflushed` trail prefix.
  // Some macro rules (notably Reduce and Rephase) use its non-emptiness as a
  // control guard for a root unwind/flush.  Tactix retains root literals in
  // its trail permanently, so this separate count carries the same lifecycle
  // fact without changing trail representation.
  uint32_t unflushed_root = 0;
  uint64_t subsume_touch_mark = 0;
  uint64_t strengthen_touch_mark = 0;
  uint64_t bce_touch_mark = 0;
  uint64_t ternary_touch_mark = 0;
  uint64_t instantiate_touch_mark = 0;
  uint64_t cce_touch_mark = 0;
  bool subsume_complete = true;
  bool strengthen_complete = true;
  bool touch_filter = true;
  // Kissat forward.c only admits irredundant victims.  The CaDiCaL
  // likely-kept redundant band remains an explicit diagnostic arm.
  bool touch_redundant = false;
  uint32_t kept_glue = 0;
  uint32_t kept_size = 0;
  BveScoreUpdateHook bve_score_update_hook = nullptr;
  void *bve_score_update_state = nullptr;
  // Optional strategy-owned physical dense-occurrence image.  Dynamic BVE
  // attaches it only while its candidate heap is live; kernel mutation
  // primitives mirror the exact watch/list lifecycle before score callbacks.
  std::vector<uint32_t> *bve_occurrence_count_image = nullptr;

  uint32_t bve_gate_mask = ~0u;
  uint32_t bve_bound = 16;
  // Policy-owned BVE occurrence cap.  UINT32_MAX leaves the public rule
  // unrestricted; DefaultPolicy installs its named strategy configuration.
  uint32_t bve_occurrence_limit = UINT32_MAX;
  // Policy-owned antecedent-size admission bound (`eliminateclslim`).  Kissat
  // declines the candidate before gate/definition extraction if either dense
  // occurrence list contains a live irredundant clause above this size.  The
  // public rule remains unrestricted until a strategy installs the bound.
  uint32_t bve_clause_size_limit = UINT32_MAX;
  // Optional absolute cap on non-tautological resolvents generated for one
  // pivot. UINT32_MAX leaves the canonical public rule unrestricted.
  uint32_t bve_resolvent_count_limit = UINT32_MAX;
  // Optional CaDiCaL-derived eager tail. Current Kissat defers this work to
  // its next forward pass; that reference-aligned strategy is the default,
  // while the proof rule remains available through TACTIX_BVE_BACKWARD=1.
  bool bve_backward = false;
  // Physical traversal profile posed by the typed BVE invocation. Logical
  // BVE semantics are unchanged; startup fastel uses its owner's insertion
  // order while ordinary Eliminate retains Kissat's canonical order.
  bool bve_fastel_traversal = false;
  // Strategy-owned XOR-recognition arity.  The proof rule's representation
  // capacity and the shipped default are named in rules/bve/bve.hpp.
  uint32_t bve_xor_arity_limit = 5;
  uint32_t bve_resolvent_size_limit = 100;
  uint32_t bve_backward_occurrence_limit = 100;
  // Kissat `definitionticks`: first-core Kitten ticks per candidate.
  uint64_t bve_definition_budget = 1000000;
  // Kissat `definitioncores`: refine once after the first extracted core.
  uint32_t bve_definition_cores = 2;
  // Per-refinement Kitten allowance relative to `bve_definition_budget`.
  uint32_t bve_definition_core_budget_multiplier = 10;
  // Strategy-owned lifetime of the embedded definition Kitten. Kissat creates
  // one per elimination invocation and clears it between candidates.
  uint64_t bve_definition_session = 0;
  uint32_t bce_occ_limit = 0;
  bool midround_budget = false;
  uint32_t factor_size = 5;
  bool transitive_early_exit = true;
  bool transitive_live_meter = true;
  bool transitive_budget_checked = false;
  uint32_t ternary_occ_limit = 100;      // cadical ternaryocclim: skip a pivot
  uint32_t ternary_max_add_percent = 1000; // cadical ternarymaxadd: stop a round
  FactorSelection *factor_selection = nullptr;
  uint32_t factor_bound = 0;
  bool factor_demote_fresh = true;
  uint32_t factor_cand_rounds = 2;
  bool no_ext_reason_bump = false;
  std::vector<VarStatus> var_status;
  std::vector<Var> retired_events;
  std::vector<Var> substituted_events;
  struct FreshVars {
    Var first = 0;
    Var end = 0;
  };
  std::vector<FreshVars> fresh_var_events;
  bool subst_demote = false;
  volatile sig_atomic_t *stop = nullptr;
  uint32_t stop_fuzz = 0;
  mutable uint64_t stop_fuzz_state = 0x9E3779B97F4A7C15ull;
  bool factor_composite = false;
  uint64_t *tick_sink = nullptr;
  bool inprocessing = false;
  // Non-owning fast path into policy-owned H.  Kissat updates saved phases at
  // assignment time; keeping the vector hook here lets the inline assignment
  // primitive do the same without exposing the rest of Heuristics to rules.
  // ScopedHeuristics/the frontend install it together with `h`.
  std::vector<int8_t> *saved_phases = nullptr;
  bool phase_saving_suppressed = false; // true only for probing assignments
  // Rule-produced, policy-consumed structural assignments.  These are H-only
  // advice: failed model searches never alter F, the trail, or proof state.
  std::array<strategy::LocalSearchHint, strategy::LOCAL_SEARCH_KIND_COUNT>
      local_search_hints;
  // Exact attribution for the most recent successful Decide boundary. Zero
  // means that decision did not consume a structural target; cumulative use
  // counts remain in Stats.
  uint64_t local_search_last_target_decision_candidate = 0;
  bool local_search_last_target_decision_matched = false;
  // Publication identity is solver state, independent of resettable telemetry.
  uint64_t local_search_publication = 0;
  uint64_t walk_seed_candidate = 0;
  uint64_t walk_preparation_budget = 0;
  uint32_t walk_objective = 0, walk_admission = 0;
  // Proof-rule declarations are staged until registry dispatch advances the
  // action epoch. Only current post-action candidates are observable/applicable.
  std::array<strategy::SemanticVariableCandidate,
             strategy::SEMANTIC_VARIABLE_PRODUCER_COUNT>
      semantic_variable_candidates;
  strategy::SemanticVariableCandidate staged_semantic_variable_candidate;
  bool semantic_variable_candidate_pending = false;
  uint64_t semantic_variable_publication = 0;
  uint64_t work_budget = 0;
  // Work already charged before a rule call against a cumulative dose.
  // Vivify uses this to pose Kissat's successive absolute tier ceilings.
  uint64_t work_budget_offset = 0;
  bool work_budget_bounded = false;
  uint8_t vivify_scope = 0;
  bool vivify_reuse = true;
  bool vivify_flush = false;
  uint32_t vivify_irredundant_sort_ratio =
      strategy::VivifyParameters::IRREDUNDANT_SORT_RATIO_DEFAULT;
  std::vector<Var> sweep_tail; // stable external identities
  uint32_t sweep_tail_next = 0;
  // Kissat's `schedule_inner` moves a proved representative to the pop end
  // of a linked schedule in O(1). The same order here: representatives go on
  // this LIFO front, consumed before the tail; a variable's earlier live
  // entry is tombstoned (INVALID_VAR) through `sweep_slot`, which names the
  // location (tail index, or front index | SWEEP_FRONT_BIT) plus one of its
  // live entry and is cleared when that entry is consumed.
  std::vector<Var> sweep_front;
  std::vector<uint32_t> sweep_slot; // by external var; 0 = no live entry
  uint32_t sweep_tail_indexed = 0;  // tail entries whose slot is recorded
  uint32_t sweep_live = 0;          // live unconsumed entries, front + tail
  std::vector<uint8_t> sweep_marked; // external vars still in the current pass
  bool sweep_incomplete = false;    // independent of a nonempty saved schedule
  uint32_t sweep_passes_completed = 0;
  // Policy choice: shipped frontends enable Kissat's
  // one_literal_on_conflict_level path; bare white-box kernels remain neutral.
  bool conflict_reuse = false;
  // Chronological gap used by the conflict-reuse shortcut.  Bare rule tests
  // default to non-chronological; DefaultPolicy poses its configured limit.
  uint32_t conflict_reuse_chrono_levels = UINT32_MAX;
  bool jump_reasons = true;
  bool jump_reasons_bigbig = false;
  uint32_t bump_reason_limit = 0;
  uint32_t bump_reason_depth = 1;
  uint32_t eager_subsume = EAGER_SUBSUME_SLOTS;
  uint32_t walk_even_break_base_milli =
      strategy::WalkParameters::EVEN_BREAK_BASE_DEFAULT;
  std::vector<strategy::WalkParameters::ScorePoint> walk_score_curve =
      strategy::WalkParameters::reference_score_curve();
  uint32_t reorder_maximum_clause_size =
      strategy::ReorderParameters::MAXIMUM_CLAUSE_SIZE_DEFAULT;
  uint32_t reorder_clause_decay_milli =
      strategy::ReorderParameters::CLAUSE_DECAY_DEFAULT;
  uint32_t reorder_binary_weight_milli =
      strategy::ReorderParameters::BINARY_WEIGHT_DEFAULT;
  uint32_t reorder_balanced_polarity_milli =
      strategy::ReorderParameters::BALANCED_POLARITY_DEFAULT;
  uint32_t parity_model_maximum_variables =
      strategy::ParityModelParameters::MAXIMUM_VARIABLES_DEFAULT;
  uint32_t parity_model_maximum_groups =
      strategy::ParityModelParameters::MAXIMUM_GROUPS_DEFAULT;
  uint32_t parity_model_maximum_constraints =
      strategy::ParityModelParameters::MAXIMUM_CONSTRAINTS_DEFAULT;
  uint32_t parity_model_minimum_group_clauses =
      strategy::ParityModelParameters::MINIMUM_GROUP_CLAUSES_DEFAULT;
  uint32_t parity_model_minimum_equations =
      strategy::ParityModelParameters::MINIMUM_EQUATIONS_DEFAULT;
  uint32_t parity_model_maximum_free_variables =
      strategy::ParityModelParameters::MAXIMUM_FREE_VARIABLES_DEFAULT;
  uint64_t parity_model_candidate_budget =
      strategy::ParityModelParameters::CANDIDATE_BUDGET_DEFAULT;
  uint32_t parity_refutation_maximum_variables =
      strategy::ParityModelParameters::REFUTATION_MAXIMUM_VARIABLES_DEFAULT;
  uint32_t parity_refutation_maximum_groups =
      strategy::ParityModelParameters::REFUTATION_MAXIMUM_GROUPS_DEFAULT;
  uint32_t parity_refutation_maximum_constraints =
      strategy::ParityModelParameters::REFUTATION_MAXIMUM_CONSTRAINTS_DEFAULT;
  uint32_t parity_refutation_maximum_arity =
      strategy::ParityModelParameters::REFUTATION_MAXIMUM_ARITY_DEFAULT;
  uint32_t parity_refutation_probe_constraints =
      strategy::ParityModelParameters::REFUTATION_PROBE_CONSTRAINTS_DEFAULT;
  uint32_t parity_refutation_minimum_probe_repeats =
      strategy::ParityModelParameters::REFUTATION_MINIMUM_PROBE_REPEATS_DEFAULT;
  uint64_t parity_refutation_maximum_proof_additions =
      strategy::ParityModelParameters::REFUTATION_MAXIMUM_PROOF_ADDITIONS_DEFAULT;
  strategy::AffineModelParameters affine_model_parameters;
  strategy::BalancedSignatureModelParameters
      balanced_signature_model_parameters;
  strategy::FermatModelParameters fermat_model_parameters;
  strategy::LookupCircuitModelParameters lookup_circuit_model_parameters;
  strategy::SimonModelParameters simon_model_parameters;
  strategy::ExactCoverModelParameters exact_cover_model_parameters;
  strategy::IndependentSetModelParameters independent_set_model_parameters;
  strategy::GraphLabelingParameters graph_labeling_parameters;
  strategy::RamseyStartupParameters ramsey_startup_parameters;
  strategy::StconnProofParameters stconn_proof_parameters;
  strategy::SignedCardinalityProofParameters signed_cardinality_proof_parameters;
  uint32_t onehot_model_maximum_variables =
      strategy::OnehotModelParameters::MAXIMUM_VARIABLES_DEFAULT;
  uint32_t onehot_model_maximum_domains =
      strategy::OnehotModelParameters::MAXIMUM_DOMAINS_DEFAULT;
  uint32_t onehot_model_maximum_constraints =
      strategy::OnehotModelParameters::MAXIMUM_CONSTRAINTS_DEFAULT;
  uint32_t onehot_model_maximum_equation_arity =
      strategy::OnehotModelParameters::MAXIMUM_EQUATION_ARITY_DEFAULT;
  uint32_t onehot_model_minimum_domains =
      strategy::OnehotModelParameters::MINIMUM_DOMAINS_DEFAULT;
  uint32_t onehot_model_minimum_equations =
      strategy::OnehotModelParameters::MINIMUM_EQUATIONS_DEFAULT;
  uint32_t onehot_model_maximum_free_domains =
      strategy::OnehotModelParameters::MAXIMUM_FREE_DOMAINS_DEFAULT;
  uint64_t onehot_model_candidate_budget =
      strategy::OnehotModelParameters::CANDIDATE_BUDGET_DEFAULT;
  uint32_t exact_one_refutation_maximum_variables =
      strategy::OnehotModelParameters::REFUTATION_MAXIMUM_VARIABLES_DEFAULT;
  uint32_t exact_one_refutation_maximum_groups =
      strategy::OnehotModelParameters::REFUTATION_MAXIMUM_GROUPS_DEFAULT;
  uint32_t exact_one_refutation_maximum_constraints =
      strategy::OnehotModelParameters::REFUTATION_MAXIMUM_CONSTRAINTS_DEFAULT;
  uint32_t exact_one_refutation_maximum_arity =
      strategy::OnehotModelParameters::REFUTATION_MAXIMUM_ARITY_DEFAULT;
  uint64_t exact_one_refutation_maximum_proof_additions =
      strategy::OnehotModelParameters::REFUTATION_MAXIMUM_PROOF_ADDITIONS_DEFAULT;
  // DefaultPolicy retains Kissat's separate focused/stable tier snapshots.
  // `tier1/tier2` is the pair posed for the rule currently being applied.
  uint32_t tier1_by_mode[2] = {2, 2};
  uint32_t tier2_by_mode[2] = {6, 6};
  uint32_t tier1 = 2, tier2 = 6;
  uint32_t reduce_victim_numerator =
      strategy::ReduceParameters::VICTIM_NUMERATOR_DEFAULT;
  uint32_t reduce_victim_denominator =
      strategy::ReduceParameters::VICTIM_DENOMINATOR_DEFAULT;
  bool reduce_protect_tiers = true;
  bool reduce_compact_collection = false;
  bool reduce_bookkeeping = true;
  bool shrink_glue_promote = true;
  bool tumble = true;

  // Fixed identities no longer occupying a hot internal slot. These remain
  // distinct external facts, not aliases of the retained root representative.
  // Only representation compaction fills these arrays; inference still owns
  // every root assignment and its proof unit.
  std::vector<int8_t> fixed_external_values;
  std::vector<uint64_t> fixed_external_units;
  uint32_t fixed_external_count = 0;

  // Strategy-supplied canonical discovery parameters. Kept at the cold tail
  // so adding this configuration does not disturb hot Kernel field offsets.
  // Explicit C1/C2 instances bypass these heuristic limits.
  strategy::SubsumeForwardParameters subsume_forward_parameters;
  strategy::StrengthenParameters strengthen_parameters;
  strategy::AnalyzeParameters analyze_parameters;
  strategy::CongruenceParameters congruence_parameters;
  strategy::ParityConsequenceParameters parity_consequence_parameters;
  strategy::AffineConsequenceParameters affine_consequence_parameters;
  strategy::AffineMatrixInitializeParameters affine_matrix_initialize_parameters;
  bool affine_matrix_initialize_parameters_active = false;
  strategy::ExactCoverExtensionParameters exact_cover_extension_parameters;
  strategy::IndependentSetCoverParameters independent_set_cover_parameters;
  strategy::Gf3AffineConsequenceParameters gf3_affine_consequence_parameters;
  strategy::DirectCspSupportParameters direct_csp_support_parameters;
  strategy::NaryCspEqualityParameters nary_csp_equality_parameters;
  strategy::NaryCspEqualityTransitivityParameters nary_csp_equality_transitivity_parameters;
  strategy::NaryCspEqualityAmoParameters nary_csp_equality_amo_parameters;
  strategy::DirectPhpRefutationParameters direct_php_refutation_parameters;
  strategy::Gf3GaussianConsequenceParameters
      gf3_gaussian_consequence_parameters;
  strategy::Gf3GeneralRowConsequenceParameters
      gf3_general_row_consequence_parameters;
  strategy::TseitinCycleBreakingParameters tseitin_cycle_breaking_parameters;
  strategy::SemanticVariableAdviceParameters semantic_variable_advice_parameters;
  // Borrowed session stream. The owner keeps it open through all structural
  // actions; sequence IDs never reset on rebind or proof-mode changes.
  FILE *structural_provenance = nullptr;
  uint64_t structural_action_sequence = 0;
  // Non-owning pointers to solver/controller-owned feature caches. They are
  // never serialized and may outlive online reconfiguration, but not Kernel.
  AffineBasisActionSession *affine_basis_action_session = nullptr;
  ExactCoverExtensionActionSession *exact_cover_extension_action_session =
      nullptr;
  IndependentSetCoverActionSession *independent_set_cover_action_session =
      nullptr;
  Gf3AffineActionSession *gf3_affine_action_session = nullptr;
  DirectCspSupportActionSession *direct_csp_support_action_session = nullptr;
  DirectCspGuardedCoverActionSession *direct_csp_guarded_cover_action_session = nullptr;
  NaryCspRelationOwnerSession *nary_csp_relation_owner_session = nullptr;
  NaryCspEqualitySession *nary_csp_equality_session = nullptr;
  NaryCspEqualityTransitivitySession *nary_csp_equality_transitivity_session = nullptr;
  NaryCspEqualityAmoSession *nary_csp_equality_amo_session = nullptr;
  DirectPhpActionSession *direct_php_action_session = nullptr;
  Gf3GaussianActionSession *gf3_gaussian_action_session = nullptr;
  Gf3GeneralRowActionSession *gf3_general_row_action_session = nullptr;
  TseitinCycleBreakingSession *tseitin_cycle_breaking_session = nullptr;
  AffineMatrixSearchSession *affine_matrix_search_session = nullptr;
  // One retained graph trajectory shared by public and Default actions.
  GraphLabelingModelSession graph_labeling_session;
  uint64_t affine_trail_retraction_epoch = 1;
  uint64_t affine_semantic_namespace_epoch = 1;
  // Direct mutation fixtures must invalidate the affected epoch or opt out.
  bool affine_capture_epochs_trusted = true;
  strategy::InstantiateParameters instantiate_parameters;
  strategy::BackboneParameters backbone_parameters;
  SymmetryUse symuse; // observation only; see SymmetryUse

#ifdef TACTIX_ENABLE_EVENT_TRACE
  // Keep diagnostic-only storage at the final object boundary. The dedicated
  // observer test intentionally links a trace-compiled test object against the
  // non-trace production library; a conditional field inside Stats shifted
  // every later Kernel member and violated that ABI prefix. Production retains
  // neither this storage nor its updates.
  uint64_t diagnostic_search_propagation_ticks = 0;
#endif

  int8_t value(Lit l) const { return values[l]; }
  uint32_t var_level(Var v) const { return assigned[v].level; }
};

// SymmetryUse bookkeeping (observation only). `tag` publishes a just-added
// clause/unit under the current run tag; the note_* hooks are reached only
// behind `k.symuse_active` and filter by literal mark or id range first.
void kernel_symuse_tag(Kernel &, const Lit *lits, uint32_t size, uint64_t id,
                       bool helper);
// Tag persistent Gaussian output under affine run `epoch` (AFFINE class).
void kernel_symuse_tag_affine(Kernel &, const Lit *lits, uint32_t size,
                              uint64_t id, uint64_t epoch);
// Search use of affine run `epoch`'s output: root-forced assignments,
// analysis antecedents and above-root propagations.
uint64_t kernel_symuse_affine_uses(const Kernel &, uint64_t epoch);
// Attribute the assignments trail[from..] by their recorded reasons.
[[gnu::noinline]] void kernel_symuse_note_trail(Kernel &, size_t from);
[[gnu::noinline]] void kernel_symuse_note_binary_use(Kernel &, Lit a, Lit b);
[[gnu::noinline]] void kernel_symuse_note_large_use(Kernel &, uint64_t id);
[[gnu::noinline, gnu::cold]] void kernel_symuse_note_delete(Kernel &,
                                                            uint64_t id);
void kernel_symuse_rebuild_filters(Kernel &);
// Tagged clauses of `run` still present at the end: units, binaries found in
// the watch lists, large clauses never retired.
uint64_t kernel_symuse_alive(const Kernel &, uint32_t run);

inline void kernel_affine_note_trail_retraction(Kernel &k) {
  if (k.affine_trail_retraction_epoch == UINT64_MAX)
    k.affine_capture_epochs_trusted = false;
  else
    ++k.affine_trail_retraction_epoch;
}

inline void kernel_affine_note_namespace_change(Kernel &k) {
  if (k.affine_semantic_namespace_epoch == UINT64_MAX)
    k.affine_capture_epochs_trusted = false;
  else
    ++k.affine_semantic_namespace_epoch;
}

inline void kernel_mark_subsume_material(Kernel &k, Var v) {
  assert(v < k.nvars);
  if (k.cold_state_deferred)
    return; // activation already counted the implicitly armed variable
  assert(k.subsume_material.size() == k.nvars);
  if (!k.subsume_material[v]) {
    k.subsume_material[v] = 1;
    assert(k.subsume_material_count != UINT64_MAX);
    ++k.subsume_material_count;
  }
}
inline bool kernel_has_subsume_material(const Kernel &k, Var v) {
  assert(v < k.nvars && !k.cold_state_deferred);
  return k.subsume_material[v] != 0;
}
inline void kernel_rearm_subsume_material(Kernel &k, Var v) {
  assert(v < k.nvars && !k.cold_state_deferred);
  k.subsume_material[v] = 1; // consumer retry, not a new producer event
}
inline void kernel_clear_subsume_material(Kernel &k) {
  assert(!k.cold_state_deferred);
  for (uint8_t &flag : k.subsume_material)
    flag = 0;
}

inline void kernel_mark_factor_literal(Kernel &k, Lit lit,
                                       const char *source = "unspecified") {
  assert(lit < k.factor_marks.size());
  if (!k.factor_marks[lit]) {
    k.factor_marks[lit] = 1;
    k.factor_literals_marked++;
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_factor_mark_transition(k, lit, true, source);
#else
    (void) source;
#endif
  }
}

void kernel_reserve_vars(Kernel &, uint32_t nvars);
void kernel_defer_cold_state(Kernel &);
void kernel_materialize_cold_state(Kernel &);
void kernel_record_import(Kernel &, Var);
// Defined inline: these are single array lookups queried in the hot
// substitution/congruence/factor sort comparators (0-conflict profile r10:
// ~2% self each as an un-inlined cross-TU call; report 3.85). Inlining folds
// the lookup into the comparator and removes the call frame; the computation
// is byte-identical.
inline uint64_t kernel_import_var_order(const Kernel &k, Var v) {
  assert(v < k.nvars);
  assert(k.internal_import_rank.size() == k.nvars);
  const uint32_t direct = k.internal_import_rank[v];
  if (direct != UINT32_MAX)
    return direct;
  const Var external = k.internal_to_external[v];
  assert(external >= k.external_import_rank.size() ||
         k.external_import_rank[external] == UINT32_MAX);
  // Direct kernel clients need no input finalization. Put their unrecorded
  // variables after recorded imports in stable external order.
  return static_cast<uint64_t>(k.next_import_rank) + external;
}

inline uint64_t kernel_import_lit_order(const Kernel &k, Lit lit) {
  return 2 * kernel_import_var_order(k, var_of(lit)) + negated(lit);
}
uint64_t kernel_compact_import_lit_order(const Kernel &, Lit);
void kernel_vars_in_import_order(const Kernel &, std::vector<Var> &);
void kernel_vars_in_external_order(const Kernel &, std::vector<Var> &);
void kernel_update_target_phases(Kernel &);
void kernel_offer_local_search_hint(Kernel &, strategy::LocalSearchKind,
                                    const std::vector<uint8_t> &assignment,
                                    uint64_t objective = UINT64_MAX,
                                    bool objective_exact = false,
                                    strategy::LocalSearchEffort effort = {});
// Signed internal-variable assignment: zero leaves that variable unspecified.
void kernel_offer_local_search_partial_hint(
    Kernel &, strategy::LocalSearchKind, const std::vector<int8_t> &assignment,
    uint64_t objective = UINT64_MAX, bool objective_exact = false,
    strategy::LocalSearchEffort effort = {});
const strategy::LocalSearchHint *kernel_local_search_hint(const Kernel &,
                                                         uint64_t candidate);

// Reserve permanent external IDs for proof-only helpers, without creating
// internal/search variables. Use after input import and before proof emission,
// identically with/without a proof writer. first is a zero-based external ID.
// No namespace rollback/reuse; deleted proof units can remain checker-visible.
bool kernel_reserve_proof_variables(Kernel &, uint32_t count, Var &first);
Var kernel_new_extension_vars(Kernel &, uint32_t count);

// Re-arm ordinary watched-clause propagation after an authenticated theory
// clause is installed above root over assignments already scanned. The caller
// must have installed a non-unit clause which is currently unit or conflicting
// and must not call this for learned conflict-analysis clauses.
void kernel_rearm_theory_clause(Kernel &, const Lit *, uint32_t size);

inline uint32_t kernel_num_output_vars(const Kernel &k) {
  return k.original_external_vars == UINT32_MAX
             ? static_cast<uint32_t>(k.external_to_internal.size())
             : k.original_external_vars;
}

inline uint32_t kernel_num_original_internal_vars(const Kernel &k) {
  return k.first_extension_var == UINT32_MAX ? k.nvars : k.first_extension_var;
}

inline Lit kernel_external_lit(const Kernel &k, Lit l) {
  assert(var_of(l) < k.internal_to_external.size());
  return make_lit(k.internal_to_external[var_of(l)], negated(l));
}

inline int kernel_externalize(const Kernel &k, Lit l) {
  return externalize(kernel_external_lit(k, l));
}

inline Lit kernel_internalize(const Kernel &k, int elit) {
  const Lit external = internalize(elit);
  const Var ev = var_of(external);
  if (ev >= k.external_to_internal.size())
    return INVALID_LIT;
  const Var iv = k.external_to_internal[ev];
  return iv == INVALID_VAR ? INVALID_LIT : make_lit(iv, negated(external));
}

inline int8_t kernel_external_value(const Kernel &k, Lit external) {
  const Var ev = var_of(external);
  if (ev < k.fixed_external_values.size() && k.fixed_external_values[ev])
    return negated(external) ? -k.fixed_external_values[ev]
                            : k.fixed_external_values[ev];
  if (ev >= k.external_to_internal.size())
    return 0;
  const Var iv = k.external_to_internal[ev];
  return iv == INVALID_VAR ? 0 : k.values[make_lit(iv, negated(external))];
}

inline uint64_t kernel_external_unit_id(const Kernel &k, Var external) {
  if (external < k.fixed_external_values.size() && k.fixed_external_values[external])
    return k.fixed_external_units[external];
  if (external >= k.external_to_internal.size())
    return 0;
  const Var iv = k.external_to_internal[external];
  return iv == INVALID_VAR ? 0 : k.unit_ids[iv];
}

void kernel_push_witness(Kernel &, Lit witness, const Lit *rest, uint32_t nrest);
// A conditional-autarky segment stores the removed clause and every literal
// to set true if that clause is false during reverse model reconstruction.
// The rule validates the cube before publishing this proof-neutral record.
// The clause uses current internal literals; the cube uses stable external
// literals so compaction never removes a legal witness variable.
void kernel_push_cube_witness(Kernel &, const Lit *clause, uint32_t nclause,
                              const Lit *witness, uint32_t nwitness);
// Reconstruct a total external model.  Terminal solver/output paths may also
// mirror it into internal values for legacy white-box consumers.  Resumable
// online paths pass false so model export cannot mutate solver state.
void kernel_extend_model(Kernel &, bool materialize_internal = true);

// Policy-free support primitive: densely renumber variables whose formula
// retirement is already complete.  External proof/API/model identities do not
// change.  Returns false when there is nothing to remove or a retirement
// invariant is not yet strong enough to compact safely.
// fold_fixed additionally archives unreferenced, proved fixed identities while
// retaining one representative in the existing compact/import image. A fixed
// variable still named by a live clause or non-root reason keeps its hot slot.
bool kernel_compact_variables(Kernel &, bool fold_fixed = false);
bool kernel_should_compact_variables(const Kernel &, uint32_t percent);

void kernel_emit_empty_from_pair(Kernel &, uint64_t id_a, uint64_t id_b);

struct PendingUnits {
  std::vector<Lit> units;    // first-recorded order (assert after the round)
  // The kernel's literal mark bytes are all-zero between conflict analyses.
  // Inprocessing runs at the root, so a pending batch borrows those bytes and
  // restores every one it touches. Only native LRAT needs the dense proof-id
  // side table; DRAT and proof-less batches allocate no per-variable storage.
  Kernel *owner = nullptr;
  std::vector<uint64_t> ids; // by var in LRAT mode; empty otherwise
  enum class Outcome { Recorded, Duplicate, Refuted };
  PendingUnits() = default;
  PendingUnits(const PendingUnits &) = delete;
  PendingUnits &operator=(const PendingUnits &) = delete;
  ~PendingUnits();
  void init(Kernel &);
  Outcome record(Kernel &, Lit u, uint64_t id);
  // A recorded unit is not yet the root value: the batch is asserted later and
  // anything in between — a propagation inside the same round, another
  // rule — can make the OPPOSITE literal root-true first. So the recorded id
  // stays here and `Kernel::unit_ids` is installed only when the unit is
  // actually asserted; `unit_ids[v]` must always name the clause for the
  // literal that is true, because every later chain citing it assumes so.
  void commit(Kernel &, Lit u) const;
  // The assert-time opposite case: {u} was recorded, ¬u is already root-true,
  // so the two resolve to the empty clause. Refuting without emitting it
  // leaves the proof incomplete on a run that answers UNSATISFIABLE.
  void refute(Kernel &, Lit u) const;
};

enum class ReplaceOutcome { Refuted, Unit, Binary, Large };
ReplaceOutcome kernel_replace_clause(Kernel &, Clause *orig, uint64_t id,
                                     const Lit *lits, uint32_t size,
                                     bool redundant, uint32_t glue,
                                     Ref *large_ref, // nullable
                                     bool mark_added_material = true);

// Representation-only large-clause replacement.  The caller has already
// proved and emitted the replacement addition; this transition emits the old
// proof deletion, preserves the arena Ref and allocation span, and updates the
// kernel's watches/derived indexes. `mark_added_material` distinguishes
// consumers whose reference algorithm publishes the surviving image as new
// subsumption/Factor material from canonical Substitute, which does not.
// Both source and image are large and the image may not grow.
// `retained_watch` (kissat strengthen.c large_on_the_fly_strengthen): that
// literal is `lits[0]` and keeps its watch entry, the other old watch is
// dropped, one watch for `lits[1]` is appended, no removed literal is marked.
void kernel_replace_large_clause_in_place(Kernel &, Ref source, uint64_t id,
                                          const Lit *lits, uint32_t size,
                                          uint32_t glue,
                                          const char *source_tag = "unspecified",
                                          bool mark_added_material = true,
                                          Lit retained_watch = INVALID_LIT);

// Proof-safe root-unit consumption shared by sparse collection and Vivify
// scheduling. Sparse collection rewrites physical material without marking it
// added; Kissat's Vivify simplifier instead calls new_binary_clause for a
// size-two terminal, while still shortening larger images in place unmarked.
// The clause form does not collect the arena; callers can continue scanning by
// the original clause width. The whole-arena form collects once at its exit.
enum class RootFlushOutcome { None, Deleted, Rewritten, Binary, Unit, Refuted };
// `clamp_glue` selects the reference glue semantics for the shrink: kissat's
// generic root flush (collect.c) leaves glue untouched, while Vivify's
// schedule-time root simplification clamps to size-1 (vivify.c).
// A sparse collection retires satisfied large records without removal advice;
// semantic simplifiers such as Vivify retain the default removal notification.
RootFlushOutcome kernel_flush_root_clause(Kernel &, Ref *ref,
                                          uint64_t *changed = nullptr,
                                          bool mark_binary_added_material = false,
                                          bool clamp_glue = false,
                                          bool mark_satisfied_removal = true);
enum class RootCollectOutcome { NoChange, Changed, Unit, Refuted };
RootCollectOutcome kernel_sparse_collect_root(Kernel &, Ref start = 0);

inline void kernel_mark_bve_retry(Kernel &k, Var v,
                                  const char *source = "unspecified",
                                  const Clause *clause = nullptr) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
  const bool before = v < k.bve_retry.size() && k.bve_retry[v];
#endif
  if (!k.cold_state_deferred)
    k.bve_retry[v] = 1;
  k.stats.bve_marks++; // events, not bit flips — see Stats::bve_marks
#ifdef TACTIX_ENABLE_EVENT_TRACE
  const bool after = v < k.bve_retry.size() && k.bve_retry[v];
  event_trace_bve_retry_mark(k, v, source, before, after, clause);
#else
  (void)source;
  (void)clause;
#endif
}
inline void kernel_notify_bve_score(Kernel &k, Var v, bool reinsert) {
  if (k.bve_score_update_hook)
    k.bve_score_update_hook(k, k.bve_score_update_state, v, reinsert);
}
inline void kernel_bve_occurrence_add(Kernel &k, Lit lit, uint32_t count = 1) {
  if (!k.bve_occurrence_count_image)
    return;
  assert(k.dense && lit < k.bve_occurrence_count_image->size());
  uint32_t &current = (*k.bve_occurrence_count_image)[lit];
  assert(UINT32_MAX - current >= count);
  current += count;
}
inline void kernel_bve_occurrence_remove(Kernel &k, Lit lit,
                                         uint32_t count = 1) {
  if (!k.bve_occurrence_count_image)
    return;
  assert(k.dense && lit < k.bve_occurrence_count_image->size());
  uint32_t &current = (*k.bve_occurrence_count_image)[lit];
  assert(current >= count);
  current -= count;
}
inline void kernel_touch_var(Kernel &k, Var v) {
  const uint64_t epoch = ++k.touch_epoch;
  if (!k.cold_state_deferred)
    k.touched[v] = epoch;
  k.stats.touch_marks++; // events; a stamp has no flip to count
}
inline void kernel_removed_var(Kernel &k, Var v) {
  const uint64_t epoch = ++k.touch_epoch;
  if (!k.cold_state_deferred)
    k.removed[v] = epoch;
  k.stats.touch_marks++;
}
// Monotone observation of the formula and the root assignment: every clause
// addition and semantic removal stamps `touch_epoch`, every root assignment
// counts in `root_assignments`.  Equal marks bracket an application which
// changed neither; representation work (list compaction, collection) is
// deliberately invisible here.
struct KernelChangeMark {
  uint64_t touch_epoch = 0;
  uint64_t root_assignments = 0;
  bool operator==(const KernelChangeMark &) const = default;
};
inline KernelChangeMark kernel_change_mark(const Kernel &k) {
  return {k.touch_epoch, k.stats.root_assignments};
}
inline bool kernel_touched_since(const Kernel &k, Var v, uint64_t since) {
  assert(!k.cold_state_deferred);
  return k.touched[v] > since;
}
inline void kernel_touch_kept_var(Kernel &k, Var v) {
  const uint64_t epoch = ++k.touch_epoch;
  if (!k.cold_state_deferred)
    k.touched_kept[v] = epoch;
  k.stats.touch_kept_marks++;
}
inline bool kernel_kept_since(const Kernel &k, Var v, uint64_t since) {
  assert(!k.cold_state_deferred);
  return k.touched_kept[v] > since;
}
inline bool kernel_likely_kept(const Kernel &k, bool redundant, uint32_t glue,
                               uint32_t size) {
  return !redundant || glue <= k.tier2 ||
         (k.kept_glue && glue <= k.kept_glue && size <= k.kept_size);
}
inline bool kernel_changed_since(const Kernel &k, Var v, uint64_t since) {
  assert(!k.cold_state_deferred);
  return k.touched[v] > since || k.removed[v] > since;
}
inline bool kernel_clause_new_material(const Kernel &k, const Clause *c,
                                       uint64_t since, size_t need = 2,
                                       bool kept = false) {
  size_t hits = 0;
  for (const Lit l : *c) {
    if (!kernel_touched_since(k, var_of(l), since) &&
        !(kept && kernel_kept_since(k, var_of(l), since)))
      continue;
    if (k.value(l) < 0)
      return true; // (b)
    if (++hits >= need)
      return true; // (a)
  }
  return false;
}
void kernel_touch_sync_fixed(Kernel &);

Ref kernel_add_large_clause(Kernel &, const Lit *lits, uint32_t size,
                            bool redundant, uint32_t glue, uint64_t id = 0,
                            bool mark_added_material = true);
uint64_t kernel_clause_position(const Kernel &, Ref);
// Restore current Kissat arena order after append-only replacements. The
// implementation caches each position and uses a linear stable radix pass.
void kernel_sort_refs_by_logical_clause_position(const Kernel &,
                                                 std::vector<Ref> &);
void kernel_inherit_clause_position(Kernel &, uint64_t replacement_id,
                                    Ref source_ref);
void kernel_add_binary_clause(Kernel &, Lit a, Lit b, uint64_t id = 0,
                              bool mark_added_material = true);
// A root-only transformation that changes an existing binary clause in place
// must re-arm propagation for its new literals just like clause insertion.
// This is separate from publication because the stable watch direction may be
// rewritten before the moved mirror is published (or without publishing a new
// logical clause at all).
void kernel_rearm_binary_clause(Kernel &, Lit a, Lit b);
// Bulk input has already established that this pair is absent.  Publish the
// same clause transition without rescanning a growing high-degree watch list.
// Unlike the general API, duplicate callers violate this function's contract.
void kernel_add_new_binary_clause(Kernel &, Lit a, Lit b, uint64_t id = 0);
enum class BinaryPopulationEffect : uint8_t {
  RepresentationOnly,
  LogicalAdd,
};
// Complete a pair whose `stable` direction was rewritten in place: append only
// the missing mirror (to `moved`'s list), then publish the normal add metadata.
void kernel_add_binary_mirror(Kernel &, Lit stable, Lit moved,
                              uint64_t id = 0);
// Publish the ordinary add metadata for a binary direction already inserted
// by a transaction that reconstructs the two directed watches independently.
void kernel_publish_binary_clause(Kernel &, Lit a, Lit b, uint64_t id,
                                  bool new_pair, BinaryPopulationEffect);
bool kernel_delete_binary(Kernel &, Lit a, Lit b);
// Remove exactly one physical copy. If another identical copy survives, its
// proof clause remains a valid reason. Native DSR deletes one literal-named
// copy; DRAT/LRAT omit this optional deletion to preserve the retained LRAT id.
// The final copy delegates to the ordinary named-pair deletion.
bool kernel_delete_binary_one_copy(Kernel &, Lit a, Lit b);
// Dense BVE is already compacting `pivot`'s connected stack while it removes
// this satisfied occurrence.  Kissat updates/reinserts only the partner's
// elimination score at that point, not the currently popped pivot.
bool kernel_delete_binary_one_copy_bve_scan(Kernel &, Lit pivot, Lit other);
bool kernel_delete_binary_all_copies(Kernel &, Lit a, Lit b);
// Forward.c duplicate sweep: retain the first physical copy in each mirrored
// watch list and retire every later copy. Returns the number retired.
uint32_t kernel_remove_duplicate_binary_copies(Kernel &, Lit a, Lit b);
// Factor's null-stack dense entry has already counted and deduplicated the
// selected grid pair.  Consume that exact count without rescanning its watch
// list before the Kissat-style eager removal.
bool kernel_delete_binary_all_copies_known(Kernel &, Lit a, Lit b,
                                           uint32_t copies);

inline uint64_t kernel_binary_key(Lit a, Lit b);

inline uint32_t kernel_binary_copies(const Kernel &k, Lit a, Lit b,
                                     uint32_t cap = UINT32_MAX) {
  uint32_t n = 0;
  const bool stop_at_large = k.watches.heads[a].binaries_first();
  for (const WatchEntry w : k.watches.entries(a)) {
    if (!w.binary()) {
      if (stop_at_large)
        break;
      continue;
    }
    if (w.other() == b && ++n == cap)
      break;
  }
  return n;
}
inline bool kernel_binary_live(const Kernel &k, Lit a, Lit b) {
  if (k.binary_liveness) {
    const bool maybe = k.binary_liveness->maybe(kernel_binary_key(a, b));
    if (!maybe || k.binary_liveness->exact())
      return maybe;
  }
  // Mirror symmetry makes either endpoint authoritative. Scan the smaller
  // exact population after the prefilter admits a possible hit.
  if (k.watches.binary_size(b) < k.watches.binary_size(a))
    std::swap(a, b);
  return kernel_binary_copies(k, a, b, 1) == 1;
}

// Remove one large entry; a root-fixed slot may already be absent after sparse
// collection. A non-root/unassigned watched literal must still own its entry.
void kernel_unwatch_large(Kernel &, Lit l, Ref ref);

// Exchange two large-clause entries in one watch list without changing their
// positions.  OTFS uses this to give a freshly allocated replacement the old
// clause's surviving-watch position while both clauses remain fully watched.
// The last two arguments are the blockers each Ref must have AFTER the swap.
void kernel_swap_large_watch_refs(Kernel &, Lit l, Ref first, Ref second,
                                  Lit blocking_for_first,
                                  Lit blocking_for_second);

void kernel_promote_watched_lit(Kernel &, Ref ref, uint32_t slot,
                                uint32_t from);

// Publish a propagation-derived root fact before its reason disappears. DSR
// checkers must see this unit while the reason clause is still available for
// RUP; the proof-only polarity table makes repeated publication idempotent.
void kernel_materialize_dsr_root_unit(Kernel &, Lit);
// Record that an asserted root unit is already a clause of the proof stream
// (a rule's own unit line or an input unit) without emitting it. The table
// serves every clausal stream whose checker keeps written units without LRAT
// ids: native DSR and DRAT. LRAT names its units in `unit_ids`.
void kernel_note_dsr_root_unit(Kernel &, Lit);
bool kernel_root_unit_table_stream(const Kernel &);

// Retire any trail reason naming this clause, so a delete never leaves a
// literal whose reason is a dropped clause. In DSR mode the reason is also the
// RUP witness for a permanent unit, so publish that unit before retiring it.
inline void kernel_retire_root_reason(Kernel &k, const Clause *c, Ref ref) {
  for (const Lit lit : *c) {
    Assigned &a = k.assigned[var_of(lit)];
    if (!a.binary && a.reason == ref && k.values[lit] != 0) {
      // In-place OTFS/Strengthen/Vivify rewrites temporarily retire the old
      // proof clause while preserving this arena Ref as a live non-root
      // reason. Only a level-zero reason becomes a permanent unit. Changing a
      // non-root record to UNIT_REASON here drops its implication graph and
      // makes the next conflict analysis read a decision as a reason.
      if (a.level != 0)
        continue;
      kernel_materialize_dsr_root_unit(k, lit);
      a.reason = UNIT_REASON;
    }
  }
}

inline PopulationClauseView kernel_population_clause(
    const Kernel &k, const Lit *literals, uint32_t size) noexcept {
  return PopulationClauseView::internal_lits(literals, size,
                                              &k.internal_to_external);
}

inline void kernel_observe_solver_clause(
    Kernel &k, SolverPopulationEventKind event_kind,
    SolverClauseKind clause_kind, uint64_t logical_id, const Lit *literals,
    uint32_t size, uint32_t copies = 1, bool redundant = false,
    uint64_t transaction_id = 0) noexcept {
  if (!k.population_observer || !k.population_observe_solver)
    return;
  SolverPopulationEvent event;
  event.kind = event_kind;
  event.clause_kind = clause_kind;
  event.logical_id = logical_id;
  event.copies = copies;
  event.redundant = redundant;
  event.transaction_id = transaction_id;
  event.clause = kernel_population_clause(k, literals, size);
  k.population_observer->observe(event);
}

inline uint64_t kernel_begin_solver_population_transaction(
    Kernel &k) noexcept {
  if (!k.population_observer || !k.population_observe_solver)
    return 0;
  const uint64_t transaction_id = ++k.next_population_transaction;
  SolverPopulationEvent event;
  event.kind = SolverPopulationEventKind::TransactionBegin;
  event.transaction_id = transaction_id;
  k.population_observer->observe(event);
  return transaction_id;
}

inline void kernel_commit_solver_population_transaction(
    Kernel &k, uint64_t transaction_id) noexcept {
  if (!transaction_id || !k.population_observer ||
      !k.population_observe_solver)
    return;
  SolverPopulationEvent event;
  event.kind = SolverPopulationEventKind::TransactionCommit;
  event.transaction_id = transaction_id;
  k.population_observer->observe(event);
}

inline void kernel_observe_replacement(
    Kernel &k, SolverReplacementPhase phase,
    SolverReplacementOutcome outcome, uint64_t predecessor_id,
    uint64_t replacement_id, const Lit *predecessor,
    uint32_t predecessor_size, const Lit *replacement,
    uint32_t replacement_size) noexcept {
  if (!k.population_observer || !k.population_observe_solver)
    return;
  SolverReplacementEvent event;
  event.phase = phase;
  event.outcome = outcome;
  event.predecessor_id = predecessor_id;
  event.replacement_id = replacement_id;
  event.predecessor =
      kernel_population_clause(k, predecessor, predecessor_size);
  event.replacement =
      kernel_population_clause(k, replacement, replacement_size);
  k.population_observer->observe(event);
}

inline void kernel_delete_clause(Kernel &k, Clause *c,
                                 bool semantic_removal = true,
                                 const char *retry_source = "delete-clause") {
  // The proof delta belongs INSIDE the retirement transition, not in front of
  // it. A deletion names one clause: DRAT by its literals, LRAT by its id, and
  // in both formats the checker retires exactly one copy per `d` line. Emitting
  // one for a record that is already garbage therefore does not "repeat" a
  // no-op — it retires a SECOND clause. Under DRAT that is whatever live clause
  // carries the same literals (a duplicate the solver keeps), leaving the
  // solver with a clause the checker no longer holds, so the next lemma that
  // resolves through it is not RUP. Under LRAT it retires one id twice, which
  // every checker rejects outright. The mark and the bookkeeping below were
  // already idempotent; this makes the whole retirement idempotent.
  if (c->garbage())
    return; // already retired
  if (k.symuse_active) [[unlikely]]
    kernel_symuse_note_delete(k, c->id());
  const Ref ref = static_cast<Ref>(reinterpret_cast<const uint32_t *>(c) -
                                   k.arena.data());
  kernel_retire_root_reason(k, c, ref);
  if (k.proof)
    k.proof->del(k.next_clause_id, c->id(), c->lits, c->size);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_note_clause_transition(k, "delete", c->lits, c->size,
                                      c->id());
#endif
  if (!c->redundant()) {
    assert(k.live_irredundant_large);
    k.live_irredundant_large--;
  }
  c->flags |= Clause::GARBAGE;
  kernel_observe_solver_clause(k, SolverPopulationEventKind::Delete,
                               SolverClauseKind::Large, c->id(), c->lits,
                               c->size, 1, c->redundant());
  if (semantic_removal && !c->redundant())
    for (const Lit l : *c) {
      kernel_mark_bve_retry(k, var_of(l), retry_source, c);
      kernel_removed_var(k, var_of(l)); // BCE's side of the substrate
      kernel_notify_bve_score(k, var_of(l), true);
    }
}

inline uint32_t kernel_shrunken_glue(Kernel &k, uint32_t old_glue,
                                     uint32_t new_size) {
  if (new_size > 2 && old_glue >= new_size) {
    if (!k.shrink_glue_promote)
      return new_size;
    k.stats.shrink_promoted++;
    return new_size - 1;
  }
  return old_glue < new_size ? old_glue : new_size;
}

inline uint64_t kernel_binary_key(Lit a, Lit b) {
  const Lit lo = a < b ? a : b, hi = a < b ? b : a;
  return (static_cast<uint64_t>(lo) << 32) | hi;
}
inline uint64_t kernel_binary_id(const Kernel &k, Lit a, Lit b) {
  return k.binary_ids.get(kernel_binary_key(a, b));
}

inline bool kernel_var_retired(const Kernel &k, Var v) {
  return v < k.var_status.size() && k.var_status[v] != VarStatus::Active;
}

inline uint64_t kernel_active_variables(const Kernel &k) {
  uint64_t active = 0;
  for (Var v = 0; v < k.nvars; v++) {
    if (kernel_var_retired(k, v))
      continue;
    // Kissat deactivates a variable as soon as it marks the variable fixed.
    // Tactix retains the root assignment and stable identity for proof/model
    // reconstruction, so retirement alone is not its complete `active` image.
    // A non-root search assignment remains active in Kissat and must remain in
    // this count; only a semantic level-zero assignment represents `fixed`.
    if (k.values[make_lit(v, false)] && k.assigned[v].level == 0)
      continue;
    active++;
  }
  return active;
}

inline uint64_t kernel_cache_lines(uint64_t n) { return (n + 31u) >> 5; }

inline void kernel_charge_ticks(Kernel &k, uint64_t n) {
  k.stats.ticks += n;
  // Kissat's ordinary search propagator adds to both `ticks` and
  // `search_ticks` (propsearch.c), while its warm-up propagator deliberately
  // adds only to `ticks` (propbeyond.c:14-24).  Warm-up is search scaffolding
  // for Walk, not work that may consume mode or inprocessing budgets.
  if (!k.inprocessing && !k.warming)
    k.stats.search_ticks += n;
  if (k.tick_sink)
    *k.tick_sink += n;
}

[[gnu::always_inline]] inline void
kernel_assign(Kernel &k, Lit lit, uint32_t level, uint32_t reason, bool binary,
              bool jumped = false) {
  assert(k.values[lit] == 0);
  const Var v = var_of(lit);
  // Kissat inlineassign.h prefetches the watch list that the newly true
  // literal will scan next.  This is pure latency hiding: it neither reads nor
  // mutates semantic state, but omitting it is visible on propagation-heavy
  // instances where the next scan otherwise begins with a cold arena slice.
  const Lit scan = negate(lit);
  if (k.watches.size(scan))
    __builtin_prefetch(k.watches.begin(scan), 0, 1);
  k.values[lit] = 1;
  k.values[negate(lit)] = -1;
  k.assigned[v] = {level, static_cast<uint32_t>(k.trail.size()), reason,
                   binary, jumped};
  k.trail.push_back(lit);
  if (!level) {
    k.stats.root_assignments++;
    assert(k.unflushed_root < UINT32_MAX);
    k.unflushed_root++;
#ifdef TACTIX_ENABLE_RULE_JOURNAL
    rule_journal_note_root_assignment(k, lit, reason, binary);
#endif
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_root_assign(k, lit);
#endif
  }
  // kissat inlineassign.h: every non-probing assignment, including a root
  // implication, refreshes the saved phase immediately.  Unassign-time saving
  // is not equivalent when a schedule changes mode while literals remain on
  // the trail.  Probing callers suppress this hook around speculative work.
  if (k.saved_phases && !k.phase_saving_suppressed && !k.probing.recording)
    (*k.saved_phases)[v] = negated(lit) ? -1 : 1;
#ifdef TACTIX_ENABLE_EVENT_TRACE
  if (k.saved_phases && !k.phase_saving_suppressed && !k.probing.recording)
    event_trace_phase_save(k, lit);
#endif
  assert(k.unassigned > 0);
  k.unassigned--;
  if (!kernel_var_retired(k, v)) {
    assert(k.unassigned_active > 0);
    k.unassigned_active--;
  }
}
inline void kernel_assign_decision(Kernel &k, Lit lit) { // level++ inside
  k.level++;
  k.frames.push_back(static_cast<uint32_t>(k.trail.size()));
  kernel_assign(k, lit, k.level, DECISION_REASON, false);
}
[[gnu::always_inline]] inline void kernel_assign_unit(Kernel &k,
                                                       Lit lit) { // root level
  kernel_assign(k, lit, 0, UNIT_REASON, false);
  // Every asserted root unit is a DSR clause (the asserting rule wrote it, or
  // it is an input unit). Without an LRAT id it is known to proof-visible
  // consumers only through the DSR root table, which must therefore hold it.
  if (k.proof && !k.unit_ids[var_of(lit)])
    kernel_note_dsr_root_unit(k, lit);
  kernel_observe_solver_clause(k, SolverPopulationEventKind::Add,
                               SolverClauseKind::Unit,
                               k.unit_ids[var_of(lit)], &lit, 1);
}
[[gnu::always_inline]] inline void
kernel_assign_binary_from(Kernel &k, Lit lit, Lit other_false,
                          const Assigned &a, bool require_bigbig = true) {
  const uint32_t level = a.level;
  if (k.jump_reasons && (!require_bigbig || k.jump_reasons_bigbig) && level &&
      a.binary) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_binary_assignment(k, lit, other_false, a.reason, true);
#endif
    k.jumped_over[var_of(lit)] = other_false;
    k.stats.jumped_reasons++;
    kernel_assign(k, lit, level, a.reason, true, true);
    return;
  }
#ifdef TACTIX_ENABLE_EVENT_TRACE
  event_trace_binary_assignment(k, lit, other_false, other_false, false);
#endif
  kernel_assign(k, lit, level, other_false, true);
}
[[gnu::always_inline]] inline void
kernel_assign_binary(Kernel &k, Lit lit, Lit other_false,
                     bool require_bigbig = true) {
  const Assigned &a = k.assigned[var_of(other_false)];
  kernel_assign_binary_from(k, lit, other_false, a, require_bigbig);
}
[[gnu::always_inline]] inline void kernel_assign_ref(Kernel &k, Lit lit,
                                                     Ref reason) {
  const Clause *c = clause_at(k.arena, reason);
  const Var v = var_of(lit);
  uint32_t lvl = 0;
  // Every assigned level is at most k.level (kernel_check_invariants).
  // Preserve the true antecedent maximum, including chrono/root implications,
  // but stop once that bound is reached. At root the maximum is already zero.
  // Kissat/CaDiCaL scan the whole reason; this only removes redundant reads.
  if (k.level)
    for (const Lit m : *c)
      if (var_of(m) != v) {
        const uint32_t ml = k.assigned[var_of(m)].level;
        if (ml > lvl) {
          lvl = ml;
          if (lvl == k.level)
            break;
        }
      }
  kernel_assign(k, lit, lvl, reason, false);
}
void kernel_backtrack(Kernel &, uint32_t target_level,
                      bool search_backtrack = true);

// Retract the whole trail suffix from `position` upward and land at the root.
// A speculative probing round is not level-structured — Backbone stamps a
// proved unit at the saved assumption level, and level zero itself when that
// level is zero — so `kernel_backtrack(k, 0)` cannot express the retraction: it
// keeps every literal whose recorded level is already zero. Unwinding by trail
// POSITION does, and routing it through the kernel keeps the per-literal
// unassign contract (both counters plus the policy's ranking structures) in one
// place instead of half-copied into the rule. Retracted assignments lose their
// cached unit ids; a replay must name its own polarity.
void kernel_unwind_speculation(Kernel &, uint32_t position);

// The one propagation-fixpoint predicate. Not `propagate_head == trail.size()`
// on its own: binary-only propagation moves that cursor while skipping every
// large watch and leaves the debt below, which the next full propagation still
// has to discharge. A verdict, a guard or an oracle that reads the raw cursor
// can therefore call a trail complete while a large clause is still unit.
inline bool kernel_propagation_fixpoint(const Kernel &k) {
  return k.propagate_head == k.trail.size() &&
         k.binary_only_head >= k.propagate_head;
}

bool kernel_propagate(Kernel &, Ref ignore = INVALID_REF);

// Walk warm-up propagation (kissat propbeyond.c): scan to a full assignment
// and keep going after conflicting clauses.  Conflicts are counted but never
// installed as search conflicts; non-conflicting implications use the normal
// assignment path and therefore refresh saved phases.
void kernel_propagate_beyond_conflicts(Kernel &);

// `use_current_level` is the chained Backbone contract: Kissat assigns every
// binary implication at the current accumulated-assumption level instead of
// compacting it to its antecedent's true level.
bool kernel_propagate_binary(Kernel &, bool use_current_level = false);

void kernel_probing_begin(Kernel &, bool rollback_pose = true);
void kernel_probing_end(Kernel &); // idempotent; a no-op outside a scope
bool kernel_propagate_probing(Kernel &, Ref ignore = INVALID_REF);

// True while the open scope is accumulating pose snapshots it will restore.
// A `rollback_pose == false` scope records nothing (every snapshot push in the
// propagation engine is under its `RECORD` instantiation) and restores
// nothing, so an unlogged watch edit inside one survives by construction. That
// is the case a scope-holding rule may edit watches in; the rollback case is
// not.
inline bool kernel_probing_rollback_active(const Kernel &k) {
  return k.probing.recording && k.probing.rollback_pose;
}


inline bool kernel_terminating(const Kernel &k) {
  if (k.stop_fuzz) {
    k.stop_fuzz_state = k.stop_fuzz_state * 6364136223846793005ull + 1442695040888963407ull;
    if ((k.stop_fuzz_state >> 33) % k.stop_fuzz == 0) {
      k.stats.stop_fuzz_fires++;
      return true;
    }
  }
  return k.stop && *k.stop;
}

// `work_budget_bounded` distinguishes an explicit zero allowance from the
// legacy zero-as-unlimited handoff. Rules poll only at their documented safe
// boundaries, so a completed atomic unit may overshoot this ceiling.
inline bool kernel_has_work_budget(const Kernel &k) {
  return k.work_budget_bounded || k.work_budget != 0;
}

inline bool kernel_work_budget_exceeded(const Kernel &k, uint64_t spent) {
  return kernel_has_work_budget(k) && spent > k.work_budget;
}

inline bool kernel_rule_interrupt(Kernel &k, uint64_t &site) {
  if (!kernel_terminating(k))
    return false;
  k.stats.rule_interrupted++;
  site++;
  return true;
}

struct KernelStopPublisher {
  Kernel &k;
  volatile sig_atomic_t *saved;
  KernelStopPublisher(Kernel &kk, volatile sig_atomic_t *s)
      : k(kk), saved(kk.stop) {
    k.stop = s;
  }
  ~KernelStopPublisher() { k.stop = saved; }
  KernelStopPublisher(const KernelStopPublisher &) = delete;
  KernelStopPublisher &operator=(const KernelStopPublisher &) = delete;
};

struct KernelAnalysisConflictPublisher {
  Kernel &k;
  AnalysisConflictHook saved_hook;
  void *saved_state;
  KernelAnalysisConflictPublisher(Kernel &kk, AnalysisConflictHook hook,
                                  void *state)
      : k(kk), saved_hook(kk.analysis_conflict_hook),
        saved_state(kk.analysis_conflict_hook_state) {
    k.analysis_conflict_hook = hook;
    k.analysis_conflict_hook_state = state;
  }
  ~KernelAnalysisConflictPublisher() {
    k.analysis_conflict_hook = saved_hook;
    k.analysis_conflict_hook_state = saved_state;
  }
  KernelAnalysisConflictPublisher(const KernelAnalysisConflictPublisher &) =
      delete;
  KernelAnalysisConflictPublisher &
  operator=(const KernelAnalysisConflictPublisher &) = delete;
};

inline uint32_t kernel_root_fixed(const Kernel &k) {
  return k.level == 0 ? static_cast<uint32_t>(k.trail.size()) : k.frames[1];
}

// Material watermarks and public root counts retain permanent fixed facts.
// Array bounds, trail cursors and active-population subtraction still use the
// physical prefix above. Compaction alone must not consume a new-root signal.
inline uint32_t kernel_logical_root_fixed(const Kernel &k) {
  return kernel_root_fixed(k) + k.fixed_external_count;
}

// A true-level assignment can be appended after higher-level literals, so the
// root is not necessarily a contiguous trail prefix.  Formula simplification
// must read the assignment's semantic level, not the current search value.
inline int8_t kernel_root_value(const Kernel &k, Lit lit) {
  const int8_t value = k.value(lit);
  return value && k.assigned[var_of(lit)].level == 0 ? value : 0;
}

inline bool kernel_has_root_assignment(const Kernel &k) {
  for (const Lit lit : k.trail)
    if (k.assigned[var_of(lit)].level == 0)
      return true;
  return false;
}

void kernel_lrat_chain(Kernel &, uint64_t closing_id);
void kernel_build_lrat_chain(
    std::vector<std::pair<uint32_t, uint64_t>> &entries,
    uint64_t closing_id, std::vector<uint64_t> &chain);

// Native LRAT only: name every root literal appended to the trail at or after
// `from` that a propagation derived, so a later chain can cite it. Callers are
// the propagation entry points, because "root-fixed implies it has a unit id"
// is a kernel invariant (`kernel_check_invariants`), not a rule's obligation.
void kernel_emit_root_units(Kernel &, size_t from);

void kernel_reason_walk(Kernel &, Var start, std::vector<uint8_t> &seen,
                        std::vector<std::pair<uint32_t, uint64_t>> &entries,
                        std::vector<Var> *touched = nullptr);
uint64_t kernel_conflict_reason_walk(
    Kernel &, std::vector<uint8_t> &seen,
    std::vector<std::pair<uint32_t, uint64_t>> &entries,
    std::vector<Var> *touched = nullptr);


// `watch_start` is Kissat's sparse-collection watermark: watches below it keep
// their current order, while relocated clauses at and above it are rewatched
// in arena order. `sparse_collection` distinguishes Reduce start=0 from a
// whole-arena rebuild; a nonzero watermark implies it for compatibility.
// `preserve_eager_recent` is reserved for Tactix-only reclamation passes whose
// reference Kissat operation does not collect the arena at all.
// Kissat's defragmentation checkpoint covers the whole `vectors` stack, which
// holds the sparse watches AND every dense occurrence list. Tactix keeps those
// in three arenas, so a checkpoint inspects all three (collect.h: only after
// creating a clause, only above 2^18 words, only when the stack is more than
// 75 % unusable). Every slice reference dies here.
void kernel_defrag_vectors(Kernel &);

void kernel_collect_garbage(Kernel &, Ref watch_start = 0,
                            bool sparse_collection = false,
                            bool preserve_eager_recent = false);
// Kissat's null-stack dense entry: disconnect every large watch and retain
// only the first binary watch for each partner in each literal list.
void kernel_flush_large_watches(Kernel &);
// Substitute's watch-mode entry (substitute.c `substitute_binaries`): every
// large watch leaves, every binary watch stays in place, duplicates included.
// `kernel_flush_large_watches` is kissat's dedicated flush, which also
// retires duplicate binaries; Substitute keeps them.
void kernel_drop_large_watches(Kernel &);
// Reconnect every live large clause in its current Kissat-equivalent arena
// order. Packed replacements retain their source position until collection,
// exactly as Kissat's in-place rewrites do.  Eliminate's sparse resume passes
// `flush_eliminated=true`: pinned Kissat first garbages, in arena order, every
// clause satisfied at root or containing an eliminated variable.
void kernel_rebuild_large_watches(Kernel &, bool flush_eliminated = false);
void kernel_resume_walk_watches(Kernel &);
void kernel_canonicalize_binary_watches(Kernel &,
                                        bool count_congruence_rebuild = true,
                                        const std::vector<std::pair<Lit, Lit>> *
                                            binary_suffix = nullptr,
                                        std::vector<std::pair<Lit, Lit>> *
                                            binary_population = nullptr);
void kernel_deduplicate_binary_watches(
    Kernel &, std::vector<std::pair<Lit, Lit>> *root_assigned = nullptr,
    std::vector<std::pair<Lit, Lit>> *binary_population = nullptr);

void kernel_dense_connect(Kernel &, OccIndex &occ,
                          bool track_order = false);
void kernel_rebuild_dense_occurrences(Kernel &);
// Append one irredundant large occurrence of `lit` to the attached image: the
// `large` membership mirror and, under Eliminate's mixed stream, `ordered`.
// A rule that gives an existing record irredundant status while an image is
// attached goes through this, never through `large` alone: the two views must
// agree (the dense oracle in kernel_check_invariants).
void kernel_dense_add_large_occurrence(Kernel &, Lit, Ref);
// Remove one large occurrence of `lit` from every attached view.  `large` is
// exactly the large payloads of `ordered` in order, so under the mixed stream
// one pass locates the entry in both; that correspondence is the invariant
// every remover must keep, which is why the Sweep session's eager membership
// erases go through here rather than through `large` alone.
void kernel_dense_remove_large_occurrence(Kernel &, Lit, Ref);
// Move a live redundant large clause into the irredundant population and
// publish it to every attached dense occurrence view.  A later in-place
// rewrite assumes this membership exists for every irredundant literal.
void kernel_promote_large_irredundant(Kernel &, Ref);
void kernel_dense_disconnect(Kernel &);
void kernel_flush_retired_redundant(Kernel &);

uint64_t kernel_irredundant_clauses(const Kernel &);
#ifndef NDEBUG
void kernel_check_binirr_counts(const Kernel &);
#endif

void kernel_check_invariants(const Kernel &);

} // namespace tactix
