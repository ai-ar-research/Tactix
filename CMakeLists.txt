cmake_minimum_required(VERSION 3.25)
project(tactix VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Kernel discipline (plan §5): C-style core, no exceptions/RTTI anywhere in
# the solver library or binary.
add_compile_options(-Wall -Wextra -fno-exceptions -fno-rtti)

# OBJECT (not STATIC): rule registration happens in static initializers
# (TACTIX_REGISTER_RULE), and an archive lets the linker drop any rule TU
# nothing references directly — exactly the situation the registry-driven
# scheduler creates (rules reached ONLY through the registry). An object
# library keeps every registered rule in every binary (§5.3: one directory
# + one registration line must be sufficient).
add_library(tactix_lib OBJECT
  src/kernel/kernel.cpp
  src/proof/proof.cpp
  src/rules/registry.cpp
  src/rules/propagate/propagate.cpp
  src/rules/decide/decide.cpp
  src/rules/analyze/analyze.cpp
  src/rules/backjump/backjump.cpp
  src/rules/restart/restart.cpp
  src/rules/lucky/lucky.cpp
  src/rules/reduce/reduce.cpp
  src/policies/ops/rephase/rephase.cpp
  src/rules/subsume_forward/subsume_forward.cpp
  src/rules/strengthen/strengthen.cpp
  src/rules/vivify/vivify.cpp
  src/rules/probe/probe.cpp
  src/rules/backbone/backbone.cpp
  src/rules/substitute/substitute.cpp
  src/rules/congruence/congruence.cpp
  src/rules/factor/factor.cpp
  src/rules/transitive_reduce/transitive_reduce.cpp
  src/rules/bve/bve.cpp
  src/policies/ops/switchmode/switchmode.cpp
  src/policies/heuristics.cpp
  src/policies/default_policy.cpp
  src/policies/random_policy.cpp
  src/policies/adversarial_policy.cpp
  src/policies/bve_stress_policy.cpp
  src/policies/congruence_stress_policy.cpp
  src/policies/factor_stress_policy.cpp
  src/policies/probing_stress_policy.cpp
  src/frontend/parse.cpp
  src/api/api.cpp
)
target_include_directories(tactix_lib PUBLIC src include)

add_executable(tactix src/main.cpp)
target_link_libraries(tactix PRIVATE tactix_lib)

enable_testing()
add_subdirectory(tests)
add_subdirectory(examples)
