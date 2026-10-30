#pragma once

#include "kernel/types.hpp"
#include "strategy/rule_parameters.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

struct Kernel;

struct ConfigurationCheckingModelResult {
  bool sat = false;
  bool interrupted = false;
  uint64_t steps = 0;
  uint64_t flips = 0;
  std::vector<uint8_t> assignment;
};

// Search an authenticated semantic projection without mutating F or phase
// memories.  The caller owns the projection proof and must validate a returned
// model against the complete live formula before publication.
ConfigurationCheckingModelResult configuration_checking_model(
    Kernel &, const std::vector<std::vector<Lit>> &clauses,
    const strategy::WalkParameters &);

} // namespace tactix
