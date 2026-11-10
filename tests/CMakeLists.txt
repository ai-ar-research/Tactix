# Unit tests (plain asserts, no framework — kernel discipline, no deps).
# The C-language API test doubles as the header/ABI sync check for api.cpp.
#
# Every conventional C++ test (test_<name>.cpp -> executable test_<name>,
# linked to tactix_lib, CTest <name>) is registered by the glob at the end of
# this file. Only targets with extra sources, include paths, properties,
# arguments or a special name are declared explicitly.

set(TACTIX_PYTHON ${CMAKE_COMMAND} -E env python3)

# libstdc++ locale facets need RTTI in the host-style definition fixture.
set_source_files_properties(test_default_definition.cpp PROPERTIES COMPILE_OPTIONS "-frtti;-fno-sanitize=vptr")

# Internal heap comparison plus real IPASIR callback/lifetime coverage.
add_executable(test_bve_order test_bve_order.cpp)
target_link_libraries(test_bve_order PRIVATE tactix_lib tactix_ipasir)
add_test(NAME bve_order COMMAND test_bve_order)

set(TACTIX_SKIP_UNLESS_PBP ${CMAKE_SOURCE_DIR}/tests/skip_unless.sh
    ${CMAKE_SOURCE_DIR}/tools/checkers/bin/veripb
    ${CMAKE_SOURCE_DIR}/tools/checkers/bin/cake-pb-cnf --)

# A test whose exit status 77 means "skipped" (a checker or input is absent).
function(tactix_skip77_test name)
  add_test(NAME ${name} COMMAND ${ARGN})
  set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77)
endfunction()

# A Python test script run under `cmake -E env python3`.
function(tactix_python_test name script)
  add_test(NAME ${name} COMMAND ${TACTIX_PYTHON} ${script} ${ARGN})
endfunction()

if(TACTIX_BUILD_SHARED_IPASIR)
  tactix_python_test(python_native_inprocess_schedule
    ${CMAKE_CURRENT_SOURCE_DIR}/test_python_inprocess_schedule.py
    $<TARGET_FILE:tactix_shared> $<TARGET_FILE:test_native_inprocess_observation> ${CMAKE_SOURCE_DIR})
  tactix_python_test(python_phase_pipeline_proof
    ${CMAKE_CURRENT_SOURCE_DIR}/test_python_phase_pipeline_proof.py
    $<TARGET_FILE:tactix_shared> $<TARGET_FILE:test_native_inprocess_observation> ${CMAKE_SOURCE_DIR})
  tactix_python_test(python_capi ${CMAKE_CURRENT_SOURCE_DIR}/test_python_capi.py
    $<TARGET_FILE:tactix_shared> ${CMAKE_SOURCE_DIR} ${CMAKE_C_COMPILER})
  tactix_python_test(python_policy_parity ${CMAKE_CURRENT_SOURCE_DIR}/test_python_policy_parity.py
    $<TARGET_FILE:tactix_shared> ${CMAKE_SOURCE_DIR} ${CMAKE_C_COMPILER})
  tactix_python_test(python_scripted_default_parity ${CMAKE_CURRENT_SOURCE_DIR}/test_python_policy_parity.py
    $<TARGET_FILE:tactix_shared> ${CMAKE_SOURCE_DIR} ${CMAKE_C_COMPILER} --policy scripted-default)
  tactix_skip77_test(python_session ${TACTIX_PYTHON}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_python_session.py
    $<TARGET_FILE:tactix_shared> ${CMAKE_SOURCE_DIR})
  tactix_skip77_test(python_restart_clock ${TACTIX_PYTHON}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_python_restart_clock.py
    $<TARGET_FILE:tactix_shared> ${CMAKE_SOURCE_DIR})
endif()

tactix_skip77_test(bve_order_proof ${TACTIX_PYTHON}
  ${CMAKE_CURRENT_SOURCE_DIR}/test_python_bve_order.py
  $<TARGET_FILE:test_bve_order> ${CMAKE_SOURCE_DIR})

# A tests/<script> proof/model check over one fixture executable and the
# source root; it exits 77 when a pinned checker is missing.
function(tactix_script_test name script target)
  tactix_skip77_test(${name} ${CMAKE_SOURCE_DIR}/tests/${script}
                     $<TARGET_FILE:${target}> ${CMAKE_SOURCE_DIR})
endfunction()

# A diagnostic test that must also build when the production library omits
# the diagnostic: then it compiles the implementation in itself.
function(tactix_standalone_diagnostic_test target enabled implementation)
  if(${enabled})
    add_executable(${target} ${target}.cpp)
  else()
    add_executable(${target} ${target}.cpp
                   ${CMAKE_SOURCE_DIR}/src/diagnostics/${implementation})
    target_compile_definitions(${target} PRIVATE ${ARGN})
  endif()
  target_link_libraries(${target} PRIVATE tactix_lib)
endfunction()

tactix_python_test(population_observer_inventory
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_population_observer_inventory.py
                   ${CMAKE_SOURCE_DIR})

tactix_skip77_test(streaming_pbp_check ${TACTIX_SKIP_UNLESS_PBP} ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_streaming_pbp_check.py
                   $<TARGET_FILE:test_sr_proof_encoding>)
tactix_skip77_test(algebraic_pbp_proofs ${TACTIX_SKIP_UNLESS_PBP}
                   ${CMAKE_SOURCE_DIR}/tests/algebraic_pbp_proofs.sh
                   $<TARGET_FILE:test_affine_matrix_search>
                   $<TARGET_FILE:test_gf3_general_row_public>
                   $<TARGET_FILE:test_cardinality_search_live>
                   ${CMAKE_SOURCE_DIR})
tactix_skip77_test(cli_pbp_proof ${TACTIX_SKIP_UNLESS_PBP}
                   ${CMAKE_SOURCE_DIR}/tests/cli_pbp_proof.sh
                   $<TARGET_FILE:tactix> ${CMAKE_SOURCE_DIR})
# The automatic default: its route, trigger and checked certificate per
# fixture, and trajectory identity with --pbp (skips without the checkers).
tactix_skip77_test(cli_auto_proof ${CMAKE_SOURCE_DIR}/tests/cli_auto_proof.sh
                   $<TARGET_FILE:tactix> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(pbp_fuzz ${TACTIX_SKIP_UNLESS_PBP} ${TACTIX_PYTHON}
                   ${CMAKE_SOURCE_DIR}/tests/test_pbp_fuzz.py
                   $<TARGET_FILE:tactix> ${CMAKE_SOURCE_DIR})
# Streaming-PBP exact deletion through the CLI (the unit cases are the
# conventional test_pbp_deletion_identity, which also writes the written-back
# root-unit fixture this script checks).
tactix_skip77_test(pbp_deletion_identity_cli ${TACTIX_SKIP_UNLESS_PBP}
                   ${CMAKE_SOURCE_DIR}/tests/pbp_deletion_identity.sh
                   $<TARGET_FILE:tactix> ${CMAKE_SOURCE_DIR}
                   $<TARGET_FILE:test_pbp_deletion_identity>)

add_executable(test_subsolver_export_resource test_subsolver_export_resource.cpp
               failing_new.cpp)
set_source_files_properties(failing_new.cpp PROPERTIES COMPILE_OPTIONS
                            -fexceptions)
target_link_libraries(test_subsolver_export_resource PRIVATE tactix_lib)
add_test(NAME subsolver_export_resource COMMAND test_subsolver_export_resource)
add_executable(test_weakening_resource test_weakening_resource.cpp failing_new.cpp)
target_link_libraries(test_weakening_resource PRIVATE tactix_lib)
add_test(NAME weakening_resource COMMAND test_weakening_resource)
add_executable(test_derivation_resource test_derivation_resource.cpp failing_new.cpp)
target_link_libraries(test_derivation_resource PRIVATE tactix_lib)
add_test(NAME derivation_resource COMMAND test_derivation_resource)

# The production object library omits the event trace and rule journal in the
# default build; these protocol tests then compile the implementation in.
tactix_standalone_diagnostic_test(test_event_trace TACTIX_ENABLE_EVENT_TRACE
                                  event_trace.cpp TACTIX_ENABLE_EVENT_TRACE=1)
add_test(NAME event_trace COMMAND test_event_trace)
tactix_python_test(event_trace_compare
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_compare_event_trace.py)
tactix_python_test(reduce_trace_compare
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_compare_reduce_trace.py)
tactix_python_test(modelcheck ${CMAKE_CURRENT_SOURCE_DIR}/test_modelcheck.py)
tactix_python_test(runtime_task ${CMAKE_CURRENT_SOURCE_DIR}/test_runtime_task.py)
tactix_standalone_diagnostic_test(test_rule_journal TACTIX_ENABLE_RULE_JOURNAL
                                  rule_journal.cpp TACTIX_ENABLE_RULE_JOURNAL=1
                                  TACTIX_TEST_STANDALONE_JOURNAL=1)
tactix_python_test(rule_journal ${CMAKE_CURRENT_SOURCE_DIR}/test_rule_journal.py
                   $<TARGET_FILE:test_rule_journal>)
if(TACTIX_ENABLE_RULE_JOURNAL)
  add_executable(test_api_journal test_api_journal.c)
  target_link_libraries(test_api_journal PRIVATE tactix_lib)
  tactix_python_test(api_journal ${CMAKE_CURRENT_SOURCE_DIR}/test_api_journal.py
                     $<TARGET_FILE:test_api_journal>)
  tactix_python_test(default_policy_rule_journal
                     ${CMAKE_CURRENT_SOURCE_DIR}/test_tactix_default_journal.py
                     $<TARGET_FILE:tactix>)
endif()
tactix_python_test(p4_fixtures ${CMAKE_CURRENT_SOURCE_DIR}/test_p4_fixtures.py)
tactix_skip77_test(campaign_certificate_cleanup ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_campaign_certificate_cleanup.py)
tactix_python_test(slurm_memory_enforcement
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_slurm_memory_enforcement.py)
tactix_python_test(symmetry_backend_build_boundary
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
tactix_python_test(fuzz_resource_limits
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzz_resource_limits.py)
tactix_python_test(fuzz_shard_aggregate
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzz_shard_aggregate.py)
tactix_python_test(fuzz_reachability
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_fuzz_reachability.py)
if(NOT TACTIX_ENABLE_EVENT_TRACE AND NOT TACTIX_ENABLE_RULE_JOURNAL)
  set(trace_symbol_test_args)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND trace_symbol_test_args --debug-stubs)
  endif()
  tactix_python_test(event_trace_zero_overhead
                     ${CMAKE_CURRENT_SOURCE_DIR}/test_no_event_trace_symbols.py
                     $<TARGET_FILE:tactix> ${trace_symbol_test_args})
endif()

# test_parity_certificate.cpp is header-only over the STL: it builds without
# tactix_lib, and the resume proof script replays its certificate.
add_executable(test_parity_certificate test_parity_certificate.cpp)
target_include_directories(test_parity_certificate PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME parity_certificate COMMAND test_parity_certificate)
tactix_script_test(parity_certificate_resume_proof
                   parity_certificate_resume_proof.sh test_parity_certificate)

tactix_python_test(lucky_late_parallel
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_lucky_parallel.py
                   $<TARGET_FILE:test_lucky>)
# The runner launches its three isolated solver modes concurrently.  Account
# for those children so a parallel CTest run does not oversubscribe its budget.
set_tests_properties(lucky_late_parallel PROPERTIES PROCESSORS 3)

# The C clients below see include/tactix.h through tactix_lib's interface.
add_executable(test_api_c test_api_c.c)
target_link_libraries(test_api_c PRIVATE tactix_lib)
add_test(NAME api_n1 COMMAND test_api_c)

# Real incremental client: only the C headers and the separate transport
# archive, retaining the registry's static registration translation units.
add_executable(test_ipasir_query test_ipasir_query.c)
target_link_libraries(test_ipasir_query PRIVATE
  "$<LINK_LIBRARY:WHOLE_ARCHIVE,tactix_ipasir>")
add_test(NAME ipasir_query COMMAND test_ipasir_query)
add_executable(test_ipasir_termination test_ipasir_termination.c)
target_link_libraries(test_ipasir_termination PRIVATE
  "$<LINK_LIBRARY:WHOLE_ARCHIVE,tactix_ipasir>")
add_test(NAME ipasir_termination COMMAND test_ipasir_termination)
add_executable(test_ipasir_learning test_ipasir_learning.c)
target_link_libraries(test_ipasir_learning PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,tactix_ipasir>")
add_test(NAME ipasir_learning COMMAND test_ipasir_learning)
tactix_skip77_test(ipasir_learning_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_ipasir_learning_proof.py
                   $<TARGET_FILE:test_ipasir_learning> ${CMAKE_SOURCE_DIR})
add_executable(test_ipasir_core test_ipasir_core.c)
target_link_libraries(test_ipasir_core PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,tactix_ipasir>")
add_test(NAME ipasir_core COMMAND test_ipasir_core)
add_executable(test_ipasir_owned_result test_ipasir_owned_result.c)
target_link_libraries(test_ipasir_owned_result PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,tactix_ipasir>")
add_test(NAME ipasir_owned_result COMMAND test_ipasir_owned_result)
tactix_skip77_test(ipasir_owned_result_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_ipasir_owned_result_proof.py
                   $<TARGET_FILE:test_ipasir_owned_result> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(ipasir_core_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_ipasir_core_proof.py
                   $<TARGET_FILE:test_ipasir_core> ${CMAKE_SOURCE_DIR})
add_executable(test_ipasir_programmable test_ipasir_programmable.c)
target_link_libraries(test_ipasir_programmable PRIVATE
  "$<LINK_LIBRARY:WHOLE_ARCHIVE,tactix_ipasir>")
add_test(NAME ipasir_programmable COMMAND test_ipasir_programmable)
add_executable(test_ipasir_cruxmiter test_ipasir_cruxmiter.c)
target_link_libraries(test_ipasir_cruxmiter PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,tactix_ipasir>")
target_include_directories(test_ipasir_cruxmiter PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ipasir_cruxmiter COMMAND test_ipasir_cruxmiter)
tactix_skip77_test(ipasir_cruxmiter_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_ipasir_cruxmiter_proof.py
                   $<TARGET_FILE:test_ipasir_cruxmiter> ${CMAKE_SOURCE_DIR})
foreach(driver dpll cdcl-no-restarts scripted-default chvatal)
  tactix_skip77_test(ipasir_${driver}_proof ${TACTIX_PYTHON}
                    ${CMAKE_CURRENT_SOURCE_DIR}/test_ipasir_cruxmiter_proof.py
                    $<TARGET_FILE:test_ipasir_cruxmiter> ${CMAKE_SOURCE_DIR} ${driver})
endforeach()
tactix_skip77_test(ipasir_programmable_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_ipasir_programmable_proof.py
                   $<TARGET_FILE:test_ipasir_programmable> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(learned_clause_export_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_learned_clause_export_proof.py
                   $<TARGET_FILE:test_learned_clause_export> ${CMAKE_SOURCE_DIR})
add_executable(tactix-ipasir-trace ${CMAKE_SOURCE_DIR}/tools/fuzz/ipasir_trace_client.c
               ${CMAKE_SOURCE_DIR}/tools/fuzz/public_random_main.cpp)
target_compile_definitions(tactix-ipasir-trace PRIVATE TACTIX_PUBLIC_RANDOM_IPASIR=1)
target_link_libraries(tactix-ipasir-trace PRIVATE
  "$<LINK_LIBRARY:WHOLE_ARCHIVE,tactix_ipasir>")
add_executable(test_ipasir_minimize test_ipasir_minimize.c
               ${CMAKE_SOURCE_DIR}/tools/fuzz/public_random_main.cpp)
target_compile_definitions(test_ipasir_minimize PRIVATE TACTIX_PUBLIC_RANDOM_IPASIR=1)
target_link_libraries(test_ipasir_minimize PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,tactix_ipasir>")
add_test(NAME ipasir_minimize COMMAND test_ipasir_minimize)
tactix_skip77_test(ipasir_minimize_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_ipasir_minimize_proof.py
                   $<TARGET_FILE:test_ipasir_minimize> ${CMAKE_SOURCE_DIR})
tactix_python_test(ipasir_trace_oracles
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_ipasir_trace_oracles.py
                   ${CMAKE_SOURCE_DIR})
tactix_skip77_test(ipasir_trace_regression ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_ipasir_trace_regression.py
                   $<TARGET_FILE:tactix-ipasir-trace> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(ipasir_query_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_ipasir_query_proof.py
                   $<TARGET_FILE:test_ipasir_query> ${CMAKE_SOURCE_DIR})

add_executable(test_api_one_search test_api_one_search.c)
target_link_libraries(test_api_one_search PRIVATE tactix_lib)
add_test(NAME api_one_search COMMAND test_api_one_search)

add_executable(test_api_pbp_fixture api_pbp_fixture.c)
target_link_libraries(test_api_pbp_fixture PRIVATE tactix_lib)
tactix_skip77_test(api_pbp_check ${TACTIX_SKIP_UNLESS_PBP} ${TACTIX_PYTHON}
                   ${CMAKE_SOURCE_DIR}/tests/test_api_pbp_check.py
                   $<TARGET_FILE:test_api_pbp_fixture> ${CMAKE_SOURCE_DIR})
# Checker-free: Debug builds audit every deletion of an early and of a divergent late
# PBP attachment.
add_test(NAME api_pbp_early
         COMMAND test_api_pbp_fixture api-pbp-early.cnf api-pbp-early.pbp
                 early)
add_test(NAME api_pbp_late_divergent
         COMMAND test_api_pbp_fixture api-pbp-late-divergent.cnf
                 api-pbp-late-divergent.pbp divergent)

# External certificate-oracle fixture: invoked only by a pinned SLURM proof
# job because it writes a proof and requires independent checker binaries.
add_executable(condition_certificate_driver condition_certificate_driver.c)
target_link_libraries(condition_certificate_driver PRIVATE tactix_lib)

tactix_python_test(condition_compacted_model
                   ${CMAKE_SOURCE_DIR}/tests/condition_compacted_model_check.py
                   $<TARGET_FILE:test_condition>)

# tactix_apply_instance on every registry entry (the §5.6 action space) plus the
# no-payload-aliasing contract. A C client, so it also checks header/ABI sync.
add_executable(test_api_instances test_api_instances.c)
target_link_libraries(test_api_instances PRIVATE tactix_lib)
add_test(NAME api_instances COMMAND test_api_instances)

# tactix_set_guidance_publication: NATIVE public dispatch publishes rule events
# into H exactly as the shared executor does; DISCARD keeps the v1 behavior.
add_executable(test_api_guidance_publication test_api_guidance_publication.c)
target_link_libraries(test_api_guidance_publication PRIVATE tactix_lib)
add_test(NAME api_guidance_publication COMMAND test_api_guidance_publication)

add_executable(test_online_api test_online_api.c)
target_link_libraries(test_online_api PRIVATE tactix_lib)
add_test(NAME online_api COMMAND test_online_api)

add_executable(test_online_all_actions test_online_all_actions.c)
target_link_libraries(test_online_all_actions PRIVATE tactix_lib)
add_test(NAME online_all_actions COMMAND test_online_all_actions)

tactix_script_test(direct_csp_guarded_cover_proof_model
                   direct_csp_guarded_cover_proof_model.sh
                   test_direct_csp_guarded_cover_live)
tactix_script_test(direct_csp_guarded_cover_owner_refresh_proof_model
                   direct_csp_guarded_cover_owner_refresh_proof_model.sh
                   test_direct_csp_guarded_cover_owner_refresh)
tactix_script_test(proof_premise_lease_proof
                   proof_premise_lease_proof.sh
                   test_proof_premise_lease_lifecycle)
tactix_script_test(affine_matrix_persistent_lifecycle_proof_model
                   affine_matrix_persistent_lifecycle_proof_model.sh
                   test_affine_matrix_persistent_lifecycle)
tactix_script_test(default_affine_search_proof_model
                   default_affine_search_proof_model.sh
                   test_default_affine_search)
tactix_script_test(default_affine_restart_trigger_proof
                   default_affine_restart_trigger_proof.sh
                   test_default_affine_restart_trigger)
tactix_skip77_test(affine_matrix_actual_geometry_proof_model
                   bash ${CMAKE_SOURCE_DIR}/tests/affine_matrix_actual_geometry_proof_model.sh
                   $<TARGET_FILE:test_affine_matrix_actual_geometry> ${CMAKE_SOURCE_DIR})

foreach(normalization_test affine_root_image affine_matrix_root_normalized)
  add_test(NAME ${normalization_test}_proof_model
           COMMAND bash ${CMAKE_SOURCE_DIR}/tests/affine_root_normalization_proof_model.sh
                   $<TARGET_FILE:test_${normalization_test}> ${CMAKE_SOURCE_DIR})
  set_tests_properties(${normalization_test}_proof_model PROPERTIES SKIP_RETURN_CODE 77)
endforeach()

tactix_script_test(direct_csp_support_proof_model
                   direct_csp_support_proof_model.sh
                   test_direct_csp_support_action)

# Tiny UNSATs through the real binary, proofs verified by drat-trim (skips 77).
tactix_skip77_test(micro_proofs ${CMAKE_SOURCE_DIR}/tests/micro_proofs.sh
                   $<TARGET_FILE:tactix> ${CMAKE_SOURCE_DIR}
                   $<TARGET_FILE:test_analyze>
                   $<TARGET_FILE:test_congruence>
                   $<TARGET_FILE:test_restart_reduce>
                   $<TARGET_FILE:test_factor_n1>
                   $<TARGET_FILE:test_online_inprocess>
                   $<TARGET_FILE:test_ulc_rat>
                   $<TARGET_FILE:tactix-public-random>)

tactix_script_test(nary_csp_equality_proof nary_csp_equality_proof.sh
                   test_nary_csp_equality)
tactix_script_test(nary_csp_equality_live_proof nary_csp_equality_live_proof.sh
                   test_nary_csp_equality_live)
tactix_script_test(nary_csp_equality_public_oracle
                   nary_csp_equality_public_oracle.sh
                   test_nary_csp_equality_public)
tactix_script_test(nary_csp_equality_amo_proof nary_csp_equality_amo_proof.sh
                   test_nary_csp_equality_amo)

# A persistent GF(2) event needs one arbitrary-width clausal reason, not the
# complete parity CNF. Verify the targeted RUP export and helper retirement.
tactix_script_test(parity_wide_reason_proof parity_wide_reason_proof.sh
                   test_parity_lifecycle)

# The pure persistent-matrix bridge must certify one wide event reason, retire
# all proof-only helpers, and leave only that reason marked for installation.
tactix_script_test(affine_matrix_transaction_proof
                   affine_matrix_transaction_proof.sh
                   test_affine_matrix_transaction)

# The persistent matrix live bridge emits its preflighted helper proof and
# reason, while ordinary watched-clause propagation consumes the result.
tactix_script_test(affine_matrix_live_proof affine_matrix_live_proof.sh
                   test_affine_matrix_live)
tactix_script_test(affine_matrix_search_proof affine_matrix_search_proof.sh
                   test_affine_matrix_search)
tactix_script_test(gf3_general_row_public_proof gf3_general_row_public_proof.sh
                   test_gf3_general_row_public)
tactix_script_test(cardinality_search_proof cardinality_search_proof.sh
                   test_cardinality_search)
tactix_script_test(cardinality_search_live_proof
                   cardinality_search_live_proof.sh
                   test_cardinality_search_live)

# Sweep: every proof it produces, replayed through the real checkers (drat-trim
# and the DSR pipeline), in sub-solver space and again after the sub-solver ->
# kernel translation. Skips 77 without a checker.
tactix_skip77_test(sweep_proofs ${CMAKE_SOURCE_DIR}/tests/sweep_proofs.sh
                   ${CMAKE_SOURCE_DIR}
                   $<TARGET_FILE:tactix-public-random>
                   $<TARGET_FILE:test_subsolver> $<TARGET_FILE:test_sweep>)

# A satisfiable Sweep replay must still emit a checkable proof prefix; see
# benchmarks/reports/2026-09-25-am455-latest-source-fuzz-consolidation.md.
tactix_skip77_test(sweep_retarget
                   ${CMAKE_SOURCE_DIR}/tests/skip_unless.sh
                   ${CMAKE_SOURCE_DIR}/tools/checkers/bin/drat-trim --
                   ${CMAKE_SOURCE_DIR}/tests/sweep_retarget.sh
                   ${CMAKE_SOURCE_DIR} $<TARGET_FILE:tactix-public-random>)

# V4 exhaustive model-equivalence oracle: 4 modes x 50k random 5-var formulas,
# model sets compared by enumeration -- the only direct check that V4's deletions
# preserve equivalence, since a proof checker accepts any delete line.
add_executable(test_tr_bruteforce test_tr_bruteforce.cpp)
target_link_libraries(test_tr_bruteforce PRIVATE tactix_lib)
foreach(mode 0 1 2 3)
  add_test(NAME tr_bruteforce_${mode} COMMAND test_tr_bruteforce ${mode})
endforeach()

tactix_python_test(inspect_policy_feedback
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_inspect_policy_feedback.py
                   $<TARGET_FILE:test_feedback_trace>
                   $<TARGET_FILE:test_default_execution_signature>)
add_test(NAME feedback_frontend
         COMMAND ${CMAKE_SOURCE_DIR}/tests/skip_unless.sh ${CMAKE_SOURCE_DIR}/tools/checkers/bin/drat-trim ${CMAKE_SOURCE_DIR}/tools/checkers/bin/cake_lpr --
                 ${CMAKE_COMMAND} -E env ASAN_OPTIONS=detect_leaks=0 python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_feedback_frontend.py
                 $<TARGET_FILE:tactix>)
set_tests_properties(feedback_frontend PROPERTIES SKIP_RETURN_CODE 77)

tactix_skip77_test(startup_parity_check ${TACTIX_SKIP_UNLESS_PBP} ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_startup_parity_check.py
                   $<TARGET_FILE:test_startup_parity> $<TARGET_FILE:tactix>)
tactix_skip77_test(count_preservation_check ${TACTIX_SKIP_UNLESS_PBP}
                   ${CMAKE_SOURCE_DIR}/tests/skip_unless.sh
                   ${CMAKE_SOURCE_DIR}/tools/checkers/bin/dsr-trim
                   ${CMAKE_SOURCE_DIR}/tools/checkers/bin/dsr-compress
                   ${CMAKE_SOURCE_DIR}/tools/checkers/bin/srcheck
                   ${CMAKE_SOURCE_DIR}/tools/checkers/bin/drat-trim
                   ${CMAKE_SOURCE_DIR}/tools/checkers/bin/cake_lpr -- ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_count_preservation_check.py
                   $<TARGET_FILE:test_count_preservation> $<TARGET_FILE:tactix>)
tactix_skip77_test(ramsey_cli
                   ${CMAKE_SOURCE_DIR}/tests/skip_unless.sh
                   ${CMAKE_SOURCE_DIR}/tools/checkers/bin/dsr-trim
                   ${CMAKE_SOURCE_DIR}/tools/checkers/bin/dsr-compress
                   ${CMAKE_SOURCE_DIR}/tools/checkers/bin/srcheck --
                   ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_ramsey_cli.py
                   $<TARGET_FILE:tactix>)

# A source conformance check, not a unit test: every Kernel scheduling-config
# field must have the writer its classification claims.
tactix_python_test(config_wiring
                   ${CMAKE_SOURCE_DIR}/tools/docs/check-config-wiring.py)

# Every rule the inprocessing slot schedules (every RuleTier::Inprocess registry
# line, since registering is scheduling) must have a governor: an EFFORT_SPECS
# row, or a named alternate whose mechanism is still in the source.
tactix_python_test(effort_specs
                   ${CMAKE_SOURCE_DIR}/tools/docs/check-effort-specs.py)

# The §5.6 reward channel: every dosed rule exposes a (yield, effort) pair,
# api.cpp's macro-labeled cases follow header numbering, and every case returns
# the counter its macro names -- a swapped pair inverts the learner's cost axis.
tactix_python_test(stat_ids ${CMAKE_SOURCE_DIR}/tools/docs/check-stat-ids.py)

tactix_python_test(default_strategy_config_check
                   ${CMAKE_SOURCE_DIR}/tools/docs/check-default-strategy-config.py)

# Kernel/H boundary guard (plan §5): kernel.hpp holds struct Heuristics as an
# opaque pointer only and must never #include policies/heuristics.hpp.
add_test(NAME kernel_h_boundary
         COMMAND bash -c "! grep -qE '^[[:space:]]*#[[:space:]]*include.*heuristics\\.hpp' '${CMAKE_SOURCE_DIR}/src/kernel/kernel.hpp'")

tactix_python_test(rule_policy_boundary
                   ${CMAKE_SOURCE_DIR}/tools/docs/check-rule-policy-boundary.py)

# DefaultPolicy static/hardcoded twin (plan §8.4 correspondence gate): one
# source over the static and hardcoded action ports; the hardcoded executable
# runs the static one and compares.
add_executable(default_correspondence_static test_default_port_correspondence.cpp)
target_link_libraries(default_correspondence_static PRIVATE tactix_lib)
tactix_port_twin(test_default_port_correspondence test_default_port_correspondence.cpp
                 $<TARGET_OBJECTS:tactix_default_policy_hardcoded>)
add_dependencies(test_default_port_correspondence default_correspondence_static)
add_test(NAME default_port_correspondence
         COMMAND test_default_port_correspondence
                 $<TARGET_FILE:default_correspondence_static>)

# The C default strategy (examples/default_strategy) against DefaultPolicy on
# the same correspondence fixtures.
add_executable(test_c_default_correspondence test_c_default_correspondence.cpp)
target_link_libraries(test_c_default_correspondence PRIVATE tactix_default tactix_lib)
target_compile_definitions(test_c_default_correspondence PRIVATE
                           TACTIX_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME c_default_correspondence COMMAND test_c_default_correspondence)
# The generated, profile and structured fixture families (extended).
add_test(NAME c_default_correspondence_extended
         COMMAND test_c_default_correspondence --extended)

# Qualification random strategy: an external client compiled only against the
# installed C header. It drives one public action at a time and never includes
# or links an internal policy implementation directly.
add_executable(tactix-public-random
               ${CMAKE_SOURCE_DIR}/tools/fuzz/public_random_main.cpp)
target_link_libraries(tactix-public-random PRIVATE tactix_lib)
add_executable(tactix-default-er-fuzz
               ${CMAKE_SOURCE_DIR}/tools/fuzz/default_er_main.cpp)
target_link_libraries(tactix-default-er-fuzz PRIVATE tactix_lib)
tactix_python_test(default_er_frontend
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_default_er_frontend.py
                   $<TARGET_FILE:tactix-default-er-fuzz>)
add_test(NAME public_random_frontend
         COMMAND ${CMAKE_COMMAND} -E env ASAN_OPTIONS=detect_leaks=0
                 python3 ${CMAKE_CURRENT_SOURCE_DIR}/test_public_random_frontend.py
                 $<TARGET_FILE:tactix-public-random>
                 $<TARGET_FILE:tactix>
                 ${CMAKE_SOURCE_DIR})

tactix_skip77_test(query_kernel_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_query_kernel_proof.py
                   $<TARGET_FILE:test_query_kernel> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(query_retention_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_query_retention_proof.py
                   $<TARGET_FILE:test_query_retention> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(query_projection_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_query_projection_proof.py
                   $<TARGET_FILE:test_query_projection> ${CMAKE_SOURCE_DIR}
                   $<TARGET_FILE:test_default_query>)
tactix_skip77_test(weakening_history_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_weakening_history_proof.py
                   $<TARGET_FILE:test_weakening_history> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(native_query_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_native_query_proof.py
                   $<TARGET_FILE:test_native_query> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(query_namespace_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_query_namespace_proof.py
                   $<TARGET_FILE:test_query_namespace> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(query_proof_names_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_query_proof_names_proof.py
                   $<TARGET_FILE:test_query_proof_names> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(native_query_sr_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_native_query_sr_proof.py
                   $<TARGET_FILE:test_native_query_sr> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(native_query_append_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_native_query_append_proof.py
                   $<TARGET_FILE:test_native_query_append> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(native_repair_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_native_repair_proof.py
                   $<TARGET_FILE:test_native_repair> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(derivation_history_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_derivation_history_proof.py
                   $<TARGET_FILE:test_derivation_history> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(derivation_sr_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_derivation_sr_proof.py
                   $<TARGET_FILE:test_derivation_sr> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(native_parity_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_native_parity_proof.py
                   $<TARGET_FILE:test_native_parity> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(native_restrictions_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_native_restrictions_proof.py
                   $<TARGET_FILE:test_native_restrictions> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(native_rollback_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_native_rollback_proof.py
                   $<TARGET_FILE:test_native_rollback> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(query_repair_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_query_repair_proof.py
                   $<TARGET_FILE:test_query_repair> ${CMAKE_SOURCE_DIR})
tactix_skip77_test(query_results_proof ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_query_results_proof.py
                   $<TARGET_FILE:test_query_results> ${CMAKE_SOURCE_DIR})

# Conventional C++ tests are discovered automatically: a test_<name>.cpp
# without an explicit target above gets a matching executable and CTest entry
# without a central edit. C and shell/integration tests remain explicit because
# their contracts need extra sources, arguments, or skip handling.
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

# GCC's -O3 analysis of the inlined clause copy in these tests' proof sinks
# reports an out-of-bounds memcpy (a false positive).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  foreach(test_target test_cardinality_certificate test_cardinality_search)
    target_compile_options(${test_target} PRIVATE
      -Wno-array-bounds -Wno-stringop-overread)
  endforeach()
endif()

# The import-free count-cache subset of the vivify compact contract, in its own
# process so its digest line matches the frozen-parent control.
add_test(NAME vivify_count_cache
         COMMAND test_vivify_compact_contract --count-cache)

# The full-device failure fixture is unavailable on platforms without /dev/full.
set_tests_properties(proof_output_failure PROPERTIES SKIP_RETURN_CODE 77)

# Bound the empty-scope interruption regression if stop polling regresses.
# The nonvacuous 728-graph clique-order sweep also exercises bounded prefixes;
# the full online suite takes about 21 seconds under ASan/UBSan.
set_tests_properties(online_inprocess PROPERTIES TIMEOUT 60)

tactix_skip77_test(tseitin_cycle_action_journal
                   ${CMAKE_SOURCE_DIR}/tests/skip_unless.sh
                   ${CMAKE_SOURCE_DIR}/tools/checkers/bin/dsr-trim
                   ${CMAKE_SOURCE_DIR}/tools/checkers/bin/srcheck --
                   ${TACTIX_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_tseitin_cycle_action_journal.py
                   $<TARGET_FILE:test_tseitin_cycle_action>)

tactix_script_test(nary_csp_equality_transitivity_proof_model
                   nary_csp_equality_transitivity_proof_model.sh
                   test_nary_csp_equality_transitivity_live)
