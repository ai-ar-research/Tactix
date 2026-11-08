#pragma once
#include <cstdint>

namespace tactix {
// Helpers inserted after a logical variable extent. Borrowed by checker-event
// consumers as well as the DSR-to-PBP converter.
struct DsrHelperNames {
  uint32_t extent = 0;
  uint32_t helpers = 0;
};
} // namespace tactix
