#pragma once

// Field tables of typed rule profiles and their tactix_online_* C mirrors,
// shared by decoding (api.cpp online_parameters) and encoding
// (parameter_encoding.hpp). Each row is F(kind, c_field, typed_field):
//   COPY     assigns the value either way;
//   NONZERO  decodes only a nonzero C value and encodes the value;
//   SETTING  maps a bool through TACTIX_ONLINE_SETTING_* (INHERIT keeps it);
//   CHOICE   maps an enum through its INHERIT-offset tag (zero keeps it);
//   CAST     maps an enum through its numeric value;
//   INVERT   maps a bool through a C flag that negates it.
// Fields with bespoke conversions stay with their callers.

#define TACTIX_BVE_FIELDS(F)                                                   \
  F(COPY, gate_mask, gate_mask)                                                \
  F(COPY, growth_bound, growth_bound)                                          \
  F(COPY, occurrence_limit, occurrence_limit)                                  \
  F(COPY, clause_size_limit, clause_size_limit)                                \
  F(COPY, xor_arity_limit, xor_arity_limit)                                    \
  F(COPY, resolvent_size_limit, resolvent_size_limit)                          \
  F(COPY, backward_occurrence_limit, backward_occurrence_limit)                \
  F(COPY, definition_budget, definition_budget)                                \
  F(COPY, definition_cores, definition_cores)                                  \
  F(COPY, definition_core_budget_multiplier, definition_core_budget_multiplier)\
  F(COPY, backward, backward)                                                  \
  F(COPY, on_the_fly_strengthening, on_the_fly_strengthening)                  \
  F(CAST, candidate_scope, candidate_scope)                                    \
  F(CAST, traversal, traversal)

#define TACTIX_ANALYZE_FIELDS(F)                                               \
  F(COPY, minimize_depth_limit, minimize_depth_limit)                          \
  F(COPY, conflict_reuse, conflict_reuse)                                      \
  F(COPY, chronological_levels, chronological_levels)                          \
  F(COPY, bump_reason_limit, bump_reason_limit)                                \
  F(COPY, bump_reason_depth, bump_reason_depth)                                \
  F(COPY, eager_subsume, eager_subsume)                                        \
  F(INVERT, disable_jump_reasons, jump_reasons)                                \
  F(COPY, enable_jump_reasons_bigbig, jump_reasons_bigbig)                     \
  F(INVERT, disable_shrink_glue_promotion, shrink_glue_promote)                \
  F(INVERT, exclude_extension_reason_bumps, bump_extension_reasons)

#define TACTIX_SWEEP_FIELDS(F)                                                 \
  F(COPY, max_depth, max_depth) F(COPY, max_depth_ceiling, max_depth_ceiling)  \
  F(COPY, max_env_vars, max_env_vars)                                          \
  F(COPY, max_env_vars_ceiling, max_env_vars_ceiling)                          \
  F(COPY, max_env_clauses, max_env_clauses)                                    \
  F(COPY, max_env_clauses_ceiling, max_env_clauses_ceiling)                    \
  F(COPY, max_candidates, max_candidates) F(COPY, env_ticks, env_ticks)        \
  F(COPY, max_proof_steps, max_proof_steps) F(COPY, max_seeds, max_seeds)      \
  F(COPY, round_ticks, round_ticks) F(COPY, strip_false, strip_false)          \
  F(COPY, measure_closure, measure_closure)                                    \
  F(COPY, full_partition, full_partition)                                      \
  F(COPY, skip_embedded_fixed, skip_embedded_fixed)                            \
  F(COPY, flip_rounds, flip_rounds) F(COPY, resume, resume)                    \
  F(COPY, force_general_proof, force_general_proof)

#define TACTIX_INSTANTIATE_FIELDS(F)                                           \
  F(COPY, minimum_clause_size, minimum_clause_size)                            \
  F(COPY, occurrence_limit, occurrence_limit)                                  \
  F(COPY, minimum_unassigned, minimum_unassigned)                              \
  F(COPY, include_redundant, include_redundant)                                \
  F(COPY, new_material_threshold, new_material_threshold)

#define TACTIX_FACTOR_FIELDS(F)                                                \
  F(COPY, maximum_clause_size, maximum_clause_size)                            \
  F(COPY, growth_bound, growth_bound)                                          \
  F(COPY, candidate_rounds, candidate_rounds) F(COPY, composite, composite)    \
  F(COPY, demote_fresh, demote_fresh)

#define TACTIX_REDUCE_FIELDS(F)                                                \
  F(COPY, tier1, tier1) F(COPY, tier2, tier2)                                  \
  F(SETTING, tier_protection_mode, protect_tiers)                              \
  F(SETTING, compact_collection_mode, compact_collection)                      \
  F(SETTING, reduction_bookkeeping_mode, reduction_bookkeeping)

#define TACTIX_WALK_FIELDS(F)                                                  \
  F(COPY, step_budget, step_budget) F(COPY, random_seed, random_seed)          \
  F(COPY, seed_candidate, seed_candidate) F(CAST, algorithm, algorithm)        \
  F(CAST, initialization, initialization)                                      \
  F(NONZERO, maximum_tries, maximum_tries)                                     \
  F(NONZERO, flips_per_try, flips_per_try)                                     \
  F(NONZERO, weight_smoothing_threshold, weight_smoothing_threshold)           \
  F(SETTING, aspiration_mode, aspiration)                                      \
  F(NONZERO, even_break_base_milli, even_break_base_milli)

#define TACTIX_REORDER_FIELDS(F)                                               \
  F(COPY, maximum_clause_size, maximum_clause_size)                            \
  F(NONZERO, clause_decay_milli, clause_decay_milli)                           \
  F(NONZERO, binary_weight_milli, binary_weight_milli)                         \
  F(NONZERO, balanced_polarity_milli, balanced_polarity_milli)

// The refutation limits ParityModel and OnehotModel share.
#define TACTIX_REFUTATION_FIELDS(F)                                            \
  F(NONZERO, refutation_maximum_variables, refutation_maximum_variables)       \
  F(NONZERO, refutation_maximum_groups, refutation_maximum_groups)             \
  F(NONZERO, refutation_maximum_constraints, refutation_maximum_constraints)   \
  F(NONZERO, refutation_maximum_arity, refutation_maximum_arity)               \
  F(NONZERO, refutation_maximum_proof_additions,                               \
    refutation_maximum_proof_additions)

#define TACTIX_PARITY_MODEL_FIELDS(F)                                          \
  F(COPY, maximum_variables, maximum_variables)                                \
  F(COPY, maximum_groups, maximum_groups)                                      \
  F(COPY, maximum_constraints, maximum_constraints)                            \
  F(COPY, minimum_group_clauses, minimum_group_clauses)                        \
  F(COPY, minimum_equations, minimum_equations)                                \
  F(COPY, maximum_free_variables, maximum_free_variables)                      \
  F(COPY, candidate_budget, candidate_budget) TACTIX_REFUTATION_FIELDS(F)      \
  F(NONZERO, refutation_probe_constraints, refutation_probe_constraints)       \
  F(NONZERO, refutation_minimum_probe_repeats, refutation_minimum_probe_repeats)

#define TACTIX_ONEHOT_MODEL_FIELDS(F)                                          \
  F(COPY, maximum_variables, maximum_variables)                                \
  F(COPY, maximum_domains, maximum_domains)                                    \
  F(COPY, maximum_constraints, maximum_constraints)                            \
  F(COPY, maximum_equation_arity, maximum_equation_arity)                      \
  F(COPY, minimum_domains, minimum_domains)                                    \
  F(COPY, minimum_equations, minimum_equations)                                \
  F(COPY, maximum_free_domains, maximum_free_domains)                          \
  F(COPY, candidate_budget, candidate_budget) TACTIX_REFUTATION_FIELDS(F)

#define TACTIX_GRAPH_LABELING_FIELDS(F)                                        \
  F(COPY, maximum_vertices, maximum_vertices)                                  \
  F(COPY, workspace_bytes, workspace_bytes)                                    \
  F(COPY, maximum_dfs_vertices, maximum_dfs_vertices)                          \
  F(COPY, maximum_dfs_nodes, maximum_dfs_nodes)                                \
  F(COPY, maximum_dfs_starts, maximum_dfs_starts)                              \
  F(COPY, early_dfs_choices, early_dfs_choices)                                \
  F(COPY, repair_rounds, repair_rounds)                                        \
  F(COPY, anneal_iterations, anneal_iterations)                                \
  F(COPY, stall_anneal_iterations, stall_anneal_iterations) F(COPY, seed, seed)

#define TACTIX_AFFINE_MODEL_FIELDS(F)                                          \
  F(COPY, maximum_variables, maximum_variables)                                \
  F(COPY, maximum_constraints, maximum_constraints)                            \
  F(COPY, maximum_arity, maximum_arity)                                        \
  F(COPY, maximum_projection, maximum_projection)                              \
  F(COPY, minimum_equations, minimum_equations)                                \
  F(COPY, maximum_entries, maximum_entries)                                    \
  F(COPY, import_bytes, import_bytes) F(COPY, linear_bytes, linear_bytes)      \
  F(COPY, table_bytes, table_bytes) F(COPY, maximum_work, maximum_work)        \
  F(COPY, candidate_budget, candidate_budget)                                  \
  F(COPY, permutations, permutations) F(COPY, seed, seed)                      \
  F(CAST, projection_mode, projection_mode)                                    \
  F(COPY, automatic_negated, automatic_negated)                                \
  F(COPY, shuffle_first, shuffle_first) F(COPY, free_phase, free_phase)        \
  F(COPY, residual_phase, residual_phase)

// The orbitopal producer's fields of an SR profile, except refinement.
#define TACTIX_ORBITOPAL_FIELDS(F)                                             \
  F(CAST, orbitopal_orientation, orbitopal_orientation)                        \
  F(CAST, orbitopal_column_order, orbitopal_column_order)                      \
  F(CAST, orbitopal_completion, orbitopal_completion)                          \
  F(CAST, orbitopal_discovery_order, orbitopal_discovery_order)                \
  F(CAST, orbitopal_row_orbit_source, orbitopal_row_orbit_source)              \
  F(COPY, minimum_orbitopal_rows, minimum_orbitopal_rows)                      \
  F(COPY, minimum_orbitopal_columns, minimum_orbitopal_columns)                \
  F(COPY, maximum_orbitopal_matrices, maximum_orbitopal_matrices)              \
  F(COPY, maximum_orbitopal_instances, maximum_orbitopal_instances)            \
  F(NONZERO, minimum_orbitopal_clique_columns, minimum_orbitopal_clique_columns)\
  F(COPY, maximum_orbitopal_clique_vertices, maximum_orbitopal_clique_vertices)\
  F(COPY, maximum_orbitopal_clique_ticks, maximum_orbitopal_clique_ticks)      \
  F(COPY, maximum_orbitopal_product_states, maximum_orbitopal_product_states)  \
  F(SETTING, orbitopal_unique_literal_amo_mode, complete_unique_literal_amos)  \
  F(SETTING, orbitopal_generator_fallback_mode, orbitopal_generator_fallback)  \
  F(SETTING, orbitopal_progress_feedback_mode, orbitopal_progress_feedback)    \
  F(SETTING, orbitopal_base_probe_filter_mode, orbitopal_base_probe_filter)    \
  F(NONZERO, orbitopal_base_probe_factor, orbitopal_base_probe_factor)

// The graph-symmetry block of an SR profile, except the clause scope.
#define TACTIX_GRAPH_SYMMETRY_FIELDS(F)                                        \
  F(COPY, maximum_graph_vertices, maximum_graph_vertices)                      \
  F(COPY, maximum_graph_edges, maximum_graph_edges)                            \
  F(COPY, maximum_symmetry_generators, maximum_generators)                     \
  F(COPY, symmetry_backend_search_budget, backend_search_budget)               \
  F(COPY, symmetry_backend_support_budget, backend_support_budget)             \
  F(COPY, symmetry_backend_component_size_limit, backend_component_size_limit) \
  F(COPY, maximum_symmetry_fixing_instances, maximum_fixing_instances)         \
  F(COPY, maximum_symmetry_generator_instances_per_pass,                       \
    maximum_generator_instances_per_pass)                                      \
  F(COPY, maximum_symmetry_group_ticks, maximum_group_ticks)                   \
  F(COPY, symmetry_chain_planning_tick_limit, chain_planning_tick_limit)       \
  F(NONZERO, symmetry_schreier_domain_limit, schreier_domain_limit)            \
  F(COPY, symmetry_seed, seed)                                                 \
  F(NONZERO, symmetry_error_bound, error_bound)                                \
  F(CAST, symmetry_fixing_mode, fixing_mode)                                   \
  F(CAST, symmetry_generator_order, generator_order)                           \
  F(CAST, symmetry_clause_order, clause_order)                                 \
  F(CAST, symmetry_literal_order, literal_order)                               \
  F(CAST, symmetry_pair_orientation, pair_orientation)                         \
  F(CHOICE, symmetry_binary_graph_mode, binary_graph_mode)                     \
  F(NONZERO, symmetry_binary_graph_density_permille,                           \
    binary_graph_density_permille)                                             \
  F(CHOICE, symmetry_clause_selection, clause_selection)                       \
  F(CHOICE, symmetry_chain_mode, chain_mode)                                   \
  F(CHOICE, symmetry_nonforcing_cut_admission, nonforcing_cut_admission)       \
  F(CHOICE, symmetry_base_selection, base_selection)                           \
  F(CHOICE, symmetry_cut_point, cut_point)                                     \
  F(CHOICE, symmetry_structured_scope, structured_scope)                       \
  F(SETTING, symmetry_direct_generator_involutions_mode,                       \
    direct_generator_involutions_only)                                         \
  F(SETTING, symmetry_decompose_mode, decompose)                               \
  F(SETTING, symmetry_prefer_depth_first_mode, prefer_depth_first)             \
  F(SETTING, symmetry_strong_certification_mode, strong_certification)         \
  F(SETTING, symmetry_factor_support_components_mode,                          \
    factor_support_components)                                                 \
  F(SETTING, symmetry_use_structured_generators_mode,                          \
    use_structured_generators)                                                 \
  F(SETTING, symmetry_stabilize_structured_variables_mode,                     \
    stabilize_structured_variables)                                            \
  F(SETTING, symmetry_preserve_detector_order_mode, preserve_detector_order)   \
  F(SETTING, symmetry_pointwise_root_stabilizer_mode,                          \
    pointwise_root_stabilizer)                                                 \
  F(SETTING, symmetry_negation_fixing_first_mode, negation_fixing_first)       \
  F(SETTING, symmetry_first_literal_fix_anchor_mode, first_literal_fix_anchor)
