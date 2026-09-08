// Fixed kernel-level diagnostic, not generated fuzz or a real-instance solve.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/registry.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
using namespace tactix;
static uint64_t zero_meter(const Kernel &) { return 0; }
int main() {
  for (unsigned extra = 0; extra != 3; ++extra) {
    Kernel k; Heuristics h; k.h=&h;
    kernel_reserve_vars(k, 2+2*extra);
    kernel_add_binary_clause(k,internalize(1),internalize(2));
    kernel_add_binary_clause(k,internalize(-1),internalize(2));
    for (unsigned i=0;i<extra;++i) {
      kernel_add_binary_clause(k,internalize(1),internalize(3+2*i));
      kernel_add_binary_clause(k,internalize(-1),internalize(4+2*i));
    }
    k.stats.conflicts=1; k.bve_gate_mask=0; k.bve_backward=false;
    DefaultStrategyConfig cfg; cfg.inprocess.phase_rounds=1;
    DefaultPolicy policy(cfg);
    DefaultInprocessScheduler::SlotEntry entry;
    entry.rule=tactix_rule_id("BVE"); entry.group=SlotGroup::Eliminate;
    entry.enabled=true; entry.dosed=true; entry.meter=zero_meter;
    entry.order=CandidateOrder::Heuristic;
    policy.inprocess().entries.push_back(entry);
    std::fill(k.bve_retry.begin(),k.bve_retry.end(),uint8_t{0});
    k.bve_retry[0]=1;
    const int result=policy.inprocess().run_phase(k,SlotGroup::Eliminate,nullptr);
    unsigned retired=0;
    for (Var v=0;v<k.nvars;++v)retired+=kernel_var_retired(k,v);
    std::printf("extra=%u result=%d eliminated=%llu retired=%u roots=%llu unit=%d resolutions=%llu\n",extra,result,
      (unsigned long long)k.stats.bve_eliminated,retired,(unsigned long long)k.stats.root_assignments,
      (int)k.value(internalize(2)),(unsigned long long)k.stats.bve_resolutions);
    assert(result!=20 && k.value(internalize(2))>0);
    kernel_check_invariants(k);
  }
}
