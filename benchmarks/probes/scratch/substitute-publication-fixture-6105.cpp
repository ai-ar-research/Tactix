// A short actual-rule fixture: SCC collapse creates roots,then late closure.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/registry.hpp"
#include "rules/substitute/substitute.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace tactix;
static void build(Kernel &k) {
  kernel_reserve_vars(k,16);
  for(Var v=0;v<k.nvars;++v)kernel_record_import(k,v);
  auto large=[&](int a,int b,int c) {
    const Lit lits[]={internalize(a),internalize(b),internalize(c)};
    kernel_add_large_clause(k,lits,3,false,0);
  };
  auto binary=[&](int a,int b) {kernel_add_binary_clause(k,internalize(a),internalize(b));};
  // This earlier large clause sees neither root until a later round/reconnect.
  large(-3,-8,1);large(3,4,5);large(8,9,10);
  binary(-3,4);binary(3,-4);binary(-3,5);binary(3,-5);
  binary(-8,9);binary(8,-9);binary(-8,10);binary(8,-10);
  k.stats.conflicts=1;k.stats.search_ticks=1000000000;
}
int main() {
  for(unsigned rounds:{1u,2u}) {
    Kernel expected,actual;build(expected);build(actual);
    ScopedHeuristics eh(expected),ah(actual);
    for(Kernel *k:{&expected,&actual}) {
      std::fill(k->h->scores.score.begin(),k->h->scores.score.end(),0);
      heuristics_update_scores(*k);
    }
    OccIndex occ;kernel_dense_connect(expected,occ);
    const uint32_t entry=expected.trail.size();
    for(unsigned round=0;round<rounds;++round) {
      const size_t begin=expected.trail.size();
      const auto result=substitute_apply_round(expected,false);
      assert(result!=ApplyResult::Unsat);
      heuristics_consume_root_assignments(expected,begin);
      heuristics_consume_retirements(expected);
      if(result==ApplyResult::NoOp)break;
    }
    const size_t before_reconnect=expected.trail.size();
    assert(substitute_reconnect_large_watches(expected,entry)!=ApplyResult::Unsat);
    heuristics_consume_root_assignments(expected,before_reconnect);
    kernel_dense_disconnect(expected);
    DefaultStrategyConfig cfg;cfg.inprocess.phase_rounds=1;cfg.inprocess.substitute_rounds=rounds;
    DefaultPolicy policy(cfg);policy.inprocess().build(actual);
    bool enabled=false;
    for(auto &e:policy.inprocess().entries) {
      const bool first=!enabled && !std::strcmp(tactix_rules()[e.rule].name,"Substitute");
      e.enabled=first;enabled|=first;
    }
    assert(policy.inprocess().run_phase(actual,SlotGroup::Probe,nullptr)==0);
    assert(actual.stats.substitute_vars==expected.stats.substitute_vars);
    assert(actual.values==expected.values);
    const bool same=actual.h->scores.stack==expected.h->scores.stack;
    std::printf("rounds=%u eliminated=%llu heap_equal=%d expected:",rounds,
                (unsigned long long)actual.stats.substitute_vars,int(same));
    for(Var v:expected.h->scores.stack)std::printf(" %u",v+1);
    std::printf(" actual:");for(Var v:actual.h->scores.stack)std::printf(" %u",v+1);
    std::puts("");
  }
}
