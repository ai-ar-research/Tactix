#include "policies/adversarial_policy.hpp"
#include "policies/stress_common.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"

#include <cstdio>
#include <cstdlib>

namespace tactix {

int adversarial_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  uint64_t rng = seed ? seed : 1;
  const uint32_t burst = stress_env_u32("TACTIX_ADV_BURST", 4);
  const uint32_t prop_pct = stress_env_u32("TACTIX_ADV_PROP", 15);
  const bool trace = getenv("TACTIX_TRACE") != nullptr;

  uint32_t since_prop = 0; // decides taken since the last propagate

  auto step = [&](const char *what) {
    if (trace)
      fprintf(stderr,
              "step %-18s level=%u trail=%zu head=%u unassigned=%u "
              "conflict=%d incons=%d\n",
              what, k.level, k.trail.size(), k.propagate_head, k.unassigned,
              (int)k.conflict_valid, (int)k.inconsistent);
    kernel_check_invariants(k); // PER-STEP (not every-256): pin the culprit
  };

  for (uint64_t steps = 0;; steps++) {
    if (stop && *stop && (steps & 1023) == 0)
      return 0;
    if (k.inconsistent)
      return 20;

    if (k.conflict_valid) {
      // must resolve the recorded conflict before anything else
      const ApplyResult r = analyze_apply(k);
      if (r == ApplyResult::Unsat)
        return 20;
      // TB4b(A): see bve_stress_policy — a reused conflict leaves nothing
      // pending, so Backjump's guard is false.
      if (backjump_guard(k))
        backjump_apply(k); // asserts the UIP at jump_level; clears pending
      since_prop = 0;
      step("analyze+backjump");
      continue;
    }

    if (k.unassigned == 0) {
      // total assignment: only propagation to a fixpoint can confirm SAT
      if (k.propagate_head < k.trail.size()) {
        propagate_apply(k);
        since_prop = 0;
        step("propagate(full)");
        continue;
      }
      return 10; // total + fixpoint + no conflict = SAT
    }

    // unassigned > 0, no conflict: the adversarial choice point.
    const bool work_pending = k.propagate_head < k.trail.size();
    const bool force_prop = since_prop >= burst && work_pending;
    const bool roll_prop = work_pending && (stress_random(rng) % 100) < prop_pct;
    if (force_prop || roll_prop) {
      propagate_apply(k);
      since_prop = 0;
      step("propagate");
    } else {
      decide_apply(k, stress_random_unassigned_literal(k, rng), true);
      since_prop++;
      step("decide");
    }
  }
}

} // namespace tactix
