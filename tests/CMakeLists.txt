# Unit tests (plain asserts, no framework — kernel discipline, no deps).
# The C-language API test doubles as the header/ABI sync check for api.cpp.

add_executable(test_kernel test_kernel.cpp)
target_link_libraries(test_kernel PRIVATE tactix_lib)
add_test(NAME kernel COMMAND test_kernel)

add_executable(test_analyze test_analyze.cpp)
target_link_libraries(test_analyze PRIVATE tactix_lib)
add_test(NAME analyze COMMAND test_analyze)

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

add_executable(test_reorder test_reorder.cpp)
target_link_libraries(test_reorder PRIVATE tactix_lib)
add_test(NAME reorder COMMAND test_reorder)

add_executable(test_walk test_walk.cpp)
target_link_libraries(test_walk PRIVATE tactix_lib)
add_test(NAME walk COMMAND test_walk)

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

add_executable(test_probe test_probe.cpp)
target_link_libraries(test_probe PRIVATE tactix_lib)
add_test(NAME probe COMMAND test_probe)

add_executable(test_backbone test_backbone.cpp)
target_link_libraries(test_backbone PRIVATE tactix_lib)
add_test(NAME backbone COMMAND test_backbone)

add_executable(test_bce test_bce.cpp)
target_link_libraries(test_bce PRIVATE tactix_lib)
add_test(NAME bce COMMAND test_bce)

add_executable(test_ternary test_ternary.cpp)
target_link_libraries(test_ternary PRIVATE tactix_lib)
add_test(NAME ternary COMMAND test_ternary)

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

add_executable(test_lucky test_lucky.cpp)
target_link_libraries(test_lucky PRIVATE tactix_lib)
add_test(NAME lucky COMMAND test_lucky)

add_executable(test_api_c test_api_c.c)
target_link_libraries(test_api_c PRIVATE tactix_lib)
target_include_directories(test_api_c PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME api_n1 COMMAND test_api_c)

add_executable(test_random_policy test_random_policy.c)
target_link_libraries(test_random_policy PRIVATE tactix_lib)
target_include_directories(test_random_policy PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME random_policy COMMAND test_random_policy)

# micro-formula proof test: tiny UNSATs through the real binary, proofs
# verified by drat-trim (skips with 77 when the checker isn't built)
add_test(NAME micro_proofs
         COMMAND ${CMAKE_SOURCE_DIR}/tests/micro_proofs.sh
                 $<TARGET_FILE:tactix> ${CMAKE_SOURCE_DIR})
set_tests_properties(micro_proofs PROPERTIES SKIP_RETURN_CODE 77)

# V5 Sweep S-b: the sub-solver's derivations replayed through the real checkers
# (drat-trim for the lemmas, lrat-check for the chains). Skips with 77 without.
add_test(NAME sweep_proofs
         COMMAND ${CMAKE_SOURCE_DIR}/tests/sweep_proofs.sh
                 $<TARGET_FILE:test_subsolver> ${CMAKE_SOURCE_DIR})
set_tests_properties(sweep_proofs PROPERTIES SKIP_RETURN_CODE 77)

# V4 exhaustive model-equivalence oracle: 4 modes x 50k random 5-var formulas,
# model sets compared by enumeration (the ONLY direct test that V4's deletions
# are equivalence-preserving — a proof checker accepts any delete line). Pass a
# second arg to raise the iteration count for a soak.
add_executable(test_tr_bruteforce test_tr_bruteforce.cpp)
target_link_libraries(test_tr_bruteforce PRIVATE tactix_lib)
foreach(mode 0 1 2 3)
  add_test(NAME tr_bruteforce_${mode} COMMAND test_tr_bruteforce ${mode})
endforeach()

add_executable(test_factor test_factor.cpp)
target_link_libraries(test_factor PRIVATE tactix_lib)
add_test(NAME factor COMMAND test_factor)

add_executable(test_factor_n1 test_factor_n1.cpp)
target_link_libraries(test_factor_n1 PRIVATE tactix_lib)
add_test(NAME factor_n1 COMMAND test_factor_n1)

# SCHED-A2: not a unit test but a SOURCE conformance check -- every Kernel
# scheduling-config field must have the writer its classification claims. Runs
# in ctest so an unwired POLICY field, or a new unclassified field, breaks the
# build rather than waiting to be noticed.
add_test(NAME config_wiring
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-config-wiring.py)

# Track H . H5: `H` is optional -- a null-H solver is sound but unguided.
add_executable(test_no_heuristics test_no_heuristics.cpp)
target_link_libraries(test_no_heuristics PRIVATE tactix_lib)
add_test(NAME no_heuristics COMMAND test_no_heuristics)

# Track H boundary guard (plan Sec 5, rev 62): the kernel interface must not
# depend on H's layout -- kernel.hpp holds struct Heuristics only as an opaque
# pointer and must NEVER #include policies/heuristics.hpp. A real include would
# also compile-fail via the include cycle, but this gives a fast, clear signal.
add_test(NAME kernel_h_boundary
         COMMAND bash -c "! grep -qE '^[[:space:]]*#[[:space:]]*include.*heuristics\\.hpp' '${CMAKE_SOURCE_DIR}/src/kernel/kernel.hpp'")
