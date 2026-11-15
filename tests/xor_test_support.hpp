#pragma once
#include "kernel/kernel.hpp"

#include <bit>
#include <cassert>
#include <initializer_list>

// Independent test encoding: forbid assignments whose parity differs from rhs.
inline void add_xor_equation(tactix::Kernel &k,
                             std::initializer_list<tactix::Var> variables,
                             bool rhs) {
  assert(variables.size() >= 2 && variables.size() <= 4);
  for (unsigned mask = 0; mask != (1u << variables.size()); ++mask) {
    if ((std::popcount(mask) & 1) == rhs)
      continue;
    tactix::Lit clause[4];
    for (size_t i = 0; i < variables.size(); ++i)
      clause[i] = tactix::make_lit(variables.begin()[i], mask & (1u << i));
    if (variables.size() == 2)
      tactix::kernel_add_binary_clause(k, clause[0], clause[1]);
    else
      tactix::kernel_add_large_clause(k, clause, variables.size(), false, 0);
  }
}
