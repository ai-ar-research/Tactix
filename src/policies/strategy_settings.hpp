// Named settings of the default strategy.
//
// The default strategy reads its optional settings by name (TACTIX_*). A C
// interface session can set them explicitly (tactix_set_default_option);
// while that session solves, its settings take precedence over the process
// environment, which remains the fallback for the command line.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace tactix {

using StrategySettings = std::vector<std::pair<std::string, std::string>>;

// The session's value for `name` if one is in scope, else the environment's.
const char *strategy_setting(const char *name);

// Makes `settings` visible to strategy_setting on this thread for the scope.
class ScopedStrategySettings {
public:
  explicit ScopedStrategySettings(const StrategySettings *settings);
  ~ScopedStrategySettings();
  ScopedStrategySettings(const ScopedStrategySettings &) = delete;
  ScopedStrategySettings &operator=(const ScopedStrategySettings &) = delete;

private:
  const StrategySettings *previous_;
};

} // namespace tactix
