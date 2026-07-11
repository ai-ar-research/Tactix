// Clause arena layout (transliterated idea from kissat/src/clause.h and
// arena.h, MIT): all large clauses (size >= 3) live contiguously in one
// uint32_t arena addressed by 32-bit word offsets. Binary clauses never
// materialize — they exist only inside watch lists. Header:
//   word0  size
//   word1  bit0 garbage | bit1 redundant | bits2.. glue
//   word2  searched (index of last replacement-scan position, >= 2)
//   word3+ literals
#pragma once

#include "kernel/types.hpp"

#include <vector>

namespace tactix {

struct Clause {
  uint32_t size;
  uint32_t flags;
  uint32_t searched;
  Lit lits[1]; // actually `size` literals

  static constexpr uint32_t GARBAGE = 1u << 0;
  static constexpr uint32_t REDUNDANT = 1u << 1;
  static constexpr uint32_t REASON = 1u << 2; // transient lock during Reduce
  static constexpr uint32_t GLUE_SHIFT = 3;

  bool garbage() const { return flags & GARBAGE; }
  bool redundant() const { return flags & REDUNDANT; }
  bool reason() const { return flags & REASON; }
  uint32_t glue() const { return flags >> GLUE_SHIFT; }
  Lit *begin() { return lits; }
  Lit *end() { return lits + size; }
  const Lit *begin() const { return lits; }
  const Lit *end() const { return lits + size; }
};

constexpr uint32_t CLAUSE_HEADER_WORDS = 3;

inline uint32_t clause_words(uint32_t size) {
  return CLAUSE_HEADER_WORDS + size;
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
