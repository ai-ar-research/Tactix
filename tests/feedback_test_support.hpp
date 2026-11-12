#pragma once
// The search-only DefaultPolicy profile of the policy-feedback tests.
#include "policies/default_strategy_config.hpp"

// No inprocessing; rephase, mode switch, restart and reduction every few
// conflicts, so a short search exercises each feedback kind.
inline tactix::DefaultStrategyConfig search_profile() {
  tactix::DefaultStrategyConfig config;
  config.inprocess.program.clear();
  config.inprocess.append_unlisted_rules = false;
  config.inprocess.structured_root_stages.clear();
  config.inprocess.structured_root_bounded_backtracking_stages.clear();
  config.inprocess.structured_root_large_domain_stages.clear();
  config.inprocess.initial_fast_elimination = false;
  config.rephase.late_lucky = false;
  config.rephase.stable_only = false;
  config.rephase.initial_interval = config.rephase.interval = 1;
  config.rephase.rotation = {tactix::DefaultRephaseSlot::Walk};
  config.rephase.walk_permille = 1;
  config.mode.initial_conflicts = config.mode.interval = 1;
  config.restart.reluctant_period = 2;
  config.reduction.initial_interval = 5;
  return config;
}

