// Kissat vector-stack primitives (vector.c, inlinevector.h, collect.h) shared
// by watch.hpp and occ.hpp: slices of one flat word arena with head `offset`,
// `size`, `capacity`; `holes` is Kissat's `vectors.usable`.
#pragma once

#include "kernel/types.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tactix {

// collect.h defaults: only after creating a clause, above 2^18 words, and
// when more than 75% of the stack is unusable.
template <class Arena>
inline bool vector_stack_needs_defrag(const Arena &arena, uint64_t holes) {
  constexpr size_t SIZE_LIMIT = size_t{1} << 18;
  // Kissat's offset-zero sentinel word, added back for SIZE_STACK.
  const size_t stack_size = arena.empty() ? 0 : arena.size() + 1;
  if (stack_size <= SIZE_LIMIT)
    return false;
  const uint64_t usable_limit = (uint64_t{stack_size} * 75) / 100;
  return holes > usable_limit;
}

// Make room for one more word in `l`'s full slice. Returns whether the slice
// relocated (as opposed to extending in place at the arena end).
template <class Arena, class Head>
inline bool vector_stack_grow(Arena &arena, std::vector<Head> &heads, Lit l,
                       uint64_t &holes) {
  Head &h = heads[l];
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
    return false;
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
  Head &hh = heads[l];
  std::copy(arena.begin() + hh.offset, arena.begin() + hh.offset + hh.size,
            arena.begin() + noff);
  // The old live image and the unused half of the new image are both usable,
  // exactly the accounting in vector.c:kissat_enlarge_vector.
  holes += ncap;
  hh.offset = noff;
  hh.capacity = ncap;
  return true;
}

// Repack every live slice, in current arena order, into a fresh exact-fit
// stack; empty heads return to their default state.
template <class Arena, class Head>
inline void vector_stack_defrag(Arena &arena, std::vector<Head> &heads,
                         uint64_t &holes) {
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
  // SHRINK_STACK: the smallest power of two holding the sentinel and all live
  // words.
  size_t stack_capacity = 1;
  while (stack_capacity < live + 1)
    stack_capacity *= 2;
  Arena fresh;
  if (live)
    fresh.reserve(stack_capacity - 1);
  for (const Lit lit : order) {
    Head &h = heads[lit];
    const uint32_t noff = static_cast<uint32_t>(fresh.size());
    fresh.insert(fresh.end(), arena.begin() + h.offset,
                 arena.begin() + h.offset + h.size);
    h.offset = noff;
    h.capacity = h.size;
  }
  for (Head &h : heads)
    if (!h.size)
      h = Head{};
  arena.swap(fresh);
  holes = 0;
}

} // namespace tactix
