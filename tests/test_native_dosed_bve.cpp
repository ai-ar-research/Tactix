// Real DefaultInprocessScheduler BVE entry, for an independent Python loop.
#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "policies/default_inprocess.hpp"
#include "strategy/bve_invocation.hpp"
#include <cassert>
#include <csignal>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace tactix;
using Formula=std::vector<std::vector<int>>;
static const std::vector<Formula> formulas{
 {{1,2,3},{-1,2,4},{1,-2,5},{-1,-2,-3},{3,4},{-3,5},{6,7},{-6,8},{-7,8},{-8,4}},
 {{1,2},{1,-2},{-1,2},{-1,-2}},
 {{1,2},{-1,3},{-3,4},{4}},
 {{1,2},{1,-2},{-1,3},{-3,4},{5,6},{-5,7},{-7,8}},
 {{1,2},{1,-2},{-1,3},{1,7,8},{4,5,6},{-4,5,6},{4,-5,6},{4,5,-6}},
 {{1,2},{-1,2},{-2,3},{-2,-3}},
 {{1,2},{1,-2},{-1,3},{1,7,8},{4,5,6},{-4,5,6},{4,-5,6},{4,5,-6},
  {9,10},{9,-10},{-9,10},{-9,-10}},
};
struct FixedBound final:BveBoundPolicy {
 const char *name()const override{return "dosed-bve-fixture";}
 uint32_t num_passes()const override{return 1;}
 uint32_t bound(uint32_t)const override{return 0;}
};
static void profile_json(const strategy::BveParameters &p) {
 std::cout<<"{\"gate_mask\":"<<p.gate_mask<<",\"growth_bound\":"<<p.growth_bound
 <<",\"occurrence_limit\":"<<p.occurrence_limit<<",\"clause_size_limit\":"<<p.clause_size_limit
 <<",\"xor_arity_limit\":"<<p.xor_arity_limit<<",\"resolvent_size_limit\":"<<p.resolvent_size_limit
 <<",\"backward_occurrence_limit\":"<<p.backward_occurrence_limit<<",\"definition_cores\":"<<p.definition_cores
 <<",\"definition_core_budget_multiplier\":"<<p.definition_core_budget_multiplier<<",\"definition_budget\":"<<p.definition_budget
 <<",\"backward\":"<<p.backward<<",\"on_the_fly_strengthening\":"<<p.on_the_fly_strengthening
 <<",\"candidate_scope\":"<<unsigned(p.candidate_scope)<<",\"resolvent_count_limit\":"<<p.resolvent_count_limit
 <<",\"traversal\":"<<unsigned(p.traversal)<<'}';
}
static constexpr int stats[]{TACTIX_STAT_BVE_ELIMINATED,TACTIX_STAT_BVE_RESOLUTIONS,
 TACTIX_STAT_PROPAGATIONS,TACTIX_STAT_TICKS,TACTIX_STAT_ROOT_FIXED,
 TACTIX_STAT_ACTIVE_VARIABLES,TACTIX_STAT_BVE_MARKS,TACTIX_STAT_ARENA_WORDS,
 TACTIX_STAT_SEARCH_TICKS,TACTIX_STAT_DECISIONS,TACTIX_STAT_PROBE_TICKS};
int main(int argc,char **argv) {
 assert(argc==1||argc==3);
 const std::string prefix=argc==3?argv[1]:"",mode=argc==3?argv[2]:"none";
 unsigned id=0;
 for(unsigned f=0;f<formulas.size();++f)for(bool aware:{false,true})
 for(unsigned permille:{0u,1000u})for(bool stable:{false,true})for(bool halted:{false,true}) {
  if(halted&&(f||aware||permille!=1000||stable))continue;
  const auto &formula=formulas[f];unsigned vars=0;
  auto *s=tactix_init();assert(s);
  for(const auto &clause:formula){for(int l:clause){vars=std::max(vars,unsigned(std::abs(l)));tactix_add(s,l);}tactix_add(s,0);}
  const std::string stem=prefix.empty()?"":prefix+"-"+std::to_string(id);
  if(!stem.empty()) {
   std::ofstream cnf(stem+".cnf");cnf<<"p cnf "<<vars<<' '<<formula.size()<<'\n';
   for(const auto &clause:formula){for(int l:clause)cnf<<l<<' ';cnf<<"0\n";}
   if(mode!="none")assert(!tactix_proof(s,(stem+".proof").c_str(),mode=="dsr"?TACTIX_PROOF_DSR_ASCII:TACTIX_PROOF_PBP));
  }
  (void)tactix_status(s); // settle the public input/proof owner before native dispatch
  assert(!tactix_set_guidance_publication(s,TACTIX_GUIDANCE_NATIVE));
  tactix_set_bve_gates(s, f==0||f>=4?0:BVE_GATE_KISSAT_DEFAULT);
  if(stable)assert(!tactix_apply(s,tactix_rule_id("SwitchMode")));
  if(f==3) {
   assert(!tactix_set_variable_retention(s,5,1));
   assert(!tactix_decide(s,-1));
   assert(tactix_apply(s,tactix_rule_id("Propagate"))==2);
   assert(tactix_apply(s,tactix_rule_id("ConflictAnalysis"))==0);
   assert(tactix_stat(s,TACTIX_STAT_CONFLICTS)>0);
  }
  if(f==4||f==6)for(int variable:{1,3,7,8})assert(!tactix_bump_variable(s,variable,vars));
  DefaultStrategyConfig config;config.inprocess.bve_occurrence_limit=formula.size();
  DefaultInprocessScheduler native(&config.inprocess);native.configure();
  auto &k=api_detail::kernel_for_fixed_testing(s);native.reset(k);native.pose_initialized();
  FixedBound bound;native.bve_bound=&bound;
  DefaultInprocessScheduler::SlotEntry entry;entry.rule=tactix_rule_id("BVE");
  entry.group=SlotGroup::Eliminate;entry.preprocess=true;entry.max_applies=1;
  entry.dosed=true;entry.order=CandidateOrder::Heuristic;entry.budget_aware=aware;
  entry.permille=permille;entry.meter=[](const Kernel &state){return state.stats.bve_resolutions;};
  native.entries.push_back(entry);
  k.work_budget=123;k.work_budget_offset=5;k.midround_budget=true;
  k.tick_sink=&k.stats.probe_ticks;
  volatile sig_atomic_t stop=halted;
  const int phase=native.run_phase(k,SlotGroup::Eliminate,&stop);
  assert(phase==0||phase==20);
  std::cout<<"{\"id\":"<<id<<",\"formula\":"<<f<<",\"aware\":"<<aware
    <<",\"halted\":"<<halted<<",\"permille\":"<<permille<<",\"stable\":"<<stable<<",\"phase\":"<<phase<<",\"profile\":";
  profile_json(strategy::BveParameters::from_kernel(k));
  std::cout<<",\"limit\":"<<native.entries[0].limit<<",\"stats\":[";
  for(unsigned i=0;i<std::size(stats);++i)std::cout<<(i?",":"")<<tactix_stat(s,stats[i]);
  if(phase!=20)assert(!k.unflushed_root);
  const auto &clock=native.elim_clock;
  std::cout<<"],\"clock\":["<<clock.next_conflicts<<','<<clock.rounds<<','<<clock.reductions_mark
   <<','<<clock.marks_mark<<','<<clock.fixed_mark<<','<<clock.material_mark<<','<<clock.blocked
   <<','<<clock.blocked_episodes<<','<<clock.anchor<<"],\"budget\":"<<k.work_budget<<",\"applies\":"<<native.entries[0].applies;
  stop=0;
  const int answer=tactix_solve(s);assert(answer==(f==1||f>=5?20:10));
  std::cout<<",\"answer\":"<<answer<<"}\n";
  if(answer==10&&!stem.empty()) {std::ofstream model(stem+".model");for(unsigned v=1;v<=vars;++v)model<<tactix_value(s,v)<<' ';model<<"0\n";}
  tactix_release(s);++id;
 }
 assert(id==57);
}
