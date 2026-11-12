#pragma once
// Cardinality fixtures shared by the cardinality-search tests.
#include "rules/semantic_extension/cardinality_search.hpp"

#include <cstdint>
#include <utility>
#include <vector>

// lower <= sum(literals) <= upper with its complete clausal encoding: every
// (size - lower + 1)-subset as a positive clause and every (upper + 1)-subset
// as a negative clause, in subset-mask order.
inline tactix::cardinality_search::Constraint cardinality_constraint(
    std::vector<int32_t> literals, uint32_t lower, uint32_t upper) {
  tactix::cardinality_search::Constraint result;
  result.literals = std::move(literals);
  result.lower = lower;
  result.upper = upper;
  const uint32_t size = result.literals.size();
  for (uint32_t mask = 1; mask < (1u << size); ++mask) {
    const uint32_t width = __builtin_popcount(mask);
    const bool positive = lower && width == size - lower + 1;
    const bool negative = upper < size && width == upper + 1;
    if (!positive && !negative) continue;
    tactix::cardinality_search::Clause clause;
    for (uint32_t at = 0; at < size; ++at)
      if (mask & (1u << at)) clause.push_back(result.literals[at]);
    if (positive) result.premises.push_back(clause);
    if (negative) {
      for (int32_t &literal : clause) literal = -literal;
      result.premises.push_back(clause);
    }
  }
  return result;
}

// Two overlapping bounds that imply 5 -> 1, and three exact-one pairs over an
// odd cycle, which conflict.
inline std::vector<tactix::cardinality_search::Constraint> cardinality_overlap() {
  return {cardinality_constraint({1, 2, 3, 4}, 2, 4),
          cardinality_constraint({2, 3, 4, 5}, 0, 2)};
}
inline std::vector<tactix::cardinality_search::Constraint> cardinality_odd() {
  return {cardinality_constraint({1, 2}, 1, 1), cardinality_constraint({2, 3}, 1, 1),
          cardinality_constraint({1, 3}, 1, 1)};
}
