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
  bool sample_post_propagation = true;
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
  uint32_t maximum_onehot_model_variables =
      OnehotModelParameters::MAXIMUM_VARIABLES_DEFAULT;
  uint32_t maximum_onehot_model_domains =
      OnehotModelParameters::MAXIMUM_DOMAINS_DEFAULT;
  uint32_t maximum_onehot_model_constraints =
      OnehotModelParameters::MAXIMUM_CONSTRAINTS_DEFAULT;
  uint32_t maximum_onehot_model_equation_arity =
      OnehotModelParameters::MAXIMUM_EQUATION_ARITY_DEFAULT;
  uint32_t maximum_onehot_model_free_domains =
      OnehotModelParameters::MAXIMUM_FREE_DOMAINS_DEFAULT;
  uint64_t maximum_onehot_model_candidates = 1024;
  uint64_t maximum_sr_validation_ticks = 10000000;
  uint64_t maximum_sr_matrices = 16;
  uint64_t maximum_sr_instances = 256;
  uint32_t maximum_sr_orbitopal_clique_minimum_columns = 16;
  uint64_t maximum_sr_orbitopal_clique_vertices = 100000;
  uint64_t maximum_sr_orbitopal_clique_ticks = 1000000;
  uint64_t maximum_sr_orbitopal_product_states = 100000;
  uint64_t maximum_sr_orbitopal_refinement_orbits = 64;
  uint32_t maximum_sr_orbitopal_base_probe_factor = 16;
  uint64_t maximum_sr_detection_ticks = 1000000;
  uint64_t maximum_sr_general_candidates_per_advance = 64;
  uint64_t maximum_sr_graph_vertices = 100000;
  uint64_t maximum_sr_graph_edges = 1000000;
  uint64_t maximum_sr_symmetry_generators = 128;
  uint64_t maximum_sr_backend_search_budget = 256;
  uint64_t maximum_sr_backend_support_budget = 1000000;
  uint64_t maximum_sr_backend_component_size = 100000;
  uint64_t maximum_sr_symmetry_fixing_instances = 256;
  uint64_t maximum_sr_symmetry_generator_instances_per_pass = 16;
  uint64_t maximum_sr_symmetry_group_ticks = 1000000;
  uint32_t maximum_sr_symmetry_error_bound = 24;
  uint32_t maximum_sr_symmetry_binary_graph_density_permille = 4000;
  // Mix independent zero/positive boundary samples with jointly positive
  // resource limits so generated symmetry work is reachable. Zero numerator
  // retains the original independent sampler; all limits still obey maxima.
  uint32_t sr_joint_positive_numerator = 1;
  uint32_t sr_joint_positive_denominator = 2;
  SubstitutionRedundancyParameters sr_reference;

  // The generator samples semantic boundaries around these caller-owned
  // reference values.  Keeping the domains here makes random exploration
  // reproducible and prevents a new rule heuristic from hiding in the
  // generator itself.
  uint32_t bve_occurrence_reference = 2000;
  uint32_t bve_clause_size_reference =
      BveParameters::RESOLVENT_SIZE_DEFAULT;
  uint32_t bve_resolvent_count_reference = UINT32_MAX;
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
  OnehotModelParameters onehot_model_reference;
  BalancedSignatureModelParameters balanced_signature_model_reference;
  FermatModelParameters fermat_model_reference;
  LookupCircuitModelParameters lookup_circuit_model_reference;
  SimonModelParameters simon_model_reference;
  StconnProofParameters stconn_proof_reference;
  SignedCardinalityProofParameters signed_cardinality_proof_reference;
  AffineModelParameters affine_model_reference;
  uint32_t maximum_affine_projection_coordinates = 32;
  uint32_t maximum_affine_scopes = 8;
  uint32_t maximum_affine_weight = 12;
  uint32_t affine_joint_positive_numerator = 1;
  uint32_t affine_joint_positive_denominator = 2;
  size_t maximum_congruence_gate_count =
      CongruenceParameters::GATE_COUNT_DEFAULT;
};

bool random_online_strategy_program(Kernel &, uint64_t seed,
                                    const RandomOnlineConfig &,
                                    OnlineStrategyProgram &,
                                    std::string *error = nullptr,
                                    volatile sig_atomic_t *stop = nullptr);

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
