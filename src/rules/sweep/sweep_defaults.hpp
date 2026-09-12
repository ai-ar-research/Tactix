#pragma once

#include "strategy/sweep_parameters.hpp"

namespace tactix {

// Compatibility alias for external source that included the old rule-local
// path. New code names the strategy-owned type directly.
using SweepReferenceDefaults = strategy::SweepReferenceDefaults;

} // namespace tactix
