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
  if (defaults.branching.focused_phase_switch_span != 2)
    return 1;
  const std::array<int8_t, 8> focused_phases{{0, 1, 0, -1, 0, 0, 0, 0}};
  if (defaults.branching.focused_phase_pattern != focused_phases)
    return 1;

  // Kissat rotates two focused epochs through forced initial/opposite phases.
  // This applies equally to ranked and random decisions.
  DefaultStrategyConfig no_random = defaults;
  no_random.branching.random_sequences = false;
  Kernel phase;
  ScopedHeuristics phase_heuristics(phase);
  kernel_reserve_vars(phase, 1);
  DefaultBranching phase_branching(&no_random.branching);
  phase.h->saved[0] = -1;
  phase.stats.switched = 2;
  if (phase_branching.pick(phase) != internalize(1))
    return 1;
  phase.stats.switched = 4;
  if (phase_branching.pick(phase) != internalize(-1))
    return 1;
  phase.h->saved[0] = 1;
  phase.stats.switched = 6;
  if (phase_branching.pick(phase) != internalize(-1))
    return 1;

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
  // Restart reuse previews the active brancher without assigning its result.
  // Kissat counts/consumes that draw, so the real decision receives draw two.
  if (kernel_externalize(random, reference_random.pick(random)) != 130 ||
      random.stats.random_decisions != 2)
    return 1;

  Kernel tumbled;
  ScopedHeuristics tumbled_heuristics(tumbled);
  kernel_reserve_vars(tumbled, 3);
  heuristics_seed_activation_order(tumbled, {2, 0, 1});
  tumbled.stats.conflicts = 855;
  DefaultBranching tumbled_random(&defaults.branching);
  tumbled_random.reset(0);
  // First seed-0 index is 1 modulo 3, which tumble maps to variable 0.
  if (kernel_externalize(tumbled, tumbled_random.pick(tumbled)) != 1)
    return 1;

  // A stable->focused transition starts a sequence even above decision level
  // one and before the ordinary randec conflict deadline.
  Kernel switched;
  ScopedHeuristics switched_heuristics(switched);
  kernel_reserve_vars(switched, 3);
  switched.level = 12;
  switched.stats.conflicts = 1844;
  DefaultBranching switched_random(&defaults.branching);
  switched_random.reset(1500); // ordinary deadline 2000: not due yet
  switched_random.start_random_sequence(switched);
  if (switched.stats.random_sequences != 1 ||
      switched_random.randec_until != 1854)
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
