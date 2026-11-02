// Native Stats counters, one row per field in declaration order. Rows carry
// no semicolons; each includer defines every TACTIX_STATS_* macro first.
// kernel.hpp expands them into struct Stats and
// strategy/feedback_statistics.inc into the deterministic feedback
// inventory, which omits the `_nanos` wall timers.
TACTIX_STATS_U64(decisions)
TACTIX_STATS_U64(conflicts)
TACTIX_STATS_U64(propagations)
TACTIX_STATS_U64(ticks) // propagation memory-traffic proxy (see propagate.cpp)
TACTIX_STATS_U64(search_ticks)
TACTIX_STATS_U64(inprocess_propagations) // of those, inside a simplification
// Monotonic policy-observation epoch.  `unflushed_root` is a transient
// prefix marker and can return to zero inside Propagate, so it cannot tell
// DefaultPolicy whether this application created a true-level-zero fact.
// Counting at the assignment primitive lets the policy skip a second walk
// over every ordinary non-root implication while retaining its rule-boundary
// fixed-variable handoff.
TACTIX_STATS_U64(root_assignments)
// clause additions that landed behind the propagation cursor and rewound it
TACTIX_STATS_U64(propagation_rearms)
TACTIX_STATS_TIMER_ARRAY(rule_nanos, [MAX_TIMED_RULES]) // per registry rule id
TACTIX_STATS_ARRAY(rule_calls, [MAX_TIMED_RULES])
TACTIX_STATS_TIMER(inprocess_nanos) // whole slot, incl. scheduling not in a rule
// Inclusive phase-boundary attribution. These four buckets partition the
// ordinary Probe/Eliminate slot; structured-root startup is separate.
TACTIX_STATS_TIMER(inprocess_entry_nanos)
TACTIX_STATS_TIMER(inprocess_dense_connect_nanos)
TACTIX_STATS_TIMER(inprocess_slot_loop_nanos)
TACTIX_STATS_TIMER(inprocess_exit_nanos)
TACTIX_STATS_TIMER(structured_root_nanos)
TACTIX_STATS_TIMER(solve_nanos)     // wall time of solve(), set by the frontend
TACTIX_STATS_U64(sr_applications)
TACTIX_STATS_U64(sr_applied)
// Generated unique-literal AMO certificates which committed a binary.
// This provenance is a monotonic scheduling signal; ordinary SR binaries
// and caller-authored payloads do not contribute.
TACTIX_STATS_U64(sr_unique_literal_amo_binaries)
// Non-unique-literal consequences committed after generated helper binaries.
// This monotonic provenance lets DefaultPolicy distinguish consumed
// scaffolding from a helper-only result without assuming producer order.
TACTIX_STATS_U64(sr_post_amo_nonhelper_applied)
// The subset of those consequences where generic graph symmetry committed
// a root unit.  This shows that the completed BIG was consumed by a later
// group computation, rather than merely preceding an independent structured
// fixing.
TACTIX_STATS_U64(sr_post_amo_graph_root_units)
TACTIX_STATS_U64(sr_nonforcing_cut_batches_declined)
TACTIX_STATS_U64(sr_nonforcing_cuts_declined)
TACTIX_STATS_U64(sr_singleton_forcing_batches_declined)
TACTIX_STATS_U64(sr_singleton_forcing_rows_declined)
TACTIX_STATS_U64(residual_handoff_calls)
TACTIX_STATS_U64(residual_handoff_variables)
TACTIX_STATS_TIMER(residual_handoff_nanos)
TACTIX_STATS_U64(sr_validation_ticks)
TACTIX_STATS_U64(sr_witness_literals)
TACTIX_STATS_U64(switched) // mode switches (S4)
TACTIX_STATS_U64(target_saved) // target-phase snapshots
TACTIX_STATS_U64(best_saved)   // best-phase snapshots
TACTIX_STATS_U64(rephased)     // Rephase applications (S3)
TACTIX_STATS_U64(reorder_eligible) // scheduled occasions past the policy gate
TACTIX_STATS_U64(reordered)    // Reorder applications (S7, policy op)
TACTIX_STATS_U64(lucky_early_calls) // DefaultPolicy startup opportunities
TACTIX_STATS_U64(lucky_late_calls)  // ... after initial preprocessing
TACTIX_STATS_U64(lucky_late_sat)    // late opportunities proving SAT
TACTIX_STATS_U64(lucky_probes) // Lucky probes run (S5)
TACTIX_STATS_U64(parity_model_calls)
TACTIX_STATS_U64(parity_model_equations)
TACTIX_STATS_U64(parity_model_rank)
TACTIX_STATS_U64(parity_model_candidates)
TACTIX_STATS_U64(parity_model_sat)
TACTIX_STATS_U64(parity_model_interrupted)
TACTIX_STATS_U64(parity_refutation_probe_declines)
TACTIX_STATS_U64(parity_refutation_recognized)
TACTIX_STATS_U64(parity_refutation_unsat)
TACTIX_STATS_U64(parity_refutation_proof_additions)
TACTIX_STATS_U64(parity_refutation_proof_deletions)
TACTIX_STATS_U64(counting_calls) TACTIX_STATS_U64(counting_stars)
TACTIX_STATS_U64(counting_unsat)
TACTIX_STATS_U64(counting_work) TACTIX_STATS_U64(counting_interrupted)
TACTIX_STATS_U64(counting_additions)
TACTIX_STATS_U64(affine_model_calls) TACTIX_STATS_U64(affine_model_equations)
TACTIX_STATS_U64(affine_model_projection) TACTIX_STATS_U64(affine_model_entries)
TACTIX_STATS_U64(affine_model_candidates) TACTIX_STATS_U64(affine_model_sat)
TACTIX_STATS_U64(affine_model_work) TACTIX_STATS_U64(affine_model_interrupted)
TACTIX_STATS_U64(affine_model_budget) TACTIX_STATS_U64(affine_model_unavailable)
TACTIX_STATS_U64(onehot_model_calls)
TACTIX_STATS_U64(onehot_model_domains)
TACTIX_STATS_U64(onehot_model_equations)
TACTIX_STATS_U64(onehot_model_rank)
TACTIX_STATS_U64(onehot_model_candidates)
TACTIX_STATS_U64(onehot_model_sat)
TACTIX_STATS_U64(onehot_model_interrupted)
TACTIX_STATS_U64(exact_one_refutation_calls)
TACTIX_STATS_U64(exact_one_refutation_recognized)
TACTIX_STATS_U64(exact_one_refutation_equations)
TACTIX_STATS_U64(exact_one_refutation_unsat)
TACTIX_STATS_U64(exact_one_refutation_interrupted)
TACTIX_STATS_U64(exact_one_refutation_proof_additions)
TACTIX_STATS_U64(exact_one_refutation_proof_deletions)
TACTIX_STATS_U64(direct_csp_model_calls)
TACTIX_STATS_U64(direct_csp_model_domains)
TACTIX_STATS_U64(direct_csp_model_edges)
TACTIX_STATS_U64(direct_csp_model_restarts)
TACTIX_STATS_U64(direct_csp_model_infeasible)
TACTIX_STATS_U64(direct_csp_model_work)
TACTIX_STATS_INIT(direct_csp_model_best, UINT64_MAX)
TACTIX_STATS_U64(direct_csp_model_sat)
TACTIX_STATS_U64(balanced_signature_calls)
TACTIX_STATS_U64(balanced_signature_recognized)
TACTIX_STATS_U64(balanced_signature_forced_ttf)
TACTIX_STATS_U64(balanced_signature_candidates)
TACTIX_STATS_U64(balanced_signature_restarts)
TACTIX_STATS_U64(balanced_signature_steps)
TACTIX_STATS_U64(balanced_signature_completion_rounds)
TACTIX_STATS_U64(balanced_signature_sat)
TACTIX_STATS_U64(balanced_signature_interrupted)
TACTIX_STATS_U64(fermat_model_calls)
TACTIX_STATS_U64(fermat_model_recognized)
TACTIX_STATS_U64(fermat_model_candidates)
TACTIX_STATS_U64(fermat_model_factor_iterations)
TACTIX_STATS_U64(fermat_model_completion_rounds)
TACTIX_STATS_U64(fermat_model_sat)
TACTIX_STATS_U64(fermat_model_interrupted)
TACTIX_STATS_U64(cerny_model_calls)
TACTIX_STATS_U64(cerny_model_recognized)
TACTIX_STATS_U64(cerny_model_states)
TACTIX_STATS_U64(cerny_model_length)
TACTIX_STATS_U64(cerny_model_sat)
TACTIX_STATS_U64(cerny_model_interrupted)
TACTIX_STATS_U64(exact_cover_model_calls)
TACTIX_STATS_U64(exact_cover_model_recognized)
TACTIX_STATS_U64(exact_cover_model_blocks)
TACTIX_STATS_U64(exact_cover_model_nodes)
TACTIX_STATS_U64(exact_cover_model_restarts)
TACTIX_STATS_U64(exact_cover_model_budget_exhausted)
TACTIX_STATS_U64(exact_cover_model_sat)
TACTIX_STATS_U64(exact_cover_model_interrupted)
TACTIX_STATS_U64(ramsey_startup_attempts)
TACTIX_STATS_U64(ramsey_startup_recognized)
TACTIX_STATS_U64(ramsey_startup_recognition_work)
TACTIX_STATS_U64(ramsey_startup_certificate_work)
TACTIX_STATS_U64(ramsey_startup_nodes)
TACTIX_STATS_U64(ramsey_startup_additions)
TACTIX_STATS_U64(ramsey_startup_deletions)
TACTIX_STATS_U64(ramsey_startup_helpers)
TACTIX_STATS_U64(ramsey_startup_interrupted)
TACTIX_STATS_U64(ramsey_startup_exhausted)
TACTIX_STATS_U64(ramsey_startup_unsat)
TACTIX_STATS_U64(graph_labeling_calls)
TACTIX_STATS_U64(graph_labeling_recognized)
TACTIX_STATS_U64(graph_labeling_vertices)
TACTIX_STATS_U64(graph_labeling_separation)
TACTIX_STATS_U64(graph_labeling_seeds)
TACTIX_STATS_U64(graph_labeling_nodes)
TACTIX_STATS_U64(graph_labeling_repairs)
TACTIX_STATS_U64(graph_labeling_anneal_attempts)
TACTIX_STATS_U64(graph_labeling_work)
TACTIX_STATS_U64(graph_labeling_budget)
TACTIX_STATS_U64(graph_labeling_interrupted)
TACTIX_STATS_U64(graph_labeling_models_rejected)
TACTIX_STATS_U64(graph_labeling_sat)
TACTIX_STATS_U64(graph_interval_equivalence_calls)
TACTIX_STATS_U64(graph_interval_equivalence_recognized)
TACTIX_STATS_U64(graph_interval_equivalence_vertices)
TACTIX_STATS_U64(graph_interval_equivalence_gates)
TACTIX_STATS_U64(graph_interval_equivalence_intervals)
TACTIX_STATS_U64(graph_interval_equivalence_groups)
TACTIX_STATS_U64(graph_interval_equivalence_selected)
TACTIX_STATS_U64(graph_interval_equivalence_work)
TACTIX_STATS_U64(graph_interval_equivalence_source_clauses)
TACTIX_STATS_U64(graph_interval_equivalence_source_literals)
TACTIX_STATS_U64(graph_interval_equivalence_source_workspace)
TACTIX_STATS_U64(graph_interval_equivalence_resource_exhausted)
TACTIX_STATS_U64(graph_interval_equivalence_interrupted)
TACTIX_STATS_U64(graph_interval_equivalence_rup_ticks)
TACTIX_STATS_U64(graph_interval_equivalence_rup_rejected)
TACTIX_STATS_U64(graph_interval_equivalence_duplicates)
TACTIX_STATS_U64(graph_interval_equivalence_published)
TACTIX_STATS_U64(graph_interval_equivalence_proof_literal_occurrences)
TACTIX_STATS_U64(independent_set_model_calls)
TACTIX_STATS_U64(independent_set_model_recognized)
TACTIX_STATS_U64(independent_set_model_vertices)
TACTIX_STATS_U64(independent_set_model_target)
TACTIX_STATS_U64(independent_set_model_greedy_size)
TACTIX_STATS_U64(independent_set_model_size)
TACTIX_STATS_U64(independent_set_model_steps)
TACTIX_STATS_U64(independent_set_model_completion_rounds)
TACTIX_STATS_U64(independent_set_model_sat)
TACTIX_STATS_U64(independent_set_model_interrupted)
TACTIX_STATS_U64(independent_set_stable_cores)
TACTIX_STATS_U64(independent_set_stable_cover_clauses)
TACTIX_STATS_U64(independent_set_stable_resolvents)
TACTIX_STATS_U64(lookup_circuit_model_calls)
TACTIX_STATS_U64(lookup_circuit_model_recognized)
TACTIX_STATS_U64(lookup_circuit_model_gates)
TACTIX_STATS_U64(lookup_circuit_model_support)
TACTIX_STATS_U64(lookup_circuit_model_vector_evaluations)
TACTIX_STATS_U64(lookup_circuit_model_csp_work)
TACTIX_STATS_U64(lookup_circuit_model_sat)
TACTIX_STATS_U64(lookup_circuit_model_interrupted)
TACTIX_STATS_U64(simon_model_calls)
TACTIX_STATS_U64(simon_model_recognized)
TACTIX_STATS_U64(simon_model_rounds)
TACTIX_STATS_U64(simon_model_candidates)
TACTIX_STATS_U64(simon_model_sat)
TACTIX_STATS_U64(simon_model_interrupted)
TACTIX_STATS_U64(stconn_proof_calls)
TACTIX_STATS_U64(stconn_proof_recognized)
TACTIX_STATS_U64(stconn_proof_width)
TACTIX_STATS_U64(stconn_proof_columns)
TACTIX_STATS_U64(stconn_proof_rup_clauses)
TACTIX_STATS_U64(stconn_proof_budget_exhausted)
TACTIX_STATS_U64(stconn_proof_unsat)
TACTIX_STATS_U64(stconn_proof_interrupted)
TACTIX_STATS_U64(signed_cardinality_proof_calls)
TACTIX_STATS_U64(signed_cardinality_proof_recognized)
TACTIX_STATS_U64(signed_cardinality_proof_groups)
TACTIX_STATS_U64(signed_cardinality_proof_bdd_nodes)
TACTIX_STATS_U64(signed_cardinality_proof_additions)
TACTIX_STATS_U64(signed_cardinality_proof_budget_exhausted)
TACTIX_STATS_U64(signed_cardinality_proof_unsat)
TACTIX_STATS_U64(signed_cardinality_proof_interrupted)
TACTIX_STATS_U64(local_search_hints_offered)
TACTIX_STATS_U64(local_search_hints_replaced)
TACTIX_STATS_U64(local_search_hint_phases_offered)
TACTIX_STATS_U64(local_search_hints_applied)
TACTIX_STATS_U64(local_search_hints_stale)
TACTIX_STATS_U64(local_search_target_install_attempts)
TACTIX_STATS_U64(local_search_target_installs)
TACTIX_STATS_U64(local_search_target_eligible_phases)
TACTIX_STATS_U64(local_search_target_changed_phases)
TACTIX_STATS_U64(local_search_target_matching_phases)
TACTIX_STATS_U64(local_search_target_replaced_phases)
TACTIX_STATS_U64(local_search_target_overwritten_native)
TACTIX_STATS_U64(local_search_target_overwritten_rephase)
TACTIX_STATS_U64(local_search_target_overwritten_walk)
TACTIX_STATS_U64(local_search_target_decisions)
TACTIX_STATS_U64(local_search_target_decision_matches)
TACTIX_STATS_U64(local_search_target_decision_overrides)
TACTIX_STATS_U64(semantic_variable_candidates_published)
TACTIX_STATS_U64(semantic_variable_candidate_existing)
TACTIX_STATS_U64(semantic_variable_candidate_introduced)
TACTIX_STATS_U64(semantic_variable_advice_attempts)
TACTIX_STATS_U64(semantic_variable_advice_applied)
TACTIX_STATS_U64(semantic_variable_advice_invalid)
TACTIX_STATS_U64(semantic_variable_advice_stale)
TACTIX_STATS_U64(semantic_variable_advice_resource_exhausted)
TACTIX_STATS_U64(semantic_variable_advice_inapplicable)
TACTIX_STATS_U64(semantic_variable_advice_variables)
TACTIX_STATS_U64(semantic_variable_advice_priority)
TACTIX_STATS_U64(walk_seed_imports)
TACTIX_STATS_U64(walk_seed_imported_phases)
TACTIX_STATS_U64(walk_seed_steps)
TACTIX_STATS_U64(walk_seed_budget_exhausted)
TACTIX_STATS_U64(walk_seed_interrupted)
TACTIX_STATS_INIT(walk_seed_ordinary_unsat, UINT64_MAX)
TACTIX_STATS_INIT(walk_seed_candidate_unsat, UINT64_MAX)
TACTIX_STATS_U64(walk_seed_improved)
TACTIX_STATS_U64(walk_seed_exported)
TACTIX_STATS_U64(walk_preparation_steps) TACTIX_STATS_U64(walk_search_steps)
TACTIX_STATS_U64(walk_preparation_budget_exhausted)
TACTIX_STATS_U64(walk_search_budget_exhausted)
TACTIX_STATS_U64(walk_seed_admitted) TACTIX_STATS_U64(walk_seed_rejected)
TACTIX_STATS_U64(walks)         // S6 Walk applications (policy op)
TACTIX_STATS_U64(walk_flips)    // literal flips
TACTIX_STATS_U64(walk_steps)    // native occurrence visits plus explicit seed work
TACTIX_STATS_U64(walk_improved) // walks that lowered the unsatisfied count
TACTIX_STATS_INIT(walk_minimum, UINT64_MAX)
TACTIX_STATS_U64(warmups) // root decision/propagation sweeps before Walk
TACTIX_STATS_U64(warming_decisions)
TACTIX_STATS_U64(warming_conflicts)
TACTIX_STATS_U64(warming_propagations)
TACTIX_STATS_U64(promoted)     // glue promotions on use (kissat deduce.c)
TACTIX_STATS_U64(learned)
TACTIX_STATS_U64(learned_units)
TACTIX_STATS_U64(learned_lits)     // sum of FINAL learned clause sizes
TACTIX_STATS_U64(learned_glue_sum) // sum of learned glues
TACTIX_STATS_U64(minimized) // literals removed by clause minimization
TACTIX_STATS_U64(minimize_depth_cutoffs) // recursive walks stopped at Kissat cap
TACTIX_STATS_U64(shrunken)  // literals removed by all-UIP block shrinking
TACTIX_STATS_U64(otfs_strengthened) // reasons strengthened on the fly
TACTIX_STATS_U64(otfs_subsumed)     // conflicts subsumed by the resolvent
TACTIX_STATS_U64(eager_tried)    // containment tests run (kissat eagertried)
TACTIX_STATS_U64(eager_subsumed) // deletions (kissat eagerly_subsumed)
TACTIX_STATS_U64(eager_reason_skipped) // subsumed but kept — still a trail reason
TACTIX_STATS_U64(shrink_promoted)
TACTIX_STATS_U64(tumble_seeded)
TACTIX_STATS_ARRAY(used_glue, [2][MAX_GLUE_USED + 1])
TACTIX_STATS_U64(retiered)           // dynamic tier recomputations
TACTIX_STATS_U64(tier2_band_dynamic)
TACTIX_STATS_U64(tier2_band_static)
TACTIX_STATS_U64(reduce_scans)
TACTIX_STATS_U64(reduce_scan_words)
TACTIX_STATS_U64(reduce_skippable_words)
TACTIX_STATS_U64(reason_bumped)      // extra vars marked by reason-side bumping
TACTIX_STATS_U64(reason_bump_rounds) // conflicts the reason-side pass ran on
TACTIX_STATS_U64(reason_bump_depth2) // rounds allowed to recurse in stable mode
TACTIX_STATS_U64(reason_bump_aborted) // rounds rolled back at the mark limit
TACTIX_STATS_U64(api_decisions) // externally chosen branches (N1)
TACTIX_STATS_U64(input_restated) // normalized input clauses restated by fresh id
TACTIX_STATS_U64(decisions_extension) // decisions on a Factor extension variable
TACTIX_STATS_U64(decisions_substituted) // ... on one Substitute rewrote away
TACTIX_STATS_MUTABLE(rule_interrupted) // applications aborted on the
TACTIX_STATS_MUTABLE(stop_fuzz_fires)  // stops reported by the stress knob,
TACTIX_STATS_U64(bumps)           // bump events (both modes, both channels)
TACTIX_STATS_U64(bumps_extension) // ...of which landed on an extension var
TACTIX_STATS_U64(bumps_ext_reason_only) // ...of those, reason-side ONLY
TACTIX_STATS_U64(restarts)
TACTIX_STATS_U64(restarts_reused) // restarts that kept a trail prefix
TACTIX_STATS_U64(reductions)
TACTIX_STATS_U64(deleted) // learned clauses deleted by Reduce
TACTIX_STATS_U64(variable_compactions)
TACTIX_STATS_U64(compacted_variables)
TACTIX_STATS_U64(binaries_deleted) // binary clauses deleted (kernel_delete_binary)
TACTIX_STATS_U64(forward_subsumptions) // SubsumeForward rounds
TACTIX_STATS_U64(forward_checked)  // SubsumeForward candidates checked
TACTIX_STATS_U64(forward_visits)
TACTIX_STATS_U64(forward_subsumed) // clauses deleted by SubsumeForward
TACTIX_STATS_U64(strengthen_rounds)  // Strengthen rounds
TACTIX_STATS_U64(strengthen_checked) // Strengthen candidate checks
TACTIX_STATS_U64(strengthen_visits)  // Kissat-compatible fused forward_steps
TACTIX_STATS_U64(strengthened)       // clauses strengthened (add+delete)
TACTIX_STATS_U64(strengthen_subsumed) // deletions by Strengthen's fused round
TACTIX_STATS_U64(budget_stops_c1) // SubsumeForward rounds cut by `work_budget`
TACTIX_STATS_U64(budget_stops_c2) // Strengthen rounds cut between candidates
TACTIX_STATS_U64(subsume_interrupted)
TACTIX_STATS_U64(strengthen_interrupted)
TACTIX_STATS_U64(flushed) // root-satisfied deletions + root-falsified literal
TACTIX_STATS_U64(sparse_collections) // startup whole-arena root collections
TACTIX_STATS_U64(sparse_collected)   // clauses deleted or rewritten by them
TACTIX_STATS_U64(vivify_rounds)  // Vivify rounds
TACTIX_STATS_U64(vivify_checked) // Vivify candidates probed
TACTIX_STATS_U64(vivified)       // clauses distilled by Vivify (add+delete)
TACTIX_STATS_U64(vivify_deleted) // root-satisfied deletions by Vivify
TACTIX_STATS_U64(vivify_flushed) // scheduled clauses removed by prefix subsumption
TACTIX_STATS_U64(vivify_reused)     // levels taken from the retained trail
TACTIX_STATS_U64(vivify_probes)     // assumptions made; reuse rate = reused/both
TACTIX_STATS_U64(vivify_reuse_full) // candidates wholly covered by the prefix
TACTIX_STATS_U64(vivify_tier3)      // tier3 candidates probed; 0 = that half of
TACTIX_STATS_U64(vivify_prioritized)   // candidates carrying the leftover bit
TACTIX_STATS_U64(vivify_reprioritized) // rounds whose leftover set was empty
TACTIX_STATS_U64(vivify_polls)      // kernel_terminating evaluations at the site
TACTIX_STATS_U64(vivify_terminated) // rounds cut short by one
TACTIX_STATS_U64(vivify_subsumed)   // candidates deleted by a reason clause
TACTIX_STATS_U64(vivify_promoted)   // redundant subsumers made irredundant
TACTIX_STATS_U64(vivify_shrunken)   // strict deduction rewrites
TACTIX_STATS_U64(vivify_implied)    // redundant independently-implied deletes
TACTIX_STATS_U64(vivify_asymmetric) // irredundant asymmetric-tautology deletes
TACTIX_STATS_U64(vivify_instantiated) // successful last-literal instantiations
TACTIX_STATS_U64(vivify_noop)       // terminal implied/instantiation declines
TACTIX_STATS_U64(probe_rounds) // Probe rounds
TACTIX_STATS_U64(probed)       // probe assumptions made
TACTIX_STATS_U64(probe_failed) // failed literals -> root units derived
TACTIX_STATS_U64(backbone_computations) // canonical batch applications
TACTIX_STATS_U64(backbone_rounds) // Backbone rounds
TACTIX_STATS_U64(backbone_probed) // binary probe assumptions made
TACTIX_STATS_U64(backbone_failed) // failed literals -> root units derived
TACTIX_STATS_U64(transitive_rounds)  // TransitiveReduce rounds
TACTIX_STATS_U64(transitive_checked) // candidate edges walked
TACTIX_STATS_U64(bce_rounds)         // BCE rounds that deleted something
TACTIX_STATS_U64(bce_deleted)        // blocked clauses deleted (witnessed)
TACTIX_STATS_U64(probe_ticks)        // Probe's own propagation ticks
TACTIX_STATS_U64(backbone_ticks)     // Backbone's own
TACTIX_STATS_U64(vivify_ticks)       // Vivify's own
TACTIX_STATS_U64(instantiate_ticks)  // Instantiate's own
TACTIX_STATS_U64(bce_ticks)          // occurrences + pairs scanned — BCE's budget
TACTIX_STATS_U64(instantiate_rounds)  // Instantiate rounds that strengthened
TACTIX_STATS_U64(instantiate_checked) // (clause, literal) pairs probed
TACTIX_STATS_U64(instantiated)        // literals removed by Instantiate

TACTIX_STATS_U64(cce_rounds)  // rounds attempted; nothing may read it — a
TACTIX_STATS_U64(cce_tried)   // candidate clauses extended
TACTIX_STATS_U64(cce_deleted) // clauses eliminated
TACTIX_STATS_U64(cce_blocked) // ... of which by the blocked exit (witnessed)
TACTIX_STATS_U64(cce_ata)     // ... of which by the asymmetric-tautology exit
TACTIX_STATS_U64(cce_ala)     // asymmetric literals added
TACTIX_STATS_U64(cce_cla)     // covered literals added (CCE's own mechanism)
TACTIX_STATS_U64(cce_ticks)   // clause literals scanned — CCE's budget currency

TACTIX_STATS_U64(sweep_rounds)       // detection rounds run
TACTIX_STATS_U64(sweep_environments) // environments built AND handed to the solver
TACTIX_STATS_U64(sweep_env_clauses)  // clauses encoded into them
TACTIX_STATS_U64(sweep_solves)       // sub-solver solve() calls
TACTIX_STATS_U64(sweep_candidates)   // candidate-pair directions actually solved
TACTIX_STATS_U64(sweep_swept)        // scheduled variables swept (kissat `swept`)
TACTIX_STATS_U64(sweep_equivalences) // pairs PROVED equivalent
TACTIX_STATS_U64(sweep_units)        // root units proved by local environments
TACTIX_STATS_U64(sweep_backbone_candidates)
TACTIX_STATS_U64(sweep_backbone_flipped)
TACTIX_STATS_U64(sweep_binaries)   // equivalence binaries ADDED with a proof
TACTIX_STATS_U64(sweep_not_rup)    // equivalences declined (not RUP)
TACTIX_STATS_U64(sweep_general)    // pairs emitted via the general proof path
TACTIX_STATS_U64(random_sequences) // random-decision sequences started
TACTIX_STATS_U64(random_decisions) // decisions actually taken at random
TACTIX_STATS_U64(sweep_known)      // pairs the formula already states as binaries
TACTIX_STATS_U64(sweep_env_unsat)  // environments refuted outright (F is UNSAT)
TACTIX_STATS_U64(sweep_budget)     // solves abandoned on the tick budget
TACTIX_STATS_U64(sweep_ticks)      // Sweep scan+sub-solver diagnostic
TACTIX_STATS_U64(sweep_env_lits)      // literal occurrences encoded
TACTIX_STATS_U64(sweep_env_false)     // of those, root-FALSE occurrences
TACTIX_STATS_U64(sweep_env_freefalse) // of those, with no unit id => free var
TACTIX_STATS_U64(sweep_env_stripped)  // root-false occurrences REMOVED
TACTIX_STATS_U64(sweep_strip_cited)   // ... with their root units cited in an
TACTIX_STATS_U64(sweep_env_units)     // unit clauses added alongside (off arm)
TACTIX_STATS_U64(sweep_ticks_dedup)   // the environment's clause-key scan
TACTIX_STATS_U64(sweep_ticks_sub)     // Sweep's scheduled effort currency
TACTIX_STATS_U64(sweep_ticks_repr)    // Tactix arena words rebuilt after emissions
TACTIX_STATS_U64(sweep_occ_f)         // env vars' large-clause degree in F
TACTIX_STATS_U64(sweep_occ_env)       // ... and inside the environment
TACTIX_STATS_U64(sweep_vars_closed)   // env vars holding their WHOLE degree
TACTIX_STATS_U64(sweep_vars_total)    // env vars
TACTIX_STATS_U64(sweep_cand_seeded)   // candidates entering the loop
TACTIX_STATS_U64(sweep_cand_refined)  // candidates dropped by model refinement
TACTIX_STATS_U64(sweep_flip_attempts) // partition literals tested by model flip
TACTIX_STATS_U64(sweep_flipped)       // successful model flips
TACTIX_STATS_U64(sweep_dir_sat)       // candidate directions that MODELLED
TACTIX_STATS_U64(sweep_dir_unsat)     // candidate directions that REFUTED
TACTIX_STATS_U64(sweep_scheduled)     // eligible seeds placed in canonical tails
TACTIX_STATS_U64(sweep_rescheduled)   // equivalence representatives moved next
TACTIX_STATS_U64(sweep_resumed)       // seeds consumed from a retained tail
TACTIX_STATS_U64(sweep_completed)     // canonical tails exhausted
TACTIX_STATS_U32(sweep_limit_vars)    // last canonical environment limits
TACTIX_STATS_U32(sweep_limit_clauses)
TACTIX_STATS_U32(sweep_limit_depth)

TACTIX_STATS_U64(ternary_rounds)     // HyperTernaryRes rounds that added clauses
TACTIX_STATS_U64(ternary_resolvents) // ternary resolvents added (redundant)
TACTIX_STATS_U64(hbr_rounds)     // X6 HyperBinaryRes: canonical rounds entered
TACTIX_STATS_U64(hbr_probes)     // root literals closed under propagation
TACTIX_STATS_U64(hbr_binaries)   // resolvents emitted
TACTIX_STATS_U64(hbr_subsumed)   // reasons deleted because the resolvent subsumed them
TACTIX_STATS_U64(hbr_declined)   // resolvents the RUP replay could not re-derive
TACTIX_STATS_U64(hbr_interrupts) // termination polls that fired mid-round
TACTIX_STATS_U64(hbr_ticks)      // private meter (see Kernel::tick_sink)
TACTIX_STATS_U64(ternary_binaries)   // binary resolvents added (permanent)
TACTIX_STATS_U64(ternary_ticks)      // occurrence entries + pairs examined
TACTIX_STATS_U64(transitive_ticks)   // cache lines scanned by the walks (kissat
TACTIX_STATS_U64(transitive_deleted) // transitively redundant binaries deleted
TACTIX_STATS_U64(transitive_units)   // failed-source units committed by V4
TACTIX_STATS_U64(transitive_polls)   // kernel_terminating evaluations at the site
TACTIX_STATS_U64(transitive_terminated) // dst walks cut by an interrupt
TACTIX_STATS_U64(substitute_rounds)  // Substitute rounds — detection passes
TACTIX_STATS_MUTABLE(substitute_ticks) // binary-watch scan cache lines
TACTIX_STATS_U64(substitute_vars)    // variables substituted away
TACTIX_STATS_U64(substitute_sccs)
TACTIX_STATS_U64(substitute_scc_lits)
TACTIX_STATS_U64(substitute_hazard_skips)
TACTIX_STATS_U64(substitute_hazard_partial)
TACTIX_STATS_U64(substitute_hazard_lits)
TACTIX_STATS_U64(deactivations)  // counted at the site, so 0 means "never fired"
TACTIX_STATS_U64(deactivation_repeats) // already disconnected retirement reports
TACTIX_STATS_U64(reactivations)  // ... of those, later re-inserted into a ranking
TACTIX_STATS_U64(conflict_reused) // conflicts resolved by reusing the conflict as
TACTIX_STATS_U64(jumped_reasons)  // binary reasons replaced by their resolvent
TACTIX_STATS_U64(jump_reason_classifications)
TACTIX_STATS_U64(jump_reason_bigbig)
TACTIX_STATS_U64(conflict_watch_repaired) // ... of those, needing watch repair
TACTIX_STATS_U64(binprop_scans)
TACTIX_STATS_U64(binprop_ordered)
// Dense occurrence arena: slices moved to the arena tail because a push
// outgrew their capacity, and defragmentations that reclaimed the abandoned
// images. Both are reachability readouts for the arena's growth policy.
TACTIX_STATS_U64(dense_slice_moves)
TACTIX_STATS_U64(dense_defrags)
TACTIX_STATS_U64(binprop_flagged)
TACTIX_STATS_U64(binprop_words)
TACTIX_STATS_U64(binprop_skipped)
TACTIX_STATS_U64(binprop_large)
TACTIX_STATS_U64(chronological)  // chrono backtrack targets taken
TACTIX_STATS_U64(backjump_stale)
TACTIX_STATS_U64(backjumps)      // completed ConflictAnalysis backjump tails
TACTIX_STATS_U64(inprocess_phases)   // policy inprocessing phases (a root descent
TACTIX_STATS_U64(parity_consequence_attempts)
TACTIX_STATS_U64(parity_consequence_clauses)
TACTIX_STATS_U64(parity_consequence_equations)
TACTIX_STATS_U64(parity_consequence_selection_work)
TACTIX_STATS_U64(parity_consequence_preflight_additions)
TACTIX_STATS_U64(parity_consequence_additions)
TACTIX_STATS_U64(parity_consequence_deletions)
TACTIX_STATS_U64(parity_consequence_helpers)
TACTIX_STATS_U64(parity_consequence_published)
TACTIX_STATS_U64(affine_consequence_attempts)
TACTIX_STATS_U64(affine_consequence_applied)
TACTIX_STATS_U64(affine_consequence_no_structure)
TACTIX_STATS_U64(affine_consequence_invalid)
TACTIX_STATS_U64(affine_consequence_inapplicable)
TACTIX_STATS_U64(affine_consequence_resource_exhausted)
TACTIX_STATS_U64(affine_consequence_interrupted)
TACTIX_STATS_U64(affine_consequence_unsupported_proof)
TACTIX_STATS_U64(affine_consequence_stale)
TACTIX_STATS_U64(affine_consequence_already_published)
TACTIX_STATS_U64(affine_consequence_snapshot_clauses)
TACTIX_STATS_U64(affine_consequence_snapshot_literals)
TACTIX_STATS_U64(affine_consequence_snapshot_work)
TACTIX_STATS_U64(affine_consequence_equations)
TACTIX_STATS_U64(affine_consequence_consequences)
TACTIX_STATS_U64(affine_consequence_recognition_work)
TACTIX_STATS_U64(affine_consequence_selected_sources)
TACTIX_STATS_U64(affine_consequence_selected_premises)
TACTIX_STATS_U64(affine_consequence_proof_work)
TACTIX_STATS_U64(affine_consequence_preflight_additions)
TACTIX_STATS_U64(affine_consequence_preflight_deletions)
TACTIX_STATS_U64(affine_consequence_additions)
TACTIX_STATS_U64(affine_consequence_deletions)
TACTIX_STATS_U64(affine_consequence_published)
TACTIX_STATS_U64(affine_consequence_journal_work)
TACTIX_STATS_U64(affine_consequence_journal_bytes)
TACTIX_STATS_U64(affine_consequence_candidate_variables)
TACTIX_STATS_U64(affine_consequence_basis_work)
TACTIX_STATS_U64(affine_consequence_rows)
TACTIX_STATS_U64(affine_consequence_definitions)
TACTIX_STATS_U64(affine_consequence_extent_growth)
TACTIX_STATS_U64(affine_consequence_session_work)
TACTIX_STATS_U64(affine_consequence_excluded_rows)
TACTIX_STATS_U64(affine_consequence_transaction_keys)
TACTIX_STATS_U64(affine_consequence_committed_keys)
TACTIX_STATS_U64(affine_matrix_init_attempts)
TACTIX_STATS_U64(affine_matrix_init_applied)
TACTIX_STATS_U64(affine_matrix_init_applied_partial)
TACTIX_STATS_U64(affine_matrix_init_incomplete_image)
TACTIX_STATS_U64(affine_matrix_init_incomplete_matrix)
TACTIX_STATS_U64(affine_matrix_init_exhausted)
TACTIX_STATS_U64(affine_matrix_init_interrupted)
TACTIX_STATS_U64(affine_matrix_init_snapshot_work)
TACTIX_STATS_U64(affine_matrix_init_recognition_work)
TACTIX_STATS_U64(affine_matrix_init_matrix_work)
TACTIX_STATS_U64(affine_matrix_init_premise_retention_work)
TACTIX_STATS_U64(affine_matrix_init_root_unit_work)
TACTIX_STATS_U64(affine_matrix_alias_work)
TACTIX_STATS_U64(affine_matrix_alias_proof_additions)
TACTIX_STATS_U64(affine_matrix_service_calls)
TACTIX_STATS_U64(affine_matrix_service_lifecycle_work)
TACTIX_STATS_U64(affine_matrix_service_applied)
TACTIX_STATS_U64(affine_matrix_service_no_event)
TACTIX_STATS_U64(affine_matrix_service_duplicate)
TACTIX_STATS_U64(affine_matrix_service_exhausted)
TACTIX_STATS_U64(affine_matrix_service_interrupted)
TACTIX_STATS_U64(affine_matrix_service_stale)
TACTIX_STATS_U64(affine_matrix_service_trail_capture_work)
TACTIX_STATS_U64(affine_matrix_service_sync_work)
TACTIX_STATS_TIMER(affine_matrix_service_sync_nanos)
TACTIX_STATS_TIMER(affine_matrix_service_publication_nanos)
TACTIX_STATS_U64(affine_matrix_service_extension_hits)
TACTIX_STATS_U64(affine_matrix_service_rebuilds)
TACTIX_STATS_U64(affine_matrix_service_rebuild_backtrack)
TACTIX_STATS_U64(affine_matrix_service_rebuild_reassignment)
TACTIX_STATS_U64(affine_matrix_service_rebuild_partial)
TACTIX_STATS_U64(affine_matrix_service_retained_assignments)
TACTIX_STATS_U64(affine_matrix_service_conditioned_assignments)
TACTIX_STATS_U64(affine_matrix_service_session_work)
TACTIX_STATS_U64(affine_matrix_service_planner_work)
TACTIX_STATS_U64(affine_matrix_service_revalidation_work)
TACTIX_STATS_U64(affine_matrix_service_proof_additions)
TACTIX_STATS_U64(affine_matrix_service_proof_deletions)
TACTIX_STATS_U64(affine_matrix_service_proof_cache_hits)
TACTIX_STATS_U64(affine_matrix_service_proof_cache_inserts)
TACTIX_STATS_U64(affine_matrix_service_helper_extent)
TACTIX_STATS_U64(affine_matrix_service_journal_bytes)
TACTIX_STATS_U64(exact_cover_extension_attempts)
TACTIX_STATS_U64(exact_cover_extension_applied)
TACTIX_STATS_U64(exact_cover_extension_no_structure)
TACTIX_STATS_U64(exact_cover_extension_already_published)
TACTIX_STATS_U64(exact_cover_extension_invalid)
TACTIX_STATS_U64(exact_cover_extension_inapplicable)
TACTIX_STATS_U64(exact_cover_extension_resource_exhausted)
TACTIX_STATS_U64(exact_cover_extension_interrupted)
TACTIX_STATS_U64(exact_cover_extension_unsupported_proof)
TACTIX_STATS_U64(exact_cover_extension_stale)
TACTIX_STATS_U64(exact_cover_extension_source_clauses)
TACTIX_STATS_U64(exact_cover_extension_source_literals)
TACTIX_STATS_U64(exact_cover_extension_recognition_work)
TACTIX_STATS_U64(exact_cover_extension_revalidation_work)
TACTIX_STATS_U64(exact_cover_extension_definitions)
TACTIX_STATS_U64(exact_cover_extension_additions)
TACTIX_STATS_U64(exact_cover_extension_literal_occurrences)
TACTIX_STATS_U64(exact_cover_extension_journal_bytes)
TACTIX_STATS_U64(exact_cover_extension_extent_growth)
TACTIX_STATS_U64(exact_cover_extension_candidate_variables)
TACTIX_STATS_U64(independent_set_cover_attempts)
TACTIX_STATS_U64(independent_set_cover_applied)
TACTIX_STATS_U64(independent_set_cover_no_structure)
TACTIX_STATS_U64(independent_set_cover_duplicate)
TACTIX_STATS_U64(independent_set_cover_invalid)
TACTIX_STATS_U64(independent_set_cover_inapplicable)
TACTIX_STATS_U64(independent_set_cover_resource_exhausted)
TACTIX_STATS_U64(independent_set_cover_interrupted)
TACTIX_STATS_U64(independent_set_cover_unsupported_proof)
TACTIX_STATS_U64(independent_set_cover_stale)
TACTIX_STATS_U64(independent_set_cover_source_clauses)
TACTIX_STATS_U64(independent_set_cover_source_literals)
TACTIX_STATS_U64(independent_set_cover_recognition_work)
TACTIX_STATS_U64(independent_set_cover_source_scan_work)
TACTIX_STATS_U64(independent_set_cover_theory_recognition_work)
TACTIX_STATS_U64(independent_set_cover_proof_work)
TACTIX_STATS_U64(independent_set_cover_planned_proof_additions)
TACTIX_STATS_U64(independent_set_cover_proof_additions)
TACTIX_STATS_U64(independent_set_cover_proof_deletions)
TACTIX_STATS_U64(independent_set_cover_proof_literal_occurrences)
TACTIX_STATS_U64(independent_set_cover_journal_work)
TACTIX_STATS_U64(independent_set_cover_published)
TACTIX_STATS_U64(independent_set_cover_candidate_variables)
TACTIX_STATS_U64(gf3_affine_attempts)
TACTIX_STATS_U64(gf3_affine_applied)
TACTIX_STATS_U64(gf3_affine_no_structure)
TACTIX_STATS_U64(gf3_affine_no_consequence)
TACTIX_STATS_U64(gf3_affine_already_published)
TACTIX_STATS_U64(gf3_affine_invalid)
TACTIX_STATS_U64(gf3_affine_inapplicable)
TACTIX_STATS_U64(gf3_affine_resource_exhausted)
TACTIX_STATS_U64(gf3_affine_interrupted)
TACTIX_STATS_U64(gf3_affine_unsupported_proof)
TACTIX_STATS_U64(gf3_affine_stale)
TACTIX_STATS_U64(gf3_affine_source_clauses)
TACTIX_STATS_U64(gf3_affine_source_literals)
TACTIX_STATS_U64(gf3_affine_equations)
TACTIX_STATS_U64(gf3_affine_owner_capture_work)
TACTIX_STATS_U64(gf3_affine_recognition_work)
TACTIX_STATS_U64(gf3_affine_planning_work)
TACTIX_STATS_U64(gf3_affine_duplicate_scan_work)
TACTIX_STATS_U64(gf3_affine_revalidation_work)
TACTIX_STATS_U64(gf3_affine_proof_work)
TACTIX_STATS_U64(gf3_affine_planned_additions)
TACTIX_STATS_U64(gf3_affine_duplicate_additions)
TACTIX_STATS_U64(gf3_affine_emitted_additions)
TACTIX_STATS_U64(gf3_affine_proof_literal_occurrences)
TACTIX_STATS_U64(gf3_affine_arena_words)
TACTIX_STATS_U64(gf3_affine_journal_work)
TACTIX_STATS_U64(gf3_affine_journal_bytes)
TACTIX_STATS_U64(gf3_affine_session_work)
TACTIX_STATS_U64(gf3_affine_session_commits)
TACTIX_STATS_U64(gf3_affine_candidate_variables)
TACTIX_STATS_U64(direct_csp_support_attempts)
TACTIX_STATS_U64(direct_csp_support_applied)
TACTIX_STATS_U64(direct_csp_support_no_structure)
TACTIX_STATS_U64(direct_csp_support_no_consequence)
TACTIX_STATS_U64(direct_csp_support_already_published)
TACTIX_STATS_U64(direct_csp_support_invalid)
TACTIX_STATS_U64(direct_csp_support_inapplicable)
TACTIX_STATS_U64(direct_csp_support_resource_exhausted)
TACTIX_STATS_U64(direct_csp_support_interrupted)
TACTIX_STATS_U64(direct_csp_support_unsupported_proof)
TACTIX_STATS_U64(direct_csp_support_stale)
TACTIX_STATS_U64(direct_csp_support_source_clauses)
TACTIX_STATS_U64(direct_csp_support_source_literals)
TACTIX_STATS_U64(direct_csp_support_owner_capture_work)
TACTIX_STATS_U64(direct_csp_support_recognition_work)
TACTIX_STATS_U64(direct_csp_support_source_scan_work)
TACTIX_STATS_U64(direct_csp_support_planning_work)
TACTIX_STATS_U64(direct_csp_support_revalidation_work)
TACTIX_STATS_U64(direct_csp_support_proof_work)
TACTIX_STATS_U64(direct_csp_support_planned_additions)
TACTIX_STATS_U64(direct_csp_support_emitted_additions)
TACTIX_STATS_U64(direct_csp_support_proof_literal_occurrences)
TACTIX_STATS_U64(direct_csp_support_journal_work)
TACTIX_STATS_U64(direct_csp_support_journal_bytes)
TACTIX_STATS_U64(direct_csp_support_extent_growth)
TACTIX_STATS_U64(direct_csp_support_session_work)
TACTIX_STATS_U64(direct_csp_support_session_commits)
TACTIX_STATS_U64(direct_csp_support_owner_commits)
TACTIX_STATS_U64(direct_csp_support_candidate_variables)
TACTIX_STATS_U64(direct_csp_support_introduced_variables)
TACTIX_STATS_U64(nary_csp_equality_attempts)
TACTIX_STATS_U64(nary_csp_equality_applied)
TACTIX_STATS_U64(nary_csp_equality_no_structure)
TACTIX_STATS_U64(nary_csp_equality_no_consequence)
TACTIX_STATS_U64(nary_csp_equality_already_published)
TACTIX_STATS_U64(nary_csp_equality_invalid)
TACTIX_STATS_U64(nary_csp_equality_inapplicable)
TACTIX_STATS_U64(nary_csp_equality_resource_exhausted)
TACTIX_STATS_U64(nary_csp_equality_interrupted)
TACTIX_STATS_U64(nary_csp_equality_unsupported_proof)
TACTIX_STATS_U64(nary_csp_equality_stale)
TACTIX_STATS_U64(nary_csp_equality_source_clauses)
TACTIX_STATS_U64(nary_csp_equality_source_literals)
TACTIX_STATS_U64(nary_csp_equality_recognition_work)
TACTIX_STATS_U64(nary_csp_equality_planning_work)
TACTIX_STATS_U64(nary_csp_equality_proof_work)
TACTIX_STATS_U64(nary_csp_equality_planned_additions)
TACTIX_STATS_U64(nary_csp_equality_emitted_additions)
TACTIX_STATS_U64(nary_csp_equality_proof_literal_occurrences)
TACTIX_STATS_U64(nary_csp_equality_journal_bytes)
TACTIX_STATS_U64(nary_csp_equality_extent_growth)
TACTIX_STATS_U64(nary_csp_equality_session_commits)
TACTIX_STATS_U64(nary_csp_equality_candidate_variables)
TACTIX_STATS_U64(nary_csp_equality_introduced_variables)
TACTIX_STATS_U64(nary_csp_equality_amo_attempts)
TACTIX_STATS_U64(nary_csp_equality_amo_applied)
TACTIX_STATS_U64(nary_csp_equality_amo_missing_equality)
TACTIX_STATS_U64(nary_csp_equality_amo_stale)
TACTIX_STATS_U64(nary_csp_equality_amo_duplicate)
TACTIX_STATS_U64(nary_csp_equality_amo_declined)
TACTIX_STATS_U64(nary_csp_equality_amo_recognition_work)
TACTIX_STATS_U64(nary_csp_equality_amo_proof_work)
TACTIX_STATS_U64(nary_csp_equality_amo_proof_additions)
TACTIX_STATS_U64(nary_csp_equality_amo_proof_deletions)
TACTIX_STATS_U64(nary_csp_equality_amo_proof_literal_occurrences)
TACTIX_STATS_U64(nary_csp_equality_amo_journal_bytes)
TACTIX_STATS_U64(nary_csp_equality_amo_binary_clauses)
TACTIX_STATS_U64(nary_csp_equality_amo_session_commits)
TACTIX_STATS_U64(nary_csp_equality_transitivity_invalid)
TACTIX_STATS_U64(nary_csp_equality_transitivity_inapplicable)
TACTIX_STATS_U64(nary_csp_equality_transitivity_resource_exhausted)
TACTIX_STATS_U64(nary_csp_equality_transitivity_interrupted)
TACTIX_STATS_U64(nary_csp_equality_transitivity_unsupported_proof)
TACTIX_STATS_U64(nary_csp_equality_transitivity_attempts)
TACTIX_STATS_U64(nary_csp_equality_transitivity_applied)
TACTIX_STATS_U64(nary_csp_equality_transitivity_missing_equality)
TACTIX_STATS_U64(nary_csp_equality_transitivity_stale)
TACTIX_STATS_U64(nary_csp_equality_transitivity_duplicate)
TACTIX_STATS_U64(nary_csp_equality_transitivity_declined)
TACTIX_STATS_U64(nary_csp_equality_transitivity_recognition_work)
TACTIX_STATS_U64(nary_csp_equality_transitivity_proof_work)
TACTIX_STATS_U64(nary_csp_equality_transitivity_proof_additions)
TACTIX_STATS_U64(nary_csp_equality_transitivity_proof_deletions)
TACTIX_STATS_U64(nary_csp_equality_transitivity_proof_literal_occurrences)
TACTIX_STATS_U64(nary_csp_equality_transitivity_journal_bytes)
TACTIX_STATS_U64(nary_csp_equality_transitivity_ternary_clauses)
TACTIX_STATS_U64(nary_csp_equality_transitivity_session_commits)
TACTIX_STATS_U64(nary_csp_equality_transitivity_planning_work)
TACTIX_STATS_U64(nary_csp_equality_transitivity_preparation_work)
TACTIX_STATS_U64(nary_csp_equality_transitivity_source_clauses)
TACTIX_STATS_U64(nary_csp_equality_transitivity_source_literals)
TACTIX_STATS_U64(nary_csp_equality_transitivity_session_bytes)

TACTIX_STATS_U64(direct_php_attempts) TACTIX_STATS_U64(direct_php_applied)
TACTIX_STATS_U64(direct_php_invalid) TACTIX_STATS_U64(direct_php_inapplicable)
TACTIX_STATS_U64(direct_php_not_recognized)
TACTIX_STATS_U64(direct_php_malformed)
TACTIX_STATS_U64(direct_php_ambiguous)
TACTIX_STATS_U64(direct_php_resource_exhausted)
TACTIX_STATS_U64(direct_php_interrupted)
TACTIX_STATS_U64(direct_php_unsupported_proof)
TACTIX_STATS_U64(direct_php_unsupported_proof_split)
TACTIX_STATS_U64(direct_php_stale)
TACTIX_STATS_U64(direct_php_already_applied)
TACTIX_STATS_U64(direct_php_replay_mismatch)
TACTIX_STATS_U64(direct_php_prepared)
TACTIX_STATS_U64(direct_php_snapshot_clauses)
TACTIX_STATS_U64(direct_php_snapshot_literals)
TACTIX_STATS_U64(direct_php_snapshot_work)
TACTIX_STATS_U64(direct_php_recognition_work)
TACTIX_STATS_U64(direct_php_replay_work)
TACTIX_STATS_U64(direct_php_proof_additions)
TACTIX_STATS_U64(direct_php_proof_rup_additions)
TACTIX_STATS_U64(direct_php_proof_rat_additions)
TACTIX_STATS_U64(direct_php_proof_deletions)
TACTIX_STATS_U64(direct_php_proof_addition_literals)
TACTIX_STATS_U64(direct_php_proof_deletion_literals)
TACTIX_STATS_U64(direct_php_proof_fresh_variables)
TACTIX_STATS_U64(direct_php_proof_events)
TACTIX_STATS_U64(direct_php_proof_maximum_event_width)
TACTIX_STATS_U64(gf3_gaussian_attempts) TACTIX_STATS_U64(gf3_gaussian_applied)
TACTIX_STATS_U64(gf3_gaussian_invalid)
TACTIX_STATS_U64(gf3_gaussian_inapplicable)
TACTIX_STATS_U64(gf3_gaussian_no_structure)
TACTIX_STATS_U64(gf3_gaussian_no_supported_outcome)
TACTIX_STATS_U64(gf3_gaussian_already_published)
TACTIX_STATS_U64(gf3_gaussian_exhausted)
TACTIX_STATS_U64(gf3_gaussian_interrupted)
TACTIX_STATS_U64(gf3_gaussian_stale)
TACTIX_STATS_U64(gf3_gaussian_unsupported_proof)
TACTIX_STATS_U64(gf3_gaussian_source_clauses)
TACTIX_STATS_U64(gf3_gaussian_source_literals)
TACTIX_STATS_U64(gf3_gaussian_capture_work)
TACTIX_STATS_U64(gf3_gaussian_bridge_work)
TACTIX_STATS_U64(gf3_gaussian_certificate_work)
TACTIX_STATS_U64(gf3_gaussian_validation_work)
TACTIX_STATS_U64(gf3_gaussian_revalidation_work)
TACTIX_STATS_U64(gf3_gaussian_proof_additions)
TACTIX_STATS_U64(gf3_gaussian_proof_literals)
TACTIX_STATS_U64(gf3_gaussian_installed_clauses)
TACTIX_STATS_U64(gf3_gaussian_duplicate_clauses)
TACTIX_STATS_U64(gf3_gaussian_journal_work)
TACTIX_STATS_U64(gf3_gaussian_journal_bytes)
TACTIX_STATS_U64(gf3_gaussian_session_work)
TACTIX_STATS_U64(gf3_gaussian_identity_work)
TACTIX_STATS_U64(gf3_gaussian_identity_bytes)
TACTIX_STATS_U64(gf3_gaussian_session_identity_bytes)
TACTIX_STATS_U64(gf3_gaussian_units) TACTIX_STATS_U64(gf3_gaussian_conflicts)
TACTIX_STATS_U64(gf3_general_row_attempts)
TACTIX_STATS_U64(gf3_general_row_applied)
TACTIX_STATS_U64(cardinality_search_attempts)
TACTIX_STATS_U64(cardinality_search_applied)
TACTIX_STATS_U64(gf3_general_row_invalid)
TACTIX_STATS_U64(gf3_general_row_inapplicable)
TACTIX_STATS_U64(gf3_general_row_no_structure)
TACTIX_STATS_U64(gf3_general_row_no_consequence)
TACTIX_STATS_U64(gf3_general_row_already_published)
TACTIX_STATS_U64(gf3_general_row_exhausted)
TACTIX_STATS_U64(gf3_general_row_interrupted)
TACTIX_STATS_U64(gf3_general_row_stale)
TACTIX_STATS_U64(gf3_general_row_unsupported_proof)
TACTIX_STATS_U64(gf3_general_row_source_clauses)
TACTIX_STATS_U64(gf3_general_row_source_literals)
TACTIX_STATS_U64(gf3_general_row_capture_work)
TACTIX_STATS_U64(gf3_general_row_bridge_work)
TACTIX_STATS_U64(gf3_general_row_planning_work)
TACTIX_STATS_U64(gf3_general_row_validation_work)
TACTIX_STATS_U64(gf3_general_row_duplicate_scan_work)
TACTIX_STATS_U64(gf3_general_row_revalidation_work)
TACTIX_STATS_U64(gf3_general_row_proof_additions)
TACTIX_STATS_U64(gf3_general_row_proof_literals)
TACTIX_STATS_U64(gf3_general_row_arena_words)
TACTIX_STATS_U64(gf3_general_row_journal_bytes)
TACTIX_STATS_U64(gf3_general_row_identity_bytes)
TACTIX_STATS_U64(gf3_general_row_session_work)
TACTIX_STATS_U64(gf3_general_row_session_identity_bytes)
TACTIX_STATS_U64(gf3_general_row_installed_clauses)
TACTIX_STATS_U64(gf3_general_row_semantic_variables)
TACTIX_STATS_U64(tseitin_cycle_attempts) TACTIX_STATS_U64(tseitin_cycle_applied)
TACTIX_STATS_U64(tseitin_cycle_invalid)
TACTIX_STATS_U64(tseitin_cycle_inapplicable)
TACTIX_STATS_U64(tseitin_cycle_no_structure)
TACTIX_STATS_U64(tseitin_cycle_already_applied)
TACTIX_STATS_U64(tseitin_cycle_exhausted)
TACTIX_STATS_U64(tseitin_cycle_interrupted)
TACTIX_STATS_U64(tseitin_cycle_stale)
TACTIX_STATS_U64(tseitin_cycle_unsupported_proof)
TACTIX_STATS_U64(tseitin_cycle_snapshot_clauses)
TACTIX_STATS_U64(tseitin_cycle_snapshot_literals)
TACTIX_STATS_U64(tseitin_cycle_snapshot_work)
TACTIX_STATS_U64(tseitin_cycle_recapture_work)
TACTIX_STATS_U64(tseitin_cycle_discovery_work)
TACTIX_STATS_U64(tseitin_cycle_retained_bytes)
TACTIX_STATS_U64(tseitin_cycle_admission_work)
TACTIX_STATS_U64(tseitin_cycle_planning_work)
TACTIX_STATS_U64(tseitin_cycle_transaction_work)
TACTIX_STATS_U64(tseitin_cycle_validation_ticks)
TACTIX_STATS_U64(tseitin_cycle_candidates)
TACTIX_STATS_U64(tseitin_cycle_offers)
TACTIX_STATS_U64(tseitin_cycle_identity_bytes)
TACTIX_STATS_U64(tseitin_cycle_identity_work)
TACTIX_STATS_U64(tseitin_cycle_journal_work)
TACTIX_STATS_U64(tseitin_cycle_journal_bytes)
TACTIX_STATS_U64(tseitin_cycle_session_work)
TACTIX_STATS_U64(tseitin_cycle_session_identity_bytes)
TACTIX_STATS_U64(tseitin_cycle_proof_additions)
TACTIX_STATS_U64(tseitin_cycle_proof_deletions)
TACTIX_STATS_U64(tseitin_cycle_proof_literal_occurrences)
TACTIX_STATS_U64(tseitin_cycle_proof_helpers)
TACTIX_STATS_U64(tseitin_cycle_proof_only_variables)
TACTIX_STATS_U64(tseitin_cycle_proof_witness_literals)
TACTIX_STATS_U64(tseitin_cycle_proof_routes)
TACTIX_STATS_U64(tseitin_cycle_units)
TACTIX_STATS_U64(congruence_rounds)  // Congruence rounds — gate-closure passes
TACTIX_STATS_U64(congruence_binary_rebuilds)
TACTIX_STATS_U64(congruence_unjustified)
TACTIX_STATS_U64(congruence_units) // root units from degenerate gates
TACTIX_STATS_U64(congruence_y1_found) // detections
TACTIX_STATS_U64(congruence_y1_units) // ... emitted and batched
TACTIX_STATS_U64(congruence_y1_ticks) // one complete binary-watch pass + replay
TACTIX_STATS_U64(congruence_binary_equivalences) // paired binaries imported
TACTIX_STATS_MUTABLE(congruence_gates_and)
TACTIX_STATS_MUTABLE(congruence_gates_xor)
TACTIX_STATS_MUTABLE(congruence_gates_ite)
TACTIX_STATS_U64(congruence_gates)  // gates EXTRACTED, over all applications
TACTIX_STATS_U64(congruence_merges) // equivalences MERGED, over all applications
TACTIX_STATS_MUTABLE(congruence_ticks)
TACTIX_STATS_MUTABLE(congruence_budget_stops) // rounds cut by `work_budget`
TACTIX_STATS_U64(factorings)         // Factor/BVA applications — grids replaced
TACTIX_STATS_U64(factor_large_remainders) // remainders offered by the widened
TACTIX_STATS_U64(factor_remainders_materialized) // immutable clause remainders
TACTIX_STATS_U64(factor_remainders_reused) // ... served from the dense watch
TACTIX_STATS_U64(factor_demotions)    // demotion calls that moved something
TACTIX_STATS_U64(factor_demoted_vars) // extension vars pushed to the queue front
TACTIX_STATS_U64(factor_ticks)   // Factor's budget currency (kissat
TACTIX_STATS_U64(bve_rounds)      // BVE canonical-apply calls (a policy that
TACTIX_STATS_U64(bve_eliminated)  // variables eliminated by BVE resolution
TACTIX_STATS_U64(bve_resolutions) // resolution pairs attempted — BVE work
TACTIX_STATS_U64(bve_otf_strengthened) // antecedents shortened during BVE scan
TACTIX_STATS_U64(bve_otf_subsumed)    // second antecedents deleted by double SSR
TACTIX_STATS_U64(fastel_rounds)     // startup owner-pass admission checks
TACTIX_STATS_U64(fastel_candidates) // candidates in those checked domains
TACTIX_STATS_U64(fastel_active)     // active variables at those checks
TACTIX_STATS_U64(fastel_discovery_ticks) // live variables inspected by admission
TACTIX_STATS_U64(fastel_budget_stops) // domains exceeding the shared effort dose
TACTIX_STATS_U64(fastel_declined)    // domains where the owner pass was minority
// Startup passes skipped before mutation because root-changing structured
// reasoning left an eligible solver-created helper in FastEl's domain.
TACTIX_STATS_U64(fastel_root_suppressed)
// Startup candidates whose direct/composed identity deferred application.
TACTIX_STATS_U64(fastel_extensions_protected)
TACTIX_STATS_U64(fastel_eliminated)  // eliminations committed by startup fastel
TACTIX_STATS_U64(fastel_resolutions) // BVE work committed by startup fastel
TACTIX_STATS_TIMER(fastel_nanos)     // startup preflight plus admitted pass time
TACTIX_STATS_U64(bve_bwd_queued)   // fresh resolvents used as subsumers
TACTIX_STATS_U64(bve_bwd_subsumed) // victims deleted (cadical elimbwsub)
TACTIX_STATS_U64(bve_bwd_strengthened) // victims shortened (cadical elimbwstr)
TACTIX_STATS_U64(bve_bwd_units)       // strengthenings that collapsed to a unit
TACTIX_STATS_U64(flushed_retired_redundant) // reference-parity flush at phase exit
TACTIX_STATS_U64(factor_rejected_by_bound) // grids the coupled bound declined
TACTIX_STATS_U64(bve_marks)
TACTIX_STATS_U64(touch_marks)
TACTIX_STATS_U64(touch_kept_marks)
TACTIX_STATS_U64(forward_mark_skipped)
TACTIX_STATS_U64(strengthen_mark_skipped)
TACTIX_STATS_U64(forward_mark_advances)
TACTIX_STATS_U64(strengthen_mark_advances)
TACTIX_STATS_U64(forward_rearmed)
TACTIX_STATS_U64(strengthen_rearmed)
TACTIX_STATS_U64(bce_mark_skipped) TACTIX_STATS_U64(bce_rearmed)
TACTIX_STATS_U64(ternary_mark_skipped) TACTIX_STATS_U64(ternary_rearmed)
TACTIX_STATS_U64(instantiate_mark_skipped) TACTIX_STATS_U64(instantiate_rearmed)
TACTIX_STATS_U64(bce_interrupted)
TACTIX_STATS_U64(ternary_interrupted)
TACTIX_STATS_U64(instantiate_interrupted)
TACTIX_STATS_U64(factor_mark_skipped) TACTIX_STATS_U64(factor_rearmed)
TACTIX_STATS_U64(subsume_holds) // outer elimination rounds held open because
TACTIX_STATS_U64(touch_redundant_dynamic)
TACTIX_STATS_U64(touch_redundant_static6)
TACTIX_STATS_U64(strengthen_red_checked)
TACTIX_STATS_U64(strengthen_red_skipped)
TACTIX_STATS_U64(strengthen_red_subsumed)
TACTIX_STATS_U64(strengthen_red_strengthened)
TACTIX_STATS_U64(cce_mark_skipped) TACTIX_STATS_U64(cce_rearmed)
TACTIX_STATS_U64(cce_mark_advances)
TACTIX_STATS_U64(bve_gates_equiv)
TACTIX_STATS_U64(bve_gates_and_or)
TACTIX_STATS_U64(bve_gates_ite)
TACTIX_STATS_U64(bve_gates_xor)
TACTIX_STATS_U64(bve_definitions_checked) // attempts
TACTIX_STATS_U64(bve_gates_definition)    // ... that found a definition
TACTIX_STATS_U64(bve_definition_ticks)
TACTIX_STATS_U64(bve_definition_budget) // attempts the per-attempt tick ceiling
TACTIX_STATS_U64(bve_definition_units)  // one-sided cores / failed pivots
TACTIX_STATS_U64(bve_gate_pairs_saved) // resolvent pairs the gate reduction
// Appended so extending the public semantic HBR surface does not perturb the
// layout of existing hot counters.
TACTIX_STATS_U64(hbr_units) // explicit semantic resolvents collapsed to units
// Appended diagnostics: completed Prepared normalization certificates, and
// actual committed shortened RUP premises; work remains recognition_work.
TACTIX_STATS_U64(affine_matrix_init_normalization_rup_checks)
TACTIX_STATS_U64(affine_matrix_init_shortening_additions)
TACTIX_STATS_U64(affine_matrix_init_maintenance_service_work)
TACTIX_STATS_U64(affine_matrix_init_discovery_recognized_groups)
TACTIX_STATS_U64(affine_matrix_init_discovery_skipped_groups)
TACTIX_STATS_U64(affine_matrix_init_discovery_recognizer_work)
TACTIX_STATS_U64(affine_matrix_service_pending_slices)
TACTIX_STATS_U64(affine_matrix_service_resumed_slices)
TACTIX_STATS_U64(affine_matrix_service_slice_steps)
TACTIX_STATS_U64(affine_matrix_service_slice_work)
TACTIX_STATS_U64(affine_matrix_service_slice_invalidations)
TACTIX_STATS_U64(affine_matrix_service_minimum_slice_work_max)
// ConflictAnalysis telemetry at the native affine-reason consumer boundary.
// These cumulative counters survive session replacement.
TACTIX_STATS_U64(affine_matrix_analyze_handles)
TACTIX_STATS_U64(affine_matrix_analyze_materializations)
TACTIX_STATS_U64(affine_matrix_analyze_vectors_constructed)
TACTIX_STATS_U64(affine_matrix_analyze_literals_constructed)
