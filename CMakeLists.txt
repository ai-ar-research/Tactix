cmake_minimum_required(VERSION 3.25)
project(tactix VERSION 0.2.1 LANGUAGES CXX C)

# A fully static executable (glibc, libstdc++, zlib), for binary releases.
option(TACTIX_STATIC_BINARY "Link the tactix executable statically" OFF)
if(TACTIX_STATIC_BINARY)
  set(ZLIB_USE_STATIC_LIBS ON)
endif()
find_package(ZLIB REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Semantic execution tracing is a diagnostic build feature.  Production pins
# use inline no-op methods and do not link the implementation, so decision and
# conflict hot paths contain no runtime trace branch.
option(TACTIX_ENABLE_EVENT_TRACE "Compile semantic event tracing" OFF)
option(TACTIX_ENABLE_RULE_JOURNAL "Compile semantic rule journaling" OFF)
# N2 population oracle: every emitted proof deletion must be backed by a clause
# the checker still holds. Production pins compile none of it (the emission
# sites are #ifdef-ed out and `Proof::population_shadow()` folds to false).
option(TACTIX_ENABLE_PROOF_SHADOW "Compile the proof-population oracle" OFF)
# Kernel discipline (plan §5): C-style core, no exceptions/RTTI in the hot
# solver. Two narrow exception islands catch failures before their noexcept
# boundaries: the pinned graph backend and optional embedded-proof export.
add_compile_options(-Wall -Wextra -fno-exceptions $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)

# Tests and examples are built when Tactix is the top-level project; a parent
# project that adds it with add_subdirectory gets only the solver and library.
option(TACTIX_BUILD_TESTS "Build the tests and example strategies" ${PROJECT_IS_TOP_LEVEL})

# DejaVu is a pinned graph-automorphism backend whose public implementation
# uses exceptions and RTTI.  Keep it in a separate translation unit with a
# narrow, noexcept boundary; the solver core retains its C-style build flags.
add_library(tactix_symmetry_backend STATIC
  src/symmetry_backend/dejavu_backend.cpp)
target_include_directories(tactix_symmetry_backend PRIVATE src)
target_include_directories(tactix_symmetry_backend SYSTEM PRIVATE
  third_party/dejavu-4c275e9)
target_compile_options(tactix_symmetry_backend PRIVATE
  -fexceptions -frtti -fvisibility=hidden)

# OBJECT (not STATIC): rule registration happens in static initializers
# (TACTIX_REGISTER_RULE), and an archive lets the linker drop any rule TU
# nothing references directly — exactly the situation the registry-driven
# scheduler creates (rules reached ONLY through the registry). An object
# library keeps every registered rule in every binary (§5.3: one directory
# + one registration line must be sufficient).
file(GLOB_RECURSE TACTIX_RULE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rules/*/*.cpp")
file(GLOB_RECURSE TACTIX_POLICY_OP_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/policies/ops/*/*.cpp")
file(GLOB_RECURSE TACTIX_STRATEGY_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/strategy/*.cpp")

add_library(tactix_lib OBJECT
  src/kernel/kernel.cpp
  src/population/population_observer.cpp
  src/proof/proof.cpp
  src/proof/dsr_to_pbp.cpp
  src/proof/direct_php_recognition.cpp
  src/rules/registry.cpp
  ${TACTIX_RULE_SOURCES}
  ${TACTIX_POLICY_OP_SOURCES}
  src/engine/subsolver.cpp
  src/engine/subsolver_proof_export.cpp
  src/policies/heuristics.cpp
  src/policies/default_policy.cpp
  src/policies/strategy_settings.cpp
  src/policies/progress.cpp
  src/policies/structured_root_strategy.cpp
  ${TACTIX_STRATEGY_SOURCES}
  src/frontend/parse.cpp
  src/frontend/dsr_follow_on.cpp
  src/api/api.cpp
  src/api/api_work.cpp
)

# Proof export can copy a huge learned core. Only this translation unit may
# catch allocation failure; `SubSolver::derivation` is noexcept to callers.
# Keep this island optimized in Debug too (as in the debug preset). Apple
# Clang 15 can otherwise coalesce libc++ vector constructors with copies from
# -fno-exceptions objects and bypass the allocation-failure handler.
set_source_files_properties(src/engine/subsolver_proof_export.cpp PROPERTIES
  COMPILE_OPTIONS "-fexceptions;$<$<CONFIG:Debug>:-O1>")
if(TACTIX_ENABLE_EVENT_TRACE)
  target_sources(tactix_lib PRIVATE src/diagnostics/event_trace.cpp)
  target_compile_definitions(tactix_lib PUBLIC TACTIX_ENABLE_EVENT_TRACE=1)
endif()
if(TACTIX_ENABLE_RULE_JOURNAL)
  target_sources(tactix_lib PRIVATE src/diagnostics/rule_journal.cpp)
  target_compile_definitions(tactix_lib PUBLIC TACTIX_ENABLE_RULE_JOURNAL=1)
endif()
if(TACTIX_ENABLE_PROOF_SHADOW)
  target_compile_definitions(tactix_lib PUBLIC TACTIX_ENABLE_PROOF_SHADOW=1)
  # backtrace_symbols_fd names the emitting rule without a debugger.
  target_link_options(tactix_lib PUBLIC -rdynamic)
endif()
target_include_directories(tactix_lib PUBLIC src include)
target_link_libraries(tactix_lib PUBLIC tactix_symmetry_backend ZLIB::ZLIB)

add_executable(tactix src/main.cpp)
target_link_libraries(tactix PRIVATE tactix_lib)

# DefaultPolicy over another action port (strategy/action_port.hpp): every
# tactix object except the production default_policy.cpp, plus a translation
# unit that recompiles it over that port. tactix-hardcoded is the §8.4
# hardcoded twin used by the paper's interface-overhead experiment.
set(TACTIX_PORT_TWIN_OBJECTS
    "$<FILTER:$<TARGET_OBJECTS:tactix_lib>,EXCLUDE,/default_policy\\.cpp\\.o$>")
function(tactix_port_usage target)
  target_include_directories(${target} PRIVATE
    $<TARGET_PROPERTY:tactix_lib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_compile_definitions(${target} PRIVATE
    $<TARGET_PROPERTY:tactix_lib,INTERFACE_COMPILE_DEFINITIONS>)
endfunction()
function(tactix_port_twin target)
  add_executable(${target} ${ARGN} ${TACTIX_PORT_TWIN_OBJECTS})
  add_dependencies(${target} tactix_lib)
  tactix_port_usage(${target})
  target_link_options(${target} PRIVATE $<TARGET_PROPERTY:tactix_lib,INTERFACE_LINK_OPTIONS>)
  target_link_libraries(${target} PRIVATE tactix_symmetry_backend ZLIB::ZLIB)
endfunction()
add_library(tactix_default_policy_hardcoded OBJECT src/policies/default_policy_hardcoded.cpp)
tactix_port_usage(tactix_default_policy_hardcoded)
tactix_port_twin(tactix-hardcoded src/main.cpp
                 $<TARGET_OBJECTS:tactix_default_policy_hardcoded>)

if(TACTIX_STATIC_BINARY)
  target_link_options(tactix PRIVATE -static)
endif()

# The installable library: libtactix.a with every rule. Rules register in
# static initializers, so consumers link the archive whole (the installed
# CMake package and pkg-config file do this).
add_library(tactix_archive STATIC $<TARGET_OBJECTS:tactix_lib>
                                  $<TARGET_OBJECTS:tactix_symmetry_backend>)
set_target_properties(tactix_archive PROPERTIES OUTPUT_NAME tactix)

include(GNUInstallDirs)
install(TARGETS tactix RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS tactix_archive ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/tactix.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/TactixConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/TactixConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Tactix
  PATH_VARS CMAKE_INSTALL_LIBDIR CMAKE_INSTALL_INCLUDEDIR)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/TactixConfigVersion.cmake
  COMPATIBILITY SameMinorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/TactixConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/TactixConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Tactix)
configure_file(cmake/tactix.pc.in ${CMAKE_CURRENT_BINARY_DIR}/tactix.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tactix.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

if(TACTIX_BUILD_TESTS)
  enable_testing()
  # Bounded certificate transport requires Linux O_TMPFILE and /proc/self/fd.
  # The Python proof oracle remains available on other platforms.
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(dsr-transport tools/bench/dsr_transport.cpp)
    add_test(NAME dsr_transport COMMAND python3
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/bench/test_dsr_transport.py $<TARGET_FILE:dsr-transport>)
    set_tests_properties(dsr_transport PROPERTIES LABELS "integration")
  endif()
  add_subdirectory(tests)
  add_subdirectory(examples)

  # Exhaustive sweeps, large solve fixtures and external checker campaigns are
  # explicit local runs rather than routine CI tests.
  set(TACTIX_EXTENDED_TESTS
      symmetry_backend lucky_late_parallel cli_auto_proof feedback_frontend
      demo3_scripted_default structural_journal_trajectory congruence_ite_collapse
      inspect_policy_feedback micro_proofs independent_set_semantic_extension
      affine_matrix congruence_ite_valued_sides dsr_transport sweep_proofs
      vivify_compact_contract api_one_search affine_matrix_search_proof subsolver
      congruence_ite_unit_proof ramsey_cli default_public_replay online_inprocess
      connected_occurrence pbp_deletion_identity_cli symmetry_clause_multiset
      event_trace_zero_overhead analysis_native_target automatic_proof
      feedback_provenance)
  foreach(test_directory IN ITEMS "${CMAKE_CURRENT_SOURCE_DIR}"
                                  "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                                  "${CMAKE_CURRENT_SOURCE_DIR}/examples")
    get_property(directory_tests DIRECTORY "${test_directory}" PROPERTY TESTS)
    foreach(test_name IN LISTS directory_tests)
      if(test_name IN_LIST TACTIX_EXTENDED_TESTS)
        set_property(TEST "${test_name}" DIRECTORY "${test_directory}"
                     APPEND PROPERTY LABELS extended)
      endif()
    endforeach()
  endforeach()
endif()
