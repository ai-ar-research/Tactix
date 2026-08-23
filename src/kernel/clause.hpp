// Clause arena layout (transliterated idea from kissat/src/clause.h and
// arena.h, MIT): all large clauses (size >= 3) live contiguously in one
// uint32_t arena addressed by 32-bit word offsets. Binary clauses never
// materialize — they exist only inside watch lists (their proof ids live
// in Kernel::binary_ids). Header:
//   word0  size
//   word1  bit0 garbage | bit1 redundant | bit2 reason | bits3-7 used
//          | bit8 vivify | bit9 shrunken | bits10.. glue
//   word2  searched (index of last replacement-scan position, >= 2)
//   word3  id_lo   (proof clause id, low word — native LRAT hints;
//   word4  id_hi    CaDiCaL stores int64 ids the same way)
//   word5+ literals
#pragma once

#include "kernel/types.hpp"

#include <vector>

namespace tactix {

struct Clause {
  uint32_t size;
  uint32_t flags;
  uint32_t searched;
  uint32_t id_lo, id_hi; // proof clause id (uint64, split)
  Lit lits[1]; // actually `size` literals

  uint64_t id() const {
    return (static_cast<uint64_t>(id_hi) << 32) | id_lo;
  }
  void set_id(uint64_t id) {
    id_lo = static_cast<uint32_t>(id);
    id_hi = static_cast<uint32_t>(id >> 32);
  }

  static constexpr uint32_t GARBAGE = 1u << 0;
  static constexpr uint32_t REDUNDANT = 1u << 1;
  static constexpr uint32_t REASON = 1u << 2; // transient lock during Reduce
  // recently-used counter (kissat clause.h `used:5`): saturated on learn
  // and on every use in conflict analysis, aged down by Reduce — protects
  // active clauses from deletion (see reduce.cpp)
  static constexpr uint32_t USED_SHIFT = 3;
  static constexpr uint32_t USED_BITS = 5;
  static constexpr uint32_t MAX_USED = (1u << USED_BITS) - 1;
  static constexpr uint32_t USED_MASK = MAX_USED << USED_SHIFT;
  // Vivify's leftover-priority bit (kissat clause.h `vivify`, cadical
  // Clause::vivify): set on every clause Vivify schedules, cleared the moment
  // one is TRIED, so what survives a budget cut is exactly the untried tail and
  // that tail heads the next round. A PRIORITY mechanism, never a suppressor —
  // when the leftover set empties, the whole scope is re-armed. Taken from
  // `glue`'s span: set_glue saturates and glue is bounded by clause size, so 23
  // bits (8.4M) is as unreachable as 24 was.
  static constexpr uint32_t VIVIFY = 1u << 8;
  // A shrunken clause retains its original arena allocation.  The semantic
  // size remains `size`; INVALID_LIT at the original final literal records the
  // stable allocation boundary exactly as in Kissat's `clause::shrunken`.
  // This is representation metadata only: proof and rule semantics never
  // inspect it.
  static constexpr uint32_t SHRUNKEN = 1u << 9;
  static constexpr uint32_t GLUE_SHIFT = 10;
  static constexpr uint32_t MAX_GLUE = (1u << (32 - GLUE_SHIFT)) - 1;

  bool garbage() const { return flags & GARBAGE; }
  bool redundant() const { return flags & REDUNDANT; }
  bool reason() const { return flags & REASON; }
  bool vivify() const { return flags & VIVIFY; }
  bool shrunken() const { return flags & SHRUNKEN; }
  void set_redundant(bool v) {
    flags = v ? (flags | REDUNDANT) : (flags & ~REDUNDANT);
    if (!v)
      set_glue(0);
  }
  void set_vivify(bool v) {
    flags = v ? (flags | VIVIFY) : (flags & ~VIVIFY);
  }
  uint32_t glue() const { return flags >> GLUE_SHIFT; }
  uint32_t used() const { return (flags & USED_MASK) >> USED_SHIFT; }
  void set_used(uint32_t u) {
    flags = (flags & ~USED_MASK) | (u << USED_SHIFT);
  }
  void set_glue(uint32_t g) {
    if (g > MAX_GLUE)
      g = MAX_GLUE;
    flags = (flags & ((1u << GLUE_SHIFT) - 1)) | (g << GLUE_SHIFT);
  }
  Lit *begin() { return lits; }
  Lit *end() { return lits + size; }
  const Lit *begin() const { return lits; }
  const Lit *end() const { return lits + size; }
};

constexpr uint32_t CLAUSE_HEADER_WORDS = 5;

inline uint32_t clause_words(uint32_t size) {
  return CLAUSE_HEADER_WORDS + size;
}

// Arena traversal must advance over the allocation, not the current semantic
// clause.  On the first in-place shrink the original last literal is replaced
// by INVALID_LIT; subsequent shrinks retain that same boundary marker.
inline uint32_t clause_actual_words(const Clause *c) {
  if (!c->shrunken())
    return clause_words(c->size);
  const Lit *p = c->end();
  while (*p++ != INVALID_LIT)
    ;
  return static_cast<uint32_t>(reinterpret_cast<const uint32_t *>(p) -
                               reinterpret_cast<const uint32_t *>(c));
}

using Arena = std::vector<uint32_t>;

inline Clause *clause_at(Arena &arena, Ref ref) {
  assert(ref + CLAUSE_HEADER_WORDS <= arena.size());
  return reinterpret_cast<Clause *>(arena.data() + ref);
}
// hot-path overload for a hoisted arena data pointer (see propagate)
inline Clause *clause_at(uint32_t *arena_data, Ref ref) {
  return reinterpret_cast<Clause *>(arena_data + ref);
}
inline const Clause *clause_at(const Arena &arena, Ref ref) {
  return reinterpret_cast<const Clause *>(arena.data() + ref);
}

} // namespace tactix
