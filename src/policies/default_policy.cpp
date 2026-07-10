#include "policies/default_policy.hpp"

#include "rules/analyze/analyze.hpp"
#include "rules/backjump/backjump.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/reduce/reduce.hpp"
#include "rules/restart/restart.hpp"

namespace tactix {

// Phase-1 shape of kissat/src/search.c's guard->apply chain: propagate,
// analyze+backjump on conflict, SAT when everything is assigned, else
// decide. Restart/Reduce/... join this chain in phase 2 via the registry.
int default_solve(Kernel &k, volatile sig_atomic_t *stop) {
  if (k.inconsistent)
    return 20;
  for (;;) {
    const ApplyResult r = propagate_apply(k);
    if (r == ApplyResult::Unsat)
      return 20;
    if (r == ApplyResult::Conflict) {
      if (analyze_apply(k) == ApplyResult::Unsat)
        return 20;
      backjump_apply(k);
      continue;
    }
    if (k.unassigned == 0)
      return 10;
    if (reduce_guard(k) && reduce_advised(k)) {
      reduce_apply(k);
      continue;
    }
    if (restart_guard(k) && restart_advised(k)) {
      restart_apply(k);
      continue;
    }
    if (stop && *stop)
      return 0;
    decide_apply_canonical(k);
  }
}

} // namespace tactix
