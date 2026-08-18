#undef NDEBUG
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

  const auto classified = [](uint32_t vars, uint64_t clauses) {
    Kernel shape;
    shape.original_external_vars = vars;
    shape.original_input_clauses = clauses;
    DefaultPolicy policy;
    policy.classify_formula_strategy(shape);
    return policy.strategy_config();
  };
  const auto schedules_congruence = [](uint32_t vars, uint64_t clauses) {
    Kernel shape;
    shape.original_external_vars = vars;
    shape.original_input_clauses = clauses;
    DefaultPolicy policy;
    policy.classify_formula_strategy(shape);
    policy.inprocess().build(shape);
    const auto &rules = tactix_rules();
    for (const auto &entry : policy.inprocess().entries)
      if (std::string(rules[entry.rule].name) == "Congruence")
        return entry.enabled && entry.preprocess;
    return false;
  };
  const auto schedules_startup_factor = [](uint32_t vars, uint64_t clauses) {
    Kernel shape;
    shape.original_external_vars = vars;
    shape.original_input_clauses = clauses;
    DefaultPolicy policy;
    policy.classify_formula_strategy(shape);
    policy.inprocess().build(shape);
    const auto &rules = tactix_rules();
    for (const auto &entry : policy.inprocess().entries)
      if (std::string(rules[entry.rule].name) == "Factor")
        return entry.enabled && entry.preprocess;
    return false;
  };
  // Pin the causal Phase-4 admission witnesses rather than their filenames.
  const auto mp = classified(6360, 25200);
  assert(!mp.inprocess.factor_repeated_enabled &&
         !mp.inprocess.vivify_enabled);
  const auto stconn = classified(936, 6488);
  assert(stconn.inprocess.preprocess_only &&
         stconn.branching.random_sequences && stconn.reorder.mode == 0 &&
         !stconn.reduction.dynamic_tiers);
  const auto grain105 = classified(3094431, 10684799);
  assert(grain105.inprocess.preprocess_only &&
         grain105.branching.random_sequences);
  const auto grain87 = classified(3165161, 10917078);
  assert(grain87.inprocess.preprocess_only &&
         !grain87.inprocess.factor_repeated_enabled &&
         grain87.branching.random_sequences);
  const auto dense_small = classified(862, 855716);
  assert(!dense_small.inprocess.bve_enabled &&
         dense_small.inprocess.forward_midround_budget &&
         dense_small.inprocess.vivify_permille ==
             DefaultStrategyConfig::PROFILE_LIGHT_PERMILLE &&
         dense_small.inprocess.subsume_forward_permille ==
             DefaultStrategyConfig::PROFILE_LIGHT_PERMILLE);
  const auto dense_large = classified(3032, 2000000);
  assert(!dense_large.inprocess.bve_enabled &&
         dense_large.inprocess.forward_midround_budget &&
         dense_large.inprocess.vivify_permille ==
             DefaultStrategyConfig::Inprocess{}.vivify_permille);
  assert(!mp.inprocess.forward_midround_budget);
  assert(schedules_congruence(6360, 25200));
  assert(schedules_congruence(862, 855716));
  assert(schedules_congruence(3032, 2000000));
  assert(schedules_startup_factor(450, 20035));
  assert(schedules_startup_factor(6360, 25200));
  assert(schedules_startup_factor(3165161, 10917078));

  // The measured Grain witness of the startup-only schedule keeps its benefit
  // and the late Lucky opportunity together.
  const auto grain_late = classified(3859779, 13462874);
  assert(grain_late.inprocess.preprocess_only &&
         !grain_late.inprocess.factor_repeated_enabled &&
         grain_late.branching.random_sequences &&
         grain_late.reorder.mode == 0 &&
         !grain_late.reduction.dynamic_tiers &&
         grain_late.rephase.late_lucky);
  // A profile may dose or decline an inprocessing slot; none may withdraw the
  // late Lucky opportunity, which both references offer on every formula
  // (kissat search.c:188, options.h:79).  Sweeping the classification space
  // rather than a witness list is the point: the defect this pins made the
  // closed late call unreachable for whole formula classes at once.
  static const uint64_t GRID_VARS[] = {
      1,       2,       127,     128,     129,     299,      300,
      301,     499,     500,     501,     799,     800,      801,
      862,     899,     900,     901,     936,     999,      1000,
      1001,    1499,    1500,    1999,    2000,    2999,     3000,
      3032,    3999,    4000,    4999,    5000,    5999,     6000,
      6360,    6499,    6500,    6999,    7000,    9999,     10000,
      14999,   15000,   19999,   20000,   24999,   25000,    31999,
      32000,   39999,   40000,   49999,   50000,   69999,    70000,
      119999,  120000,  199999,  200000,  299999,  300000,   499999,
      500000,  799999,  800000,  1000000, 2999999, 3000000,  3094431,
      3129999, 3130000, 3165161, 3859779, 9999999, 10000000, 20000000};
  static const uint64_t GRID_RATIOS[] = {0,  1,  3,  4,  5,  6,   9,
                                         10, 11, 19, 20, 21, 49,  50,
                                         51, 59, 60, 61, 89, 90,  91,
                                         199, 200, 201, 499, 500, 501, 1000};
  for (const uint64_t shape_vars : GRID_VARS)
    for (const uint64_t ratio : GRID_RATIOS)
      for (const int64_t nudge : {int64_t{-1}, int64_t{0}, int64_t{1}}) {
        const int64_t shape_clauses =
            static_cast<int64_t>(ratio * shape_vars) + nudge;
        if (shape_clauses < 0)
          continue;
        const auto shaped = classified(static_cast<uint32_t>(shape_vars),
                                       static_cast<uint64_t>(shape_clauses));
        assert(shaped.rephase.late_lucky);
        assert(shaped.branching.random_sequences);
      }
  assert(grain87.rephase.late_lucky && stconn.rephase.late_lucky &&
         dense_small.rephase.late_lucky && dense_large.rephase.late_lucky &&
         mp.rephase.late_lucky);

  // Formula shape never chooses the reference-disagreement arm.  A caller
  // can still select CaDiCaL's randec-off policy explicitly, and
  // classification must preserve that typed choice.
  DefaultStrategyConfig profile_no_random = defaults;
  profile_no_random.branching.random_sequences = false;
  DefaultPolicy no_random_profile(profile_no_random);
  Kernel legacy_shape;
  legacy_shape.original_external_vars = 3859779;
  legacy_shape.original_input_clauses = 13462874;
  no_random_profile.classify_formula_strategy(legacy_shape);
  assert(!no_random_profile.strategy_config().branching.random_sequences);

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

  // An API/owned-H path may parse before DefaultPolicy attaches `H`. Preserve
  // the tumble population across that boundary: header-only variables must not
  // change Kissat's random-index modulus.
  Kernel late_tumbled;
  kernel_reserve_vars(late_tumbled, 5);
  late_tumbled.input_activation_order = {0, 2, 4};
  {
    ScopedHeuristics late_heuristics(late_tumbled);
    if (late_heuristics.h.random_order != std::vector<Var>({0, 2, 4}) ||
        !late_tumbled.input_activation_order.empty())
      return 1;
  }

  Kernel compacted_random;
  ScopedHeuristics compacted_heuristics(compacted_random);
  kernel_reserve_vars(compacted_random, 5);
  heuristics_seed_activation_order(compacted_random, {2, 0, 4, 1, 3});
  kernel_assign_unit(compacted_random, make_lit(0, false));
  kernel_assign_unit(compacted_random, make_lit(1, true));
  heuristics_compact_random_order_fixed(compacted_random);
  if (compacted_heuristics.h.random_order !=
          std::vector<Var>({2, 0, 4, 3}) ||
      compacted_heuristics.h.fixed_representative != 0)
    return 1;
  // A later preprocessing rule can derive another root unit. It remains in
  // the random-index population but is excluded from the first stable heap;
  // the original compacted representative stays in the random population but
  // is fixed too, so Kissat excludes it from the heap as well.
  kernel_assign_unit(compacted_random, make_lit(3, false));
  heuristics_update_scores(compacted_random);
  if (compacted_heuristics.h.random_order !=
          std::vector<Var>({2, 0, 4, 3}) ||
      heuristics_heap_contains(compacted_random, 0) ||
      heuristics_heap_contains(compacted_random, 3))
    return 1;

  // `kissat_decide` increments its level before checking the random-sequence
  // deadline.  DefaultBranching is called before Tactix Decide adds the frame,
  // so a reused level-one restart prefix normally corresponds to Kissat level
  // two and keeps using the ranked brancher even when randec is due.
  Kernel reused;
  ScopedHeuristics reused_heuristics(reused);
  kernel_reserve_vars(reused, 501);
  reused.level = 1;
  reused.stats.conflicts = 855;
  DefaultBranching reused_random(&defaults.branching);
  reused_random.reset(0);
  if (kernel_externalize(reused, reused_random.pick(reused)) != 501 ||
      reused.stats.random_sequences != 0 ||
      reused.stats.random_decisions != 0)
    return 1;

  // Compact original-only universes fit inside one complete reference random
  // interval.  Letting their retained level-one prefix start the due sequence
  // prevents every restart from replaying the same ranked tail; extension or
  // larger universes retain the faithful boundary above.
  Kernel compact_reused;
  ScopedHeuristics compact_reused_heuristics(compact_reused);
  kernel_reserve_vars(compact_reused, 3);
  compact_reused.level = 1;
  compact_reused.stats.conflicts = 855;
  DefaultBranching compact_reused_random(&defaults.branching);
  compact_reused_random.reset(0);
  (void)compact_reused_random.pick(compact_reused);
  if (compact_reused.stats.random_sequences != 1 ||
      compact_reused.stats.random_decisions != 1)
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
  custom.inprocess.sweep_vars = 67;
  custom.inprocess.factor_active_ceiling = 71;
  custom.inprocess.sweep_skip_embedded_fixed = false;
  custom.inprocess.sweep_resume = false;
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
  assert(snapshot.find("inprocess.sweep_vars=67") != std::string::npos);
  assert(snapshot.find("inprocess.factor_active_ceiling=71") !=
         std::string::npos);
  assert(snapshot.find("inprocess.sweep_skip_embedded_fixed=0") !=
         std::string::npos);
  assert(snapshot.find("inprocess.sweep_resume=0") != std::string::npos);

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
