#pragma once

#include "parameter_schema.hpp"

#include <cstdint>

namespace tactix {

struct NaryCspEqualityLimits {
  TACTIX_DECLARE_PARAMETER_FIELDS(NaryCspEqualityParameters)

  bool valid() const;
  bool operator==(const NaryCspEqualityLimits &) const = default;
};

struct NaryCspEqualityAmoLimits {
  TACTIX_DECLARE_PARAMETER_FIELDS(NaryCspEqualityAmoParameters)

  bool valid() const;
  bool operator==(const NaryCspEqualityAmoLimits &) const = default;
};

struct NaryCspEqualityTransitivityLimits {
  TACTIX_DECLARE_PARAMETER_FIELDS(NaryCspEqualityTransitivityParameters)

  bool valid() const;
  bool operator==(const NaryCspEqualityTransitivityLimits &) const = default;
};

} // namespace tactix
