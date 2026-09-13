#pragma once

#include "strategy/online_inprocess.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tactix::strategy {

struct RandomOnlineConfig {
  std::vector<std::string> rules;
  uint32_t maximum_sequence_length = 8;
  uint32_t maximum_selected_batch = 8;
  // Generated fixpoint repetitions always use this finite ceiling. The public
  // executor still accepts RepeatSpec{UntilScopedFixpoint, 0} when an explicit
  // caller supplies the external stop condition required for an unbounded run.
  uint32_t maximum_repeat_count = 4;
  uint64_t maximum_application_budget = 64;
  uint64_t maximum_bve_tick_budget = 100000;
  uint64_t maximum_propagation_budget = 100000;
  uint32_t maximum_analyze_bump_reason_limit = 10;
  uint32_t maximum_analyze_bump_reason_depth = 4;
  uint64_t maximum_walk_step_budget = 2000000;
  uint64_t maximum_rule_work_budget = 2000000;
  uint32_t maximum_parity_model_variables =
      ParityModelParameters::MAXIMUM_VARIABLES_DEFAULT;
  uint32_t maximum_parity_model_groups =
      ParityModelParameters::MAXIMUM_GROUPS_DEFAULT;
  uint32_t maximum_parity_model_constraints =
      ParityModelParameters::MAXIMUM_CONSTRAINTS_DEFAULT;
  uint32_t maximum_parity_model_free_variables =
      ParityModelParameters::MAXIMUM_FREE_VARIABLES_DEFAULT;
  uint64_t maximum_parity_model_candidates = 1024;

  // The generator samples semantic boundaries around these caller-owned
  // reference values.  Keeping the domains here makes random exploration
  // reproducible and prevents a new rule heuristic from hiding in the
  // generator itself.
  uint32_t bve_occurrence_reference = 2000;
  uint32_t bve_clause_size_reference =
      BveParameters::RESOLVENT_SIZE_DEFAULT;
  uint32_t maximum_bve_growth_bound =
      4 * BveParameters::GROWTH_BOUND_DEFAULT;
  uint64_t maximum_bve_definition_budget =
      2 * BveParameters::DEFINITION_BUDGET_DEFAULT;
  uint32_t maximum_bve_definition_cores =
      2 * BveParameters::DEFINITION_CORES_DEFAULT;
  uint32_t maximum_bve_definition_core_budget_multiplier =
      2 * BveParameters::DEFINITION_CORE_BUDGET_MULTIPLIER_DEFAULT;
  AnalyzeParameters analyze_reference;
  SubsumeForwardParameters subsume_reference;
  StrengthenParameters strengthen_reference;
  CongruenceParameters congruence_reference;
  InstantiateParameters instantiate_reference;
  BackboneParameters backbone_reference;
  SweepParameters sweep_reference;
  VivifyParameters vivify_reference;
  BceParameters bce_reference;
  TernaryParameters ternary_reference;
  FactorParameters factor_reference;
  TransitiveReduceParameters transitive_reference;
  SubstituteParameters substitute_reference;
  ReduceParameters reduce_reference;
  WalkParameters walk_reference;
  ReorderParameters reorder_reference;
  ParityModelParameters parity_model_reference;
  size_t maximum_congruence_gate_count =
      CongruenceParameters::GATE_COUNT_DEFAULT;
};

bool random_online_strategy_program(Kernel &, uint64_t seed,
                                    const RandomOnlineConfig &,
                                    OnlineStrategyProgram &,
                                    std::string *error = nullptr,
                                    volatile sig_atomic_t *stop = nullptr);

std::string serialize_online_strategy_program(const OnlineStrategyProgram &);
bool deserialize_online_strategy_program(const std::string &,
                                         OnlineStrategyProgram &,
                                         std::string *error = nullptr);

inline bool random_online_inprocess_program(
    Kernel &kernel, uint64_t seed, const RandomOnlineConfig &config,
    OnlineInprocessProgram &program, std::string *error = nullptr,
    volatile sig_atomic_t *stop = nullptr) {
  return random_online_strategy_program(kernel, seed, config, program, error,
                                        stop);
}

inline std::string
serialize_online_inprocess_program(const OnlineInprocessProgram &program) {
  return serialize_online_strategy_program(program);
}

} // namespace tactix::strategy
