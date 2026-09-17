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
  assert(program.size() == 18);
  assert(validate_inprocess_program(program, &error) && error.empty());
  assert(program[1].rule == "Substitute" &&
         program[7].rule == "Substitute" &&
         program[1].id != program[7].id);
  const auto sr = std::find_if(program.begin(), program.end(),
                               [](const InprocessStep &step) {
                                 return step.rule ==
                                        "SubstitutionRedundancy";
                               });
  assert(sr == program.end());
  assert(program[1].followup == Followup::AfterCongruence);
  assert(program[7].followup == Followup::AfterSweepOrFinal);

  tactix::DefaultStrategyConfig defaults;
  const OnlineStrategyProgram root =
      tactix::DefaultStrategyConfig::reference_structured_root_program();
  assert(root.sequence.size() == 3);
  assert(root.repeat.mode == RepeatMode::UntilScopedFixpoint &&
         root.repeat.limit == 0);
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
  const auto &model = defaults.inprocess.structured_root_stages[0].program;
  assert(model.repeat.mode == RepeatMode::Once && model.sequence.size() == 1);
  assert(model.sequence[0].rule == "Lucky" && model.sequence[0].instances == std::vector<uint64_t>{8});
  assert(std::holds_alternative<AffineModelParameters>(model.sequence[0].parameters));
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
  program[12].parameters = SubsumeForwardParameters{};
  program[13].parameters = StrengthenParameters{};
  assert(validate_inprocess_program(program, &error));
  program[0].parameters = StrengthenParameters{};
  assert(!validate_inprocess_program(program, &error));
  assert(error == "C2 parameters are attached to a different rule");

  program = reference_inprocess_program();
  program[0].candidate_order = static_cast<CandidateOrder>(99);
  assert(!validate_inprocess_program(program, &error));
  assert(error == "candidate order is invalid");
}
