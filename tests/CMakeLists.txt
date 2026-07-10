# Unit tests (plain asserts, no framework — kernel discipline, no deps).
# The C-language API test doubles as the header/ABI sync check for api.cpp.

add_executable(test_kernel test_kernel.cpp)
target_link_libraries(test_kernel PRIVATE tactix_lib)
add_test(NAME kernel COMMAND test_kernel)

add_executable(test_analyze test_analyze.cpp)
target_link_libraries(test_analyze PRIVATE tactix_lib)
add_test(NAME analyze COMMAND test_analyze)

add_executable(test_restart_reduce test_restart_reduce.cpp)
target_link_libraries(test_restart_reduce PRIVATE tactix_lib)
add_test(NAME restart_reduce COMMAND test_restart_reduce)

add_executable(test_api_c test_api_c.c)
target_link_libraries(test_api_c PRIVATE tactix_lib)
target_include_directories(test_api_c PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME api_n1 COMMAND test_api_c)

# micro-formula proof test: tiny UNSATs through the real binary, proofs
# verified by drat-trim (skips with 77 when the checker isn't built)
add_test(NAME micro_proofs
         COMMAND ${CMAKE_SOURCE_DIR}/tests/micro_proofs.sh
                 $<TARGET_FILE:tactix> ${CMAKE_SOURCE_DIR})
set_tests_properties(micro_proofs PROPERTIES SKIP_RETURN_CODE 77)
