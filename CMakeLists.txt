cmake_minimum_required(VERSION 3.25)
project(tactix VERSION 0.1.0 LANGUAGES CXX C)

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
add_compile_options(-Wall -Wextra -fno-exceptions -fno-rtti)

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
  src/proof/direct_php_recognition.cpp
  src/rules/registry.cpp
  ${TACTIX_RULE_SOURCES}
  ${TACTIX_POLICY_OP_SOURCES}
  src/engine/subsolver.cpp
  src/engine/subsolver_proof_export.cpp
  src/policies/heuristics.cpp
  src/policies/default_policy.cpp
  src/policies/structured_root_strategy.cpp
  src/policies/random_policy.cpp
  src/policies/random_controller_policy.cpp
  src/policies/controller_trace.cpp
  src/policies/trace.cpp
  src/policies/stress_registry.cpp
  ${TACTIX_STRATEGY_SOURCES}
  src/frontend/parse.cpp
  src/api/api.cpp
)

# Proof export can copy a huge learned core. Only this translation unit may
# catch allocation failure; `SubSolver::derivation` is noexcept to callers.
set_source_files_properties(src/engine/subsolver_proof_export.cpp PROPERTIES
  COMPILE_OPTIONS -fexceptions)
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
target_link_libraries(tactix_lib PUBLIC tactix_symmetry_backend)

add_executable(tactix src/main.cpp)
target_link_libraries(tactix PRIVATE tactix_lib)

enable_testing()
add_subdirectory(tests)
add_subdirectory(examples)
