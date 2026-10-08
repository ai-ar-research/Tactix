#undef NDEBUG
#include "policies/default_strategy_config.hpp"

#include <algorithm>
#include <cassert>
#include <string>
#include <vector>

using namespace tactix::strategy;

int main() {
  std::vector<InprocessStep> program = reference_inprocess_program();
  std::string error;
  assert(program.size() == 33);
  assert(validate_inprocess_program(program, &error) && error.empty());
  std::vector<const InprocessStep *> substitutes;
  for (const InprocessStep &step : program)
    if (step.rule == "Substitute")
      substitutes.push_back(&step);
  assert(substitutes.size() == 2 &&
         substitutes[0]->id != substitutes[1]->id);
  assert(substitutes[0]->followup == Followup::AfterCongruence);
  assert(substitutes[1]->followup == Followup::AfterSweepOrFinal);
  const auto parity = std::find_if(program.begin(), program.end(),
                                   [](const InprocessStep &step) {
                                     return step.rule == "ParityConsequence";
                                   });
  assert(parity != program.end() && !parity->enabled &&
         parity == program.begin() + 1);
  static constexpr const char *theory_actions[] = {
      "AffineConsequence", "ExactCoverExtension", "IndependentSetCover",
      "Gf3AffineConsequence", "DirectCspSupport", "DirectPhpRefutation",
      "Gf3GaussianConsequence", "TseitinCycleBreaking",
      "GraphIntervalEquivalence", "AffineMatrixInitialize",
      "NaryCspEquality", "Gf3GeneralRowConsequence",
      "NaryCspEqualityAmo", "NaryCspEqualityTransitivity"};
  for (size_t i = 0; i < std::size(theory_actions); ++i) {
    const auto action = std::find_if(
        program.begin(), program.end(), [&](const InprocessStep &step) {
          return step.rule == theory_actions[i];
        });
    assert(action == program.begin() + 2 + static_cast<ptrdiff_t>(i) &&
           !action->enabled);
  }
  const auto sr = std::find_if(program.begin(), program.end(),
                               [](const InprocessStep &step) {
                                 return step.rule ==
                                        "SubstitutionRedundancy";
                               });
  assert(sr == program.end());

  tactix::DefaultStrategyConfig defaults;
  const OnlineStrategyProgram root =
      tactix::DefaultStrategyConfig::reference_structured_root_program();
  assert(root.sequence.size() == 3);
  assert(root.repeat.mode == RepeatMode::UntilSizeScaledFixpoint &&
         root.repeat.limit == 128);
  for (const OnlineInvocation &step : root.sequence) {
    assert(step.granularity == InvocationGranularity::CompleteCurrentPass);
    assert(step.post_application == PostApplication::Propagate);
  }
  const auto &pure =
      std::get<BveParameters>(root.sequence[0].parameters);
  assert(root.sequence[0].rule == "BVE" &&
         root.sequence[0].work_unit == WorkUnit::BveTicks &&
         pure.candidate_scope == BveCandidateScope::PureOnly);
  const auto &orbitopal = std::get<tactix::SubstitutionRedundancyParameters>(
      root.sequence[1].parameters);
  assert(root.sequence[1].rule == "SubstitutionRedundancy");
  assert(root.sequence[1].repeat.mode == RepeatMode::UntilScopedFixpoint &&
         root.sequence[1].repeat.limit == 0);
  assert(orbitopal.producer ==
             tactix::SubstitutionRedundancyProducer::Orbitopal &&
         orbitopal.maximum_detection_ticks == UINT64_MAX &&
         orbitopal.maximum_validation_ticks == 10'000'000 &&
         orbitopal.maximum_orbitopal_matrices == UINT64_MAX &&
         orbitopal.maximum_orbitopal_instances == UINT64_MAX &&
         orbitopal.maximum_orbitopal_product_states == 100'000 &&
         orbitopal.graph_symmetry.maximum_graph_vertices ==
             tactix::DefaultStrategyConfig::
                 SYMMETRY_GRAPH_COMPONENT_LIMIT_DEFAULT &&
         orbitopal.graph_symmetry.backend_component_size_limit ==
             tactix::DefaultStrategyConfig::
                 SYMMETRY_GRAPH_COMPONENT_LIMIT_DEFAULT &&
         orbitopal.orbitopal_column_order ==
             tactix::OrbitopalColumnOrder::CooccurrenceGreedyClique &&
         !orbitopal.orbitopal_generator_fallback &&
         orbitopal.orbitopal_progress_feedback &&
         orbitopal.orbitopal_completion ==
             tactix::OrbitopalCompletion::IndividualizedGraphCompletion);
  const auto &graph = std::get<tactix::SubstitutionRedundancyParameters>(
      root.sequence[2].parameters);
  assert(root.sequence[2].rule == "SubstitutionRedundancy");
  assert(root.sequence[0].repeat.mode == RepeatMode::Once);
  assert(root.sequence[2].repeat.mode == RepeatMode::Once);
  assert(graph.graph_symmetry.maximum_graph_vertices ==
             tactix::DefaultStrategyConfig::
                 SYMMETRY_GRAPH_COMPONENT_LIMIT_DEFAULT &&
         graph.graph_symmetry.backend_component_size_limit ==
             tactix::DefaultStrategyConfig::
                 SYMMETRY_GRAPH_COMPONENT_LIMIT_DEFAULT);
  assert(graph.producer ==
             tactix::SubstitutionRedundancyProducer::GraphSymmetry &&
         graph.graph_symmetry.maximum_fixing_instances == 128 &&
         graph.graph_symmetry.maximum_generator_instances_per_pass == 1);

  assert(defaults.inprocess.structured_root_stages.size() == 4);
  assert(defaults.inprocess.structured_root_backtracking_domain_limit ==
         32'000);
  assert(defaults.inprocess.structured_root_bounded_backtracking_stages.size() ==
         defaults.inprocess.structured_root_stages.size());
  assert(defaults.inprocess.structured_root_large_domain_limit == 50'000);
  assert(defaults.inprocess.structured_root_large_domain_stages.size() ==
         defaults.inprocess.structured_root_stages.size());
  const auto &affine_stage = defaults.inprocess.structured_root_stages[0];
  assert(affine_stage.program.sequence.size() == 1);
  const auto &affine = affine_stage.program.sequence.front();
  assert(affine.id == 109 && affine.rule == "Lucky" &&
         affine.granularity == InvocationGranularity::OneInstance &&
         affine.instances == std::vector<uint64_t>{8} &&
         std::holds_alternative<AffineModelParameters>(
             affine.parameters));
  for (size_t stage = 1;
       stage < defaults.inprocess.structured_root_stages.size(); ++stage)
    for (const auto &step :
         defaults.inprocess.structured_root_stages[stage].program.sequence)
      assert(step.rule != "Lucky");
  assert(defaults.inprocess.structured_root_stages[2]
             .structured_handoff_after_prior_progress);
  const auto no_progress =
      tactix::DefaultStrategyConfig::structured_root_stage_after_feedback(
          defaults.inprocess.structured_root_stages[2], false, false, false);
  const auto progress =
      tactix::DefaultStrategyConfig::structured_root_stage_after_feedback(
          defaults.inprocess.structured_root_stages[2], true, false, false);
  const auto helper_progress =
      tactix::DefaultStrategyConfig::structured_root_stage_after_feedback(
          defaults.inprocess.structured_root_stages[2], true, true, false);
  const auto prior_cut_progress =
      tactix::DefaultStrategyConfig::structured_root_stage_after_feedback(
          defaults.inprocess.structured_root_stages[3], true, false, true);
  const auto &no_progress_graph =
      std::get<tactix::SubstitutionRedundancyParameters>(
          no_progress.program.sequence[2].parameters).graph_symmetry;
  const auto &progress_graph =
      std::get<tactix::SubstitutionRedundancyParameters>(
          progress.program.sequence[2].parameters).graph_symmetry;
  const auto &helper_progress_graph =
      std::get<tactix::SubstitutionRedundancyParameters>(
          helper_progress.program.sequence[2].parameters).graph_symmetry;
  const auto &prior_cut_progress_graph =
      std::get<tactix::SubstitutionRedundancyParameters>(
          prior_cut_progress.program.sequence[0].parameters).graph_symmetry;
  assert(!no_progress_graph.use_structured_generators &&
         !no_progress_graph.stabilize_structured_variables);
  assert(progress_graph.use_structured_generators &&
         progress_graph.stabilize_structured_variables);
  // The owner's negation-first order and cut point (find_non_trivial_orbit).
  assert(progress_graph.negation_fixing_first &&
         progress_graph.cut_point ==
             tactix::SymmetryCutPoint::LargestOrbitLast);
  // satsuma.h:809-810: symmetry programs repeat only on symmetry-breaking
  // output and never after any pass reached a detection limit.
  assert(defaults.inprocess.structured_root_stages[0].repeat_trigger ==
             RepeatTrigger::AnyProgress &&
         !defaults.inprocess.structured_root_stages[0]
              .stop_repeat_at_detection_limit);
  for (const auto *stages :
       {&defaults.inprocess.structured_root_stages,
        &defaults.inprocess.structured_root_bounded_backtracking_stages,
        &defaults.inprocess.structured_root_large_domain_stages})
    for (size_t index = 1; index < stages->size(); ++index) {
      const auto &stage = (*stages)[index];
      assert(stage.repeat_trigger == RepeatTrigger::SymmetryBreakingOutput &&
             stage.stop_repeat_at_detection_limit &&
             stage.program.repeat.mode ==
                 RepeatMode::UntilSizeScaledFixpoint);
      // The sticky limit lets a later stage finish its pass but not repeat.
      const auto limited =
          tactix::DefaultStrategyConfig::structured_root_stage_after_feedback(
              stage, true, false, false, true);
      assert(limited.program.repeat.mode == RepeatMode::Once &&
             limited.program.sequence.size() == stage.program.sequence.size());
      const auto unlimited =
          tactix::DefaultStrategyConfig::structured_root_stage_after_feedback(
              stage, true, false, false, false);
      assert(unlimited.program.repeat.mode ==
             RepeatMode::UntilSizeScaledFixpoint);
    }
  {
    // satsuma.h:168-176: above the graph component limit the structured
    // detectors are skipped and reached_limits is recorded; AMO completion
    // (matrix count zero) is untouched.
    tactix::Kernel root;
    tactix::kernel_reserve_vars(root, 3);
    auto stage = defaults.inprocess.structured_root_stages[1];
    size_t matrix_steps = 0;
    for (auto &step : stage.program.sequence)
      if (auto *p = std::get_if<tactix::SubstitutionRedundancyParameters>(
              &step.parameters)) {
        p->graph_symmetry.maximum_graph_vertices = 5;
        matrix_steps += p->producer ==
                            tactix::SubstitutionRedundancyProducer::Orbitopal &&
                        p->maximum_orbitopal_matrices;
      }
    assert(matrix_steps > 0);
    bool limited = false;
    const auto gated =
        tactix::DefaultStrategyConfig::structured_root_stage_after_feedback(
            stage, false, false, false, false, &root, &limited);
    assert(limited && gated.program.repeat.mode == RepeatMode::Once);
    for (const auto &step : gated.program.sequence)
      if (const auto *p = std::get_if<tactix::SubstitutionRedundancyParameters>(
              &step.parameters))
        if (p->producer == tactix::SubstitutionRedundancyProducer::Orbitopal)
          assert(!p->maximum_orbitopal_matrices &&
                 p->complete_unique_literal_amos);
    for (auto &step : stage.program.sequence)
      if (auto *p = std::get_if<tactix::SubstitutionRedundancyParameters>(
              &step.parameters))
        p->graph_symmetry.maximum_graph_vertices = 6;
    const auto admitted =
        tactix::DefaultStrategyConfig::structured_root_stage_after_feedback(
            stage, false, false, false, false, &root, &limited);
    assert(!limited && admitted.program.repeat.mode ==
                           RepeatMode::UntilSizeScaledFixpoint);
  }
  assert(progress_graph.nonforcing_cut_admission ==
         tactix::SymmetryNonforcingCutAdmission::RequireCompoundProgress);
  assert(helper_progress_graph.nonforcing_cut_admission ==
         tactix::SymmetryNonforcingCutAdmission::Publish);
  assert(prior_cut_progress_graph.nonforcing_cut_admission ==
         tactix::SymmetryNonforcingCutAdmission::Publish);
  for (const auto &stage : defaults.inprocess.structured_root_stages) {
    assert(validate_online_strategy_program(stage.program));
    assert(stage.on_budget_exhaustion == BudgetExhaustionPolicy::Continue);
    OnlineStrategyProgram replay;
    const auto wire = serialize_online_strategy_program(stage.program);
    assert(deserialize_online_strategy_program(wire, replay));
    assert(serialize_online_strategy_program(replay) == wire);
    for (const auto &step : stage.program.sequence)
      if (const auto *p = std::get_if<ReduceParameters>(&step.parameters))
        assert(p->compact_collection && !p->victim_numerator &&
               !p->reduction_bookkeeping);
  }
  for (const auto &stage :
       defaults.inprocess.structured_root_bounded_backtracking_stages) {
    assert(validate_online_strategy_program(stage.program));
    for (const auto &step : stage.program.sequence)
      if (const auto *p =
              std::get_if<tactix::SubstitutionRedundancyParameters>(
                  &step.parameters))
        assert(p->graph_symmetry.backend_search_budget == 1);
  }
  // Above the domain limit the owner runs its whole pass, AMO completion and
  // row/product refinement included; only the fixing pass changes, which the
  // graph step selects through its Schreier domain limit.
  const auto &large = defaults.inprocess.structured_root_large_domain_stages;
  const auto &bounded =
      defaults.inprocess.structured_root_bounded_backtracking_stages;
  for (size_t index = 0; index < large.size(); ++index) {
    assert(large[index].program.sequence.size() ==
           bounded[index].program.sequence.size());
    assert(!large[index].structured_handoff_after_prior_progress);
    for (size_t at = 0; at < large[index].program.sequence.size(); ++at) {
      const auto &step = large[index].program.sequence[at];
      assert(step.id == bounded[index].program.sequence[at].id);
      const auto *p =
          std::get_if<tactix::SubstitutionRedundancyParameters>(&step.parameters);
      if (!p)
        continue;
      assert(p->graph_symmetry.backend_search_budget == 1);
      if (p->producer == tactix::SubstitutionRedundancyProducer::GraphSymmetry)
        assert(p->graph_symmetry.schreier_domain_limit == 50'000 &&
               !p->graph_symmetry.use_structured_generators &&
               !p->graph_symmetry.stabilize_structured_variables &&
               p->graph_symmetry.cut_point ==
                   tactix::SymmetryCutPoint::LargestOrbitLast);
    }
  }
  const auto large_has = [&](StepId id) {
    for (const auto &stage : large)
      for (const auto &step : stage.program.sequence)
        if (step.id == id)
          return true;
    return false;
  };
  // AMO completion (87, 92), row (90, 86) and product (97, 82) refinement.
  for (const StepId id : {87, 92, 90, 86, 97, 82, 84, 95})
    assert(large_has(id));
  for (const auto &stage :
       defaults.inprocess.structured_root_large_domain_stages)
    assert(validate_online_strategy_program(stage.program));

  program[1].id = program[0].id;
  assert(!validate_inprocess_program(program, &error));
  assert(error == "StepId is duplicated");
  program = reference_inprocess_program();
  program[0].id = 0;
  assert(!validate_inprocess_program(program, &error));
  program = reference_inprocess_program();
  program[0].rule.clear();
  assert(!validate_inprocess_program(program, &error));
  program = reference_inprocess_program();
  program[0].phase = static_cast<InprocessPhase>(99);
  assert(!validate_inprocess_program(program, &error));
  program = reference_inprocess_program();
  program[0].preprocessing = false;
  program[0].followup = Followup::AfterCongruence;
  assert(!validate_inprocess_program(program, &error));

  // Reorder, omission, repetition and per-occurrence activation are all valid
  // syntax. Registry/type validation happens transactionally at attachment.
  program = {
      {90, "BVE", InprocessPhase::Eliminate, true},
      {91, "Sweep", InprocessPhase::Probe, false},
      {92, "BVE", InprocessPhase::Probe, true},
  };
  BveParameters posed;
  posed.growth_bound = 7;
  program[0].parameters = posed;
  assert(validate_inprocess_program(program, &error));
  program[1].parameters = posed;
  assert(!validate_inprocess_program(program, &error));
  assert(error == "BVE parameters are attached to a different rule");
  program[1].parameters = std::monostate{};
  std::get<BveParameters>(program[0].parameters).xor_arity_limit =
      tactix::BVE_XOR_ARITY_CAPACITY + 1;
  assert(!validate_inprocess_program(program, &error));

  program = reference_inprocess_program();
  const auto subsume = std::find_if(
      program.begin(), program.end(), [](const InprocessStep &step) {
        return step.rule == "SubsumeForward";
      });
  const auto strengthen = std::find_if(
      program.begin(), program.end(), [](const InprocessStep &step) {
        return step.rule == "Strengthen";
      });
  assert(subsume != program.end() && strengthen != program.end());
  subsume->parameters = SubsumeForwardParameters{};
  strengthen->parameters = StrengthenParameters{};
  assert(validate_inprocess_program(program, &error));
  program[0].parameters = StrengthenParameters{};
  assert(!validate_inprocess_program(program, &error));
  assert(error == "C2 parameters are attached to a different rule");

  program = reference_inprocess_program();
  program[0].candidate_order = static_cast<CandidateOrder>(99);
  assert(!validate_inprocess_program(program, &error));
  assert(error == "candidate order is invalid");
}
