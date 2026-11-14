#pragma once

// Native parameter declarations and their stable visitor order share one list.
// Rows are (type, member, default); defaults expand in the owning class scope.
// FIRST/NEXT differ only when emitting the comma-separated visitor arguments.
#define TACTIX_DECLARE_PARAMETER_FIELD(Type, Name, Default) Type Name = Default;
#define TACTIX_DECLARE_PARAMETER_FIELDS(Family)                                \
  TACTIX_PARAMETER_SCHEMA_##Family(TACTIX_DECLARE_PARAMETER_FIELD,              \
                                   TACTIX_DECLARE_PARAMETER_FIELD)
#define TACTIX_FIRST_PARAMETER_FIELD(Type, Name, Default) p.Name
#define TACTIX_NEXT_PARAMETER_FIELD(Type, Name, Default) , p.Name
#define TACTIX_VISIT_PARAMETER_FIELDS(Family)                                  \
  template <class Parameters, class Visitor>                                  \
  auto visit_parameter_fields(FieldsOf<Family>, Parameters &p, Visitor visit) { \
    return visit(TACTIX_PARAMETER_SCHEMA_##Family(                             \
        TACTIX_FIRST_PARAMETER_FIELD, TACTIX_NEXT_PARAMETER_FIELD));            \
  }

#define TACTIX_PARAMETER_SCHEMA_BalancedSignatureModelParameters(FIRST, NEXT) \
  FIRST(uint32_t, maximum_balls, MAXIMUM_BALLS_DEFAULT) \
  NEXT(uint32_t, maximum_width, MAXIMUM_WIDTH_DEFAULT) \
  NEXT(uint64_t, restarts, RESTARTS_DEFAULT) \
  NEXT(uint64_t, steps_per_restart, STEPS_PER_RESTART_DEFAULT) \
  NEXT(uint32_t, samples_per_step, SAMPLES_PER_STEP_DEFAULT) \
  NEXT(uint64_t, seed, SEED_DEFAULT) \
  NEXT(uint32_t, completion_rounds, COMPLETION_ROUNDS_DEFAULT)

#define TACTIX_PARAMETER_SCHEMA_FermatModelParameters(FIRST, NEXT) \
  FIRST(uint32_t, maximum_bits, MAXIMUM_BITS_DEFAULT) \
  NEXT(uint32_t, comparator_tail_clauses, COMPARATOR_TAIL_CLAUSES_DEFAULT) \
  NEXT(uint64_t, rho_constants, RHO_CONSTANTS_DEFAULT) \
  NEXT(uint64_t, rho_iterations, RHO_ITERATIONS_DEFAULT)

#define TACTIX_PARAMETER_SCHEMA_LookupCircuitModelParameters(FIRST, NEXT) \
  FIRST(uint32_t, maximum_fanin, MAXIMUM_FANIN_DEFAULT) \
  NEXT(uint32_t, maximum_fanout, MAXIMUM_FANOUT_DEFAULT) \
  NEXT(uint32_t, maximum_primary_inputs, MAXIMUM_PRIMARY_INPUTS_DEFAULT) \
  NEXT(uint32_t, maximum_support, MAXIMUM_SUPPORT_DEFAULT) \
  NEXT(uint64_t, maximum_vector_gate_evaluations, MAXIMUM_VECTOR_GATE_EVALUATIONS_DEFAULT)

#define TACTIX_PARAMETER_SCHEMA_SimonModelParameters(FIRST, NEXT) \
  FIRST(uint32_t, maximum_rounds, MAXIMUM_ROUNDS_DEFAULT) \
  NEXT(uint64_t, maximum_candidates, MAXIMUM_CANDIDATES_DEFAULT)

#define TACTIX_PARAMETER_SCHEMA_ExactCoverModelParameters(FIRST, NEXT) \
  FIRST(uint32_t, block_arity, BLOCK_ARITY_DEFAULT) \
  NEXT(uint64_t, maximum_nodes, MAXIMUM_NODES_DEFAULT) \
  NEXT(uint64_t, restarts, RESTARTS_DEFAULT) \
  NEXT(uint64_t, restart_nodes, RESTART_NODES_DEFAULT) \
  NEXT(uint64_t, seed, SEED_DEFAULT)

#define TACTIX_PARAMETER_SCHEMA_IndependentSetModelParameters(FIRST, NEXT) \
  FIRST(uint32_t, maximum_vertices, MAXIMUM_VERTICES_DEFAULT) \
  NEXT(uint64_t, maximum_steps, MAXIMUM_STEPS_DEFAULT) \
  NEXT(uint64_t, completion_rounds, COMPLETION_ROUNDS_DEFAULT) \
  NEXT(uint64_t, seed, SEED_DEFAULT)

#define TACTIX_PARAMETER_SCHEMA_StconnProofParameters(FIRST, NEXT) \
  FIRST(uint32_t, maximum_width, MAXIMUM_WIDTH_DEFAULT) \
  NEXT(uint32_t, maximum_columns, MAXIMUM_COLUMNS_DEFAULT) \
  NEXT(uint64_t, maximum_rup_clauses, MAXIMUM_RUP_CLAUSES_DEFAULT)

#define TACTIX_PARAMETER_SCHEMA_SignedCardinalityProofParameters(FIRST, NEXT) \
  FIRST(uint32_t, maximum_variables, MAXIMUM_VARIABLES_DEFAULT) \
  NEXT(uint32_t, maximum_groups, MAXIMUM_GROUPS_DEFAULT) \
  NEXT(uint32_t, maximum_constraints, MAXIMUM_CONSTRAINTS_DEFAULT) \
  NEXT(uint32_t, maximum_group_arity, MAXIMUM_GROUP_ARITY_DEFAULT) \
  NEXT(uint64_t, maximum_bdd_nodes, MAXIMUM_BDD_NODES_DEFAULT) \
  NEXT(uint64_t, maximum_proof_additions, MAXIMUM_PROOF_ADDITIONS_DEFAULT)

#define TACTIX_PARAMETER_SCHEMA_AffineImageFields(FIRST, NEXT) \
  FIRST(uint64_t, maximum_clauses, 0) \
  NEXT(uint64_t, maximum_snapshot_literals, 0) \
  NEXT(uint64_t, maximum_snapshot_work, 0) \
  NEXT(uint64_t, maximum_groups, 0) \
  NEXT(uint64_t, maximum_patterns, 0) \
  NEXT(uint64_t, maximum_elimination_work, 0) \
  NEXT(uint32_t, maximum_arity, 0) \
  NEXT(uint32_t, maximum_intermediate_width, 0) \
  NEXT(uint32_t, maximum_sources, 0) \
  NEXT(uint32_t, maximum_output_width, 0) \
  NEXT(uint64_t, maximum_proof_additions, 0) \
  NEXT(uint64_t, maximum_proof_deletions, 0) \
  NEXT(uint64_t, maximum_journal_bytes, 0)

#define TACTIX_PARAMETER_SCHEMA_AffineBasisFields(FIRST, NEXT) \
  FIRST(uint64_t, basis_maximum_work, 0) \
  NEXT(uint32_t, basis_maximum_intermediate_width, 0) \
  NEXT(uint32_t, basis_maximum_sources, 0) \
  NEXT(uint32_t, basis_maximum_premises, 0) \
  NEXT(uint32_t, basis_maximum_nodes, 0) \
  NEXT(uint32_t, basis_maximum_direct_width, 0) \
  NEXT(uint32_t, maximum_external_extent, 0) \
  NEXT(uint64_t, maximum_session_keys, 0) \
  NEXT(uint64_t, maximum_session_work, 0)

#define TACTIX_PARAMETER_SCHEMA_ExactCoverExtensionParameters(FIRST, NEXT) \
  FIRST(uint32_t, arity, 0) \
  NEXT(uint64_t, maximum_items, 0) \
  NEXT(uint64_t, maximum_source_clauses, 0) \
  NEXT(uint64_t, maximum_source_literals, 0) \
  NEXT(uint64_t, maximum_recognition_work, 0) \
  NEXT(uint64_t, maximum_premise_references, 0) \
  NEXT(uint32_t, maximum_definitions, 0) \
  NEXT(uint64_t, maximum_additions, 0) \
  NEXT(uint64_t, maximum_literal_occurrences, 0) \
  NEXT(uint32_t, maximum_extent_growth, 0) \
  NEXT(uint32_t, maximum_external_extent, 0) \
  NEXT(uint64_t, maximum_journal_bytes, 0)

#define TACTIX_PARAMETER_SCHEMA_IndependentSetCoverParameters(FIRST, NEXT) \
  FIRST(uint64_t, maximum_clauses, 0) \
  NEXT(uint64_t, maximum_literals, 0) \
  NEXT(uint64_t, maximum_recognition_work, 0) \
  NEXT(uint64_t, maximum_proof_work, 0) \
  NEXT(uint64_t, maximum_proof_additions, 0) \
  NEXT(uint64_t, maximum_proof_deletions, 0) \
  NEXT(uint64_t, maximum_proof_literal_occurrences, 0) \
  NEXT(uint64_t, maximum_journal_bytes, 0) \
  NEXT(uint32_t, maximum_vertices, 0) \
  NEXT(uint32_t, maximum_target, 0)

#define TACTIX_PARAMETER_SCHEMA_Gf3AffineConsequenceParameters(FIRST, NEXT) \
  FIRST(uint64_t, maximum_source_clauses, 0) \
  NEXT(uint64_t, maximum_source_literals, 0) \
  NEXT(uint32_t, maximum_domains, 0) \
  NEXT(uint32_t, maximum_equations, 0) \
  NEXT(uint32_t, maximum_equation_arity, 0) \
  NEXT(uint32_t, maximum_tuples_per_equation, 0) \
  NEXT(uint32_t, maximum_result_arity, 0) \
  NEXT(uint32_t, maximum_result_tuples, 0) \
  NEXT(uint64_t, maximum_additions, 0) \
  NEXT(uint64_t, maximum_addition_literals, 0) \
  NEXT(uint64_t, maximum_recognition_work, 0) \
  NEXT(uint64_t, maximum_planning_work, 0) \
  NEXT(uint64_t, maximum_duplicate_scan_work, 0) \
  NEXT(uint64_t, maximum_revalidation_work, 0) \
  NEXT(uint64_t, maximum_proof_work, 0) \
  NEXT(uint64_t, maximum_proof_additions, 0) \
  NEXT(uint64_t, maximum_proof_literal_occurrences, 0) \
  NEXT(uint64_t, maximum_arena_words, 0) \
  NEXT(uint64_t, maximum_journal_bytes, 0) \
  NEXT(uint32_t, maximum_external_extent, 0) \
  NEXT(uint64_t, maximum_session_keys, 0) \
  NEXT(uint64_t, maximum_session_work, 0)

#define TACTIX_PARAMETER_SCHEMA_DirectCspGuardedCoverParameters(FIRST, NEXT) \
  FIRST(uint64_t, maximum_source_clauses, 0) \
  NEXT(uint64_t, maximum_source_literals, 0) \
  NEXT(uint64_t, maximum_recognition_work, 0) \
  NEXT(uint32_t, maximum_domains, 0) \
  NEXT(uint32_t, maximum_domain_values, 0) \
  NEXT(uint32_t, maximum_cover_literals, 0) \
  NEXT(uint32_t, maximum_or_definitions, 0) \
  NEXT(uint32_t, maximum_or_inputs, 0) \
  NEXT(uint32_t, maximum_output_literals, 0) \
  NEXT(uint64_t, maximum_proof_work, 0) \
  NEXT(uint64_t, maximum_proof_additions, 0) \
  NEXT(uint64_t, maximum_proof_literal_occurrences, 0) \
  NEXT(uint64_t, maximum_journal_bytes, 0) \
  NEXT(uint32_t, maximum_external_extent, 0) \
  NEXT(uint64_t, maximum_session_keys, 0) \
  NEXT(uint64_t, maximum_session_work, 0)

#define TACTIX_PARAMETER_SCHEMA_DirectCspSupportParameters(FIRST, NEXT) \
  FIRST(uint64_t, maximum_source_clauses, 0) \
  NEXT(uint64_t, maximum_source_literals, 0) \
  NEXT(uint64_t, maximum_recognition_work, 0) \
  NEXT(uint32_t, maximum_domain_values, 0) \
  NEXT(uint32_t, maximum_direct_clause_literals, 0) \
  NEXT(uint32_t, maximum_definition_clause_literals, 0) \
  NEXT(uint64_t, maximum_proof_work, 0) \
  NEXT(uint64_t, maximum_proof_additions, 0) \
  NEXT(uint64_t, maximum_proof_literal_occurrences, 0) \
  NEXT(uint64_t, maximum_journal_bytes, 0) \
  NEXT(uint32_t, maximum_extent_growth, 0) \
  NEXT(uint32_t, maximum_external_extent, 0) \
  NEXT(uint64_t, maximum_session_keys, 0) \
  NEXT(uint64_t, maximum_session_work, 0) \
  NEXT(bool, allow_extension, false)

#define TACTIX_PARAMETER_SCHEMA_NaryCspEqualityParameters(FIRST, NEXT) \
  FIRST(uint64_t, maximum_source_clauses, 0) \
  NEXT(uint64_t, maximum_source_literals, 0) \
  NEXT(uint64_t, maximum_recognition_work, 0) \
  NEXT(uint32_t, maximum_domain_values, 0) \
  NEXT(uint64_t, maximum_forbidden_tuples, 0) \
  NEXT(uint64_t, maximum_proof_work, 0) \
  NEXT(uint64_t, maximum_proof_additions, 0) \
  NEXT(uint64_t, maximum_proof_literal_occurrences, 0) \
  NEXT(uint64_t, maximum_journal_bytes, 0) \
  NEXT(uint32_t, maximum_extent_growth, 0) \
  NEXT(uint32_t, maximum_external_extent, 0) \
  NEXT(uint64_t, maximum_session_keys, 0)

#define TACTIX_PARAMETER_SCHEMA_NaryCspEqualityAmoParameters(FIRST, NEXT) \
  FIRST(uint64_t, maximum_source_clauses, 0) \
  NEXT(uint64_t, maximum_source_literals, 0) \
  NEXT(uint64_t, maximum_recognition_work, 0) \
  NEXT(uint32_t, maximum_domain_values, 0) \
  NEXT(uint64_t, maximum_forbidden_tuples, 0) \
  NEXT(uint64_t, maximum_proof_work, 0) \
  NEXT(uint64_t, maximum_proof_additions, 0) \
  NEXT(uint64_t, maximum_proof_literal_occurrences, 0) \
  NEXT(uint64_t, maximum_journal_bytes, 0) \
  NEXT(uint32_t, maximum_external_extent, 0) \
  NEXT(uint64_t, maximum_session_keys, 0)

#define TACTIX_PARAMETER_SCHEMA_NaryCspEqualityTransitivityParameters(FIRST, NEXT) \
  FIRST(uint64_t, maximum_source_clauses, 0) \
  NEXT(uint64_t, maximum_source_literals, 0) \
  NEXT(uint64_t, maximum_recognition_work, 0) \
  NEXT(uint64_t, maximum_planning_work, 0) \
  NEXT(uint32_t, maximum_domain_values, 0) \
  NEXT(uint64_t, maximum_forbidden_tuples, 0) \
  NEXT(uint64_t, maximum_proof_work, 0) \
  NEXT(uint64_t, maximum_proof_additions, 0) \
  NEXT(uint64_t, maximum_proof_literal_occurrences, 0) \
  NEXT(uint64_t, maximum_journal_bytes, 0) \
  NEXT(uint32_t, maximum_external_extent, 0) \
  NEXT(uint64_t, maximum_session_keys, 0) \
  NEXT(uint64_t, maximum_session_bytes, 0) \
  NEXT(uint64_t, maximum_arena_words, 0) \
  NEXT(uint64_t, maximum_watch_words, 0) \
  NEXT(uint64_t, maximum_preparation_bytes, 0)

#define TACTIX_PARAMETER_SCHEMA_DirectPhpRefutationParameters(FIRST, NEXT) \
  FIRST(uint64_t, maximum_snapshot_clauses, 100000) \
  NEXT(uint64_t, maximum_snapshot_literals, 1000000) \
  NEXT(uint64_t, maximum_snapshot_work, 10000000) \
  NEXT(uint64_t, maximum_recognition_clauses, 100000) \
  NEXT(uint64_t, maximum_recognition_literals, 1000000) \
  NEXT(uint64_t, maximum_recognition_work, 100000000) \
  NEXT(uint64_t, maximum_row_candidates, 10000) \
  NEXT(uint64_t, maximum_row_combinations, 1000000) \
  NEXT(uint64_t, maximum_mapping_trials, 1000000) \
  NEXT(uint32_t, maximum_holes, 20) \
  NEXT(uint64_t, maximum_proof_additions, 1000000) \
  NEXT(uint64_t, maximum_proof_deletions, 1000000) \
  NEXT(uint64_t, maximum_proof_literals, 100000000) \
  NEXT(uint64_t, maximum_fresh_variables, 1000000) \
  NEXT(uint64_t, maximum_proof_events, 2000000) \
  NEXT(uint64_t, maximum_replay_work, 100000000)

#define TACTIX_PARAMETER_SCHEMA_Gf3GaussianConsequenceParameters(FIRST, NEXT) \
  FIRST(uint64_t, maximum_source_clauses, 1000) \
  NEXT(uint64_t, maximum_source_literals, 10000) \
  NEXT(uint32_t, maximum_domains, 100) \
  NEXT(uint32_t, maximum_equations, 100) \
  NEXT(uint32_t, maximum_equation_arity, 8) \
  NEXT(uint32_t, maximum_tuples_per_equation, 10000) \
  NEXT(uint64_t, maximum_bridge_work, 1000000) \
  NEXT(uint64_t, maximum_bridge_source_clauses, 10000) \
  NEXT(uint64_t, maximum_bridge_source_literals, 100000) \
  NEXT(uint32_t, maximum_bridge_domains, 100) \
  NEXT(uint32_t, maximum_bridge_equations, 100) \
  NEXT(uint64_t, maximum_bridge_terms, 1000) \
  NEXT(uint64_t, maximum_authentication_references, 10000) \
  NEXT(uint64_t, maximum_references_per_equation, 1000) \
  NEXT(uint64_t, maximum_gaussian_work, 100000) \
  NEXT(uint64_t, maximum_fill_entries, 100000) \
  NEXT(uint32_t, maximum_intermediate_width, 32) \
  NEXT(uint32_t, maximum_provenance_terms, 100) \
  NEXT(uint32_t, maximum_derivations, 1000) \
  NEXT(uint64_t, maximum_retained_plan_bytes, 1000000) \
  NEXT(uint64_t, maximum_certificate_work, 10000000) \
  NEXT(uint64_t, maximum_events, 100000) \
  NEXT(uint64_t, maximum_event_literals, 1000000) \
  NEXT(uint64_t, maximum_active_clauses, 100000) \
  NEXT(uint64_t, maximum_retained_source_references, 10000) \
  NEXT(uint32_t, maximum_event_width, 32) \
  NEXT(uint32_t, maximum_branch_domains, 32) \
  NEXT(uint32_t, maximum_consequences, 32) \
  NEXT(uint64_t, maximum_capture_work, 1000000) \
  NEXT(uint64_t, maximum_revalidation_work, 1000000) \
  NEXT(uint64_t, maximum_proof_additions, 100000) \
  NEXT(uint64_t, maximum_proof_literal_occurrences, 1000000) \
  NEXT(uint64_t, maximum_journal_bytes, 1000000) \
  NEXT(uint64_t, maximum_identity_bytes, 4000000) \
  NEXT(uint32_t, maximum_external_extent, 100000) \
  NEXT(uint64_t, maximum_session_keys, 10000) \
  NEXT(uint64_t, maximum_session_work, 1000000) \
  NEXT(uint64_t, maximum_session_identity_bytes, 4000000)

#define TACTIX_PARAMETER_SCHEMA_TseitinCycleBreakingParameters(FIRST, NEXT) \
  FIRST(uint64_t, maximum_snapshot_clauses, 100000) \
  NEXT(uint64_t, maximum_snapshot_literals, 1000000) \
  NEXT(uint64_t, maximum_snapshot_work, 10000000) \
  NEXT(uint32_t, maximum_snapshot_variables, 1000000) \
  NEXT(uint64_t, maximum_discovery_work, 10000000) \
  NEXT(uint64_t, maximum_discovery_clauses, 100000) \
  NEXT(uint64_t, maximum_discovery_groups, 100000) \
  /* Canonical retained payload bytes only. */ \
  NEXT(uint64_t, maximum_discovery_retained_bytes, 16000000) \
  NEXT(uint64_t, maximum_admission_work, 10000000) \
  NEXT(uint64_t, maximum_planning_work, 10000000) \
  NEXT(uint32_t, maximum_witness_length, 100000) \
  NEXT(uint32_t, maximum_planning_additions, 1) \
  NEXT(uint64_t, maximum_transaction_work, 10000000) \
  NEXT(uint64_t, maximum_transaction_additions, 3) \
  NEXT(uint64_t, maximum_transaction_deletions, 2) \
  NEXT(uint64_t, maximum_transaction_helpers, 2) \
  NEXT(uint64_t, maximum_transaction_witness_literals, 1000000) \
  NEXT(uint64_t, maximum_validation_ticks, 10000000) \
  NEXT(uint64_t, maximum_components, 10000) \
  NEXT(uint64_t, maximum_identity_bytes, 4000000) \
  NEXT(uint64_t, maximum_total_identity_bytes, 16000000) \
  NEXT(uint64_t, maximum_identity_work, 32000000) \
  NEXT(uint64_t, maximum_journal_bytes, 8000000) \
  NEXT(uint64_t, maximum_session_keys, 10000) \
  NEXT(uint64_t, maximum_session_work, 1000000) \
  NEXT(uint64_t, maximum_session_identity_bytes, 16000000)

#define TACTIX_PARAMETER_SCHEMA_SemanticVariableAdviceParameters(FIRST, NEXT) \
  FIRST(uint32_t, maximum_variables, 0) \
  NEXT(uint64_t, maximum_priority_sum, 0) \
  NEXT(uint32_t, existing_theory_priority, 0) \
  NEXT(uint32_t, introduced_theory_priority, 0)
