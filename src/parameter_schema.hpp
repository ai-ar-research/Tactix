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

// Shared source-recognition and certificate budgets; callers retain field order.
#define TACTIX_SOURCE_RECOGNITION_FIELDS(FIRST, NEXT) \
  FIRST(uint64_t, maximum_source_clauses, 0) \
  NEXT(uint64_t, maximum_source_literals, 0) \
  NEXT(uint64_t, maximum_recognition_work, 0)

#define TACTIX_CSP_PROOF_BUDGET_FIELDS(F) \
  F(uint64_t, maximum_proof_work, 0) \
  F(uint64_t, maximum_proof_additions, 0) \
  F(uint64_t, maximum_proof_literal_occurrences, 0) \
  F(uint64_t, maximum_journal_bytes, 0)

#define TACTIX_PARAMETER_SCHEMA_ExactCoverExtensionParameters(FIRST, NEXT) \
  FIRST(uint32_t, arity, 0) \
  NEXT(uint64_t, maximum_items, 0) \
  TACTIX_SOURCE_RECOGNITION_FIELDS(NEXT, NEXT) \
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
  TACTIX_SOURCE_RECOGNITION_FIELDS(FIRST, NEXT) \
  NEXT(uint32_t, maximum_domains, 0) \
  NEXT(uint32_t, maximum_domain_values, 0) \
  NEXT(uint32_t, maximum_cover_literals, 0) \
  NEXT(uint32_t, maximum_or_definitions, 0) \
  NEXT(uint32_t, maximum_or_inputs, 0) \
  NEXT(uint32_t, maximum_output_literals, 0) \
  TACTIX_CSP_PROOF_BUDGET_FIELDS(NEXT) \
  NEXT(uint32_t, maximum_external_extent, 0) \
  NEXT(uint64_t, maximum_session_keys, 0) \
  NEXT(uint64_t, maximum_session_work, 0)

#define TACTIX_PARAMETER_SCHEMA_DirectCspSupportParameters(FIRST, NEXT) \
  TACTIX_SOURCE_RECOGNITION_FIELDS(FIRST, NEXT) \
  NEXT(uint32_t, maximum_domain_values, 0) \
  NEXT(uint32_t, maximum_direct_clause_literals, 0) \
  NEXT(uint32_t, maximum_definition_clause_literals, 0) \
  TACTIX_CSP_PROOF_BUDGET_FIELDS(NEXT) \
  NEXT(uint32_t, maximum_extent_growth, 0) \
  NEXT(uint32_t, maximum_external_extent, 0) \
  NEXT(uint64_t, maximum_session_keys, 0) \
  NEXT(uint64_t, maximum_session_work, 0) \
  NEXT(bool, allow_extension, false)

#define TACTIX_PARAMETER_SCHEMA_NaryCspEqualityParameters(FIRST, NEXT) \
  TACTIX_SOURCE_RECOGNITION_FIELDS(FIRST, NEXT) \
  NEXT(uint32_t, maximum_domain_values, 0) \
  NEXT(uint64_t, maximum_forbidden_tuples, 0) \
  TACTIX_CSP_PROOF_BUDGET_FIELDS(NEXT) \
  NEXT(uint32_t, maximum_extent_growth, 0) \
  NEXT(uint32_t, maximum_external_extent, 0) \
  NEXT(uint64_t, maximum_session_keys, 0)

#define TACTIX_PARAMETER_SCHEMA_NaryCspEqualityAmoParameters(FIRST, NEXT) \
  TACTIX_SOURCE_RECOGNITION_FIELDS(FIRST, NEXT) \
  NEXT(uint32_t, maximum_domain_values, 0) \
  NEXT(uint64_t, maximum_forbidden_tuples, 0) \
  TACTIX_CSP_PROOF_BUDGET_FIELDS(NEXT) \
  NEXT(uint32_t, maximum_external_extent, 0) \
  NEXT(uint64_t, maximum_session_keys, 0)

#define TACTIX_PARAMETER_SCHEMA_NaryCspEqualityTransitivityParameters(FIRST, NEXT) \
  TACTIX_SOURCE_RECOGNITION_FIELDS(FIRST, NEXT) \
  NEXT(uint64_t, maximum_planning_work, 0) \
  NEXT(uint32_t, maximum_domain_values, 0) \
  NEXT(uint64_t, maximum_forbidden_tuples, 0) \
  TACTIX_CSP_PROOF_BUDGET_FIELDS(NEXT) \
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

// Profiles whose declaration order also matches their C conversion order.
// Rows: (conversion, C member, native member, type, native default).
// Defaults expand in the owning class; the API ignores the last two columns.
#define TACTIX_DECLARE_PROFILE_FIELD(Kind, C, Name, Type, Default) Type Name = Default;

#define TACTIX_ANALYZE_FIELDS(F) \
  F(COPY, minimize_depth_limit, minimize_depth_limit, uint32_t, MINIMIZE_DEPTH_DEFAULT) \
  F(COPY, conflict_reuse, conflict_reuse, bool, true) \
  F(COPY, chronological_levels, chronological_levels, uint32_t, 100) \
  F(COPY, bump_reason_limit, bump_reason_limit, uint32_t, 0) \
  F(COPY, bump_reason_depth, bump_reason_depth, uint32_t, 1) \
  F(COPY, eager_subsume, eager_subsume, uint32_t, EAGER_SUBSUME_CAPACITY) \
  F(INVERT, disable_jump_reasons, jump_reasons, bool, true) \
  F(COPY, enable_jump_reasons_bigbig, jump_reasons_bigbig, bool, false) \
  F(INVERT, disable_shrink_glue_promotion, shrink_glue_promote, bool, true) \
  F(INVERT, exclude_extension_reason_bumps, bump_extension_reasons, bool, true)

#define TACTIX_INSTANTIATE_FIELDS(F) \
  F(COPY, minimum_clause_size, minimum_clause_size, uint32_t, CLAUSE_SIZE_DEFAULT) \
  F(COPY, occurrence_limit, occurrence_limit, uint32_t, OCCURRENCE_DEFAULT) \
  F(COPY, minimum_unassigned, minimum_unassigned, uint32_t, UNASSIGNED_DEFAULT) \
  F(COPY, include_redundant, include_redundant, bool, false) \
  F(COPY, new_material_threshold, new_material_threshold, uint32_t, NEW_MATERIAL_DEFAULT)

#define TACTIX_FACTOR_FIELDS(F) \
  F(COPY, maximum_clause_size, maximum_clause_size, uint32_t, 5) \
  F(COPY, growth_bound, growth_bound, uint32_t, 0) \
  F(COPY, candidate_rounds, candidate_rounds, uint32_t, 2) \
  F(COPY, composite, composite, bool, false) \
  F(COPY, demote_fresh, demote_fresh, bool, true)

#define TACTIX_SWEEP_FIELDS(F) \
  F(COPY, max_depth, max_depth, uint32_t, SweepReferenceDefaults::DEPTH) \
  F(COPY, max_depth_ceiling, max_depth_ceiling, uint32_t, SweepReferenceDefaults::MAX_DEPTH) \
  F(COPY, max_env_vars, max_env_vars, uint32_t, SweepReferenceDefaults::VARS) \
  F(COPY, max_env_vars_ceiling, max_env_vars_ceiling, uint32_t, SweepReferenceDefaults::MAX_VARS) \
  F(COPY, max_env_clauses, max_env_clauses, uint32_t, SweepReferenceDefaults::CLAUSES) \
  F(COPY, max_env_clauses_ceiling, max_env_clauses_ceiling, uint32_t, SweepReferenceDefaults::MAX_CLAUSES) \
  F(COPY, max_candidates, max_candidates, uint32_t, SweepReferenceDefaults::CANDIDATE_PAIRS) \
  F(COPY, env_ticks, env_ticks, uint64_t, SweepReferenceDefaults::ENVIRONMENT_TICKS) \
  F(COPY, max_proof_steps, max_proof_steps, uint32_t, SweepReferenceDefaults::PROOF_STEPS) \
  F(COPY, max_seeds, max_seeds, uint32_t, SweepReferenceDefaults::SEED_CAP) \
  F(COPY, round_ticks, round_ticks, uint64_t, 0) \
  F(COPY, strip_false, strip_false, bool, true) \
  F(COPY, measure_closure, measure_closure, bool, false) \
  F(COPY, full_partition, full_partition, bool, true) \
  F(COPY, skip_embedded_fixed, skip_embedded_fixed, bool, false) \
  F(COPY, flip_rounds, flip_rounds, uint32_t, SweepReferenceDefaults::FLIP_ROUNDS) \
  F(COPY, resume, resume, bool, true) \
  F(COPY, force_general_proof, force_general_proof, bool, false)

#define TACTIX_REFUTATION_FIELDS(F) \
  F(NONZERO, refutation_maximum_variables, refutation_maximum_variables, uint32_t, REFUTATION_MAXIMUM_VARIABLES_DEFAULT) \
  F(NONZERO, refutation_maximum_groups, refutation_maximum_groups, uint32_t, REFUTATION_MAXIMUM_GROUPS_DEFAULT) \
  F(NONZERO, refutation_maximum_constraints, refutation_maximum_constraints, uint32_t, REFUTATION_MAXIMUM_CONSTRAINTS_DEFAULT) \
  F(NONZERO, refutation_maximum_arity, refutation_maximum_arity, uint32_t, REFUTATION_MAXIMUM_ARITY_DEFAULT) \
  F(NONZERO, refutation_maximum_proof_additions, refutation_maximum_proof_additions, uint64_t, REFUTATION_MAXIMUM_PROOF_ADDITIONS_DEFAULT)

#define TACTIX_ONEHOT_MODEL_FIELDS(F) \
  F(COPY, maximum_variables, maximum_variables, uint32_t, MAXIMUM_VARIABLES_DEFAULT) \
  F(COPY, maximum_domains, maximum_domains, uint32_t, MAXIMUM_DOMAINS_DEFAULT) \
  F(COPY, maximum_constraints, maximum_constraints, uint32_t, MAXIMUM_CONSTRAINTS_DEFAULT) \
  F(COPY, maximum_equation_arity, maximum_equation_arity, uint32_t, MAXIMUM_EQUATION_ARITY_DEFAULT) \
  F(COPY, minimum_domains, minimum_domains, uint32_t, MINIMUM_DOMAINS_DEFAULT) \
  F(COPY, minimum_equations, minimum_equations, uint32_t, MINIMUM_EQUATIONS_DEFAULT) \
  F(COPY, maximum_free_domains, maximum_free_domains, uint32_t, MAXIMUM_FREE_DOMAINS_DEFAULT) \
  F(COPY, candidate_budget, candidate_budget, uint64_t, CANDIDATE_BUDGET_DEFAULT) \
  TACTIX_REFUTATION_FIELDS(F)
