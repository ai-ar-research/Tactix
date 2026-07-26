#include "policies/walk_stress_policy.hpp"

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

namespace {

inline uint64_t ws_random(uint64_t &s) {
  s ^= s >> 12;
  s ^= s << 25;
  s ^= s >> 27;
  return s * 0x2545F4914F6CDD1Dull;
}

Lit ws_random_unassigned_literal(Kernel &k, uint64_t &rng) {
  assert(k.unassigned > 0);
  for (;;) {
    const Var v = static_cast<Var>(ws_random(rng) % k.nvars);
    if (k.values[make_lit(v, false)] == 0)
      return make_lit(v, ws_random(rng) & 1);
  }
}

} // namespace

int walk_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop) {
  uint64_t rng = seed ? seed : 1;
  const bool trace = getenv("TACTIX_TRACE") != nullptr;

  auto step = [&](const char *what) {
    if (trace)
      fprintf(stderr,
              "step %-14s level=%u trail=%zu head=%u unassigned=%u "
              "walks=%llu stable=%d\n",
              what, k.level, k.trail.size(), k.propagate_head, k.unassigned,
              (unsigned long long)k.stats.walks, k.h ? (int)k.h->stable : -1);
    kernel_check_invariants(k);
  };

  // Walk at a random ~1-in-4 of the steps, at whatever state the loop is in —
  // including mid-trail and (deliberately) off a propagation fixpoint, which
  // DefaultPolicy never does.
  auto maybe_walk = [&]() {
    if (ws_random(rng) % 4 || !walk_guard(k))
      return;
    // budgets from 1 step (immediate cutoff, exercises the "no improvement"
    // and pending-trail-invalidation paths) up to a few million
    static const uint64_t BUDGETS[6] = {1, 7, 100, 5000, 250000, 4000000};
    walk_apply_steps(k, BUDGETS[ws_random(rng) % 6]);
    step("walk");
    // mode flips change the seed source (target when stable, saved otherwise)
    if (k.h && !(ws_random(rng) % 3))
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
      backjump_apply(k);
      step("analyze+backjump");
      maybe_walk();
      continue;
    }

    if (k.propagate_head < k.trail.size()) {
      maybe_walk(); // walk with an UNPROPAGATED trail on purpose
      propagate_apply(k);
      step("propagate");
      continue;
    }

    if (k.unassigned == 0)
      return 10; // total + fixpoint = SAT

    maybe_walk();
    if (k.conflict_valid || k.propagate_head < k.trail.size())
      continue; // a walk never causes either, but re-check rather than assume
    decide_apply(k, ws_random_unassigned_literal(k, rng), true);
    step("decide");
  }
}

} // namespace tactix
