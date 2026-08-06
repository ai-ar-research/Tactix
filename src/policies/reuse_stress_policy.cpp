#include "policies/reuse_stress_policy.hpp"
#include "policies/stress_common.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/restart/restart.hpp"

#include <cstdio>
#include <cstdlib>

namespace tactix {

int reuse_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop) {
  KernelStopPublisher stop_published(k, stop);
  uint64_t rng = seed ? seed : 1;
  const bool trace = getenv("TACTIX_TRACE") != nullptr;
  k.conflict_reuse = true; // the mechanism under stress, forced on

  // Propagate to a FIXPOINT and then check invariants. The fixpoint matters:
  // kernel_check_invariants' escaped-unit / falsified-clause checks are gated on
  // `propagate_head == |trail| && !conflict_valid`, and those are precisely the
  // checks a missing watch repair trips. Returns true if a conflict is pending.
  auto settle = [&](const char *what) {
    bool conflict = false;
    while (k.propagate_head < k.trail.size() && !k.inconsistent) {
      if (propagate_apply(k) == ApplyResult::Conflict) {
        conflict = true;
        break;
      }
    }
    if (trace)
      fprintf(stderr,
              "step %-16s level=%u trail=%zu head=%u unassigned=%u "
              "conflict=%d reused=%llu repaired=%llu\n",
              what, k.level, k.trail.size(), k.propagate_head, k.unassigned,
              (int)conflict, (unsigned long long)k.stats.conflict_reused,
              (unsigned long long)k.stats.conflict_watch_repaired);
    kernel_check_invariants(k);
    return conflict;
  };

  for (uint64_t steps = 0;; steps++) {
    if (stop && *stop && (steps & 1023) == 0)
      return 0;
    if (k.inconsistent)
      return 20;

    if (k.conflict_valid) {
      if (analyze_apply(k) == ApplyResult::Unsat)
        return 20;
      if (backjump_guard(k)) {
        // ALWAYS CHRONOLOGICAL, and deliberately at the very top of the legal
        // range: `backjump_apply_to` clamps into [jump_level, level-1], so
        // level-1 is the maximal chronological target. This is what makes the
        // trail non-level-sorted, which is the only way a later conflict can
        // have exactly one literal at the conflict level.
        backjump_apply_to(k, k.level ? k.level - 1 : 0);
      }
      settle("analyze+backjump");
      continue;
    }

    if (k.propagate_head < k.trail.size()) {
      settle("propagate");
      continue;
    }

    if (k.unassigned == 0)
      return 10; // total + fixpoint + no conflict = SAT

    // ~1 step in 8, drop to a RANDOM level and re-settle. This is the half of
    // the hazard the shortcut itself cannot show: a reused conflict only strands
    // a unit once a LATER backtrack unassigns the literal it forced. Dropping
    // below the reuse's own level and then propagating to a fixpoint is exactly
    // that sequence, and the invariant check at the fixpoint is the oracle.
    if (!(stress_random(rng) % 8) && restart_guard(k)) {
      restart_apply_to(k, static_cast<uint32_t>(stress_random(rng) % k.level));
      if (settle("restart"))
        continue;
    }

    if (k.conflict_valid || k.propagate_head < k.trail.size())
      continue;
    decide_apply(k, stress_random_unassigned_literal(k, rng), true);
    settle("decide");
  }
}

} // namespace tactix
