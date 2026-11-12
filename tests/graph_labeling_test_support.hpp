#pragma once
// The 4x4 permutation fixture of the graph-labeling tests.
#include "kernel/kernel.hpp"

#include <cstdint>
#include <vector>

// Column covers and row at-most-one over 16 variables, plus redundant
// same-column exclusions; `difficult` makes the projected objective too strong.
inline void permutation(tactix::Kernel &k, bool difficult) {
  constexpr uint32_t n = 4;
  tactix::kernel_reserve_vars(k, n * n);
  for (uint32_t column = 0; column < n; ++column) {
    std::vector<tactix::Lit> cover;
    for (uint32_t row = 0; row < n; ++row)
      cover.push_back(tactix::make_lit(row * n + column, false));
    tactix::kernel_add_large_clause(k, cover.data(), cover.size(), false, 0);
  }
  for (uint32_t row = 0; row < n; ++row)
    for (uint32_t a = 0; a < n; ++a)
      for (uint32_t b = a + 1; b < n; ++b)
        tactix::kernel_add_binary_clause(k, tactix::make_lit(row * n + a, true),
                                         tactix::make_lit(row * n + b, true));
  // Redundant same-column exclusions; the difficult objective must remain
  // guidance rather than UNSAT.
  for (uint32_t a = 0; a < n; ++a)
    for (uint32_t b = a + 1; b < n; ++b)
      tactix::kernel_add_binary_clause(
          k, tactix::make_lit(a * n + (difficult ? 2 : 0), true),
          tactix::make_lit(b * n + (difficult ? 2 : 0), true));
  k.graph_labeling_parameters.work.fill(100000);
  k.graph_labeling_parameters.workspace_bytes = 1 << 20;
  k.graph_labeling_parameters.repair_rounds = 3;
  k.graph_labeling_parameters.anneal_iterations = 20;
  k.graph_labeling_parameters.stall_anneal_iterations = 3;
}
