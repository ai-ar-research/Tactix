#pragma once
// The 3x3 exact-cover fixture shared by the exact-cover tests: one at-most-one
// block per row of items 1..9, then the three column and three diagonal
// covering clauses. The block occupancies are the definable theory variables.
#include "dimacs_text.hpp"
#include "test_io.hpp"
#include "kernel/kernel.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

inline std::vector<std::vector<int32_t>> exact_cover_clauses(
    bool missing_edge = false) {
  std::vector<std::vector<int32_t>> result;
  for (int block = 0; block < 3; ++block)
    for (int first = 0; first < 3; ++first)
      for (int second = first + 1; second < 3; ++second)
        if (!missing_edge || block != 2 || first != 0 || second != 1)
          result.push_back({-(3 * block + first + 1), -(3 * block + second + 1)});
  for (int position = 0; position < 3; ++position) {
    std::vector<int32_t> clause;
    for (int block = 0; block < 3; ++block)
      clause.push_back(3 * block + position + 1);
    result.push_back(clause);
  }
  for (int diagonal = 0; diagonal < 3; ++diagonal) {
    std::vector<int32_t> clause;
    for (int block = 0; block < 3; ++block)
      clause.push_back(3 * block + (block + diagonal) % 3 + 1);
    result.push_back(clause);
  }
  return result;
}

// The fixture parsed, optionally with the extra clause 1 2 3.
inline tactix::Kernel exact_cover_kernel(bool extra = false) {
  auto clauses = exact_cover_clauses();
  if (extra) clauses.push_back({1, 2, 3});
  return parse_dimacs_text(dimacs_text(9, clauses));
}

// The kernel state a declined action must leave unchanged, and its journal.
struct ExactCoverUnchanged {
  const tactix::Kernel &kernel;
  FILE *records;
  std::vector<tactix::Var> mapping = kernel.external_to_internal;
  uint64_t next_id = kernel.next_clause_id;
  size_t arena = kernel.arena.size();
  uint32_t nvars = kernel.nvars;
  uint32_t outputs = tactix::kernel_num_output_vars(kernel);
  long journal = position(records);

  void check() const {
    assert(kernel.external_to_internal == mapping &&
           kernel.next_clause_id == next_id && kernel.arena.size() == arena &&
           kernel.nvars == nvars &&
           tactix::kernel_num_output_vars(kernel) == outputs &&
           position(records) == journal);
  }
};
