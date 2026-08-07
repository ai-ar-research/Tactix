#include "policies/walk_stress_policy.hpp"
#include "policies/stress_common.hpp"

#include "policies/heuristics.hpp"
#include "policies/ops/walk/walk.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"

#include <cstdio>
#include <cstdlib>

namespace tactix {

int walk_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  uint64_t rng = seed ? seed : 1;
  const bool trace = stress_env_flag("TACTIX_TRACE");

  auto step = [&](const char *what) {
    if (trace)
      fprintf(stderr,
              "step %-14s level=%u trail=%zu head=%u unassigned=%u "
              "walks=%llu stable=%d\n",
              what, k.level, k.trail.size(), k.propagate_head, k.unassigned,
              (unsigned long long)k.stats.walks, k.h ? (int)k.h->stable : -1);
    kernel_check_invariants(k);
  };

  // Walk at roughly 1 step in 4, at whatever state the loop is in, including
  // mid-trail and off a propagation fixpoint.
  auto maybe_walk = [&]() {
    if (stress_random(rng) % 4 || !walk_guard(k))
      return;
    // budgets from 1 step (immediate cutoff, exercises the "no improvement"
    // and pending-trail-invalidation paths) up to a few million
    static const uint64_t BUDGETS[6] = {1, 7, 100, 5000, 250000, 4000000};
    walk_apply_steps(k, BUDGETS[stress_random(rng) % 6]);
    step("walk");
    // mode flips change the seed source (target when stable, saved otherwise)
    if (k.h && !(stress_random(rng) % 3))
      k.h->stable = !k.h->stable;
  };

  for (uint64_t steps = 0;; steps++) {
    if (stop && *stop && (steps & 1023) == 0)
      return 0;
    if (k.inconsistent)
      return 20;

    if (k.conflict_valid) {
      if (analyze_apply(k) == ApplyResult::Unsat)
        return 20;
      // A reused conflict leaves nothing pending, so Backjump's guard is false.
      if (backjump_guard(k))
        backjump_apply(k);
      step("analyze+backjump");
      maybe_walk();
      continue;
    }

    if (k.propagate_head < k.trail.size()) {
      maybe_walk(); // walk with an unpropagated trail on purpose
      propagate_apply(k);
      step("propagate");
      continue;
    }

    if (k.unassigned == 0)
      return 10; // total + fixpoint = SAT

    maybe_walk();
    if (k.conflict_valid || k.propagate_head < k.trail.size())
      continue; // a walk never causes either, but re-check rather than assume
    decide_apply(k, stress_random_unassigned_literal(k, rng), true);
    step("decide");
  }
}

} // namespace tactix
