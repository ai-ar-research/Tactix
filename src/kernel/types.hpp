// Core value types (plan §5): kissat-style index-based representation
// (see kissat/src/literal.h for the encoding — MIT, Biere et al.).
#pragma once

#include <cassert>
#include <cstdint>

namespace tactix {

using Var = uint32_t; // 0-based internal variable index
using Lit = uint32_t; // 2*var + negated
using Ref = uint32_t; // word offset of a clause in the arena

constexpr Lit INVALID_LIT = UINT32_MAX;
constexpr Var INVALID_VAR = UINT32_MAX;
constexpr Ref INVALID_REF = UINT32_MAX;

// reason sentinels (kissat/src/assign.h)
constexpr uint32_t DECISION_REASON = UINT32_MAX;
constexpr uint32_t UNIT_REASON = UINT32_MAX - 1;

constexpr Lit make_lit(Var v, bool negated) { return (v << 1) | negated; }
constexpr Var var_of(Lit l) { return l >> 1; }
constexpr bool negated(Lit l) { return l & 1; }
constexpr Lit negate(Lit l) { return l ^ 1; }

// external DIMACS literal (nonzero int) <-> internal Lit
constexpr Lit internalize(int elit) {
  assert(elit != 0);
  const Var v = (elit > 0 ? elit : -elit) - 1;
  return make_lit(v, elit < 0);
}
constexpr int externalize(Lit l) {
  const int e = static_cast<int>(var_of(l)) + 1;
  return negated(l) ? -e : e;
}

} // namespace tactix
