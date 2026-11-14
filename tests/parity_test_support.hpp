#pragma once
// XOR fixtures shared by the parity tests.
#include "kernel/kernel.hpp"
#include "rules/lucky/parity_certificate.hpp"

#include <vector>

// x1+x3+x4 = x4+x5+x6 = x6+x7+x3 = x2+x5+x7 = 0 over variables 1..7; the
// four rows sum to x1 + x2 = 0.
inline std::vector<tactix::parity_certificate::Equation> parity_cycle() {
  return {{{1, 3, 4, 0}, 3, false}, {{4, 5, 6, 0}, 3, false},
          {{6, 7, 3, 0}, 3, false}, {{2, 5, 7, 0}, 3, false}};
}

// The complete clausal encoding of each equation, with consecutive native IDs.
inline void add_xor_clauses(
    tactix::Kernel &k,
    const std::vector<tactix::parity_certificate::Equation> &equations) {
  for (const auto &e : equations)
    for (const auto &c : tactix::parity_certificate::xor_equation(
             e.variables.data(), e.size, e.rhs)) {
      tactix::Lit lits[16];
      for (unsigned i = 0; i < c.size; ++i) lits[i] = tactix::internalize(c.lits[i]);
      if (c.size == 2)
        tactix::kernel_add_binary_clause(k, lits[0], lits[1], ++k.next_clause_id);
      else
        tactix::kernel_add_large_clause(k, lits, c.size, false, 0,
                                        ++k.next_clause_id);
    }
}
