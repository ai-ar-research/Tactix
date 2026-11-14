// Kissat watch/vector scheme (watch.h, inlinevector.h, vector.c): literal
// lists are slices of one uint32_t arena; a binary uses one word, a large
// watch a blocking literal plus a clause ref. Tail vectors extend in place,
// else relocate to a doubled slice; dead words match `vectors.usable`, and
// only the post-clause checkpoint defragments, so a scan delays relocations
// (proplit.h's `delayed` stack).
#pragma once

#include "kernel/types.hpp"
#include "kernel/vector_stack.hpp"

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
  // `size = binaries + 2 * large_entries` fixes the count's low bit, which
  // instead holds the order flag.
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

// One entry of a watch list: a binary is one word, a large entry two
// (blocking word, then arena ref), so readers step per entry. `ref()` is
// valid only on a large entry.
struct WatchEntry {
  const uint32_t *word;
  bool binary() const { return Watch{*word}.binary(); }
  Lit other() const { return Watch{*word}.other(); }
  Lit blocking() const { return Watch{*word}.blocking(); }
  Ref ref() const { return word[1]; }
};

// Forward iterator carrying the per-entry stride; range-for shaped so
// `break`/`continue`/`return` keep their meaning.
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

// The slice is snapshotted at construction: a reader must not push to any
// watch list while iterating.
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

  // Live binary clauses from the per-list counts (kissat
  // `statistics.clauses_binary`).
  uint64_t binary_clauses() const {
    uint64_t words = 0;
    for (const WatchHead &head : heads)
      words += head.binary_count();
    assert(!(words & 1));
    return words / 2;
  }

  // The collect.h checkpoint (vector_stack.hpp).
  void defrag_if_needed() {
    if (vector_stack_needs_defrag(arena, holes))
      vector_stack_defrag(arena, heads, holes);
  }

  uint32_t *begin(Lit l) { return arena.data() + heads[l].offset; }
  uint32_t *end(Lit l) { return begin(l) + heads[l].size; }
  uint32_t size(Lit l) const { return heads[l].size; }
  uint32_t binary_size(Lit l) const { return heads[l].binary_count(); }

  // Entry-aware read-only walk of l's list, used by every reader outside the
  // propagation engines.
  WatchRange entries(Lit l) const {
    const uint32_t *b = arena.data() + heads[l].offset;
    return {b, b + heads[l].size};
  }

  // In-place entry-aware compaction: `keep(WatchEntry)` decides per entry; a
  // dropped large entry loses both words. No growth, so no list moves.
  template <class Keep> void compact(Lit l, Keep keep) {
    compact_map_large(l, keep, [](uint32_t ref) { return ref; });
  }

  // compact() that also forwards retained large references in the same pass;
  // same stable-order contract.
  template <class Keep, class Map>
  void compact_map_large(Lit l, Keep keep, Map map) {
    uint32_t *const b = begin(l);
    uint32_t *q = b;
    const uint32_t *p = b;
    const uint32_t *const e = end(l);
    uint32_t binaries = 0;
    bool has_large = false, binaries_first = true;
    while (p != e) {
      const bool bin = Watch{*p}.binary();
      const WatchEntry entry{p};
      if (keep(entry)) {
        if (bin) {
          binaries++;
          binaries_first = binaries_first && !has_large;
          *q++ = *p++;
        } else {
          has_large = true;
          *q++ = *p++;
          *q++ = map(*p++);
        }
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

  // Batch rebuild: replace the arena once with exact final slices (existing
  // entries as prefix), then fill by `push` without relocation.
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
    // Keep the arena's paid-for global append headroom.
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

  // Make room for `n` words in l's slice without changing its contents (the
  // probing restore may follow a capacity-clamping defrag).
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
  void grow(Lit l) { vector_stack_grow(arena, heads, l, holes); }
};

} // namespace tactix
