#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/registry.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace tactix;

class Ladder final : public BveBoundPolicy {
  bool terminal_;
public:
  explicit Ladder(bool terminal):terminal_(terminal){}
  const char *name() const override{return "cut-bound-fixture";}
  uint32_t num_passes() const override{return terminal_?1:3;}
  uint32_t bound(uint32_t pass) const override {
    const uint32_t levels[]={0,3,5};assert(pass<num_passes());return levels[pass];
  }
};
class Recorder final : public FactorMatchPolicy {
public:
  std::vector<uint32_t> seen;
  const char *name() const override{return "cut-bound-recorder";}
  void set_bound(uint32_t bound) override{seen.push_back(bound);}
  double seed_score(const Kernel &,Lit,double) override{return 0;}
  double next_factor_score(const Kernel &,Lit,uint32_t,Lit,double) override{return 0;}
  bool accept(uint32_t,uint32_t,int) override{return false;}
};
static uint64_t eliminated(const Kernel &k){return k.stats.bve_eliminated;}

static void check(bool terminal) {
  Kernel k;Heuristics h;k.h=&h;kernel_reserve_vars(k,8);
  for(int v=1;v<=8;v+=2)kernel_add_binary_clause(k,internalize(v),internalize(v+1));
  k.stats.conflicts=1;
  Ladder ladder(terminal);Recorder recorder;DefaultPolicy policy;
  auto &scheduler=policy.inprocess();
  scheduler.bve_bound=&ladder;scheduler.factor_match=&recorder;
  const uint64_t entry_material = k.subsume_material_count;
  scheduler.elim_clock.material_mark = entry_material - 1;
  DefaultInprocessScheduler::SlotEntry bve;
  bve.rule=tactix_rule_id("BVE");bve.group=SlotGroup::Eliminate;
  bve.enabled=true;bve.dosed=true;bve.order=CandidateOrder::Heuristic;
  bve.meter=eliminated;bve.permille=0;
  scheduler.entries.push_back(bve);
  assert(scheduler.run_phase(k,SlotGroup::Eliminate,nullptr)==0);
  assert(k.stats.bve_eliminated==1 && scheduler.entries[0].limit==0);
  assert(scheduler.entries[0].tm.admitted==1);
  assert(recorder.seen.size()==1 && recorder.seen[0]==0);
  // Variables 7 and 8 belong to an untouched disjoint pair. The native phase
  // releases its schedule before final completion bookkeeping, so even at
  // the terminal bound it never clears their retry bits on a quota cut.
  if(!terminal)for(uint8_t retry:k.bve_retry)assert(retry);
  else assert(k.bve_retry[var_of(internalize(7))] && k.bve_retry[var_of(internalize(8))]);
  assert(scheduler.elim_clock.marks_mark==(terminal?k.stats.bve_marks:0));
  assert(scheduler.elim_clock.material_mark ==
         (terminal ? k.subsume_material_count : entry_material - 1));
  const uint64_t saved_material = scheduler.elim_clock.material_mark;
  assert(scheduler.run_phase(k,SlotGroup::Probe,nullptr)==0);
  assert(scheduler.elim_clock.material_mark == saved_material);
  assert(recorder.seen.size()==2);
  assert(recorder.seen[1]==(terminal?0:3));
  assert(k.propagate_head==k.trail.size() && !k.dense && k.large_watches_connected);
  assert(tactix_rules()[bve.rule].guard(k));kernel_check_invariants(k);
  std::printf("terminal=%u after-cut Factor bound=%u retry7=%u retry8=%u\n",terminal,
              recorder.seen[1],k.bve_retry[var_of(internalize(7))],k.bve_retry[var_of(internalize(8))]);
}
int main(){check(false);check(true);std::puts("BVE cut-bound lifecycle: PASS");}
