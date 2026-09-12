// Kissat watch/vector scheme (watch.h, inlinevector.h, vector.c; MIT): literal
// lists are slices of one uint32_t arena. A binary uses one word; a large watch
// uses a blocking-literal word plus a clause-ref word. Tail vectors extend in
// place while the global allocation has room; otherwise they relocate to a
// doubled slice. Dead and spare words match `vectors.usable`, and only the
// post-clause checkpoint may defragment. A scan therefore delays relocations
// until its input pointers are closed (proplit.h's `delayed` stack).
#pragma once

#include "kernel/types.hpp"

#include <algorithm>
#include <vector>

namespace tactix {

struct Watch {
  uint32_t raw;
  bool binary() const { return raw & 1; }
  Lit other() const { return raw >> 1; }    // binary: the other literal
  Lit blocking() const { return raw >> 1; } // large: blocking literal
};

inline Watch binary_watch(Lit other) { return {(other << 1) | 1u}; }
inline Watch blocking_watch(Lit blocking) { return {blocking << 1}; }

struct WatchHead {
  uint32_t offset = 0, size = 0, capacity = 0;
  // `size = binaries + 2 * large_entries`, so the low bit of the exact
  // binary population is already determined by `size`. Reuse that redundant
  // bit for the only independent order flag and keep a full-width count.
  uint32_t binary_count_and_order = 0;

  uint32_t binary_count() const {
    return (binary_count_and_order & ~uint32_t{1}) | (size & 1u);
  }
  bool binaries_first() const { return !(binary_count_and_order & 1u); }
  bool has_large() const { return binary_count() != size; }

  void set_layout(uint32_t binaries, bool first = true) {
    assert(binaries <= size);
    assert(!((size - binaries) & 1u));
    binary_count_and_order =
        (binaries & ~uint32_t{1}) | static_cast<uint32_t>(!first);
  }
  void set_binaries_first(bool first) { set_layout(binary_count(), first); }
  void add_binary_word() {
    const uint32_t binaries = binary_count();
    const bool first = binaries_first();
    assert(size < UINT32_MAX && binaries < UINT32_MAX);
    size++;
    set_layout(binaries + 1, first);
  }
  void add_large_words() {
    const uint32_t binaries = binary_count();
    const bool first = binaries_first();
    assert(size <= UINT32_MAX - 2);
    size += 2;
    set_layout(binaries, first);
  }
};
static_assert(sizeof(WatchHead) == 16,
              "hot watch heads must remain four words");

// One ENTRY of a watch list, as seen by a reader. A binary entry is one word, a
// large entry two (blocking word, then the arena ref), so a reader must step per
// entry and never per word: a word-wise walk reinterprets a large clause's ref
// as a watch. `ref()` is valid only on a large entry.
struct WatchEntry {
  const uint32_t *word;
  bool binary() const { return Watch{*word}.binary(); }
  Lit other() const { return Watch{*word}.other(); }
  Lit blocking() const { return Watch{*word}.blocking(); }
  Ref ref() const { return word[1]; }
};

// Forward iterator whose ++ carries the per-entry stride, so no reader open-codes
// it (that stride was transcribed at every reader before this existed, and a
// wrong one is silently wrong rather than a crash). Range-for shaped rather than
// callback shaped on purpose: `break`/`continue`/`return` in a reader's body keep
// their exact meaning, which is what makes converting a reader to it provably
// behaviour-preserving.
class WatchIter {
public:
  explicit WatchIter(const uint32_t *p) : p_(p) {}
  WatchEntry operator*() const { return WatchEntry{p_}; }
  WatchIter &operator++() {
    p_ += Watch{*p_}.binary() ? 1u : 2u;
    return *this;
  }
  bool operator!=(const WatchIter &o) const { return p_ != o.p_; }

private:
  const uint32_t *p_;
};

// The slice is snapshotted at construction: a reader must not push to any watch
// list while iterating (a push can relocate the list or the whole arena — see the
// file header). Every open-coded reader already had that precondition.
struct WatchRange {
  const uint32_t *first, *last;
  WatchIter begin() const { return WatchIter{first}; }
  WatchIter end() const { return WatchIter{last}; }
  bool empty() const { return first == last; }
};

struct WatchLists {
  std::vector<uint32_t> arena;
  std::vector<WatchHead> heads; // by lit
  // Exact analogue of Kissat's `vectors.usable`: dead relocated words plus
  // unused capacity inside every live literal slice.
  uint64_t holes = 0;

  void resize_lits(size_t nlits) { heads.resize(nlits); }

  // Live binary clauses. Every list already carries its exact binary count, so
  // this sums one word per literal where walking the entries costs one step per
  // watch. kissat reads the same quantity from `statistics.clauses_binary`.
  uint64_t binary_clauses() const {
    uint64_t words = 0;
    for (const WatchHead &head : heads)
      words += head.binary_count();
    assert(!(words & 1));
    return words / 2;
  }

  // collect.h defaults: inspect only after creating a clause, only above
  // 2^18 words, and only when strictly more than 75% of the allocated vector
  // stack is unusable.  In particular propagation itself never defragments.
  void defrag_if_needed() {
    constexpr size_t SIZE_LIMIT = size_t{1} << 18;
    // Kissat reserves stack offset zero as the empty-vector sentinel.  Tactix
    // has an explicit empty head and omits that stored word, so add it back
    // only for the pinned SIZE_STACK predicate.
    const size_t stack_size = arena.empty() ? 0 : arena.size() + 1;
    if (stack_size <= SIZE_LIMIT)
      return;
    const uint64_t usable_limit = (uint64_t{stack_size} * 75) / 100;
    if (holes <= usable_limit)
      return;
    defrag();
  }

  uint32_t *begin(Lit l) { return arena.data() + heads[l].offset; }
  uint32_t *end(Lit l) { return begin(l) + heads[l].size; }
  uint32_t size(Lit l) const { return heads[l].size; }
  uint32_t binary_size(Lit l) const { return heads[l].binary_count(); }

  // Entry-aware read-only walk of l's list — the one iteration primitive every
  // reader outside the propagation engines goes through (§5.3 puts watch
  // maintenance in the kernel).
  WatchRange entries(Lit l) const {
    const uint32_t *b = arena.data() + heads[l].offset;
    return {b, b + heads[l].size};
  }

  // In-place, entry-aware compaction: `keep(WatchEntry)` decides per ENTRY, and a
  // dropped large entry loses BOTH its words. Same write-cursor the removal sites
  // open-coded, in one place — no growth, so no other list moves.
  template <class Keep> void compact(Lit l, Keep keep) {
    uint32_t *const b = begin(l);
    uint32_t *q = b;
    const uint32_t *p = b;
    const uint32_t *const e = end(l);
    uint32_t binaries = 0;
    bool has_large = false, binaries_first = true;
    while (p != e) {
      const bool bin = Watch{*p}.binary();
      if (keep(WatchEntry{p})) {
        if (bin) {
          binaries++;
          binaries_first = binaries_first && !has_large;
        } else {
          has_large = true;
        }
        *q++ = *p++;
        if (!bin)
          *q++ = *p++;
      } else {
        p += bin ? 1 : 2;
      }
    }
    WatchHead &head = heads[l];
    const uint32_t old_size = head.size;
    head.size = static_cast<uint32_t>(q - b);
    holes += old_size - head.size;
    head.set_layout(binaries, binaries_first);
    assert(head.has_large() == has_large);
  }

  // Rewrite the clause reference of every retained large entry in place.
  // Collection uses this after entry-aware compaction: prefix watch order and
  // blockers stay byte-identical while arena references are forwarded around
  // garbage records that preceded the sparse-collection watermark.
  template <class Map> void map_large_refs(Lit l, Map map) {
    uint32_t *p = begin(l);
    uint32_t *const e = end(l);
    while (p != e) {
      if (Watch{*p}.binary()) {
        p++;
      } else {
        p[1] = map(p[1]);
        p += 2;
      }
    }
  }

  // Shrink after dropping only large entries (capacity is retained and
  // reused). Binary population and relative layout are unchanged.
  void truncate_large(Lit l, uint32_t n) {
    WatchHead &head = heads[l];
    assert(n <= head.size);
    const uint32_t binaries = head.binary_count();
    const bool first = head.binaries_first();
    holes += head.size - n;
    head.size = n;
    head.set_layout(binaries, first);
  }

  // Restore or shrink a snapshotted list while keeping its existing capacity.
  // Growing consumes invalid tail words; shrinking creates them.
  void resize_live(Lit l, uint32_t n) {
    WatchHead &head = heads[l];
    assert(n <= head.capacity);
    if (n > head.size) {
      const uint32_t delta = n - head.size;
      assert(holes >= delta);
      holes -= delta;
    } else {
      holes += head.size - n;
    }
    head.size = n;
  }

  // Batch rebuild support. A caller that knows every forthcoming per-literal
  // append can replace the fragmented arena once with exact final slices,
  // then fill them through ordinary `push` calls without any relocation.
  // Existing entry order is preserved as the prefix of each new slice.
  void begin_batch_reserve() {
    for (WatchHead &head : heads)
      head.capacity = head.size;
  }
  void reserve_batch_word(Lit lit) {
    assert(heads[lit].capacity < UINT32_MAX);
    heads[lit].capacity++;
  }
  void finish_batch_reserve(bool preserve_global_capacity = true) {
    size_t total = 0;
    for (const WatchHead &head : heads)
      total += head.capacity;
    assert(total <= UINT32_MAX);
    // Packing must not erase global append headroom that the normal arena
    // growth policy already paid for. In particular, an exact-size vector
    // would double at the first post-batch learned or large-clause watch.
    const size_t reserve_floor = preserve_global_capacity ? arena.capacity() : 0;
    std::vector<uint32_t> fresh;
    fresh.reserve(std::max(total, reserve_floor));
    fresh.resize(total);
    size_t next = 0;
    for (WatchHead &head : heads) {
      const uint32_t old_offset = head.offset;
      std::copy_n(arena.begin() + old_offset, head.size,
                  fresh.begin() + next);
      head.offset = static_cast<uint32_t>(next);
      next += head.capacity;
    }
    assert(next == total);
    arena.swap(fresh);
    size_t live = 0;
    for (const WatchHead &head : heads)
      live += head.size;
    holes = total - live;
  }

  // make room for `n` words in l's slice without changing its contents.
  // Needed by the probing restore (kernel.cpp): a list snapshotted at size n
  // can be compacted and then have its capacity clamped to the smaller size by
  // an intervening defrag, so putting the snapshot back may have to re-grow.
  void ensure_capacity(Lit l, uint32_t n) {
    while (heads[l].capacity < n)
      grow(l);
  }

  void push(Lit l, uint32_t word) {
    WatchHead &h = heads[l];
    if (h.size == h.capacity)
      grow(l);
    assert(holes);
    holes--;
    arena[heads[l].offset + heads[l].size++] = word;
  }
  void push_binary(Lit l, Lit other) {
    WatchHead &h = heads[l];
    const uint32_t binaries = h.binary_count();
    bool first = h.binaries_first();
    if (h.has_large())
      first = false;
    push(l, binary_watch(other).raw);
    h.set_layout(binaries + 1, first);
  }
  void push_large(Lit l, Lit blocking, Ref ref) {
    WatchHead &h = heads[l];
    if (h.capacity - h.size >= 2) {
      // Both words fit the existing slice: no growth or relocation can occur.
      // Adding two preserves size parity, hence the packed binary/order image.
      assert(holes >= 2 && h.size <= UINT32_MAX - 2);
      uint32_t *const dst = arena.data() + h.offset + h.size;
      dst[0] = blocking_watch(blocking).raw;
      dst[1] = ref;
      h.size += 2;
      holes -= 2;
      return;
    }
    const uint32_t binaries = h.binary_count();
    const bool first = h.binaries_first();
    push(l, blocking_watch(blocking).raw);
    push(l, ref);
    h.set_layout(binaries, first);
  }

private:
  void grow(Lit l) {
    WatchHead &h = heads[l];
    assert(h.size == h.capacity);
    if (arena.capacity() == 0)
      // Kissat's first unsigned-stack allocation has four words, one of which
      // is the offset-zero empty-vector sentinel omitted by this representation.
      arena.reserve(3);
    // inlinevector.h: a vector ending at the global stack end consumes one
    // more stack word directly whenever the stack allocation has room.
    if (static_cast<size_t>(h.offset) + h.size == arena.size() &&
        arena.size() < arena.capacity()) {
      arena.resize(arena.size() + 1);
      h.capacity++;
      holes++;
      return;
    }
    const uint32_t ncap = h.size ? 2 * h.size : 1;
    assert(static_cast<uint64_t>(arena.size()) + ncap <= UINT32_MAX);
    const size_t needed = arena.size() + ncap;
    if (needed > arena.capacity()) {
      // ENLARGE_STACK doubles its power-of-two allocation.  The physical
      // Tactix vector omits the sentinel, so each mapped capacity is 2^k-1.
      size_t stack_capacity = 4;
      while (stack_capacity - 1 < needed)
        stack_capacity *= 2;
      arena.reserve(stack_capacity - 1);
    }
    const uint32_t noff = static_cast<uint32_t>(arena.size());
    arena.resize(arena.size() + ncap);
    // h may dangle after resize; re-index
    WatchHead &hh = heads[l];
    std::copy(arena.begin() + hh.offset, arena.begin() + hh.offset + hh.size,
              arena.begin() + noff);
    // The old live image and the unused half of the new image are both usable,
    // exactly the accounting in vector.c:kissat_enlarge_vector.
    holes += ncap;
    hh.offset = noff;
    hh.capacity = ncap;
  }

  void defrag() {
    std::vector<Lit> order;
    order.reserve(heads.size());
    for (Lit lit = 0; lit < heads.size(); lit++)
      if (heads[lit].size)
        order.push_back(lit);
    std::sort(order.begin(), order.end(), [&](Lit a, Lit b) {
      return heads[a].offset < heads[b].offset;
    });
    size_t live = 0;
    for (const Lit lit : order)
      live += heads[lit].size;
    // SHRINK_STACK keeps the smallest power-of-two stack containing the
    // offset-zero sentinel and all live words (unlike first allocation, this
    // may be one or two words).
    size_t stack_capacity = 1;
    while (stack_capacity < live + 1)
      stack_capacity *= 2;
    std::vector<uint32_t> fresh;
    if (live)
      fresh.reserve(stack_capacity - 1);
    for (const Lit lit : order) {
      WatchHead &h = heads[lit];
      const uint32_t noff = static_cast<uint32_t>(fresh.size());
      fresh.insert(fresh.end(), arena.begin() + h.offset,
                   arena.begin() + h.offset + h.size);
      h.offset = noff;
      h.capacity = h.size;
    }
    for (WatchHead &h : heads)
      if (!h.size)
        h = WatchHead{};
    arena.swap(fresh);
    holes = 0;
  }
};

} // namespace tactix
