# Unit tests (plain asserts, no framework — kernel discipline, no deps).
# The C-language API test doubles as the header/ABI sync check for api.cpp.

add_executable(test_kernel test_kernel.cpp)
target_link_libraries(test_kernel PRIVATE tactix_lib)
add_test(NAME kernel COMMAND test_kernel)

add_executable(test_parse test_parse.cpp)
target_link_libraries(test_parse PRIVATE tactix_lib)
add_test(NAME parse COMMAND test_parse)

# The proof backend's serializers, byte for byte (N2's writer).
add_executable(test_proof test_proof.cpp)
target_link_libraries(test_proof PRIVATE tactix_lib)
add_test(NAME proof COMMAND test_proof)

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

# Sweep: candidate selection + environment construction. An arg raises the
# brute-force oracle's soak size.
add_executable(test_sweep test_sweep.cpp)
target_link_libraries(test_sweep PRIVATE tactix_lib)
add_test(NAME sweep COMMAND test_sweep)

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
                 ${CMAKE_SOURCE_DIR}/src/policies/event_trace.cpp)
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
  add_test(NAME default_policy_rule_journal
           COMMAND ${CMAKE_COMMAND} -E env python3
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_tactix_default_journal.py
                   $<TARGET_FILE:tactix>)
endif()
add_test(NAME p4_fixtures
         COMMAND ${CMAKE_COMMAND} -E env python3
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_p4_fixtures.py)
if(NOT TACTIX_ENABLE_EVENT_TRACE AND NOT TACTIX_ENABLE_RULE_JOURNAL)
  add_test(NAME event_trace_zero_overhead
           COMMAND ${CMAKE_COMMAND} -E env python3
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_no_event_trace_symbols.py
                   $<TARGET_FILE:tactix>)
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

add_executable(test_lucky test_lucky.cpp)
target_link_libraries(test_lucky PRIVATE tactix_lib)
add_test(NAME lucky COMMAND test_lucky)

add_executable(test_api_c test_api_c.c)
target_link_libraries(test_api_c PRIVATE tactix_lib)
target_include_directories(test_api_c PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME api_n1 COMMAND test_api_c)

# tactix_apply_instance on every registry entry (the §5.6 action space) plus the
# no-payload-aliasing contract. A C client, so it also checks header/ABI sync.
add_executable(test_api_instances test_api_instances.c)
target_link_libraries(test_api_instances PRIVATE tactix_lib)
target_include_directories(test_api_instances PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME api_instances COMMAND test_api_instances)

add_executable(test_random_policy test_random_policy.c)
target_link_libraries(test_random_policy PRIVATE tactix_lib)
target_include_directories(test_random_policy PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME random_policy COMMAND test_random_policy)

# --trace/--replay (the §8.5 offline replay harness) plus the cross-process
# reproducibility of --random-policy=SEED. Runs the real binary; never skips.
add_test(NAME trace_replay
         COMMAND ${CMAKE_SOURCE_DIR}/tests/trace_replay.sh $<TARGET_FILE:tactix>)

# Tiny UNSATs through the real binary, proofs verified by drat-trim (skips 77).
add_test(NAME micro_proofs
         COMMAND ${CMAKE_SOURCE_DIR}/tests/micro_proofs.sh
                 $<TARGET_FILE:tactix> ${CMAKE_SOURCE_DIR})
set_tests_properties(micro_proofs PROPERTIES SKIP_RETURN_CODE 77)

# Sweep: every proof it produces, replayed through the real checkers (drat-trim
# for the lemmas, lrat-check for the chains), in sub-solver space and again
# after the sub-solver -> kernel translation. Skips 77 without a checker.
add_test(NAME sweep_proofs
         COMMAND ${CMAKE_SOURCE_DIR}/tests/sweep_proofs.sh ${CMAKE_SOURCE_DIR}
                 $<TARGET_FILE:test_subsolver> $<TARGET_FILE:test_sweep>)
set_tests_properties(sweep_proofs PROPERTIES SKIP_RETURN_CODE 77)

# V4 exhaustive model-equivalence oracle: 4 modes x 50k random 5-var formulas,
# model sets compared by enumeration -- the only direct check that V4's deletions
# preserve equivalence, since a proof checker accepts any delete line.
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
