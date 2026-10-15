# §8.4 expressiveness demos — external clients of the frozen v1 C API
# (no kernel access, no internal headers). Reviewed at the phase-2 API
# freeze (demos 1–3); demos 4–6 land by phase 4.

foreach(demo demo1_dpll demo2_no_restarts demo3_scripted_default)
  add_executable(${demo} ${demo}.c)
  target_link_libraries(${demo} PRIVATE tactix_lib)
  target_include_directories(${demo} PRIVATE ${CMAKE_SOURCE_DIR}/include)
  add_test(NAME ${demo} COMMAND ${demo})
endforeach()
target_link_libraries(demo3_scripted_default PRIVATE m)

# A competition-shaped external policy client. It intentionally includes only
# the public C API: this is an embedding example, not a privileged core policy.
add_executable(tactix-cruxmiter-policy cruxmiter_policy.c)
target_link_libraries(tactix-cruxmiter-policy PRIVATE tactix_lib)
target_include_directories(tactix-cruxmiter-policy PRIVATE ${CMAKE_SOURCE_DIR}/include)

# The crux opening followed by an unmodified DefaultPolicy handoff.
add_executable(tactix-crux-fallback-policy crux_fallback_policy.c)
target_link_libraries(tactix-crux-fallback-policy PRIVATE tactix_lib)
target_include_directories(tactix-crux-fallback-policy PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Dedicated benchmark-family policies implemented against the same frozen
# public interface.  EEVBNN accepts both plain and gzip-compressed DIMACS.
add_executable(tactix-eevbnn-policy eevbnn_policy.c)
target_link_libraries(tactix-eevbnn-policy PRIVATE tactix_lib ZLIB::ZLIB)
target_include_directories(tactix-eevbnn-policy PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(tactix-random3sat-policy random3sat_policy.c)
target_link_libraries(tactix-random3sat-policy PRIVATE tactix_lib)
target_include_directories(tactix-random3sat-policy PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Semantic-variable CDCL policy for symbreak's Chvatal SAT subproblems.
add_executable(tactix-chvatal-policy chvatal_policy.c)
target_link_libraries(tactix-chvatal-policy PRIVATE tactix_lib)
target_include_directories(tactix-chvatal-policy PRIVATE ${CMAKE_SOURCE_DIR}/include)
