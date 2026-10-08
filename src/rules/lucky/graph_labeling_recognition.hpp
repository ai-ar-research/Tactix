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

// Every usable clause holds only primary variables (externals below n*n, with
// n <= maximum_vertices and n*n <= variables), so only that region's watch
// lists are read; each large clause once, from the watch of lits[0]. Cost is
// independent of the rest of the formula; derivation and measurements:
// benchmarks/reports/2026-10-07-graph-labeling-recognition-budget/README.md.
// Charge covers clause/watch inspection, mapping/element initialization and
// graph materialization. maximum_bytes bounds logical workspace payload,
// excluding allocator overhead and the caller's already-owned Kernel/output.
// Failure publishes no partial shape and never changes Kernel or proof state.
template<class Charge, class Visit>
bool visit_region_clauses(const Kernel &k, uint64_t region, Charge &charge,
                          Visit &&visit) {
  region = std::min<uint64_t>(region, k.external_to_internal.size());
  for (uint64_t external = 0; external < region; ++external) {
    if (!charge(1)) return false;
    const Var internal = k.external_to_internal[external];
    if (internal == INVALID_VAR || internal >= k.nvars) continue;
    for (const Lit lit : {make_lit(internal, false), make_lit(internal, true)}) {
      for (WatchEntry watch : k.watches.entries(lit)) {
        if (!charge(1)) return false;
        if (watch.binary()) continue;
        const Clause *clause = clause_at(k.arena, watch.ref());
        if (clause->lits[0] != lit || clause->garbage() || clause->redundant())
          continue;
        if (!visit(*clause)) return false;
      }
    }
  }
  return true;
}

template<class Charge>
bool recognize(const Kernel &k, uint32_t maximum_vertices,
               uint64_t maximum_bytes, Charge &&charge, Shape &out) {
  out = {};
  Shape shape;
  uint32_t n = 0;
  const uint64_t external_variables = k.external_to_internal.size();
  if (!k.large_watches_connected) return false;
  uint64_t side = std::min<uint64_t>(maximum_vertices, UINT32_MAX);
  while (side && side * side > external_variables) --side;
  // A cheap first pass selects a possible dimension from an indexed column.
  // Full coverage and distinct rows are checked before the result is exposed.
  if (side >= 3 &&
      !visit_region_clauses(k, side * side, charge, [&](const Clause &clause) {
        const uint32_t width = clause.size;
        if (width < 3 || width <= n || width > side) return true;
        uint32_t column = UINT32_MAX;
        for (Lit lit : clause) {
          if (!charge(1)) return false;
          const Var external = k.internal_to_external[var_of(lit)];
          if (uint64_t(external) >= uint64_t(width) * width ||
              (column != UINT32_MAX && external % width != column))
            return true;
          column = external % width;
        }
        n = width;
        return true;
      }))
    return false;
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
  std::vector<uint32_t> touched;
  touched.reserve(n);
  // Rows are restored from the touched list, so a clause costs its literals
  // (plus n only when it is an in-column candidate), never a full row reset.
  const auto index = [&](const Clause &clause) {
    if (clause.size < 3 || clause.size > n) return true;
    uint32_t column = UINT32_MAX;
    bool indexed = true;
    for (Lit lit : clause) {
      if (!charge(1)) return false;
      const Var external = k.internal_to_external[var_of(lit)];
      if (external >= square || rows[external / n] ||
          (column != UINT32_MAX && external % n != column)) {
        indexed = false;
        break;
      }
      rows[external / n] = 1;
      touched.push_back(external / n);
      recovered[external / n] = lit;
      column = external % n;
    }
    bool charged = true;
    if (indexed) {
      // Parsing may remove root-false entries before storing a column.
      // Reconstruct only such entries; an unassigned missing row is not
      // evidence of a cover. Fixed entries cannot be selected by a validated
      // completion, regardless of their historical sign convention.
      for (uint32_t row = 0; row < n; ++row) {
        if (!charge(1)) {
          charged = false;
          break;
        }
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
    for (uint32_t row : touched) rows[row] = 0;
    touched.clear();
    if (!charged) return false;
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
    return true;
  };
  if (!visit_region_clauses(k, square, charge, index)) return false;
  for (uint8_t present : columns) {
    if (!charge(1) || !present) return false;
  }
  if (!charge(square)) return false;
  std::vector<uint8_t> connected(static_cast<size_t>(square), 0);
  uint32_t separation = n + 1;
  for (uint64_t first = 0; first < square; ++first) {
    if (!charge(1)) return false;
    const Lit lit = negate(shape.primary[first]);
    for (WatchEntry watch : k.watches.entries(lit)) {
      if (!charge(1)) return false;
      if (!watch.binary() || lit >= watch.other()) continue;
      const Var second = k.internal_to_external[var_of(watch.other())];
      if (second >= square ||
          watch.other() != negate(shape.primary[second]) ||
          first % n != second % n || first / n == second / n)
        continue;
      const uint32_t a = uint32_t(std::min<uint64_t>(first / n, second / n));
      const uint32_t b = uint32_t(std::max<uint64_t>(first / n, second / n));
      connected[uint64_t(a) * n + b] = 1;
      separation = std::min(separation, uint32_t(first % n) + 1);
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
