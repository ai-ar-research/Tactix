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
// Native algebraic trail reason.  Unlike a Ref, this payload is owned by the
// attached affine session and must be materialized before a clausal consumer
// follows it.
constexpr uint32_t AFFINE_REASON = UINT32_MAX - 2;

constexpr Lit make_lit(Var v, bool negated) { return (v << 1) | negated; }
constexpr Var var_of(Lit l) { return l >> 1; }
constexpr bool negated(Lit l) { return l & 1; }
constexpr Lit negate(Lit l) { return l ^ 1; }

// Absolute variable identifier of a signed external literal.  The int64_t
// promotion keeps INT32_MIN well-defined; callers that reject zero or values
// outside the DIMACS domain do so at their own interface boundary.
constexpr uint32_t external_variable_of(int32_t literal) {
  return static_cast<uint32_t>(literal < 0 ? -int64_t(literal) : literal);
}

// Canonical ordering for signed external literals: variable first, then sign
// (negative before positive when both spell the same variable). Validation of
// zero and INT32_MIN remains the caller's interface responsibility.
constexpr bool external_literal_less(int32_t first, int32_t second) {
  const uint32_t first_variable = external_variable_of(first);
  const uint32_t second_variable = external_variable_of(second);
  return first_variable < second_variable ||
         (first_variable == second_variable && first < second);
}

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
