#undef NDEBUG
#include "policies/default_policy.hpp"
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include <cassert>
#include <climits>
#include <cstdio>
using namespace tactix;

namespace tactix {
struct DefaultNolWalkTestAccess {
  static bool due(DefaultPolicy &p,const Kernel &k){return p.nol_walk_due(k);}
  static void schedule(DefaultPolicy &p,const Kernel &k,
                       const NolHypergraphSliceResult &r){p.schedule_nol_walk(k,r);}
};
}

static DefaultStrategyConfig config() {
  DefaultStrategyConfig c;
  c.nol_walk.enabled=true;
  c.nol_walk.maximum_authentication_work=99;
  c.nol_walk.work_quantum=7;
  c.nol_walk.initial_tick_interval=10;
  c.nol_walk.maximum_delay_ticks=40;
  c.nol_walk.fruitlessness_threshold=2;
  return c;
}

int main() {
  FILE *input=std::tmpfile();
  assert(input);
  const char formula[] =
      "p cnf 9 6\n"
      "1 2 3 0\n4 5 6 0\n7 8 9 0\n"
      "-1 -4 -7 0\n-2 -5 -8 0\n-3 -6 -9 0\n";
  assert(std::fwrite(formula,1,sizeof formula-1,input)==sizeof formula-1);
  std::rewind(input);
  Kernel k;
  assert(parse_dimacs(input,k)==nullptr);
  std::fclose(input);
  NolHypergraphWalkSession session;
  k.nol_hypergraph_walk_session=&session;
  const auto authentication=nol_hypergraph_model_authenticate(k);
  assert(authentication.status==NolHypergraphAuthenticationStatus::Authenticated);
  DefaultPolicy policy(config());
  policy.reset_schedule(k);

  k.stats.search_ticks=9;
  assert(!DefaultNolWalkTestAccess::due(policy,k));
  k.stats.search_ticks=10;
  assert(DefaultNolWalkTestAccess::due(policy,k));

  NolHypergraphSliceResult pending;
  pending.status=NolHypergraphSliceStatus::Pending;
  DefaultNolWalkTestAccess::schedule(policy,k,pending);
  assert(policy.nol_walk_diagnostics().next_ticks==17);
  assert(policy.nol_walk_diagnostics().delay_ticks==10);
  assert(policy.nol_walk_diagnostics().fruitless_completions==0);

  NolHypergraphSliceResult complete;
  complete.status=NolHypergraphSliceStatus::TrajectoryComplete;
  k.stats.search_ticks=17;
  DefaultNolWalkTestAccess::schedule(policy,k,complete);
  assert(policy.nol_walk_diagnostics().next_ticks==27);
  assert(policy.nol_walk_diagnostics().fruitless_completions==1);
  k.stats.search_ticks=27;
  DefaultNolWalkTestAccess::schedule(policy,k,complete);
  assert(policy.nol_walk_diagnostics().delay_ticks==20);
  assert(policy.nol_walk_diagnostics().next_ticks==47);
  assert(policy.nol_walk_diagnostics().fruitless_completions==0);

  NolHypergraphSliceResult useful;
  useful.status=NolHypergraphSliceStatus::TrajectoryComplete;
  useful.useful_hint=true;
  k.stats.search_ticks=47;
  DefaultNolWalkTestAccess::schedule(policy,k,useful);
  assert(policy.nol_walk_diagnostics().delay_ticks==10);
  assert(policy.nol_walk_diagnostics().next_ticks==57);

  // An ordinary restart does not wake noL, so a partial reuse restart cannot
  // accidentally run the root-only service.
  ++k.stats.restarts;
  assert(!DefaultNolWalkTestAccess::due(policy,k));
  ++k.stats.root_assignments;
  assert(DefaultNolWalkTestAccess::due(policy,k));
  DefaultNolWalkTestAccess::schedule(policy,k,pending);
  kernel_affine_note_namespace_change(k);
  assert(DefaultNolWalkTestAccess::due(policy,k));

  // Every clock addition saturates rather than wrapping into an early dose.
  k.stats.search_ticks=UINT64_MAX-3;
  DefaultNolWalkTestAccess::schedule(policy,k,pending);
  assert(policy.nol_walk_diagnostics().next_ticks==UINT64_MAX);
  k.stats.search_ticks=UINT64_MAX-1;
  assert(!DefaultNolWalkTestAccess::due(policy,k));
  k.stats.search_ticks=UINT64_MAX;
  assert(DefaultNolWalkTestAccess::due(policy,k));

  DefaultStrategyConfig invalid=config();
  invalid.nol_walk.work_quantum=0;
  assert(!invalid.valid());
  invalid=config();invalid.nol_walk.maximum_delay_ticks=9;
  assert(!invalid.valid());
  const auto serialized=config().serialize();
  assert(serialized.find("nol_walk.enabled=1")!=std::string::npos);
  assert(serialized.find("nol_walk.maximum_authentication_work=99")!=std::string::npos);
  assert(serialized.find("nol_walk.work_quantum=7")!=std::string::npos);
  assert(serialized.find("nol_walk.initial_tick_interval=10")!=std::string::npos);
  assert(serialized.find("nol_walk.maximum_delay_ticks=40")!=std::string::npos);
  assert(serialized.find("nol_walk.fruitlessness_threshold=2")!=std::string::npos);
}
