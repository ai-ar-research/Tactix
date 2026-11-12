#pragma once

#include "strategy/scoped_rule_parameters.hpp"

#include <cstdint>

namespace tactix {
struct Kernel;

namespace strategy {

struct AnalyzeParameters {
  // Kissat minimizedepth; zero disables recursive minimization.
  static constexpr uint32_t MINIMIZE_DEPTH_DEFAULT = 1000;
  // The eager register has four slots; larger values are unrepresentable.
  static constexpr uint32_t EAGER_SUBSUME_CAPACITY = 4;
  uint32_t minimize_depth_limit = MINIMIZE_DEPTH_DEFAULT;
  bool conflict_reuse = true;
  uint32_t chronological_levels = 100;
  uint32_t bump_reason_limit = 0;
  uint32_t bump_reason_depth = 1;
  uint32_t eager_subsume = EAGER_SUBSUME_CAPACITY;
  bool jump_reasons = true;
  bool jump_reasons_bigbig = false;
  bool shrink_glue_promote = true;
  bool bump_extension_reasons = true;

  constexpr bool valid() const {
    return eager_subsume <= EAGER_SUBSUME_CAPACITY;
  }
  static AnalyzeParameters from_kernel(const Kernel &);
};

void install_rule_parameters(Kernel &, const AnalyzeParameters &);

using ScopedAnalyzeParameters = ScopedRuleParameters<AnalyzeParameters>;

} // namespace strategy
} // namespace tactix
