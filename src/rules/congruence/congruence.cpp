#include "rules/congruence/congruence.hpp"

#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"   // inprocess_reach_root
#include "rules/substitute/substitute.hpp" // big_path_chain (bridged LRAT chains)

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef TACTIX_ENABLE_EVENT_TRACE
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace tactix {

namespace {

// Arity cap: an AND gate over more inputs is skipped (kissat caps too), which
// bounds extraction and the equivalence proof chain.
constexpr uint32_t CONG_ARITY_LIMIT = 64;
// safety cap on gates extracted per round.
constexpr size_t CONG_GATE_LIMIT = 20000000;
// Max inputs of an extracted XOR gate; the match proof chain is O(2^arity), so
// it stays small (kissat --congruencexorarity). An arity-a XOR is a size-(a+1)
// clause group, hence the 3..CONG_XOR_ARITY+1 scan range.
constexpr uint32_t CONG_XOR_ARITY = 4;

// --- the meter and the mid-round budget --------------------------------------
// `stats.congruence_ticks` is charged at every scan site, so the policy can
// price a round in the rule's own currency. The stop predicate is consulted
// only where `kernel_terminating` already is, inheriting that soundness
// argument; `work_budget == 0` is unbounded. Round-scoped state, Congruence
// being one canonical round with no reentrancy. Design
// `docs/design/2026-08-05-g1-congruence-governor-design.md` §4.
struct Budget {
  uint64_t start = 0;
  uint64_t limit = 0; // 0 = unbounded
  bool cut = false; // set at whichever stop actually fired; counted once/round
  bool over(const Kernel &k) {
    if (!limit || k.stats.congruence_ticks - start <= limit)
      return false;
    cut = true;
    return true;
  }
};
Budget cong_budget;
// Scoped so every exit path leaves the budget cleared for the next caller.
struct BudgetScope {
  const Kernel &k;
  explicit BudgetScope(const Kernel &kk) : k(kk) {
    cong_budget = {kk.stats.congruence_ticks, kk.work_budget};
  }
  ~BudgetScope() {
    if (cong_budget.cut)
      k.stats.congruence_budget_stops++; // once per cut round, not per site
    cong_budget = {};
  }
};

// Allocation-boundary tracing is compiled only into semantic-trace builds.
// Large formulas can finish parsing yet fail in one of Congruence's several
// dense working sets; a line-buffered stage record identifies the last
// completed allocation without adding a branch to production binaries.
#ifdef TACTIX_ENABLE_EVENT_TRACE
void congruence_stage_trace(const Kernel &k, const char *stage,
                            size_t first = 0, size_t second = 0,
                            size_t third = 0, size_t fourth = 0) {
  static FILE *out = nullptr;
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    const char *path = std::getenv("TACTIX_CONGRUENCE_MEMORY_TRACE");
    if (path && *path) {
      out = std::fopen(path, "w");
      if (out)
        std::setvbuf(out, nullptr, _IOLBF, 0);
    }
  }
  if (!out)
    return;
  struct rusage usage {};
  getrusage(RUSAGE_SELF, &usage);
  unsigned long virtual_pages = 0;
  if (FILE *statm = std::fopen("/proc/self/statm", "r")) {
    (void)std::fscanf(statm, "%lu", &virtual_pages);
    std::fclose(statm);
  }
  const unsigned long virtual_kib =
      virtual_pages * static_cast<unsigned long>(sysconf(_SC_PAGESIZE) / 1024);
  std::fprintf(out,
               "%s\trss_kib=%ld\tvm_kib=%lu\tvars=%u\tarena_words=%zu\t"
               "watch_words=%zu\twatch_capacity=%zu\tfirst=%zu\tsecond=%zu\t"
               "third=%zu\tfourth=%zu\n",
               stage, usage.ru_maxrss, virtual_kib, k.nvars, k.arena.size(),
               k.watches.arena.size(), k.watches.arena.capacity(), first,
               second, third, fourth);
}
#else
inline void congruence_stage_trace(const Kernel &, const char *, size_t = 0,
                                   size_t = 0, size_t = 0, size_t = 0) {}
#endif

// Mark the binary-watch partners of `lit`.
template <int V> void mark_bin_partners(const Kernel &k, Lit lit,
                                        std::vector<uint8_t> &mark) {
  for (const WatchEntry w : k.watches.entries(lit))
    if (w.binary())
      mark[w.other()] = V;
}

} // namespace

RuleEligibility congruence_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0);
}

void congruence_extract(const Kernel &k, std::vector<CongGate> &out) {
  const bool lrat = k.proof && k.proof->lrat();
  std::vector<uint8_t> mark(2 * k.nvars, 0); // binary partners of ¬output
  // AND gate `out = AND(inputs)`: a long base clause holding `out` where every
  // other literal l has the binary (¬out ∨ ¬l), so input = ¬l; OR is the dual,
  // found by trying every literal as the output. `pairs` is reused and the
  // `reserve`s exact — one malloc per gate is a measurable share of the solve.
  std::vector<std::pair<Lit, uint64_t>> pairs;
  const auto emit = [&](const Clause *c, Lit outp) {
    const Lit nout = negate(outp);
    CongGate g;
    g.output = outp;
    if (lrat)
      g.set_base_id(c->id());
    g.inputs.reserve(c->size - 1);
    if (lrat)
      g.reserve_input_bin_ids(c->size - 1);
    for (const Lit l : *c)
      if (l != outp) {
        const Lit inp = negate(l);
        g.inputs.push_back(inp);
        if (lrat)
          g.push_input_bin_id(kernel_binary_id(k, nout, inp));
      }
    // canonicalize inputs (the structural key) — keep bin_ids parallel
    if (lrat) {
      pairs.clear();
      pairs.resize(g.inputs.size());
      for (size_t i = 0; i < g.inputs.size(); i++)
        pairs[i] = {g.inputs[i], g.input_bin_id(i)};
      std::sort(pairs.begin(), pairs.end());
      for (size_t i = 0; i < pairs.size(); i++) {
        g.inputs[i] = pairs[i].first;
        g.input_bin_id(i) = pairs[i].second;
      }
    } else {
      std::sort(g.inputs.begin(), g.inputs.end());
    }
    out.push_back(std::move(g));
  };
  // Keep gates whose clauses contain root-assigned literals.  The clauses still
  // state the original relation, and the closure below simplifies that relation
  // with the live root values.  Dropping these gates here severs precisely the
  // value-driven cascade Congruence is meant to compute.
  if (k.dense) {
    // Literal-major over the phase occ index: mark ¬out's binary partners once
    // per output literal, not once per clause-occurrence. The tag sort below
    // restores the exact clause-major emission sequence — a hard constraint,
    // since merge order feeds the search trajectory.
    struct Tag {
      Ref ref;
      uint32_t idx; // outp's index within the clause
    };
    std::vector<Tag> tags;
    const size_t base = out.size();
    const Lit nlits = 2 * k.nvars;
    for (Lit outp = 0; outp < nlits; outp++) {
      // Read-only work: the gates already emitted stay valid, and the closure
      // simply sees fewer of them.
      if (!(outp & 255) && cong_budget.over(k))
        break;
      const std::vector<Ref> &occ = k.dense->large[outp];
      if (occ.empty())
        continue;
      const Lit nout = negate(outp);
      bool marked = false;
      k.stats.congruence_ticks += occ.size();
      for (const Ref ref : occ) {
        const Clause *c = clause_at(k.arena, ref);
        if (c->garbage() || c->redundant() || c->size < 3 ||
            c->size > CONG_ARITY_LIMIT + 1)
          continue;
        k.stats.congruence_ticks += c->size;
        // locate outp in the (still logically valid) original clause
        uint32_t idx = UINT32_MAX;
        for (uint32_t j = 0; j < c->size; j++) {
          if (c->lits[j] == outp) {
            idx = j;
            break;
          }
        }
        if (idx == UINT32_MAX)
          continue; // stale occurrence entry
        if (!marked) {
          mark_bin_partners<1>(k, nout, mark);
          marked = true;
        }
        bool all = true;
        for (const Lit l : *c) {
          if (l == outp)
            continue;
          if (!mark[negate(l)]) { // need binary (¬out ∨ ¬l)
            all = false;
            break;
          }
        }
        if (!all)
          continue;
        // Do not materialize the gate yet.  Literal-major discovery can find
        // far more than the clause-major cap before the sort below knows which
        // prefix survives.  Keeping just this eight-byte descriptor makes the
        // cap a real memory bound and avoids one tiny allocation per discarded
        // gate; the descriptor contains everything `emit` needs afterward.
        tags.push_back({ref, idx});
      }
      if (marked)
        mark_bin_partners<0>(k, nout, mark); // unmark once per literal
    }
    // restore clause-major order: (arena ref, candidate index within clause)
    std::sort(tags.begin(), tags.end(), [](const Tag &a, const Tag &b) {
      return a.ref != b.ref ? a.ref < b.ref : a.idx < b.idx;
    });
    const size_t remaining = base < CONG_GATE_LIMIT
                                 ? CONG_GATE_LIMIT - base
                                 : 0;
    // The cap is checked before each clause, so the final clause may contribute
    // at most CONG_ARITY_LIMIT+1 outputs beyond it.
    out.reserve(base + std::min(tags.size(),
                                remaining + CONG_ARITY_LIMIT + 1));
    // clause-major CONG_GATE_LIMIT semantics: checked before each new clause
    for (size_t i = 0; i < tags.size();) {
      if (out.size() >= CONG_GATE_LIMIT)
        break;
      const Ref r = tags[i].ref;
      const Clause *c = clause_at(k.arena, r);
      for (; i < tags.size() && tags[i].ref == r; i++) {
        assert(tags[i].idx < c->size);
        emit(c, c->lits[tags[i].idx]);
      }
    }
    return;
  }
  // Extraction is read-only, so aborting is free: the caller returns NoOp.
  uint32_t xtick = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    if (k.stop_fuzz || !(++xtick & 255)) {
      if (kernel_terminating(k)) {
        k.stats.rule_interrupted++;
        return;
      }
      if (cong_budget.over(k))
        return; // budget stop — read-only work, fewer gates, sound
    }
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    ref += words;
    if (c->garbage() || c->redundant() || c->size < 3 ||
        c->size > CONG_ARITY_LIMIT + 1)
      continue;
    if (out.size() >= CONG_GATE_LIMIT)
      return;
    k.stats.congruence_ticks += c->size;
    for (const Lit outp : *c) {
      const Lit nout = negate(outp);
      mark_bin_partners<1>(k, nout, mark);
      bool all = true;
      for (const Lit l : *c) {
        if (l == outp)
          continue;
        if (!mark[negate(l)]) { // need binary (¬out ∨ ¬l)
          all = false;
          break;
        }
      }
      mark_bin_partners<0>(k, nout, mark); // unmark
      if (!all)
        continue;
      emit(c, outp);
    }
  }
}

namespace {

// Closure maps an immutable canonical input key to a replaceable gate id.
// Keys live in one literal arena, records in a second vector, and a flat table
// stores 32-bit record indices. The table is never iterated, so bucket order
// cannot affect merge order; a 70% load bounds probes at the 20M-gate cap.
template <typename T, uint32_t BLOCK_SHIFT = 20> class PagedVector {
public:
  static constexpr size_t BLOCK_SIZE = size_t{1} << BLOCK_SHIFT;
  static constexpr size_t BLOCK_MASK = BLOCK_SIZE - 1;

  void reserve(size_t entries) {
    blocks_.reserve((entries + BLOCK_SIZE - 1) / BLOCK_SIZE);
  }
  void push_back(const T &value) {
    const size_t block = size_ >> BLOCK_SHIFT;
    if (block == blocks_.size())
      blocks_.emplace_back(new T[BLOCK_SIZE]);
    blocks_[block][size_ & BLOCK_MASK] = value;
    size_++;
  }
  T &operator[](size_t index) {
    assert(index < size_);
    return blocks_[index >> BLOCK_SHIFT][index & BLOCK_MASK];
  }
  const T &operator[](size_t index) const {
    assert(index < size_);
    return blocks_[index >> BLOCK_SHIFT][index & BLOCK_MASK];
  }
  size_t size() const { return size_; }
  size_t capacity() const { return blocks_.size() * BLOCK_SIZE; }

private:
  std::vector<std::unique_ptr<T[]>> blocks_;
  size_t size_ = 0;
};

// Direct literal indexing without one monolithic nlits-word allocation.  A
// missing page is semantically all EMPTY, and pages appear only when closure
// registration first touches one of their literals.  Fine-grained pages retain
// O(1) lookup while avoiding dense commitment for untouched literal regions.
class PagedLiteralTails {
public:
  static constexpr uint32_t EMPTY = UINT32_MAX;
  static constexpr uint32_t BLOCK_SHIFT = 12;
  static constexpr size_t BLOCK_SIZE = size_t{1} << BLOCK_SHIFT;
  static constexpr size_t BLOCK_MASK = BLOCK_SIZE - 1;

  explicit PagedLiteralTails(size_t literals = 0)
      : blocks_((literals + BLOCK_SIZE - 1) / BLOCK_SIZE) {}
  uint32_t get(Lit lit) const {
    const auto &block = blocks_[lit >> BLOCK_SHIFT];
    return block ? block[lit & BLOCK_MASK] : EMPTY;
  }
  uint32_t &get_or_create(Lit lit) {
    auto &block = blocks_[lit >> BLOCK_SHIFT];
    if (!block) {
      block.reset(new uint32_t[BLOCK_SIZE]);
      std::fill(block.get(), block.get() + BLOCK_SIZE, EMPTY);
      allocated_blocks_++;
    }
    return block[lit & BLOCK_MASK];
  }
  size_t directory_size() const { return blocks_.size(); }
  size_t allocated_blocks() const { return allocated_blocks_; }

private:
  std::vector<std::unique_ptr<uint32_t[]>> blocks_;
  size_t allocated_blocks_ = 0;
};

// Dense import ranks form a permutation of the current variables.  Store that
// inverse permutation at the information-theoretic word width instead of four
// bytes per variable.  After compaction leaves holes in permanent ranks, sort
// the packed variables in place with an MSD radix permutation: ranks are
// unique, so this yields the same exact total order without a second array.
class CompactVarOrder {
public:
  CompactVarOrder() = default;
  explicit CompactVarOrder(const Kernel &k) : size_(k.nvars) {
    bits_ = 1;
    while ((uint64_t{1} << bits_) < std::max<uint64_t>(k.nvars, 2))
      bits_++;
    const uint64_t total_bits = static_cast<uint64_t>(k.nvars) * bits_;
    const size_t word_count = (total_bits + 63) / 64 + 1;
    // A single packed vector is compact but still asks malloc for one
    // 150+ MiB contiguous extent on the largest Tier-D inputs. Congruence has
    // already built millions of variable-sized gate records by this point, so
    // that request can fail under the 16 GiB debug envelope even when the
    // aggregate bytes fit. Fixed-size pages retain the same bit layout and
    // random access while bounding each allocator request.
    words_.reserve(word_count);
    for (size_t word = 0; word < word_count; word++)
      words_.push_back(0);
    congruence_stage_trace(k, "binary-order-storage-end", word_count, bits_);
    if (k.next_import_rank == k.nvars)
      for (Var variable = 0; variable < k.nvars; variable++) {
        const uint64_t rank = kernel_import_var_order(k, variable);
        assert(rank < k.nvars);
        set(rank, variable);
      }
    else {
      for (Var variable = 0; variable < k.nvars; variable++)
        set(variable, variable);
      uint32_t maximum_rank = k.next_import_rank ? k.next_import_rank - 1 : 0;
      int shift = 0;
      while (static_cast<uint64_t>(maximum_rank) >>
             (shift + RADIX_BITS))
        shift += RADIX_BITS;
      radix_sort(k, 0, k.nvars, shift);
    }
#ifndef NDEBUG
    for (size_t i = 1; i < size_; i++)
      assert(kernel_import_var_order(k, (*this)[i - 1]) <
             kernel_import_var_order(k, (*this)[i]));
#endif
    congruence_stage_trace(k, "binary-order-fill-end", size_, bits_);
  }
  size_t size() const { return size_; }
  Var operator[](size_t index) const {
    const uint64_t bit = static_cast<uint64_t>(index) * bits_;
    const size_t word = bit >> 6;
    const uint32_t shift = bit & 63;
    uint64_t value = words_[word] >> shift;
    if (shift + bits_ > 64)
      value |= words_[word + 1] << (64 - shift);
    return static_cast<Var>(value & ((uint64_t{1} << bits_) - 1));
  }
  uint32_t bits() const { return bits_; }

private:
  static constexpr uint32_t RADIX_BITS = 11;
  static constexpr uint32_t RADIX_SIZE = 1u << RADIX_BITS;
  static constexpr uint32_t RADIX_MASK = RADIX_SIZE - 1;
  void set(size_t index, Var variable) {
    const uint64_t bit = static_cast<uint64_t>(index) * bits_;
    const size_t word = bit >> 6;
    const uint32_t shift = bit & 63;
    const uint64_t value_mask = (uint64_t{1} << bits_) - 1;
    words_[word] = (words_[word] & ~(value_mask << shift)) |
                   (static_cast<uint64_t>(variable) << shift);
    if (shift + bits_ > 64) {
      const uint32_t high_bits = shift + bits_ - 64;
      const uint64_t high_mask = (uint64_t{1} << high_bits) - 1;
      words_[word + 1] =
          (words_[word + 1] & ~high_mask) |
          (static_cast<uint64_t>(variable) >> (64 - shift));
    }
  }
  void swap(size_t a, size_t b) {
    const Var av = (*this)[a], bv = (*this)[b];
    set(a, bv);
    set(b, av);
  }
  uint32_t digit(const Kernel &k, Var variable, int shift) const {
    return static_cast<uint32_t>(kernel_import_var_order(k, variable) >> shift) &
           RADIX_MASK;
  }
  void radix_sort(const Kernel &k, uint32_t begin, uint32_t end, int shift) {
    if (end - begin < 2)
      return;
    std::array<uint32_t, RADIX_SIZE> counts{}, starts{}, next{};
    for (uint32_t i = begin; i < end; i++)
      counts[digit(k, (*this)[i], shift)]++;
    uint32_t position = begin;
    for (uint32_t bucket = 0; bucket < RADIX_SIZE; bucket++) {
      starts[bucket] = next[bucket] = position;
      position += counts[bucket];
    }
    for (uint32_t bucket = 0; bucket < RADIX_SIZE; bucket++) {
      const uint32_t bucket_end = starts[bucket] + counts[bucket];
      while (next[bucket] < bucket_end) {
        const uint32_t target = digit(k, (*this)[next[bucket]], shift);
        if (target == bucket)
          next[bucket]++;
        else
          swap(next[bucket], next[target]++);
      }
    }
    if (shift >= static_cast<int>(RADIX_BITS))
      for (uint32_t bucket = 0; bucket < RADIX_SIZE; bucket++)
        if (counts[bucket] > 1)
          radix_sort(k, starts[bucket], starts[bucket] + counts[bucket],
                     shift - RADIX_BITS);
  }
  PagedVector<uint64_t> words_;
  size_t size_ = 0;
  uint32_t bits_ = 0;
};

// Sparse literal -> insertion-ordered Ref list.  The forward congruence pass
// never iterates map keys; it only looks up a literal, scans that literal's
// Refs in insertion order, and appends one Ref.  A node-based unordered_map of
// heap-backed vectors spends multiple allocations per selected literal.  This
// flat index stores one 32-bit entry id per bucket and paged list nodes, while
// preserving the only observable order.
class SparseRefOccurrences {
public:
  static constexpr uint32_t EMPTY = UINT32_MAX;

  void reserve(size_t maximum_nodes) {
    entries_.reserve(maximum_nodes);
    nodes_.reserve(maximum_nodes);
    size_t capacity = 64;
    while (maximum_nodes * 10 > capacity * 7)
      capacity *= 2;
    buckets_.assign(capacity, EMPTY);
  }
  uint32_t find(Lit lit) const {
    if (buckets_.empty())
      return EMPTY;
    const size_t mask = buckets_.size() - 1;
    size_t pos = hash_lit(lit) & mask;
    while (buckets_[pos] != EMPTY) {
      const uint32_t entry = buckets_[pos];
      if (entries_[entry].lit == lit)
        return entry;
      pos = (pos + 1) & mask;
    }
    return EMPTY;
  }
  void push_back(Lit lit, Ref ref) {
    if (buckets_.empty())
      buckets_.assign(64, EMPTY);
    if ((entries_.size() + 1) * 10 > buckets_.size() * 7)
      rehash(2 * buckets_.size());
    const size_t mask = buckets_.size() - 1;
    size_t pos = hash_lit(lit) & mask;
    while (buckets_[pos] != EMPTY &&
           entries_[buckets_[pos]].lit != lit)
      pos = (pos + 1) & mask;
    uint32_t entry = buckets_[pos];
    if (entry == EMPTY) {
      assert(entries_.size() < EMPTY);
      entry = static_cast<uint32_t>(entries_.size());
      entries_.push_back({lit, EMPTY, EMPTY, 0});
      buckets_[pos] = entry;
    }
    assert(nodes_.size() < EMPTY);
    const uint32_t node = static_cast<uint32_t>(nodes_.size());
    nodes_.push_back({ref, EMPTY});
    Entry &e = entries_[entry];
    if (e.tail == EMPTY)
      e.head = node;
    else
      nodes_[e.tail].next = node;
    e.tail = node;
    e.count++;
  }

  uint32_t first(uint32_t entry) const {
    assert(entry < entries_.size());
    return entries_[entry].head;
  }
  uint32_t next(uint32_t node) const {
    assert(node < nodes_.size());
    return nodes_[node].next;
  }
  Ref ref(uint32_t node) const {
    assert(node < nodes_.size());
    return nodes_[node].ref;
  }
  size_t count(uint32_t entry) const {
    assert(entry < entries_.size());
    return entries_[entry].count;
  }

private:
  struct Entry {
    Lit lit;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
  };
  struct Node {
    Ref ref;
    uint32_t next;
  };
  static size_t hash_lit(Lit lit) {
    uint64_t h = lit;
    h ^= h >> 16;
    h *= UINT64_C(0x7feb352d);
    h ^= h >> 15;
    h *= UINT64_C(0x846ca68b);
    h ^= h >> 16;
    return static_cast<size_t>(h);
  }
  void rehash(size_t capacity) {
    std::vector<uint32_t> replacement(capacity, EMPTY);
    const size_t mask = capacity - 1;
    for (uint32_t entry = 0; entry < entries_.size(); entry++) {
      size_t pos = hash_lit(entries_[entry].lit) & mask;
      while (replacement[pos] != EMPTY)
        pos = (pos + 1) & mask;
      replacement[pos] = entry;
    }
    buckets_.swap(replacement);
  }

  std::vector<uint32_t> buckets_;
  PagedVector<Entry> entries_;
  PagedVector<Node> nodes_;
};

class ClosureTable {
public:
  struct Lookup {
    uint32_t record;
    bool inserted;
  };

  void reserve(size_t entries, size_t key_literals) {
    record_blocks_.reserve((entries + RECORD_BLOCK_SIZE - 1) /
                           RECORD_BLOCK_SIZE);
    literal_blocks_.reserve((key_literals + LITERAL_BLOCK_SIZE - 1) /
                            LITERAL_BLOCK_SIZE);
    size_t buckets = 64;
    while (entries * 10 > buckets * 7)
      buckets *= 2;
    rehash(buckets);
  }

  Lookup lookup_or_insert(const std::vector<Lit> &key, uint32_t gate) {
    if (buckets_.empty())
      rehash(64);
    const size_t hash = hash_key(key.data(), key.size());
    size_t pos = find_slot(key, hash);
    if (buckets_[pos] != EMPTY)
      return {buckets_[pos], false};

    if ((static_cast<size_t>(record_count_) + 1) * 10 >
        buckets_.size() * 7) {
      rehash(2 * buckets_.size());
      pos = find_slot(key, hash);
      assert(buckets_[pos] == EMPTY);
    }
    assert(record_count_ < EMPTY);
    assert(key.size() <= UINT32_MAX);
    const uint32_t begin = append_key(key);
    const uint32_t record = record_count_++;
    ensure_record_block(record);
    record_at(record) = {begin, static_cast<uint32_t>(key.size()), gate};
    buckets_[pos] = record;
    return {record, true};
  }

  uint32_t gate(uint32_t record) const {
    assert(record < record_count_);
    return record_at(record).gate;
  }
  void replace(uint32_t record, uint32_t gate) {
    assert(record < record_count_);
    record_at(record).gate = gate;
  }
  size_t size() const { return record_count_; }
  size_t bucket_count() const { return buckets_.size(); }
  size_t literal_count() const { return literal_count_; }

private:
  static constexpr uint32_t EMPTY = UINT32_MAX;
  // Blocks are large enough to amortize allocator metadata and small enough
  // that one re-key never asks the allocator to duplicate a multi-hundred-MiB
  // vector.  A key never crosses a literal block, so comparisons stay a single
  // contiguous scan.
  static constexpr uint32_t RECORD_BLOCK_SHIFT = 20;
  static constexpr uint32_t RECORD_BLOCK_SIZE = 1u << RECORD_BLOCK_SHIFT;
  static constexpr uint32_t RECORD_BLOCK_MASK = RECORD_BLOCK_SIZE - 1;
  static constexpr uint32_t LITERAL_BLOCK_SHIFT = 22;
  static constexpr uint32_t LITERAL_BLOCK_SIZE = 1u << LITERAL_BLOCK_SHIFT;
  static constexpr uint32_t LITERAL_BLOCK_MASK = LITERAL_BLOCK_SIZE - 1;
  struct Record {
    uint32_t begin;
    uint32_t size;
    uint32_t gate;
  };

  static size_t hash_key(const Lit *begin, size_t size) {
    size_t h = 1469598103934665603ull;
    for (size_t i = 0; i < size; i++) {
      h ^= static_cast<size_t>(begin[i]);
      h *= 1099511628211ull;
    }
    // FNV's low bits are weak for power-of-two linear probing.  This avalanche
    // changes bucket placement only; equality and processing order are intact.
    h ^= h >> 33;
    h *= static_cast<size_t>(0xff51afd7ed558ccdull);
    h ^= h >> 29;
    return h;
  }

  bool equal(uint32_t record, const std::vector<Lit> &key) const {
    const Record &r = record_at(record);
    if (r.size != key.size())
      return false;
    const Lit *stored = literal_at(r.begin);
    for (uint32_t i = 0; i < r.size; i++)
      if (stored[i] != key[i])
        return false;
    return true;
  }

  Record &record_at(uint32_t record) {
    return record_blocks_[record >> RECORD_BLOCK_SHIFT]
                         [record & RECORD_BLOCK_MASK];
  }
  const Record &record_at(uint32_t record) const {
    return record_blocks_[record >> RECORD_BLOCK_SHIFT]
                         [record & RECORD_BLOCK_MASK];
  }
  const Lit *literal_at(uint32_t begin) const {
    return literal_blocks_[begin >> LITERAL_BLOCK_SHIFT].get() +
           (begin & LITERAL_BLOCK_MASK);
  }
  void ensure_record_block(uint32_t record) {
    const size_t needed = (record >> RECORD_BLOCK_SHIFT) + 1;
    while (record_blocks_.size() < needed)
      record_blocks_.emplace_back(new Record[RECORD_BLOCK_SIZE]);
  }
  void ensure_literal_block(uint32_t begin) {
    const size_t needed = (begin >> LITERAL_BLOCK_SHIFT) + 1;
    while (literal_blocks_.size() < needed)
      literal_blocks_.emplace_back(new Lit[LITERAL_BLOCK_SIZE]);
  }
  uint32_t append_key(const std::vector<Lit> &key) {
    assert(key.size() <= LITERAL_BLOCK_SIZE);
    uint64_t begin = literal_slots_;
    const uint32_t offset = static_cast<uint32_t>(begin) & LITERAL_BLOCK_MASK;
    if (offset + key.size() > LITERAL_BLOCK_SIZE)
      begin = (begin + LITERAL_BLOCK_MASK) &
              ~static_cast<uint64_t>(LITERAL_BLOCK_MASK);
    assert(begin + key.size() <= UINT32_MAX);
    ensure_literal_block(static_cast<uint32_t>(begin));
    Lit *target = literal_blocks_[begin >> LITERAL_BLOCK_SHIFT].get() +
                  (begin & LITERAL_BLOCK_MASK);
    std::copy(key.begin(), key.end(), target);
    literal_slots_ = begin + key.size();
    literal_count_ += key.size();
    return static_cast<uint32_t>(begin);
  }

  size_t find_slot(const std::vector<Lit> &key, size_t hash) const {
    assert(!buckets_.empty());
    const size_t mask = buckets_.size() - 1;
    size_t pos = hash & mask;
    while (buckets_[pos] != EMPTY && !equal(buckets_[pos], key))
      pos = (pos + 1) & mask;
    return pos;
  }

  void rehash(size_t capacity) {
    assert(capacity >= 64 && !(capacity & (capacity - 1)));
    std::vector<uint32_t> replacement(capacity, EMPTY);
    const size_t mask = capacity - 1;
    for (uint32_t record = 0; record < record_count_; record++) {
      const Record &r = record_at(record);
      size_t pos = hash_key(literal_at(r.begin), r.size) & mask;
      while (replacement[pos] != EMPTY)
        pos = (pos + 1) & mask;
      replacement[pos] = record;
    }
    buckets_.swap(replacement);
  }

  std::vector<uint32_t> buckets_;
  std::vector<std::unique_ptr<Record[]>> record_blocks_;
  std::vector<std::unique_ptr<Lit[]>> literal_blocks_;
  uint32_t record_count_ = 0;
  uint64_t literal_slots_ = 0;
  size_t literal_count_ = 0;
};

// Fixed-capacity key for the small-clause index and the XOR/ITE dedup sets: no
// per-key heap node, no vector hashing. Semantics match a std::vector key's — a
// sorted key strictly increases, so the zero padding never aliases a literal.
struct SmallKey {
  Lit lits[CONG_XOR_ARITY + 1] = {0, 0, 0, 0, 0};
  uint32_t size = 0;
  bool operator==(const SmallKey &o) const {
    if (size != o.size)
      return false;
    for (uint32_t i = 0; i < CONG_XOR_ARITY + 1; i++)
      if (lits[i] != o.lits[i])
        return false;
    return true;
  }
  bool operator<(const SmallKey &o) const {
    for (uint32_t i = 0; i < CONG_XOR_ARITY + 1; i++)
      if (lits[i] != o.lits[i])
        return lits[i] < o.lits[i];
    return size < o.size; // unreachable under the strict-increase invariant
  }
};
struct SmallKeyHash {
  size_t operator()(const SmallKey &sk) const {
    size_t h = 1469598103934665603ull;
    for (uint32_t i = 0; i < CONG_XOR_ARITY + 1; i++) {
      h ^= static_cast<size_t>(sk.lits[i]);
      h *= 1099511628211ull;
    }
    h ^= sk.size;
    h *= 1099511628211ull;
    return h;
  }
};
// sorted key from ≤ CONG_XOR_ARITY+1 literals
inline SmallKey small_key(const Lit *ls, uint32_t n) {
  SmallKey sk;
  sk.size = n;
  for (uint32_t i = 0; i < n; i++) {
    const Lit x = ls[i];
    uint32_t j = i;
    for (; j > 0 && sk.lits[j - 1] > x; j--)
      sk.lits[j] = sk.lits[j - 1];
    sk.lits[j] = x;
  }
  return sk;
}

// Flat open-addressed SmallKey set — the clause index and both extraction dedup
// sets. A set, not a map: XOR and ITE only ask "does this clause exist", and
// keys live in the bucket array so a lookup touches one cache line. `size == 0`
// is the empty marker; iteration order is hash-dependent, see `sorted_keys`.
class SmallKeySet {
public:
  SmallKeySet() : buckets_(64), mask_(63) {}
  size_t size() const { return size_; }
  const std::vector<SmallKey> &buckets() const { return buckets_; }
  bool contains(const SmallKey &sk) const {
    return buckets_[slot(sk)].size != 0;
  }
  // true iff `sk` was not already present (std::unordered_set::insert().second)
  bool insert(const SmallKey &sk) {
    if ((size_ + 1) * 10 > (static_cast<size_t>(mask_) + 1) * 7)
      grow();
    const uint32_t b = slot(sk);
    if (buckets_[b].size != 0)
      return false;
    buckets_[b] = sk;
    size_++;
    return true;
  }

private:
  // FNV's low bits mix poorly across key positions, which linear probing
  // punishes; finalize with the murmur3 avalanche. Bucket placement only.
  static uint32_t bucket_hash(const SmallKey &sk) {
    uint64_t h = SmallKeyHash{}(sk);
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdull;
    h ^= h >> 29;
    return static_cast<uint32_t>(h);
  }
  // the bucket holding `sk`, or the empty one it would take (load < 70 %, so
  // the probe always terminates)
  uint32_t slot(const SmallKey &sk) const {
    uint32_t b = bucket_hash(sk) & mask_;
    while (buckets_[b].size != 0 && !(buckets_[b] == sk))
      b = (b + 1) & mask_;
    return b;
  }
  void grow() {
    std::vector<SmallKey> old;
    old.swap(buckets_);
    buckets_.assign(2 * old.size(), SmallKey{});
    mask_ = static_cast<uint32_t>(buckets_.size() - 1);
    for (const SmallKey &sk : old)
      if (sk.size != 0)
        buckets_[slot(sk)] = sk;
  }
  std::vector<SmallKey> buckets_;
  uint32_t mask_;
  size_t size_ = 0;
};

// Full small-clause membership used by XOR/ITE extraction.  The old open
// addressing table stored a 24-byte key in every bucket and temporarily held
// both tables while growing; tens of millions of ternaries therefore needed
// several GiB merely to answer membership queries.  A sorted unique compact
// record stores the common first three literals inline and only the optional
// fourth/fifth literals in a paged tail.  It is the same mathematical set and
// canonical iteration order without a full padded key per clause.
class ClauseIndex {
  static constexpr uint32_t OFFSET_BITS = 30;
  static constexpr uint32_t OFFSET_MASK = (uint32_t{1} << OFFSET_BITS) - 1;
  struct Record {
    Lit lits[3];
    uint32_t meta;
  };
  static_assert(sizeof(Record) == 16);

public:
  void reserve(size_t n) { records_.reserve(n); }
  void insert(const SmallKey &key) {
    assert(key.size >= 3 && key.size <= CONG_XOR_ARITY + 1);
    assert(tails_.size() + key.size - 3 <= OFFSET_MASK);
    Record record{{key.lits[0], key.lits[1], key.lits[2]},
                  static_cast<uint32_t>(tails_.size()) |
                      ((key.size - 3) << OFFSET_BITS)};
    for (uint32_t i = 3; i < key.size; i++)
      tails_.push_back(key.lits[i]);
    records_.push_back(record);
  }
  void finish() {
    std::sort(records_.begin(), records_.end(),
              [&](const Record &a, const Record &b) {
                return compare(a, b) < 0;
              });
    records_.erase(
        std::unique(records_.begin(), records_.end(),
                    [&](const Record &a, const Record &b) {
                      return compare(a, b) == 0;
                    }),
        records_.end());
  }
  bool contains(const SmallKey &key) const {
    const auto it = std::lower_bound(
        records_.begin(), records_.end(), key,
        [&](const Record &record, const SmallKey &wanted) {
          return compare(record, wanted) < 0;
        });
    return it != records_.end() && compare(*it, key) == 0;
  }
  size_t size() const { return records_.size(); }

  class Iterator {
  public:
    Iterator(const ClauseIndex *index, size_t position)
        : index_(index), position_(position) {}
    SmallKey operator*() const { return index_->key(records(position_)); }
    Iterator &operator++() {
      position_++;
      return *this;
    }
    bool operator!=(const Iterator &other) const {
      return position_ != other.position_;
    }

  private:
    const Record &records(size_t position) const {
      return index_->records_[position];
    }
    const ClauseIndex *index_;
    size_t position_;
  };
  Iterator begin() const { return Iterator(this, 0); }
  Iterator end() const { return Iterator(this, records_.size()); }

  // Preserve the old work-meter denomination.  SmallKeySet started at 64
  // buckets and doubled just before crossing 70 percent load; sorted_keys
  // charged one visit per bucket plus one per stored key.
  size_t legacy_bucket_count() const {
    size_t buckets = 64;
    while (records_.size() * 10 > buckets * 7)
      buckets *= 2;
    return buckets;
  }

private:
  uint32_t size(const Record &record) const {
    return 3 + (record.meta >> OFFSET_BITS);
  }
  uint32_t offset(const Record &record) const {
    return record.meta & OFFSET_MASK;
  }
  Lit literal(const Record &record, uint32_t position) const {
    if (position < 3)
      return record.lits[position];
    if (position >= size(record))
      return 0;
    return tails_[offset(record) + position - 3];
  }
  int compare(const Record &a, const Record &b) const {
    for (uint32_t i = 0; i < CONG_XOR_ARITY + 1; i++) {
      const Lit alit = literal(a, i), blit = literal(b, i);
      if (alit != blit)
        return alit < blit ? -1 : 1;
    }
    const uint32_t asize = size(a), bsize = size(b);
    return asize == bsize ? 0 : (asize < bsize ? -1 : 1);
  }
  int compare(const Record &record, const SmallKey &key) const {
    for (uint32_t i = 0; i < CONG_XOR_ARITY + 1; i++) {
      const Lit stored = literal(record, i);
      if (stored != key.lits[i])
        return stored < key.lits[i] ? -1 : 1;
    }
    const uint32_t stored_size = size(record);
    return stored_size == key.size ? 0 : (stored_size < key.size ? -1 : 1);
  }
  SmallKey key(const Record &record) const {
    SmallKey result;
    result.size = size(record);
    for (uint32_t i = 0; i < result.size; i++)
      result.lits[i] = literal(record, i);
    return result;
  }

  std::vector<Record> records_;
  PagedVector<Lit> tails_;
};

// Iterating a hash table is implementation-ordered, and gate emission order
// feeds the closure's merge order and hence the search trajectory, which must
// not fork across toolchains. `operator<` totally orders stored keys, so this
// is independent of the table's. Built once per apply, by value.
static const ClauseIndex &sorted_keys(const Kernel &k,
                                      const ClauseIndex &index) {
  k.stats.congruence_ticks += index.legacy_bucket_count() + index.size();
  return index;
}

// Index every irredundant clause of size 3..CONG_XOR_ARITY+1 by its sorted
// literals, including clauses currently satisfied or shortened by root values:
// while present they still justify the original XOR/ITE relation, which the
// closure simplifies under those values.  Gate clauses are never rewritten
// within an apply, so one snapshot is valid for the whole closure.
void build_clause_index(const Kernel &k, ClauseIndex &index) {
  std::vector<Lit> ls;
  // Every indexed clause has at least three literals and therefore at least
  // eight arena words.  Reserving this exact structural upper bound prevents a
  // second full vector from coexisting during geometric growth.
  index.reserve(k.arena.size() / clause_words(3));
  // Extraction is read-only, so aborting is free: the caller returns NoOp.
  uint32_t xtick = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    if (k.stop_fuzz || !(++xtick & 255)) {
      if (kernel_terminating(k)) {
        k.stats.rule_interrupted++;
        return;
      }
      if (cong_budget.over(k))
        return; // a truncated index only ever loses gates: XOR and ITE both
                // require every defining clause to be present, so a missing
                // entry can suppress a gate but never fabricate one.
    }
    const Clause *c = clause_at(k.arena, ref);
    ref += clause_words(c->size);
    if (c->garbage() || c->redundant() || c->size < 3 ||
        c->size > CONG_XOR_ARITY + 1)
      continue;
    k.stats.congruence_ticks += c->size;
    ls.clear();
    for (const Lit l : *c)
      ls.push_back(l);
    index.insert(small_key(ls.data(), static_cast<uint32_t>(ls.size())));
  }
  index.finish();
}

// A size-s clause group over {v0..v_{s-1}} is the XOR relation v0⊕…⊕v_{s-1} = c
// only when all 2^{s-1} clauses of one fixed sign parity are present; less is
// not the relation, and the gate would be unsound (`docs/calculus.md` V6). It
// yields s gates, one per output vᵢ, with the other variables as inputs.
void congruence_extract_xor(const Kernel &k, const ClauseIndex &index,
                            const ClauseIndex &sorted,
                            std::vector<CongGate> &out) {
  (void)k;
  SmallKeySet seen;     // by sorted positive vars
  std::vector<Lit> pos; // positive lits, sorted
  Lit probe[CONG_XOR_ARITY + 1];
  uint32_t xtick = 0;
  for (const SmallKey &base : sorted) {
    if (k.stop_fuzz || !(++xtick & 255)) {
      if (kernel_terminating(k)) {
        k.stats.rule_interrupted++;
        return;
      }
      if (cong_budget.over(k))
        return; // budget stop — read-only, fewer gates
    }
    const uint32_t s = base.size;
    k.stats.congruence_ticks += s;
    // positive-lit form of the variables, sorted; dedup a relation to once.
    pos.clear();
    int base_parity = 0;
    for (uint32_t bi = 0; bi < s; bi++) {
      const Lit l = base.lits[bi];
      pos.push_back(make_lit(var_of(l), false));
      base_parity ^= static_cast<int>(negated(l));
    }
    std::sort(pos.begin(), pos.end());
    bool dup = false; // a repeated variable ⇒ not a clean relation
    for (uint32_t i = 1; i < s; i++)
      if (pos[i] == pos[i - 1]) {
        dup = true;
        break;
      }
    if (dup)
      continue;
    const SmallKey pos_key = small_key(pos.data(), s);
    if (seen.contains(pos_key))
      continue;
    // require all 2^{s-1} clauses of parity == base_parity to exist.
    bool all = true;
    const uint32_t combos = 1u << (s - 1);
    k.stats.congruence_ticks += combos;
    for (uint32_t m = 0; m < combos && all; m++) {
      int par = 0;
      for (uint32_t i = 0; i + 1 < s; i++) {
        const int sign = (m >> i) & 1;
        par ^= sign;
        probe[i] = pos[i] ^ static_cast<Lit>(sign);
      }
      probe[s - 1] = pos[s - 1] ^ static_cast<Lit>(base_parity ^ par); // fix parity
      if (!index.contains(small_key(probe, s)))
        all = false;
    }
    if (!all)
      continue;
    seen.insert(pos_key);
    // The relation's parity R = base_parity ⊕ 1, so for output vᵢ the output
    // literal is pos[i] flipped by R. Never also XOR in `s`: that negates every
    // even-arity gate's output, invisibly to XOR↔XOR key matching but not to any
    // unit/merge channel reading the output's polarity.
    const Lit r = static_cast<Lit>(base_parity ^ 1);
    for (uint32_t i = 0; i < s; i++) {
      CongGate g;
      g.kind = GateKind::Xor;
      g.output = pos[i] ^ r; // value = R ⊕ vᵢ = ⊕_{j≠i} vⱼ
      g.inputs.reserve(s - 1); // exact: push_back's growth costs 2-3 mallocs
      for (uint32_t j = 0; j < s; j++)
        if (j != i)
          g.inputs.push_back(pos[j]); // all positive, already sorted
      out.push_back(std::move(g));
    }
  }
}

bool normalize_ite(Lit &cond, Lit &then_l, Lit &else_l); // defined below

// A ladder lemma: RUP-checkable, cited by the level below it, dead once the
// conclusion is in, and kept so the round can `del` it. LRAT deletes by id;
// DRAT needs the literals.
struct TransientLemma {
  uint64_t id;
  std::vector<Lit> lits;
};

// Drop the transient ladder lemmas, newest first.
void delete_transients(Kernel &k, std::vector<TransientLemma> &t) {
  if (k.proof)
    for (size_t i = t.size(); i-- > 0;)
      k.proof->del(k.next_clause_id, t[i].id, t[i].lits.data(),
                   static_cast<uint32_t>(t[i].lits.size()));
  t.clear();
}

// TACTIX_CONG_PROBE: count the degenerate-gate discards by cause — the
// inferences this rule does not draw. Env-gated, default-identical.
static const bool cong_probe = std::getenv("TACTIX_CONG_PROBE") != nullptr;
static uint64_t cong_n_and = 0, cong_n_ite = 0, cong_n_xor = 0, cong_n_clash = 0;
// Accepted merges whose representative is already root-assigned — see val_merge.
static uint64_t cong_val_merge = 0;
// Root-assigned literals seen by the closure at apply entry / exit.
static uint64_t cong_root_in = 0, cong_root_out = 0;

// TACTIX_CONG_PROBE, part 2 — the closure/matching discriminator, per gate
// kind: a funnel from canon_ok through insert / collide to merged, with stale,
// already_eq, clash and chain_fail the ways a collision fails to merge.
// `collide` ≈ 0 against a large `insert` puts the fault in canonicalization; a
// large `collide` with a small `merged` puts it downstream.
struct CongMatchStats {
  uint64_t canon_ok = 0, canon_skip = 0, canon_unit = 0, canon_merge = 0;
  uint64_t insert = 0, collide = 0, stale = 0, already_eq = 0;
  uint64_t clash = 0, chain_fail = 0, merged = 0;
  // The missing value channel: kissat's closure keeps `values[]` live, shrinking
  // a valued gate and turning a merge with a valued participant into a unit.
  // This closure reads no value, so these columns size the gap.
  uint64_t val_gate = 0, val_merge = 0;
};
static CongMatchStats cong_match[3]; // indexed by GateKind

// TACTIX_CONG_TIME: where one application's wall clock goes, by stage — the
// split that says which currency a governor should charge.
static const bool cong_time = std::getenv("TACTIX_CONG_TIME") != nullptr;
struct CongTime {
  uint64_t extract_and = 0, index = 0, sort = 0, xor_x = 0, ite_x = 0;
  uint64_t rebuild = 0, binary_extract = 0, binary_units = 0, setup = 0;
  uint64_t binary_import = 0, canon = 0, table = 0, proof = 0;
  uint64_t closure_all = 0, forward = 0, apply_all = 0;
  uint64_t items = 0;
};
static CongTime cong_t;
// RAII stage accumulator; when the probe is off it never reads the clock.
struct Stage {
  uint64_t *sink;
  uint64_t t0;
  explicit Stage(uint64_t *s) : sink(s), t0(cong_time ? now_nanos() : 0) {}
  ~Stage() {
    if (cong_time)
      *sink += now_nanos() - t0;
  }
};

// What canonicalizing a gate under the current representatives yielded. A
// degenerate gate is not a dead end: each degeneracy is itself an inference
// over F (`docs/calculus.md` V6).
enum class Canon {
  Ok,    // `key` + `out_lit` valid — file the gate in the table
  Unit,  // the gate forces `out_lit` to a constant: the root unit is ¬out_lit
  InputUnit, // `key[0]` is a root unit forced by a valued gate
  Merge, // the gate collapsed to out_lit ≡ key[0] — no second gate needed
  Skip,  // degenerate in a way this rule does not exploit (ITE)
};

// Canonicalize gate g under the current union-find into (key, out_lit): key the
// structural hash, out_lit the literal whose class is merged on a key collision
// and whose value is f(key). `cancelled` receives the positive representatives
// that dropped out of an XOR key, which `add_xor_degeneracy_chain`'s ladder
// quantifies over. `reps` is caller-owned scratch and `Find` a template
// parameter for the same reason: this is the rule's hottest loop.
template <class Find>
Canon canonicalize(const Kernel &k, const CongGate &g, Find &find,
                   std::vector<Lit> &key, Lit &out_lit,
                   std::vector<Lit> &cancelled, std::vector<Lit> &reps,
                   bool &value_derived) {
  cancelled.clear();
  value_derived = false;
  if (g.kind == GateKind::And) {
    key.clear();
    bool false_input = false;
    for (const Lit input : g.inputs) {
      const Lit l = find(input);
      const int8_t v = k.value(l);
      if (v > 0) {
        value_derived = true; // AND(...,1,...) = AND(...)
        continue;
      }
      if (v < 0) {
        false_input = true; // AND(...,0,...) = 0
        value_derived = true;
        break;
      }
      key.push_back(l);
    }
    std::sort(key.begin(), key.end());
    out_lit = g.output;
    // AND degeneracies: AND(…,x,¬x,…) = 0 gives the root unit ¬out, and
    // AND(…,x,x,…) deduplicates, possibly down to the unary gate out ≡ x. These
    // are directly RUP over the live DB, unlike the XOR parity cases, so
    // `cancelled` stays empty and the ladder collapses to its top==0 form.
    {
      size_t n = 0;
      for (size_t j = 0; j < key.size(); j++) {
        if (n && key[j] == key[n - 1]) {
          if (cong_probe) cong_n_and++;
          continue; // idempotent duplicate: same representative twice
        }
        if (n && key[j] == negate(key[n - 1])) {
          if (cong_probe) cong_n_and++;
          return Canon::Unit; // clashing inputs: value(out_lit) is constant 0
        }
        key[n++] = key[j];
      }
      key.resize(n);
    }
    const int8_t out_value = k.value(out_lit);
    if (false_input) {
      if (out_value < 0)
        return Canon::Skip; // the valued relation is already satisfied
      return Canon::Unit;   // force ¬out_lit (or expose a root contradiction)
    }
    if (out_value > 0) {
      if (key.empty())
        return Canon::Skip;
      // out=1 forces every remaining input to one.  Emit one and reschedule
      // this gate after propagation; repeated pops drain the whole input list.
      key.assign(1, key.front());
      value_derived = true;
      return Canon::InputUnit;
    }
    if (out_value < 0) {
      // With all other inputs fixed true, out=0 forces the sole remaining
      // input false.  With two or more inputs it is only a residual clause.
      if (key.size() == 1) {
        key[0] = negate(key[0]);
        value_derived = true;
        return Canon::InputUnit;
      }
      return Canon::Skip;
    }
    if (key.empty()) {
      // AND()=1.  Canon::Unit records ¬out_lit, so flip its carrier.
      out_lit = negate(out_lit);
      value_derived = true;
      return Canon::Unit;
    }
    if (key.size() == 1)
      return Canon::Merge; // unary AND(x) ⇒ out_lit ≡ key[0]
    return Canon::Ok;
  }
  if (g.kind == GateKind::Ite) {
    Lit cond = find(g.inputs[0]), th = find(g.inputs[1]), el = find(g.inputs[2]);
    out_lit = g.output;
    const int8_t cv = k.value(cond), tv = k.value(th), ev = k.value(el),
                 ov = k.value(out_lit);
    auto force_output = [&](bool truth) {
      // Canon::Unit records ¬out_lit.
      out_lit = truth ? negate(g.output) : g.output;
      value_derived = true;
      return Canon::Unit;
    };
    auto equate_output = [&](Lit other) {
      const int8_t other_value = k.value(other);
      if (ov != 0 && other_value == 0) {
        key.assign(1, ov > 0 ? other : negate(other));
        value_derived = true;
        return Canon::InputUnit;
      }
      if (ov == 0 && other_value != 0)
        return force_output(other_value > 0);
      if (ov != 0 && other_value != 0) {
        if ((ov > 0) == (other_value > 0))
          return Canon::Skip;
        // Re-emit the already false output literal so record_unit constructs
        // the root contradiction with its trail reason.
        return force_output(ov < 0);
      }
      key.assign(1, other);
      value_derived = true;
      return Canon::Merge;
    };
    // A fixed condition selects one branch outright.
    if (cv != 0)
      return equate_output(cv > 0 ? th : el);
    // Equal branches erase the condition.
    if (th == el)
      return equate_output(th);
    // Two constant branches either force the output or turn the ITE into a
    // literal equivalence with its condition.
    if (tv != 0 && ev != 0) {
      if ((tv > 0) == (ev > 0))
        return force_output(tv > 0);
      return equate_output(tv > 0 ? cond : negate(cond));
    }
    // With a fixed output, a branch carrying the opposite value rules out its
    // side of the condition.  Propagation after this unit exposes the selected
    // branch on the next pop.
    if (ov != 0) {
      if (tv != 0 && ((tv > 0) != (ov > 0))) {
        key.assign(1, negate(cond));
        value_derived = true;
        return Canon::InputUnit;
      }
      if (ev != 0 && ((ev > 0) != (ov > 0))) {
        key.assign(1, cond);
        value_derived = true;
        return Canon::InputUnit;
      }
    }
    // Defer any ITE whose representative operands share a variable: then==else,
    // then==¬else (an XOR), or cond touching a branch all collapse it to a
    // simpler gate a later round or propagation handles. Skipping stays sound.
    if (var_of(cond) == var_of(th) || var_of(cond) == var_of(el) ||
        var_of(th) == var_of(el)) {
      if (cong_probe) cong_n_ite++;
      return Canon::Skip;
    }
    const bool neg_lhs = normalize_ite(cond, th, el);
    key = {cond, th, el}; // fixed order — the structural key
    out_lit = neg_lhs ? negate(g.output) : g.output;
    return Canon::Ok;
  }
  // XOR: map inputs to reps, absorb each rep's sign into a global parity, and
  // cancel repeated variables (x⊕x=0). The parity flips the output literal.
  int parity = 0;
  reps.clear();
  reps.reserve(g.inputs.size());
  for (const Lit in : g.inputs) {
    Lit r = find(in);
    const int8_t value = k.value(r);
    if (value != 0) {
      if (value > 0)
        parity ^= 1;
      value_derived = true;
      continue;
    }
    if (negated(r)) {
      parity ^= 1;
      r = negate(r);
    }
    reps.push_back(r); // positive rep lit
  }
  std::sort(reps.begin(), reps.end());
  key.clear();
  for (size_t j = 0; j < reps.size();) {
    size_t cnt = 1;
    while (j + cnt < reps.size() && reps[j + cnt] == reps[j])
      cnt++;
    if (cnt & 1)
      key.push_back(reps[j]); // odd multiplicity survives
    else
      cancelled.push_back(reps[j]); // fully cancelled — the ladder quantifies here
    j += cnt;
  }
  // value(out_lit) = ⊕key, by construction.
  out_lit = parity ? negate(g.output) : g.output;
  if (key.empty()) {
    if (cong_probe) cong_n_xor++;
    return Canon::Unit; // every variable cancelled ⇒ out_lit is the constant 0
  }
  const int8_t out_value = k.value(out_lit);
  if (out_value != 0) {
    value_derived = true;
    if (key.size() == 1) {
      if (out_value < 0)
        key[0] = negate(key[0]);
      return Canon::InputUnit;
    }
    return Canon::Skip; // a residual parity constraint, not a gate key
  }
  if (key.size() == 1)
    return Canon::Merge; // XOR(x) ⇒ out_lit ≡ x
  return Canon::Ok;
}

// --- native-LRAT chains by RUP replay ---------------------------------------
// V6's lemma families are each RUP over the current DB, but their antecedent
// chains resist hand-derivation: canonicalization folds a sign layer on top of
// the union-find bridging. Replay instead (the primitive V1 Probe and C3 Vivify
// use) and the chain comes out in the checker's own propagation order by
// construction — the one property a hand-built chain silently gets wrong.
// Returns false when the lemma is not RUP; the caller then drops the inference
// rather than write an unverifiable line.
bool congruence_rup_chain(Kernel &k, const Lit *lits, uint32_t n,
                          std::vector<uint64_t> &chain) {
  assert(k.level == 0 && !k.conflict_valid);
  chain.clear();
  // Kissat's Congruence emits these structurally derived units and binaries
  // directly (learn_congruence_unit/add_binary_clause); it does not run a
  // speculative propagation before every DRAT lemma.  DRAT needs no explicit
  // antecedent chain and the campaign checker validates the emitted proof as a
  // whole.  Native LRAT still needs the exact ordered chain, so only that mode
  // pays for replay and its watch-pose rollback.
  if (!k.proof || !k.proof->lrat())
    return true;
  kernel_probing_begin(k);
  bool conflict = false;
  Var implied = UINT32_MAX; // a lemma literal the assumptions already forced TRUE
  for (uint32_t i = 0; i < n && !conflict; i++) {
    const Lit neg = negate(lits[i]);
    const int8_t v = k.value(neg);
    if (v < 0) {
      // `lits[i]` is already true, so assuming its negation conflicts at once
      // and the reason that forced it is the refutation. Not an optimisation:
      // on a miter the ladder's own prefix propagates the conclusion, so
      // treating this as "not RUP" loses what the ladder exists for.
      implied = var_of(lits[i]);
      conflict = true;
      break;
    }
    if (v > 0)
      continue; // already implied by an earlier assumption — nothing to assume
    k.level++;
    k.frames.push_back(static_cast<uint32_t>(k.trail.size()));
    kernel_assign(k, neg, k.level, DECISION_REASON, false);
    if (kernel_propagate_probing(k))
      conflict = true;
  }
  uint64_t last = 0;
  std::vector<std::pair<uint32_t, uint64_t>> entries;
  if (conflict) {
    // Persistent walk scratch: a fresh `seen(k.nvars, 0)` per replayed lemma
    // dominates the native-LRAT path — never reintroduce it. Restored all-zero
    // in O(marked), so it is clean on entry for any kernel this thread uses.
    static thread_local std::vector<uint8_t> seen;
    static thread_local std::vector<Var> touched;
    if (seen.size() < k.nvars)
      seen.resize(k.nvars, 0);
    touched.clear();
    if (implied != UINT32_MAX)
      // the walk includes the implied literal's own reason, which sorting by
      // trail position puts last, so it closes the chain
      kernel_reason_walk(k, implied, seen, entries, &touched);
    else
      last = kernel_conflict_reason_walk(k, seen, entries, &touched);
    for (const Var tv : touched)
      seen[tv] = 0;
  }
  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  if (k.level > 0)
    kernel_backtrack(k, 0, /*update_phases=*/false);
  kernel_probing_end(k);
  // A failed replay is a dropped inference, counted in
  // `congruence_unjustified`; a nonzero count is how a ladder bug hides.
  if (cong_probe && !conflict) {
    std::fprintf(stderr, "[rup-replay] not RUP:");
    for (uint32_t i = 0; i < n; i++)
      std::fprintf(stderr, " %d(v=%d)", externalize(lits[i]),
                   (int)k.value(lits[i]));
    std::fprintf(stderr, "\n");
  }
  if (!conflict)
    return false;
  // (trail position ascending, conflict last) — kernel.hpp's chain contract
  kernel_build_lrat_chain(entries, last, chain);
  // An empty chain is not a proof: a trivially true lemma conflicts against its
  // own assumption, so the walk yields no antecedents and every LRAT checker
  // rejects the line. Refusing here covers all three ladders at one choke
  // point, at the cost of completeness where the line would cost N2.
  if (chain.empty())
    return false;
  return true;
}

// Degeneracy proof chain for one XOR gate (kissat's
// add_xor_shrinking_proof_chain; `docs/calculus.md` V6). A collapsed canonical
// key forces the gate's output, and that conclusion is not RUP on its own — the
// parity clauses propagate nothing until all but one literal is assigned — so
// the ladder is not optional. For size = |cancelled| … 1 it emits every sign
// pattern over cancelled[0..size-1] appended to each conclusion; the top level
// falsifies one parity clause, each lower level closes on the two above it.
bool add_xor_degeneracy_chain(Kernel &k, const std::vector<Lit> &cancelled,
                              const std::vector<std::vector<Lit>> &concl,
                              std::vector<std::vector<uint64_t>> &concl_chains,
                              std::vector<TransientLemma> &transient) {
  const bool lrat = k.proof && k.proof->lrat();
  const size_t nc = concl.size();
  concl_chains.assign(nc, {});
  if (!k.proof)
    return true;
  // cancellation removes variables in pairs, so this is ≤ 2 in practice and the
  // ladder is 2^(n+1)−2 lemmas
  assert(cancelled.size() <= CONG_XOR_ARITY / 2 + 1 && "ladder would blow up");
  const int top = static_cast<int>(cancelled.size());
  std::vector<Lit> lemma;
  std::vector<uint64_t> prev, cur, chain;

  if (top == 0) { // no cancelled variables ⇒ the conclusion stands on its own
    for (size_t c = 0; c < nc; c++)
      if (lrat && !congruence_rup_chain(k, concl[c].data(),
                                        static_cast<uint32_t>(concl[c].size()),
                                        concl_chains[c]))
        return false;
    return true;
  }

  for (int size = top; size >= 1; size--) {
    const uint32_t combos = 1u << size;
    cur.assign(static_cast<size_t>(combos) * nc, 0);
    for (uint32_t m = 0; m < combos; m++)
      for (size_t c = 0; c < nc; c++) {
        lemma.clear();
        for (int i = 0; i < size; i++)
          lemma.push_back(cancelled[static_cast<size_t>(i)] ^
                          static_cast<Lit>((m >> i) & 1));
        lemma.insert(lemma.end(), concl[c].begin(), concl[c].end());
        // A cancelled literal reappearing in the conclusion makes this rung
        // tautological, hence uncheckable; skip the merge in both formats.
        if (Proof::tautological(lemma.data(),
                                static_cast<uint32_t>(lemma.size())))
          return false;
        if (!lrat) {
          k.proof->add_rup(0, lemma.data(),
                           static_cast<uint32_t>(lemma.size()), nullptr, 0);
          continue;
        }
        if (size == top) {
          if (!congruence_rup_chain(k, lemma.data(),
                                    static_cast<uint32_t>(lemma.size()), chain))
            return false; // not RUP — emit nothing, the caller skips the merge
        } else {
          const size_t hi = static_cast<size_t>(m | (1u << size));
          chain.assign({prev[static_cast<size_t>(m) * nc + c],
                        prev[hi * nc + c]});
        }
        const uint64_t id = ++k.next_clause_id;
        k.proof->add_rup(id, lemma.data(), static_cast<uint32_t>(lemma.size()),
                         chain.data(), static_cast<uint32_t>(chain.size()));
        cur[static_cast<size_t>(m) * nc + c] = id;
        transient.push_back({id, lemma}); // dropped after the merge
      }
    prev.swap(cur);
  }
  if (lrat)
    for (size_t c = 0; c < nc; c++)
      concl_chains[c] = {prev[c], prev[nc + c]}; // the two size-1 parents
  return true;
}

// XOR-match proof chain (kissat add_xor_matching_proof_chain): make the two
// equivalence binaries checkable by a parity-elimination ladder, one rhs
// variable per level, closing as the degeneracy ladder does. Returns false if
// the top level does not replay. `rhs.back()` is absorbed by the gates'
// clauses, so the ladder starts one short.
bool add_xor_matching_proof_chain(Kernel &k, const std::vector<Lit> &rhs, Lit a,
                                  Lit b, std::vector<uint64_t> &chain_fwd,
                                  std::vector<uint64_t> &chain_rev,
                                  std::vector<TransientLemma> &transient) {
  chain_fwd.clear();
  chain_rev.clear();
  if (!k.proof)
    return true;
  const bool lrat = k.proof->lrat();
  const int top = static_cast<int>(rhs.size()) - 1;
  if (top < 1) {
    // Nothing to eliminate; the binaries stand on the gates alone, which under
    // LRAT would mean empty hint chains. Replay each binary for its own chain
    // instead; a failed replay skips the merge, which is sound.
    if (!lrat)
      return true;
    const Lit fwd[2] = {negate(a), b};
    const Lit rev[2] = {a, negate(b)};
    return congruence_rup_chain(k, fwd, 2, chain_fwd) &&
           congruence_rup_chain(k, rev, 2, chain_rev);
  }
  std::vector<Lit> lemma;
  std::vector<uint64_t> prev, cur, ch;
  for (int size = top; size >= 1; size--) {
    const uint32_t combos = 1u << size;
    cur.assign(static_cast<size_t>(combos) * 2, 0);
    for (uint32_t m = 0; m < combos; m++)
      for (int dir = 0; dir < 2; dir++) {
        lemma.clear();
        for (int i = 0; i < size; i++)
          lemma.push_back(rhs[static_cast<size_t>(i)] ^
                          static_cast<Lit>((m >> i) & 1));
        if (dir == 0) {
          lemma.push_back(negate(a));
          lemma.push_back(b);
        } else {
          lemma.push_back(a);
          lemma.push_back(negate(b));
        }
        // A merged output coinciding with an rhs literal or the other output
        // makes this rung tautological or duplicate-carrying: a tautology
        // replays to an empty chain, and cake_lpr reads a repeated literal as a
        // PR witness and rejects the line. Skip the merge.
        if (Proof::tautological(lemma.data(),
                                static_cast<uint32_t>(lemma.size())))
          return false;
        if (!lrat) {
          k.proof->add_rup(0, lemma.data(),
                           static_cast<uint32_t>(lemma.size()), nullptr, 0);
          continue;
        }
        if (size == top) {
          if (!congruence_rup_chain(k, lemma.data(),
                                    static_cast<uint32_t>(lemma.size()), ch))
            return false;
        } else {
          const size_t hi = static_cast<size_t>(m | (1u << size));
          ch.assign({prev[static_cast<size_t>(m) * 2 + static_cast<size_t>(dir)],
                     prev[hi * 2 + static_cast<size_t>(dir)]});
        }
        const uint64_t id = ++k.next_clause_id;
        k.proof->add_rup(id, lemma.data(), static_cast<uint32_t>(lemma.size()),
                         ch.data(), static_cast<uint32_t>(ch.size()));
        cur[static_cast<size_t>(m) * 2 + static_cast<size_t>(dir)] = id;
        transient.push_back({id, lemma});
      }
    prev.swap(cur);
  }
  if (lrat) {
    chain_fwd = {prev[0], prev[2]}; // the two size-1 parents, dir 0
    chain_rev = {prev[1], prev[3]}; // dir 1
  }
  return true;
}

// Normalize an ITE triple [cond,then,else] in place to the canonical key form
// (cond and then positive); returns whether the output must be negated.
bool normalize_ite(Lit &cond, Lit &then_l, Lit &else_l) {
  if (negated(cond)) { // cond ? t : e  ==  ¬cond ? e : t
    cond = negate(cond);
    std::swap(then_l, else_l);
  }
  if (negated(then_l)) { // ¬(cond ? ¬t : ¬e) = cond ? t : e, flipping the output
    then_l = negate(then_l);
    else_l = negate(else_l);
    return true;
  }
  return false;
}

// ITE extraction. Per-lhs conditional equivalences lhs≡w come from clause pairs
// (A∨B∨lhs)+(A∨¬B∨¬lhs) ⇒ lhs≡¬B when ¬A; one under c pairs with one under ¬c
// into a candidate. Every candidate is verified against all four defining
// ternaries before emission, so a mis-detected pair cannot yield an unsound
// gate.
void congruence_extract_ite(const Kernel &k, const ClauseIndex &index,
                            const ClauseIndex &sorted,
                            std::vector<CongGate> &out) {
  (void)k;
  auto has = [&](Lit a, Lit b, Lit c) -> bool {
    const Lit probe[3] = {a, b, c};
    return index.contains(small_key(probe, 3));
  };
  // collect conditional equivalences per lhs literal
  std::unordered_map<Lit, std::vector<std::pair<Lit, Lit>>> ce;
  uint32_t xtick = 0;
  for (const SmallKey &cl : sorted) {
    // sorted; only the first 3 literals are scanned
    if (k.stop_fuzz || !(++xtick & 255)) {
      if (kernel_terminating(k)) {
        k.stats.rule_interrupted++;
        return;
      }
      if (cong_budget.over(k))
        return; // budget stop — read-only, fewer gates
    }
    k.stats.congruence_ticks += 6; // two index probes per rotation, three
    for (int li = 0; li < 3; li++) {
      const Lit L = cl.lits[li], A = cl.lits[(li + 1) % 3],
                B = cl.lits[(li + 2) % 3];
      // (A∨B∨L) + (A∨¬B∨¬L) ⇒ L≡¬B when ¬A
      if (has(A, negate(B), negate(L)))
        ce[L].push_back({negate(A), negate(B)});
      // (A∨B∨L) + (¬A∨B∨¬L) ⇒ L≡¬A when ¬B
      if (has(negate(A), B, negate(L)))
        ce[L].push_back({negate(B), negate(A)});
    }
  }
  SmallKeySet seen;
  std::vector<Lit> ce_keys; // canonical order (see sorted_keys)
  ce_keys.reserve(ce.size());
  for (const auto &kv : ce)
    ce_keys.push_back(kv.first);
  std::sort(ce_keys.begin(), ce_keys.end());
  for (const Lit L : ce_keys) {
    if (cong_budget.over(k))
      return; // budget stop — read-only, fewer gates
    auto &lst = ce[L];
    if (lst.size() > 256)
      continue; // bound the O(n²) pairing on pathological hubs
    k.stats.congruence_ticks += lst.size() * lst.size() / 2 + 1;
    for (size_t i = 0; i < lst.size(); i++)
      for (size_t j = i + 1; j < lst.size(); j++) {
        const Lit c1 = lst[i].first, w1 = lst[i].second;
        const Lit c2 = lst[j].first, w2 = lst[j].second;
        if (c1 != negate(c2))
          continue; // need complementary conditions
        Lit cond, then_l, else_l;
        if (!negated(c1)) { // c1 is the positive condition
          cond = c1;
          then_l = w1;
          else_l = w2;
        } else {
          cond = c2;
          then_l = w2;
          else_l = w1;
        }
        // A candidate with else ≡ ¬then is a ternary XOR in an ITE costume, and
        // XOR extraction already finds that relation from the same clauses. The
        // ITE copies could only ever be discarded, since `unite` keeps
        // repr[¬big]=¬repr[big]. kissat drops them at construction too
        // (congruence.c new_ite_gate).
        if (else_l == negate(then_l))
          continue;
        // soundness: require all four ITE-defining clauses to exist
        if (!(has(cond, negate(else_l), L) && has(cond, else_l, negate(L)) &&
              has(negate(cond), negate(then_l), L) &&
              has(negate(cond), then_l, negate(L))))
          continue;
        // identity key in the fixed sequence [L,cond,then,else], not sorted
        SmallKey keyd;
        keyd.size = 4;
        keyd.lits[0] = L;
        keyd.lits[1] = cond;
        keyd.lits[2] = then_l;
        keyd.lits[3] = else_l;
        if (!seen.insert(keyd))
          continue;
        CongGate g;
        g.kind = GateKind::Ite;
        g.output = L;
        g.inputs = {cond, then_l, else_l}; // [cond,then,else] fixed order
        out.push_back(std::move(g));
      }
  }
}

// ITE-match proof chain (kissat add_ite_matching_proof_chain): case-split on the
// shared condition, emitting the four ternary lemmas that make the two
// equivalence binaries RUP, each a resolvent of one cond-side clause of each
// gate. Under LRAT each is replayed for its chain and the two binaries close on
// their cond-sides. Returns false if any lemma fails to replay.
bool add_ite_matching_proof_chain(Kernel &k, Lit cond, Lit a, Lit b,
                                  std::vector<uint64_t> &chain_fwd,
                                  std::vector<uint64_t> &chain_rev,
                                  std::vector<TransientLemma> &transient) {
  chain_fwd.clear();
  chain_rev.clear();
  if (!k.proof)
    return true;
  const bool lrat = k.proof->lrat();
  std::vector<uint64_t> ch;
  // (¬a∨b) closes on its two cond-sides; (a∨¬b) on the other two.
  auto emit = [&](Lit x, Lit y, Lit z, std::vector<uint64_t> &into) -> bool {
    const Lit l[3] = {x, y, z};
    // The case-split literal can coincide with a merged output, making the
    // lemma tautological. Both formats bail, so the two proof modes keep the
    // same search trajectory.
    if (Proof::tautological(l, 3))
      return false;
    if (!lrat) {
      k.proof->add_rup(0, l, 3, nullptr, 0);
      return true;
    }
    if (!congruence_rup_chain(k, l, 3, ch))
      return false;
    const uint64_t id = ++k.next_clause_id;
    k.proof->add_rup(id, l, 3, ch.data(), static_cast<uint32_t>(ch.size()));
    transient.push_back({id, {l[0], l[1], l[2]}});
    into.push_back(id);
    return true;
  };
  return emit(negate(a), b, cond, chain_fwd) &&
         emit(negate(a), b, negate(cond), chain_fwd) &&
         emit(a, negate(b), cond, chain_rev) &&
         emit(a, negate(b), negate(cond), chain_rev);
}

// Add the equivalence a==b from two congruent AND gates as (¬a∨b),(a∨¬b). The
// gates' inputs are equal only under the current representatives, so the LRAT
// chain must thread the bridging binaries: a's input binaries force ga's
// inputs, `big_path_chain` bridges each to the gb input in its class, and gb's
// base conflicts. Returns false iff that chain fails — its only route can run
// through a root-assigned literal — and the caller must then skip the merge
// (N2). All-or-nothing: both binaries are justified before either is added.
struct AddedEquivalence {
  bool forward = false;
  bool reverse = false;
};

template <class Find>
bool add_equivalence(Kernel &k, const CongGate &ga, const CongGate &gb,
                     Find &find, AddedEquivalence *added = nullptr) {
  const bool lrat = k.proof && k.proof->lrat();
  const Lit a = ga.output, b = gb.output;
  std::vector<uint64_t> chain, br;
  // chain forcing conflict for (¬g_from.out ∨ g_to.out)
  auto build = [&](const CongGate &g_from, const CongGate &g_to) -> bool {
    chain.assign(g_from.input_bin_ids().begin(),
                 g_from.input_bin_ids().end());
    std::unordered_map<Lit, Lit> rep2from;
    for (const Lit in : g_from.inputs)
      rep2from[find(in)] = in;
    for (const Lit to_in : g_to.inputs) {
      const auto it = rep2from.find(find(to_in));
      if (it == rep2from.end())
        return false; // no matching input class (should not happen for a match)
      const Lit from_in = it->second;
      if (from_in == to_in)
        continue;
      if (!big_path_chain(k, from_in, to_in, br))
        return false;
      chain.insert(chain.end(), br.begin(), br.end());
    }
    chain.push_back(g_to.base_id());
    // Two same-class inputs can thread bridge paths sharing binary ids, and a
    // repeated hint is illegal LRAT. It propagates nothing at its second
    // position, so dropping it leaves the propagation sequence intact.
    {
      std::unordered_set<uint64_t> seen_ids;
      size_t w = 0;
      for (const uint64_t idh : chain)
        if (seen_ids.insert(idh).second)
          chain[w++] = idh;
      chain.resize(w);
    }
    return true;
  };
  // Justify first, mutate second: under LRAT each direction's chain is built
  // and kept before anything is added, so a failure aborts the equivalence.
  std::vector<uint64_t> chain_fwd, chain_rev;
  if (lrat) {
    const bool need_fwd = !kernel_binary_live(k, negate(a), b);
    const bool need_rev = !kernel_binary_live(k, a, negate(b));
    if (need_fwd) {
      if (!build(ga, gb))
        return false;
      chain_fwd = chain;
    }
    if (need_rev) {
      if (!build(gb, ga))
        return false;
      chain_rev = chain;
    }
  }
  auto add_bin = [&](Lit x, Lit y, const std::vector<uint64_t> &ch) {
    if (kernel_binary_live(k, x, y))
      return;
    // a degenerate pair (outputs equal or complementary) would make this
    // binary duplicate-carrying or tautological, neither emittable
    assert(x != y && x != negate(y));
    const uint64_t id = ++k.next_clause_id;
    const Lit lits[2] = {x, y};
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, lits, 2, lrat ? ch.data() : nullptr,
                       lrat ? static_cast<uint32_t>(ch.size()) : 0);
    kernel_add_binary_clause(k, x, y, id);
  };
  const bool had_fwd = kernel_binary_live(k, negate(a), b);
  const bool had_rev = kernel_binary_live(k, a, negate(b));
  add_bin(negate(a), b, chain_fwd); // ga.out=1 → ga inputs → bridge → gb.base
  add_bin(a, negate(b), chain_rev); // gb.out=1 → gb inputs → bridge → ga.base
  if (added) {
    added->forward = !had_fwd;
    added->reverse = !had_rev;
  }
  return true;
}

// Add the equivalence a==b as (¬a∨b),(a∨¬b): bare adds under DRAT, where the
// preceding ladder lemmas already made them RUP, and under LRAT each binary
// carries the chain its ladder returned, so no hint-less line is emitted.
bool add_equivalence_chained(Kernel &k, Lit a, Lit b,
                             const std::vector<uint64_t> &chain_fwd,
                             const std::vector<uint64_t> &chain_rev,
                             AddedEquivalence *added = nullptr) {
  const bool lrat = k.proof && k.proof->lrat();
  // All-or-nothing, checked before either binary is added: under LRAT a binary
  // that is still missing needs a non-empty chain or the line is uncheckable.
  // Skipping is sound.
  if (lrat) {
    if (!kernel_binary_live(k, negate(a), b) && chain_fwd.empty())
      return false;
    if (!kernel_binary_live(k, a, negate(b)) && chain_rev.empty())
      return false;
  }
  auto add_bin = [&](Lit x, Lit y, const std::vector<uint64_t> &ch) {
    if (kernel_binary_live(k, x, y))
      return;
    // a degenerate pair (outputs equal or complementary) would make this
    // binary duplicate-carrying or tautological, neither emittable
    assert(x != y && x != negate(y));
    const uint64_t id = ++k.next_clause_id;
    const Lit lits[2] = {x, y};
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, lits, 2, lrat ? ch.data() : nullptr,
                       lrat ? static_cast<uint32_t>(ch.size()) : 0);
    kernel_add_binary_clause(k, x, y, id);
  };
  const bool had_fwd = kernel_binary_live(k, negate(a), b);
  const bool had_rev = kernel_binary_live(k, a, negate(b));
  add_bin(negate(a), b, chain_fwd);
  add_bin(a, negate(b), chain_rev);
  if (added) {
    added->forward = !had_fwd;
    added->reverse = !had_rev;
  }
  return true;
}

// Kissat records every union-find merge with the two binary clauses connecting
// the *current class representatives*.  This is stronger than merely keeping
// an arbitrary edge that proves the same equality: the representative-star
// shape is exactly the graph consumed by the immediately following Substitute
// pass.  Gate matching first makes the raw output equality live; binary
// matching already has it.  Consequently each missing representative clause
// is RUP here, including in LRAT mode.
bool add_representative_equivalence(Kernel &k, Lit smaller, Lit larger) {
  assert(smaller != larger && smaller != negate(larger));
  const Lit forward[2] = {negate(larger), smaller};
  const Lit reverse[2] = {larger, negate(smaller)};
  const bool need_forward = !kernel_binary_live(k, forward[0], forward[1]);
  const bool need_reverse = !kernel_binary_live(k, reverse[0], reverse[1]);
#ifdef TACTIX_ENABLE_EVENT_TRACE
  static uint32_t traced_sparse_adds = 0;
  const bool trace_sparse_add =
      (need_forward || need_reverse) && traced_sparse_adds++ < 32;
  if (trace_sparse_add)
    congruence_stage_trace(k, "representative-binary-add-begin", smaller,
                           larger, need_forward, need_reverse);
#endif
  const bool lrat = k.proof && k.proof->lrat();
  std::vector<uint64_t> forward_chain, reverse_chain;
  if (lrat) {
    if (need_forward &&
        !congruence_rup_chain(k, forward, 2, forward_chain))
      return false;
    if (need_reverse &&
        !congruence_rup_chain(k, reverse, 2, reverse_chain))
      return false;
  }
  auto add = [&](const Lit *clause, const std::vector<uint64_t> &chain) {
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, clause, 2,
                       lrat ? chain.data() : nullptr,
                       lrat ? static_cast<uint32_t>(chain.size()) : 0);
    kernel_add_binary_clause(k, clause[0], clause[1], id);
  };
  if (need_forward)
    add(forward, forward_chain);
  if (need_reverse)
  add(reverse, reverse_chain);
#ifdef TACTIX_ENABLE_EVENT_TRACE
  if (trace_sparse_add)
    congruence_stage_trace(k, "representative-binary-add-end",
                           k.watches.arena.size(), k.watches.arena.capacity(),
                           k.watches.holes);
#endif
  return true;
}

// The binary-resolution unit channel (`docs/calculus.md` V6a). Two binaries
// (l ∨ o) and (l ∨ ¬o) resolve on `o` to {l}, and `l`'s watch list holds both
// partners, so one linear sweep finds every such unit. A strict special case of
// V2 Backbone's precondition, kept here for coverage at linear cost; placement
// argument and removal path in V6a and
// `docs/scheduling-parity-ledger.md` §13.3.
// `record` batches, so no unit is asserted mid-scan.
const bool cong_y1 = [] {
  const char *s = std::getenv("TACTIX_CONG_Y1");
  return !s || *s != '0';
}();

// Kissat's Congruence first strengthens an irredundant ternary with a binary:
//
//   (a ∨ b ∨ c), (¬a ∨ b)  ⊢  (b ∨ c)
//
// (and symmetrically for each pivot).  The extracted binaries are part of the
// gate database itself: on the Tier-B convergence witness they double the
// binary population before AND/XOR/ITE extraction, exposing the equivalences
// that drive the rest of the closure.  Only binaries present at entry are used
// as antecedents, matching Kissat's fixed offset table; newly derived pairs are
// deduplicated but do not recursively seed this pass.
static bool find_congruence_binary_candidates(
    Kernel &k, std::vector<std::pair<Lit, Lit>> &candidates) {
  candidates.clear();
  bool complete = true;
  uint32_t tick = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    if (k.stop_fuzz || !(++tick & 255)) {
      if (kernel_terminating(k)) {
        k.stats.rule_interrupted++;
        complete = false;
        break;
      }
      if (cong_budget.over(k)) {
        complete = false;
        break;
      }
    }
    const Clause *c = clause_at(k.arena, ref);
    ref += clause_words(c->size);
    if (c->garbage() || c->redundant() || c->size != 3)
      continue;
    Lit lits[3] = {c->lits[0], c->lits[1], c->lits[2]};
    if (k.value(lits[0]) || k.value(lits[1]) || k.value(lits[2]))
      continue;
    k.stats.congruence_ticks += 3;
    Lit x = INVALID_LIT, y = INVALID_LIT;
    for (unsigned pivot = 0; pivot != 3; pivot++) {
      const Lit a = lits[pivot], b = lits[(pivot + 1) % 3],
                cc = lits[(pivot + 2) % 3];
      if (kernel_binary_live(k, negate(a), b) ||
          kernel_binary_live(k, negate(a), cc)) {
        x = b;
        y = cc;
        break;
      }
    }
    if (x == INVALID_LIT)
      continue;
    if (kernel_binary_live(k, x, y))
      continue;
    candidates.emplace_back(x, y);
  }
  return complete;
}

static void install_congruence_binary_candidates(
    Kernel &k, const std::vector<std::pair<Lit, Lit>> &candidates,
    uint64_t &extracted) {
  const bool lrat = k.proof && k.proof->lrat();
  std::vector<uint64_t> chain;
  std::vector<std::pair<Lit, uint32_t>> touched_binary_lists;
  const auto note_prefix = [&](Lit lit) {
    const uint32_t *p = k.watches.begin(lit);
    const uint32_t *const end = k.watches.end(lit);
    while (p != end && Watch{*p}.binary())
      p++;
    touched_binary_lists.emplace_back(
        lit, static_cast<uint32_t>(p - k.watches.begin(lit)));
  };
  for (const auto &[x, y] : candidates) {
    // Discovery snapshots entry membership before any pair is installed, so
    // derived binaries never recursively seed this pass.  The live watch
    // database now serves only to suppress later duplicate candidates.
    if (kernel_binary_live(k, x, y))
      continue;
    const Lit binary[2] = {x, y};
    chain.clear();
    if (lrat && !congruence_rup_chain(k, binary, 2, chain)) {
      k.stats.congruence_unjustified++;
      continue;
    }
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, binary, 2,
                       lrat ? chain.data() : nullptr,
                       lrat ? static_cast<uint32_t>(chain.size()) : 0);
    note_prefix(x);
    note_prefix(y);
    kernel_add_binary_clause(k, x, y, id);
    extracted++;
  }
  // Kissat accumulates extracted pairs in a separate dense-mode suffix, radix
  // sorts that suffix by (min compact literal, max compact literal), then
  // reconnects it after the pre-existing binary prefix.  Proof emission above
  // remains in arena traversal order; only sparse propagation order changes.
  // Tactix adds directly to the packed watch lists, after their large entries,
  // so restore the same old-prefix / sorted-new-suffix / large layout here.
  std::stable_sort(touched_binary_lists.begin(), touched_binary_lists.end(),
                   [](const auto &a, const auto &b) {
                     return a.first < b.first;
                   });
  touched_binary_lists.erase(
      std::unique(touched_binary_lists.begin(), touched_binary_lists.end(),
                  [](const auto &a, const auto &b) {
                    return a.first == b.first;
                  }),
      touched_binary_lists.end());
  std::vector<uint32_t> new_binaries, large_words;
  for (const auto &[lit, binary_prefix] : touched_binary_lists) {
    new_binaries.clear();
    large_words.clear();
    uint32_t *const begin = k.watches.begin(lit);
    uint32_t *p = begin + binary_prefix;
    uint32_t *const end = k.watches.end(lit);
    while (p != end) {
      if (Watch{*p}.binary()) {
        new_binaries.push_back(*p++);
      } else {
        large_words.push_back(*p++);
        large_words.push_back(*p++);
      }
    }
    std::sort(new_binaries.begin(), new_binaries.end(),
              [&](uint32_t a, uint32_t b) {
                return kernel_import_lit_order(k, Watch{a}.other()) <
                       kernel_import_lit_order(k, Watch{b}.other());
              });
    uint32_t *out = begin + binary_prefix;
    out = std::copy(new_binaries.begin(), new_binaries.end(), out);
    std::copy(large_words.begin(), large_words.end(), out);
    k.watches.heads[lit].binaries_first = true;
  }
}

// After propagating the closure, Kissat enters dense mode a second time and
// forward-subsumes large clauses modulo the still-live representative map.
// This is deliberately local to Congruence: once the map is reset, ordinary
// SubsumeForward can no longer see that (a v b) and (repr(a) v repr(b)) match.
template <class Find>
static uint64_t forward_subsume_congruent_clauses(Kernel &k, Find &find) {
  // Dense entry first deduplicates binaries and drops every one satisfied by a
  // newly propagated root unit.  At a root fixpoint an assigned binary must be
  // satisfied (the both-false case would already have refuted).
  kernel_deduplicate_binary_watches(k);
  congruence_stage_trace(k, "forward-deduplicate-end");
  std::vector<std::pair<Lit, Lit>> satisfied_binaries;
  for (Lit lit = 0; lit < 2 * k.nvars; lit++)
    for (const WatchEntry w : k.watches.entries(lit)) {
      if (!w.binary())
        continue;
      const Lit other = w.other();
      if (kernel_import_lit_order(k, lit) >=
          kernel_import_lit_order(k, other))
        continue;
      if (k.value(lit) || k.value(other)) {
        assert(k.value(lit) > 0 || k.value(other) > 0);
        satisfied_binaries.emplace_back(lit, other);
      }
    }
  congruence_stage_trace(k, "forward-satisfied-scan-end",
                         satisfied_binaries.size());
  for (const auto &[a, b] : satisfied_binaries) {
    const bool deleted = kernel_delete_binary(k, a, b);
    assert(deleted);
    (void) deleted;
  }
  congruence_stage_trace(k, "forward-satisfied-delete-end",
                         satisfied_binaries.size());
  std::vector<uint8_t> matchable(k.nvars, 0);
  for (Var v = 0; v < k.nvars; v++) {
    if (k.value(make_lit(v, false)) || kernel_var_retired(k, v))
      continue;
    const Lit lit = make_lit(v, false);
    const Lit repr = find(lit);
    if (lit != repr) {
      matchable[v] = 1;
      matchable[var_of(repr)] = 1;
    }
  }
  congruence_stage_trace(k, "forward-matchable-end", matchable.size());

  struct Candidate {
    Ref ref;
    uint32_t size;
  };
  std::vector<Candidate> candidates;
  std::vector<uint8_t> seen(2 * k.nvars, 0);
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  std::vector<Lit> canonical;
  uint64_t deleted = 0;
  congruence_stage_trace(k, "forward-scratch-end", seen.size(), marks.size());

  // Return false for a root-satisfied or representative-tautological clause.
  // Otherwise preserve first-occurrence order in `canonical`; Kissat uses that
  // order to break ties when choosing the least-occurring connection literal.
  const auto canonicalize_clause = [&](const Clause *c,
                                       bool *contains_matchable) {
    canonical.clear();
    *contains_matchable = false;
    bool keep = true;
    for (const Lit lit : *c) {
      const int8_t value = k.value(lit);
      if (value > 0) {
        keep = false;
        break;
      }
      if (value < 0)
        continue;
      *contains_matchable |= matchable[var_of(lit)] != 0;
      const Lit repr = find(lit);
      const int8_t repr_value = k.value(repr);
      if (repr_value > 0) {
        keep = false;
        break;
      }
      if (repr_value < 0 || seen[repr])
        continue;
      if (seen[negate(repr)]) {
        keep = false;
        break;
      }
      seen[repr] = 1;
      canonical.push_back(repr);
    }
    for (const Lit lit : canonical)
      seen[lit] = 0;
    return keep;
  };

  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    ref += words;
    if (c->garbage() || c->redundant())
      continue;
    bool contains_matchable = false;
    if (!canonicalize_clause(c, &contains_matchable)) {
      kernel_delete_clause(k, c);
      deleted++;
      continue;
    }
    if (contains_matchable)
      candidates.push_back(
          {static_cast<Ref>(ref - words), static_cast<uint32_t>(canonical.size())});
  }
  congruence_stage_trace(k, "forward-candidates-end", candidates.size());
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const Candidate &a, const Candidate &b) {
                     return a.size < b.size;
                   });
  congruence_stage_trace(k, "forward-candidate-sort-end", candidates.size());

  // Only literals selected as the least-occurring connection of an accepted
  // candidate ever own a list.  A dense vector-of-vectors spends 24 bytes for
  // every literal even when no candidate names it; on 128_125 that means 97
  // million empty vectors before storing one Ref.  Sparse lookup preserves
  // the exact candidate and insertion order.
  SparseRefOccurrences occ;
  occ.reserve(candidates.size());
  for (const Candidate candidate : candidates) {
    if (kernel_terminating(k))
      break;
    Clause *c = clause_at(k.arena, candidate.ref);
    if (c->garbage())
      continue;
    bool contains_matchable = false;
    const bool live = canonicalize_clause(c, &contains_matchable);
    assert(live && contains_matchable);
    (void) live;
    (void) contains_matchable;
    for (const Lit lit : canonical)
      marks[lit] = 1;
    bool subsumed = false;
    for (const Lit lit : canonical) {
      const uint32_t occurrence = occ.find(lit);
      if (occurrence == SparseRefOccurrences::EMPTY)
        continue;
      for (uint32_t node = occ.first(occurrence);
           node != SparseRefOccurrences::EMPTY; node = occ.next(node)) {
        const Ref dref = occ.ref(node);
        const Clause *d = clause_at(k.arena, dref);
        bool d_matchable = false;
        std::vector<Lit> dcanonical;
        // `canonicalize_clause` owns shared scratch, so preserve the candidate
        // before canonicalizing a possible subsumer.
        dcanonical.swap(canonical);
        const bool d_live = canonicalize_clause(d, &d_matchable);
        bool subset = d_live;
        for (const Lit other : canonical)
          if (!marks[other]) {
            subset = false;
            break;
          }
        canonical.swap(dcanonical);
        if (subset) {
          subsumed = true;
          break;
        }
      }
      if (subsumed)
        break;
    }
    for (const Lit lit : canonical)
      marks[lit] = 0;
    if (subsumed) {
      kernel_delete_clause(k, c);
      deleted++;
      continue;
    }
    Lit least = INVALID_LIT;
    size_t least_count = 0;
    for (const Lit lit : canonical) {
      const uint32_t occurrence = occ.find(lit);
      const size_t count = occurrence == SparseRefOccurrences::EMPTY
                               ? 0
                               : occ.count(occurrence);
      if (least == INVALID_LIT || count < least_count) {
        least = lit;
        least_count = count;
      }
    }
    assert(least != INVALID_LIT);
    occ.push_back(least, candidate.ref);
  }

  if (deleted) {
    ArenaOrdering *const saved_order = k.arena_ordering;
    k.arena_ordering = nullptr;
    kernel_collect_garbage(k);
    k.arena_ordering = saved_order;
  } else
    kernel_rebuild_large_watches(k);
  return deleted;
}

template <class Record> bool find_binary_units(Kernel &k, Record &&record) {
  if (!cong_y1)
    return false;
  // Persistent scratch, restored in O(marked): a per-call 2·nvars vector would
  // make this sweep memset-bound on a large formula.
  static thread_local std::vector<uint8_t> mark;
  static thread_local std::vector<Lit> touched;
  const size_t nlits = 2 * static_cast<size_t>(k.nvars);
  if (mark.size() < nlits)
    mark.resize(nlits, 0);
  const bool lrat = k.proof && k.proof->lrat();
  std::vector<uint64_t> chain;
  for (Var v = 0; v < k.nvars; v++) {
    // V6a owns one complete linear pass.  The shared Congruence allowance may
    // truncate gate extraction or closure, but must not starve this pass after
    // they have already spent it.  Only a global termination request can cut
    // the pass (soundly dropping the remaining optional inferences).
    if (!(v & 255) && kernel_terminating(k))
      return false;
    if (kernel_var_retired(k, v))
      continue;
    for (int s = 0; s < 2; s++) {
      const Lit l = make_lit(v, s != 0);
      // A root-assigned `l` has nothing to learn: already true, and false is
      // impossible at a quiescent root.
      if (k.value(l) != 0)
        continue;
      const uint64_t scan_ticks = 1 + k.watches.size(l);
      k.stats.congruence_y1_ticks += scan_ticks;
      // Retain the inclusive Congruence accounting used by the outer rule.
      k.stats.congruence_ticks += k.watches.size(l);
      touched.clear();
      bool found = false;
      for (const WatchEntry w : k.watches.entries(l)) {
        if (!w.binary())
          continue;
        const Lit o = w.other();
        if (k.value(o) != 0)
          continue; // satisfied or already-propagated binary
        if (mark[negate(o)]) {
          found = true;
          break;
        }
        if (!mark[o]) {
          mark[o] = 1;
          touched.push_back(o);
        }
      }
      for (const Lit t : touched)
        mark[t] = 0;
      if (!found)
        continue;
      k.stats.congruence_y1_found++;
      // Replayed, never hand-built: a hand-built pair silently gets propagation
      // order wrong. The replay charges real propagation, so meter it here.
      const uint64_t t0 = k.stats.ticks;
      const bool ok = !lrat || congruence_rup_chain(k, &l, 1, chain);
      const uint64_t replay_ticks = k.stats.ticks - t0;
      k.stats.congruence_y1_ticks += replay_ticks;
      k.stats.congruence_ticks += replay_ticks;
      if (!ok) {
        k.stats.congruence_unjustified++;
        continue; // sound: the inference is dropped, nothing is emitted
      }
      if (record(l, chain))
        return true; // the batch refuted
    }
  }
  return false;
}


} // namespace — public rule entry points (header-declared) below

// External-linkage forwarder for the primitive Sweep shares.
bool congruence_replay_rup(Kernel &k, const Lit *lits, uint32_t n,
                           std::vector<uint64_t> &chain) {
  return congruence_rup_chain(k, lits, n, chain);
}

void congruence_extract_gates(const Kernel &k, std::vector<CongGate> &out,
                              bool with_xor_ite) {
  const size_t base = out.size();
  congruence_stage_trace(k, "and-begin", out.size());
  {
    Stage s(&cong_t.extract_and);
    congruence_extract(k, out);
  }
  congruence_stage_trace(k, "and-end", out.size());
  k.stats.congruence_gates_and += out.size() - base; // see Stats
  if (with_xor_ite) {
    ClauseIndex index; // built once, shared by XOR + ITE extraction
    congruence_stage_trace(k, "small-index-begin", out.size());
    {
      Stage s(&cong_t.index);
      build_clause_index(k, index);
    }
    congruence_stage_trace(k, "small-index-end", out.size(), index.size());
    // …and so is its canonical iteration order: the index is immutable for the
    // whole apply, so sorting it per consumer would recompute the same vector.
    const ClauseIndex *sorted = nullptr;
    {
      Stage s(&cong_t.sort);
      sorted = &sorted_keys(k, index);
    }
    congruence_stage_trace(k, "small-sort-end", out.size(), sorted->size());
    const size_t before_xor = out.size();
    {
      Stage s(&cong_t.xor_x);
      congruence_extract_xor(k, index, *sorted, out);
    }
    congruence_stage_trace(k, "xor-end", out.size(), sorted->size());
    k.stats.congruence_gates_xor += out.size() - before_xor;
    const size_t before_ite = out.size();
    {
      Stage s(&cong_t.ite_x);
      congruence_extract_ite(k, index, *sorted, out);
    }
    congruence_stage_trace(k, "ite-end", out.size(), sorted->size());
    k.stats.congruence_gates_ite += out.size() - before_ite;
  }
}

ApplyResult congruence_apply(Kernel &k) {
  assert(congruence_guard(k));
  Stage apply_stage(&cong_t.apply_all);
  // The round's work budget, in `congruence_ticks`, handed by the policy
  // through `Kernel::work_budget` (0 = unbounded). Cleared on every exit.
  BudgetScope budget_scope(k);
  const uint64_t apply_t0 = cong_time ? now_nanos() : 0; // in-flight, for the print
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  congruence_stage_trace(k, "root-end");
  // Kissat enters dense mode before extraction and keeps the live binaries in
  // closure->binaries, where find_binary answers every later membership query
  // without touching large-watch tails.  Build the corresponding exact index
  // once for the whole Tactix application.  It follows additions and deletions
  // through the kernel hooks, while k.binary_ids remains the proof-id archive.
  struct ScopedBinaryLiveness {
    Kernel &kernel;
    BinaryLivenessIndex index;
    explicit ScopedBinaryLiveness(Kernel &k) : kernel(k) {
      assert(!kernel.binary_liveness);
      size_t pairs = 0;
      for (Lit lit = 0; lit < 2 * kernel.nvars; lit++)
        for (const WatchEntry w : kernel.watches.entries(lit))
          pairs += w.binary() && lit < w.other();
      index.reserve(pairs);
      for (Lit lit = 0; lit < 2 * kernel.nvars; lit++)
        for (const WatchEntry w : kernel.watches.entries(lit))
          if (w.binary() && lit < w.other())
            index.add(kernel_binary_key(lit, w.other()));
      kernel.binary_liveness = &index;
    }
    ~ScopedBinaryLiveness() {
      assert(kernel.binary_liveness == &index);
      kernel.binary_liveness = nullptr;
    }
  } binary_liveness(k);
  std::vector<std::pair<Lit, Lit>> binary_candidates;
  {
    Stage s(&cong_t.binary_extract);
    (void)find_congruence_binary_candidates(k, binary_candidates);
  }
  congruence_stage_trace(k, "binary-candidates-end",
                         binary_candidates.size());
  {
    Stage s(&cong_t.rebuild);
    kernel_canonicalize_binary_watches(k, true, &binary_candidates);
  }
  congruence_stage_trace(k, "binary-rebuild-end");
  k.stats.congruence_rounds++;
  if (cong_probe)
    cong_root_in = kernel_root_fixed(k);

  // AND, XOR and ITE gates all participate in one union-find closure: an AND
  // input can be an XOR/ITE output.
  const bool lrat = k.proof && k.proof->lrat();

  // Root units derived by this round are asserted and propagated immediately.
  // Congruence is a value-driven closure: each new root value can simplify a
  // gate into the next unit or equivalence in the same application.  Earlier
  // batching needed a dense literal/id pair for every variable; immediate
  // root closure makes the root assignment and `unit_ids` that same-sign
  // deduplication and opposite-sign refutation state.
  bool recorded_unit = false;
  congruence_stage_trace(k, "unit-state-end");
  auto record_unit = [&](Lit u, const std::vector<uint64_t> &uchain,
                         uint64_t *counter = nullptr) -> bool {
    const Var v = var_of(u);
    if (k.value(u) > 0)
      return false; // already true at the root — the inference adds nothing
    const uint64_t trail_id = k.unit_ids[v];
    const bool contradicts_trail = k.value(u) < 0;
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, &u, 1, lrat ? uchain.data() : nullptr,
                       lrat ? static_cast<uint32_t>(uchain.size()) : 0);
    if (contradicts_trail) {
      kernel_emit_empty_from_pair(k, id, trail_id);
      return true;
    }
    ++(counter ? *counter : k.stats.congruence_units);
    recorded_unit = true;
    k.unit_ids[v] = id; // this unit is the variable's identity from here
    kernel_assign_unit(k, u);
    return inprocess_reach_root(k);
  };
  // Every recorded unit has already reached root closure.  Keep the common
  // finish path for result accounting and all early exits.
  auto finish = [&](bool merged) -> ApplyResult {
    if (cong_probe)
      cong_root_out = kernel_root_fixed(k);
    if (k.inconsistent)
      return ApplyResult::Unsat;
    return (merged || recorded_unit) ? ApplyResult::Ok : ApplyResult::NoOp;
  };

  // Strengthening precedes every other Congruence channel in the reference:
  // the resulting binaries participate in unit/equivalence discovery and in
  // gate extraction during this same closure.
  uint64_t extracted_binaries = 0;
  {
    Stage s(&cong_t.binary_extract);
    install_congruence_binary_candidates(k, binary_candidates,
                                         extracted_binaries);
  }
  std::vector<std::pair<Lit, Lit>>().swap(binary_candidates);
  congruence_stage_trace(k, "binary-extract-end", extracted_binaries);

  std::vector<CongGate> gates;
  // Gate extraction precedes unit discovery in Kissat.  This snapshot order is
  // semantic: gates containing a literal fixed by the new binary-unit sweep
  // must remain present so the closure can simplify them into residual units,
  // binaries, and equivalences.
  congruence_extract_gates(k, gates, /*with_xor_ite=*/true);
  congruence_stage_trace(k, "gates-end", gates.size());
  // Extraction is read-only, so an interrupt raised during it costs nothing to
  // honour — nothing has been applied yet.
  if (kernel_terminating(k)) {
    k.stats.rule_interrupted++;
    return finish(/*merged=*/false);
  }

  // Kissat indexes every gate by RHS literal and propagates a root unit only
  // through gates containing that literal (congruence.c
  // simplify_gates/propagate_unit).  This immutable value index includes the
  // output too.  It is separate from the representative-occurrence index
  // below, whose keys change as equivalence classes merge.
  struct OccNode {
    uint32_t gate;
    uint32_t next;
  };
  const uint32_t nlits = 2 * k.nvars;
  PagedLiteralTails value_occ_tails(nlits);
  PagedVector<OccNode> value_occ_nodes;
  size_t value_occurrences = gates.size();
  for (const CongGate &g : gates)
    value_occurrences += g.inputs.size();
  value_occ_nodes.reserve(value_occurrences);
  const auto add_value_occurrence = [&](Lit lit, uint32_t gate) {
    assert(value_occ_nodes.size() < UINT32_MAX);
    const uint32_t node = static_cast<uint32_t>(value_occ_nodes.size());
    uint32_t &tail = value_occ_tails.get_or_create(lit);
    if (tail == UINT32_MAX) {
      value_occ_nodes.push_back({gate, node});
    } else {
      const uint32_t head = value_occ_nodes[tail].next;
      value_occ_nodes.push_back({gate, head});
      value_occ_nodes[tail].next = node;
    }
    tail = node;
  };
  for (uint32_t gate = 0; gate < gates.size(); gate++) {
    for (const Lit input : gates[gate].inputs)
      add_value_occurrence(input, gate);
    add_value_occurrence(gates[gate].output, gate);
  }
  congruence_stage_trace(k, "value-occurrences-end", gates.size(),
                         value_occ_nodes.size());

  // The binary-resolution unit sweep follows binary and gate extraction; its
  // immediate root closure seeds value-driven gate simplification below.
  {
    Stage s(&cong_t.binary_units);
    if (find_binary_units(k, [&](Lit u, const std::vector<uint64_t> &ch) {
          return record_unit(u, ch, &k.stats.congruence_y1_units);
        }))
      return ApplyResult::Unsat;
  }
  congruence_stage_trace(k, "binary-units-end", gates.size());

  // The congruence closure (kissat's), an incremental worklist over a union-find
  // on literals: merge the outputs of two gates that canonicalize to the same
  // (kind, key), then re-process only the gates whose inputs contain the merged
  // literal, so the collapse propagates through the circuit layers in one apply
  // (`docs/calculus.md` V6).
  const uint64_t setup_t0 = cong_time ? now_nanos() : 0;
  congruence_stage_trace(k, "closure-repr-begin", gates.size(), nlits);
  std::vector<Lit> repr(nlits);
  for (Lit l = 0; l < static_cast<Lit>(nlits); l++)
    repr[l] = l;
  congruence_stage_trace(k, "closure-repr-end", gates.size(), repr.size());
  auto find = [&](Lit l) {
    while (repr[l] != l)
      l = repr[l] = repr[repr[l]];
    return l;
  };
  auto unite = [&](Lit representative, Lit retired) {
    repr[retired] = representative;
    repr[negate(retired)] = negate(representative);
  };

  // Per-kind tables keyed by the canonical input key (kinds must not collide).
  // Pre-size each one from the extracted gate population.  Canonicalization
  // can only shrink an input key on its first insertion; later re-keying grows
  // normally if a gate is rescheduled after a merge.
  size_t gate_counts[3] = {0, 0, 0};
  size_t input_counts[3] = {0, 0, 0};
  for (const CongGate &g : gates) {
    const int kind = static_cast<int>(g.kind);
    gate_counts[kind]++;
    input_counts[kind] += g.inputs.size();
  }
  ClosureTable table[3];
  // Canonical-input literal -> gate ids, preserving registration order.  A
  // vector<vector<...>> spends 24 bytes on every literal before storing one
  // dependency.  Each dense slot instead names the tail of a circular linked
  // list: tail.next is the head, so one 32-bit endpoint supports ordered append
  // and ordered traversal.  Paged nodes avoid geometric whole-buffer copies.
  PagedLiteralTails occ_tails(nlits);
  congruence_stage_trace(k, "closure-occ-heads-end", gates.size(),
                         occ_tails.directory_size());
  PagedVector<OccNode> occ_nodes;
  occ_nodes.reserve(input_counts[0] + input_counts[1] + input_counts[2]);
  congruence_stage_trace(k, "closure-occ-nodes-end", gates.size(),
                         occ_nodes.capacity());
  auto add_occurrence = [&](Lit lit, uint32_t gate) {
    assert(occ_nodes.size() < UINT32_MAX);
    const uint32_t node = static_cast<uint32_t>(occ_nodes.size());
    uint32_t &tail = occ_tails.get_or_create(lit);
    if (tail == UINT32_MAX) {
      occ_nodes.push_back({gate, node});
    } else {
      const uint32_t head = occ_nodes[tail].next;
      occ_nodes.push_back({gate, head});
      occ_nodes[tail].next = node;
    }
    tail = node;
  };
  // The initial FIFO is exactly 0..gates.size()-1; represent that prefix
  // implicitly and store only later reschedules.  Queue membership is one bit
  // per gate, not one byte.
  const size_t initial_work = gates.size();
  PagedVector<uint32_t> rescheduled_work;
  rescheduled_work.reserve(gates.size());
  size_t work_head = 0;
  std::vector<uint64_t> inq((gates.size() + 63) / 64, UINT64_MAX);
  const auto queued = [&](uint32_t gate) {
    return (inq[gate >> 6] >> (gate & 63)) & 1;
  };
  const auto set_queued = [&](uint32_t gate, bool value) {
    const uint64_t bit = UINT64_C(1) << (gate & 63);
    if (value)
      inq[gate >> 6] |= bit;
    else
      inq[gate >> 6] &= ~bit;
  };
  const auto work_size = [&]() {
    return initial_work + rescheduled_work.size();
  };
  congruence_stage_trace(k, "closure-work-end", work_size(), inq.size());
  if (cong_time)
    cong_t.setup += now_nanos() - setup_t0;

  uint64_t neq = 0;
  bool unsat = false;
  std::vector<Lit> key, key_j, cancelled, cancelled_j;
  std::vector<Lit> reps; // canonicalize's XOR scratch (not reentrant — one buffer)
  std::vector<TransientLemma> transient; // ladder lemmas, del'd after each use

  // The contradictory merge: the closure proved a ≡ b while the union-find holds
  // a ≡ ¬b, so the formula is UNSAT and each opposing unit is directly RUP
  // (a propagates b over the new binary and ¬a over the in-class bridge path).
  // `record_unit` pairs them into the empty clause; returns true iff refuted.
  auto refute_contradictory = [&](Lit a) -> bool {
    const Lit u1 = negate(a), u2 = a;
    std::vector<uint64_t> ch1, ch2;
    if (lrat && !congruence_rup_chain(k, &u1, 1, ch1)) {
      k.stats.congruence_unjustified++;
      return false;
    }
    if (record_unit(u1, ch1))
      return true;
    if (lrat && !congruence_rup_chain(k, &u2, 1, ch2)) {
      k.stats.congruence_unjustified++;
      return false; // {¬a} stays pending — half the inference, still sound
    }
    return record_unit(u2, ch2);
  };

  // Merge two provably-equivalent literals: unite their classes and reschedule
  // the gates whose canonical inputs just changed. Proving the equivalence is
  // the caller's job — it differs per kind and per degeneracy.
  auto same_binary = [](Lit a, Lit b, Lit c, Lit d) {
    return (a == c && b == d) || (a == d && b == c);
  };
  auto merge_lits = [&](Lit x, Lit y,
                        const AddedEquivalence *temporary = nullptr) -> bool {
    const Lit oa = find(x), ob = find(y);
    if (oa == ob)
      return false;
    if (cong_probe && (k.value(oa) != 0 || k.value(ob) != 0))
      cong_val_merge++;
    // Kissat's representatives are selected by compact internal-literal order.
    // Tactix deliberately keeps DIMACS/external-backed variable numbers, so a
    // raw literal comparison silently chooses a different representative and
    // changes every later gate key.  Compare through the parser's import-order
    // bridge instead (the same bridge Substitute uses).
    const bool a_first = kernel_import_lit_order(k, oa) <
                         kernel_import_lit_order(k, ob);
    const Lit sm = a_first ? oa : ob, big = a_first ? ob : oa;
    if (neq < 16)
      congruence_stage_trace(k, "binary-import-merge-begin", x, y, sm, big);
    if (!add_representative_equivalence(k, sm, big)) {
      if (temporary) {
        if (temporary->forward)
          (void)kernel_delete_binary(k, negate(x), y);
        if (temporary->reverse)
          (void)kernel_delete_binary(k, x, negate(y));
      }
      return false;
    }
    unite(sm, big);
    neq++;
    if (neq <= 16)
      congruence_stage_trace(k, "binary-import-merge-added", neq,
                             k.watches.arena.size(), k.watches.holes);
    for (const Lit side : {big, negate(big)}) {
      const uint32_t tail = occ_tails.get(side);
      if (tail == UINT32_MAX)
        continue;
      uint32_t node = occ_nodes[tail].next;
      while (true) {
        const uint32_t gg = occ_nodes[node].gate;
        if (!queued(gg)) {
          set_queued(gg, true);
          rescheduled_work.push_back(gg);
        }
        if (node == tail)
          break;
        node = occ_nodes[node].next;
      }
    }
    // The raw-output binaries are proof scaffolding only.  Kissat keeps the
    // representative pair emitted above, not a second edge between arbitrary
    // members of the newly joined classes.  Do retain a raw clause when it was
    // already part of the input graph, or when it is itself the representative
    // clause.
    if (temporary) {
      const Lit rf0 = negate(big), rf1 = sm;
      const Lit rr0 = big, rr1 = negate(sm);
      auto retire = [&](bool added, Lit a, Lit b) {
        if (!added || same_binary(a, b, rf0, rf1) ||
            same_binary(a, b, rr0, rr1))
          return;
        const bool deleted = kernel_delete_binary(k, a, b);
        assert(deleted);
        (void)deleted;
      };
      retire(temporary->forward, negate(x), y);
      retire(temporary->reverse, x, negate(y));
    }
    return true;
  };

  // Kissat's `find_equivalences` channel runs before propagating the gate
  // closure (congruence.c). Import every equivalence already stated by the
  // paired binaries (a v !b), (!a v b) into this round's representatives.
  // This is not redundant with Substitute: the imported classes simplify
  // gates during this very closure, and a non-zero merge count is the signal
  // that `probe_initially` uses to run its eager Substitute follow-up.
  //
  // Follow Kissat's positive-literal, smaller-endpoint traversal. Besides
  // making every pair unique, that order keeps the representative trajectory
  // stable for trace comparison.
  const uint64_t binary_import_t0 = cong_time ? now_nanos() : 0;
  std::vector<Lit> marked;
  CompactVarOrder binary_vars(k);
  for (size_t variable_index = 0; variable_index < binary_vars.size();
       variable_index++) {
    const Var v = binary_vars[variable_index];
    if (unsat)
      break;
    if (v < k.var_status.size() &&
        k.var_status[v] != VarStatus::Active)
      continue;
    const Lit lit = make_lit(v, false);
    if (k.value(lit) != 0)
      continue;
    bool restart;
    do {
      restart = false;
      for (const WatchEntry w : k.watches.entries(lit)) {
        if (!w.binary())
          continue;
        const Lit other = w.other();
        if (kernel_import_lit_order(k, lit) >
            kernel_import_lit_order(k, other))
          continue;
        marked.push_back(other);
      }
      if (variable_index < 16 || !(variable_index % 250000))
        congruence_stage_trace(k, "binary-import-collected", variable_index,
                               marked.size(), marked.capacity(), neq);
      std::sort(marked.begin(), marked.end());
      marked.erase(std::unique(marked.begin(), marked.end()), marked.end());
      const Lit not_lit = negate(lit);
      for (const WatchEntry w : k.watches.entries(not_lit)) {
        if (!w.binary())
          continue;
        const Lit other = w.other();
        if (kernel_import_lit_order(k, not_lit) >
            kernel_import_lit_order(k, other) ||
            lit == other ||
            !std::binary_search(marked.begin(), marked.end(), negate(other)))
          continue;
        const Lit lit_repr = find(lit), other_repr = find(other);
        if (lit_repr == other_repr)
          continue;
        if (lit_repr == negate(other_repr)) {
          unsat = refute_contradictory(lit);
          break;
        }
        if (merge_lits(lit, other)) {
          k.stats.congruence_binary_equivalences++;
          restart = true;
        } else
          k.stats.congruence_unjustified++;
        break;
      }
      marked.clear();
    } while (restart && !unsat);
    if (variable_index < 16 || !(variable_index % 250000))
      congruence_stage_trace(k, "binary-import-variable-end", variable_index,
                             k.watches.arena.size(), k.watches.holes, neq);
  }
  if (cong_time)
    cong_t.binary_import += now_nanos() - binary_import_t0;
  std::vector<Lit>().swap(marked);
  binary_vars = CompactVarOrder{};
  congruence_stage_trace(k, "binary-import-scratch-released", gates.size(),
                         occ_tails.allocated_blocks());
  if (unsat)
    return ApplyResult::Unsat;
  if (gates.empty()) {
    k.stats.congruence_merges += neq;
    return finish(/*merged=*/neq != 0);
  }
  for (int kind = 0; kind < 3; kind++)
    if (gate_counts[kind])
      table[kind].reserve(gate_counts[kind], input_counts[kind]);
  congruence_stage_trace(
      k, "closure-table-end", gates.size(),
      table[0].bucket_count() + table[1].bucket_count() +
          table[2].bucket_count());

  // Every gate is initially queued, so units found before this point need no
  // explicit reschedule.  From here on, admit only already-consumed gates that
  // mention a newly root-assigned variable.  Scan both signs because one root
  // assignment changes the value view of its complement too.
  size_t value_trail_head = k.trail.size();
  const auto reschedule_assigned_gates = [&]() {
    while (value_trail_head < k.trail.size()) {
      const Lit assigned = k.trail[value_trail_head++];
      for (const Lit side : {assigned, negate(assigned)}) {
        const uint32_t tail = value_occ_tails.get(side);
        if (tail == UINT32_MAX)
          continue;
        uint32_t node = value_occ_nodes[tail].next;
        while (true) {
          const uint32_t gate = value_occ_nodes[node].gate;
          if (!queued(gate)) {
            set_queued(gate, true);
            rescheduled_work.push_back(gate);
          }
          if (node == tail)
            break;
          node = value_occ_nodes[node].next;
        }
      }
    }
  };

  // The closure is the long block, so it must be interruptible: abort between
  // worklist items, each all-or-nothing (binaries and proof chain both emitted,
  // or nothing). The batched root units are still asserted from what it did.
  uint32_t stop_tick = 0;
  const uint64_t closure_t0 = cong_time ? now_nanos() : 0;
  while (work_head < work_size() && !unsat) {
    if (k.stop_fuzz || !(++stop_tick & 255)) {
      if (kernel_terminating(k)) {
        k.stats.rule_interrupted++;
        break;
      }
      // The budget rides the same between-items stop as the termination flag,
      // inheriting its soundness argument verbatim.
      if (cong_budget.over(k))
        break;
    }
    cong_t.items++;
    const uint32_t i = work_head < initial_work
                           ? static_cast<uint32_t>(work_head)
                           : rescheduled_work[work_head - initial_work];
    work_head++;
    set_queued(i, false);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (!(work_head % 250000))
      congruence_stage_trace(
          k, "closure-progress", work_head,
          table[0].size() + table[1].size() + table[2].size(),
          table[0].literal_count() + table[1].literal_count() +
              table[2].literal_count(),
          occ_nodes.size());
#endif
    Lit out_i;
    bool value_derived_i = false;
    Canon ci;
    {
      Stage s(&cong_t.canon);
      ci = canonicalize(k, gates[i], find, key, out_i, cancelled, reps,
                        value_derived_i);
    }
    k.stats.congruence_ticks += 1 + gates[i].inputs.size();
    CongMatchStats &cm = cong_match[static_cast<int>(gates[i].kind)];
    if (cong_probe)
      (ci == Canon::Ok     ? cm.canon_ok
       : ci == Canon::Skip ? cm.canon_skip
       : ci == Canon::Unit || ci == Canon::InputUnit ? cm.canon_unit
                           : cm.canon_merge)++;
    if (ci == Canon::Skip)
      continue; // degenerate in a way this rule does not exploit — sound
    if (ci == Canon::InputUnit) {
      const Lit unit = key[0];
      const bool newly_fixed = k.value(unit) == 0;
      std::vector<uint64_t> chain;
      if (!congruence_rup_chain(k, &unit, 1, chain)) {
        k.stats.congruence_unjustified++;
        continue;
      }
      if (record_unit(unit, chain)) {
        unsat = true;
        break;
      }
      // Root propagation changes only the gates admitted by the immutable
      // value-occurrence index (including output positions).
      if (newly_fixed)
        reschedule_assigned_gates();
      continue;
    }
    // --- a degenerate gate is an inference, not a dead end ------------------
    if (ci == Canon::Unit) {
      // value(out_lit) = 0, so ¬out_lit is a root unit. XOR arrives with its
      // cancelled set; an AND clash arrives with `cancelled` empty, its unit
      // directly RUP, so the ladder's top==0 form just replays the chain.
      const Lit unit = negate(out_i);
      const bool newly_fixed = k.value(unit) == 0;
      std::vector<std::vector<uint64_t>> cch;
      transient.clear();
      bool justified;
      if (value_derived_i) {
        cch.resize(1);
        justified = congruence_rup_chain(k, &unit, 1, cch[0]);
      } else {
        const std::vector<std::vector<Lit>> concl = {{unit}};
        justified =
            add_xor_degeneracy_chain(k, cancelled, concl, cch, transient);
      }
      if (!justified) {
        k.stats.congruence_unjustified++;
        delete_transients(k, transient);
        continue;
      }
      const bool refuted = record_unit(unit, cch[0]);
      delete_transients(k, transient);
      if (refuted) {
        unsat = true;
        break;
      }
      if (newly_fixed)
        reschedule_assigned_gates();
      continue; // the gate is consumed by its own inference
    }
    if (ci == Canon::Merge) {
      // value(out_lit) = ⊕key = key[0], so out_lit ≡ key[0]: the unary-gate
      // merge, with no second gate needed (`docs/calculus.md` V6).
      const Lit other = key[0];
      if (find(out_i) == find(other))
        continue; // already known
      // x ≡ ¬x: justify the equivalence exactly as the normal merge would,
      // then refute instead of uniting the classes
      const bool clash = find(out_i) == negate(find(other));
      if (clash && cong_probe)
        cong_n_clash++;
      if (clash && out_i == negate(other)) {
        // literal-form contradiction: the equivalence binaries degenerate to
        // two opposing units, directly RUP for AND. The XOR form would need a
        // ladder over degenerate conclusions and stays dropped, which is sound.
        if (gates[i].kind == GateKind::And) {
          if (refute_contradictory(out_i)) {
            unsat = true;
            break;
          }
        } else
          k.stats.congruence_unjustified++;
        continue;
      }
      const std::vector<std::vector<Lit>> concl = {{negate(out_i), other},
                                                  {out_i, negate(other)}};
      std::vector<std::vector<uint64_t>> cch;
      transient.clear();
      bool justified;
      if (value_derived_i) {
        cch.resize(2);
        justified = congruence_rup_chain(k, concl[0].data(), 2, cch[0]) &&
                    congruence_rup_chain(k, concl[1].data(), 2, cch[1]);
      } else {
        justified =
            add_xor_degeneracy_chain(k, cancelled, concl, cch, transient);
      }
      if (!justified) {
        k.stats.congruence_unjustified++;
        continue; // ladder did not replay — emit nothing, drop the inference
      }
      AddedEquivalence added;
      if (!add_equivalence_chained(k, out_i, other, cch[0], cch[1],
                                   &added)) {
        k.stats.congruence_unjustified++;
        delete_transients(k, transient);
        continue; // an unjustifiable binary — drop the inference, keep N2
      }
      delete_transients(k, transient);
      if (clash) {
        if (refute_contradictory(out_i)) {
          unsat = true;
          break;
        }
        continue; // replay failed — the inference is dropped
      }
      if (!merge_lits(out_i, other, &added)) {
        k.stats.congruence_unjustified++;
        continue;
      }
      continue;
    }
    if (cong_probe) {
      bool valued = k.value(out_i) != 0;
      for (const Lit l : key)
        valued = valued || k.value(l) != 0;
      cm.val_gate += valued;
    }
    Stage table_stage(&cong_t.table);
    for (const Lit l : key)
      add_occurrence(l, i); // reschedule when a canonical input merges
    ClosureTable &tab = table[static_cast<int>(gates[i].kind)];
    const ClosureTable::Lookup lookup = tab.lookup_or_insert(key, i);
    if (lookup.inserted) {
      if (cong_probe)
        cm.insert++;
      continue;
    }
    if (cong_probe)
      cm.collide++;
    // Recompute the stored gate's canonical form: the union-find may have moved
    // on, and only a shared current key proves the outputs equal. A degenerate
    // stored gate is evicted rather than exploited here — it is still in the
    // worklist, so its own pop draws the inference.
    const uint32_t j = tab.gate(lookup.record);
    Lit out_j;
    bool value_derived_j = false;
    if (canonicalize(k, gates[j], find, key_j, out_j, cancelled_j, reps,
                     value_derived_j) !=
            Canon::Ok ||
        key_j != key) {
      if (cong_probe)
        cm.stale++;
      // Evict the stale/degenerate value while retaining the immutable key,
      // exactly like assigning through the old unordered_map iterator.
      tab.replace(lookup.record, i);
      continue;
    }
    const Lit oa = find(out_j);
    const Lit ob = find(out_i);
    if (oa == ob) {
      if (cong_probe)
        cm.already_eq++;
      continue; // already equal
    }
    // x ≡ ¬x: justify the congruence like any other match, then refute
    const bool clash = (oa == negate(ob));
    if (clash && cong_probe) {
      cong_n_clash++;
      cm.clash++;
    }
    if (clash && out_i == negate(out_j)) {
      // literal-form contradiction (same variable, opposite signs) — see the
      // Merge-branch twin of this guard.
      if (gates[i].kind == GateKind::And) {
        if (refute_contradictory(out_j)) {
          unsat = true;
          break;
        }
      } else
        k.stats.congruence_unjustified++;
      continue;
    }
    // Congruent outputs, so add oa == ob. An equivalence whose chain cannot be
    // built is skipped rather than emitted unjustified: sound, and N2 holds.
    if (gates[i].kind == GateKind::Xor || gates[i].kind == GateKind::Ite) {
      Stage proof_stage(&cong_t.proof);
      std::vector<uint64_t> cf, cr;
      transient.clear();
      const bool ok =
          gates[i].kind == GateKind::Xor
              ? add_xor_matching_proof_chain(k, key, out_j, out_i, cf, cr,
                                             transient)
              : add_ite_matching_proof_chain(k, key[0], out_j, out_i, cf, cr,
                                             transient); // key[0] = cond
      if (!ok) {
        k.stats.congruence_unjustified++;
        if (cong_probe) {
          cm.chain_fail++;
          std::fprintf(stderr, "[cong-probe] MATCH chain failed (kind=%d)\n", (int)gates[i].kind);
        }
        continue; // no checkable chain — drop the inference, keep N2
      }
      AddedEquivalence added;
      if (!add_equivalence_chained(k, out_j, out_i, cf, cr, &added)) {
        k.stats.congruence_unjustified++;
        if (cong_probe)
          cm.chain_fail++;
        delete_transients(k, transient);
        continue; // an unjustifiable binary — drop the inference, keep N2
      }
      delete_transients(k, transient);
      if (clash) {
        if (refute_contradictory(out_j)) {
          unsat = true;
          break;
        }
        continue; // replay failed — the inference is dropped
      }
      if (cong_probe)
        cm.merged++;
      if (!merge_lits(out_j, out_i, &added)) {
        k.stats.congruence_unjustified++;
        if (cong_probe)
          cm.chain_fail++;
        continue;
      }
      continue;
    } else {
      Stage proof_stage(&cong_t.proof);
      AddedEquivalence added;
      if (!add_equivalence(k, gates[j], gates[i], find, &added)) {
        k.stats.congruence_unjustified++;
        if (cong_probe)
          cm.chain_fail++;
        continue; // no checkable chain — drop the inference, keep proof valid
      }
      if (clash) {
        if (refute_contradictory(out_j)) {
          unsat = true;
          break;
        }
        continue; // replay failed — the inference is dropped
      }
      if (cong_probe)
        cm.merged++;
      if (!merge_lits(out_j, out_i, &added)) {
        k.stats.congruence_unjustified++;
        if (cong_probe)
          cm.chain_fail++;
        continue;
      }
      continue;
    }
  }
  const size_t closure_work_size = work_size();
  congruence_stage_trace(k, "closure-loop-end", work_head, closure_work_size,
                         neq, recorded_unit ? 1 : 0);

  // Forward subsumption needs only `repr` through `find`.  Releasing the gate
  // hash, dependency index, and work queue at this phase boundary prevents two
  // complete dense working sets from overlapping.  On rank 8 these dead
  // closure indices retain about 1.5 GiB; keeping them alive made even the
  // forward pass's first 16 MiB flat bucket allocation fail under 16 GiB.
  for (ClosureTable &tab : table)
    tab = ClosureTable{};
  occ_tails = PagedLiteralTails{};
  occ_nodes = PagedVector<OccNode>{};
  rescheduled_work = PagedVector<uint32_t>{};
  std::vector<uint64_t>().swap(inq);
  // The forward pass consumes only the completed representative map.  Keeping
  // the extracted gates alive here retained 20M fixed records (roughly 0.6
  // GiB on the largest Tier-D circuit) beside a wholly separate occurrence
  // index. Preserve the count needed by statistics and release the records at
  // the actual lifetime boundary.
  const size_t completed_gate_count = gates.size();
  std::vector<CongGate>().swap(gates);
  congruence_stage_trace(k, "closure-scratch-released", work_head,
                         closure_work_size, completed_gate_count,
                         recorded_unit ? 1 : 0);
  if (!unsat && (neq || recorded_unit) && !kernel_terminating(k)) {
    congruence_stage_trace(k, "closure-forward-begin", work_head,
                           closure_work_size, neq, recorded_unit ? 1 : 0);
    Stage s(&cong_t.forward);
    (void)forward_subsume_congruent_clauses(k, find);
    congruence_stage_trace(k, "closure-forward-end", work_head,
                           closure_work_size, neq, recorded_unit ? 1 : 0);
  }
  // No later phase consults the union-find: forward subsumption was its final
  // consumer. Release the dense literal representative map before root
  // propagation and any eager Substitute follow-up allocate their own state.
  std::vector<Lit>().swap(repr);
  congruence_stage_trace(k, "closure-representatives-released", work_head,
                         closure_work_size, neq, recorded_unit ? 1 : 0);
  if (cong_time) {
    cong_t.closure_all += now_nanos() - closure_t0;
    const double ms = 1e-6;
    std::fprintf(stderr,
                 "[cong-time] cumulative ms: apply %.0f | extract{and %.0f "
                 "index %.0f sort %.0f xor %.0f ite %.0f} | rebuild %.0f "
                 "binary{extract %.0f units %.0f import %.0f} setup %.0f | "
                 "closure %.0f {canon %.0f match %.0f (of which proof %.0f)} "
                 "| forward %.0f | items %llu\n",
                 (cong_t.apply_all + (now_nanos() - apply_t0)) * ms,
                 cong_t.extract_and * ms,
                 cong_t.index * ms, cong_t.sort * ms, cong_t.xor_x * ms,
                 cong_t.ite_x * ms, cong_t.rebuild * ms,
                 cong_t.binary_extract * ms, cong_t.binary_units * ms,
                 cong_t.binary_import * ms, cong_t.setup * ms,
                 cong_t.closure_all * ms, cong_t.canon * ms,
                 cong_t.table * ms, cong_t.proof * ms, cong_t.forward * ms,
                 (unsigned long long)cong_t.items);
  }
  if (cong_probe) {
    std::fprintf(stderr,
                 "[cong-probe] degeneracies (AND/XOR/clash DRAWN since C3b; "
                 "ITE-share still a discard): "
                 "AND=%llu ITE-share=%llu "
                 "XOR-cancel=%llu | contradictory merges (refuted): %llu\n",
                 (unsigned long long)cong_n_and, (unsigned long long)cong_n_ite,
                 (unsigned long long)cong_n_xor,
                 (unsigned long long)cong_n_clash);
    // The closure funnel, per kind; cumulative across every application in
    // the process, like the degeneracy counters above.
    static const char *kind_name[3] = {"and", "xor", "ite"};
    std::fprintf(stderr,
                 "[cong-probe] closure funnel (cumulative, per kind)\n"
                 "[cong-probe] %-4s %12s %12s %12s %12s %12s %10s %10s %10s "
                 "%12s\n",
                 "kind", "canon_ok", "canon_skip", "insert", "collide", "stale",
                 "already_eq", "chain_fail", "merged", "val_gate");
    for (int kk = 0; kk < 3; ++kk) {
      const CongMatchStats &c = cong_match[kk];
      std::fprintf(stderr,
                   "[cong-probe] %-4s %12llu %12llu %12llu %12llu %12llu "
                   "%10llu %10llu %10llu %12llu\n",
                   kind_name[kk], (unsigned long long)c.canon_ok,
                   (unsigned long long)c.canon_skip,
                   (unsigned long long)c.insert, (unsigned long long)c.collide,
                   (unsigned long long)c.stale,
                   (unsigned long long)c.already_eq,
                   (unsigned long long)c.chain_fail,
                   (unsigned long long)c.merged,
                   (unsigned long long)c.val_gate);
    }
    std::fprintf(stderr,
                 "[cong-probe] value channel: root-assigned at entry %llu / at "
                 "exit %llu | merges with a valued participant %llu | units "
                 "%llu\n",
                 (unsigned long long)cong_root_in,
                 (unsigned long long)cong_root_out,
                 (unsigned long long)cong_val_merge,
                 (unsigned long long)k.stats.congruence_units);
  }
  if (std::getenv("TACTIX_CONG_TRACE"))
    std::fprintf(stderr,
                 "[cong] %zu gates, %llu binaries extracted, %llu "
                 "equivalences added\n",
                 completed_gate_count,
                 (unsigned long long)extracted_binaries,
                 (unsigned long long)neq);
  k.stats.congruence_gates += completed_gate_count;
  k.stats.congruence_merges += neq;
  if (unsat)
    return ApplyResult::Unsat;

  // Assert the batch of derived root units, then close the root. On a miter this
  // is what refutes at zero conflicts: congruence proves aᵢ ≡ bᵢ for the
  // matching signals, every output XOR cancels to the unit ¬outᵢ, and
  // propagating those falsifies the output constraint.
  return finish(/*merged=*/neq != 0);
}

uint32_t congruence_instances(Kernel &, uint64_t *out, uint32_t cap) {
  if (cap >= 1)
    out[0] = INSTANCE_CANONICAL;
  return 1;
}

ApplyResult congruence_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return congruence_apply(k);
  return ApplyResult::NoOp; // only the canonical (full-round) instance
}

TACTIX_REGISTER_RULE(Congruence, RuleTier::Inprocess, congruence_guard,
                     congruence_apply, congruence_instances,
                     congruence_apply_instance);

} // namespace tactix
