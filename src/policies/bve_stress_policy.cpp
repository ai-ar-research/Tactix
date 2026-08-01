#include "policies/bve_stress_policy.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"

#include <cstdio>
#include <cstdlib>

namespace tactix {

static inline uint64_t bs_random(uint64_t &s) {
  s ^= s >> 12;
  s ^= s << 25;
  s ^= s >> 27;
  return s * 0x2545F4914F6CDD1Dull;
}

static Lit bs_random_unassigned_literal(Kernel &k, uint64_t &rng) {
  assert(k.unassigned > 0);
  for (;;) {
    const Var v = static_cast<Var>(bs_random(rng) % k.nvars);
    if (k.values[make_lit(v, false)] == 0)
      return make_lit(v, bs_random(rng) & 1);
  }
}

int bve_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  uint64_t rng = seed ? seed : 1;
  const bool trace = getenv("TACTIX_TRACE") != nullptr;
  const int bve_id = tactix_rule_id("BVE");
  const RuleVTable &bve = tactix_rules()[static_cast<size_t>(bve_id)];

  auto step = [&](const char *what) {
    if (trace)
      fprintf(stderr,
              "step %-16s level=%u trail=%zu head=%u unassigned=%u elim=%llu "
              "conflict=%d incons=%d\n",
              what, k.level, k.trail.size(), k.propagate_head, k.unassigned,
              (unsigned long long)k.stats.bve_eliminated, (int)k.conflict_valid,
              (int)k.inconsistent);
    kernel_check_invariants(k); // PER-STEP (not every-256): pin the culprit
  };

  for (uint64_t steps = 0;; steps++) {
    if (stop && *stop && (steps & 1023) == 0)
      return 0;
    if (k.inconsistent)
      return 20;

    if (k.conflict_valid) {
      if (analyze_apply(k) == ApplyResult::Unsat)
        return 20;
      // TB4b(A): the conflict-reuse shortcut resolves the conflict INSIDE
      // Analyze and leaves nothing pending, so Backjump's guard is false —
      // calling it anyway would read a previous conflict's pending record.
      if (backjump_guard(k))
        backjump_apply(k);
      step("analyze+backjump");
      continue;
    }

    if (k.propagate_head < k.trail.size()) {
      propagate_apply(k);
      step("propagate");
      continue;
    }

    // fixpoint, no conflict.
    if (k.unassigned == 0)
      return 10; // total + fixpoint = SAT

    // Maximize BVE at the root: eliminate every schedulable variable before
    // deciding. bve.apply drains root closure at entry and eliminates the best
    // schedulable candidate (or NoOp when none stays within budget).
    if (k.level == 0) {
      const ApplyResult r = apply_rule(bve, k);
      if (r == ApplyResult::Unsat)
        return 20;
      if (r == ApplyResult::Ok) {
        step("bve");
        continue;
      }
      // NoOp: nothing more to eliminate at the root this round.
    }

    decide_apply(k, bs_random_unassigned_literal(k, rng), true);
    step("decide");
  }
}

} // namespace tactix
