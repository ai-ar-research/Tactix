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

# Sweep: candidate selection + environment construction. Pass an
# iteration count to raise the brute-force oracle's soak size.
add_executable(test_sweep test_sweep.cpp)
target_link_libraries(test_sweep PRIVATE tactix_lib)
add_test(NAME sweep COMMAND test_sweep)

add_executable(test_scheduling test_scheduling.cpp)
target_link_libraries(test_scheduling PRIVATE tactix_lib)
add_test(NAME scheduling COMMAND test_scheduling)

# RV-S13: the §5.6 POSE surface — a preposed slot, a disabled entry proved inert
# at its rule's own counter, and the TACTIX_SLOT_*/CURSOR_*/ORDER_* knobs shown
# to reach the entry the policy walks. None of it had any coverage.
add_executable(test_slot_pose test_slot_pose.cpp)
target_link_libraries(test_slot_pose PRIVATE tactix_lib)
add_test(NAME slot_pose COMMAND test_slot_pose)

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

# N1: every Vivify band reachable through the API, and the unscoped path
# excluding none. The Factor breach (test_factor_n1) in a second rule.
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

# RV-S14: tactix_apply_instance on EVERY registry entry (the §5.6 action space)
# + the no-payload-aliasing contract. Also a C client, so it shares test_api_c's
# header/ABI sync property.
add_executable(test_api_instances test_api_instances.c)
target_link_libraries(test_api_instances PRIVATE tactix_lib)
target_include_directories(test_api_instances PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME api_instances COMMAND test_api_instances)

add_executable(test_random_policy test_random_policy.c)
target_link_libraries(test_random_policy PRIVATE tactix_lib)
target_include_directories(test_random_policy PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME random_policy COMMAND test_random_policy)

# RV-S12: --trace/--replay, the §8.5 offline replay harness, plus the
# cross-process reproducibility of --random-policy=SEED that the whole fuzz
# harness rests on. Runs the real binary; no checker needed, so it never skips.
add_test(NAME trace_replay
         COMMAND ${CMAKE_SOURCE_DIR}/tests/trace_replay.sh $<TARGET_FILE:tactix>)

# micro-formula proof test: tiny UNSATs through the real binary, proofs
# verified by drat-trim (skips with 77 when the checker isn't built)
add_test(NAME micro_proofs
         COMMAND ${CMAKE_SOURCE_DIR}/tests/micro_proofs.sh
                 $<TARGET_FILE:tactix> ${CMAKE_SOURCE_DIR})
set_tests_properties(micro_proofs PROPERTIES SKIP_RETURN_CODE 77)

# Sweep: every proof it produces, replayed through the real checkers (drat-trim
# for the lemmas, lrat-check for the chains). test_subsolver covers the
# derivations in SUB-SOLVER space; test_sweep covers the same derivations after
# the sub-solver -> kernel translation, which is separate code and is where the
# S-e polarity N2 breach lived. Skips with 77 without a checker.
add_test(NAME sweep_proofs
         COMMAND ${CMAKE_SOURCE_DIR}/tests/sweep_proofs.sh ${CMAKE_SOURCE_DIR}
                 $<TARGET_FILE:test_subsolver> $<TARGET_FILE:test_sweep>)
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

# Not a unit test but a SOURCE conformance check -- every Kernel
# scheduling-config field must have the writer its classification claims. Runs
# in ctest so an unwired POLICY field, or a new unclassified field, breaks the
# build rather than waiting to be noticed.
add_test(NAME config_wiring
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-config-wiring.py)

# SCHED-A2b: the same shape, one level up -- every rule the inprocessing SLOT
# schedules (i.e. every RuleTier::Inprocess registry line, since registering IS
# scheduling) must have a governor: an EFFORT_SPECS row, or a named alternate
# one whose mechanism is still in the source. Nothing else covers this: fuzz is
# green on a performance regression, and the unit tests call a rule's apply
# directly without traversing the slot.
add_test(NAME effort_specs
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-effort-specs.py)

# SCHED-A3: the same shape again, but taken over the REFERENCE's option sets
# rather than over our own source. The parity ledger is only as good as the set
# it was generated from, so every option in kissat/src/options.h and
# cadical/src/options.hpp must be accounted for -- a ledger row, §13.6's
# dismissal roster, or a named carried/exempt roster entry. A reference bump
# that adds an option therefore fails here instead of silently turning the
# ledger back into a lower bound. Requires the untracked reference clones
# (CLAUDE.md); it FAILS rather than skips without them, since a conformance
# check that can go vacuously green is worse than a red one.
add_test(NAME option_coverage
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-option-coverage.py)

# RV-S11, the same shape over the §5.6 REWARD channel: every dosed rule exposes
# a (yield, effort) pair, header numbering matches api.cpp's case order, and
# every case returns the counter its macro names. The suite's only previous
# evidence here was `tactix_stat(s, id) != UINT64_MAX`, a presence check that a
# swapped pair passes while the learner's cost axis inverts.
add_test(NAME stat_ids
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-stat-ids.py)

# Same shape, over source COMMENTS: they carry the invariant plus a reference
# pointer, never the narrative of how we found out -- no dates, SLURM job ids,
# window ids or score numbers (standing user directive, given twice). A one-off
# sweep enforced it and the pattern was back within the hour, so it is enforced
# here instead of remembered. The checker names its own rule; see it for what
# counts as an allowed reference pointer.
add_test(NAME comment_hygiene
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_SOURCE_DIR}/tools/docs/check-comment-hygiene.py)

# `H` is optional -- a null-H solver is sound but unguided.
add_executable(test_no_heuristics test_no_heuristics.cpp)
target_link_libraries(test_no_heuristics PRIVATE tactix_lib)
add_test(NAME no_heuristics COMMAND test_no_heuristics)

# Kernel/H boundary guard (plan Sec 5): the kernel interface must not
# depend on H's layout -- kernel.hpp holds struct Heuristics only as an opaque
# pointer and must NEVER #include policies/heuristics.hpp. A real include would
# also compile-fail via the include cycle, but this gives a fast, clear signal.
add_test(NAME kernel_h_boundary
         COMMAND bash -c "! grep -qE '^[[:space:]]*#[[:space:]]*include.*heuristics\\.hpp' '${CMAKE_SOURCE_DIR}/src/kernel/kernel.hpp'")

# SCHED-H2: a reused policy object must be indistinguishable from a fresh one
# — the property the phase-6 learner's many-solves-per-process loop rests on.
add_executable(test_solve_reset test_solve_reset.cpp)
target_link_libraries(test_solve_reset PRIVATE tactix_lib)
add_test(NAME solve_reset COMMAND test_solve_reset)
