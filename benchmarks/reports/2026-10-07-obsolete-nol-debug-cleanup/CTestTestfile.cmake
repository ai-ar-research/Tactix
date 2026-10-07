# CMake generated Testfile for 
# Source directory: /home/hwu/SAT-solving/benchmarks/runs/20261007-nol-retired-1e8295106-full-debug/source
# Build directory: /home/hwu/SAT-solving/build/pin-nol-retired-1e8295106-debug
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test("dsr_transport" "python3" "/home/hwu/SAT-solving/benchmarks/runs/20261007-nol-retired-1e8295106-full-debug/source/tools/bench/test_dsr_transport.py" "/home/hwu/SAT-solving/build/pin-nol-retired-1e8295106-debug/dsr-transport")
set_tests_properties("dsr_transport" PROPERTIES  _BACKTRACE_TRIPLES "/home/hwu/SAT-solving/benchmarks/runs/20261007-nol-retired-1e8295106-full-debug/source/CMakeLists.txt;99;add_test;/home/hwu/SAT-solving/benchmarks/runs/20261007-nol-retired-1e8295106-full-debug/source/CMakeLists.txt;0;")
subdirs("tests")
subdirs("examples")
