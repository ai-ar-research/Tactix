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

static void condition_cube_replay_round_trips() {
  OnlineStrategyProgram program;
  program.repeat = {RepeatMode::Once, 1};
  OnlineInvocation step;
  step.id = 375;
  step.rule = "Condition";
  step.granularity = InvocationGranularity::OneInstance;
  step.repeat = {RepeatMode::Once, 1};
  ConditionInstance cube;
  cube.clause = {internalize(1), internalize(2)};
  cube.witness = {internalize(1), internalize(3), internalize(4)};
  step.condition_instances.push_back(cube);
  program.sequence.push_back(step);
  std::string error;
  assert(validate_online_strategy_program(program, &error));
  const std::string wire = serialize_online_strategy_program(program);
  assert(wire.find(":condition[") != std::string::npos);
  OnlineStrategyProgram replay;
  assert(deserialize_online_strategy_program(wire, replay, &error));
  assert(serialize_online_strategy_program(replay) == wire);
  const std::string bad = wire.substr(0, wire.find(":condition[")) +
                          ":condition[3,0]";
  assert(!deserialize_online_strategy_program(bad, replay, &error));
}

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
  config.maximum_sr_graph_vertices = 0;
  assert(!random_online_strategy_program(k, 1, config, output, &error));

  config = {};
  config.maximum_sr_orbitopal_clique_ticks = 0;
  assert(!random_online_strategy_program(k, 1, config, output, &error));

  config = {};
  config.maximum_sr_orbitopal_product_states = 0;
  assert(!random_online_strategy_program(k, 1, config, output, &error));

  config = {};
  config.maximum_sr_orbitopal_clique_minimum_columns = 1;
  assert(!random_online_strategy_program(k, 1, config, output, &error));

  config = {};
  config.maximum_sr_symmetry_error_bound = 0;
  assert(!random_online_strategy_program(k, 1, config, output, &error));

  config = {};
  config.sr_joint_positive_denominator = 0;
  assert(!random_online_strategy_program(k, 1, config, output, &error));
  assert(error == "random-online joint SR probability is invalid");
  config.sr_joint_positive_denominator = 1;
  config.sr_joint_positive_numerator = 2;
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
  bool sequence_repeats[
      static_cast<unsigned>(RepeatMode::UntilSizeScaledFixpoint) + 1] = {};
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
  reduce.parameters = ReduceParameters{3, 8, 2, 5, false, true, false};
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
  ParityModelParameters parity_parameters{47, 89, 101, 2, 7, 9, 12345};
  parity_parameters.refutation_maximum_variables = 211;
  parity_parameters.refutation_maximum_groups = 223;
  parity_parameters.refutation_maximum_constraints = 227;
  parity_parameters.refutation_maximum_arity = 3;
  parity_parameters.refutation_probe_constraints = 29;
  parity_parameters.refutation_minimum_probe_repeats = 5;
  parity_parameters.refutation_maximum_proof_additions = 1234567;
  parity.parameters = parity_parameters;
  parity.granularity = InvocationGranularity::CanonicalApplication;
  parity.repeat = {RepeatMode::Once, 1};
  parity.work_unit = WorkUnit::Applications;
  parity.budget = WorkLimit::bounded(1);
  program.sequence.push_back(parity);

  OnlineInvocation onehot = parity;
  onehot.id = 64;
  OnehotModelParameters onehot_parameters{53, 79, 103, 6, 5, 11, 13, 23456};
  onehot_parameters.refutation_maximum_variables = 229;
  onehot_parameters.refutation_maximum_groups = 233;
  onehot_parameters.refutation_maximum_constraints = 239;
  onehot_parameters.refutation_maximum_arity = 3;
  onehot_parameters.refutation_maximum_proof_additions = 1234568;
  onehot.parameters = onehot_parameters;
  program.sequence.push_back(onehot);

  OnlineInvocation sr = parity;
  sr.id = 65;
  sr.rule = "SubstitutionRedundancy";
  SubstitutionRedundancyParameters sr_parameters;
  sr_parameters.maximum_validation_ticks = 1001;
  sr_parameters.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  sr_parameters.orbitopal_orientation = OrbitopalOrientation::Both;
  sr_parameters.minimum_orbitopal_rows = 3;
  sr_parameters.minimum_orbitopal_columns = 4;
  sr_parameters.maximum_orbitopal_matrices = 1002;
  sr_parameters.maximum_orbitopal_instances = 1003;
  sr_parameters.minimum_orbitopal_clique_columns = 1018;
  sr_parameters.maximum_orbitopal_clique_vertices = 1016;
  sr_parameters.maximum_orbitopal_clique_ticks = 1017;
  sr_parameters.maximum_orbitopal_product_states = 1019;
  sr_parameters.complete_unique_literal_amos = true;
  sr_parameters.orbitopal_generator_fallback = false;
  sr_parameters.maximum_orbitopal_refinement_orbits = 1020;
  sr_parameters.orbitopal_base_probe_filter = false;
  sr_parameters.orbitopal_base_probe_factor = 19;
  sr_parameters.maximum_detection_ticks = 1004;
  sr_parameters.maximum_general_candidates_per_advance = 1005;
  sr_parameters.orbitopal_column_order =
      OrbitopalColumnOrder::CooccurrenceMaximumClique;
  sr_parameters.orbitopal_completion =
      OrbitopalCompletion::IndividualizedGraphCompletion;
  sr_parameters.orbitopal_discovery_order =
      OrbitopalDiscoveryOrder::UniqueLiteralThenRefinement;
  sr_parameters.graph_symmetry.maximum_graph_vertices = 1006;
  sr_parameters.graph_symmetry.maximum_graph_edges = 1007;
  sr_parameters.graph_symmetry.maximum_generators = 1008;
  sr_parameters.graph_symmetry.backend_search_budget = 1009;
  sr_parameters.graph_symmetry.backend_support_budget = 1010;
  sr_parameters.graph_symmetry.backend_component_size_limit = 1011;
  sr_parameters.graph_symmetry.maximum_fixing_instances = 1012;
  sr_parameters.graph_symmetry.maximum_generator_instances_per_pass = 1013;
  sr_parameters.graph_symmetry.maximum_group_ticks = 1014;
  sr_parameters.graph_symmetry.seed = 1015;
  sr_parameters.graph_symmetry.error_bound = 17;
  sr_parameters.graph_symmetry.fixing_mode =
      SymmetryFixingMode::GeneratorsOnly;
  sr_parameters.graph_symmetry.generator_order =
      SymmetryTraversalOrder::Reverse;
  sr_parameters.graph_symmetry.clause_order =
      SymmetryTraversalOrder::Seeded;
  sr_parameters.graph_symmetry.literal_order =
      SymmetryTraversalOrder::Reverse;
  sr_parameters.graph_symmetry.clause_scope = SymmetryClauseScope::All;
  sr_parameters.graph_symmetry.pair_orientation =
      SymmetryPairOrientation::Seeded;
  sr_parameters.graph_symmetry.direct_generator_involutions_only = false;
  sr_parameters.graph_symmetry.binary_graph_mode =
      SymmetryBinaryGraphMode::DirectEdges;
  sr_parameters.graph_symmetry.binary_graph_density_permille = 2021;
  sr_parameters.graph_symmetry.clause_selection =
      SymmetryClauseSelection::Shortest;
  sr_parameters.graph_symmetry.chain_mode = SymmetryChainMode::FixAndCut;
  sr_parameters.graph_symmetry.base_selection =
      SymmetryBaseSelection::CommonClauseAmo;
  sr_parameters.graph_symmetry.decompose = false;
  sr_parameters.graph_symmetry.prefer_depth_first = true;
  sr_parameters.graph_symmetry.strong_certification = true;
  sr_parameters.graph_symmetry.pointwise_root_stabilizer = true;
  sr.parameters = sr_parameters;
  program.sequence.push_back(sr);

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
         !decoded_reduce.protect_tiers && decoded_reduce.compact_collection &&
         !decoded_reduce.reduction_bookkeeping);
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
         decoded_parity.candidate_budget == 12345 &&
         decoded_parity.refutation_maximum_variables == 211 &&
         decoded_parity.refutation_maximum_groups == 223 &&
         decoded_parity.refutation_maximum_constraints == 227 &&
         decoded_parity.refutation_maximum_arity == 3 &&
         decoded_parity.refutation_probe_constraints == 29 &&
         decoded_parity.refutation_minimum_probe_repeats == 5 &&
         decoded_parity.refutation_maximum_proof_additions == 1234567);
  const auto &decoded_onehot =
      std::get<OnehotModelParameters>(decoded.sequence[4].parameters);
  assert(decoded_onehot.maximum_variables == 53 &&
         decoded_onehot.maximum_domains == 79 &&
         decoded_onehot.maximum_constraints == 103 &&
         decoded_onehot.maximum_equation_arity == 6 &&
         decoded_onehot.minimum_domains == 5 &&
         decoded_onehot.minimum_equations == 11 &&
         decoded_onehot.maximum_free_domains == 13 &&
         decoded_onehot.candidate_budget == 23456 &&
         decoded_onehot.refutation_maximum_variables == 229 &&
         decoded_onehot.refutation_maximum_groups == 233 &&
         decoded_onehot.refutation_maximum_constraints == 239 &&
         decoded_onehot.refutation_maximum_arity == 3 &&
         decoded_onehot.refutation_maximum_proof_additions == 1234568);
  const auto &decoded_sr =
      std::get<SubstitutionRedundancyParameters>(
          decoded.sequence[5].parameters);
  assert(decoded_sr == sr_parameters);
  assert(encoded.find(":sr22(1001,3,2,3,4,1002,1003,1004,1005,1006,") !=
         std::string::npos);

  auto bounded_schreier = program;
  auto &bounded_parameters = std::get<SubstitutionRedundancyParameters>(
      bounded_schreier.sequence[5].parameters);
  bounded_parameters.graph_symmetry.schreier_domain_limit = 50'000;
  const std::string bounded_wire =
      serialize_online_strategy_program(bounded_schreier);
  assert(bounded_wire.find(":sr26(") != std::string::npos);
  OnlineStrategyProgram bounded_replay;
  assert(deserialize_online_strategy_program(bounded_wire, bounded_replay,
                                             &error));
  assert(std::get<SubstitutionRedundancyParameters>(
             bounded_replay.sequence[5].parameters)
             .graph_symmetry.schreier_domain_limit == 50'000);
  assert(serialize_online_strategy_program(bounded_replay) == bounded_wire);

  auto progress_feedback = program;
  std::get<SubstitutionRedundancyParameters>(
      progress_feedback.sequence[5].parameters).orbitopal_progress_feedback = true;
  const std::string progress_wire =
      serialize_online_strategy_program(progress_feedback);
  assert(progress_wire.find(":sr27(") != std::string::npos);
  OnlineStrategyProgram progress_replay;
  assert(deserialize_online_strategy_program(progress_wire, progress_replay,
                                             &error));
  assert(std::get<SubstitutionRedundancyParameters>(
             progress_replay.sequence[5].parameters)
             .orbitopal_progress_feedback);
  assert(serialize_online_strategy_program(progress_replay) == progress_wire);

  auto cut_admission = program;
  std::get<SubstitutionRedundancyParameters>(
      cut_admission.sequence[5].parameters)
      .graph_symmetry.nonforcing_cut_admission =
      SymmetryNonforcingCutAdmission::RequireCompoundProgress;
  const std::string cut_admission_wire =
      serialize_online_strategy_program(cut_admission);
  assert(cut_admission_wire.find(":sr28(") != std::string::npos);
  OnlineStrategyProgram cut_admission_replay;
  assert(deserialize_online_strategy_program(
      cut_admission_wire, cut_admission_replay, &error));
  assert(std::get<SubstitutionRedundancyParameters>(
             cut_admission_replay.sequence[5].parameters)
             .graph_symmetry.nonforcing_cut_admission ==
         SymmetryNonforcingCutAdmission::RequireCompoundProgress);
  assert(serialize_online_strategy_program(cut_admission_replay) ==
         cut_admission_wire);

  // sr29 appends the owner's chain order; older wires restore its defaults.
  for (const bool negation_first : {false, true}) {
    auto chain_order = cut_admission;
    auto &graph = std::get<SubstitutionRedundancyParameters>(
                      chain_order.sequence[5].parameters).graph_symmetry;
    graph.cut_point = negation_first ? SymmetryCutPoint::FirstOpen
                                     : SymmetryCutPoint::LargestOrbitLast;
    graph.negation_fixing_first = negation_first;
    const std::string wire = serialize_online_strategy_program(chain_order);
    assert(wire.find(":sr29(") != std::string::npos);
    OnlineStrategyProgram replay;
    assert(deserialize_online_strategy_program(wire, replay, &error));
    const auto &decoded_graph = std::get<SubstitutionRedundancyParameters>(
                                    replay.sequence[5].parameters).graph_symmetry;
    assert(decoded_graph.cut_point == graph.cut_point &&
           decoded_graph.negation_fixing_first == negation_first &&
           decoded_graph.nonforcing_cut_admission ==
               SymmetryNonforcingCutAdmission::RequireCompoundProgress);
    assert(serialize_online_strategy_program(replay) == wire);
  }
  // sr30 appends the owner's first-literal fix anchor.
  {
    auto anchored = cut_admission;
    auto &graph = std::get<SubstitutionRedundancyParameters>(
                      anchored.sequence[5].parameters).graph_symmetry;
    graph.first_literal_fix_anchor = true;
    const std::string wire = serialize_online_strategy_program(anchored);
    assert(wire.find(":sr30(") != std::string::npos);
    OnlineStrategyProgram replay;
    assert(deserialize_online_strategy_program(wire, replay, &error));
    const auto &decoded_graph = std::get<SubstitutionRedundancyParameters>(
                                    replay.sequence[5].parameters).graph_symmetry;
    assert(decoded_graph.first_literal_fix_anchor &&
           decoded_graph.cut_point == SymmetryCutPoint::FirstOpen &&
           !decoded_graph.negation_fixing_first);
    assert(serialize_online_strategy_program(replay) == wire);
  }
  assert(!std::get<SubstitutionRedundancyParameters>(
              cut_admission_replay.sequence[5].parameters)
              .graph_symmetry.first_literal_fix_anchor);
  assert(std::get<SubstitutionRedundancyParameters>(
             cut_admission_replay.sequence[5].parameters)
                 .graph_symmetry.cut_point == SymmetryCutPoint::FirstOpen &&
         !std::get<SubstitutionRedundancyParameters>(
              cut_admission_replay.sequence[5].parameters)
              .graph_symmetry.negation_fixing_first);

  // Equal-shaped versions retain their historical enum domain.
  for (const auto order : {OrbitopalColumnOrder::CooccurrenceGreedyClique,
                          OrbitopalColumnOrder::CooccurrenceBoundedGreedyClique}) {
    auto ordered = program;
    std::get<SubstitutionRedundancyParameters>(ordered.sequence[5].parameters)
        .orbitopal_column_order = order;
    const std::string wire = serialize_online_strategy_program(ordered);
    OnlineStrategyProgram restored;
    assert(deserialize_online_strategy_program(wire, restored, &error));
    assert(serialize_online_strategy_program(restored) == wire);
    std::string legacy = wire;
    const size_t tag = legacy.find(":sr22(");
    assert(tag != std::string::npos);
    const size_t close = legacy.find("):instances", tag);
    const size_t callback = legacy.rfind(',', legacy.rfind(',', legacy.rfind(',', close - 1) - 1) - 1);
    const size_t components = legacy.rfind(',', callback - 1);
    const size_t planning = legacy.rfind(',', components - 1);
    const size_t source = legacy.rfind(',', planning - 1);
    legacy.erase(source, close - source);
    legacy.replace(tag + 1, 4, "sr14");
    assert(deserialize_online_strategy_program(legacy, restored, &error) ==
           (order == OrbitopalColumnOrder::CooccurrenceGreedyClique));
    if (order == OrbitopalColumnOrder::CooccurrenceGreedyClique)
      assert(serialize_online_strategy_program(restored) == wire);
  }

  for (const auto source : {OrbitopalRowOrbitSource::DetectedGenerators,
                            OrbitopalRowOrbitSource::RefinementCells}) {
    auto selected = program;
    auto &parameters = std::get<SubstitutionRedundancyParameters>(
        selected.sequence[5].parameters);
    parameters.orbitopal_row_orbit_source = source;
    const std::string wire = serialize_online_strategy_program(selected);
    OnlineStrategyProgram restored;
    assert(deserialize_online_strategy_program(wire, restored, &error));
    assert(serialize_online_strategy_program(restored) == wire);
    std::string invalid = wire;
    const size_t close = invalid.find("):instances", invalid.find(":sr22("));
    const size_t callback = invalid.rfind(',', invalid.rfind(',', invalid.rfind(',', close - 1) - 1) - 1);
    const size_t components = invalid.rfind(',', callback - 1);
    const size_t planning = invalid.rfind(',', components - 1);
    const size_t field = invalid.rfind(',', planning - 1);
    invalid.replace(field + 1, planning - field - 1, "2");
    assert(!deserialize_online_strategy_program(invalid, restored, &error));
  }

  for (const auto scope : {OrbitopalDiscoveryOrder::RefinementOnly,
                           OrbitopalDiscoveryOrder::UniqueLiteralOnly}) {
    auto scoped = program;
    auto &parameters = std::get<SubstitutionRedundancyParameters>(
        scoped.sequence[5].parameters);
    parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
    parameters.orbitopal_discovery_order = scope;
    const std::string wire = serialize_online_strategy_program(scoped);
    OnlineStrategyProgram restored;
    assert(deserialize_online_strategy_program(wire, restored, &error));
    assert(serialize_online_strategy_program(restored) == wire);
    size_t field = wire.find(":sr22(") + std::strlen(":sr22(");
    for (unsigned column = 0; column < 41; ++column)
      field = wire.find(',', field) + 1;
    const size_t end = wire.find(',', field);
    assert(field && end != std::string::npos);
    std::string invalid = wire;
    invalid.replace(field, end - field, "4");
    assert(!deserialize_online_strategy_program(invalid, restored, &error));
  }

  for (const auto selection : {SymmetryBaseSelection::FirstAmoThenCommonClause,
                               SymmetryBaseSelection::FirstAmoThenOrderedRemainder,
                               SymmetryBaseSelection::FirstAmoThenForwardRemainder}) {
    auto first_amo = program;
    auto &parameters = std::get<SubstitutionRedundancyParameters>(
        first_amo.sequence[5].parameters);
    parameters.graph_symmetry.base_selection = selection;
    const std::string wire = serialize_online_strategy_program(first_amo);
    OnlineStrategyProgram restored;
    assert(deserialize_online_strategy_program(wire, restored, &error));
    assert(serialize_online_strategy_program(restored) == wire);
    assert(std::get<SubstitutionRedundancyParameters>(
               restored.sequence[5].parameters).graph_symmetry.base_selection ==
           selection);
    std::string invalid = wire;
    const size_t close = invalid.find("):instances", invalid.find(":sr22("));
    const size_t callback = invalid.rfind(',', invalid.rfind(',', invalid.rfind(',', close - 1) - 1) - 1);
    const size_t components = invalid.rfind(',', callback - 1);
    const size_t planning = invalid.rfind(',', components - 1);
    const size_t source = invalid.rfind(',', planning - 1);
    const size_t flag = invalid.rfind(',', source - 1);
    const size_t base = invalid.rfind(',', flag - 1);
    invalid.replace(base + 1, flag - base - 1, "5");
    assert(!deserialize_online_strategy_program(invalid, restored, &error));
    std::string old_wire = wire;
    // The four appended Boolean fields postdate these schemas.
    for (unsigned field = 0; field < 4; ++field)
      old_wire.erase(old_wire.rfind(',', old_wire.find("):instances", old_wire.find(":sr22(")) - 1), 2);
    old_wire.replace(old_wire.find(":sr22(") + 1, 4, "sr17");
    assert(deserialize_online_strategy_program(old_wire, restored, &error) ==
           (selection != SymmetryBaseSelection::FirstAmoThenForwardRemainder));
  }

  for (const auto mode : {SymmetryChainMode::ClausalFixing,
                          SymmetryChainMode::FixAndCut,
                          SymmetryChainMode::FixAndCutResift}) {
    auto selected = program;
    std::get<SubstitutionRedundancyParameters>(selected.sequence[5].parameters)
        .graph_symmetry.chain_mode = mode;
    const auto wire = serialize_online_strategy_program(selected);
    OnlineStrategyProgram restored;
    assert(deserialize_online_strategy_program(wire, restored, &error));
    assert(serialize_online_strategy_program(restored) == wire);
    auto old_wire = wire;
    // The four appended Boolean fields postdate these schemas.
    for (unsigned field = 0; field < 4; ++field)
      old_wire.erase(old_wire.rfind(',', old_wire.find("):instances", old_wire.find(":sr22(")) - 1), 2);
    old_wire.replace(old_wire.find(":sr22(") + 1, 4, "sr18");
    assert(deserialize_online_strategy_program(old_wire, restored, &error) ==
           (mode != SymmetryChainMode::FixAndCutResift));
    auto invalid = wire;
    size_t field = invalid.find(":sr22(") + std::strlen(":sr22(");
    for (unsigned i = 0; i < 45; ++i) field = invalid.find(',', field) + 1;
    invalid.replace(field, invalid.find(',', field) - field, "3");
    assert(!deserialize_online_strategy_program(invalid, restored, &error));
  }

  for (const bool factor : {false, true}) {
    auto selected = program;
    std::get<SubstitutionRedundancyParameters>(selected.sequence[5].parameters)
        .graph_symmetry.factor_support_components = factor;
    const auto wire = serialize_online_strategy_program(selected);
    OnlineStrategyProgram restored;
    assert(deserialize_online_strategy_program(wire, restored, &error));
    assert(serialize_online_strategy_program(restored) == wire);
    const auto close = wire.find("):instances", wire.find(":sr22("));
    const auto callback = wire.rfind(',', wire.rfind(',', wire.rfind(',', close - 1) - 1) - 1);
    const auto last = wire.rfind(',', callback - 1);
    auto invalid = wire;
    invalid.replace(last + 1, callback - last - 1, "2");
    assert(!deserialize_online_strategy_program(invalid, restored, &error));
    auto legacy = wire;
    legacy.erase(last, close - last);
    legacy.replace(legacy.find(":sr22(") + 1, 4, "sr19");
    assert(deserialize_online_strategy_program(legacy, restored, &error));
    assert(!std::get<SubstitutionRedundancyParameters>(restored.sequence[5].parameters)
                .graph_symmetry.factor_support_components);
  }

  for (const bool preserve : {false, true}) {
    auto selected = program;
    std::get<SubstitutionRedundancyParameters>(selected.sequence[5].parameters)
        .graph_symmetry.preserve_detector_order = preserve;
    const auto wire = serialize_online_strategy_program(selected);
    OnlineStrategyProgram restored;
    assert(deserialize_online_strategy_program(wire, restored, &error));
    assert(serialize_online_strategy_program(restored) == wire);
    const auto close = wire.find("):instances", wire.find(":sr22("));
    const auto last = wire.rfind(',', wire.rfind(',', wire.rfind(',', close - 1) - 1) - 1);
    auto invalid = wire;
    invalid.replace(last + 1, wire.find(',', last + 1) - last - 1, "2");
    assert(!deserialize_online_strategy_program(invalid, restored, &error));
    auto legacy = wire;
    legacy.erase(last, close - last);
    legacy.replace(legacy.find(":sr22(") + 1, 4, "sr20");
    assert(deserialize_online_strategy_program(legacy, restored, &error));
    assert(!std::get<SubstitutionRedundancyParameters>(restored.sequence[5].parameters)
                .graph_symmetry.preserve_detector_order);
  }

  for (unsigned mode = 0; mode < 4; ++mode) {
    auto selected = program;
    auto &g = std::get<SubstitutionRedundancyParameters>(selected.sequence[5].parameters).graph_symmetry;
    g.use_structured_generators = mode & 1;
    g.stabilize_structured_variables = mode & 2;
    const auto wire = serialize_online_strategy_program(selected);
    OnlineStrategyProgram restored;
    assert(deserialize_online_strategy_program(wire, restored, &error));
    assert(serialize_online_strategy_program(restored) == wire);
    const auto close = wire.find("):instances", wire.find(":sr22("));
    const auto scope = wire.rfind(',', close - 1);
    const auto basis = wire.rfind(',', scope - 1);
    for (const auto field : {basis, scope}) {
      auto invalid = wire;
      invalid.replace(field + 1, 1, "2");
      assert(!deserialize_online_strategy_program(invalid, restored, &error));
    }
    auto legacy = wire;
    legacy.erase(basis, close - basis);
    legacy.replace(legacy.find(":sr22(") + 1, 4, "sr21");
    assert(deserialize_online_strategy_program(legacy, restored, &error));
    const auto &old = std::get<SubstitutionRedundancyParameters>(restored.sequence[5].parameters).graph_symmetry;
    assert(!old.use_structured_generators && !old.stabilize_structured_variables);
  }

  {
    auto selected = program;
    auto &g = std::get<SubstitutionRedundancyParameters>(selected.sequence[5].parameters).graph_symmetry;
    g.use_structured_generators = true;
    g.stabilize_structured_variables = true;
    g.structured_scope = SymmetryStructuredScope::AxisAnchors;
    const auto wire = serialize_online_strategy_program(selected);
    const auto tag = wire.find(":sr23(");
    assert(tag != std::string::npos);
    OnlineStrategyProgram restored;
    assert(deserialize_online_strategy_program(wire, restored, &error));
    assert(serialize_online_strategy_program(restored) == wire);
    const auto close = wire.find("):instances", tag);
    const auto field = wire.rfind(',', close - 1);
    for (const auto value : {"2", "18446744073709551615"}) {
      auto invalid = wire;
      invalid.replace(field + 1, close - field - 1, value);
      assert(!deserialize_online_strategy_program(invalid, restored, &error));
    }
    auto legacy = wire;
    legacy.erase(field, close - field);
    legacy.replace(tag + 1, 4, "sr22");
    assert(deserialize_online_strategy_program(legacy, restored, &error));
    assert(std::get<SubstitutionRedundancyParameters>(restored.sequence[5].parameters)
               .graph_symmetry.structured_scope == SymmetryStructuredScope::WholeMatrix);
    assert(serialize_online_strategy_program(restored) == legacy);
  }

  for (const auto scope : {SymmetryStructuredScope::WholeMatrix,
                           SymmetryStructuredScope::AxisAnchors}) {
    for (const auto selection : {SymmetryBaseSelection::ShortestClause,
             SymmetryBaseSelection::FirstShortestThenForwardRemainder}) {
      auto selected = program;
      auto &g = std::get<SubstitutionRedundancyParameters>(selected.sequence[5].parameters).graph_symmetry;
      g.base_selection = selection;
      const bool initial_only = selection == SymmetryBaseSelection::FirstShortestThenForwardRemainder;
      g.structured_scope = scope;
      const auto wire = serialize_online_strategy_program(selected);
      const auto tag = wire.find(initial_only ? ":sr25(" : ":sr24(");
      assert(tag != std::string::npos);
      OnlineStrategyProgram restored;
      assert(deserialize_online_strategy_program(wire, restored, &error));
      assert(serialize_online_strategy_program(restored) == wire);
      auto legacy = wire;
      legacy.replace(tag + 1, 4, initial_only ? "sr24" : "sr23");
      assert(!deserialize_online_strategy_program(legacy, restored, &error));
      size_t field = tag + std::strlen(":sr24(");
      for (unsigned index = 0; index < 46; ++index)
        field = wire.find(',', field) + 1;
      auto invalid = wire;
      invalid.replace(field, wire.find(',', field) - field, initial_only ? "7" : "6");
      assert(!deserialize_online_strategy_program(invalid, restored, &error));
    }
  }

  // sr12 predates fix-and-cut and base selection and resumes with the legacy
  // clausal-fixing schedule.
  for (const uint64_t limit : {uint64_t{0}, uint64_t{1}, UINT64_MAX}) {
    auto selected = program;
    std::get<SubstitutionRedundancyParameters>(selected.sequence[5].parameters)
        .graph_symmetry.chain_planning_tick_limit = limit;
    const auto wire = serialize_online_strategy_program(selected);
    OnlineStrategyProgram restored;
    assert(deserialize_online_strategy_program(wire, restored, &error));
    assert(serialize_online_strategy_program(restored) == wire);
  }
  std::string old_sr16 = encoded;
  const size_t sr17 = old_sr16.find(":sr22(");
  const size_t sr17_close = old_sr16.find("):instances", sr17);
  const size_t callback = old_sr16.rfind(',', old_sr16.rfind(',', old_sr16.rfind(',', sr17_close - 1) - 1) - 1);
    const size_t components = old_sr16.rfind(',', callback - 1);
  const size_t planning = old_sr16.rfind(',', components - 1);
  old_sr16.erase(planning, sr17_close - planning);
  old_sr16.replace(sr17 + 1, 4, "sr16");
  assert(deserialize_online_strategy_program(old_sr16, decoded, &error));
  assert(std::get<SubstitutionRedundancyParameters>(decoded.sequence[5].parameters)
             .graph_symmetry.chain_planning_tick_limit == 0);
  std::string old_sr15 = old_sr16;
  const size_t sr16 = old_sr15.find(":sr16(");
  const size_t sr16_close = old_sr15.find("):instances", sr16);
  const size_t row_source = old_sr15.rfind(',', sr16_close - 1);
  old_sr15.erase(row_source, sr16_close - row_source);
  old_sr15.replace(sr16 + 1, 4, "sr15");
  assert(deserialize_online_strategy_program(old_sr15, decoded, &error));
  assert(std::get<SubstitutionRedundancyParameters>(
      decoded.sequence[5].parameters).orbitopal_row_orbit_source ==
      OrbitopalRowOrbitSource::DetectedGenerators);
  std::string old_sr13 = old_sr15;
  const size_t sr14 = old_sr13.find(":sr15(");
  const size_t sr14_close = old_sr13.find("):instances", sr14);
  const size_t root_setting = old_sr13.rfind(',', sr14_close - 1);
  assert(sr14 != std::string::npos && root_setting > sr14);
  std::string invalid_root = old_sr15;
  invalid_root.replace(root_setting + 1, sr14_close - root_setting - 1, "2");
  assert(!deserialize_online_strategy_program(invalid_root, decoded, &error));
  old_sr13.erase(root_setting, sr14_close - root_setting);
  old_sr13.replace(sr14 + 1, 4, "sr13");
  assert(deserialize_online_strategy_program(old_sr13, decoded, &error));
  assert(!std::get<SubstitutionRedundancyParameters>(
      decoded.sequence[5].parameters).graph_symmetry.pointwise_root_stabilizer);
  std::string old_sr12 = old_sr13;
  const size_t sr13 = old_sr12.find(":sr13(");
  assert(sr13 != std::string::npos);
  const size_t sr13_close = old_sr12.find("):instances", sr13);
  assert(sr13_close != std::string::npos);
  size_t fix_and_cut = sr13_close;
  for (unsigned i = 0; i < 2; ++i) {
    fix_and_cut = old_sr12.rfind(',', fix_and_cut - 1);
    assert(fix_and_cut != std::string::npos && fix_and_cut > sr13);
  }
  old_sr12.erase(fix_and_cut, sr13_close - fix_and_cut);
  old_sr12.replace(sr13 + 1, 4, "sr12");
  assert(deserialize_online_strategy_program(old_sr12, decoded, &error));
  const auto &legacy_sr12 = std::get<SubstitutionRedundancyParameters>(
      decoded.sequence[5].parameters);
  const SubstitutionRedundancyParameters sr_defaults;
  assert(legacy_sr12.graph_symmetry.chain_mode ==
             sr_defaults.graph_symmetry.chain_mode &&
         legacy_sr12.graph_symmetry.base_selection ==
             sr_defaults.graph_symmetry.base_selection);

  // sr11 predates stabilizer-clause selection and resumes with its typed
  // default.
  std::string old_sr11 = old_sr12;
  const size_t sr12 = old_sr11.find(":sr12(");
  assert(sr12 != std::string::npos);
  const size_t sr12_close = old_sr11.find("):instances", sr12);
  assert(sr12_close != std::string::npos);
  const size_t clause_selection = old_sr11.rfind(',', sr12_close - 1);
  assert(clause_selection != std::string::npos && clause_selection > sr12);
  old_sr11.erase(clause_selection, sr12_close - clause_selection);
  old_sr11.replace(sr12 + 1, 4, "sr11");
  assert(deserialize_online_strategy_program(old_sr11, decoded, &error));
  const auto &legacy_sr11 = std::get<SubstitutionRedundancyParameters>(
      decoded.sequence[5].parameters);
  assert(legacy_sr11.graph_symmetry.clause_selection ==
         sr_defaults.graph_symmetry.clause_selection);

  // sr10 predates the binary graph representation settings and resumes with
  // their historical typed defaults.
  std::string old_sr10 = old_sr11;
  const size_t sr11 = old_sr10.find(":sr11(");
  assert(sr11 != std::string::npos);
  const size_t sr11_close = old_sr10.find("):instances", sr11);
  assert(sr11_close != std::string::npos);
  size_t binary_graph = sr11_close;
  for (unsigned i = 0; i < 2; ++i) {
    binary_graph = old_sr10.rfind(',', binary_graph - 1);
    assert(binary_graph != std::string::npos && binary_graph > sr11);
  }
  old_sr10.erase(binary_graph, sr11_close - binary_graph);
  old_sr10.replace(sr11 + 1, 4, "sr10");
  assert(deserialize_online_strategy_program(old_sr10, decoded, &error));
  const auto &legacy_sr10 = std::get<SubstitutionRedundancyParameters>(
      decoded.sequence[5].parameters);
  assert(legacy_sr10.graph_symmetry.binary_graph_mode ==
             sr_defaults.graph_symmetry.binary_graph_mode &&
         legacy_sr10.graph_symmetry.binary_graph_density_permille ==
             sr_defaults.graph_symmetry.binary_graph_density_permille);

  // sr9 predates the discovery-phase order and resumes with its historical
  // refinement-first schedule.
  std::string old_sr9 = old_sr10;
  const size_t sr10 = old_sr9.find(":sr10(");
  assert(sr10 != std::string::npos);
  const size_t sr10_close = old_sr9.find("):instances", sr10);
  assert(sr10_close != std::string::npos);
  const size_t discovery = old_sr9.rfind(',', sr10_close - 1);
  assert(discovery != std::string::npos && discovery > sr10);
  old_sr9.erase(discovery, sr10_close - discovery);
  old_sr9.replace(sr10 + 1, 4, "sr9");
  assert(deserialize_online_strategy_program(old_sr9, decoded, &error));
  const auto &legacy_sr9 = std::get<SubstitutionRedundancyParameters>(
      decoded.sequence[5].parameters);
  assert(legacy_sr9.orbitopal_discovery_order ==
         OrbitopalDiscoveryOrder::RefinementThenUniqueLiteral);

  // sr8 predates the four individualized-refinement scheduling fields. Its
  // reader restores their historical typed defaults.
  std::string old_sr8 = old_sr9;
  const size_t sr9 = old_sr8.find(":sr9(");
  assert(sr9 != std::string::npos);
  const size_t sr9_close = old_sr8.find("):instances", sr9);
  assert(sr9_close != std::string::npos);
  size_t appended = sr9_close;
  for (unsigned i = 0; i < 4; ++i) {
    appended = old_sr8.rfind(',', appended - 1);
    assert(appended != std::string::npos && appended > sr9);
  }
  old_sr8.erase(appended, sr9_close - appended);
  old_sr8.replace(sr9 + 1, 3, "sr8");
  assert(deserialize_online_strategy_program(old_sr8, decoded, &error));
  const auto &legacy_sr8 = std::get<SubstitutionRedundancyParameters>(
      decoded.sequence[5].parameters);
  assert(legacy_sr8.orbitopal_generator_fallback ==
             sr_defaults.orbitopal_generator_fallback &&
         legacy_sr8.maximum_orbitopal_refinement_orbits ==
             sr_defaults.maximum_orbitopal_refinement_orbits &&
         legacy_sr8.orbitopal_base_probe_filter ==
             sr_defaults.orbitopal_base_probe_filter &&
         legacy_sr8.orbitopal_base_probe_factor ==
             sr_defaults.orbitopal_base_probe_factor);

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
         !legacy_reduce.compact_collection && legacy_reduce.reduction_bookkeeping);

  // The nine-field SR tuple remains replayable after producer settings moved
  // to the versioned payload.
  const std::string old_sr =
      "sequence-repeat=once:1 "
      "step=73:SubstitutionRedundancy:3:once:1:0:1:rulework(0,0):"
      "material(0,0):sr(91,1,3,4,92,93,94,0,95):instances[]:grids[]";
  assert(deserialize_online_strategy_program(old_sr, decoded, &error));
  const auto &legacy_sr = std::get<SubstitutionRedundancyParameters>(
      decoded.sequence[0].parameters);
  assert(legacy_sr.maximum_validation_ticks == 91 &&
         legacy_sr.producer == SubstitutionRedundancyProducer::Orbitopal &&
         legacy_sr.orbitopal_orientation == OrbitopalOrientation::Rows &&
         legacy_sr.minimum_orbitopal_rows == 3 &&
         legacy_sr.minimum_orbitopal_columns == 4 &&
         legacy_sr.maximum_orbitopal_matrices == 92 &&
         legacy_sr.maximum_orbitopal_instances == 93 &&
         legacy_sr.maximum_detection_ticks == 94 &&
         legacy_sr.maximum_general_candidates_per_advance == 95);

  // sr2 traces predate configurable graph traversal and retain canonical
  // generator, clause and literal order.
  const std::string old_sr2 =
      "sequence-repeat=once:1 "
      "step=74:SubstitutionRedundancy:3:once:1:0:1:rulework(0,0):"
      "material(0,0):sr2(101,3,2,3,4,102,103,104,105,106,107,108,109,"
      "110,111,112,113,114,115,16,2,1,0,1):instances[]:grids[]";
  assert(deserialize_online_strategy_program(old_sr2, decoded, &error));
  const auto &legacy_sr2 = std::get<SubstitutionRedundancyParameters>(
      decoded.sequence[0].parameters);
  assert(legacy_sr2.graph_symmetry.generator_order ==
             SymmetryTraversalOrder::Canonical &&
         legacy_sr2.graph_symmetry.clause_order ==
             SymmetryTraversalOrder::Canonical &&
         legacy_sr2.graph_symmetry.literal_order ==
             SymmetryTraversalOrder::Canonical);

  // sr3 traces predate structured column ordering and retain canonical,
  // unbounded orbitopal settings.
  const std::string old_sr3 =
      "sequence-repeat=once:1 "
      "step=75:SubstitutionRedundancy:3:once:1:0:1:rulework(0,0):"
      "material(0,0):sr3(101,1,0,3,4,102,103,104,105,106,107,108,109,"
      "110,111,112,113,114,115,16,2,1,0,1,2,1,0,1,2,0):"
      "instances[]:grids[]";
  assert(deserialize_online_strategy_program(old_sr3, decoded, &error));
  const auto &legacy_sr3 = std::get<SubstitutionRedundancyParameters>(
      decoded.sequence[0].parameters);
  assert(legacy_sr3.orbitopal_column_order ==
             OrbitopalColumnOrder::Canonical &&
         legacy_sr3.maximum_orbitopal_clique_vertices == UINT64_MAX &&
         legacy_sr3.maximum_orbitopal_clique_ticks == UINT64_MAX);

  // sr4 traces reordered every legal orbitopal matrix, before the
  // small-matrix admission cutoff became configurable.
  const std::string old_sr4 =
      "sequence-repeat=once:1 "
      "step=76:SubstitutionRedundancy:3:once:1:0:1:rulework(0,0):"
      "material(0,0):sr4(101,1,0,3,4,102,103,104,105,106,107,108,109,"
      "110,111,112,113,114,115,16,2,1,0,1,2,1,0,1,2,0,1,116,117):"
      "instances[]:grids[]";
  assert(deserialize_online_strategy_program(old_sr4, decoded, &error));
  const auto &legacy_sr4 = std::get<SubstitutionRedundancyParameters>(
      decoded.sequence[0].parameters);
  assert(legacy_sr4.minimum_orbitopal_clique_columns == 2);

  const std::string old_sr5 =
      "sequence-repeat=once:1 "
      "step=77:SubstitutionRedundancy:3:once:1:0:1:rulework(0,0):"
      "material(0,0):sr5(101,1,0,3,4,102,103,104,105,106,107,108,109,"
      "110,111,112,113,114,115,16,2,1,0,1,2,1,0,1,2,0,1,116,117,5):"
      "instances[]:grids[]";
  assert(deserialize_online_strategy_program(old_sr5, decoded, &error));
  const auto &legacy_sr5 = std::get<SubstitutionRedundancyParameters>(
      decoded.sequence[0].parameters);
  assert(legacy_sr5.minimum_orbitopal_clique_columns == 5 &&
         legacy_sr5.orbitopal_completion == OrbitopalCompletion::MatrixOnly);
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

static void random_clausal_fixing_retains_known_orbit_targets() {
  Kernel k = broad_fixture();
  RandomOnlineConfig config;
  config.rules = {"SubstitutionRedundancy"};
  config.maximum_sequence_length = 1;
  OnlineStrategyProgram program;
  std::string error;
  // This seed selects the clausal producer on S2 x S40. Extending its
  // stabilizer base must retain the known two-point orbit even when random
  // sifting samples only the much larger independent component.
  assert(random_online_strategy_program(k, 58, config, program, &error));
  assert(validate_online_strategy_program(program, &error));
}

static void random_sr_is_structured_replayable_and_effective() {
  RandomOnlineConfig config;
  config.rules = {"SubstitutionRedundancy"};
  config.maximum_sequence_length = 1;
  bool saw_explicit = false, saw_graph_explicit = false, saw_complete = false,
       saw_effective = false, saw_effective_resift = false;
  bool post_actions[2] = {};
  std::set<uint64_t> validation_limits, detection_limits;
  std::set<SubstitutionRedundancyProducer> producers;
  std::set<OrbitopalOrientation> orientations;
  std::set<OrbitopalColumnOrder> orbitopal_orders;
  std::set<OrbitopalCompletion> orbitopal_completions;
  std::set<OrbitopalDiscoveryOrder> orbitopal_discovery_orders;
  std::set<OrbitopalRowOrbitSource> row_orbit_sources;
  std::set<bool> planning_limits;
  bool unique_literal_amos[2] = {};
  std::set<SymmetryFixingMode> fixing_modes;
  std::set<SymmetryTraversalOrder> generator_orders, clause_orders,
      literal_orders;
  std::set<SymmetryClauseScope> clause_scopes;
  std::set<SymmetryPairOrientation> pair_orientations;
  std::set<SymmetryBinaryGraphMode> binary_graph_modes;
  std::set<SymmetryClauseSelection> clause_selections;
  std::set<SymmetryChainMode> chain_modes;
  std::set<SymmetryBaseSelection> base_selections;
  std::set<SymmetryBaseSelection> effective_fix_and_cut_bases;
  std::set<uint32_t> binary_graph_densities;
  bool involutions_only[2] = {}, root_stabilizers[2] = {};
  bool nonforcing_cut_admissions[3] = {};
  bool cut_points[2] = {}, negation_fixings[2] = {}, fix_anchors[2] = {};
  bool component_modes[2] = {}, effective_components[2] = {};
  bool detector_orders[2] = {}, effective_detector_orders[2] = {};
  bool handoffs[4] = {}, effective_handoffs[4] = {};
  bool structured_scopes[2] = {}, effective_structured_scopes[2] = {};
  std::set<uint64_t> graph_vertex_limits, generator_limits,
      orbitopal_clique_vertex_limits, orbitopal_clique_tick_limits,
      orbitopal_product_state_limits;
  std::set<uint32_t> orbitopal_clique_minimum_columns;
  std::set<uint32_t> symmetry_seeds;
  for (uint64_t seed = 1; seed <= 4096; ++seed) {
    Kernel k = sr_fixture();
    OnlineStrategyProgram program;
    std::string error;
    assert(random_online_strategy_program(k, seed, config, program, &error));
    assert(program.sequence.size() == 1);
    const OnlineInvocation &step = program.sequence.front();
    post_actions[static_cast<unsigned>(step.post_application)] = true;
    const auto &parameters =
        std::get<SubstitutionRedundancyParameters>(step.parameters);
    validation_limits.insert(parameters.maximum_validation_ticks);
    detection_limits.insert(parameters.maximum_detection_ticks);
    producers.insert(parameters.producer);
    orientations.insert(parameters.orbitopal_orientation);
    orbitopal_orders.insert(parameters.orbitopal_column_order);
    orbitopal_completions.insert(parameters.orbitopal_completion);
    orbitopal_discovery_orders.insert(parameters.orbitopal_discovery_order);
    row_orbit_sources.insert(parameters.orbitopal_row_orbit_source);
    planning_limits.insert(parameters.graph_symmetry.chain_planning_tick_limit != 0);
    unique_literal_amos[parameters.complete_unique_literal_amos] = true;
    orbitopal_clique_vertex_limits.insert(
        parameters.maximum_orbitopal_clique_vertices);
    orbitopal_clique_tick_limits.insert(
        parameters.maximum_orbitopal_clique_ticks);
    orbitopal_product_state_limits.insert(
        parameters.maximum_orbitopal_product_states);
    orbitopal_clique_minimum_columns.insert(
        parameters.minimum_orbitopal_clique_columns);
    fixing_modes.insert(parameters.graph_symmetry.fixing_mode);
    generator_orders.insert(parameters.graph_symmetry.generator_order);
    clause_orders.insert(parameters.graph_symmetry.clause_order);
    literal_orders.insert(parameters.graph_symmetry.literal_order);
    clause_scopes.insert(parameters.graph_symmetry.clause_scope);
    pair_orientations.insert(parameters.graph_symmetry.pair_orientation);
    binary_graph_modes.insert(parameters.graph_symmetry.binary_graph_mode);
    clause_selections.insert(parameters.graph_symmetry.clause_selection);
    chain_modes.insert(parameters.graph_symmetry.chain_mode);
    base_selections.insert(parameters.graph_symmetry.base_selection);
    root_stabilizers[parameters.graph_symmetry.pointwise_root_stabilizer] = true;
    nonforcing_cut_admissions[static_cast<unsigned>(
        parameters.graph_symmetry.nonforcing_cut_admission)] = true;
    cut_points[static_cast<unsigned>(parameters.graph_symmetry.cut_point)] = true;
    negation_fixings[parameters.graph_symmetry.negation_fixing_first] = true;
    fix_anchors[parameters.graph_symmetry.first_literal_fix_anchor] = true;
    component_modes[parameters.graph_symmetry.factor_support_components] = true;
    detector_orders[parameters.graph_symmetry.preserve_detector_order] = true;
    const unsigned handoff = parameters.graph_symmetry.use_structured_generators |
        (parameters.graph_symmetry.stabilize_structured_variables << 1);
    handoffs[handoff] = true;
    const unsigned structured_scope = static_cast<unsigned>(parameters.graph_symmetry.structured_scope);
    structured_scopes[structured_scope] = true;
    binary_graph_densities.insert(
        parameters.graph_symmetry.binary_graph_density_permille);
    involutions_only[parameters.graph_symmetry
                         .direct_generator_involutions_only] = true;
    graph_vertex_limits.insert(
        parameters.graph_symmetry.maximum_graph_vertices);
    generator_limits.insert(parameters.graph_symmetry.maximum_generators);
    symmetry_seeds.insert(parameters.graph_symmetry.seed);
    saw_explicit |= !step.substitution_redundancy_instances.empty();
    saw_graph_explicit |=
        parameters.producer ==
            SubstitutionRedundancyProducer::GraphSymmetry &&
        !step.substitution_redundancy_instances.empty();
    saw_complete |=
        step.granularity == InvocationGranularity::CompleteCurrentPass;

    const std::string encoded = serialize_online_strategy_program(program);
    OnlineStrategyProgram replay;
    assert(deserialize_online_strategy_program(encoded, replay, &error));
    assert(serialize_online_strategy_program(replay) == encoded);
    auto semantic_instances = step.substitution_redundancy_instances;
    for (auto &instance : semantic_instances)
      instance.orbitopal_prefix_certificate = false;
    for (auto &instance : semantic_instances)
      instance.symmetry_chain_certificate = false;
    for (auto &instance : semantic_instances)
      instance.unique_literal_amo_certificate = false;
    assert(replay.sequence.front().substitution_redundancy_instances ==
           semantic_instances);

    OnlineStrategyExecutor executor;
    assert(executor.configure(replay, seed));
    const uint64_t applications_before = k.stats.sr_applications;
    const uint64_t clause_id_before = k.next_clause_id;
    const InvocationResult result = executor.advance(k);
    if (result.made_rule_progress) {
      saw_effective = true;
      if (parameters.producer == SubstitutionRedundancyProducer::GraphSymmetry &&
          parameters.graph_symmetry.fixing_mode == SymmetryFixingMode::StabilizerOnly) {
        effective_components[parameters.graph_symmetry.factor_support_components] = true;
        effective_detector_orders[parameters.graph_symmetry.preserve_detector_order] = true;
        effective_handoffs[handoff] = true;
        if (parameters.graph_symmetry.stabilize_structured_variables)
          effective_structured_scopes[structured_scope] = true;
      }
      saw_effective_resift |=
          parameters.producer == SubstitutionRedundancyProducer::GraphSymmetry &&
          parameters.graph_symmetry.chain_mode == SymmetryChainMode::FixAndCutResift &&
          parameters.graph_symmetry.fixing_mode == SymmetryFixingMode::StabilizerOnly;
      assert(k.stats.sr_applications > applications_before);
      assert(k.next_clause_id > clause_id_before || k.inconsistent);
      if (parameters.producer == SubstitutionRedundancyProducer::GraphSymmetry &&
          parameters.graph_symmetry.chain_mode == SymmetryChainMode::FixAndCut &&
          parameters.graph_symmetry.fixing_mode == SymmetryFixingMode::StabilizerOnly)
        effective_fix_and_cut_bases.insert(parameters.graph_symmetry.base_selection);
    }
    kernel_check_invariants(k);
  }
  assert(producers.size() == 4 && orientations.size() == 3 &&
         orbitopal_orders.size() == 4 &&
         orbitopal_completions.size() == 2 &&
         orbitopal_discovery_orders.size() == 4 && row_orbit_sources.size() == 2 && planning_limits.size() == 2 &&
         unique_literal_amos[0] && unique_literal_amos[1] &&
         fixing_modes.size() == 3 && generator_orders.size() == 3 &&
         clause_orders.size() == 3 && literal_orders.size() == 3 &&
         clause_scopes.size() == 2 && pair_orientations.size() == 3 &&
         binary_graph_modes.size() == 3 && clause_selections.size() == 3 &&
         chain_modes.size() == 3 && base_selections.size() == 7 &&
         binary_graph_densities.size() > 32 &&
         involutions_only[0] && involutions_only[1] &&
         root_stabilizers[0] && root_stabilizers[1]);
  assert(nonforcing_cut_admissions[0] && nonforcing_cut_admissions[1] &&
         nonforcing_cut_admissions[2]);
  assert(cut_points[0] && cut_points[1] && negation_fixings[0] &&
         negation_fixings[1] && fix_anchors[0] && fix_anchors[1]);
  assert(component_modes[0] && component_modes[1]);
  assert(effective_components[0] && effective_components[1]);
  for (unsigned mode = 0; mode < 4; ++mode)
    assert(handoffs[mode] && effective_handoffs[mode]);
  assert(structured_scopes[0] && structured_scopes[1]);
  assert(effective_structured_scopes[0] && effective_structured_scopes[1]);
  assert(detector_orders[0] && detector_orders[1]);
  assert(effective_detector_orders[0] && effective_detector_orders[1]);
  assert(effective_fix_and_cut_bases.count(
      SymmetryBaseSelection::FirstAmoThenCommonClause));
  assert(effective_fix_and_cut_bases.count(
      SymmetryBaseSelection::FirstAmoThenOrderedRemainder));
  assert(effective_fix_and_cut_bases.count(
      SymmetryBaseSelection::FirstAmoThenForwardRemainder));
  assert(effective_fix_and_cut_bases.count(SymmetryBaseSelection::ShortestClause));
  assert(effective_fix_and_cut_bases.count(SymmetryBaseSelection::FirstShortestThenForwardRemainder));
  assert(graph_vertex_limits.size() > 8 && generator_limits.size() > 8 &&
         orbitopal_clique_vertex_limits.size() > 8 &&
         orbitopal_clique_tick_limits.size() > 8 &&
         orbitopal_product_state_limits.size() > 8 &&
         orbitopal_clique_minimum_columns.size() > 4 &&
         symmetry_seeds.size() > 32);
  assert(saw_explicit && saw_graph_explicit && saw_complete && saw_effective);
  assert(saw_effective_resift);
  assert(post_actions[static_cast<unsigned>(PostApplication::None)] &&
         post_actions[static_cast<unsigned>(PostApplication::Propagate)]);
  assert(validation_limits.size() > 32 && detection_limits.size() > 32);
}

static void joint_sr_limits_are_configurable_and_replayable() {
  RandomOnlineConfig boundary;
  boundary.rules = {"SubstitutionRedundancy"};
  boundary.maximum_sequence_length = 1;
  boundary.sr_joint_positive_numerator = 0;
  RandomOnlineConfig equivalent = boundary;
  equivalent.sr_joint_positive_denominator = UINT32_MAX;
  RandomOnlineConfig positive = boundary;
  positive.sr_joint_positive_numerator = 1;
  positive.sr_joint_positive_denominator = 1;
  bool saw_disabled_admission = false;
  for (uint64_t seed = 1; seed <= 128; ++seed) {
    Kernel k = sr_fixture();
    OnlineStrategyProgram old, same, active;
    std::string error;
    assert(random_online_strategy_program(k, seed, boundary, old, &error));
    assert(random_online_strategy_program(k, seed, equivalent, same, &error));
    assert(serialize_online_strategy_program(old) ==
           serialize_online_strategy_program(same));
    assert(random_online_strategy_program(k, seed, positive, active, &error));
    const auto &before = std::get<SubstitutionRedundancyParameters>(
        old.sequence.front().parameters);
    const auto &after = std::get<SubstitutionRedundancyParameters>(
        active.sequence.front().parameters);
    const auto &graph = after.graph_symmetry;
    saw_disabled_admission |= !before.maximum_detection_ticks ||
        !before.graph_symmetry.maximum_graph_vertices ||
        !before.graph_symmetry.maximum_graph_edges ||
        !before.graph_symmetry.maximum_generators;
    assert(after.maximum_validation_ticks > 0 &&
           after.maximum_validation_ticks <= positive.maximum_sr_validation_ticks);
    assert(after.maximum_detection_ticks > 0 &&
           after.maximum_detection_ticks <= positive.maximum_sr_detection_ticks);
    assert(graph.maximum_graph_vertices > 0 &&
           graph.maximum_graph_vertices <= positive.maximum_sr_graph_vertices);
    assert(graph.maximum_graph_edges > 0 &&
           graph.maximum_graph_edges <= positive.maximum_sr_graph_edges);
    assert(graph.maximum_generators > 0 &&
           graph.maximum_generators <= positive.maximum_sr_symmetry_generators);
    assert(graph.maximum_group_ticks > 0 &&
           graph.maximum_group_ticks <= positive.maximum_sr_symmetry_group_ticks);
    assert(graph.maximum_fixing_instances > 0 &&
           graph.maximum_fixing_instances <= positive.maximum_sr_symmetry_fixing_instances);
    // The resource mixture does not force semantic choices or move earlier
    // parameter draws. Changed materialized instances can move later draws.
    assert(before.producer == after.producer);
    assert(before.graph_symmetry.seed == graph.seed);
    assert(before.graph_symmetry.base_selection == graph.base_selection);
    assert(before.graph_symmetry.chain_mode == graph.chain_mode);
    assert(before.graph_symmetry.fixing_mode == graph.fixing_mode);
    const std::string encoded = serialize_online_strategy_program(active);
    OnlineStrategyProgram replay;
    assert(deserialize_online_strategy_program(encoded, replay, &error));
    assert(serialize_online_strategy_program(replay) == encoded);
    kernel_check_invariants(k);
  }
  assert(saw_disabled_admission);
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
  config.bve_resolvent_count_reference = 43;
  bool saw_bve_scope[3] = {};
  bool saw_bve_traversal[2] = {};
  for (uint64_t seed = 1; seed <= 64; ++seed) {
    OnlineStrategyProgram program;
    assert(random_online_strategy_program(k, seed, config, program));
    for (size_t i = 0; i < program.sequence.size(); ++i) {
      const auto *parameters =
          std::get_if<BveParameters>(&program.sequence[i].parameters);
      assert(parameters);
      assert(parameters->growth_bound == 0);
      assert(parameters->definition_budget == 0);
      assert(parameters->definition_cores == 1);
      assert(parameters->definition_core_budget_multiplier == 0);
      saw_bve_scope[static_cast<unsigned>(parameters->candidate_scope)] = true;
      saw_bve_traversal[static_cast<unsigned>(parameters->traversal)] = true;
      const std::string encoded = serialize_online_strategy_program(program);
      assert(parameters->resolvent_count_limit == 0 ||
             parameters->resolvent_count_limit == 1 ||
             parameters->resolvent_count_limit == 43 ||
             parameters->resolvent_count_limit == 44 ||
             parameters->resolvent_count_limit == UINT32_MAX);
      assert(encoded.find(":bve4(") != std::string::npos);
      OnlineStrategyProgram replay;
      std::string error;
      assert(deserialize_online_strategy_program(encoded, replay, &error));
      assert(std::get<BveParameters>(replay.sequence[i].parameters)
                 .candidate_scope == parameters->candidate_scope);
      assert(std::get<BveParameters>(replay.sequence[i].parameters)
                 .resolvent_count_limit == parameters->resolvent_count_limit);
      assert(std::get<BveParameters>(replay.sequence[i].parameters)
                 .traversal == parameters->traversal);
    }
  }
  assert(saw_bve_scope[0] && saw_bve_scope[1] && saw_bve_scope[2]);
  assert(saw_bve_traversal[0] && saw_bve_traversal[1]);

  OnlineStrategyProgram legacy_bve;
  std::string legacy_bve_error;
  assert(deserialize_online_strategy_program(
      "sequence-repeat=once:1 "
      "step=79:BVE:3:once:1:1:1:post(0):rulework(0,0):material(0,0):"
      "bve(0,1,2,3,2,4,5,6,1,7,0):instances[]:grids[]",
      legacy_bve, &legacy_bve_error));
  assert(std::get<BveParameters>(legacy_bve.sequence.front().parameters)
             .candidate_scope == BveCandidateScope::All);

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
  config.affine_model_reference.maximum_variables=79;
  config.affine_model_reference.maximum_constraints=137;
  config.affine_model_reference.maximum_arity=9;
  config.affine_model_reference.maximum_projection=17;
  config.affine_model_reference.maximum_work=263;
  config.maximum_affine_projection_coordinates=3;
  bool selected_affine_instance=false;
  std::set<uint64_t> affine_variables,affine_work;
  bool selected_parity_instance = false, selected_onehot_instance = false;
  bool selected_balanced_instance = false, selected_fermat_instance = false;
  bool selected_lookup_instance = false, selected_simon_instance = false;
  bool selected_stconn_instance = false;
  bool selected_signed_cardinality_instance = false;
  bool selected_exact_cover_instance = false;
  bool selected_independent_set_instance = false;
  std::set<uint32_t> independent_set_vertices;
  std::set<uint64_t> independent_set_steps;
  std::set<uint32_t> exact_cover_arities;
  std::set<uint64_t> exact_cover_nodes, exact_cover_restarts;
  std::set<uint32_t> parity_variables, parity_groups, parity_constraints,
      parity_free;
  std::set<uint64_t> parity_candidates;
  std::set<uint32_t> onehot_variables, onehot_domains, onehot_constraints,
      onehot_arities, onehot_free;
  std::set<uint64_t> onehot_candidates;
  std::set<uint32_t> balanced_balls, balanced_widths, balanced_samples,
      balanced_completion;
  std::set<uint64_t> balanced_restarts, balanced_steps, balanced_seeds;
  std::set<uint32_t> fermat_bits, fermat_tail;
  std::set<uint64_t> fermat_constants, fermat_iterations;
  std::set<uint32_t> lookup_fanin, lookup_fanout, lookup_inputs,
      lookup_support;
  std::set<uint64_t> lookup_evaluations;
  std::set<uint32_t> simon_rounds;
  std::set<uint64_t> simon_candidates;
  std::set<uint32_t> stconn_widths, stconn_columns;
  std::set<uint64_t> stconn_rup_clauses;
  std::set<uint32_t> signed_cardinality_variables,
      signed_cardinality_groups, signed_cardinality_constraints,
      signed_cardinality_arities;
  std::set<uint64_t> signed_cardinality_nodes,
      signed_cardinality_additions;
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
    } else if(const auto *parameters=std::get_if<AffineModelParameters>(&step.parameters)) {
      assert(parameters->valid());
      assert(parameters->maximum_variables<=79&&parameters->maximum_constraints<=137);
      assert(parameters->maximum_arity<=9&&parameters->maximum_projection<=17);
      assert(parameters->maximum_work<=263&&parameters->projection.size()<=3);
      affine_variables.insert(parameters->maximum_variables);
      affine_work.insert(parameters->maximum_work);
    } else if (const auto *parameters =
                   std::get_if<BalancedSignatureModelParameters>(
                       &step.parameters)) {
      assert(parameters->valid());
      assert(parameters->maximum_balls <=
             config.balanced_signature_model_reference.maximum_balls);
      assert(parameters->maximum_width <=
             config.balanced_signature_model_reference.maximum_width);
      assert(parameters->restarts <=
             config.balanced_signature_model_reference.restarts);
      assert(parameters->steps_per_restart <=
             config.balanced_signature_model_reference.steps_per_restart);
      assert(parameters->samples_per_step <=
             config.balanced_signature_model_reference.samples_per_step);
      assert(parameters->completion_rounds <=
             config.balanced_signature_model_reference.completion_rounds);
      balanced_balls.insert(parameters->maximum_balls);
      balanced_widths.insert(parameters->maximum_width);
      balanced_restarts.insert(parameters->restarts);
      balanced_steps.insert(parameters->steps_per_restart);
      balanced_samples.insert(parameters->samples_per_step);
      balanced_seeds.insert(parameters->seed);
      balanced_completion.insert(parameters->completion_rounds);
    } else if (const auto *parameters =
                   std::get_if<FermatModelParameters>(&step.parameters)) {
      assert(parameters->valid());
      fermat_bits.insert(parameters->maximum_bits);
      fermat_tail.insert(parameters->comparator_tail_clauses);
      fermat_constants.insert(parameters->rho_constants);
      fermat_iterations.insert(parameters->rho_iterations);
    } else if (const auto *parameters =
                   std::get_if<LookupCircuitModelParameters>(
                       &step.parameters)) {
      assert(parameters->valid());
      lookup_fanin.insert(parameters->maximum_fanin);
      lookup_fanout.insert(parameters->maximum_fanout);
      lookup_inputs.insert(parameters->maximum_primary_inputs);
      lookup_support.insert(parameters->maximum_support);
      lookup_evaluations.insert(parameters->maximum_vector_gate_evaluations);
    } else if (const auto *parameters =
                   std::get_if<SimonModelParameters>(&step.parameters)) {
      assert(parameters->valid());
      simon_rounds.insert(parameters->maximum_rounds);
      simon_candidates.insert(parameters->maximum_candidates);
    } else if (const auto *parameters =
                   std::get_if<StconnProofParameters>(&step.parameters)) {
      assert(parameters->valid());
      stconn_widths.insert(parameters->maximum_width);
      stconn_columns.insert(parameters->maximum_columns);
      stconn_rup_clauses.insert(parameters->maximum_rup_clauses);
    } else if (const auto *parameters =
                   std::get_if<SignedCardinalityProofParameters>(
                       &step.parameters)) {
      assert(parameters->valid());
      signed_cardinality_variables.insert(parameters->maximum_variables);
      signed_cardinality_groups.insert(parameters->maximum_groups);
      signed_cardinality_constraints.insert(parameters->maximum_constraints);
      signed_cardinality_arities.insert(parameters->maximum_group_arity);
      signed_cardinality_nodes.insert(parameters->maximum_bdd_nodes);
      signed_cardinality_additions.insert(
          parameters->maximum_proof_additions);
    } else if (const auto *parameters =
                   std::get_if<ExactCoverModelParameters>(&step.parameters)) {
      assert(parameters->valid());
      exact_cover_arities.insert(parameters->block_arity);
      exact_cover_nodes.insert(parameters->maximum_nodes);
      exact_cover_restarts.insert(parameters->restarts);
    } else if (const auto *parameters = std::get_if<IndependentSetModelParameters>(
                   &step.parameters)) {
      assert(parameters->valid());
      independent_set_vertices.insert(parameters->maximum_vertices);
      independent_set_steps.insert(parameters->maximum_steps);
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
    selected_affine_instance |=
        std::find(step.instances.begin(),step.instances.end(),uint64_t{8})!=step.instances.end();
    selected_parity_instance |=
        std::find(step.instances.begin(), step.instances.end(), uint64_t{6}) !=
        step.instances.end();
    selected_onehot_instance |=
        std::find(step.instances.begin(), step.instances.end(), uint64_t{7}) !=
        step.instances.end();
    selected_balanced_instance |=
        std::find(step.instances.begin(), step.instances.end(), uint64_t{10}) !=
        step.instances.end();
    selected_fermat_instance |=
        std::find(step.instances.begin(), step.instances.end(), uint64_t{11}) !=
        step.instances.end();
    selected_lookup_instance |=
        std::find(step.instances.begin(), step.instances.end(), uint64_t{13}) !=
        step.instances.end();
    selected_simon_instance |=
        std::find(step.instances.begin(), step.instances.end(), uint64_t{14}) !=
        step.instances.end();
    selected_stconn_instance |=
        std::find(step.instances.begin(), step.instances.end(), uint64_t{15}) !=
        step.instances.end();
    selected_signed_cardinality_instance |=
        std::find(step.instances.begin(), step.instances.end(), uint64_t{16}) !=
        step.instances.end();
    selected_exact_cover_instance |=
        std::find(step.instances.begin(), step.instances.end(), uint64_t{17}) !=
        step.instances.end();
    selected_independent_set_instance |=
        std::find(step.instances.begin(), step.instances.end(), uint64_t{18}) !=
        step.instances.end();
    OnlineStrategyProgram replay;
    std::string error;
    assert(deserialize_online_strategy_program(
        serialize_online_strategy_program(program), replay, &error));
    assert(serialize_online_strategy_program(replay) ==
           serialize_online_strategy_program(program));
  }
  assert(selected_affine_instance&&affine_variables.size()>8&&affine_work.size()>8);
  assert(selected_parity_instance && selected_onehot_instance);
  assert(selected_balanced_instance && balanced_balls.size() > 8 &&
         balanced_widths.size() > 4 && balanced_restarts.size() > 8 &&
         balanced_steps.size() > 8 && balanced_samples.size() > 8 &&
         balanced_seeds.size() > 8 && balanced_completion.size() > 8);
  assert(selected_fermat_instance && fermat_bits.size() > 8 &&
         fermat_tail.size() > 8 && fermat_constants.size() > 8 &&
         fermat_iterations.size() > 8);
  assert(selected_lookup_instance && lookup_fanin.size() > 4 &&
         lookup_fanout.size() > 4 && lookup_inputs.size() > 8 &&
         lookup_support.size() > 8 && lookup_evaluations.size() > 8);
  assert(selected_simon_instance && simon_rounds.size() > 8 &&
         simon_candidates.size() > 8);
  assert(selected_stconn_instance && stconn_widths.size() > 4 &&
         stconn_columns.size() > 8 && stconn_rup_clauses.size() > 8);
  assert(selected_signed_cardinality_instance &&
         signed_cardinality_variables.size() > 8 &&
         signed_cardinality_groups.size() > 8 &&
         signed_cardinality_constraints.size() > 8 &&
         signed_cardinality_arities.size() > 4 &&
         signed_cardinality_nodes.size() > 8 &&
         signed_cardinality_additions.size() > 8);
  assert(selected_exact_cover_instance && exact_cover_arities.size() > 4 &&
         exact_cover_nodes.size() > 8 && exact_cover_restarts.size() > 2);
  assert(selected_independent_set_instance &&
         independent_set_vertices.size() > 8 && independent_set_steps.size() > 8);
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
                    std::holds_alternative<OnehotModelParameters>(p) ||
                    std::holds_alternative<
                        BalancedSignatureModelParameters>(p) ||
                    std::holds_alternative<FermatModelParameters>(p) ||
                    std::holds_alternative<LookupCircuitModelParameters>(p) ||
                    std::holds_alternative<SimonModelParameters>(p) ||
                    std::holds_alternative<StconnProofParameters>(p) ||
                    std::holds_alternative<
                        SignedCardinalityProofParameters>(p) ||
                    std::holds_alternative<ExactCoverModelParameters>(p) ||
                    std::holds_alternative<IndependentSetModelParameters>(p) ||
                    std::holds_alternative<AffineModelParameters>(p);
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
  condition_cube_replay_round_trips();
  random_clausal_fixing_retains_known_orbit_targets();
  invalid_configuration_is_rejected();
  seeds_are_replayable_and_behavior_is_wide();
  generated_program_interrupts_and_reconfigures_online();
  generation_observes_interruption_without_mutating_kernel();
  configurable_parameters_round_trip();
  random_walk_and_reorder_vary_scoring_parameters();
  random_sr_is_structured_replayable_and_effective();
  joint_sr_limits_are_configurable_and_replayable();
  explicit_selection_reaches_beyond_a_prefix();
  parameter_domains_are_caller_controlled();
  every_registry_action_has_random_support();
  random_search_choices_cover_phases_targets_and_parameters();
  generated_programs_execute_effective_semantic_transitions();
  serialized_configuration_round_trips_and_executes();
  production_random_reaches_late_semantics_and_replays_them();
  std::puts("Random online strategy breadth and replay PASS");
}
