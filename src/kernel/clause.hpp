// Clause arena layout (after kissat/src/clause.h and arena.h): large clauses
// (size >= 3) live contiguously in one uint32_t arena addressed by word
// offsets; binaries exist only in watch lists (ids in Kernel::binary_ids).
//   word0  size
//   word1  bit0 garbage | bit1 redundant | bit2 reason | bits3-7 used
//          | bit8 vivify | bit9 shrunken | bits10-11 Factor scratch
//          | bit12 detached reason | bits13.. glue
//   word2  searched (last replacement-scan position, >= 2)
//   word3-4 proof clause id, low then high word (as CaDiCaL)
//   word5+ literals
#pragma once

#include "kernel/types.hpp"
#include "kernel/realloc_words.hpp"

#include <utility>
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
  // recently-used counter (kissat clause.h `used:5`): saturated on learn and
  // use in conflict analysis, aged by Reduce (reduce.cpp)
  static constexpr uint32_t USED_SHIFT = 3;
  static constexpr uint32_t USED_BITS = 5;
  static constexpr uint32_t MAX_USED = (1u << USED_BITS) - 1;
  static constexpr uint32_t USED_MASK = MAX_USED << USED_SHIFT;
  // Vivify's leftover-priority bit (kissat clause.h `vivify`, cadical
  // Clause::vivify): set when scheduled, cleared when tried, so the untried
  // tail heads the next round; re-armed when empty. Glue keeps 23 bits.
  static constexpr uint32_t VIVIFY = 1u << 8;
  // A shrunken clause keeps its original allocation: INVALID_LIT at the
  // original final literal marks the boundary (Kissat `clause::shrunken`).
  // Representation metadata only.
  static constexpr uint32_t SHRUNKEN = 1u << 9;
  // Factor's two transient marks (kissat `clause::quotient`): occurrence
  // reserved and occurrence matched by a column; in the header so a dense scan
  // reads them with `size`. Cleared before Factor returns.
  static constexpr uint32_t FACTOR_RESERVED = 1u << 10;
  static constexpr uint32_t FACTOR_USED = 1u << 11;
  // Persistent record kind for a proof-visible reason absent from the watched
  // formula (REASON is Reduce's transient lock).
  static constexpr uint32_t DETACHED_REASON = 1u << 12;
  // Bit 13 remains unused so this staged native-reason cutover does not alter
  // the packed glue layout of ordinary clauses.
  static constexpr uint32_t GLUE_SHIFT = 14;
  static constexpr uint32_t MAX_GLUE = (1u << (32 - GLUE_SHIFT)) - 1;

  bool garbage() const { return flags & GARBAGE; }
  bool redundant() const { return flags & REDUNDANT; }
  bool reason() const { return flags & REASON; }
  bool vivify() const { return flags & VIVIFY; }
  bool shrunken() const { return flags & SHRUNKEN; }
  bool factor_reserved() const { return flags & FACTOR_RESERVED; }
  bool factor_used() const { return flags & FACTOR_USED; }
  bool detached_reason() const { return flags & DETACHED_REASON; }
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

// Arena traversal steps over the whole allocation; an in-place shrink marks
// the original last literal with INVALID_LIT.
inline uint32_t clause_actual_words(const Clause *c) {
  if (!c->shrunken())
    return clause_words(c->size);
  const Lit *p = c->end();
  while (*p++ != INVALID_LIT)
    ;
  return static_cast<uint32_t>(reinterpret_cast<const uint32_t *>(p) -
                               reinterpret_cast<const uint32_t *>(c));
}

// Growable word arena (Kissat's arena reallocs); Refs are word offsets and no
// Clause pointer survives growth. Collection builds a fresh arena.
using Arena = ReallocWords<uint32_t>;

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

// Every record of an arena in address order, garbage included:
// `for (const auto [ref, c] : arena_records(k.arena))`. Advancing steps over
// the allocation (`clause_actual_words`). Always inlined, so each walk keeps
// its hand-written code shape.
template <class A> struct ArenaRecords {
  struct Record {
    Ref ref;
    decltype(clause_at(std::declval<A &>(), Ref{})) clause;
  };
  struct End {};
  struct Iterator {
    A *arena;
    Ref ref;
    [[gnu::always_inline]] Record operator*() const {
      return {ref, clause_at(*arena, ref)};
    }
    [[gnu::always_inline]] Iterator &operator++() {
      ref += clause_actual_words(clause_at(*arena, ref));
      return *this;
    }
    [[gnu::always_inline]] bool operator!=(End) const {
      return ref < arena->size();
    }
  };
  A &arena;
  [[gnu::always_inline]] Iterator begin() const { return {&arena, 0}; }
  [[gnu::always_inline]] End end() const { return {}; }
};

template <class A>
[[gnu::always_inline]] inline ArenaRecords<A> arena_records(A &arena) {
  return {arena};
}

} // namespace tactix
