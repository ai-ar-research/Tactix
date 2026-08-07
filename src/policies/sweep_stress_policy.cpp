#include "policies/sweep_stress_policy.hpp"
#include "policies/stress_common.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"

#include <cstdio>
#include <cstdlib>

namespace tactix {

int sweep_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  uint64_t rng = seed ? seed : 1;
  const bool trace = getenv("TACTIX_TRACE") != nullptr;
  const bool no_subst = getenv("TACTIX_SWEEP_STRESS_NOSUBST") != nullptr;
  const uint32_t max_rounds = stress_env_u32("TACTIX_SWEEP_STRESS_ROUNDS", 4);

  const RuleVTable &sweep =
      tactix_rules()[static_cast<size_t>(tactix_rule_id("Sweep"))];
  const RuleVTable &subst =
      tactix_rules()[static_cast<size_t>(tactix_rule_id("Substitute"))];

  auto step = [&](const char *what) {
    if (trace)
      fprintf(stderr,
              "step %-16s level=%u trail=%zu head=%u unassigned=%u "
              "sweep_rnd=%llu equiv=%llu bins=%llu notrup=%llu gen=%llu "
              "conflict=%d incons=%d\n",
              what, k.level, k.trail.size(), k.propagate_head, k.unassigned,
              (unsigned long long)k.stats.sweep_rounds,
              (unsigned long long)k.stats.sweep_equivalences,
              (unsigned long long)k.stats.sweep_binaries,
              (unsigned long long)k.stats.sweep_not_rup,
              (unsigned long long)k.stats.sweep_general,
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
      if (backjump_guard(k)) // a reused conflict leaves nothing pending
        backjump_apply(k);
      step("analyze+backjump");
      continue;
    }

    // --- the stressed window: sweep every root, to a fixpoint ---------------
    // Sits before the propagate branch on purpose, so Sweep is entered off a
    // propagation fixpoint (legal: `sweep_guard` is `!inconsistent && level ==
    // 0` and the apply drains via `inprocess_reach_root`) and again on the
    // formula its own binaries just changed. `k.work_budget` stays 0 so each
    // apply runs the whole round rather than S-f's dose — more environments per
    // root is more candidate pairs, hence more chances one is not RUP.
    if (k.level == 0) {
      bool progressed = false;
      for (uint32_t round = 0; round < max_rounds; round++) {
        if (!sweep.guard(k))
          break;
        const ApplyResult r = apply_rule(sweep, k);
        if (r == ApplyResult::Unsat)
          return 20;
        if (r != ApplyResult::Ok)
          break; // NoOp: nothing more to derive at this root
        step("sweep");
        progressed = true;
        // Consume the equivalences Sweep just stated (V3 owns the substitution
        // and the binary deletion). The deletions change which clauses the next
        // environment may cite, which is the `env subset of F` invariant S-e's
        // chains rest on.
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
