#pragma once
#include <cstdint>
#include <limits>

namespace tactix::direct_csp_guarded_cover::detail {
// Saturated costs cannot wrap into a small successful allowance. All compound
// costs occur after a positive structural charge, so saturation also exceeds
// the remaining allowance when the declared allowance itself is UINT64_MAX.
inline uint64_t add(uint64_t a, uint64_t b) {
  constexpr auto maximum = std::numeric_limits<uint64_t>::max();
  return b > maximum - a ? maximum : a + b;
}
inline uint64_t multiply(uint64_t a, uint64_t b) {
  constexpr auto maximum = std::numeric_limits<uint64_t>::max();
  return a && b > maximum / a ? maximum : a * b;
}
} // namespace tactix::direct_csp_guarded_cover::detail
