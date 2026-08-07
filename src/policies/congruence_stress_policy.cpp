#include "policies/congruence_stress_policy.hpp"
#include "policies/stress_common.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"

#include <cstdio>
#include <cstdlib>

namespace tactix {

int congruence_stress_solve(Kernel &k, uint64_t seed,
                            volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  uint64_t rng = seed ? seed : 1;
  const bool trace = stress_env_flag("TACTIX_TRACE");
  const bool no_subst = stress_env_flag("TACTIX_CONG_STRESS_NOSUBST");
  const uint32_t max_rounds = stress_env_u32("TACTIX_CONG_STRESS_ROUNDS", 8);
  // Drives the mid-round budget, unreachable under DefaultPolicy because a
  // generated case is solved before the inprocessing slot runs. Each apply gets
  // a fresh pseudo-random budget in [1, N], so the round is cut at a different
  // point every time and every partial-round certificate gets checked. Pair
  // with TACTIX_STOP_FUZZ=<huge>: the rule's stop checks are strided 1-in-256
  // unless `stop_fuzz` is set, and a tiny case never reaches 256 items.
  const uint32_t stress_budget = stress_env_u32("TACTIX_CONG_STRESS_BUDGET", 0);

  const RuleVTable &cong =
      tactix_rules()[static_cast<size_t>(tactix_rule_id("Congruence"))];
  const RuleVTable &subst =
      tactix_rules()[static_cast<size_t>(tactix_rule_id("Substitute"))];

  auto step = [&](const char *what) {
    if (trace)
      fprintf(stderr,
              "step %-16s level=%u trail=%zu head=%u unassigned=%u "
              "cong_rounds=%llu cong_units=%llu conflict=%d incons=%d\n",
              what, k.level, k.trail.size(), k.propagate_head, k.unassigned,
              (unsigned long long)k.stats.congruence_rounds,
              (unsigned long long)k.stats.congruence_units,
              (int)k.conflict_valid, (int)k.inconsistent);
    kernel_check_invariants(k); // per step, not every-256: pin the culprit
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
      continue;
    }

    // --- the stressed window: everything legal at the root ------------------
    // Sits before the propagate branch on purpose, so congruence is entered off
    // a propagation fixpoint (legal since the D-LEVEL purification — it drains
    // via inprocess_reach_root) and again immediately on the formula its own
    // units and equivalences just changed.
    if (k.level == 0) {
      bool progressed = false;
      for (uint32_t round = 0; round < max_rounds; round++) {
        if (!cong.guard(k))
          break;
        if (stress_budget) {
          // Log-uniform in [1, N], not uniform: the N2-critical stop is the one
          // after some merges have been emitted, and a flat draw over a range
          // wide enough to reach the closure spends nearly every case still in
          // extraction (measured: 0 gates).
          uint32_t hi = 1;
          while (hi < stress_budget && (stress_random(rng) & 1))
            hi <<= 1;
          k.work_budget = 1 + stress_random(rng) % (hi > stress_budget ? stress_budget : hi);
        }
        const ApplyResult r = apply_rule(cong, k);
        k.work_budget = 0; // scoped to the apply, exactly as the policy does
        if (r == ApplyResult::Unsat)
          return 20; // V6 refuted, reachable only via the unit channel
        if (r != ApplyResult::Ok)
          break; // NoOp: fixpoint for this root visit
        step("congruence");
        progressed = true;
        // Collapse the equivalences congruence just added, so the next round
        // sees new degeneracies rather than the same ones.
        if (!no_subst && subst.guard(k)) {
          const ApplyResult s = apply_rule(subst, k);
          if (s == ApplyResult::Unsat)
            return 20;
          if (s == ApplyResult::Ok)
            step("substitute");
        }
      }
      if (progressed)
        continue;
    }

    if (k.propagate_head < k.trail.size()) {
      propagate_apply(k);
      step("propagate");
      continue;
    }

    if (k.unassigned == 0)
      return 10; // total + fixpoint = SAT

    decide_apply(k, stress_random_unassigned_literal(k, rng), true);
    step("decide");
  }
}

} // namespace tactix
