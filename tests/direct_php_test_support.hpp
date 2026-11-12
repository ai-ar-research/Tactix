#pragma once
// Pigeonhole fixtures and ample limits shared by the direct-PHP tests.
#include "kernel/kernel.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

// PHP(holes + 1, holes): variable p*holes + h + 1 places pigeon p in hole h;
// one at-least-one clause per pigeon, then the at-most-one binaries per hole.
inline std::vector<std::vector<int32_t>> php_clauses(uint32_t holes) {
  const uint32_t pigeons = holes + 1;
  const auto variable = [holes](uint32_t pigeon, uint32_t hole) {
    return int32_t(pigeon * holes + hole + 1);
  };
  std::vector<std::vector<int32_t>> result;
  for (uint32_t pigeon = 0; pigeon < pigeons; ++pigeon) {
    std::vector<int32_t> alo;
    for (uint32_t hole = 0; hole < holes; ++hole)
      alo.push_back(variable(pigeon, hole));
    result.push_back(alo);
  }
  for (uint32_t hole = 0; hole < holes; ++hole)
    for (uint32_t first = 0; first < pigeons; ++first)
      for (uint32_t second = first + 1; second < pigeons; ++second)
        result.push_back({-variable(first, hole), -variable(second, hole)});
  return result;
}

// The PHP clauses as native clauses with IDs 1..n.
inline tactix::Kernel php_kernel(uint32_t holes = 3) {
  assert(holes >= 2);
  tactix::Kernel kernel;
  tactix::kernel_reserve_vars(kernel, (holes + 1) * holes);
  for (const auto &external : php_clauses(holes)) {
    std::vector<tactix::Lit> clause;
    for (int32_t literal : external) clause.push_back(tactix::internalize(literal));
    const uint64_t id = ++kernel.next_clause_id;
    if (clause.size() == 2)
      tactix::kernel_add_binary_clause(kernel, clause[0], clause[1], id);
    else
      tactix::kernel_add_large_clause(kernel, clause.data(), clause.size(),
                                      false, 0, id);
  }
  return kernel;
}

// Ample snapshot, recognition and certificate bounds.
template <class Limits> void ample_php_limits(Limits &limits) {
  limits.snapshot = {100000, 1000000, 10000000};
  limits.recognition = {100000, 1000000, 100000000, 10000, 1000000, 1000000, 20};
  limits.certificate = {1000000, 1000000, 100000000, 1000000, 2000000};
}
