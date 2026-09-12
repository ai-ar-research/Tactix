#undef NDEBUG
#include "tactix.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

int main() {
  tactix *solver = tactix_init();
  assert(tactix_config(solver, TACTIX_CFG_ANALYZE_MINIMIZE_DEPTH) == 1000);
  for (uint32_t value : {0u, 1u, 999u, 1000u, 1001u, UINT32_MAX}) {
    tactix_set_analyze_minimize_depth(solver, value);
    assert(tactix_config(solver, TACTIX_CFG_ANALYZE_MINIMIZE_DEPTH) == value);
  }
  tactix_release(solver);
  std::puts("Analyze per-call depth pose/readback boundaries PASS");
}
