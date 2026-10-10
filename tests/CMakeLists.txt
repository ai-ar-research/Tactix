# Unit tests (plain asserts, no framework — kernel discipline, no deps).
# The C-language API test doubles as the header/ABI sync check for api.cpp.

add_executable(test_kernel test_kernel.cpp)
target_link_libraries(test_kernel PRIVATE tactix_lib)
add_test(NAME kernel COMMAND test_kernel)

add_executable(test_clause_arena_replay test_clause_arena_replay.cpp)
target_link_libraries(test_clause_arena_replay PRIVATE tactix_lib)
add_test(NAME clause_arena_replay COMMAND test_clause_arena_replay)

add_executable(test_parse test_parse.cpp)
target_link_libraries(test_parse PRIVATE tactix_lib)
add_test(NAME parse COMMAND test_parse)

# The proof backend's serializers, byte for byte (N2's writer).
add_executable(test_proof test_proof.cpp)
target_link_libraries(test_proof PRIVATE tactix_lib)
add_test(NAME proof COMMAND test_proof)

add_executable(test_population_observer test_population_observer.cpp)
target_link_libraries(test_population_observer PRIVATE tactix_lib)
add_test(NAME population_observer COMMAND test_population_observer)
add_test(NAME population_observer_inventory
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_population_observer_inventory.py
                 ${CMAKE_SOURCE_DIR})

add_executable(test_sr_proof_encoding test_sr_proof_encoding.cpp)
target_link_libraries(test_sr_proof_encoding PRIVATE tactix_lib)
add_test(NAME sr_proof_encoding COMMAND test_sr_proof_encoding)

add_executable(test_ulc_rat test_ulc_rat.cpp)
target_link_libraries(test_ulc_rat PRIVATE tactix_lib)
add_test(NAME ulc_rat COMMAND test_ulc_rat)

add_executable(test_analyze test_analyze.cpp)
target_link_libraries(test_analyze PRIVATE tactix_lib)
add_test(NAME analyze COMMAND test_analyze)

# Native ConflictAnalysis payload vs the public chrono instance (gap plan
# Step 6): identical trail, learned clauses, tiers, schedule and proof.
add_executable(test_analysis_native_target test_analysis_native_target.cpp)
target_link_libraries(test_analysis_native_target PRIVATE tactix_lib)
add_test(NAME analysis_native_target COMMAND test_analysis_native_target)

add_executable(test_restart_reduce test_restart_reduce.cpp)
target_link_libraries(test_restart_reduce PRIVATE tactix_lib)
add_test(NAME restart_reduce COMMAND test_restart_reduce)

add_executable(test_switchmode test_switchmode.cpp)
target_link_libraries(test_switchmode PRIVATE tactix_lib)
add_test(NAME switchmode COMMAND test_switchmode)

add_executable(test_rephase test_rephase.cpp)
target_link_libraries(test_rephase PRIVATE tactix_lib)
add_test(NAME rephase COMMAND test_rephase)

add_executable(test_subsolver test_subsolver.cpp)
target_link_libraries(test_subsolver PRIVATE tactix_lib)
add_test(NAME subsolver COMMAND test_subsolver)

add_executable(test_subsolver_export_resource test_subsolver_export_resource.cpp
               failing_new.cpp)
set_source_files_properties(failing_new.cpp PROPERTIES COMPILE_OPTIONS
                            -fexceptions)
target_link_libraries(test_subsolver_export_resource PRIVATE tactix_lib)
add_test(NAME subsolver_export_resource COMMAND test_subsolver_export_resource)

# Sweep: candidate selection + environment construction. An arg raises the
# brute-force oracle's soak size.
add_executable(test_sweep test_sweep.cpp)
target_link_libraries(test_sweep PRIVATE tactix_lib)
add_test(NAME sweep COMMAND test_sweep)
add_test(NAME sweep_interrupted_direction
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_sweep_interrupted_direction.py
                 $<TARGET_FILE:tactix>)

add_executable(test_scheduling test_scheduling.cpp)
target_link_libraries(test_scheduling PRIVATE tactix_lib)
add_test(NAME scheduling COMMAND test_scheduling)

add_executable(test_strategy_config test_strategy_config.cpp)
target_link_libraries(test_strategy_config PRIVATE tactix_lib)
add_test(NAME strategy_config COMMAND test_strategy_config)

if(TACTIX_ENABLE_EVENT_TRACE)
  add_executable(test_event_trace test_event_trace.cpp)
else()
  # Exercise the diagnostic implementation even though the production object
  # library deliberately omits it in the default build.
  add_executable(test_event_trace test_event_trace.cpp
                 ${CMAKE_SOURCE_DIR}/src/diagnostics/event_trace.cpp)
  target_compile_definitions(test_event_trace PRIVATE
                             TACTIX_ENABLE_EVENT_TRACE=1)
endif()
target_link_libraries(test_event_trace PRIVATE tactix_lib)
add_test(NAME event_trace COMMAND test_event_trace)
add_test(NAME event_trace_compare
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_compare_event_trace.py)
add_test(NAME reduce_trace_compare
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_compare_reduce_trace.py)
add_test(NAME modelcheck
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_modelcheck.py)
add_test(NAME runtime_task
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_runtime_task.py)
add_test(NAME kissat_journal_overlay_static
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_kissat_journal_overlay_static.py)

if(TACTIX_ENABLE_RULE_JOURNAL)
  add_executable(test_rule_journal test_rule_journal.cpp)
else()
  # The production library omits the implementation; build it only into this
  # diagnostic protocol test.
  add_executable(test_rule_journal test_rule_journal.cpp
                 ${CMAKE_SOURCE_DIR}/src/diagnostics/rule_journal.cpp)
  target_compile_definitions(test_rule_journal PRIVATE
                             TACTIX_ENABLE_RULE_JOURNAL=1
                             TACTIX_TEST_STANDALONE_JOURNAL=1)
endif()
target_link_libraries(test_rule_journal PRIVATE tactix_lib)
add_test(NAME rule_journal
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_rule_journal.py
                 $<TARGET_FILE:test_rule_journal>)
if(TACTIX_ENABLE_RULE_JOURNAL)
  add_executable(test_api_journal test_api_journal.c)
  target_link_libraries(test_api_journal PRIVATE tactix_lib)
  target_include_directories(test_api_journal PRIVATE ${CMAKE_SOURCE_DIR}/include)
  add_test(NAME api_journal
           COMMAND ${CMAKE_COMMAND} -E env python3
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_api_journal.py
                   $<TARGET_FILE:test_api_journal>)
  add_test(NAME default_policy_rule_journal
           COMMAND ${CMAKE_COMMAND} -E env python3
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_tactix_default_journal.py
                   $<TARGET_FILE:tactix>)
endif()
add_test(NAME p4_fixtures
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_p4_fixtures.py)
add_test(NAME campaign_certificate_cleanup
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_campaign_certificate_cleanup.py)
add_test(NAME tierd_campaign_pinning
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_tierd_campaign_pinning.py)
add_test(NAME slurm_memory_enforcement
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_slurm_memory_enforcement.py)
add_test(NAME symmetry_backend_build_boundary
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_symmetry_backend_flags.py
                 ${CMAKE_BINARY_DIR}/compile_commands.json
                 ${CMAKE_SOURCE_DIR})
add_executable(test_symmetry_backend test_symmetry_backend.cpp)
target_include_directories(test_symmetry_backend PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(test_symmetry_backend SYSTEM PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party/dejavu-4c275e9)
target_compile_options(test_symmetry_backend PRIVATE -fexceptions -frtti)
target_link_libraries(test_symmetry_backend PRIVATE tactix_symmetry_backend)
add_test(NAME symmetry_backend COMMAND test_symmetry_backend)
add_executable(test_symmetry_clause_multiset test_symmetry_clause_multiset.cpp)
target_include_directories(test_symmetry_clause_multiset PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME symmetry_clause_multiset COMMAND test_symmetry_clause_multiset)
add_executable(test_orbitopal_group test_orbitopal_group.cpp)
target_include_directories(test_orbitopal_group PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_orbitopal_group PRIVATE tactix_lib)
add_test(NAME orbitopal_group COMMAND test_orbitopal_group)
add_test(NAME fuzz_slurm_snapshot
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzz_slurm_snapshot.py)
add_test(NAME fuzz_reachability
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzz_reachability.py)
if(NOT TACTIX_ENABLE_EVENT_TRACE AND NOT TACTIX_ENABLE_RULE_JOURNAL)
  set(trace_symbol_test_args)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND trace_symbol_test_args --debug-stubs)
  endif()
  add_test(NAME event_trace_zero_overhead
           COMMAND ${CMAKE_COMMAND} -E env python3
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_no_event_trace_symbols.py
                   $<TARGET_FILE:tactix> ${trace_symbol_test_args})
endif()

# The §5.6 pose surface: a preposed slot, a disabled entry inert at its own
# rule's counter, and the TACTIX_SLOT_*/CURSOR_*/ORDER_* knobs.
add_executable(test_slot_pose test_slot_pose.cpp)
target_link_libraries(test_slot_pose PRIVATE tactix_lib)
add_test(NAME slot_pose COMMAND test_slot_pose)

add_executable(test_reorder test_reorder.cpp)
target_link_libraries(test_reorder PRIVATE tactix_lib)
add_test(NAME reorder COMMAND test_reorder)

add_executable(test_walk test_walk.cpp)
target_link_libraries(test_walk PRIVATE tactix_lib)
add_test(NAME walk COMMAND test_walk)

add_executable(test_parity_model test_parity_model.cpp)
target_link_libraries(test_parity_model PRIVATE tactix_lib)
add_test(NAME parity_model COMMAND test_parity_model)

add_executable(test_onehot_model test_onehot_model.cpp)
target_link_libraries(test_onehot_model PRIVATE tactix_lib)
add_test(NAME onehot_model COMMAND test_onehot_model)

add_executable(test_subsume test_subsume.cpp)
target_link_libraries(test_subsume PRIVATE tactix_lib)
add_test(NAME subsume COMMAND test_subsume)

add_executable(test_strengthen test_strengthen.cpp)
target_link_libraries(test_strengthen PRIVATE tactix_lib)
add_test(NAME strengthen COMMAND test_strengthen)

add_executable(test_instantiate test_instantiate.cpp)
target_link_libraries(test_instantiate PRIVATE tactix_lib)
add_test(NAME instantiate COMMAND test_instantiate)

add_executable(test_vivify test_vivify.cpp)
target_link_libraries(test_vivify PRIVATE tactix_lib)
add_test(NAME vivify COMMAND test_vivify)

# N1: every Vivify band reachable through the API, the unscoped path excluding
# none.
add_executable(test_vivify_n1 test_vivify_n1.cpp)
target_link_libraries(test_vivify_n1 PRIVATE tactix_lib)
add_test(NAME vivify_n1 COMMAND test_vivify_n1)

add_executable(test_probe test_probe.cpp)
target_link_libraries(test_probe PRIVATE tactix_lib)
add_test(NAME probe COMMAND test_probe)

add_executable(test_backbone test_backbone.cpp)
target_link_libraries(test_backbone PRIVATE tactix_lib)
add_test(NAME backbone COMMAND test_backbone)

add_executable(test_bce test_bce.cpp)
target_link_libraries(test_bce PRIVATE tactix_lib)
add_test(NAME bce COMMAND test_bce)

add_executable(test_cce test_cce.cpp)
target_link_libraries(test_cce PRIVATE tactix_lib)
add_test(NAME cce COMMAND test_cce)

add_executable(test_ternary test_ternary.cpp)
target_link_libraries(test_ternary PRIVATE tactix_lib)
add_test(NAME ternary COMMAND test_ternary)

add_executable(test_hyperbinary test_hyperbinary.cpp)
target_link_libraries(test_hyperbinary PRIVATE tactix_lib)
add_test(NAME hyperbinary COMMAND test_hyperbinary)

add_executable(test_decide test_decide.cpp)
target_link_libraries(test_decide PRIVATE tactix_lib)
add_test(NAME decide COMMAND test_decide)

add_executable(test_reconstruct test_reconstruct.cpp)
target_link_libraries(test_reconstruct PRIVATE tactix_lib)
add_test(NAME reconstruct COMMAND test_reconstruct)

add_executable(test_substitute test_substitute.cpp)
target_link_libraries(test_substitute PRIVATE tactix_lib)
add_test(NAME substitute COMMAND test_substitute)

add_executable(test_congruence test_congruence.cpp)
target_link_libraries(test_congruence PRIVATE tactix_lib)
add_test(NAME congruence COMMAND test_congruence)

add_executable(test_binary_deletion test_binary_deletion.cpp)
target_link_libraries(test_binary_deletion PRIVATE tactix_lib)
add_test(NAME binary_deletion COMMAND test_binary_deletion)

add_executable(test_transitive_reduce test_transitive_reduce.cpp)
target_link_libraries(test_transitive_reduce PRIVATE tactix_lib)
add_test(NAME transitive_reduce COMMAND test_transitive_reduce)

add_executable(test_bve test_bve.cpp)
target_link_libraries(test_bve PRIVATE tactix_lib)
add_test(NAME bve COMMAND test_bve)

add_executable(test_bve_invocation test_bve_invocation.cpp)
target_link_libraries(test_bve_invocation PRIVATE tactix_lib)
add_test(NAME bve_invocation COMMAND test_bve_invocation)

add_executable(test_lucky test_lucky.cpp)
target_link_libraries(test_lucky PRIVATE tactix_lib)
add_test(NAME lucky COMMAND test_lucky)

add_executable(test_lucky_terminal test_lucky_terminal.cpp)
target_link_libraries(test_lucky_terminal PRIVATE tactix_lib)
add_test(NAME lucky_terminal COMMAND test_lucky_terminal)

add_executable(test_api_c test_api_c.c)
target_link_libraries(test_api_c PRIVATE tactix_lib)
target_include_directories(test_api_c PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME api_n1 COMMAND test_api_c)

# External certificate-oracle fixture: invoked only by a pinned SLURM proof
# job because it writes a proof and requires independent checker binaries.
add_executable(condition_certificate_driver condition_certificate_driver.c)
target_link_libraries(condition_certificate_driver PRIVATE tactix_lib)
target_include_directories(condition_certificate_driver PRIVATE
                           ${CMAKE_SOURCE_DIR}/include)

add_test(NAME condition_compacted_model
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tests/condition_compacted_model_check.py
                 $<TARGET_FILE:test_condition>)

# tactix_apply_instance on every registry entry (the §5.6 action space) plus the
# no-payload-aliasing contract. A C client, so it also checks header/ABI sync.
add_executable(test_api_instances test_api_instances.c)
target_link_libraries(test_api_instances PRIVATE tactix_lib)
target_include_directories(test_api_instances PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME api_instances COMMAND test_api_instances)

# tactix_set_guidance_publication: NATIVE public dispatch publishes rule events
# into H exactly as the shared executor does; DISCARD keeps the v1 behavior.
add_executable(test_api_guidance_publication test_api_guidance_publication.c)
target_link_libraries(test_api_guidance_publication PRIVATE tactix_lib)
target_include_directories(test_api_guidance_publication PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME api_guidance_publication COMMAND test_api_guidance_publication)

add_executable(test_online_api test_online_api.c)
target_link_libraries(test_online_api PRIVATE tactix_lib)
target_include_directories(test_online_api PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME online_api COMMAND test_online_api)

add_executable(test_online_all_actions test_online_all_actions.c)
target_link_libraries(test_online_all_actions PRIVATE tactix_lib)
target_include_directories(test_online_all_actions PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME online_all_actions COMMAND test_online_all_actions)

add_executable(test_theory_action_v5_runtime test_theory_action_v5_runtime.cpp)
target_link_libraries(test_theory_action_v5_runtime PRIVATE tactix_lib)
add_test(NAME theory_action_v5_runtime COMMAND test_theory_action_v5_runtime)

add_executable(test_random_controller_policy test_random_controller_policy.cpp)
target_link_libraries(test_random_controller_policy PRIVATE tactix_lib)
add_test(NAME random_controller_policy COMMAND test_random_controller_policy)

add_executable(test_direct_csp_guarded_cover test_direct_csp_guarded_cover.cpp)
target_link_libraries(test_direct_csp_guarded_cover PRIVATE tactix_lib)
add_test(NAME direct_csp_guarded_cover COMMAND test_direct_csp_guarded_cover)

add_executable(test_direct_csp_guarded_cover_live test_direct_csp_guarded_cover_live.cpp)
target_link_libraries(test_direct_csp_guarded_cover_live PRIVATE tactix_lib)
add_test(NAME direct_csp_guarded_cover_live COMMAND test_direct_csp_guarded_cover_live)
add_test(NAME direct_csp_guarded_cover_proof_model
         COMMAND ${CMAKE_SOURCE_DIR}/tests/direct_csp_guarded_cover_proof_model.sh
                 $<TARGET_FILE:test_direct_csp_guarded_cover_live> ${CMAKE_SOURCE_DIR})
set_tests_properties(direct_csp_guarded_cover_proof_model PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME direct_csp_guarded_cover_owner_refresh_proof_model
         COMMAND ${CMAKE_SOURCE_DIR}/tests/direct_csp_guarded_cover_owner_refresh_proof_model.sh
                 $<TARGET_FILE:test_direct_csp_guarded_cover_owner_refresh> ${CMAKE_SOURCE_DIR})
set_tests_properties(direct_csp_guarded_cover_owner_refresh_proof_model PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME affine_matrix_persistent_lifecycle_proof_model
         COMMAND ${CMAKE_SOURCE_DIR}/tests/affine_matrix_persistent_lifecycle_proof_model.sh
                 $<TARGET_FILE:test_affine_matrix_persistent_lifecycle> ${CMAKE_SOURCE_DIR})
set_tests_properties(affine_matrix_persistent_lifecycle_proof_model PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME default_affine_search_proof_model
         COMMAND ${CMAKE_SOURCE_DIR}/tests/default_affine_search_proof_model.sh
                 $<TARGET_FILE:test_default_affine_search> ${CMAKE_SOURCE_DIR})
set_tests_properties(default_affine_search_proof_model PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME default_affine_restart_trigger_proof
         COMMAND ${CMAKE_SOURCE_DIR}/tests/default_affine_restart_trigger_proof.sh
                 $<TARGET_FILE:test_default_affine_restart_trigger> ${CMAKE_SOURCE_DIR})
set_tests_properties(default_affine_restart_trigger_proof PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME affine_matrix_actual_geometry_proof_model
         COMMAND bash ${CMAKE_SOURCE_DIR}/tests/affine_matrix_actual_geometry_proof_model.sh
                 $<TARGET_FILE:test_affine_matrix_actual_geometry> ${CMAKE_SOURCE_DIR})
set_tests_properties(affine_matrix_actual_geometry_proof_model PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME random_controller_affine_resource_proof_model
         COMMAND bash ${CMAKE_SOURCE_DIR}/tests/random_controller_affine_resource_proof_model.sh
                 $<TARGET_FILE:test_random_controller_affine_resource> ${CMAKE_SOURCE_DIR})
set_tests_properties(random_controller_affine_resource_proof_model PROPERTIES SKIP_RETURN_CODE 77)

foreach(normalization_test affine_root_image affine_matrix_root_normalized)
  add_test(NAME ${normalization_test}_proof_model
           COMMAND bash ${CMAKE_SOURCE_DIR}/tests/affine_root_normalization_proof_model.sh
                   $<TARGET_FILE:test_${normalization_test}> ${CMAKE_SOURCE_DIR})
  set_tests_properties(${normalization_test}_proof_model PROPERTIES SKIP_RETURN_CODE 77)
endforeach()

add_executable(test_direct_csp_guarded_cover_frontend test_direct_csp_guarded_cover_frontend.cpp)
target_link_libraries(test_direct_csp_guarded_cover_frontend PRIVATE tactix_lib)
add_test(NAME direct_csp_guarded_cover_frontend COMMAND test_direct_csp_guarded_cover_frontend)

add_executable(test_direct_csp_support_action test_direct_csp_support_action.cpp)
target_link_libraries(test_direct_csp_support_action PRIVATE tactix_lib)
add_test(NAME direct_csp_support_action COMMAND test_direct_csp_support_action)
add_test(NAME direct_csp_support_proof_model
         COMMAND ${CMAKE_SOURCE_DIR}/tests/direct_csp_support_proof_model.sh
                 $<TARGET_FILE:test_direct_csp_support_action>
                 ${CMAKE_SOURCE_DIR})
set_tests_properties(direct_csp_support_proof_model PROPERTIES
                     SKIP_RETURN_CODE 77)

add_executable(test_random_policy test_random_policy.c)
target_link_libraries(test_random_policy PRIVATE tactix_lib)
target_include_directories(test_random_policy PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME random_policy COMMAND test_random_policy)

# --trace/--replay (the §8.5 offline replay harness) plus cross-process
# reproducibility of the typed online random policy. Runs the real binary;
# never skips.
add_test(NAME trace_replay
         COMMAND ${CMAKE_SOURCE_DIR}/tests/trace_replay.sh $<TARGET_FILE:tactix>)

# Tiny UNSATs through the real binary, proofs verified by drat-trim (skips 77).
add_test(NAME micro_proofs
         COMMAND ${CMAKE_SOURCE_DIR}/tests/micro_proofs.sh
                 $<TARGET_FILE:tactix> ${CMAKE_SOURCE_DIR}
                 $<TARGET_FILE:test_analyze>
                 $<TARGET_FILE:test_congruence>
                 $<TARGET_FILE:test_restart_reduce>
                 $<TARGET_FILE:test_factor_n1>
                 $<TARGET_FILE:test_online_inprocess>
                 $<TARGET_FILE:test_ulc_rat>)
set_tests_properties(micro_proofs PROPERTIES SKIP_RETURN_CODE 77)


# Compact aligned-domain equality abstraction for ternary diagonal nogoods:
# validate the pure planner/model projection and every RAT/RUP lemma through
# the pinned native DSR checker route.
add_executable(test_nary_csp_equality test_nary_csp_equality.cpp)
target_link_libraries(test_nary_csp_equality PRIVATE tactix_lib)
add_test(NAME nary_csp_equality COMMAND test_nary_csp_equality)

add_executable(test_nary_csp_relation_owner test_nary_csp_relation_owner.cpp)
target_link_libraries(test_nary_csp_relation_owner PRIVATE tactix_lib)
add_test(NAME nary_csp_relation_owner COMMAND test_nary_csp_relation_owner)
add_test(NAME nary_csp_equality_proof
         COMMAND ${CMAKE_SOURCE_DIR}/tests/nary_csp_equality_proof.sh
                 $<TARGET_FILE:test_nary_csp_equality>
                 ${CMAKE_SOURCE_DIR})
set_tests_properties(nary_csp_equality_proof PROPERTIES SKIP_RETURN_CODE 77)

add_executable(test_nary_csp_equality_live test_nary_csp_equality_live.cpp)
target_link_libraries(test_nary_csp_equality_live PRIVATE tactix_lib)
add_test(NAME nary_csp_equality_live COMMAND test_nary_csp_equality_live)
add_test(NAME nary_csp_equality_live_proof
         COMMAND ${CMAKE_SOURCE_DIR}/tests/nary_csp_equality_live_proof.sh
                 $<TARGET_FILE:test_nary_csp_equality_live>
                 ${CMAKE_SOURCE_DIR})
set_tests_properties(nary_csp_equality_live_proof PROPERTIES SKIP_RETURN_CODE 77)

add_executable(test_nary_csp_equality_public
               test_nary_csp_equality_public.cpp)
target_link_libraries(test_nary_csp_equality_public PRIVATE tactix_lib)
add_test(NAME nary_csp_equality_public COMMAND test_nary_csp_equality_public)
add_test(NAME nary_csp_equality_public_oracle
         COMMAND ${CMAKE_SOURCE_DIR}/tests/nary_csp_equality_public_oracle.sh
                 $<TARGET_FILE:test_nary_csp_equality_public>
                 ${CMAKE_SOURCE_DIR})
set_tests_properties(nary_csp_equality_public_oracle PROPERTIES
                     SKIP_RETURN_CODE 77)

add_executable(test_nary_csp_equality_amo
               test_nary_csp_equality_amo.cpp)
target_link_libraries(test_nary_csp_equality_amo PRIVATE tactix_lib)
add_test(NAME nary_csp_equality_amo COMMAND test_nary_csp_equality_amo)
add_test(NAME nary_csp_equality_amo_proof
         COMMAND ${CMAKE_SOURCE_DIR}/tests/nary_csp_equality_amo_proof.sh
                 $<TARGET_FILE:test_nary_csp_equality_amo>
                 ${CMAKE_SOURCE_DIR})
set_tests_properties(nary_csp_equality_amo_proof PROPERTIES SKIP_RETURN_CODE 77)

# A persistent GF(2) event needs one arbitrary-width clausal reason, not the
# complete parity CNF. Verify the targeted RUP export and helper retirement.
add_test(NAME parity_wide_reason_proof
         COMMAND ${CMAKE_SOURCE_DIR}/tests/parity_wide_reason_proof.sh
                 $<TARGET_FILE:test_parity_lifecycle>
                 ${CMAKE_SOURCE_DIR})
set_tests_properties(parity_wide_reason_proof PROPERTIES SKIP_RETURN_CODE 77)

# The pure persistent-matrix bridge must certify one wide event reason, retire
# all proof-only helpers, and leave only that reason marked for installation.
add_test(NAME affine_matrix_transaction_proof
         COMMAND ${CMAKE_SOURCE_DIR}/tests/affine_matrix_transaction_proof.sh
                 $<TARGET_FILE:test_affine_matrix_transaction>
                 ${CMAKE_SOURCE_DIR})
set_tests_properties(affine_matrix_transaction_proof PROPERTIES
                     SKIP_RETURN_CODE 77)

# The persistent matrix live bridge emits its preflighted helper proof and
# reason, while ordinary watched-clause propagation consumes the result.
add_test(NAME affine_matrix_live_proof
         COMMAND ${CMAKE_SOURCE_DIR}/tests/affine_matrix_live_proof.sh
                 $<TARGET_FILE:test_affine_matrix_live>
                 ${CMAKE_SOURCE_DIR})
set_tests_properties(affine_matrix_live_proof PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME affine_matrix_search_proof
         COMMAND ${CMAKE_SOURCE_DIR}/tests/affine_matrix_search_proof.sh
                 $<TARGET_FILE:test_affine_matrix_search>
                 ${CMAKE_SOURCE_DIR})
set_tests_properties(affine_matrix_search_proof PROPERTIES SKIP_RETURN_CODE 77)

# Sweep: every proof it produces, replayed through the real checkers (drat-trim
# for the lemmas, lrat-check for the chains), in sub-solver space and again
# after the sub-solver -> kernel translation. Skips 77 without a checker.
add_test(NAME sweep_proofs
         COMMAND ${CMAKE_SOURCE_DIR}/tests/sweep_proofs.sh ${CMAKE_SOURCE_DIR}
                 $<TARGET_FILE:tactix>
                 $<TARGET_FILE:test_subsolver> $<TARGET_FILE:test_sweep>)
set_tests_properties(sweep_proofs PROPERTIES SKIP_RETURN_CODE 77)

# A satisfiable Sweep replay must still emit a checkable LRAT prefix. CakeLPR
# must reach only the expected absent-empty end; see
# benchmarks/reports/2026-09-25-am455-latest-source-fuzz-consolidation.md.
add_test(NAME sweep_retarget_lrat
         COMMAND ${CMAKE_SOURCE_DIR}/tests/sweep_retarget_lrat.sh
                 ${CMAKE_SOURCE_DIR} $<TARGET_FILE:tactix>)

# V4 exhaustive model-equivalence oracle: 4 modes x 50k random 5-var formulas,
# model sets compared by enumeration -- the only direct check that V4's deletions
# preserve equivalence, since a proof checker accepts any delete line.
add_executable(test_tr_bruteforce test_tr_bruteforce.cpp)
target_link_libraries(test_tr_bruteforce PRIVATE tactix_lib)
foreach(mode 0 1 2 3)
  add_test(NAME tr_bruteforce_${mode} COMMAND test_tr_bruteforce ${mode})
endforeach()

add_test(NAME inspect_policy_feedback
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_inspect_policy_feedback.py
                 $<TARGET_FILE:test_feedback_trace>
                 $<TARGET_FILE:test_default_execution_signature>)
add_test(NAME feedback_frontend
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_feedback_frontend.py
                 $<TARGET_FILE:tactix>)
add_test(NAME controller_native_producers
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_controller_native_producers.py
                 $<TARGET_FILE:tactix>)
add_test(NAME affine_matrix_cli
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_affine_matrix_cli.py
                 $<TARGET_FILE:tactix>)
add_test(NAME direct_csp_controller
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_direct_csp_controller.py
                 $<TARGET_FILE:tactix>
                 ${CMAKE_SOURCE_DIR}/tools/fuzz/checkmodel)
add_test(NAME public_theory_target_tables
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_public_theory_target_tables.py)
add_test(NAME public_theory_target_score
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_public_theory_target_score.py)

add_test(NAME startup_parity_check
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_startup_parity_check.py
                 $<TARGET_FILE:test_startup_parity> $<TARGET_FILE:tactix>)
add_test(NAME count_preservation_check
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_count_preservation_check.py
                 $<TARGET_FILE:test_count_preservation> $<TARGET_FILE:tactix>)
add_test(NAME ramsey_cli
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_ramsey_cli.py
                 $<TARGET_FILE:tactix>)
add_test(NAME congruence_sr_check
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_congruence_sr_check.py
                 $<TARGET_FILE:tactix>)

add_executable(test_factor test_factor.cpp)
target_link_libraries(test_factor PRIVATE tactix_lib)
add_test(NAME factor COMMAND test_factor)

add_executable(test_factor_n1 test_factor_n1.cpp)
target_link_libraries(test_factor_n1 PRIVATE tactix_lib)
add_test(NAME factor_n1 COMMAND test_factor_n1)

add_executable(test_subsume_n1 test_subsume_n1.cpp)
target_link_libraries(test_subsume_n1 PRIVATE tactix_lib)
add_test(NAME subsume_n1 COMMAND test_subsume_n1)

# A source conformance check, not a unit test: every Kernel scheduling-config
# field must have the writer its classification claims.
add_test(NAME config_wiring
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-config-wiring.py)

# Every rule the inprocessing slot schedules (every RuleTier::Inprocess registry
# line, since registering is scheduling) must have a governor: an EFFORT_SPECS
# row, or a named alternate whose mechanism is still in the source.
add_test(NAME effort_specs
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-effort-specs.py)

# Every option in kissat/src/options.h and cadical/src/options.hpp must be
# accounted for by a parity-ledger row, §13.6's dismissal roster, or a named
# carried/exempt entry, so a reference bump cannot turn the ledger into a silent
# lower bound. Fails rather than skips without the reference clones (CLAUDE.md).
add_test(NAME option_coverage
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-option-coverage.py)

# The §5.6 reward channel: every dosed rule exposes a (yield, effort) pair,
# header numbering matches api.cpp's case order, and every case returns the
# counter its macro names -- a swapped pair inverts the learner's cost axis.
add_test(NAME stat_ids
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-stat-ids.py)

# Adding a native statistic must not silently omit it from deterministic
# feedback. Native timers are present in observations but excluded from replay.
add_test(NAME feedback_statistics
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-feedback-statistics.py)

# Source citations in the living docs must resolve; a stale one still reads
# correct. Narrow on purpose: it prints what it skipped, so green is not
# full coverage.
add_test(NAME citations
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-citations.py)

# A `§X.Y` pointing at the wrong section reads exactly like one that resolves,
# so every section reference in the living docs must resolve to a real section.
add_test(NAME section_refs
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-section-refs.py)

# The phase-3 exit is "implemented, or a reasoned no-implement with evidence",
# falsifiable only if every disposition has a ledger row.
add_test(NAME inventory
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-inventory.py)

# The same shape over source comments, enforced rather than remembered. The
# checker states the rule it applies; see it for what a comment may carry.
add_test(NAME comment_hygiene
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-comment-hygiene.py)

# Living documentation is an onboarding surface, not an append-only archive.
# These budgets and cheap cross-file invariants prevent wrap-up from silently
# growing or contradicting it again.
add_test(NAME doc_size
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-doc-size.py)

# The HTML guide is manually maintained, so at minimum its public/folded rule
# inventory must stay identical to the calculus (plus V6a's internal card).
add_test(NAME proof_rule_sync
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-proof-rule-sync.py)
add_test(NAME default_strategy_config_check
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-default-strategy-config.py)

# `H` is optional -- a null-H solver is sound but unguided.
add_executable(test_no_heuristics test_no_heuristics.cpp)
target_link_libraries(test_no_heuristics PRIVATE tactix_lib)
add_test(NAME no_heuristics COMMAND test_no_heuristics)

# Kernel/H boundary guard (plan §5): kernel.hpp holds struct Heuristics as an
# opaque pointer only and must never #include policies/heuristics.hpp.
add_test(NAME kernel_h_boundary
         COMMAND bash -c "! grep -qE '^[[:space:]]*#[[:space:]]*include.*heuristics\\.hpp' '${CMAKE_SOURCE_DIR}/src/kernel/kernel.hpp'")

add_test(NAME rule_policy_boundary
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-rule-policy-boundary.py)

# A reused policy object must be indistinguishable from a fresh one; the phase-6
# learner solves many instances per process.
add_executable(test_solve_reset test_solve_reset.cpp)
target_link_libraries(test_solve_reset PRIVATE tactix_lib)
add_test(NAME solve_reset COMMAND test_solve_reset)

# Conventional C++ tests are discovered automatically. Existing explicit
# declarations above preserve special names/arguments; a newly added
# test_<name>.cpp gets a matching executable and CTest entry without a central
# edit. C and shell/integration tests remain explicit because their contracts
# need extra include paths, arguments, or skip handling.
file(GLOB TACTIX_CONVENTIONAL_TESTS CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")
foreach(test_source IN LISTS TACTIX_CONVENTIONAL_TESTS)
  get_filename_component(test_target "${test_source}" NAME_WE)
  if(NOT TARGET "${test_target}")
    string(REGEX REPLACE "^test_" "" test_name "${test_target}")
    add_executable("${test_target}" "${test_source}")
    target_link_libraries("${test_target}" PRIVATE tactix_lib)
    add_test(NAME "${test_name}" COMMAND "${test_target}")
  endif()
endforeach()

# Bound the empty-scope interruption regression if stop polling regresses.
# The nonvacuous 728-graph clique-order sweep also exercises bounded prefixes;
# the full online suite takes about 21 seconds under ASan/UBSan.
set_tests_properties(online_inprocess PROPERTIES TIMEOUT 60)

add_test(NAME tseitin_cycle_action_journal
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_tseitin_cycle_action_journal.py
                 $<TARGET_FILE:test_tseitin_cycle_action>)

add_test(NAME nary_csp_equality_transitivity_proof_model
         COMMAND ${CMAKE_SOURCE_DIR}/tests/nary_csp_equality_transitivity_proof_model.sh
                 $<TARGET_FILE:test_nary_csp_equality_transitivity_live> ${CMAKE_SOURCE_DIR})
set_tests_properties(nary_csp_equality_transitivity_proof_model PROPERTIES SKIP_RETURN_CODE 77)
