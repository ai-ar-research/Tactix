# Larger solver/proof scenarios belong to the integration gate.
set(_integration_scenarios
  affine_basis_live affine_matrix affine_matrix_dirty_rows affine_matrix_persistent_lifecycle_proof_model affine_matrix_search_proof
  analysis_native_target analyze api_one_search automatic_proof cce_public_budget
  cli_auto_proof conflict_reuse_targets congruence_index_cleanup congruence_ite_collapse congruence_ite_unit_proof
  congruence_ite_valued_sides connected_occurrence count_preservation_check default_action_map default_public_correspondence
  default_public_replay demo3_scripted_default dsr_to_pbp dsr_transport factor
  feedback_frontend feedback_provenance feedback_trace gf3_gaussian_planner independent_set_semantic_extension
  inspect_policy_feedback lucky_late_parallel micro_proofs native_policy_feedback online_inprocess
  pbp_deletion_identity_cli ramsey_policy_paths ramsey_startup solve_reset structural_journal_trajectory
  subsolver sweep sweep_proofs symmetry_backend symmetry_clause_multiset
  tr_bruteforce_0 tr_bruteforce_1 tr_bruteforce_2 tr_bruteforce_3 tseitin_cycle_action
  tseitin_cycle_action_journal vivify_compact_contract vivify_count_cache vivify_order_contract
)
get_property(_tests DIRECTORY PROPERTY TESTS)
foreach(_test IN LISTS _tests)
  get_test_property(${_test} LABELS _labels)
  if(_test IN_LIST _integration_scenarios OR _test MATCHES "proof|fuzz")
    set_tests_properties(${_test} PROPERTIES LABELS "integration")
  elseif(NOT _labels)
    set_tests_properties(${_test} PROPERTIES LABELS "unit")
  endif()
endforeach()
