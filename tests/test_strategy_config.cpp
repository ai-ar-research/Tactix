#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"

#include <cassert>
#include <cstdlib>
#include <string>

using namespace tactix;

int main() {
  DefaultStrategyConfig defaults;
  assert(defaults.reduction.initial_interval ==
         DefaultPolicy::REDUCE_INTERVAL_INIT);
  assert(defaults.mode.interval == DefaultPolicy::MODE_INTERVAL);
  assert(defaults.inprocess.eliminate_interval ==
         DefaultInprocessScheduler::SUBSUME_INTERVAL);
  assert(defaults.analysis.bump_reason_rate ==
         DefaultPolicy::BUMP_REASONS_RATE);

  DefaultStrategyConfig custom = defaults;
  custom.reduction.initial_interval = 37;
  custom.mode.initial_conflicts = 41;
  custom.rephase.initial_interval = 43;
  custom.reorder.initial_interval = 47;
  custom.analysis.chronological_levels = 53;
  custom.inprocess.probe_permille = 59;
  DefaultPolicy policy(custom);
  Kernel k;
  ScopedHeuristics heuristics(k);
  policy.reset_schedule(k);
  assert(policy.reduce.next_conflicts == 37);
  assert(policy.mode.conflicts_limit == 41);
  assert(policy.rephase.next_conflicts == 43);
  assert(policy.reorder.next_conflicts == 47);
  assert(policy.chrono_levels == 53);
  const std::string snapshot = policy.strategy_config_snapshot();
  assert(snapshot.find("reduction.initial_interval=37") != std::string::npos);
  assert(snapshot.find("inprocess.probe_permille=59") != std::string::npos);

  // Environment arms overlay the typed profile and remain visible in its
  // readback rather than becoming hidden use-site constants.
  setenv("TACTIX_BUMP_REASONS_RATE", "17", 1);
  setenv("TACTIX_WALK_PERMILLE", "61", 1);
  Kernel one;
  kernel_reserve_vars(one, 1);
  DefaultPolicy overlaid;
  assert(overlaid.solve(one) == 10);
  assert(overlaid.strategy_config().analysis.bump_reason_rate == 17.0);
  assert(overlaid.strategy_config().rephase.walk_permille == 61);
  unsetenv("TACTIX_BUMP_REASONS_RATE");
  unsetenv("TACTIX_WALK_PERMILLE");
}
