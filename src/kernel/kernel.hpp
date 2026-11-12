#pragma once

#include "kernel/clause.hpp"
#include "kernel/connected_occurrence.hpp"
#include "kernel/occ.hpp"
#include "kernel/splitmix64.hpp"
#include "kernel/structural_journal.hpp"
#include "kernel/query_projection.hpp"
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
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tactix {

struct Kernel;
namespace session { class QueryKernel; }

// Lets a query owner with a preflighted replacement certificate discard native
// sources without deleting them from the previous certificate. Never a rule.
class QueryPopulationRetirement {
  friend class session::QueryKernel;
  QueryPopulationRetirement() = default;
};
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

// Open-addressing binary-id table (one allocation, unlike unordered_map).
// After input the watch lists are the membership authority and only the
// size-monotone addition count remains.
class BinaryIdMap {
  static constexpr uint64_t EMPTY = UINT64_MAX;
  // Separate key and id arrays, so a kernel can omit the ids.
  std::vector<uint64_t> keys_;
  std::vector<uint64_t> values_;
  size_t size_ = 0;
  bool track_keys_ = true;
  bool track_values_ = true;

  static uint64_t hash(uint64_t x) {
    return splitmix64_hash(x);
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
    // The pair count is a monotone watermark; the archive can be released.
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

  // Linear probing at 7/8 load: keys are never erased.
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

// Optional no-false-negative prefilter for binary membership queries; the
// watch lists stay the exact authority (stale bits fall back to a scan).
class BinaryLivenessIndex {
  // Blocked layout: a key's three probe bits share one 512-bit cache line.
  static constexpr uint64_t BLOCK_BITS = 512;
  static constexpr uint64_t BLOCK_WORDS = BLOCK_BITS / 64;
  std::vector<uint64_t> bits_;
  uint64_t block_mask_ = 0;
  size_t entry_capacity_ = 0;
  uint32_t exact_literals_ = 0;

  static uint64_t hash(uint64_t x) {
    return splitmix64_hash(x);
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
  // A compact literal universe uses an exact pair bitmap when it is at most
  // 256 MiB and no larger than the two-word watch payload of
  // 'storage_entries'; otherwise the blocked Bloom filter.
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

// Packed replacement proof ID -> source's current arena position (per generation).
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
  // No alias since the last collection: every clause sits at its own Ref.
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
void event_trace_phase_save(const Kernel &, Lit);
void event_trace_root_assign(const Kernel &, Lit);
void event_trace_sweep_flip(const Kernel &, Var, const char *, Lit, bool,
                            uint64_t);
void event_trace_factor_mark_transition(const Kernel &, Lit, bool,
                                        const char *);
void event_trace_bve_retry_mark(const Kernel &, Var, const char *, bool, bool,
                                const Clause *);
#endif

struct FactorSelection;
struct Heuristics;

struct Assigned {
  uint32_t level;
  uint32_t trail; // position on the trail
  uint32_t reason; // Ref, or other literal if binary, or sentinels
  bool binary;
  bool jumped;
  bool affine = false;
};

static_assert(sizeof(Assigned) == 16,
              "native affine reasons must not widen the hot assignment table");

struct Link {
  uint32_t prev, next, stamp;
};

struct Stats {
  static constexpr uint32_t MAX_TIMED_RULES = 64;
  static constexpr uint32_t MAX_GLUE_USED = 127;
  // Counters live in kernel/stats.inc (also the feedback inventory).
#define TACTIX_STATS_U64(name) uint64_t name = 0;
#define TACTIX_STATS_U32(name) uint32_t name = 0;
#define TACTIX_STATS_MUTABLE(name) mutable uint64_t name = 0;
#define TACTIX_STATS_INIT(name, value) uint64_t name = value;
#define TACTIX_STATS_ARRAY(name, dims) uint64_t name dims = {};
#define TACTIX_STATS_TIMER(name) uint64_t name = 0;
#define TACTIX_STATS_TIMER_ARRAY(name, dims) uint64_t name dims = {};
#include "kernel/stats.inc"
#undef TACTIX_STATS_U64
#undef TACTIX_STATS_U32
#undef TACTIX_STATS_MUTABLE
#undef TACTIX_STATS_INIT
#undef TACTIX_STATS_ARRAY
#undef TACTIX_STATS_TIMER
#undef TACTIX_STATS_TIMER_ARRAY
};

struct PendingLearned {
  bool valid = false;
  std::vector<Lit> lits; // lits[0] = asserting literal
  Ref ref = INVALID_REF; // arena ref for size >= 3, INVALID otherwise
  uint32_t glue = 0;
  uint32_t jump_level = 0;
  uint64_t id = 0; // proof clause id
};

struct OccIndex {
  FlatLists<Ref> large; // Probe-only owner; may hold stale garbage Refs
  // Kissat dense watches are one mixed stream: the entry-time binary prefix,
  // arena-order large occurrences, then clauses created while dense. Other
  // simplifiers use the large-only projection.
  using Entry = ConnectedOccurrences::Entry;
  static_assert(sizeof(Entry) == sizeof(uint32_t),
                "dense mixed entries must match Kissat's one-word watch");
  ConnectedOccurrences ordered; // sole payload owner in mixed mode
  // Only Eliminate/BVE consumes the mixed stream.
  bool track_order = false;

  size_t large_lits() const { return track_order ? ordered.lits() : large.lits(); }
  uint32_t large_size(Lit lit) const {
    return track_order ? ordered.large_size(lit) : large.size(lit);
  }
  // Dispatch once outside the consumer's loop.
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

// Glue-tier retier schedule (kissat tiers.c; deadline tested in analyze.c).
// Changes neither F, R nor the proof. A native request carries it by value; a
// public session installs it into `Kernel::retier_schedule` (calculus.md §3 M3).
struct RetierSchedule {
  bool enabled = false;
  uint64_t next_conflicts = 0, interval = 0, max_interval = 0;
  uint32_t growth = 1;
  uint32_t tier1_default = 2, tier2_default = 6;
  uint32_t tier1_relative_permille = 500, tier2_relative_permille = 900;
  // Kissat's separate focused (0) and stable (1) tier snapshots.
  uint32_t tier1_by_mode[2] = {2, 2};
  uint32_t tier2_by_mode[2] = {6, 6};
  bool operator==(const RetierSchedule &) const = default;
};

struct Kernel;
// Policy-only observer for Kissat's BVE schedule: while a BVE dose owns a live
// heap, each affected variable is rescored at the mutation. `reinsert` marks
// removed material (rearms a popped candidate).
using BveScoreUpdateHook = void (*)(Kernel &, void *, Var, bool reinsert);

// Trail with a reserved backing store: every variable-growth path reserves it
// before an assignment, so a push needs no growth check (Kissat's raw stack).
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

  // Adopts compaction's replacement trail, exposing its spare reservation.
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
  bool operator==(const ReservedTrail &other) const {
    return size() == other.size() &&
           std::equal(begin(), end(), other.begin());
  }

private:
  std::vector<Lit> storage_;
  size_t used_ = 0;
};

struct Kernel {
  uint32_t nvars = 0;
  // Keeps the Kernel ABI of the standalone event-trace test (tests/CMakeLists.txt).
  size_t diagnostic_policy_reduce_bytes = 0;
  size_t diagnostic_policy_inprocess_bytes = 0;
  // Stable external identities vs the dense search indices (identity before
  // the first compaction); a dead external maps to INVALID_VAR.
  std::vector<Var> internal_to_external;
  std::vector<Var> external_to_internal;
  // Kissat-style live compact identity (permutation plus inverse): formula
  // representation state, usable by proof rules without `H`.
  std::vector<Var> compact_import_order;
  std::vector<Var> compact_import_position;
  uint32_t original_external_vars = UINT32_MAX;
  uint32_t first_extension_var = UINT32_MAX;
  bool inconsistent = false; // root-level conflict happened
  // Monotone, never reset by statistics: witnessed projection repairs and SR
  // steps may change models on externally fixed variables.
  bool projection_changed = false;

  std::vector<int8_t> values;     // by lit: 1 true, -1 false, 0 unassigned
  std::vector<Assigned> assigned; // by var
  std::vector<Lit> jumped_over;

  ReservedTrail trail;
  uint32_t propagate_head = 0;
  // Lowest trail position binary-only propagation moved `propagate_head` past
  // without full coverage (UINT32_MAX: none). Binary-only scans skip large
  // watches, so the next full propagation pulls the cursor back here.
  uint32_t binary_only_head = UINT32_MAX;
  bool trail_tainted = false;
  std::vector<uint32_t> frames = {0};
  uint32_t level = 0;
  uint32_t unassigned = 0;
  uint32_t unassigned_active = 0;
  bool warming = false; // Kissat walk warm-up: suppress random branching.
  uint64_t walk_random_seed = 0; // policy RNG snapshot for a faithful Walk

  Arena arena;
  // A packed rewrite's proof id -> source position + 1 (0: no alias), since
  // Kissat shortens in place; collection materializes and clears it.
  ClausePositionAliases clause_position_alias;
  // Logical position of the first reducible record; collection recomputes it.
  Ref first_reducible = INVALID_REF;
  WatchLists watches;
  // Kissat's `watching`: false while large watches are disconnected.
  bool large_watches_connected = true;
  ArenaOrdering *arena_ordering = nullptr;
  OccIndex *dense = nullptr;
  // Literals whose dense large-occurrence list the caller clears in one pass,
  // so an in-place rewrite skips per-clause erasure (as kissat substitution).
  const std::vector<uint8_t> *dense_bulk_cleared = nullptr;
  // Reusable marks for the above; never sized per call.
  std::vector<uint8_t> dense_bulk_scratch;
  // Collection's survivor list, kernel-owned to avoid reallocation.
  std::vector<Ref> collect_order_scratch;
  // The dense rebuild's reference list (collection's own list is live then).
  std::vector<Ref> dense_order_scratch;
  // Delayed large-watch relocation scratch; `delayed_used` is the live length.
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
  // Admission order of the SEEN | REASON_BUMP set (Kissat's separate stack);
  // EVSIDS heap topology depends on it.
  std::vector<Var> bump_order;
  std::vector<Var> shrinkable; // vars marked during a block-shrink walk
  uint32_t analyzed_seen = 0;
  std::vector<Var> reason_bumped; // REASON_BUMP marks, for rollback
  std::vector<Var> minimize_scratch; // iterative binary-reason chain
  std::vector<Lit> learned_scratch; // the clause under construction
  std::vector<Lit> otfs_scratch;    // OTFS resolvent assembly
  // OTFS replacement records; the old reason is retired after Backjump.
  std::vector<Ref> otfs_retired;
  // Policy observation after the learned clause joined the formula.
  std::vector<Var> radix_scratch;   // radix-sort double buffer
  std::vector<uint8_t> marked_levels;
  std::vector<uint32_t> touched_levels;

  static constexpr uint32_t EAGER_SUBSUME_SLOTS =
      strategy::AnalyzeParameters::EAGER_SUBSUME_CAPACITY;
  Ref eager_recent[EAGER_SUBSUME_SLOTS] = {INVALID_REF, INVALID_REF,
                                           INVALID_REF, INVALID_REF};
  std::vector<uint8_t> eager_marks;

  uint64_t next_clause_id = 0;
  // Live irredundant large clauses (kissat `statistics.clauses_irredundant`,
  // read in constant time by kissat_classify and BINIRR_CLAUSES).
  uint64_t live_irredundant_large = 0;
  std::vector<uint64_t> unit_ids; // by var
  // Root units already written to the proof, by variable: emitted polarity, 0
  // if none. Separate from `unit_ids`, which rules observe.
  std::vector<int8_t> dsr_root_values;
  const Proof *dsr_root_proof = nullptr;
  uint64_t dsr_root_stream_identity = 0;
  BinaryIdMap binary_ids;
  BinaryLivenessIndex *binary_liveness = nullptr; // scoped dense-rule index
  std::vector<Var> input_activation_order;
  // Parse-order binary image for startup structural model recovery: small
  // formulas only, released after the model-search opportunity.
  std::vector<uint64_t> input_binary_order;
  bool input_binary_order_captured = false;
  // The Fermat owner's input summary: long-clause count/shape plus a ring of
  // the last 80 raw binary DIMACS pairs.
  static constexpr uint32_t INPUT_CLAUSE_TAIL_CAPACITY = 80;
  std::vector<std::array<int32_t, 2>> input_clause_tail;
  uint32_t input_clause_tail_next = 0;
  uint32_t input_long_clause_count = 0;
  uint64_t input_positive_binary_count = 0;
  uint64_t input_binary_clause_count = 0;
  uint64_t input_ternary_clause_count = 0;
  uint64_t input_unit_clause_count = 0;
  // Original-input terminal-certificate boundary, closed before the first rule.
  bool startup_certificate_open = true;
  // Original zero-delimited DIMACS literals, kept until the first action.
  std::vector<int32_t> startup_input;
  void close_startup_certificate() {
    if (!startup_certificate_open)
      return;
    startup_certificate_open = false;
    std::vector<int32_t>().swap(startup_input);
  }
  // The last DIMACS header's counts, so an input replay poses the same kernel.
  struct InputHeader {
    bool declared = false;
    uint32_t variables = 0;
    uint64_t clauses = 0;
  } input_header;
  uint64_t input_nonunit_clause_count = 0;
  uint32_t input_uniform_nonunit_width = 0;
  bool input_nonunit_uniform = true;
  uint32_t input_fermat_width = 0;
  bool input_fermat_long_shape = true;
  std::vector<uint8_t> input_activated; // per-var membership, parse-scoped
  std::vector<uint8_t> input_marks;     // per-lit membership, parse-scoped
  // Permanent external import ranks (Kissat tumble=1 first-occurrence order).
  std::vector<uint32_t> external_import_rank;
  // The same rank by current compact identity (UINT32_MAX: not yet seen).
  std::vector<uint32_t> internal_import_rank;
  // Inverse of `external_import_rank`; retired identities are holes.
  std::vector<Var> import_rank_to_external;
  uint32_t next_import_rank = 0;
  bool input_refuted = false;

  Proof *proof = nullptr;
  // Nullable non-owning population observer (settled by its binding).
  PopulationObserver *population_observer = nullptr;
  // Observer-lifetime authentication: a detached capture stays untrusted.
  uint64_t population_binding_epoch = 0;
  bool population_observe_raw_input = true;
  bool population_observe_solver = true;
  uint64_t next_population_transaction = 0;
  Stats stats;

  // DefaultPolicy's frontend may defer cold per-variable state until startup
  // propagation and early Lucky fail; other clients construct it eagerly.
  bool cold_state_deferred = false;
  std::vector<uint8_t> bve_retry;

  // Kissat's persistent `flags->factor` bit, per literal polarity.
  std::vector<uint8_t> factor_marks;
  // Kissat's cumulative `literals_factor` watermark (never decremented).
  uint64_t factor_literals_marked = 0;
  // Whether the last Factor drained its schedule (kissat
  // factor.c::run_factorization, `limits.factor.marked`).
  bool factor_completed = false;

  // Kissat's retained Backbone candidate polarities (bit 0 positive, bit 1
  // negative), scheduled first.
  std::vector<uint8_t> backbone_priority;

  // Bumped by every `kernel_collect_garbage`; older Refs are stale.
  uint64_t arena_generation = 1;

  // Cold strategy boundary epoch advanced by registry dispatch and public API
  // mutations; online selections bind to it.
  uint64_t strategy_epoch = 0;

  uint64_t touch_epoch = 0;
  // Native subsumption-material lifecycle, separate from touch epochs; all
  // activated variables are armed during deferred startup.
  std::vector<uint8_t> subsume_material;
  uint64_t subsume_material_count = 0;
  std::vector<uint64_t> touched; // by var
  std::vector<uint64_t> removed; // by var
  std::vector<uint64_t> touched_kept; // by var
  uint32_t touch_fixed_head = 0;
  // Root prefix whose satisfied opposite occurrences BCE/CCE have marked.
  uint32_t root_occurrence_head = 0;
  // Kissat's `unflushed` root-unit count, a control guard for Reduce and
  // Rephase; Tactix keeps root literals on the trail.
  uint32_t unflushed_root = 0;
  uint64_t subsume_touch_mark = 0;
  uint64_t strengthen_touch_mark = 0;
  uint64_t bce_touch_mark = 0;
  // Smallest finite ceiling that declined canonical work (scoped profiles).
  uint32_t bce_rejected_occ_limit = 0;
  // A query-compatible batch covers only its cube's selected literal domain.
  std::vector<int32_t> bce_query_cube;
  uint64_t bce_query_touch_mark = 0;
  uint64_t ternary_touch_mark = 0;
  uint64_t instantiate_touch_mark = 0;
  uint64_t cce_touch_mark = 0;
  // No finite budget can exceed UINT64_MAX, so it denotes no refused cover.
  uint64_t cce_rejected_budget = UINT64_MAX;
  bool subsume_complete = true;
  bool strengthen_complete = true;
  bool touch_filter = true;
  // Kissat forward.c admits only irredundant victims; the CaDiCaL band is a diagnostic arm.
  bool touch_redundant = false;
  uint32_t kept_glue = 0;
  uint32_t kept_size = 0;
  BveScoreUpdateHook bve_score_update_hook = nullptr;
  void *bve_score_update_state = nullptr;
  // Optional strategy-owned dense-occurrence count image (live BVE heap).
  std::vector<uint32_t> *bve_occurrence_count_image = nullptr;

  uint32_t bve_gate_mask = ~0u;
  uint32_t bve_bound = 16;
  // Policy-owned BVE occurrence cap (UINT32_MAX: unrestricted).
  uint32_t bve_occurrence_limit = UINT32_MAX;
  // Kissat `eliminateclslim`: decline a candidate with a larger live
  // irredundant occurrence. Unrestricted until a strategy installs it.
  uint32_t bve_clause_size_limit = UINT32_MAX;
  // Optional cap on non-tautological resolvents per pivot (UINT32_MAX: none).
  uint32_t bve_resolvent_count_limit = UINT32_MAX;
  // Optional CaDiCaL-derived eager tail; Kissat defers it to the next forward
  // pass (default). TACTIX_BVE_BACKWARD=1 enables it.
  bool bve_backward = false;
  // CaDiCaL's generation-time self-subsuming resolution.
  bool bve_on_the_fly_strengthening = false;
  // Startup fastel traversal order; ordinary Eliminate uses Kissat's.
  bool bve_fastel_traversal = false;
  // Strategy-owned XOR arity; limits are named in rules/bve/bve.hpp.
  uint32_t bve_xor_arity_limit = 5;
  uint32_t bve_resolvent_size_limit = 100;
  uint32_t bve_backward_occurrence_limit = 100;
  // Kissat `definitionticks`: first-core Kitten ticks per candidate.
  uint64_t bve_definition_budget = 1000000;
  // Kissat `definitioncores`: refine once after the first extracted core.
  uint32_t bve_definition_cores = 2;
  // Per-refinement Kitten allowance relative to `bve_definition_budget`.
  uint32_t bve_definition_core_budget_multiplier = 10;
  // Definition Kitten lifetime (Kissat: one per elimination, cleared per candidate).
  uint64_t bve_definition_session = 0;
  uint32_t bce_occ_limit = 0;
  bool bce_preserve_query = false;
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
  // Strategy advice by zero-based external identity; rules stay unfiltered.
  std::unordered_set<uint32_t> variable_retention;
  const std::unordered_set<uint32_t> *substitute_retention = nullptr;
  std::vector<VarStatus> var_status;
  std::vector<Var> retired_events;
  // TACTIX_SUPPORT_HOLD_RETIREMENTS: keep retirement events queued (Kissat's
  // post-Sweep union waits for Substitute).
  bool retirement_publication_held = false;
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
  // Non-owning hook into policy-owned saved phases, refreshed at assignment
  // time as in Kissat; installed together with `h`.
  std::vector<int8_t> *saved_phases = nullptr;
  bool phase_saving_suppressed = false; // true only for probing assignments
  // Rule-produced structural assignments: H-only advice.
  std::array<strategy::LocalSearchHint, strategy::LOCAL_SEARCH_KIND_COUNT>
      local_search_hints;
  // Structural target consumed by the most recent successful Decide (0: none).
  uint64_t local_search_last_target_decision_candidate = 0;
  bool local_search_last_target_decision_matched = false;
  // Publication identity is solver state, independent of resettable telemetry.
  uint64_t local_search_publication = 0;
  uint64_t walk_seed_candidate = 0;
  uint64_t walk_preparation_budget = 0;
  uint32_t walk_objective = 0, walk_admission = 0;
  uint32_t walk_algorithm = 0, walk_maximum_tries = 100;
  uint32_t walk_initialization = 0;
  uint64_t walk_flips_per_try = 1000000;
  uint32_t walk_weight_smoothing_threshold = 50;
  uint32_t walk_smoothing_old_numerator = 3;
  uint32_t walk_smoothing_denominator = 10;
  bool walk_aspiration = true;
  // Staged proof-rule declarations, observable after the action epoch advances.
  std::array<strategy::SemanticVariableCandidate,
             strategy::SEMANTIC_VARIABLE_PRODUCER_COUNT>
      semantic_variable_candidates;
  strategy::SemanticVariableCandidate staged_semantic_variable_candidate;
  bool semantic_variable_candidate_pending = false;
  uint64_t semantic_variable_publication = 0;
  uint64_t work_budget = 0;
  // Work already charged against a cumulative dose (Vivify tier ceilings).
  uint64_t work_budget_offset = 0;
  bool work_budget_bounded = false;
  uint8_t vivify_scope = 0;
  bool vivify_reuse = true;
  bool vivify_flush = false;
  uint32_t vivify_irredundant_sort_ratio =
      strategy::VivifyParameters::IRREDUNDANT_SORT_RATIO_DEFAULT;
  std::vector<Var> sweep_tail; // stable external identities
  uint32_t sweep_tail_next = 0;
  // Kissat's `schedule_inner` moves a proved representative to the pop end in
  // O(1): representatives go on this LIFO front, consumed before the tail. An
  // earlier live entry is tombstoned via `sweep_slot` (tail index, or front
  // index | SWEEP_FRONT_BIT, plus one).
  std::vector<Var> sweep_front;
  std::vector<uint32_t> sweep_slot; // by external var; 0 = no live entry
  uint32_t sweep_tail_indexed = 0;  // tail entries whose slot is recorded
  uint32_t sweep_live = 0;          // live unconsumed entries, front + tail
  std::vector<uint8_t> sweep_marked; // external vars still in the current pass
  bool sweep_incomplete = false;    // independent of a nonempty saved schedule
  uint32_t sweep_passes_completed = 0;
  // Kissat's one_literal_on_conflict_level path (off in bare kernels).
  bool conflict_reuse = false;
  // Chronological gap for the conflict-reuse shortcut.
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
  strategy::ReorderParameters reorder_parameters;
  strategy::ParityModelParameters parity_model_parameters;
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
  strategy::OnehotModelParameters onehot_model_parameters;
  // The retier schedule a public session installed.
  RetierSchedule retier_schedule;
  // `tier1/tier2` is the pair posed for the rule currently being applied.
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

  // Fixed external identities archived by compaction: distinct facts, not
  // aliases of the root representative.
  std::vector<int8_t> fixed_external_values;
  std::vector<uint64_t> fixed_external_units;
  uint32_t fixed_external_count = 0;

  // Strategy-supplied discovery parameters (cold tail, keeps hot offsets);
  // explicit C1/C2 instances bypass them.
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
  // Where structural actions record; a borrowed stream stays open through
  // them. Sequence IDs never reset on rebind or proof-mode changes.
  StructuralJournal structural_provenance;
  // Opt-in journal encoding without per-step proof-event telemetry.
  bool affine_matrix_provenance_compact = false;
  uint64_t structural_action_sequence = 0;
  // Non-owning feature caches; never serialized, must not outlive Kernel.
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
  bool affine_reasons_unaffordable = false; // the session's budget ran out in analysis
  // One retained graph trajectory shared by public and Default actions.
  GraphLabelingModelSession graph_labeling_session;
  uint64_t affine_trail_retraction_epoch = 1;
  uint64_t affine_semantic_namespace_epoch = 1;
  // Direct mutation fixtures must invalidate the affected epoch or opt out.
  bool affine_capture_epochs_trusted = true;
  strategy::InstantiateParameters instantiate_parameters;
  strategy::BackboneParameters backbone_parameters;

  // Cold, optional query-projection service; never read by propagation.
  QueryProjectionService *query_projection = nullptr;

#ifdef TACTIX_ENABLE_EVENT_TRACE
  // Diagnostic-only storage stays at the final object boundary, so a
  // trace-compiled test object keeps the production Kernel layout prefix.
  uint64_t diagnostic_search_propagation_ticks = 0;
#endif

  int8_t value(Lit l) const { return values[l]; }
  uint32_t var_level(Var v) const { return assigned[v].level; }
};

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
// Cold primitive: append one native coordinate for an unmapped external
// identity, installing the mapping first. The caller owns root safety.
void kernel_reconnect_external_variable(Kernel &, Var external);
void kernel_defer_cold_state(Kernel &);
void kernel_materialize_cold_state(Kernel &);
void kernel_record_import(Kernel &, Var);
// Inline: queried in hot sort comparators.
inline uint64_t kernel_import_var_order(const Kernel &k, Var v) {
  assert(v < k.nvars);
  assert(k.internal_import_rank.size() == k.nvars);
  const uint32_t direct = k.internal_import_rank[v];
  if (direct != UINT32_MAX)
    return direct;
  const Var external = k.internal_to_external[v];
  assert(external >= k.external_import_rank.size() ||
         k.external_import_rank[external] == UINT32_MAX);
  // Direct clients: unrecorded variables follow recorded imports.
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

// Reserve permanent external IDs for proof-only helpers after input import,
// identically with or without a proof writer; first is zero-based. No reuse.
bool kernel_reserve_proof_variables(Kernel &, uint32_t count, Var &first);
Var kernel_new_extension_vars(Kernel &, uint32_t count);

// Re-arm propagation after an authenticated non-unit theory clause, currently
// unit or conflicting, is installed above root. Not for learned clauses.
void kernel_rearm_theory_clause(Kernel &, const Lit *, uint32_t size);

inline uint32_t kernel_num_output_vars(const Kernel &k) {
  return k.original_external_vars == UINT32_MAX
             ? static_cast<uint32_t>(k.external_to_internal.size())
             : k.original_external_vars;
}

// Solver-created variables have external identities at or beyond the input
// boundary (stable across compaction). Startup policy advice only.
inline bool kernel_startup_retained(const Kernel &k, Var variable) {
  return k.substitute_retention &&
         k.substitute_retention->count(k.internal_to_external[variable]);
}

inline bool kernel_is_solver_extension(const Kernel &k, Var variable) {
  assert(variable < k.nvars);
  assert(variable < k.internal_to_external.size());
  return k.original_external_vars != UINT32_MAX &&
         k.internal_to_external[variable] >= k.original_external_vars;
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
// A conditional-autarky segment: the removed clause (internal literals) and
// the validated cube (stable external literals) set true if it is false.
void kernel_push_cube_witness(Kernel &, const Lit *clause, uint32_t nclause,
                              const Lit *witness, uint32_t nwitness);
// Reconstruct a total external model; resumable online paths pass false so
// export does not mutate internal values.
void kernel_extend_model(Kernel &, bool materialize_internal = true);
// Seed unassigned retired coordinates from a licensed assumption cube.
void kernel_extend_model_seeded(Kernel &, std::span<const int32_t>,
                                 bool materialize_internal = false);

// Densely renumber variables whose retirement is complete; external
// identities do not change. False when nothing to remove or not yet safe.
// fold_fixed also archives unreferenced fixed identities, keeping one
// representative; a fixed variable named by a live clause or reason stays.
bool kernel_compact_variables(Kernel &, bool fold_fixed = false);
bool kernel_should_compact_variables(const Kernel &, uint32_t percent);

void kernel_emit_empty(Kernel &);

struct PendingUnits {
  std::vector<Lit> units;    // first-recorded order (assert after the round)
  // Borrows the all-zero literal mark bytes at root and restores them.
  Kernel *owner = nullptr;
  enum class Outcome { Recorded, Duplicate, Refuted };
  PendingUnits() = default;
  PendingUnits(const PendingUnits &) = delete;
  PendingUnits &operator=(const PendingUnits &) = delete;
  ~PendingUnits();
  void init(Kernel &);
  Outcome record(Kernel &, Lit u);
  // A recorded unit is not yet the root value: the opposite literal may
  // become root-true before the batch is asserted.
};

enum class ReplaceOutcome { Refuted, Unit, Binary, Large };
ReplaceOutcome kernel_replace_clause(Kernel &, Clause *orig, uint64_t id,
                                     const Lit *lits, uint32_t size,
                                     bool redundant, uint32_t glue,
                                     Ref *large_ref, // nullable
                                     bool mark_added_material = true);

// Representation-only large-clause replacement after the caller emitted the
// replacement addition: emits the old deletion, keeps the Ref and span, and
// updates watches and indexes. The image may not grow. `mark_added_material`
// publishes it as new subsumption/Factor material (not canonical Substitute).
// `retained_watch` (kissat strengthen.c large_on_the_fly_strengthen) is
// `lits[0]` and keeps its entry; one watch for `lits[1]` is appended.
void kernel_replace_large_clause_in_place(Kernel &, Ref source, uint64_t id,
                                          const Lit *lits, uint32_t size,
                                          uint32_t glue,
                                          const char *source_tag = "unspecified",
                                          bool mark_added_material = true,
                                          Lit retained_watch = INVALID_LIT);

// Proof-safe root-unit consumption for sparse collection (unmarked) and
// Vivify scheduling (a size-two terminal is a new binary, as Kissat). The
// clause form does not collect; the whole-arena form collects once at exit.
enum class RootFlushOutcome { None, Deleted, Rewritten, Binary, Unit, Refuted };
// `clamp_glue`: kissat's root flush (collect.c) keeps glue, Vivify clamps to
// size-1 (vivify.c). Sparse collection passes no removal advice.
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
// Monotone observation: clause additions and semantic removals stamp
// `touch_epoch`, root assignments count in `root_assignments`. Representation
// work is invisible here.
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
// A proof-visible non-root implication reason outside the watched formula:
// GARBAGE (ignored by scans) and REASON-locked while its assignment is live;
// unassignment releases it. It is the arena form of a native theory reason.
Ref kernel_add_detached_large_reason(Kernel &, const Lit *lits, uint32_t size,
                                     uint64_t id);
uint64_t kernel_clause_position(const Kernel &, Ref);
// Restore current Kissat arena order after append-only replacements (radix).
void kernel_sort_refs_by_logical_clause_position(const Kernel &,
                                                 std::vector<Ref> &);
void kernel_inherit_clause_position(Kernel &, uint64_t replacement_id,
                                    Ref source_ref);
void kernel_add_binary_clause(Kernel &, Lit a, Lit b, uint64_t id = 0,
                              bool mark_added_material = true);
// An in-place root binary rewrite re-arms propagation for its new literals.
void kernel_rearm_binary_clause(Kernel &, Lit a, Lit b);
// Bulk input: the pair is known absent (duplicates violate the contract).
void kernel_add_new_binary_clause(Kernel &, Lit a, Lit b, uint64_t id = 0);
enum class BinaryPopulationEffect : uint8_t {
  RepresentationOnly,
  LogicalAdd,
};
// Complete a pair whose `stable` direction was rewritten in place.
void kernel_add_binary_mirror(Kernel &, Lit stable, Lit moved,
                              uint64_t id = 0);
// Publish add metadata for a binary direction already inserted.
void kernel_publish_binary_clause(Kernel &, Lit a, Lit b, uint64_t id,
                                  bool new_pair, BinaryPopulationEffect);
bool kernel_delete_binary(Kernel &, Lit a, Lit b);
// Remove exactly one physical copy; a surviving copy stays a valid reason.
// The final copy delegates to the named-pair deletion.
bool kernel_delete_binary_one_copy(Kernel &, Lit a, Lit b);
// Dense BVE: like Kissat, rescore only the partner; the popped pivot is
// mid-scan.
bool kernel_delete_binary_one_copy_bve_scan(Kernel &, Lit pivot, Lit other);
bool kernel_delete_binary_all_copies(Kernel &, Lit a, Lit b);
bool kernel_discard_query_binary_copies(Kernel &, Lit, Lit,
                                        const QueryPopulationRetirement &);
// forward.c duplicate sweep: keep the first copy, retire later ones; count.
uint32_t kernel_remove_duplicate_binary_copies(Kernel &, Lit a, Lit b);
// Factor's dense entry already counted the pair's copies.
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
  // Either endpoint is authoritative: scan the smaller population.
  if (k.watches.binary_size(b) < k.watches.binary_size(a))
    std::swap(a, b);
  return kernel_binary_copies(k, a, b, 1) == 1;
}

// Remove one large entry (a root-fixed slot may already be absent).
void kernel_unwatch_large(Kernel &, Lit l, Ref ref);

// Exchange two large-clause entries in one watch list in place (OTFS); the
// last two arguments are each Ref's blocker after the swap.
void kernel_swap_large_watch_refs(Kernel &, Lit l, Ref first, Ref second,
                                  Lit blocking_for_first,
                                  Lit blocking_for_second);

void kernel_promote_watched_lit(Kernel &, Ref ref, uint32_t slot,
                                uint32_t from);

// Publish a propagation-derived root unit while its reason is still available;
// idempotent through the polarity table.
void kernel_materialize_dsr_root_unit(Kernel &, Lit);
// Record an asserted root unit as already in the proof, without emitting it.
void kernel_note_dsr_root_unit(Kernel &, Lit);
bool kernel_root_unit_table_stream(const Kernel &);

// Retire any trail reason naming this clause, publishing a root unit first.
inline void kernel_retire_root_reason(Kernel &k, const Clause *c, Ref ref) {
  for (const Lit lit : *c) {
    Assigned &a = k.assigned[var_of(lit)];
    if (!a.affine && !a.binary && a.reason == ref && k.values[lit] != 0) {
      // In-place rewrites keep a live non-root reason Ref; only a level-zero
      // reason becomes a permanent unit.
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

inline bool kernel_records_weakening(const Kernel &k) noexcept {
  return k.population_observer && k.population_observe_solver &&
         k.population_observer->records_weakening();
}

inline ScopedWeakeningCapture::ScopedWeakeningCapture(Kernel &k, WeakeningFamily family,
    const Lit *clause, uint32_t size, Lit witness, uint64_t source_id,
    bool binary, bool all_binary_copies) noexcept : kernel_(k) {
  if (!kernel_records_weakening(k)) return;
  token_ = kernel_prepare_weakening(k, family,
      kernel_population_clause(k, clause, size),
      kernel_population_clause(k, &witness, 1), source_id, binary, all_binary_copies);
}

inline ScopedWeakeningCapture::~ScopedWeakeningCapture() {
  if (token_) kernel_finish_weakening(kernel_, token_, false);
}

inline ScopedWeakeningCapture::ScopedWeakeningCapture(Kernel &k, WeakeningFamily family,
    PopulationClauseView clause, PopulationClauseView witness, uint64_t source_id,
    bool binary, bool all_binary_copies, uint64_t reconstruction_words) noexcept : kernel_(k) {
  if (!kernel_records_weakening(k)) return;
  token_ = kernel_prepare_weakening(k, family, clause, witness, source_id,
                                  binary, all_binary_copies, reconstruction_words);
}

inline void ScopedWeakeningCapture::commit() noexcept {
  if (token_) kernel_finish_weakening(kernel_, token_, true);
  token_ = 0;
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

template <bool QueryDiscard = false>
inline void kernel_delete_clause_impl(Kernel &k, Clause *c,
                                 bool semantic_removal = true,
                                 const char *retry_source = "delete-clause",
                                 bool checker_visible = true) {
  // The proof delta lives inside the retirement: a second `d` for a garbage
  // record would retire a live duplicate in the checker.
  if (c->garbage())
    return; // already retired
  const Ref ref = static_cast<Ref>(reinterpret_cast<const uint32_t *>(c) -
                                   k.arena.data());
  kernel_retire_root_reason(k, c, ref);
  // Deferred GF(2) placeholders have no checker identity and retire here
  // without a proof deletion.
  assert(checker_visible || QueryDiscard || (c->redundant() && !c->id()));
  if (k.proof && checker_visible)
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
  if (semantic_removal)
    for (const Lit l : *c) {
      if (!c->redundant())
        kernel_mark_bve_retry(k, var_of(l), retry_source, c);
      kernel_removed_var(k, var_of(l)); // both rules quantify over learned clauses
      if (!c->redundant())
        kernel_notify_bve_score(k, var_of(l), true);
    }
}

inline void kernel_delete_clause(Kernel &k, Clause *c,
                                 bool semantic_removal = true,
                                 const char *retry_source = "delete-clause",
                                 bool checker_visible = true) {
  kernel_delete_clause_impl(k,c,semantic_removal,retry_source,checker_visible);
}

inline void kernel_discard_query_clause(Kernel &k, Clause *c,
                                        const QueryPopulationRetirement &) {
  assert(k.trail.empty() && !k.level && !k.pending.valid && !k.conflict_valid);
  kernel_delete_clause_impl<true>(k,c,true,"query-discard",false);
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
    // Kissat's `fixed` is a level-zero assignment.
    if (k.values[make_lit(v, false)] && k.assigned[v].level == 0)
      continue;
    active++;
  }
  return active;
}

inline uint64_t kernel_cache_lines(uint64_t n) { return (n + 31u) >> 5; }

inline void kernel_charge_ticks(Kernel &k, uint64_t n) {
  k.stats.ticks += n;
  // Kissat search adds to both `ticks` and `search_ticks` (propsearch.c);
  // warm-up adds only `ticks` (propbeyond.c:14-24).
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
  // Kissat inlineassign.h prefetches the next watch list (latency only).
  const Lit scan = negate(lit);
  if (k.watches.size(scan))
    __builtin_prefetch(k.watches.begin(scan), 0, 1);
  k.values[lit] = 1;
  k.values[negate(lit)] = -1;
  k.assigned[v] = {level, static_cast<uint32_t>(k.trail.size()), reason,
                   binary, jumped, false};
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
  // kissat inlineassign.h: non-probing assignments refresh the saved phase.
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
inline void kernel_assign_affine(Kernel &k, Lit lit, uint32_t level) {
  assert(level);
  kernel_assign(k, lit, level, AFFINE_REASON, false);
  k.assigned[var_of(lit)].affine = true;
}
inline void kernel_assign_decision(Kernel &k, Lit lit) { // level++ inside
  k.level++;
  k.frames.push_back(static_cast<uint32_t>(k.trail.size()));
  kernel_assign(k, lit, k.level, DECISION_REASON, false);
}
[[gnu::always_inline]] inline void kernel_assign_unit(Kernel &k,
                                                       Lit lit) { // root level
  kernel_assign(k, lit, 0, UNIT_REASON, false);
  // An asserted root unit without a unit id must be in the DSR root table.
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
    k.jumped_over[var_of(lit)] = other_false;
    k.stats.jumped_reasons++;
    kernel_assign(k, lit, level, a.reason, true, true);
    return;
  }
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
  // Stop once the antecedent maximum reaches k.level; Kissat/CaDiCaL scan the
  // whole reason, so this only removes redundant reads.
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

// Retract the trail suffix from `position` and land at the root. Speculative
// rounds are not level-structured (Backbone stamps units at level zero), so
// this unwinds by position through the per-literal unassign; retracted
// assignments lose cached unit ids.
void kernel_unwind_speculation(Kernel &, uint32_t position);

// The one propagation-fixpoint predicate: the raw cursor is not enough while
// a binary-only debt remains.
inline bool kernel_propagation_fixpoint(const Kernel &k) {
  return k.propagate_head == k.trail.size() &&
         k.binary_only_head >= k.propagate_head;
}

bool kernel_propagate(Kernel &, Ref ignore = INVALID_REF);

// Walk warm-up propagation (kissat propbeyond.c): continue past conflicts,
// counting but never installing them; implications refresh saved phases.
void kernel_propagate_beyond_conflicts(Kernel &);

// `use_current_level`: chained Backbone assigns at the current level (Kissat).
bool kernel_propagate_binary(Kernel &, bool use_current_level = false);

void kernel_probing_begin(Kernel &, bool rollback_pose = true);
void kernel_probing_end(Kernel &); // idempotent; a no-op outside a scope
bool kernel_propagate_probing(Kernel &, Ref ignore = INVALID_REF);

// True while the open scope records pose snapshots it will restore; a
// `rollback_pose == false` scope records and restores nothing.
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

// `work_budget_bounded` distinguishes an explicit zero allowance from
// zero-as-unlimited. A completed atomic unit may overshoot the ceiling.
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

inline uint32_t kernel_root_fixed(const Kernel &k) {
  return k.level == 0 ? static_cast<uint32_t>(k.trail.size()) : k.frames[1];
}

// Root counts including archived fixed facts; compaction alone must not
// consume a new-root signal.
inline uint32_t kernel_logical_root_fixed(const Kernel &k) {
  return kernel_root_fixed(k) + k.fixed_external_count;
}

// The root is not a contiguous trail prefix: read the semantic level.
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

// Publish root literals from `from` onward that propagation derived.
void kernel_emit_root_units(Kernel &, size_t from);


// `watch_start` is Kissat's sparse-collection watermark: watches below it keep
// their order, clauses above are rewatched in arena order. `sparse_collection`
// distinguishes Reduce start=0 from a whole rebuild. `preserve_eager_recent`
// is for reclamation passes whose Kissat operation does not collect.
// Defragmentation (collect.h) inspects all three vector arenas: only after
// creating a clause, above 2^18 words, when more than 75 % unusable.
void kernel_defrag_vectors(Kernel &);

void kernel_collect_garbage(Kernel &, Ref watch_start = 0,
                            bool sparse_collection = false,
                            bool preserve_eager_recent = false);
// Kissat's null-stack dense entry: drop large watches, keep first binary copies.
void kernel_flush_large_watches(Kernel &);
// Substitute's watch-mode entry (substitute.c `substitute_binaries`): large
// watches leave, binary watches stay, duplicates included.
void kernel_drop_large_watches(Kernel &);
// Reconnect live large clauses in Kissat-equivalent arena order.
// `flush_eliminated` (Eliminate's resume) first garbages clauses satisfied at
// root or containing an eliminated variable.
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
// Append one irredundant large occurrence to every attached view (`large`
// and, under Eliminate's mixed stream, `ordered`).
void kernel_dense_add_large_occurrence(Kernel &, Lit, Ref);
// Remove one large occurrence from every attached view; `large` is exactly
// the large payloads of `ordered` in order.
void kernel_dense_remove_large_occurrence(Kernel &, Lit, Ref);
// Promote a live redundant large clause to irredundant and publish it to
// every attached dense view.
void kernel_promote_large_irredundant(Kernel &, Ref);
void kernel_dense_disconnect(Kernel &);
void kernel_flush_retired_redundant(Kernel &);

uint64_t kernel_irredundant_clauses(const Kernel &);
#ifndef NDEBUG
void kernel_check_binirr_counts(const Kernel &);
#endif

void kernel_check_invariants(const Kernel &);

} // namespace tactix
