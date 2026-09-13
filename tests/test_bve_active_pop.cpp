// Root-producing elimination can reinsert its pivot before retiring it.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/registry.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>
using namespace tactix;
static uint64_t zero_meter(const Kernel &) { return 0; }
static void run(unsigned sides,bool negative,bool reverse,bool stable,
                bool refute,ProofFormat format,const std::string &stem) {
  const bool keep=!stem.empty();
  const std::string path=keep?stem+".proof":"/tmp/tactix-bve-active-"+std::to_string(getpid())+".proof";
  Proof proof;assert(proof.open(path.c_str(),format));
  Kernel k;kernel_reserve_vars(k,6);k.proof=&proof;
  proof.bind_variable_map(&k.internal_to_external);
  const int u=negative?-2:2;
  std::vector<std::vector<int>> input={{1,u},{-1,u}};
  if(sides&1)input.push_back({1,u,3});
  if(sides&2)input.push_back({-1,u,4});
  if(reverse && input.size()>3)std::swap(input[2],input[3]);
  if(refute)for(int a:{5,-5})for(int b:{6,-6})input.push_back({a,b});
  for(const auto &row:input) {
    std::vector<Lit> lits;for(int l:row)lits.push_back(internalize(l));
    const uint64_t id=++k.next_clause_id;
    if(lits.size()==2)kernel_add_binary_clause(k,lits[0],lits[1],id);
    else kernel_add_large_clause(k,lits.data(),lits.size(),false,0,id);
  }
  ScopedHeuristics h(k);k.h->stable=stable;
  if(stable)heuristics_update_scores(k);
  k.stats.conflicts=1;k.bve_gate_mask=0;k.bve_backward=false;
  const auto &rule=tactix_rules()[tactix_rule_id("BVE")];
  std::fill(k.bve_retry.begin(),k.bve_retry.end(),uint8_t{0});
  uint64_t instances[12];const uint32_t n=rule.instances(k,instances,12);
  assert(rule.guard(k) && std::find(instances,instances+n,lit_instance(k,internalize(1)))!=instances+n);
  k.bve_retry[0]=1;
  DefaultStrategyConfig cfg;cfg.inprocess.phase_rounds=1;
  DefaultPolicy policy(cfg);DefaultInprocessScheduler::SlotEntry e;
  e.rule=tactix_rule_id("BVE");e.group=SlotGroup::Eliminate;e.enabled=true;e.dosed=true;
  e.meter=zero_meter;e.order=CandidateOrder::Heuristic;policy.inprocess().entries.push_back(e);
  assert(policy.inprocess().run_phase(k,SlotGroup::Eliminate,nullptr)!=20);
  unsigned retired=0;for(Var v=0;v<k.nvars;++v)retired+=kernel_var_retired(k,v);
  assert(retired==1u+bool(sides&1)+bool(sides&2));
  assert(k.stats.bve_eliminated==retired);
  assert(k.value(internalize(u))>0 && !k.level && k.propagate_head==k.trail.size());
  assert(!k.dense && !k.bve_score_update_hook && !k.bve_occurrence_count_image);
  kernel_check_invariants(k);
  const uint64_t eliminated=k.stats.bve_eliminated;
  assert(default_solve(k)==(refute?20:10));
  if(!refute) {
    kernel_extend_model(k);
    for(const auto &row:input) {
      bool satisfied=false;for(int l:row)satisfied|=k.value(internalize(l))>0;
      assert(satisfied);
    }
  }
  proof.flush();
  if(keep) {
    std::ofstream cnf(stem+".cnf");cnf<<"p cnf 6 "<<input.size()<<'\n';
    for(const auto &row:input){for(int l:row)cnf<<l<<' ';cnf<<"0\n";}
  } else assert(!std::remove(path.c_str()));
  std::printf("sides=%u negative=%u reverse=%u stable=%u refute=%u eliminated=%llu retired=%u model/proof-path PASS\n",
              sides,negative,reverse,stable,refute,(unsigned long long)eliminated,retired);
}
int main(int argc,char **argv) {
  if(argc==2) {
    for(bool negative:{false,true})for(bool stable:{false,true})for(bool lrat:{false,true}) {
      const std::string stem=std::string(argv[1])+"-"+std::to_string(negative)+"-"+std::to_string(stable)+(lrat?"-lrat":"-drat");
      run(3,negative,false,stable,true,lrat?ProofFormat::Lrat:ProofFormat::DratAscii,stem);
    }
  } else {
    for(unsigned sides=1;sides<=3;++sides)for(bool negative:{false,true})for(bool reverse:{false,true})for(bool stable:{false,true})
      run(sides,negative,reverse,stable,false,ProofFormat::DratAscii,"");
  }
}
