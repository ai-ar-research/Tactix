#pragma once
// The C++ unit tests' first include. It keeps assert() on in every build type,
// brings in the standard headers most tests use and opens namespace tactix.
//
// TEST(name) { ... } defines a test case and registers it; run_tests() runs a
// file's cases in definition order. unit_test_main.cpp supplies the main()
// that a test file without its own main() gets; a test whose main() parses
// arguments or prepares the process calls run_tests() itself.
#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace tactix {}
using namespace tactix;

namespace unit_test {

using Case = void (*)();

inline std::vector<Case> &registered_cases() {
  static std::vector<Case> cases;
  return cases;
}

struct Registration {
  explicit Registration(Case test) { registered_cases().push_back(test); }
};

} // namespace unit_test

// Runs this executable's registered cases in definition order; returns how
// many ran.
inline size_t run_tests() {
  for (const unit_test::Case test : unit_test::registered_cases())
    test();
  return unit_test::registered_cases().size();
}

#define TEST(name)                                                             \
  static void name();                                                          \
  static const unit_test::Registration name##_registration(name);              \
  static void name()
