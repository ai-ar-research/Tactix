#pragma once
#include "tactix.h"
#include <cstdio>

// Read-only, after solving: public clients lack the CLI's full statistics dump.
// Require real BCE/Substitute output rather than merely entering a round;
// BVE retains the existing resolution-work witness, not just pure elimination.
inline void tactix_public_reach(tactix_solver *solver) {
  static constexpr struct { const char *name; int stat; } counters[] = {
      {"Propagate", TACTIX_STAT_PROPAGATIONS}, {"Decide", TACTIX_STAT_DECISIONS},
      {"ConflictAnalysis", TACTIX_STAT_LEARNED}, {"BCE", TACTIX_STAT_BCE_DELETED},
      {"BVE", TACTIX_STAT_BVE_RESOLUTIONS}, {"Substitute", TACTIX_STAT_SUBSTITUTE_VARS}};
  for (const auto &counter : counters)
    std::printf("c public-reach %s %llu\n", counter.name,
                static_cast<unsigned long long>(tactix_stat(solver, counter.stat)));
}
