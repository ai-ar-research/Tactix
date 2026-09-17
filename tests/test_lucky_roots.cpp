#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace tactix;

static void core(Kernel &k, int guard, int a, int b, int c) {
  for (const std::vector<int> row : {std::vector<int>{guard,a,b},
                                   {guard,a,-b}, {guard,-a,c}, {guard,-a,-c}}) {
    Lit lits[3];
    for (unsigned i=0;i<3;i++) lits[i]=internalize(row[i]);
    kernel_add_large_clause(k,lits,3,false,0);
  }
}

static unsigned fixture(unsigned roots, bool reverse, bool fold) {
  Kernel k;
  ScopedHeuristics owner(k);
  const unsigned n=roots+1+16;
  kernel_reserve_vars(k,n);
  std::vector<Var> order;
  for (Var v=0;v<n;v++) order.push_back(v);
  if(reverse) std::reverse(order.begin(),order.end());
  for(Var v:order) kernel_record_import(k,v);
  heuristics_seed_activation_order(k,order);
  kernel_add_binary_clause(k,internalize(1),internalize(2));
  kernel_add_binary_clause(k,internalize(1),internalize(-2));
  for(unsigned ev=3;ev<=roots+1;ev++)
    kernel_add_binary_clause(k,internalize(-1),internalize(static_cast<int>(ev)));
  const int base=roots+2;
  core(k,base,base+1,base+2,base+3);
  core(k,-(base+4),base+5,base+6,base+7);
  core(k,base+11,base+8,base+9,base+10);
  core(k,-(base+15),base+12,base+13,base+14);
  // The real startup path executes Lucky before observing the search stop.
  // Inprocessing respects the stop, so it cannot accidentally remove the
  // motivating roots on behalf of the missing Lucky lifecycle consumer.
  volatile sig_atomic_t stop=1;
  DefaultStrategyConfig config;
  config.inprocess.structured_root_before_lucky = false; // reference Lucky lifecycle
  assert(DefaultPolicy(config).solve(k,&stop)==0);
  assert(k.stats.lucky_early_calls==1 && k.stats.lucky_late_calls==1);
  assert(k.level==0 && kernel_root_fixed(k)==roots);
  unsigned stale=0;
  for(Var v=0;v<k.nvars;v++)
    if(kernel_root_value(k,make_lit(v,false))) stale+=heuristics_queue_contains(k,v);
  assert(k.unassigned_active==17);
  assert(kernel_compact_variables(k,fold));
  heuristics_compact_random_order_fixed(k);
  // A repaired policy's inactive roots do not consume rank positions even
  // when their proof/model identities remain physically hot.
  unsigned live_rank=0;
  for(Var v=owner.h.queue_first;v!=INVALID_VAR;v=owner.h.links[v].next) {
    if(!kernel_root_value(k,make_lit(v,false))) {
      ++live_rank;
      if(!stale) assert(owner.h.links[v].stamp==live_rank);
    }
  }
  assert(live_rank==17);
  kernel_check_invariants(k);
  return stale;
}

int main(int argc,char **argv) {
  const bool negative=argc==2 && !std::strcmp(argv[1],"--expect-missing");
  assert(argc==1 || negative);
  unsigned stale=0, expected=0, cases=0;
  for(unsigned roots:{1u,5u,29u}) for(bool reverse:{false,true}) for(bool fold:{false,true}) {
    stale+=fixture(roots,reverse,fold); expected+=roots; ++cases;
  }
  std::printf("Lucky root consumer: cases=%u stale=%u expected-old=%u\n",cases,stale,expected);
  assert(negative ? stale==expected : stale==0);
}
