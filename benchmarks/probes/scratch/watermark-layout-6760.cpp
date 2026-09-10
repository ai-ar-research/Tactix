// Fixed representation probe, not a solver invocation or runtime benchmark.
#include "kernel/kernel.hpp"
#include <cstddef>
#include <cstdio>

int main() {
  using tactix::Kernel;
  std::printf("sizeof_Kernel=%zu\nalignof_Kernel=%zu\n", sizeof(Kernel), alignof(Kernel));
#define FIELD(name) std::printf(#name "=%zu\n", offsetof(Kernel, name))
  FIELD(values);
  FIELD(assigned);
  FIELD(trail);
  FIELD(arena);
  FIELD(clause_position_alias);
#ifdef WATERMARK_CURRENT
  FIELD(first_reducible);
#endif
  FIELD(watches);
  FIELD(large_watches_connected);
  FIELD(arena_ordering);
  FIELD(dense);
  FIELD(delayed);
  FIELD(h);
  FIELD(stats);
  FIELD(tumble);
#undef FIELD
}
