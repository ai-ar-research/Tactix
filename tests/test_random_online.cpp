#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/random_policy.hpp"
#include "policies/trace.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/random_online.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <unistd.h>

using namespace tactix;
using namespace tactix::strategy;

static Kernel broad_fixture() {
  Kernel k;
  kernel_reserve_vars(k, 42);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  for (int variable = 3; variable <= 42; ++variable) {
    const Lit victim[] = {internalize(1), internalize(2),
                          internalize(variable)};
    kernel_add_large_clause(k, victim, 3, false, 0);
  }
  return k;
}

static void add3(Kernel &k, int a, int b, int c) {
  const Lit clause[] = {internalize(a), internalize(b), internalize(c)};
  kernel_add_large_clause(k, clause, 3, false, 0);
}

static Kernel layered_congruence_fixture() {
  Kernel k;
  kernel_reserve_vars(k, 5);
  for (const int output : {3, 4}) {
    kernel_add_binary_clause(k, internalize(-output), internalize(1));
    kernel_add_binary_clause(k, internalize(-output), internalize(2));
    add3(k, output, -1, -2);
  }
  add3(k, -5, 3, 4);
  add3(k, 5, -3, 4);
  add3(k, 5, 3, -4);
  add3(k, -5, -3, -4);
  return k;
}

static Kernel sr_fixture() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  const auto add = [&](int a, int b) {
    kernel_add_binary_clause(k, internalize(a), internalize(b),
                             ++k.next_clause_id);
  };
  add(1, 2);
  add(3, 4);
  add(-1, -3);
  add(-2, -4);
  return k;
}

static void invalid_configuration_is_rejected() {
  Kernel k = broad_fixture();
  OnlineStrategyProgram output;
  std::string error;

  RandomOnlineConfig config;
  config.maximum_sequence_length = 0;
  assert(!random_online_strategy_program(k, 1, config, output, &error));
  assert(error == "random-online maxima must be positive");

  config = {};
  config.maximum_bve_definition_cores = 0;
  assert(!random_online_strategy_program(k, 1, config, output, &error));

  config = {};
  config.rules = {"not-a-registered-action"};
  assert(!random_online_strategy_program(k, 1, config, output, &error));
  assert(error == "random-online action is not registered");
}

static void seeds_are_replayable_and_behavior_is_wide() {
  Kernel k = broad_fixture();
  RandomOnlineConfig config;
  config.rules = {"SubsumeForward", "Strengthen", "Congruence", "BVE",
                  "Backbone", "Instantiate", "Vivify", "BCE",
                  "HyperTernaryRes", "Factor", "TransitiveReduce",
                  "Substitute", "Reduce"};

  bool granularities[4] = {};
  bool repeats[3] = {};
  bool repetitions_by_granularity[4][3] = {};
  bool sequence_repeats[3] = {};
  bool rule_work_budgets[3] = {};
  bool touch_filter_modes[3] = {};
  bool redundant_modes[3] = {};
  bool bounded_budget = false, unlimited_budget = false;
  bool unlimited_noncomplete = false;
  bool long_sequence = false;
  std::set<std::string> orderings;
  std::set<std::string> parameterizations;

  for (uint64_t seed = 1; seed <= 2048; ++seed) {
    OnlineStrategyProgram first, second;
    std::string error;
    assert(random_online_strategy_program(k, seed, config, first, &error));
    assert(random_online_strategy_program(k, seed, config, second, &error));
    assert(serialize_online_strategy_program(first) ==
           serialize_online_strategy_program(second));
    OnlineStrategyProgram decoded;
    assert(deserialize_online_strategy_program(
        serialize_online_strategy_program(first), decoded, &error));
    assert(serialize_online_strategy_program(decoded) ==
           serialize_online_strategy_program(first));
    assert(validate_online_strategy_program(first, &error));
    OnlineStrategyExecutor executor;
    assert(executor.configure(first, seed, ReconfigureMode::ResetRuntime,
                              &error));

    long_sequence |= first.sequence.size() > 1;
    sequence_repeats[static_cast<unsigned>(first.repeat.mode)] = true;
    std::string order;
    for (const OnlineInvocation &step : first.sequence) {
      rule_work_budgets[static_cast<unsigned>(step.rule_work_budget.mode)] =
          true;
      touch_filter_modes[static_cast<unsigned>(step.material.touch_filter)] =
          true;
      redundant_modes[static_cast<unsigned>(
          step.material.likely_kept_redundant)] = true;
      granularities[static_cast<unsigned>(step.granularity)] = true;
      repeats[static_cast<unsigned>(step.repeat.mode)] = true;
      repetitions_by_granularity[static_cast<unsigned>(step.granularity)]
                                 [static_cast<unsigned>(step.repeat.mode)] = true;
      bounded_budget |= !step.budget.is_unlimited();
      unlimited_budget |= step.budget.is_unlimited();
      unlimited_noncomplete |=
          step.budget.is_unlimited() &&
          step.granularity != InvocationGranularity::CompleteCurrentPass;
      if (step.granularity == InvocationGranularity::CompleteCurrentPass)
        assert(!step.budget.is_unlimited() &&
               "complete random enumeration needs a finite safety ceiling");
      if (!order.empty())
        order += ',';
      order += step.rule;
    }
    orderings.insert(order);
    parameterizations.insert(serialize_online_strategy_program(first));
  }

  assert(long_sequence && bounded_budget && unlimited_budget &&
         unlimited_noncomplete);
  assert(std::all_of(std::begin(granularities), std::end(granularities),
                     [](bool seen) { return seen; }));
  assert(std::all_of(std::begin(repeats), std::end(repeats),
                     [](bool seen) { return seen; }));
  assert(std::all_of(std::begin(sequence_repeats),
                     std::end(sequence_repeats),
                     [](bool seen) { return seen; }));
  assert(std::all_of(std::begin(rule_work_budgets),
                     std::end(rule_work_budgets),
                     [](bool seen) { return seen; }));
  assert(std::all_of(std::begin(touch_filter_modes),
                     std::end(touch_filter_modes),
                     [](bool seen) { return seen; }));
  assert(std::all_of(std::begin(redundant_modes),
                     std::end(redundant_modes),
                     [](bool seen) { return seen; }));
  for (const auto &modes : repetitions_by_granularity)
    assert(std::all_of(std::begin(modes), std::end(modes),
                       [](bool seen) { return seen; }));
  assert(orderings.size() > 32);
  assert(parameterizations.size() > 1024);
}

static void generated_program_interrupts_and_reconfigures_online() {
  Kernel k = broad_fixture();
  RandomOnlineConfig config;
  config.rules = {"SubsumeForward"};
  config.maximum_sequence_length = 1;
  OnlineStrategyProgram generated;
  std::string error;
  assert(random_online_strategy_program(k, 17, config, generated, &error));
  assert(generated.sequence.size() == 1);

  OnlineStrategyExecutor executor;
  assert(executor.configure(generated, 90, ReconfigureMode::ResetRuntime,
                            &error));
  const StateVersion before = state_version(k);
  const uint64_t clause_id_before = k.next_clause_id;
  volatile sig_atomic_t stop = 1;
  const InvocationResult interrupted = executor.advance(k, &stop);
  assert(interrupted.status == InvocationStatus::Interrupted);
  assert(interrupted.applications == 0 && executor.step_consumed(
             generated.sequence.front().id) == 0);
  assert(state_version(k).arena_generation == before.arena_generation);
  assert(state_version(k).touch_epoch == before.touch_epoch);
  assert(state_version(k).strategy_epoch == before.strategy_epoch);
  assert(k.next_clause_id == clause_id_before);

  OnlineStrategyProgram replay;
  assert(deserialize_online_strategy_program(
      serialize_online_strategy_program(generated), replay, &error));
  assert(executor.configure(replay, 91,
                            ReconfigureMode::RetainMatchingStepBudgets,
                            &error));
  assert(k.next_clause_id == clause_id_before &&
         "online reconfiguration must not restart or mutate the solver");
  stop = 0;
  const InvocationResult resumed = executor.advance(k, &stop);
  assert(resumed.configuration_revision == 91);
  assert(resumed.status != InvocationStatus::Interrupted &&
         resumed.status != InvocationStatus::InvalidRequest &&
         resumed.status != InvocationStatus::StaleRequest);
  kernel_check_invariants(k);
}

static void generation_observes_interruption_without_mutating_kernel() {
  Kernel k = broad_fixture();
  RandomOnlineConfig config;
  config.rules = {"SubsumeForward"};
  OnlineStrategyProgram generated;
  std::string error;
  const StateVersion before = state_version(k);
  const uint64_t clause_id_before = k.next_clause_id;
  volatile sig_atomic_t stop = 1;
  assert(!random_online_strategy_program(k, 29, config, generated, &error,
                                         &stop));
  assert(error == "random-online generation interrupted");
  assert(state_version(k).arena_generation == before.arena_generation);
  assert(state_version(k).touch_epoch == before.touch_epoch);
  assert(state_version(k).strategy_epoch == before.strategy_epoch);
  assert(k.next_clause_id == clause_id_before);
  kernel_check_invariants(k);
}

static void configurable_parameters_round_trip() {
  OnlineStrategyProgram program;
  OnlineInvocation reduce;
  reduce.id = 60;
  reduce.rule = "Reduce";
  reduce.parameters = ReduceParameters{3, 8, 2, 5, false, true};
  reduce.granularity = InvocationGranularity::CanonicalApplication;
  reduce.repeat = {RepeatMode::Once, 1};
  reduce.work_unit = WorkUnit::Applications;
  reduce.budget = WorkLimit::bounded(1);
  program.sequence.push_back(reduce);

  OnlineInvocation walk;
  walk.id = 61;
  walk.rule = "Walk";
  WalkParameters walk_parameters;
  walk_parameters.step_budget = 17;
  walk_parameters.random_seed = 19;
  walk_parameters.even_break_base_milli = 2300;
  walk_parameters.score_curve = {{0, 1700}, {2500, 2600}, {9000, 8100}};
  walk.parameters = walk_parameters;
  walk.granularity = InvocationGranularity::CanonicalApplication;
  walk.repeat = {RepeatMode::Once, 1};
  walk.work_unit = WorkUnit::Applications;
  walk.budget = WorkLimit::bounded(1);
  program.sequence.push_back(walk);

  OnlineInvocation reorder;
  reorder.id = 62;
  reorder.rule = "Reorder";
  reorder.parameters = ReorderParameters{23, 375, 1250, 2750};
  reorder.granularity = InvocationGranularity::CanonicalApplication;
  reorder.repeat = {RepeatMode::Once, 1};
  reorder.work_unit = WorkUnit::Applications;
  reorder.budget = WorkLimit::bounded(1);
  program.sequence.push_back(reorder);

  OnlineInvocation parity;
  parity.id = 63;
  parity.rule = "Lucky";
  parity.parameters = ParityModelParameters{47, 89, 101, 2, 7, 9, 12345};
  parity.granularity = InvocationGranularity::CanonicalApplication;
  parity.repeat = {RepeatMode::Once, 1};
  parity.work_unit = WorkUnit::Applications;
  parity.budget = WorkLimit::bounded(1);
  program.sequence.push_back(parity);

  OnlineInvocation onehot = parity;
  onehot.id = 64;
  onehot.parameters =
      OnehotModelParameters{53, 79, 103, 6, 5, 11, 13, 23456};
  program.sequence.push_back(onehot);

  std::string error;
  assert(validate_online_strategy_program(program, &error));
  const std::string encoded = serialize_online_strategy_program(program);
  OnlineStrategyProgram decoded;
  assert(deserialize_online_strategy_program(encoded, decoded, &error));
  assert(serialize_online_strategy_program(decoded) == encoded);
  const auto &decoded_reduce =
      std::get<ReduceParameters>(decoded.sequence[0].parameters);
  assert(decoded_reduce.victim_numerator == 2 &&
         decoded_reduce.victim_denominator == 5 &&
         !decoded_reduce.protect_tiers && decoded_reduce.compact_collection);
  const auto &decoded_walk =
      std::get<WalkParameters>(decoded.sequence[1].parameters);
  assert(decoded_walk.even_break_base_milli == 2300 &&
         decoded_walk.score_curve == walk_parameters.score_curve);
  const auto &decoded_reorder =
      std::get<ReorderParameters>(decoded.sequence[2].parameters);
  assert(decoded_reorder.clause_decay_milli == 375 &&
         decoded_reorder.binary_weight_milli == 1250 &&
         decoded_reorder.balanced_polarity_milli == 2750);
  const auto &decoded_parity =
      std::get<ParityModelParameters>(decoded.sequence[3].parameters);
  assert(decoded_parity.maximum_variables == 47 &&
         decoded_parity.maximum_groups == 89 &&
         decoded_parity.maximum_constraints == 101 &&
         decoded_parity.minimum_group_clauses == 2 &&
         decoded_parity.minimum_equations == 7 &&
         decoded_parity.maximum_free_variables == 9 &&
         decoded_parity.candidate_budget == 12345);
  const auto &decoded_onehot =
      std::get<OnehotModelParameters>(decoded.sequence[4].parameters);
  assert(decoded_onehot.maximum_variables == 53 &&
         decoded_onehot.maximum_domains == 79 &&
         decoded_onehot.maximum_constraints == 103 &&
         decoded_onehot.maximum_equation_arity == 6 &&
         decoded_onehot.minimum_domains == 5 &&
         decoded_onehot.minimum_equations == 11 &&
         decoded_onehot.maximum_free_domains == 13 &&
         decoded_onehot.candidate_budget == 23456);

  // Trace-v3 payloads written before scoring became configurable retain the
  // reference scoring defaults.
  const std::string old =
      "sequence-repeat=once:1 "
      "step=71:Walk:3:once:1:0:1:rulework(0,0):material(0,0):"
      "walk(5,7):instances[]:grids[]";
  assert(deserialize_online_strategy_program(old, decoded, &error));
  const auto &legacy = std::get<WalkParameters>(decoded.sequence[0].parameters);
  assert(legacy.step_budget == 5 && legacy.random_seed == 7 &&
         legacy.even_break_base_milli ==
             WalkParameters::EVEN_BREAK_BASE_DEFAULT &&
         legacy.score_curve == WalkParameters::reference_score_curve());

  const std::string old_reduce =
      "sequence-repeat=once:1 "
      "step=72:Reduce:3:once:1:0:1:rulework(0,0):material(0,0):"
      "reduce(2,6):instances[]:grids[]";
  assert(deserialize_online_strategy_program(old_reduce, decoded, &error));
  const auto &legacy_reduce =
      std::get<ReduceParameters>(decoded.sequence[0].parameters);
  assert(legacy_reduce.victim_numerator == 3 &&
         legacy_reduce.victim_denominator == 4 &&
         legacy_reduce.protect_tiers &&
         !legacy_reduce.compact_collection);
}

static void random_walk_and_reorder_vary_scoring_parameters() {
  Kernel k = broad_fixture();
  ScopedHeuristics heuristics(k);
  RandomOnlineConfig config;
  config.rules = {"Walk", "Reorder"};
  std::set<uint32_t> walk_even_bases, walk_curve_bases;
  std::set<uint32_t> reorder_decay, reorder_binary, reorder_polarity;
  for (uint64_t seed = 1; seed <= 256; ++seed) {
    OnlineStrategyProgram program;
    std::string error;
    assert(random_online_strategy_program(k, seed, config, program, &error));
    for (const OnlineInvocation &step : program.sequence) {
      if (const auto *p = std::get_if<WalkParameters>(&step.parameters)) {
        assert(p->valid());
        assert(p->even_break_base_milli > 0);
        walk_even_bases.insert(p->even_break_base_milli);
        for (const WalkParameters::ScorePoint point : p->score_curve) {
          assert(point.break_base_milli > 0);
          walk_curve_bases.insert(point.break_base_milli);
        }
      } else if (const auto *p =
                     std::get_if<ReorderParameters>(&step.parameters)) {
        reorder_decay.insert(p->clause_decay_milli);
        reorder_binary.insert(p->binary_weight_milli);
        reorder_polarity.insert(p->balanced_polarity_milli);
      }
    }
  }
  assert(walk_even_bases.size() > 8 && walk_curve_bases.size() > 16);
  assert(reorder_decay.size() > 8 && reorder_binary.size() > 8 &&
         reorder_polarity.size() > 8);
}

static void random_sr_is_structured_replayable_and_effective() {
  RandomOnlineConfig config;
  config.rules = {"SubstitutionRedundancy"};
  config.maximum_sequence_length = 1;
  bool saw_explicit = false, saw_complete = false, saw_effective = false;
  std::set<uint64_t> validation_limits, detection_limits;
  for (uint64_t seed = 1; seed <= 1024; ++seed) {
    Kernel k = sr_fixture();
    OnlineStrategyProgram program;
    std::string error;
    assert(random_online_strategy_program(k, seed, config, program, &error));
    assert(program.sequence.size() == 1);
    const OnlineInvocation &step = program.sequence.front();
    const auto &parameters =
        std::get<SubstitutionRedundancyParameters>(step.parameters);
    validation_limits.insert(parameters.maximum_validation_ticks);
    detection_limits.insert(parameters.maximum_detection_ticks);
    saw_explicit |= !step.substitution_redundancy_instances.empty();
    saw_complete |=
        step.granularity == InvocationGranularity::CompleteCurrentPass;

    const std::string encoded = serialize_online_strategy_program(program);
    OnlineStrategyProgram replay;
    assert(deserialize_online_strategy_program(encoded, replay, &error));
    assert(serialize_online_strategy_program(replay) == encoded);
    assert(replay.sequence.front().substitution_redundancy_instances ==
           step.substitution_redundancy_instances);

    OnlineStrategyExecutor executor;
    assert(executor.configure(replay, seed));
    const InvocationResult result = executor.advance(k);
    if (result.made_rule_progress) {
      saw_effective = true;
      assert(k.value(internalize(-1)) > 0);
    }
  }
  assert(saw_explicit && saw_complete && saw_effective);
  assert(validation_limits.size() > 32 && detection_limits.size() > 32);
}

static OnlineStrategyProgram exact_factor_replay_program(Kernel &k) {
  kernel_reserve_vars(k, 5);
  FactorGrid grid;
  grid.factors = {internalize(1), internalize(2)};
  grid.remainders = {{internalize(3)}, {internalize(4)}, {internalize(5)}};
  for (const Lit factor : grid.factors)
    for (const auto &remainder : grid.remainders)
      kernel_add_binary_clause(k, factor, remainder.front());
  OnlineStrategyProgram program;
  OnlineInvocation step;
  step.id = 77;
  step.rule = "Factor";
  step.parameters = FactorParameters{9, 3, 4, true, false};
  step.granularity = InvocationGranularity::OneInstance;
  step.factor_grids.push_back(std::move(grid));
  step.repeat = {RepeatMode::Once, 1};
  step.work_unit = WorkUnit::Applications;
  step.budget = WorkLimit::bounded(1);
  program.sequence.push_back(std::move(step));
  return program;
}

static void serialized_configuration_round_trips_and_executes() {
  Kernel original, replay;
  OnlineStrategyProgram program = exact_factor_replay_program(original);
  (void)exact_factor_replay_program(replay);
  const std::string encoded = serialize_online_strategy_program(program);
  OnlineStrategyProgram decoded;
  std::string error;
  assert(deserialize_online_strategy_program(encoded, decoded, &error));
  assert(serialize_online_strategy_program(decoded) == encoded);
  assert(decoded.sequence.front().factor_grids.size() == 1);

  // Replays produced before the additive native-rule budget field remain
  // valid and inherit the caller's rule allowance.
  std::string legacy = encoded;
  const size_t marker = legacy.find(":rulework(");
  assert(marker != std::string::npos);
  const size_t marker_end = legacy.find(')', marker);
  assert(marker_end != std::string::npos);
  legacy.erase(marker, marker_end - marker + 1);
  const size_t material_marker = legacy.find(":material(");
  assert(material_marker != std::string::npos);
  const size_t material_end = legacy.find(')', material_marker);
  assert(material_end != std::string::npos);
  legacy.erase(material_marker, material_end - material_marker + 1);
  OnlineStrategyProgram legacy_decoded;
  assert(deserialize_online_strategy_program(legacy, legacy_decoded, &error));
  assert(legacy_decoded.sequence.front().rule_work_budget.mode ==
         RuleWorkBudgetMode::Inherit);
  assert(legacy_decoded.sequence.front().rule_work_budget.amount == 0);
  assert(legacy_decoded.sequence.front().material.touch_filter ==
         SettingOverride::Inherit);

  OnlineStrategyExecutor first, second;
  assert(first.configure(program, 31, ReconfigureMode::ResetRuntime, &error));
  assert(second.configure(decoded, 31, ReconfigureMode::ResetRuntime, &error));
  const InvocationResult a = first.advance(original);
  const InvocationResult b = second.advance(replay);
  assert(a.status == b.status && a.rule_result == b.rule_result);
  assert(a.work == b.work && a.instance == b.instance);
  assert(original.stats.factorings == 1 && replay.stats.factorings == 1);
  assert(original.nvars == replay.nvars &&
         original.next_clause_id == replay.next_clause_id);

  OnlineStrategyProgram unchanged = decoded;
  assert(!deserialize_online_strategy_program(encoded + " garbage", decoded,
                                               &error));
  assert(serialize_online_strategy_program(decoded) ==
         serialize_online_strategy_program(unchanged));
  kernel_check_invariants(original);
  kernel_check_invariants(replay);
}

static void explicit_selection_reaches_beyond_a_prefix() {
  Kernel k = broad_fixture();
  const int id = tactix_rule_id("SubsumeForward");
  assert(id >= 0);
  std::vector<uint64_t> complete;
  assert(enumerate_complete_instances(k, tactix_rules()[id], complete) ==
         InvocationStatus::Completed);
  assert(complete.size() > 16);

  RandomOnlineConfig config;
  config.rules = {"SubsumeForward"};
  bool reached_late = false;
  for (uint64_t seed = 1; seed <= 512 && !reached_late; ++seed) {
    OnlineStrategyProgram program;
    assert(random_online_strategy_program(k, seed, config, program));
    for (const OnlineInvocation &step : program.sequence)
      for (uint64_t instance : step.instances) {
        const auto found = std::find(complete.begin(), complete.end(), instance);
        assert(found != complete.end());
        reached_late |= static_cast<size_t>(found - complete.begin()) >= 16;
      }
  }
  assert(reached_late);
}

static void parameter_domains_are_caller_controlled() {
  Kernel k = broad_fixture();
  RandomOnlineConfig config;
  config.rules = {"BVE"};
  config.maximum_bve_growth_bound = 0;
  config.maximum_bve_definition_budget = 0;
  config.maximum_bve_definition_cores = 1;
  config.maximum_bve_definition_core_budget_multiplier = 0;
  config.bve_occurrence_reference = 37;
  config.bve_clause_size_reference = 41;
  for (uint64_t seed = 1; seed <= 64; ++seed) {
    OnlineStrategyProgram program;
    assert(random_online_strategy_program(k, seed, config, program));
    for (const OnlineInvocation &step : program.sequence) {
      const auto *parameters = std::get_if<BveParameters>(&step.parameters);
      assert(parameters);
      assert(parameters->growth_bound == 0);
      assert(parameters->definition_budget == 0);
      assert(parameters->definition_cores == 1);
      assert(parameters->definition_core_budget_multiplier == 0);
    }
  }

  config = {};
  config.rules = {"Congruence"};
  config.maximum_congruence_gate_count = 0;
  for (uint64_t seed = 1; seed <= 64; ++seed) {
    OnlineStrategyProgram program;
    assert(random_online_strategy_program(k, seed, config, program));
    for (const OnlineInvocation &step : program.sequence) {
      const auto *parameters =
          std::get_if<CongruenceParameters>(&step.parameters);
      assert(parameters);
      assert(parameters->gate_count_limit == 0);
    }
  }

  config = {};
  config.rules = {"Vivify"};
  config.vivify_reference.irredundant_sort_ratio = 1;
  bool saw_reuse[2] = {}, saw_flush[2] = {};
  bool saw_scope[6] = {};
  for (uint64_t seed = 1; seed <= 128; ++seed) {
    OnlineStrategyProgram program;
    assert(random_online_strategy_program(k, seed, config, program));
    for (const OnlineInvocation &step : program.sequence) {
      const auto *parameters = std::get_if<VivifyParameters>(&step.parameters);
      assert(parameters && parameters->irredundant_sort_ratio == 1);
      saw_reuse[parameters->reuse] = true;
      saw_flush[parameters->flush] = true;
      saw_scope[parameters->scope] = true;
    }
  }
  assert(saw_reuse[0] && saw_reuse[1] && saw_flush[0] && saw_flush[1]);
  assert(std::all_of(std::begin(saw_scope), std::end(saw_scope),
                     [](bool seen) { return seen; }));

  config = {};
  config.rules = {"Sweep"};
  config.maximum_sequence_length = 1;
  bool saw_general_proof[2] = {};
  std::string sweep_encoding;
  for (uint64_t seed = 1; seed <= 128; ++seed) {
    OnlineStrategyProgram program;
    assert(random_online_strategy_program(k, seed, config, program));
    const auto *parameters =
        std::get_if<SweepParameters>(&program.sequence.front().parameters);
    assert(parameters);
    saw_general_proof[parameters->force_general_proof] = true;
    sweep_encoding = serialize_online_strategy_program(program);
    OnlineStrategyProgram replay;
    std::string error;
    assert(deserialize_online_strategy_program(sweep_encoding, replay, &error));
    const auto *replayed =
        std::get_if<SweepParameters>(&replay.sequence.front().parameters);
    assert(replayed &&
           replayed->force_general_proof == parameters->force_general_proof);
  }
  assert(saw_general_proof[0] && saw_general_proof[1]);

  // Programs serialized before the additive proof-path choice remain valid.
  const size_t sweep = sweep_encoding.find(":sweep(");
  assert(sweep != std::string::npos);
  const size_t sweep_end = sweep_encoding.find(')', sweep);
  const size_t last_field = sweep_encoding.rfind(',', sweep_end);
  assert(sweep_end != std::string::npos && last_field != std::string::npos);
  sweep_encoding.erase(last_field, sweep_end - last_field);
  OnlineStrategyProgram legacy_sweep;
  std::string legacy_error;
  assert(deserialize_online_strategy_program(sweep_encoding, legacy_sweep,
                                             &legacy_error));
  const auto *legacy_parameters = std::get_if<SweepParameters>(
      &legacy_sweep.sequence.front().parameters);
  assert(legacy_parameters && !legacy_parameters->force_general_proof);

  config = {};
  config.rules = {"Lucky"};
  config.maximum_sequence_length = 1;
  config.maximum_parity_model_variables = 73;
  config.maximum_parity_model_groups = 97;
  config.maximum_parity_model_constraints = 131;
  config.maximum_parity_model_free_variables = 11;
  config.maximum_parity_model_candidates = 257;
  config.maximum_onehot_model_variables = 71;
  config.maximum_onehot_model_domains = 83;
  config.maximum_onehot_model_constraints = 127;
  config.maximum_onehot_model_equation_arity = 7;
  config.maximum_onehot_model_free_domains = 13;
  config.maximum_onehot_model_candidates = 251;
  bool selected_parity_instance = false, selected_onehot_instance = false;
  std::set<uint32_t> parity_variables, parity_groups, parity_constraints,
      parity_free;
  std::set<uint64_t> parity_candidates;
  std::set<uint32_t> onehot_variables, onehot_domains, onehot_constraints,
      onehot_arities, onehot_free;
  std::set<uint64_t> onehot_candidates;
  for (uint64_t seed = 1; seed <= 512; ++seed) {
    OnlineStrategyProgram program;
    std::string generation_error;
    if (!random_online_strategy_program(k, seed, config, program,
                                        &generation_error)) {
      std::fprintf(stderr, "Lucky generation seed %llu: %s\n",
                   (unsigned long long)seed, generation_error.c_str());
      std::fprintf(stderr, "%s\n",
                   serialize_online_strategy_program(program).c_str());
      assert(false);
    }
    const OnlineInvocation &step = program.sequence.front();
    if (const auto *parameters =
            std::get_if<ParityModelParameters>(&step.parameters)) {
      assert(parameters->valid());
      parity_variables.insert(parameters->maximum_variables);
      parity_groups.insert(parameters->maximum_groups);
      parity_constraints.insert(parameters->maximum_constraints);
      parity_free.insert(parameters->maximum_free_variables);
      parity_candidates.insert(parameters->candidate_budget);
    } else {
      const auto *onehot_parameters =
          std::get_if<OnehotModelParameters>(&step.parameters);
      assert(onehot_parameters && onehot_parameters->valid());
      onehot_variables.insert(onehot_parameters->maximum_variables);
      onehot_domains.insert(onehot_parameters->maximum_domains);
      onehot_constraints.insert(onehot_parameters->maximum_constraints);
      onehot_arities.insert(onehot_parameters->maximum_equation_arity);
      onehot_free.insert(onehot_parameters->maximum_free_domains);
      onehot_candidates.insert(onehot_parameters->candidate_budget);
    }
    selected_parity_instance |=
        std::find(step.instances.begin(), step.instances.end(), uint64_t{6}) !=
        step.instances.end();
    selected_onehot_instance |=
        std::find(step.instances.begin(), step.instances.end(), uint64_t{7}) !=
        step.instances.end();
    OnlineStrategyProgram replay;
    std::string error;
    assert(deserialize_online_strategy_program(
        serialize_online_strategy_program(program), replay, &error));
    assert(serialize_online_strategy_program(replay) ==
           serialize_online_strategy_program(program));
  }
  assert(selected_parity_instance && selected_onehot_instance);
  assert(parity_variables.size() > 8 && parity_groups.size() > 8 &&
         parity_constraints.size() > 8 && parity_free.size() > 8 &&
         parity_candidates.size() > 8);
  assert(onehot_variables.size() > 8 && onehot_domains.size() > 8 &&
         onehot_constraints.size() > 8 && onehot_arities.size() > 3 &&
         onehot_free.size() > 8 && onehot_candidates.size() > 8);

  const std::vector<std::pair<std::string, bool (*)(const ActionParameters &)>>
      typed_rules = {
          {"BCE", [](const ActionParameters &p) {
             return std::holds_alternative<BceParameters>(p);
           }},
          {"HyperTernaryRes", [](const ActionParameters &p) {
             return std::holds_alternative<TernaryParameters>(p);
           }},
          {"Factor", [](const ActionParameters &p) {
             return std::holds_alternative<FactorParameters>(p);
           }},
          {"TransitiveReduce", [](const ActionParameters &p) {
             return std::holds_alternative<TransitiveReduceParameters>(p);
           }},
          {"Substitute", [](const ActionParameters &p) {
             return std::holds_alternative<SubstituteParameters>(p);
           }},
          {"Reduce", [](const ActionParameters &p) {
             return std::holds_alternative<ReduceParameters>(p);
           }},
          {"Lucky", [](const ActionParameters &p) {
             return std::holds_alternative<ParityModelParameters>(p) ||
                    std::holds_alternative<OnehotModelParameters>(p);
           }},
      };
  for (const auto &[rule, has_expected_type] : typed_rules) {
    config = {};
    config.rules = {rule};
    std::set<std::string> serializations;
    for (uint64_t seed = 1; seed <= 128; ++seed) {
      OnlineStrategyProgram program;
      assert(random_online_strategy_program(k, seed, config, program));
      for (const OnlineInvocation &step : program.sequence)
        assert(has_expected_type(step.parameters));
      serializations.insert(serialize_online_strategy_program(program));
    }
    assert(serializations.size() > 16);
  }
}

static void sample_each_applicable_action(Kernel &k,
                                          std::set<std::string> &seen) {
  for (const RuleVTable &rule : tactix_rules()) {
    if (!rule.guard(k))
      continue;
    RandomOnlineConfig config;
    config.rules = {rule.name};
    config.maximum_sequence_length = 1;
    OnlineStrategyProgram program;
    std::string error;
    assert(random_online_strategy_program(k, rule.id + 1, config, program,
                                          &error));
    assert(program.sequence.size() == 1);
    assert(program.sequence.front().rule == rule.name);
    assert(validate_online_strategy_program(program, &error));
    seen.insert(rule.name);
  }
}

static void every_registry_action_has_random_support() {
  std::set<std::string> seen;
  {
    Kernel k = broad_fixture();
    ScopedHeuristics heuristics(k);
    sample_each_applicable_action(k, seen);
  }
  {
    Kernel k = broad_fixture();
    ScopedHeuristics heuristics(k);
    assert(decide_apply(k, internalize(5), true) == ApplyResult::Ok);
    sample_each_applicable_action(k, seen);
  }
  {
    Kernel k;
    ScopedHeuristics heuristics(k);
    kernel_reserve_vars(k, 3);
    const Lit conflict[] = {internalize(1), internalize(2), internalize(3)};
    kernel_add_large_clause(k, conflict, 3, false, 0);
    assert(decide_apply(k, internalize(-1), true) == ApplyResult::Ok);
    assert(decide_apply(k, internalize(-2), true) == ApplyResult::Ok);
    assert(decide_apply(k, internalize(-3), true) == ApplyResult::Ok);
    assert(propagate_service(k) == ApplyResult::Conflict);
    std::set<std::string> conflict_seen;
    sample_each_applicable_action(k, conflict_seen);
    assert(conflict_seen.count("Restart") && conflict_seen.count("Reduce"));
    seen.insert(conflict_seen.begin(), conflict_seen.end());
  }
  assert(seen.size() == tactix_rules().size());
}

static void random_search_choices_cover_phases_targets_and_parameters() {
  {
    Kernel k = broad_fixture();
    ScopedHeuristics heuristics(k);
    RandomOnlineConfig config;
    config.rules = {"Decide"};
    bool phase[2] = {};
    for (uint64_t seed = 1; seed <= 512; ++seed) {
      OnlineStrategyProgram program;
      assert(random_online_strategy_program(k, seed, config, program));
      for (const OnlineInvocation &step : program.sequence)
        for (const uint64_t instance : step.instances) {
          const int32_t literal =
              static_cast<int32_t>(static_cast<uint32_t>(instance));
          phase[literal < 0] = true;
        }
    }
    assert(phase[0] && phase[1]);
  }
  {
    Kernel k = broad_fixture();
    ScopedHeuristics heuristics(k);
    for (int literal : {5, 8, 9, 12})
      assert(decide_apply(k, internalize(literal), true) == ApplyResult::Ok);
    RandomOnlineConfig config;
    config.rules = {"Restart"};
    std::set<uint64_t> targets;
    for (uint64_t seed = 1; seed <= 512; ++seed) {
      OnlineStrategyProgram program;
      assert(random_online_strategy_program(k, seed, config, program));
      for (const OnlineInvocation &step : program.sequence)
        targets.insert(step.instances.begin(), step.instances.end());
    }
    assert(targets.size() >= 4);
    assert(targets.count(0) && targets.count(4));
  }
  {
    Kernel k;
    ScopedHeuristics heuristics(k);
    kernel_reserve_vars(k, 3);
    const Lit conflict[] = {internalize(1), internalize(2), internalize(3)};
    kernel_add_large_clause(k, conflict, 3, false, 0);
    assert(decide_apply(k, internalize(-1), true) == ApplyResult::Ok);
    assert(decide_apply(k, internalize(-2), true) == ApplyResult::Ok);
    assert(decide_apply(k, internalize(-3), true) == ApplyResult::Ok);
    assert(propagate_service(k) == ApplyResult::Conflict);
    RandomOnlineConfig config;
    config.rules = {"ConflictAnalysis"};
    std::set<uint32_t> depths;
    std::set<uint32_t> chronological_levels, bump_limits, bump_depths;
    std::set<uint32_t> eager_sizes;
    bool reuse[2] = {};
    bool jump[2] = {}, bigbig[2] = {}, promote[2] = {}, extension[2] = {};
    std::set<uint64_t> targets;
    for (uint64_t seed = 1; seed <= 512; ++seed) {
      OnlineStrategyProgram program;
      assert(random_online_strategy_program(k, seed, config, program));
      for (const OnlineInvocation &step : program.sequence) {
        const auto *parameters =
            std::get_if<AnalyzeParameters>(&step.parameters);
        assert(parameters);
        depths.insert(parameters->minimize_depth_limit);
        reuse[parameters->conflict_reuse] = true;
        chronological_levels.insert(parameters->chronological_levels);
        bump_limits.insert(parameters->bump_reason_limit);
        bump_depths.insert(parameters->bump_reason_depth);
        eager_sizes.insert(parameters->eager_subsume);
        jump[parameters->jump_reasons] = true;
        bigbig[parameters->jump_reasons_bigbig] = true;
        promote[parameters->shrink_glue_promote] = true;
        extension[parameters->bump_extension_reasons] = true;
        targets.insert(step.instances.begin(), step.instances.end());
      }
    }
    assert(depths.size() > 3);
    assert(reuse[0] && reuse[1]);
    assert(chronological_levels.size() > 3);
    assert(bump_limits.size() > 3 && bump_depths.size() > 3);
    assert(eager_sizes.size() ==
           AnalyzeParameters::EAGER_SUBSUME_CAPACITY + 1);
    assert(jump[0] && jump[1] && bigbig[0] && bigbig[1]);
    assert(promote[0] && promote[1] && extension[0] && extension[1]);
    assert(targets.count(0) && targets.count(2));
  }
}

static void generated_programs_execute_effective_semantic_transitions() {
  bool explicit_effective = false;
  bool complete_layered_fixpoint = false;
  for (uint64_t seed = 1;
       seed <= 8192 && !(explicit_effective && complete_layered_fixpoint);
       ++seed) {
    Kernel k = layered_congruence_fixture();
    RandomOnlineConfig config;
    config.rules = {"Congruence"};
    config.maximum_sequence_length = 1;
    config.maximum_repeat_count = 4;
    config.maximum_application_budget = 4;
    OnlineStrategyProgram program;
    assert(random_online_strategy_program(k, seed, config, program));
    const OnlineInvocation &step = program.sequence.front();
    const bool explicit_selection =
        step.granularity == InvocationGranularity::OneInstance ||
        step.granularity == InvocationGranularity::SelectedBatch;
    const bool complete_fixpoint =
        step.granularity == InvocationGranularity::CompleteCurrentPass &&
        step.repeat.mode == RepeatMode::UntilScopedFixpoint;

    OnlineStrategyExecutor executor;
    assert(executor.configure(program, seed));
    bool progress = false;
    for (unsigned advances = 0; advances != 64 && !executor.complete();
         ++advances) {
      const InvocationResult result = executor.advance(k);
      progress |= result.made_rule_progress;
      assert(result.status != InvocationStatus::InvalidRequest);
      assert(result.status != InvocationStatus::ResourceExhausted);
      assert(result.status != InvocationStatus::StaleRequest);
      if (result.status == InvocationStatus::BudgetExhausted ||
          result.status == InvocationStatus::Interrupted)
        break;
    }
    explicit_effective |= explicit_selection && progress &&
                          k.stats.congruence_merges != 0;
    complete_layered_fixpoint |=
        complete_fixpoint && k.stats.congruence_rounds == 0 &&
        k.stats.congruence_merges != 0 && k.stats.congruence_units != 0 &&
        k.value(internalize(-5)) > 0;
    kernel_check_invariants(k);
  }
  assert(explicit_effective &&
         "generated noncanonical Congruence payload must change state");
  assert(complete_layered_fixpoint &&
         "generated complete/fixpoint schedule must expose its second layer");
}

static void production_random_reaches_late_semantics_and_replays_them() {
  Kernel inventory = broad_fixture();
  const int c1 = tactix_rule_id("SubsumeForward");
  assert(c1 >= 0);
  std::vector<uint64_t> complete;
  assert(enumerate_complete_instances(inventory, tactix_rules()[c1], complete) ==
         InvocationStatus::Completed);
  assert(complete.size() > 16);
  const std::set<uint64_t> late(complete.begin() + 16, complete.end());

  const std::string path = "/tmp/tactix-random-late-" +
                           std::to_string(static_cast<long>(getpid())) +
                           ".trace";
  RandomOnlineConfig config;
  // Exercise the production controller and generator while keeping this unit
  // fixture focused on search service plus the clause rule whose late
  // noncanonical instances it observes.
  config.rules = {"SubsumeForward", "Propagate", "ConflictAnalysis",
                  "Decide", "Restart"};
  bool reached = false;
  uint64_t selected_seed = 0;
  for (uint64_t seed = 1; seed <= 32 && !reached; ++seed) {
    Kernel k = broad_fixture();
    ScopedHeuristics heuristics(k);
    RuleTrace trace;
    assert(trace.open(path.c_str(), "random", seed));
    assert(random_solve(k, seed, nullptr, &trace, &config) == 10);
    trace.close();
    std::ifstream input(path);
    assert(input);
    std::string line;
    while (std::getline(input, line)) {
      if (line.rfind("p ", 0) != 0)
        continue;
      const size_t program_begin = line.find(' ', 2);
      assert(program_begin != std::string::npos);
      OnlineStrategyProgram program;
      std::string error;
      assert(deserialize_online_strategy_program(
          line.substr(program_begin + 1), program, &error));
      for (const OnlineInvocation &step : program.sequence)
        if (step.rule == "SubsumeForward")
          for (const uint64_t payload : step.instances)
            reached |= late.count(payload) != 0;
    }
    if (reached)
      selected_seed = seed;
  }
  assert(reached &&
         "production RandomPolicy must sample beyond the old first16 cap");

  Kernel replay = broad_fixture();
  ScopedHeuristics replay_heuristics(replay);
  assert(replay_solve(replay, path.c_str(), nullptr) == 10);
  assert(selected_seed != 0);
  std::remove(path.c_str());
  kernel_check_invariants(replay);
}

int main() {
  invalid_configuration_is_rejected();
  seeds_are_replayable_and_behavior_is_wide();
  generated_program_interrupts_and_reconfigures_online();
  generation_observes_interruption_without_mutating_kernel();
  configurable_parameters_round_trip();
  random_walk_and_reorder_vary_scoring_parameters();
  random_sr_is_structured_replayable_and_effective();
  explicit_selection_reaches_beyond_a_prefix();
  parameter_domains_are_caller_controlled();
  every_registry_action_has_random_support();
  random_search_choices_cover_phases_targets_and_parameters();
  generated_programs_execute_effective_semantic_transitions();
  serialized_configuration_round_trips_and_executes();
  production_random_reaches_late_semantics_and_replays_them();
  std::puts("Random online strategy breadth and replay PASS");
}
