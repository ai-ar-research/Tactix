#pragma once

#include <cstdint>

namespace tactix {
struct Kernel;

namespace strategy {

struct AnalyzeParameters {
  // Kissat's shipped minimizedepth. Zero deliberately disables recursive
  // reason minimization while preserving first-UIP learning soundness.
  static constexpr uint32_t MINIMIZE_DEPTH_DEFAULT = 1000;
  // The eager register has four representation slots.  Values through that
  // capacity are strategy choices; a larger value cannot be represented.
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

class ScopedAnalyzeParameters {
public:
  ScopedAnalyzeParameters(Kernel &, const AnalyzeParameters &);
  ScopedAnalyzeParameters(const ScopedAnalyzeParameters &) = delete;
  ScopedAnalyzeParameters &operator=(const ScopedAnalyzeParameters &) = delete;
  ~ScopedAnalyzeParameters();

private:
  Kernel &kernel_;
  AnalyzeParameters saved_;
};

} // namespace strategy
} // namespace tactix
