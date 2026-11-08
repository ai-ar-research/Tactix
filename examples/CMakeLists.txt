# §8.4 expressiveness demos — external clients of the frozen v1 C API
# (no kernel access, no internal headers). Reviewed at the phase-2 API
# freeze (demos 1–3); demos 4–6 land by phase 4.

foreach(demo demo1_dpll demo2_no_restarts demo3_scripted_default)
  add_executable(${demo} ${demo}.c)
  target_link_libraries(${demo} PRIVATE tactix_lib)
  add_test(NAME ${demo} COMMAND ${demo})
endforeach()
target_link_libraries(demo3_scripted_default PRIVATE m)

# A competition-shaped external policy client. It intentionally includes only
# the public C API: this is an embedding example, not a privileged core policy.
add_executable(tactix-cruxmiter-policy cruxmiter_policy.c)
target_link_libraries(tactix-cruxmiter-policy PRIVATE tactix_lib)

# Dedicated benchmark-family policies implemented against the same frozen
# public interface.  EEVBNN accepts both plain and gzip-compressed DIMACS.
add_executable(tactix-eevbnn-policy eevbnn_policy.c)
target_link_libraries(tactix-eevbnn-policy PRIVATE tactix_lib ZLIB::ZLIB)

add_executable(tactix-random3sat-policy random3sat_policy.c)
target_link_libraries(tactix-random3sat-policy PRIVATE tactix_lib)

# Paper strategy for symbreak's Chvatal SAT subproblems.
add_executable(tactix-chvatal-policy chvatal_policy.c)
target_link_libraries(tactix-chvatal-policy PRIVATE tactix_lib)

# Polynomial PHP reduction expressed entirely through public strategy calls.
add_executable(tactix-php-er php_er_policy.c)
target_link_libraries(tactix-php-er PRIVATE tactix_lib)
add_test(NAME php_er_policy COMMAND ${CMAKE_COMMAND} -E env python3
  ${CMAKE_SOURCE_DIR}/tests/test_php_er_policy.py
  $<TARGET_FILE:tactix-php-er> ${CMAKE_SOURCE_DIR})
