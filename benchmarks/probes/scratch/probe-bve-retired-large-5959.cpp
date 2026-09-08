// Candidate learns u, then deletes a large antecedent satisfied by u while
// the pivot is still active. This can reinsert the pivot before retirement.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/registry.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
using namespace tactix;
static uint64_t zero_meter(const Kernel &) {return 0;}
int main() {
  for (unsigned sides=1;sides<=3;++sides) {
    Kernel k;Heuristics h;k.h=&h;kernel_reserve_vars(k,4);
    kernel_add_binary_clause(k,internalize(1),internalize(2));
    kernel_add_binary_clause(k,internalize(-1),internalize(2));
    if (sides&1) {const Lit c[]={internalize(1),internalize(2),internalize(3)};kernel_add_large_clause(k,c,3,false,0);}
    if (sides&2) {const Lit c[]={internalize(-1),internalize(2),internalize(4)};kernel_add_large_clause(k,c,3,false,0);}
    k.stats.conflicts=1;k.bve_gate_mask=0;k.bve_backward=false;
    DefaultStrategyConfig cfg;cfg.inprocess.phase_rounds=1;
    DefaultPolicy policy(cfg);DefaultInprocessScheduler::SlotEntry e;
    e.rule=tactix_rule_id("BVE");e.group=SlotGroup::Eliminate;e.enabled=true;e.dosed=true;e.meter=zero_meter;e.order=CandidateOrder::Heuristic;
    policy.inprocess().entries.push_back(e);
    std::fill(k.bve_retry.begin(),k.bve_retry.end(),uint8_t{0});k.bve_retry[0]=1;
    const int result=policy.inprocess().run_phase(k,SlotGroup::Eliminate,nullptr);
    unsigned retired=0;for(Var v=0;v<k.nvars;++v)retired+=kernel_var_retired(k,v);
    std::printf("sides=%u result=%d eliminated=%llu retired=%u roots=%llu resolutions=%llu\n",sides,result,
      (unsigned long long)k.stats.bve_eliminated,retired,(unsigned long long)k.stats.root_assignments,(unsigned long long)k.stats.bve_resolutions);
    assert(result!=20 && k.value(internalize(2))>0);kernel_check_invariants(k);
  }
}
