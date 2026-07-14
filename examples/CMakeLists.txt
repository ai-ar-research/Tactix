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
