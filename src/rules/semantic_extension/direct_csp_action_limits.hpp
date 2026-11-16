#pragma once

#include "parameter_schema.hpp"

#include <cstdint>

namespace tactix {

struct DirectCspGuardedCoverActionLimits {
  TACTIX_DECLARE_PARAMETER_FIELDS(DirectCspGuardedCoverParameters)

  bool valid() const;
  bool operator==(const DirectCspGuardedCoverActionLimits &) const = default;
};

struct DirectCspSupportActionLimits {
  TACTIX_DECLARE_PARAMETER_FIELDS(DirectCspSupportParameters)

  bool valid() const;
};

} // namespace tactix
