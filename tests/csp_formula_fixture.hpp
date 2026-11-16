#pragma once
// Ordered CNFs shared by direct-support and nary-equality tests.
#include "dimacs_text.hpp"

#include <cstdint>
#include <string>
#include <vector>

// Three exactly-one domains and their forbidden diagonal tuples.
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

// Bipartite support; unit_row makes the first left value unsupported.
inline std::vector<std::vector<int32_t>>
csp_support_clauses(bool unit_row = false) {
  std::vector<std::vector<int32_t>> result{
      {1, 2, 3}, {-1, -2}, {-1, -3}, {-2, -3},
      {4, 5, 6, 7}, {-4, -5}, {-4, -6}, {-4, -7}, {-5, -6}, {-5, -7},
      {-6, -7}};
  if (unit_row)
    result.insert(result.end(), {{-1, -4}, {-1, -5}, {-1, -6}});
  result.insert(result.end(),
                {{-1, -7}, {-2, -4}, {-2, -6}, {-2, -7}, {-3, -5}});
  return result;
}

inline std::string csp_support_dimacs(bool unit_row = false,
                                      bool checker_suffix = false) {
  auto formula = csp_support_clauses(unit_row);
  // This suffix has a different clause order from the nary-equality fixture.
  if (checker_suffix)
    formula.insert(formula.end(), {{20, 21}, {-20, 21}, {20, -21}, {-20, -21}});
  return dimacs_text(checker_suffix ? 21 : 7, formula);
}
