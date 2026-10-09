#pragma once

#include "kernel/kernel.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace tactix::graph_labeling {

// A heuristic projection, never a logical certificate. The indexed rectangle
// follows the reference encoding: external identity row*N+column selects the
// vertex at that label. Signs are recovered from the column-cover clauses.
// Arbitrary renamings without this rectangle are conservatively unsupported.
struct Shape {
  uint32_t vertices = 0;
  uint32_t separation = 0;
  std::vector<Lit> primary;
  std::vector<std::pair<uint32_t, uint32_t>> edges;
  std::vector<std::vector<uint32_t>> adjacency;
};

// Charge includes clause/watch inspection, mapping/element initialization and
// graph materialization. maximum_bytes bounds logical workspace payload,
// excluding allocator overhead and the caller's already-owned Kernel/output.
// Failure publishes no partial shape and never changes Kernel or proof state.
template<class Charge>
bool recognize(const Kernel &k, uint32_t maximum_vertices,
               uint64_t maximum_bytes, Charge &&charge, Shape &out) {
  out = {};
  Shape shape;
  uint32_t n = 0;
  const uint64_t external_variables = k.external_to_internal.size();
  // A cheap first pass selects a possible dimension from an indexed column.
  // Full coverage and distinct rows are checked before the result is exposed.
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!charge(1)) return false;
    const uint32_t width = clause->size;
    if (!clause->garbage() && !clause->redundant() && width >= 3 &&
        width > n && width <= maximum_vertices &&
        uint64_t(width) * width <= external_variables) {
      uint32_t column = UINT32_MAX;
      bool plausible = true;
      for (Lit lit : *clause) {
        if (!charge(1)) return false;
        const Var external = k.internal_to_external[var_of(lit)];
        if (uint64_t(external) >= uint64_t(width) * width ||
            (column != UINT32_MAX && external % width != column)) {
          plausible = false;
          break;
        }
        column = external % width;
      }
      if (plausible) n = width;
    }
    ref += clause_actual_words(clause);
  }
  if (!n) return false;
  const uint64_t square = uint64_t(n) * n;
  // Primary literals, edge bitmap, fully reserved edge/adjacency vectors,
  // row/column marks, and vector descriptors fit this conservative bound.
  if (square > (UINT64_MAX - uint64_t(n) * 64) / 16 ||
      square * 16 + uint64_t(n) * 64 > maximum_bytes ||
      square > SIZE_MAX / sizeof(Lit) || !charge(square + 3 * uint64_t(n)))
    return false;
  shape.primary.assign(static_cast<size_t>(square), INVALID_LIT);
  std::vector<uint8_t> columns(n, 0), rows(n, 0);
  std::vector<Lit> recovered(n);
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!charge(1)) return false;
    if (!clause->garbage() && !clause->redundant() &&
        clause->size >= 3 && clause->size <= n) {
      if (!charge(n)) return false;
      std::fill(rows.begin(), rows.end(), 0);
      uint32_t column = UINT32_MAX;
      bool indexed = true;
      for (Lit lit : *clause) {
        if (!charge(1)) return false;
        const Var external = k.internal_to_external[var_of(lit)];
        if (external >= square || rows[external / n] ||
            (column != UINT32_MAX && external % n != column)) {
          indexed = false;
          break;
        }
        rows[external / n] = 1;
        recovered[external / n] = lit;
        column = external % n;
      }
      if (indexed) {
        // Parsing may remove root-false entries before storing a column.
        // Reconstruct only such entries; an unassigned missing row is not
        // evidence of a cover. Fixed entries cannot be selected by a validated
        // completion, regardless of their historical sign convention.
        for (uint32_t row = 0; row < n; ++row) {
          if (!charge(1)) return false;
          if (rows[row]) continue;
          const Var internal = k.external_to_internal[uint64_t(row) * n + column];
          if (internal == INVALID_VAR || internal >= k.nvars ||
              !k.value(make_lit(internal, false))) {
            indexed = false;
            break;
          }
          recovered[row] = make_lit(internal, k.value(make_lit(internal, false)) > 0);
        }
      }
      if (indexed) {
        for (Lit lit : recovered) {
          if (!charge(1)) return false;
          const Var external = k.internal_to_external[var_of(lit)];
          Lit &entry = shape.primary[external];
          if (kernel_var_retired(k, var_of(lit)) ||
              (entry != INVALID_LIT && entry != lit))
            return false;
          entry = lit;
        }
        columns[column] = 1;
      }
    }
    ref += clause_actual_words(clause);
  }
  for (uint8_t present : columns) {
    if (!charge(1) || !present) return false;
  }
  if (!charge(square)) return false;
  std::vector<uint8_t> connected(static_cast<size_t>(square), 0);
  uint32_t separation = n + 1;
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit) {
    if (!charge(1)) return false;
    const Var first = k.internal_to_external[var_of(lit)];
    if (first >= square || lit != negate(shape.primary[first])) continue;
    for (WatchEntry watch : k.watches.entries(lit)) {
      if (!charge(1)) return false;
      if (!watch.binary() || lit >= watch.other()) continue;
      const Var second = k.internal_to_external[var_of(watch.other())];
      if (second >= square ||
          watch.other() != negate(shape.primary[second]) ||
          first % n != second % n || first / n == second / n)
        continue;
      const uint32_t a = std::min(first / n, second / n);
      const uint32_t b = std::max(first / n, second / n);
      connected[uint64_t(a) * n + b] = 1;
      separation = std::min(separation, first % n + 1);
    }
  }
  if (separation > n || !charge(square + n)) return false;
  shape.edges.reserve(static_cast<size_t>(square - n) / 2);
  shape.adjacency.resize(n);
  for (auto &row : shape.adjacency) row.reserve(n - 1);
  for (uint32_t a = 0; a < n; ++a) {
    for (uint32_t b = a + 1; b < n; ++b) {
      if (!charge(1)) return false;
      if (!connected[uint64_t(a) * n + b]) continue;
      shape.edges.emplace_back(a, b);
      shape.adjacency[a].push_back(b);
      shape.adjacency[b].push_back(a);
    }
  }
  shape.vertices = n;
  shape.separation = separation;
  out = std::move(shape);
  return true;
}

} // namespace tactix::graph_labeling
