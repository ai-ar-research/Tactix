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
  assert(validate_inprocess_program(program, &error) && error.empty());

  tactix::DefaultStrategyConfig defaults;
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
  // The shared program retains the selected startup cut and fixing order.
  assert(progress_graph.negation_fixing_first &&
         progress_graph.cut_point == tactix::SymmetryCutPoint::FirstOpen);
  assert(progress_graph.nonforcing_cut_admission ==
         tactix::SymmetryNonforcingCutAdmission::RequireCompoundProgress);
  assert(helper_progress_graph.nonforcing_cut_admission ==
         tactix::SymmetryNonforcingCutAdmission::Publish);
  assert(prior_cut_progress_graph.nonforcing_cut_admission ==
         tactix::SymmetryNonforcingCutAdmission::Publish);
  for (const auto &stage : defaults.inprocess.structured_root_stages) {
    assert(validate_online_strategy_program(stage.program));
    assert(stage.on_budget_exhaustion == BudgetExhaustionPolicy::Continue);
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
