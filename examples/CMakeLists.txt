# Example strategies: clients of the public C API only.

foreach(demo demo1_dpll demo2_no_restarts demo3_scripted_default)
  add_executable(${demo} ${demo}.c)
  target_link_libraries(${demo} PRIVATE tactix_lib)
  add_test(NAME ${demo} COMMAND ${demo})
endforeach()
target_link_libraries(demo3_scripted_default PRIVATE m)

add_executable(tactix-cruxmiter-policy cruxmiter_policy.c)
target_link_libraries(tactix-cruxmiter-policy PRIVATE tactix_lib tactix_cruxmiter_program)

# Paper strategy for symbreak's Chvatal SAT subproblems.
add_executable(tactix-chvatal-policy chvatal_policy.c)
target_link_libraries(tactix-chvatal-policy PRIVATE tactix_lib)

# ER definitions followed by ordinary solving, public API only.
add_executable(tactix-php-er php_er_policy.cpp php_input.cpp)
target_link_libraries(tactix-php-er PRIVATE tactix_lib)
add_test(NAME php_er_policy COMMAND ${CMAKE_COMMAND} -E env python3
  ${CMAKE_SOURCE_DIR}/tests/test_php_er_policy.py
  $<TARGET_FILE:tactix-php-er>)

# The default strategy over tactix.h alone; test_c_default_correspondence
# drives it beside DefaultPolicy. An object library: a client links it with
# tactix_lib.
add_library(tactix_default OBJECT default_strategy/default_strategy.c
            default_strategy/settings.c default_strategy/search.c
            default_strategy/inprocess.c default_strategy/services.c)
target_include_directories(tactix_default PUBLIC default_strategy)
target_link_libraries(tactix_default PUBLIC tactix_lib m)
set_target_properties(tactix_default PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
                      C_EXTENSIONS OFF)
target_compile_options(tactix_default PRIVATE -Wall -Wextra -Wpedantic)
add_executable(tactix-default default_strategy/main.c)
target_link_libraries(tactix-default PRIVATE tactix_default tactix_lib)
