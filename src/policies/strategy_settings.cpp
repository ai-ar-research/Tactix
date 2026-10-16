#include "policies/strategy_settings.hpp"

#include <cstdlib>
#include <cstring>

namespace tactix {

namespace {
thread_local const StrategySettings *active = nullptr;
}

const char *strategy_setting(const char *name) {
  if (active)
    for (const auto &[key, value] : *active)
      if (key == name) return value.c_str();
  return std::getenv(name);
}

ScopedStrategySettings::ScopedStrategySettings(const StrategySettings *settings)
    : previous_(active) {
  active = settings;
}

ScopedStrategySettings::~ScopedStrategySettings() { active = previous_; }

} // namespace tactix
