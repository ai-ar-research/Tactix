cmake_minimum_required(VERSION 3.25)
project(tactix VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Kernel discipline (plan §5): C-style core, no exceptions/RTTI anywhere in
# the solver library or binary.
add_compile_options(-Wall -Wextra -fno-exceptions -fno-rtti)

add_library(tactix_lib STATIC
  src/kernel/kernel.cpp
  src/proof/proof.cpp
  src/rules/registry.cpp
  src/rules/propagate/propagate.cpp
  src/rules/decide/decide.cpp
  src/rules/analyze/analyze.cpp
  src/rules/backjump/backjump.cpp
  src/rules/restart/restart.cpp
  src/rules/reduce/reduce.cpp
  src/policies/default_policy.cpp
  src/frontend/parse.cpp
  src/api/api.cpp
)
target_include_directories(tactix_lib PUBLIC src include)

add_executable(tactix src/main.cpp)
target_link_libraries(tactix PRIVATE tactix_lib)

enable_testing()
add_subdirectory(tests)
