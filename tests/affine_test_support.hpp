#pragma once
// Affine-consequence parameters and clause input shared by the affine tests.
#include "kernel/kernel.hpp"
#include "affine_profile_fixture.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

// The current internal literals of external clause literals.
inline std::vector<tactix::Lit> physical(tactix::Kernel &k,
                                         const std::vector<int32_t> &clause) {
  std::vector<tactix::Lit> result;
  for (int32_t x : clause) {
    const tactix::Lit l = tactix::kernel_internalize(k, x);
    assert(l != tactix::INVALID_LIT);
    result.push_back(l);
  }
  return result;
}

// Add an irredundant binary or large input clause with the next clause id.
inline void add(tactix::Kernel &k, const std::vector<int32_t> &clause) {
  const auto body = physical(k, clause);
  const uint64_t id = ++k.next_clause_id;
  if (body.size() == 2)
    tactix::kernel_add_binary_clause(k, body[0], body[1], id);
  else
    tactix::kernel_add_large_clause(k, body.data(), body.size(), false, 0, id);
}
