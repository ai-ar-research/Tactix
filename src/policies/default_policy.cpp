#include "policies/default_policy.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include "rules/restart/restart.hpp"

namespace tactix {

// Glucose-style EMA restart trigger (Audemard–Simon): restart when the
// short-term glue average exceeds the long-term one by a margin — search
// quality is degrading, so abandon the prefix.
static constexpr double RESTART_MARGIN = 1.25;
static constexpr uint64_t RESTART_FLOOR_INTERVAL = 50;

void DefaultPolicy::observe_glue(uint32_t glue) {
  if (restart.ema_initialized) {
    restart.fast_glue += (glue - restart.fast_glue) / 32.0;
    restart.slow_glue += (glue - restart.slow_glue) / 16384.0;
  } else {
    restart.fast_glue = restart.slow_glue = glue;
    restart.ema_initialized = true;
  }
}

bool DefaultPolicy::want_restart(const Kernel &k) const {
  return restart.ema_initialized &&
         k.stats.conflicts >= restart.floor_conflicts &&
         restart.fast_glue > RESTART_MARGIN * restart.slow_glue;
}

bool DefaultPolicy::want_reduce(const Kernel &k) const {
  return k.stats.conflicts >= reduce.next_conflicts;
}

Lit DefaultBranching::pick(Kernel &k) {
  assert(k.unassigned > 0);
  const Var v = kernel_queue_next_unassigned(k);
  return make_lit(v, k.saved[v] < 0);
}

// Phase-1 shape of kissat/src/search.c's guard->apply chain: propagate,
// analyze+backjump on conflict, SAT when everything is assigned, else
// decide — with Restart/Reduce scheduled between conflicts by THIS
// policy's state (rev 17: rules carry no scheduling). S3–S5 join in
// phase 2.
int DefaultPolicy::solve(Kernel &k, volatile sig_atomic_t *stop) {
  if (k.inconsistent)
    return 20;
  for (;;) {
    const ApplyResult r = propagate_apply(k);
    if (r == ApplyResult::Unsat)
      return 20;
    if (r == ApplyResult::Conflict) {
      if (analyze_apply(k) == ApplyResult::Unsat)
        return 20;
      observe_glue(k.pending.glue); // policy observes; Analyze stays pure
      backjump_apply(k);
      continue;
    }
    if (k.unassigned == 0)
      return 10;
    if (want_reduce(k) && reduce_guard(k)) {
      // sub-policy chooses HOW MUCH of the rule's ranked candidates die
      ReductionPolicy &rp = reduction ? *reduction : default_reduction_;
      reduce_candidates(k, reduce_scratch_);
      reduce_apply_refs(
          k, reduce_scratch_.data(),
          std::min(rp.victims(k, reduce_scratch_.size()),
                   reduce_scratch_.size()));
      reduce.next_conflicts = k.stats.conflicts + 2000 + 300 * k.stats.reductions;
      continue;
    }
    if (want_restart(k) && restart_guard(k)) {
      restart_apply(k);
      restart.floor_conflicts = k.stats.conflicts + RESTART_FLOOR_INTERVAL;
      continue;
    }
    if (stop && *stop)
      return 0;
    // sub-policy chooses the branch (any unassigned literal is legal, N1)
    BranchingPolicy &bp = branching ? *branching : default_branching_;
    decide_apply(k, bp.pick(k), false);
  }
}

int default_solve(Kernel &k, volatile sig_atomic_t *stop) {
  DefaultPolicy policy;
  return policy.solve(k, stop);
}

} // namespace tactix
