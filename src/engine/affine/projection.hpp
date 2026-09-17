#pragma once
#include "engine/affine/syndrome_search.hpp"
#include <numeric>

namespace tactix::strategy::affine {
struct Equation { std::vector<size_t> variables; bool rhs; };
struct Coordinate { size_t variable; bool complemented; };
enum class ProjectionStatus { Ready, Inconsistent, Budget, Interrupted, Invalid };
struct Projection {
  size_t variables = 0, nonprojected = 0;
  std::vector<size_t> order;
  std::vector<Coordinate> coordinates;
  std::vector<Bits> basis, columns;
  Bits target;
  uint64_t work = 0;
};
inline bool bit(const Bits &v, size_t i) { return (v[i / 64] >> (i % 64)) & 1; }
inline void flip(Bits &v, size_t i) { v[i / 64] ^= uint64_t{1} << (i % 64); }

// The projection is the exact image of the input affine system. Budget failure
// yields no image; the caller must not interpret it as inconsistency or UNSAT.
// Coordinates are signed, distinct variable identities, in caller order.
template<class Meter>
ProjectionStatus project(size_t variables, const std::vector<Equation> &equations,
                         const std::vector<Coordinate> &coordinates,
                         uint64_t maximum_bytes, Meter &meter, Projection &out) {
  out = {};
  if (variables > SIZE_MAX - 64 || coordinates.size() > variables)
    return ProjectionStatus::Invalid;
  ProjectionStatus status = ProjectionStatus::Ready;
  const auto charge = [&](uint64_t amount) {
    if (!meter.charge(amount)) {
      status = meter.interrupted() ? ProjectionStatus::Interrupted : ProjectionStatus::Budget;
      return false;
    }
    out.work += amount;
    return true;
  };
  if (!charge(1)) return status;
  const size_t words = (variables + 64) / 64;
  // A conservative bound for simultaneously owned coefficient data, index
  // vectors, and row descriptors. Input equations and allocator metadata are
  // outside this declared workspace-payload budget.
  const unsigned __int128 bytes =
      static_cast<unsigned __int128>(variables) *
          (2 * static_cast<unsigned __int128>(words) * sizeof(uint64_t) +
           2 * sizeof(Bits) + 3 * sizeof(size_t) + sizeof(Coordinate)) +
      static_cast<unsigned __int128>(2 * words) * sizeof(uint64_t);
  if (bytes > maximum_bytes || bytes > SIZE_MAX)
    return ProjectionStatus::Budget;
  if (!charge(variables)) return status;
  std::vector<size_t> index(variables, SIZE_MAX);
  for (const Coordinate c : coordinates) {
    if (!charge(1)) return status;
    if (c.variable >= variables || index[c.variable] != SIZE_MAX)
      return ProjectionStatus::Invalid;
    index[c.variable] = 0;
  }
  out.order.reserve(variables);
  for (size_t v = 0; v < variables; ++v) {
    if (!charge(1)) return status;
    if (index[v] == SIZE_MAX) out.order.push_back(v);
  }
  out.nonprojected = out.order.size();
  for (const Coordinate c : coordinates) out.order.push_back(c.variable);
  for (size_t i = 0; i < variables; ++i) index[out.order[i]] = i;
  out.variables = variables;
  out.coordinates = coordinates;
  out.basis.resize(variables);
  for (const Equation &equation : equations) {
    if (!charge(words)) return status;
    Bits row(words, 0);
    for (size_t v : equation.variables) {
      if (!charge(1)) return status;
      if (v >= variables) return ProjectionStatus::Invalid;
      flip(row, index[v]); // repeated terms cancel over GF(2)
    }
    if (equation.rhs) flip(row, variables);
    bool inserted = false;
    for (size_t p = 0; p < variables; ++p) {
      if (!charge(1)) return status;
      if (!bit(row, p)) continue;
      if (out.basis[p].empty()) {
        out.basis[p] = std::move(row);
        inserted = true;
        break;
      }
      if (!charge(words)) return status;
      for (size_t w = 0; w < words; ++w) row[w] ^= out.basis[p][w];
    }
    if (!inserted && bit(row, variables)) return ProjectionStatus::Inconsistent;
  }
  size_t checks = 0;
  for (size_t p = out.nonprojected; p < variables; ++p)
    checks += !out.basis[p].empty();
  // A zero system uses one zero word; the search then admits every support.
  const size_t check_words = std::max(size_t{1}, (checks + 63) / 64);
  if (!charge(coordinates.size() * check_words + check_words)) return status;
  out.columns.assign(coordinates.size(), Bits(check_words, 0));
  out.target.assign(check_words, 0);
  size_t check = 0;
  for (size_t p = out.nonprojected; p < variables; ++p) {
    if (out.basis[p].empty()) continue;
    bool rhs = bit(out.basis[p], variables);
    for (size_t i = 0; i < coordinates.size(); ++i) {
      if (!charge(1)) return status;
      if (!bit(out.basis[p], out.nonprojected + i)) continue;
      flip(out.columns[i], check);
      rhs ^= coordinates[i].complemented;
    }
    if (rhs) flip(out.target, check);
    ++check;
  }
  return ProjectionStatus::Ready;
}

// Reconstruct in original variable order. Free nonprojected coordinates retain
// the caller's assignment; no hidden variable-order or phase search is done.
// This pure helper changes no solver state, including on interruption/failure.
template<class Meter>
bool reconstruct(const Projection &p, const Bits &selected,
                 std::vector<uint8_t> &assignment, Meter &meter, uint64_t &work) {
  work = 0;
  if (assignment.size() != p.variables ||
      selected.size() != (p.coordinates.size() + 63) / 64) return false;
  const auto charge = [&](uint64_t amount) {
    if (!meter.charge(amount)) return false;
    work += amount;
    return true;
  };
  for (size_t i = 0; i < p.coordinates.size(); ++i) {
    if (!charge(1)) return false;
    assignment[p.coordinates[i].variable] = bit(selected, i) ^ p.coordinates[i].complemented;
  }
  for (size_t pivot = p.variables; pivot-- > 0;) {
    if (!charge(1)) return false;
    if (p.basis[pivot].empty()) continue;
    bool rhs = bit(p.basis[pivot], p.variables);
    for (size_t j = pivot + 1; j < p.variables; ++j) {
      if (!charge(1)) return false;
      if (bit(p.basis[pivot], j) && assignment[p.order[j]]) rhs = !rhs;
    }
    if (pivot >= p.nonprojected) {
      if (assignment[p.order[pivot]] != rhs) return false;
    } else assignment[p.order[pivot]] = rhs;
  }
  return true;
}
} // namespace tactix::strategy::affine
