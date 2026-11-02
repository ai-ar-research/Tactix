#pragma once
// The kernel state a declined theory action must leave untouched: sizes,
// identity maps, arena generation and per-literal watch counts.
#include "kernel/kernel.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

struct Image {
  uint32_t nvars = 0;
  uint64_t next_id = 0;
  size_t arena = 0;
  size_t extent = 0;
  size_t trail = 0;
  uint64_t generation = 0;
  std::vector<tactix::Var> internal_to_external;
  std::vector<tactix::Var> external_to_internal;
  std::vector<size_t> watches;
  bool operator==(const Image &) const = default;
};

inline Image image(const tactix::Kernel &kernel) {
  Image result;
  result.nvars = kernel.nvars;
  result.next_id = kernel.next_clause_id;
  result.arena = kernel.arena.size();
  result.extent = kernel.external_to_internal.size();
  result.trail = kernel.trail.size();
  result.generation = kernel.arena_generation;
  result.internal_to_external = kernel.internal_to_external;
  result.external_to_internal = kernel.external_to_internal;
  for (tactix::Lit literal = 0; literal < 2 * kernel.nvars; ++literal)
    result.watches.push_back(kernel.watches.size(literal));
  return result;
}
