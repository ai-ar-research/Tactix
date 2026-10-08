#include "policies/default_strategy_config.hpp"

#include <algorithm>
#include <cassert>

namespace tactix {

std::vector<strategy::OnlineStrategyStage>
DefaultStrategyConfig::structured_root_preprocessing_stages() {
  using namespace strategy;
  // Retain the transliterated reference profile as an explicit control. The
  // stronger composition changes scheduling, not any producer's legality.
  const auto reference = reference_structured_root_program();
  const auto find = [&](const char *rule) -> OnlineInvocation {
    const auto at = std::find_if(reference.sequence.begin(), reference.sequence.end(),
        [&](const OnlineInvocation &step) { return step.rule == rule; });
    assert(at != reference.sequence.end());
    return *at;
  };
  auto pure = find("BVE");
  pure.id = 94;
  // Removing a pure pivot can expose another. The reference step's PureClosure
  // scope selects that whole finite closure in one pass, from one occurrence
  // image, before another full symmetry-discovery scan is paid for. Each
  // successful application retires a variable; the shared budget and stop
  // checks still bound the work.
  auto product = find("SubstitutionRedundancy");
  product.id = 82;
  product.repeat = {RepeatMode::Once, 1};
  const auto sr = [](OnlineInvocation &step) -> SubstitutionRedundancyParameters & {
    return std::get<SubstitutionRedundancyParameters>(step.parameters);
  };
  sr(product).orbitopal_discovery_order =
      OrbitopalDiscoveryOrder::RefinementThenUniqueLiteral;
  auto rows = product;
  rows.id = 86;
  sr(rows).orbitopal_orientation = OrbitopalOrientation::Rows;
  auto amo = product;
  amo.id = 87;
  sr(amo).maximum_orbitopal_matrices = 0;

  OnlineInvocation compact;
  compact.id = 85;
  compact.rule = "Reduce";
  ReduceParameters compact_parameters;
  compact_parameters.victim_numerator = 0;
  compact_parameters.compact_collection = true;
  compact_parameters.reduction_bookkeeping = false;
  compact.parameters = compact_parameters;
  compact.granularity = InvocationGranularity::CanonicalApplication;
  compact.post_application = PostApplication::Propagate;
  const auto compaction = [&](StepId id) {
    auto step = compact;
    step.id = id;
    return step;
  };

  auto initial_product = product;
  initial_product.id = 97;
  sr(initial_product).orbitopal_generator_fallback = true;
  sr(initial_product).orbitopal_discovery_order =
      OrbitopalDiscoveryOrder::RefinementOnly;
  auto initial_rows = rows;
  initial_rows.id = 90;
  sr(initial_rows).orbitopal_discovery_order =
      OrbitopalDiscoveryOrder::RefinementOnly;
  auto ulc = rows;
  ulc.id = 92;
  sr(ulc).orbitopal_discovery_order = OrbitopalDiscoveryOrder::UniqueLiteralOnly;
  sr(ulc).orbitopal_completion = OrbitopalCompletion::MatrixOnly;
  sr(ulc).orbitopal_column_order =
      OrbitopalColumnOrder::CooccurrenceBoundedGreedyClique;
  sr(ulc).maximum_detection_ticks = SYMMETRY_TOTAL_TICK_LIMIT_DEFAULT;

  OnlineInvocation graph;
  graph.id = 84;
  graph.rule = "SubstitutionRedundancy";
  graph.parameters = product.parameters;
  graph.granularity = InvocationGranularity::CompleteCurrentPass;
  auto &generic = sr(graph);
  generic.orbitopal_orientation = OrbitopalOrientation::Rows;
  generic.orbitopal_discovery_order = OrbitopalDiscoveryOrder::RefinementOnly;
  generic.producer = SubstitutionRedundancyProducer::GraphSymmetry;
  auto &group = generic.graph_symmetry;
  group.clause_scope = SymmetryClauseScope::All;
  group.clause_selection = SymmetryClauseSelection::AmoThenShortest;
  group.maximum_graph_vertices = 100'000;
  group.maximum_graph_edges = 1'000'000;
  group.maximum_generators = 128;
  group.backend_search_budget = 64;
  group.backend_support_budget = 64ull << 20;
  group.backend_component_size_limit = 100'000;
  group.maximum_fixing_instances = UINT64_MAX;
  group.maximum_generator_instances_per_pass = 1;
  group.maximum_group_ticks = 20'000'000'000ull;
  group.schreier_domain_limit = 50'000;
  group.chain_mode = SymmetryChainMode::FixAndCut;
  group.factor_support_components = true;
  group.preserve_detector_order = true;
  group.use_structured_generators = true;
  group.stabilize_structured_variables = true;
  group.structured_scope = SymmetryStructuredScope::WholeMatrix;
  group.base_selection = SymmetryBaseSelection::FirstAmoThenForwardRemainder;
  // The owner's cut point (symmetries.h find_non_trivial_orbit): the largest
  // stabilizer orbit's highest variable.
  group.cut_point = SymmetryCutPoint::LargestOrbitLast;
  group.negation_fixing_first = true;
  // Root-forcing chains earn their complete atomic prefix. A kind-2-only batch
  // merely perturbs the initial search trajectory, so the startup policy waits
  // for a forcing member while explicit strategies retain full FixAndCut.
  group.nonforcing_cut_admission =
      SymmetryNonforcingCutAdmission::RequireCompoundProgress;

  sr(rows).orbitopal_discovery_order = OrbitopalDiscoveryOrder::RefinementOnly;
  sr(product).orbitopal_discovery_order = OrbitopalDiscoveryOrder::RefinementOnly;
  std::array<OnlineStrategyProgram, 3> programs;
  programs[0].sequence = {pure, amo, initial_product, compaction(98), ulc,
                          compaction(93), initial_rows, compaction(91)};
  programs[1].sequence = {pure, amo, graph, compact, rows, compaction(89),
                          product, compaction(88)};
  for (auto &program : programs) {
    // The owner's iteration scale (satsuma.h iteration_max): a large formula
    // earns few repetitions, a shrinking one earns more.
    program.repeat = {RepeatMode::UntilSizeScaledFixpoint, 128};
    for (auto &step : program.sequence) {
      auto *parameters = std::get_if<SubstitutionRedundancyParameters>(&step.parameters);
      if (!parameters)
        continue;
      parameters->orbitopal_row_orbit_source = OrbitopalRowOrbitSource::RefinementCells;
      auto &settings = parameters->graph_symmetry;
      settings.maximum_generators = UINT64_MAX;
      settings.chain_planning_tick_limit = 20'000'000'000ull;
      settings.maximum_group_ticks = UINT64_MAX;
      settings.maximum_graph_vertices = 5'000'000;
      settings.maximum_graph_edges = UINT64_MAX;
      settings.backend_component_size_limit = 5'000'000;
      settings.pointwise_root_stabilizer = true;
    }
  }
  auto remaining = programs[1].sequence[2];
  remaining.id = 95;
  auto &remaining_group = sr(remaining).graph_symmetry;
  remaining_group.use_structured_generators = false;
  remaining_group.stabilize_structured_variables = false;
  programs[2].sequence = {remaining, compaction(96)};
  // Run the terminal affine-model opportunity inside the same structured-root
  // lifecycle as the accepted schedule.  It must precede any eliminating or
  // symmetry stage that can erase the complete encoding.  DefaultPolicy poses
  // the configured parameters (or removes this stage when disabled) before
  // execution; the public Lucky-8 action remains the sole implementation.
  OnlineInvocation affine;
  affine.id = 109;
  affine.rule = "Lucky";
  affine.parameters = AffineModelParameters{};
  affine.granularity = InvocationGranularity::OneInstance;
  affine.instances = {8};
  affine.budget = WorkLimit::bounded(1);
  OnlineStrategyProgram model_program;
  model_program.sequence = {std::move(affine)};
  std::vector<OnlineStrategyStage> stages;
  stages.push_back(
      {std::move(model_program), BudgetExhaustionPolicy::Continue});
  for (auto &program : programs) {
    std::vector<StepId> followups;
    for (const auto &step : program.sequence)
      if (step.rule == "Reduce")
        followups.push_back(step.id);
    OnlineStrategyStage stage{std::move(program), BudgetExhaustionPolicy::Continue,
                              std::move(followups)};
    // satsuma.h:809-810: another pass only after symmetry-breaking output and
    // while no pass has reached a detection limit.
    stage.repeat_trigger = RepeatTrigger::SymmetryBreakingOutput;
    stage.stop_repeat_at_detection_limit = true;
    stages.push_back(std::move(stage));
  }
  // The preceding program has already run the same refinement/ULC producers
  // to a scoped no-progress boundary. Repeating that discovery inside the
  // following graph step is useful only if an earlier stage changed the root
  // formula. Generic graph discovery remains independent of this feedback.
  stages[2].structured_handoff_after_prior_progress = true;
  return stages;
}

strategy::OnlineStrategyStage
DefaultStrategyConfig::structured_root_stage_after_feedback(
    const strategy::OnlineStrategyStage &configured,
    bool prior_stage_progress, bool prior_unique_literal_amo_progress,
    bool prior_nonforcing_cut_progress, bool prior_detection_limit,
    const Kernel *root, bool *graph_component_limit) {
  auto scheduled = configured;
  // Satsuma runs its structured detectors only while its symmetry graph is
  // below the component limit and otherwise records reached_limits
  // (satsuma.h:168-176). AMO completion belongs to its clause database
  // (cnf.h ulc_add_amo) and still runs. The graph is the one the generic
  // producer builds for the live root formula.
  bool over_component_limit = false;
  if (root)
    for (auto &step : scheduled.program.sequence) {
      auto *parameters =
          std::get_if<SubstitutionRedundancyParameters>(&step.parameters);
      if (!parameters ||
          parameters->producer != SubstitutionRedundancyProducer::Orbitopal ||
          !parameters->maximum_orbitopal_matrices)
        continue;
      uint64_t work = 0;
      if (substitution_redundancy_symmetry_graph_vertices(
              *root, parameters->graph_symmetry, work) >
          parameters->graph_symmetry.maximum_graph_vertices) {
        parameters->maximum_orbitopal_matrices = 0;
        over_component_limit = true;
      }
    }
  if (graph_component_limit)
    *graph_component_limit = over_component_limit;
  // The owner's reached_limits is sticky across iterations (satsuma.h:49,
  // 810): a later stage still completes the current pass, but never repeats.
  if ((prior_detection_limit || over_component_limit) &&
      scheduled.stop_repeat_at_detection_limit &&
      strategy::fixpoint_repeat(scheduled.program.repeat.mode))
    scheduled.program.repeat = {strategy::RepeatMode::Once, 1};
  for (auto &step : scheduled.program.sequence) {
    auto *parameters =
        std::get_if<SubstitutionRedundancyParameters>(&step.parameters);
    if (!parameters ||
        parameters->producer != SubstitutionRedundancyProducer::GraphSymmetry)
      continue;
    // A nonforcing cut discovered only after ULC completion carries forward
    // certified information from that helper BIG.  The startup BCE pass may
    // then retire the helper binaries, so publish the cut instead of erasing
    // both the temporary representation and its durable consequence.  Two
    // independent complete graph stages finding a cut are also compound
    // certified progress even when each stage exposes only one row.  A lone
    // binary-only batch remains withheld because it changes only the initial
    // CDCL fundamental domain.  Public graph enumeration keeps its caller-
    // selected admission mode.
    if (prior_unique_literal_amo_progress || prior_nonforcing_cut_progress)
      parameters->graph_symmetry.nonforcing_cut_admission =
          SymmetryNonforcingCutAdmission::Publish;
    if (scheduled.structured_handoff_after_prior_progress &&
        !prior_stage_progress) {
      parameters->graph_symmetry.use_structured_generators = false;
      parameters->graph_symmetry.stabilize_structured_variables = false;
    }
  }
  return scheduled;
}

std::vector<strategy::OnlineStrategyStage>
DefaultStrategyConfig::
bounded_backtracking_structured_root_preprocessing_stages() {
  auto stages = structured_root_preprocessing_stages();
  for (auto &stage : stages)
    for (auto &step : stage.program.sequence)
      if (auto *parameters =
              std::get_if<SubstitutionRedundancyParameters>(&step.parameters))
        parameters->graph_symmetry.backend_search_budget = 1;
  return stages;
}

std::vector<strategy::OnlineStrategyStage>
DefaultStrategyConfig::large_domain_structured_root_preprocessing_stages() {
  using namespace strategy;
  // Above the domain limit the owner runs its whole pass, including AMO
  // completion and row/row-column detection (satsuma.h:163-176), and changes
  // only the fixing pass to negation/binary fixing (satsuma.h:217, 227).
  // The graph step selects that fixing through schreier_domain_limit, which
  // skips the structured handoff there; there is no handoff to gate.
  auto stages = bounded_backtracking_structured_root_preprocessing_stages();
  for (auto &stage : stages) {
    for (auto &step : stage.program.sequence) {
      auto *parameters =
          std::get_if<SubstitutionRedundancyParameters>(&step.parameters);
      if (!parameters ||
          parameters->producer != SubstitutionRedundancyProducer::GraphSymmetry)
        continue;
      parameters->graph_symmetry.use_structured_generators = false;
      parameters->graph_symmetry.stabilize_structured_variables = false;
    }
    stage.structured_handoff_after_prior_progress = false;
  }
  return stages;
}

} // namespace tactix
