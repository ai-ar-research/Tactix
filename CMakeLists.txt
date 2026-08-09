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

# Kernel discipline (plan §5): C-style core, no exceptions/RTTI anywhere in
# the solver library or binary.
add_compile_options(-Wall -Wextra -fno-exceptions -fno-rtti)

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

add_library(tactix_lib OBJECT
  src/kernel/kernel.cpp
  src/proof/proof.cpp
  src/rules/registry.cpp
  ${TACTIX_RULE_SOURCES}
  ${TACTIX_POLICY_OP_SOURCES}
  src/engine/subsolver.cpp
  src/policies/heuristics.cpp
  src/policies/default_policy.cpp
  src/policies/random_policy.cpp
  src/policies/trace.cpp
  src/policies/stress_registry.cpp
  src/frontend/parse.cpp
  src/api/api.cpp
)
if(TACTIX_ENABLE_EVENT_TRACE)
  target_sources(tactix_lib PRIVATE src/policies/event_trace.cpp)
  target_compile_definitions(tactix_lib PUBLIC TACTIX_ENABLE_EVENT_TRACE=1)
endif()
target_include_directories(tactix_lib PUBLIC src include)

add_executable(tactix src/main.cpp)
target_link_libraries(tactix PRIVATE tactix_lib)

enable_testing()
add_subdirectory(tests)
add_subdirectory(examples)
