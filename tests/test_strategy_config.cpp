#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/reduce/reduce.hpp"

#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>

using namespace tactix;

static void add_php_4_3(Kernel &k) {
  kernel_reserve_vars(k, 12);
  const auto variable = [](int pigeon, int hole) {
    return pigeon * 3 + hole + 1;
  };
  std::vector<Lit> clause;
  for (int pigeon = 0; pigeon < 4; ++pigeon) {
    clause.clear();
    for (int hole = 0; hole < 3; ++hole)
      clause.push_back(internalize(variable(pigeon, hole)));
    kernel_add_large_clause(k, clause.data(), clause.size(), false, 0);
  }
  for (int hole = 0; hole < 3; ++hole)
    for (int first = 0; first < 4; ++first)
      for (int second = first + 1; second < 4; ++second)
        kernel_add_binary_clause(k, internalize(-variable(first, hole)),
                                 internalize(-variable(second, hole)));
}

int main() {
  DefaultStrategyConfig defaults;
  assert(defaults.reduction.initial_interval ==
         DefaultPolicy::REDUCE_INTERVAL_INIT);
  assert(defaults.mode.interval == DefaultPolicy::MODE_INTERVAL);
  assert(defaults.inprocess.eliminate_interval ==
         DefaultInprocessScheduler::SUBSUME_INTERVAL);
  assert(defaults.analysis.bump_reason_rate ==
         DefaultPolicy::BUMP_REASONS_RATE);
  assert(defaults.reduction.tier1_default == TIER1_STATIC);
  assert(defaults.reduction.tier2_default == TIER2_STATIC);
  assert(defaults.branching.random_seed == 0);

  // Kissat's first seed-0 LCG value selects internal variable 254 of 260.
  // Pin both the generator and the removal of the old bounded-retry fallback.
  Kernel random;
  ScopedHeuristics random_heuristics(random);
  kernel_reserve_vars(random, 260);
  random.stats.conflicts = 855;
  DefaultBranching reference_random(&defaults.branching);
  reference_random.reset(0);
  const Lit random_lit = reference_random.pick(random);
  if (kernel_externalize(random, random_lit) != 255 ||
      random.stats.random_sequences != 1 ||
      random.stats.random_decisions != 1)
    return 1;

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

  // Kissat recomputes the current mode's tier snapshot at every Reduce in
  // addition to its analysis-side cadence. Pose Reduce at conflict zero and
  // put the scheduled retier far away so every recomputation is attributable
  // to one reduction boundary.
  DefaultStrategyConfig cadence = defaults;
  cadence.reduction.retier_initial_interval = 1ull << 40;
  Kernel php;
  ScopedHeuristics php_heuristics(php);
  add_php_4_3(php);
  DefaultPolicy scheduled_only(cadence);
  scheduled_only.reset_schedule_per_solve = false;
  scheduled_only.reduce.next_conflicts = 0;
  assert(scheduled_only.solve(php) == 20);
  assert(php.stats.reductions > 0);
  assert(php.stats.retiered == php.stats.reductions &&
         "every Reduce must refresh exactly one current-mode tier snapshot");
}
