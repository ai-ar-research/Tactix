#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/reduce/reduce.hpp"

#include <cassert>
#include <cstdlib>
#include <limits>
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
  assert(defaults.inprocess.substitute_rounds ==
         DefaultStrategyConfig::SUBSTITUTE_ROUNDS_DEFAULT);
  assert(defaults.inprocess.substitute_search_rounds == 1);
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

  assert(defaults.analysis.small_clause_limit == 100000);
  assert(defaults.analysis.big_binary_fraction_permille == 990);
  assert(defaults.analysis.conflict_reuse && defaults.analysis.jump_reasons);
  assert(defaults.analysis.shrink_glue_promote &&
         defaults.analysis.bump_extension_reasons);
  assert(defaults.analysis.eager_subsume ==
         strategy::AnalyzeParameters::EAGER_SUBSUME_CAPACITY);
  assert(defaults.inprocess.bve_enabled &&
         defaults.inprocess.bve_permille == 100);
  assert(defaults.inprocess.bve_xor_arity_limit == 5);
  assert(defaults.inprocess.bve_resolvent_size_limit == 100);
  assert(defaults.inprocess.bve_backward_occurrence_limit == 100);
  assert(defaults.inprocess.bve_definition_budget == 1000000);
  assert(defaults.inprocess.bve_definition_cores == 2);
  assert(defaults.inprocess.bve_definition_core_budget_multiplier == 10);
  assert(defaults.inprocess.factor_repeated_enabled &&
         defaults.inprocess.factor_permille == 50 &&
         defaults.inprocess.factor_active_ceiling == 0);
  assert(!defaults.inprocess.cadical_arena_ordering_enabled &&
         "production must retain Kissat creation order");
  assert(defaults.inprocess.subsume_forward_permille == 100);
  assert(defaults.inprocess.vivify_enabled &&
         defaults.inprocess.vivify_permille == 100);
  std::string validation_error;
  assert(defaults.valid(&validation_error) && validation_error.empty());
  {
    DefaultStrategyConfig invalid = defaults;
    invalid.inprocess.vivify_total_share = 0;
    assert(!invalid.valid(&validation_error));
    assert(!validation_error.empty());
  }
  {
    DefaultStrategyConfig invalid = defaults;
    invalid.inprocess.program[0].effort_permille = 1001;
    assert(!invalid.valid(&validation_error));
  }
  {
    DefaultStrategyConfig invalid = defaults;
    invalid.inprocess.vivify_irredundant_productivity_percent = 101;
    assert(!invalid.valid(&validation_error));
  }
  for (const double invalid_percent :
       {-1.0, 100.01, std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN()}) {
    DefaultStrategyConfig invalid_low = defaults;
    invalid_low.reduction.low_percent = invalid_percent;
    assert(!invalid_low.valid(&validation_error));
    DefaultStrategyConfig invalid_high = defaults;
    invalid_high.reduction.high_percent = invalid_percent;
    assert(!invalid_high.valid(&validation_error));
  }
  {
    // Kissat accepts both endpoints independently.  When low is at least
    // high, its reduction fraction stays at low instead of ramping downward.
    DefaultStrategyConfig reverse = defaults;
    reverse.reduction.low_percent = 70.0;
    reverse.reduction.high_percent = 30.0;
    assert(reverse.valid(&validation_error));
    GlueTieredReduction reduction(&reverse.reduction);
    reduction.ramp = true;
    Kernel k;
    k.stats.reductions = 99;
    assert(reduction.victims(k, 100) == 70);
  }

  {
    DefaultStrategyConfig typed = defaults;
    typed.inprocess.bve_backward = true;
    typed.inprocess.bce_parameters.occurrence_limit = 11;
    typed.inprocess.ternary_parameters = {12, 13};
    typed.inprocess.factor_parameters = {4, 5, 6, true, false};
    typed.inprocess.transitive_parameters = {false, false, true};
    typed.inprocess.substitute_parameters = {true};
    typed.inprocess.vivify_parameters = {3, false, true, 14};
    Kernel posed;
    kernel_reserve_vars(posed, 1);
    DefaultPolicy typed_policy(typed);
    typed_policy.inprocess().build(posed);
    assert(posed.bve_backward && posed.bce_occ_limit == 11);
    assert(posed.ternary_occ_limit == 12 &&
           posed.ternary_max_add_percent == 13);
    assert(posed.factor_size == 4 && posed.factor_bound == 5 &&
           posed.factor_cand_rounds == 6 && posed.factor_composite &&
           !posed.factor_demote_fresh);
    assert(!posed.transitive_early_exit && !posed.transitive_live_meter &&
           posed.transitive_budget_checked && posed.subst_demote);
    assert(posed.vivify_scope == 3 && !posed.vivify_reuse &&
           posed.vivify_flush && posed.vivify_irredundant_sort_ratio == 14);
  }
  {
    DefaultStrategyConfig invalid = defaults;
    invalid.analysis.eager_subsume =
        strategy::AnalyzeParameters::EAGER_SUBSUME_CAPACITY + 1;
    assert(!invalid.valid(&validation_error));
  }

  DefaultPolicy classifier;
  const auto at_limit = classifier.classify_formula_counts(100000, 0);
  assert(at_limit.small && !at_limit.bigbig);
  const auto above_below_fraction =
      classifier.classify_formula_counts(990, 100001 - 990);
  assert(!above_below_fraction.small && !above_below_fraction.bigbig);
  const auto above_at_fraction =
      classifier.classify_formula_counts(991, 100001 - 991);
  assert(!above_at_fraction.small && above_at_fraction.bigbig);

  // Kissat's `classification.small` is write-only and `bigbig` has exactly
  // one consumer: ordinary fast binary assignment. Classification therefore
  // cannot mutate any typed strategy field or create a formula profile.
  const std::string production_snapshot = classifier.strategy_config_snapshot();
  static const uint64_t GRID_VARS[] = {1, 128, 862, 1042, 6360, 126152,
                                       3000000, 10000000};
  static const uint64_t GRID_RATIOS[] = {0, 1, 4, 10, 50, 500, 1000};
  for (const uint64_t vars : GRID_VARS)
    for (const uint64_t ratio : GRID_RATIOS) {
      const uint64_t clauses = vars * ratio;
      (void)classifier.classify_formula_counts(clauses / 2,
                                               clauses - clauses / 2);
      assert(classifier.strategy_config_snapshot() == production_snapshot &&
             "formula size/density must not change production strategy");
    }

  // Explicit typed overrides remain diagnostic arms and classification must
  // neither activate nor erase them.
  DefaultStrategyConfig diagnostic = defaults;
  diagnostic.inprocess.bve_enabled = false;
  diagnostic.inprocess.factor_repeated_enabled = false;
  diagnostic.inprocess.bve_permille = 7;
  diagnostic.inprocess.factor_permille = 11;
  diagnostic.inprocess.subsume_forward_permille = 13;
  diagnostic.inprocess.vivify_enabled = false;
  diagnostic.inprocess.vivify_permille = 17;
  diagnostic.inprocess.forward_midround_budget = true;
  diagnostic.inprocess.cadical_arena_ordering_enabled = true;
  DefaultPolicy explicit_arm(diagnostic);
  const std::string diagnostic_snapshot = explicit_arm.strategy_config_snapshot();
  (void)explicit_arm.classify_formula_counts(1000000, 1);
  assert(explicit_arm.strategy_config_snapshot() == diagnostic_snapshot);

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
  // Import and activation order are separate under Kissat's tumble. Compact
  // indices preserve the former while queue/score seeding follows the latter.
  kernel_record_import(tumbled, 1);
  kernel_record_import(tumbled, 2);
  kernel_record_import(tumbled, 0);
  heuristics_seed_activation_order(tumbled, {2, 0, 1});
  if (tumbled.compact_import_order != std::vector<Var>({1, 2, 0}))
    return 1;
  tumbled.stats.conflicts = 855;
  DefaultBranching tumbled_random(&defaults.branching);
  tumbled_random.reset(0);
  // First seed-0 index is 1 modulo 3, the second compact import (variable 2).
  if (kernel_externalize(tumbled, tumbled_random.pick(tumbled)) != 3)
    return 1;

  // An API/owned-H path may parse before DefaultPolicy attaches `H`. Preserve
  // the tumble population across that boundary: header-only variables must not
  // change Kissat's random-index modulus.
  Kernel late_tumbled;
  kernel_reserve_vars(late_tumbled, 5);
  late_tumbled.input_activation_order = {0, 2, 4};
  {
    ScopedHeuristics late_heuristics(late_tumbled);
    if (late_tumbled.compact_import_order != std::vector<Var>({0, 2, 4}) ||
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
  if (compacted_random.compact_import_order !=
          std::vector<Var>({0, 2, 3, 4}) ||
      compacted_heuristics.h.fixed_representative != 0)
    return 1;
  // A later preprocessing rule can derive another root unit. It remains in
  // the random-index population but is excluded from the first stable heap;
  // the original compacted representative stays in the random population but
  // is fixed too, so Kissat excludes it from the heap as well.
  kernel_assign_unit(compacted_random, make_lit(3, false));
  heuristics_update_scores(compacted_random);
  if (compacted_random.compact_import_order !=
          std::vector<Var>({0, 2, 3, 4}) ||
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

  // Restart reuse calls Kissat's next-decision selector without first adding
  // a frame.  At retained level one an overdue random sequence therefore
  // starts and consumes one draw, even though an ordinary Tactix pick at the
  // same pre-frame level must stay ranked (the case above).
  DefaultBranching restart_reused_random(&defaults.branching);
  restart_reused_random.reset(0);
  const Lit restart_reused_choice =
      restart_reused_random.pick_for_restart_reuse(reused);
  if (reused.stats.random_sequences != 1 ||
      reused.stats.random_decisions != 1 ||
      restart_reused_random.randec_remaining != 10 ||
      restart_reused_choice == INVALID_LIT)
    return 1;

  // Kissat rejects the same due sequence above retained level one.
  Kernel deep_reused;
  ScopedHeuristics deep_reused_heuristics(deep_reused);
  kernel_reserve_vars(deep_reused, 3);
  deep_reused.level = 2;
  deep_reused.stats.conflicts = 855;
  DefaultBranching deep_reused_random(&defaults.branching);
  deep_reused_random.reset(0);
  (void)deep_reused_random.pick_for_restart_reuse(deep_reused);
  if (deep_reused.stats.random_sequences != 0 ||
      deep_reused.stats.random_decisions != 0)
    return 1;

  // Formula size does not create another admission path.  A compact original-
  // only universe at the same retained level-one prefix is still Kissat level
  // two after `kissat_decide` increments the level, so it must remain ranked.
  Kernel compact_reused;
  ScopedHeuristics compact_reused_heuristics(compact_reused);
  kernel_reserve_vars(compact_reused, 3);
  compact_reused.level = 1;
  compact_reused.stats.conflicts = 855;
  DefaultBranching compact_reused_random(&defaults.branching);
  compact_reused_random.reset(0);
  if (kernel_externalize(compact_reused,
                         compact_reused_random.pick(compact_reused)) != 3 ||
      compact_reused.stats.random_sequences != 0 ||
      compact_reused.stats.random_decisions != 0)
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
      switched_random.randec_remaining != 10)
    return 1;

  // Kissat charges random-sequence lifetime only when search propagation
  // returns a conflict. An OTFS conflict added inside analysis advances the
  // global conflict count but does not consume the sequence counter.
  switched_random.on_search_propagation_conflict();
  switched.stats.conflicts++;
  if (switched_random.randec_remaining != 9)
    return 1;
  switched.stats.conflicts++; // analysis/OTFS conflict: deliberately uncharged
  if (switched_random.randec_remaining != 9)
    return 1;
  for (unsigned i = 0; i != 8; ++i)
    switched_random.on_search_propagation_conflict();
  if (switched_random.randec_remaining != 1)
    return 1;
  const uint64_t random_decisions_before = switched.stats.random_decisions;
  (void)switched_random.pick(switched);
  if (switched.stats.random_decisions != random_decisions_before + 1)
    return 1;
  switched_random.on_search_propagation_conflict();
  (void)switched_random.pick(switched);
  if (switched_random.randec_remaining ||
      switched.stats.random_decisions != random_decisions_before + 1)
    return 1;

  DefaultStrategyConfig custom = defaults;
  custom.reduction.initial_interval = 37;
  custom.mode.initial_conflicts = 41;
  custom.rephase.initial_interval = 43;
  custom.reorder.initial_interval = 47;
  custom.analysis.chronological_levels = 53;
  custom.inprocess.probe_permille = 59;
  custom.inprocess.sweep_parameters.max_env_vars = 67;
  custom.inprocess.factor_active_ceiling = 71;
  custom.inprocess.sweep_parameters.skip_embedded_fixed = false;
  custom.inprocess.sweep_parameters.resume = false;
  custom.inprocess.sweep_parameters.force_general_proof = true;
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
  assert(snapshot.find("inprocess.sweep_force_general_proof=1") !=
         std::string::npos);

  // The arithmetic cadence changes only deadlines after a reduction.  Kissat
  // always seeds the first deadline from `reduceinit`.
  custom.reduction.square_root_cadence = false;
  custom.reduction.arithmetic_interval = 91;
  DefaultPolicy arithmetic(custom);
  arithmetic.reset_schedule(k);
  assert(arithmetic.reduce.next_conflicts == 37);

  // The configured reluctant period must govern the first cycle too;
  // subsequent ticks must not be the first place that reads the setting.
  custom.restart.reluctant_period = 3;
  DefaultPolicy short_restart(custom);
  short_restart.reset_schedule(k);
  assert(short_restart.reluctant.wait == 3);
  short_restart.tick_reluctant();
  short_restart.tick_reluctant();
  assert(!short_restart.reluctant.trigger);
  short_restart.tick_reluctant();
  assert(short_restart.reluctant.trigger);

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
