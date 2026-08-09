// Watch scheme (transliterated from kissat/src/watch.h + vectors.c, MIT):
// watch lists are slices of one flat uint32_t arena, addressed by per-literal
// {offset, size, capacity} heads — no per-list heap objects, contiguous hot
// data. A binary clause occupies ONE slot (other literal, low-bit tag); a large
// clause TWO consecutive slots (blocking literal, then arena clause ref).
//
// Growth relocates a list to the arena end (amortized doubling) and leaves a
// hole; when holes exceed half the arena it is defragmented. Because a push can
// therefore move any list, or the whole arena on reallocation, pushes must
// never happen while a list is being scanned — propagation delays its watch
// relocations and flushes them after each scan (kissat's `delayed`).
#pragma once

#include "kernel/types.hpp"

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
  bool has_large = false;
  bool binaries_first = true;
};

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
  uint64_t holes = 0;           // dead words left by relocations

  void resize_lits(size_t nlits) { heads.resize(nlits); }

  uint32_t *begin(Lit l) { return arena.data() + heads[l].offset; }
  uint32_t *end(Lit l) { return begin(l) + heads[l].size; }
  uint32_t size(Lit l) const { return heads[l].size; }

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
    bool has_large = false, binaries_first = true;
    while (p != e) {
      const bool bin = Watch{*p}.binary();
      if (keep(WatchEntry{p})) {
        if (bin)
          binaries_first = binaries_first && !has_large;
        else
          has_large = true;
        *q++ = *p++;
        if (!bin)
          *q++ = *p++;
      } else {
        p += bin ? 1 : 2;
      }
    }
    truncate(l, static_cast<uint32_t>(q - b));
    heads[l].has_large = has_large;
    heads[l].binaries_first = binaries_first;
  }

  // shrink after in-place compaction (capacity is retained and reused)
  void truncate(Lit l, uint32_t n) {
    assert(n <= heads[l].size);
    heads[l].size = n;
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
    arena[heads[l].offset + heads[l].size++] = word;
  }
  void push_binary(Lit l, Lit other) {
    WatchHead &h = heads[l];
    if (h.has_large)
      h.binaries_first = false;
    push(l, binary_watch(other).raw);
  }
  void push_large(Lit l, Lit blocking, Ref ref) {
    push(l, blocking_watch(blocking).raw);
    push(l, ref);
    heads[l].has_large = true;
  }

private:
  void grow(Lit l) {
    WatchHead &h = heads[l];
    const uint32_t ncap = h.capacity ? 2 * h.capacity : 4;
    const uint32_t noff = static_cast<uint32_t>(arena.size());
    arena.resize(arena.size() + ncap);
    // h may dangle after resize; re-index
    WatchHead &hh = heads[l];
    std::copy(arena.begin() + hh.offset, arena.begin() + hh.offset + hh.size,
              arena.begin() + noff);
    holes += hh.capacity;
    hh.offset = noff;
    hh.capacity = ncap;
    if (holes > arena.size() / 2 && arena.size() > (1u << 16))
      defrag();
  }

  void defrag() {
    std::vector<uint32_t> fresh;
    fresh.reserve(arena.size() - holes);
    for (WatchHead &h : heads) {
      const uint32_t noff = static_cast<uint32_t>(fresh.size());
      fresh.insert(fresh.end(), arena.begin() + h.offset,
                   arena.begin() + h.offset + h.size);
      h.offset = noff;
      h.capacity = h.size;
    }
    arena.swap(fresh);
    holes = 0;
  }
};

} // namespace tactix
