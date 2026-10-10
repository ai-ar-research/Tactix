#pragma once

#include <cstdint>

namespace tactix {

struct Kernel;

namespace strategy {

// Strategy-owned controls for one Vivify invocation.  They select the
// candidate band, reuse/flush policy, and the reference population threshold;
// none is a soundness precondition on an explicit clause action.
struct VivifyParameters {
  static constexpr uint32_t IRREDUNDANT_SORT_RATIO_DEFAULT = 10;

  uint8_t scope = 0;
  bool reuse = true;
  bool flush = false;
  uint32_t irredundant_sort_ratio = IRREDUNDANT_SORT_RATIO_DEFAULT;
  // One band of a cumulative dose ledger (vivify.c: each tier's share joins
  // one probing-tick ceiling). With `band`, the application is bounded by
  // `band_budget` measured from a dose start `band_offset` ticks earlier, so
  // unused work of an earlier band carries into later ones and an overrun is
  // precharged. Per application only: installs and from_kernel leave it off.
  bool band = false;
  uint64_t band_budget = 0;
  uint64_t band_offset = 0;

  constexpr bool valid() const {
    return scope <= 5 && irredundant_sort_ratio != 0 &&
           (band || (!band_budget && !band_offset));
  }

  static VivifyParameters from_kernel(const Kernel &);
};

inline bool vivify_skip_global_order(uint64_t irredundant,
                                     uint64_t redundant,
                                     const VivifyParameters &parameters) {
  return irredundant / parameters.irredundant_sort_ratio > redundant;
}

void install_rule_parameters(Kernel &, const VivifyParameters &);

class ScopedVivifyParameters {
public:
  ScopedVivifyParameters(Kernel &, const VivifyParameters &);
  ScopedVivifyParameters(const ScopedVivifyParameters &) = delete;
  ScopedVivifyParameters &operator=(const ScopedVivifyParameters &) = delete;
  ~ScopedVivifyParameters();

private:
  Kernel &kernel_;
  VivifyParameters saved_;
};

} // namespace strategy
} // namespace tactix
