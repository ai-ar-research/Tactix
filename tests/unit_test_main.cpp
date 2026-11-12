// The main() of a unit test that defines none: it runs the registered TEST
// cases. Linked from the tactix_test_main archive, so a test file with its own
// main() keeps that one.
#include "unit_test.hpp"

int main() {
  const size_t cases = run_tests();
  if (cases == 0) {
    std::fputs("no TEST cases registered\n", stderr);
    return 1;
  }
  std::printf("%zu tests passed\n", cases);
}
