#pragma once
// Three exactly-one domains and their three forbidden diagonal tuples.
#include "dimacs_text.hpp"

#include <cstdint>
#include <string>
#include <vector>

// A nonzero first variable appends an isolated contradictory two-variable CNF.
inline std::vector<std::vector<int32_t>>
nary_equality_clauses(int32_t contradiction_first = 0) {
  std::vector<std::vector<int32_t>> result{
      {1, 2, 3}, {-1, -2}, {-1, -3}, {-2, -3},
      {4, 5, 6}, {-4, -5}, {-4, -6}, {-5, -6},
      {7, 8, 9}, {-7, -8}, {-7, -9}, {-8, -9},
      {-1, -4, -7}, {-2, -5, -8}, {-3, -6, -9}};
  if (contradiction_first) {
    const int32_t a = contradiction_first, b = a + 1;
    result.insert(result.end(), {{a, b}, {a, -b}, {-a, b}, {-a, -b}});
  }
  return result;
}

inline std::string nary_equality_dimacs(int32_t contradiction_first = 0) {
  return dimacs_text(contradiction_first ? contradiction_first + 1 : 9,
                     nary_equality_clauses(contradiction_first));
}
