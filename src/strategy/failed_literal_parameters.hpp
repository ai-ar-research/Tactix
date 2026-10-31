#pragma once

#include "strategy/scoped_rule_parameters.hpp"

#include <cstdint>

namespace tactix {

struct Kernel;

namespace strategy {

struct BackboneParameters {
  static constexpr uint32_t INITIAL_ROUNDS_DEFAULT = 100;
  static constexpr uint32_t MAXIMUM_ROUNDS_DEFAULT = 1000;

  uint32_t initial_rounds = INITIAL_ROUNDS_DEFAULT;
  uint32_t maximum_rounds = MAXIMUM_ROUNDS_DEFAULT;

  static BackboneParameters from_kernel(const Kernel &);
  constexpr bool valid() const { return true; }
};

void install_rule_parameters(Kernel &, const BackboneParameters &);

using ScopedBackboneParameters = ScopedRuleParameters<BackboneParameters>;

} // namespace strategy
} // namespace tactix
