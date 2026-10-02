#pragma once

#include <cstdint>

namespace tactix::strategy {

enum class RuleWorkBudgetMode : uint8_t { Inherit, Unlimited, Bounded };

// Native effort is separate from the composition budget. Bounded includes
// zero; Inherit leaves the caller's wires untouched for the application.
struct RuleWorkBudget {
  RuleWorkBudgetMode mode = RuleWorkBudgetMode::Inherit;
  uint64_t amount = 0;
};

enum class SettingOverride : uint8_t { Inherit, Disable, Enable };

// Discovery settings do not filter explicit semantic instances.
struct RuleMaterialSettings {
  SettingOverride touch_filter = SettingOverride::Inherit;
  SettingOverride likely_kept_redundant = SettingOverride::Inherit;
};

} // namespace tactix::strategy
