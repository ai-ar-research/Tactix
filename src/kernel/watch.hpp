// Watch scheme (transliterated from kissat/src/watch.h + vectors.c, MIT):
// watch lists are slices of ONE flat uint32_t arena, addressed by
// per-literal {offset, size, capacity} heads — no per-list heap objects,
// contiguous hot data. A binary clause occupies ONE slot (other literal,
// low-bit tag); a large clause TWO consecutive slots (blocking literal,
// then arena clause ref).
//
// Growth relocates a list to the arena end (amortized doubling) and leaves
// a hole; when holes exceed half the arena it is defragmented. Because a
// push can therefore move ANY list (or the whole arena on reallocation),
// pushes must never happen while a list is being scanned — propagation
// delays its watch relocations and flushes them after each scan (the
// reason kissat has `delayed`; see rules/propagate).
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
};

struct WatchLists {
  std::vector<uint32_t> arena;
  std::vector<WatchHead> heads; // by lit
  uint64_t holes = 0;           // dead words left by relocations

  void resize_lits(size_t nlits) { heads.resize(nlits); }

  uint32_t *begin(Lit l) { return arena.data() + heads[l].offset; }
  uint32_t *end(Lit l) { return begin(l) + heads[l].size; }
  uint32_t size(Lit l) const { return heads[l].size; }

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
  void push_binary(Lit l, Lit other) { push(l, binary_watch(other).raw); }
  void push_large(Lit l, Lit blocking, Ref ref) {
    push(l, blocking_watch(blocking).raw);
    push(l, ref);
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
