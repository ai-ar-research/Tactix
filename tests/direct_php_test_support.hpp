#pragma once
// Pigeonhole fixtures and ample limits shared by the direct-PHP tests.
#include "pigeonhole_kernel.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

// PHP(holes + 1, holes): variable p*holes + h + 1 places pigeon p in hole h;
// one at-least-one clause per pigeon, then the at-most-one binaries per hole.
inline std::vector<std::vector<int32_t>> php_clauses(uint32_t holes) {
  return pigeonhole_clauses(holes + 1, holes);
}

// The PHP clauses as native clauses with IDs 1..n.
inline tactix::Kernel php_kernel(uint32_t holes = 3) {
  assert(holes >= 2);
  tactix::Kernel kernel;
  add_pigeonhole(kernel, holes + 1, holes, PhpOrder::HoleMajor, true);
  return kernel;
}

// Ample snapshot, recognition and certificate bounds.
template <class Limits> void ample_php_limits(Limits &limits) {
  limits.snapshot = {100000, 1000000, 10000000};
  limits.recognition = {100000, 1000000, 100000000, 10000, 1000000, 1000000, 20};
  limits.certificate = {1000000, 1000000, 100000000, 1000000, 2000000};
}
