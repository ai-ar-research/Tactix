#pragma once

#include "kernel/types.hpp"
#include <span>

namespace tactix {
struct Kernel;

// Query-local membership, indexed by internal variable. Root facts outside
// the region remain usable. This primitive never certifies a full-F fixpoint.
struct PropagationRegion {
  std::span<const uint32_t> stamps;
  uint32_t epoch;
};
bool kernel_propagate_region(Kernel &, const PropagationRegion &);
} // namespace tactix
