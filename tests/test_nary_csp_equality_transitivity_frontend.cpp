#undef NDEBUG
#include <cstdint>
#include <cstdio>
namespace capi {
extern "C" {
#include "tactix.h"
}
}
#include "frontend/parse.hpp"
#include "policies/random_controller_policy.hpp"
#include "strategy/controller_observation.hpp"
#include "strategy/random_online.hpp"
#include "rules/semantic_extension/nary_csp_equality_transitivity_action.hpp"
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <type_traits>
using namespace tactix;
using namespace tactix::strategy;
namespace {
using Selection=NaryCspEqualityTransitivitySelection;
const std::array<std::array<size_t,3>,3> owners{{{0,1,3},{0,2,4},{1,2,5}}};
std::vector<std::vector<int32_t>> clauses(){
 std::vector<std::vector<int32_t>> out;
 for(size_t d=0;d<6;++d) { out.push_back({int32_t(2*d+1),int32_t(2*d+2)});out.push_back({-int32_t(2*d+1),-int32_t(2*d+2)}); }
 for(auto t:owners) for(size_t j=0;j<2;++j) out.push_back({-int32_t(2*t[0]+j+1),-int32_t(2*t[1]+j+1),-int32_t(2*t[2]+j+1)});
 return out;
}
NaryCspEqualityTransitivityParameters profile(){return {1000,10000,1000000,1000000,16,1000,10000,1000,10000,100000,100,100,1000000,1000,1000,1000000};}
NaryCspEqualityParameters equality_profile(){return {1000,10000,1000000,16,1000,10000,1000,10000,100000,1,100,100};}
OnlineInvocation action(const Selection &selection){
 OnlineInvocation a; a.id=4501;a.rule="NaryCspEqualityTransitivity";a.parameters=profile();
 a.granularity=InvocationGranularity::OneInstance;a.budget=WorkLimit::bounded(1);
 a.nary_csp_equality_transitivity_instances={selection};return a;
}
void execute(Kernel &k,OnlineInvocation a){OnlineStrategyProgram p;p.sequence={std::move(a)};OnlineStrategyExecutor e;assert(e.configure(p,1));auto r=e.advance(k);assert(r.status==InvocationStatus::Completed&&r.made_rule_progress);}
void parse(Kernel &k){FILE *f=std::tmpfile();assert(f);auto cs=clauses();std::fprintf(f,"p cnf 12 %zu\n",cs.size());for(auto &c:cs){for(auto l:c)std::fprintf(f,"%d ",l);std::fputs("0\n",f);}std::rewind(f);assert(!parse_dimacs(f,k));std::fclose(f);}
Selection prepare(Kernel &k,NaryCspRelationOwnerSession &relations,NaryCspEqualitySession &e){
 k.nary_csp_relation_owner_session=&relations;k.nary_csp_equality_session=&e;
 Selection selected;for(size_t d=0;d<3;++d)selected.domains[d]={int32_t(2*d+1),int32_t(2*d+2)};
 size_t i=0;for(auto owner:owners){
  std::vector<std::vector<int32_t>> ds;for(auto d:owner)ds.push_back({int32_t(2*d+1),int32_t(2*d+2)});
  OnlineInvocation equality;equality.id=4200+i;equality.rule="NaryCspEquality";equality.parameters=equality_profile();equality.granularity=InvocationGranularity::OneInstance;equality.nary_csp_equality_instances={{ds,{0,1,2}}};execute(k,equality);
  for(const auto &r:e.records())if(r.key.domains==ds){selected.equalities[i]=r.key;selected.equality_variables[i]=r.extension_variable;}
  ++i;
 }assert(nary_csp_equality_transitivity_selection_valid(selected));return selected;
}
void typed_replay_declines_and_consumption(){
 for(unsigned mode=0;mode<4;++mode){
  Kernel k;parse(k);FILE *journal=std::tmpfile();assert(journal);k.structural_provenance=journal;
  NaryCspRelationOwnerSession relations;NaryCspEqualitySession equality;NaryCspEqualityTransitivitySession session;k.nary_csp_equality_transitivity_session=&session;
  auto selected=prepare(k,relations,equality);auto a=action(selected);
  if(mode==1)std::get<NaryCspEqualityTransitivityParameters>(a.parameters).maximum_recognition_work=1;
  if(mode==2)++a.nary_csp_equality_transitivity_instances[0].equality_variables[2];
  volatile sig_atomic_t stop=1;if(mode==3)k.stop=&stop;
  OnlineStrategyProgram p;p.sequence={a};std::string error;assert(validate_online_strategy_program(p,&error));
  auto wire=serialize_online_strategy_program(p);OnlineStrategyProgram replay;assert(deserialize_online_strategy_program(wire,replay,&error));assert(serialize_online_strategy_program(replay)==wire);assert(replay.sequence[0].nary_csp_equality_transitivity_instances==a.nary_csp_equality_transitivity_instances);
  if(mode==0){
   auto malformed=p;malformed.sequence[0].granularity=InvocationGranularity::NativeProducerOffer;
   assert(!validate_online_strategy_program(malformed,&error,true));
   malformed=p;malformed.sequence[0].parameters=NaryCspEqualityAmoParameters{};
   assert(!validate_online_strategy_program(malformed,&error));
   malformed=p;malformed.sequence[0].nary_csp_equality_transitivity_instances[0].equalities[0].domains.clear();
   assert(!validate_online_strategy_program(malformed,&error));
  }
  const auto arena=k.arena;const auto watches=k.watches.arena;const auto next=k.next_clause_id;const auto extent=k.external_to_internal.size();
  OnlineStrategyExecutor executor;assert(executor.configure(p,1));auto result=executor.advance(k);
  if(mode==0){
   assert(result.status==InvocationStatus::Completed&&result.made_rule_progress&&k.external_to_internal.size()==extent);
   assert(session.keys().size()==1&&k.stats.nary_csp_equality_transitivity_ternary_clauses==3);
   OnlineStrategyExecutor duplicate;assert(duplicate.configure(p,2));assert(duplicate.advance(k).status==InvocationStatus::NoChange);
   const auto &decide=tactix_rules()[tactix_rule_id("Decide")];const auto &propagate=tactix_rules()[tactix_rule_id("Propagate")];
   for(auto v:{selected.equality_variables[0],selected.equality_variables[1]}){
    assert(apply_rule_instance(decide,k,v)==ApplyResult::Ok);if(propagate.guard(k))assert(apply_rule(propagate,k)==ApplyResult::Ok);
   }
   auto implied=kernel_internalize(k,int32_t(selected.equality_variables[2]));assert(k.values[implied]>0);
   const auto &reason=k.assigned[var_of(implied)];assert(!reason.binary&&reason.reason!=DECISION_REASON&&reason.reason!=UNIT_REASON);
   const auto *c=clause_at(k.arena,reason.reason);assert(c->size==3);
   std::vector<int32_t> cause;for(unsigned j=0;j<c->size;++j)cause.push_back(kernel_externalize(k,c->lits[j]));std::sort(cause.begin(),cause.end());
   std::vector<int32_t> expected{-int32_t(selected.equality_variables[0]),-int32_t(selected.equality_variables[1]),int32_t(selected.equality_variables[2])};std::sort(expected.begin(),expected.end());assert(cause==expected);
   assert(apply_rule_instance(decide,k,1)==ApplyResult::Ok);if(propagate.guard(k)){auto r=apply_rule(propagate,k);assert(r==ApplyResult::Ok||r==ApplyResult::Sat);}
   for(auto &original:clauses()){bool satisfied=false;for(auto l:original)satisfied|=kernel_external_value(k,internalize(l))>0;assert(satisfied);}
  }else{
   assert(result.status==(mode==1?InvocationStatus::ResourceExhausted:mode==2?InvocationStatus::NoChange:InvocationStatus::Interrupted));
   assert(k.arena==arena&&k.watches.arena==watches&&k.next_clause_id==next&&session.keys().empty());
   if(mode==3){k.stop=nullptr;auto retry=executor.advance(k);assert(retry.status==InvocationStatus::Completed&&retry.made_rule_progress);}
  }
  std::fclose(journal);
 }
}
void controller_selection(){
 Kernel k;parse(k);FILE *journal=std::tmpfile();assert(journal);k.structural_provenance=journal;
 NaryCspRelationOwnerSession relations;NaryCspEqualitySession e;NaryCspEqualityTransitivitySession session;k.nary_csp_equality_transitivity_session=&session;
 auto selected=prepare(k,relations,e);RandomControllerConfig config;config.parameters.rules={"NaryCspEqualityTransitivity"};config.offered_actions["NaryCspEqualityTransitivity"]={action(selected)};
 bool used=false;for(uint64_t seed=1;seed<128&&!used;++seed){RandomControllerSelector selector(seed);controller::ActionRequest request;std::string error;assert(selector.choose(k,controller::observe_kernel(k),config,request,error));if(request.type!=controller::ActionType::Rule)continue;
  assert(request.invocation.nary_csp_equality_transitivity_instances==std::vector<Selection>{selected});controller::RestartController executor(k,{});auto result=executor.apply_rule(request,config.allowance("NaryCspEqualityTransitivity"));assert(result.invocation.status==InvocationStatus::Completed&&result.invocation.made_rule_progress);used=true;
 }assert(used);std::fclose(journal);
}
template<class T> capi::tactix_online_step &base_step(T &s){
 if constexpr(std::is_same_v<T,capi::tactix_online_step>)return s;
 else return base_step(s.base);
}
void configure_base(capi::tactix_online_step &s,uint32_t id){s.step_id=id;s.rule_id=id;s.granularity=TACTIX_ONLINE_GRANULARITY_ONE;s.repeat_mode=TACTIX_ONLINE_REPEAT_ONCE;s.repeat_limit=1;s.work_unit=TACTIX_ONLINE_WORK_APPLICATIONS;s.budget=1;}
void c15_deep_copy(){
 auto *solver=capi::tactix_init();assert(solver);FILE *journal=std::tmpfile();assert(journal);
 for(auto &c:clauses()){for(auto l:c)capi::tactix_add(solver,l);capi::tactix_add(solver,0);}
 assert(capi::tactix_bind_structural_provenance(solver,journal)==0);
 int original[6][2];for(int d=0;d<6;++d){original[d][0]=2*d+1;original[d][1]=2*d+2;}
 capi::tactix_online_nary_csp_domain owner_domains[3][3];
 capi::tactix_online_nary_csp_equality_transitivity_instance choice{};
 for(size_t i=0;i<3;++i){
  for(size_t d=0;d<3;++d)owner_domains[i][d]={original[owners[i][d]],2};
  capi::tactix_online_nary_csp_choice choices[]={{1,0},{2,0}};
  capi::tactix_online_nary_csp_prefix_instance prefix{owner_domains[i],3,0,choices,2};
  capi::tactix_online_step_v8 ps{};configure_base(base_step(ps),39);
  base_step(ps).parameters.kind=TACTIX_ONLINE_PARAMETERS_NARY_CSP_PREFIX;
  base_step(ps).parameters.value.nary_csp_prefix={1000,10000,1000000,8,16,1000,7,16,10000,1000,10000,100000,1,100,100,1000000};
  ps.nary_csp_prefix_instances=&prefix;ps.nary_csp_prefix_instance_count=1;
  capi::tactix_online_program_v8 pp{&ps,1,TACTIX_ONLINE_REPEAT_ONCE,1};
  assert(capi::tactix_online_configure_v8(solver,&pp,100+i,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
  capi::tactix_online_result out{};assert(capi::tactix_online_advance(solver,&out)==0&&out.status==TACTIX_ONLINE_COMPLETED&&out.made_rule_progress);
  capi::tactix_online_nary_csp_equality_instance eq{owner_domains[i],3,0,1,2};
  capi::tactix_online_step_v10 es{};configure_base(base_step(es),42);
  base_step(es).parameters.kind=TACTIX_ONLINE_PARAMETERS_NARY_CSP_EQUALITY;
  base_step(es).parameters.value.nary_csp_equality={1000,10000,1000000,16,1000,10000,1000,10000,100000,1,100,100};
  es.nary_csp_equality_instances=&eq;es.nary_csp_equality_instance_count=1;
  capi::tactix_online_program_v10 ep{&es,1,TACTIX_ONLINE_REPEAT_ONCE,1};
  assert(capi::tactix_online_configure_v10(solver,&ep,200+i,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
  assert(capi::tactix_online_advance(solver,&out)==0&&out.status==TACTIX_ONLINE_COMPLETED&&out.made_rule_progress);
  choice.domains[i]={original[i],2};choice.equalities[i]=eq;choice.equality_variables[i]=uint32_t(14+2*i);
 }
 capi::tactix_online_step_v15 step{};configure_base(base_step(step),45);
 step.equality_transitivity_profile=TACTIX_EQUALITY_TRANSITIVITY_PROFILE_EXACT;
 step.equality_transitivity_parameters={1000,10000,1000000,1000000,16,1000,10000,1000,10000,100000,100,100,1000000,1000,1000,1000000};
 step.equality_transitivity_instances=&choice;step.equality_transitivity_instance_count=1;
 capi::tactix_online_program_v15 program{&step,1,TACTIX_ONLINE_REPEAT_ONCE,1};
 step.equality_transitivity_profile=99;
 assert(capi::tactix_online_configure_v15(solver,&program,298,TACTIX_ONLINE_RECONFIGURE_RESET)==-1);
 step.equality_transitivity_profile=TACTIX_EQUALITY_TRANSITIVITY_PROFILE_EXACT;
 step.equality_transitivity_parameters.maximum_planning_work=0;
 assert(capi::tactix_online_configure_v15(solver,&program,299,TACTIX_ONLINE_RECONFIGURE_RESET)==-1);
 step.equality_transitivity_parameters.maximum_planning_work=1000000;
 choice.equalities[0].domain_count=0;
 assert(capi::tactix_online_configure_v15(solver,&program,300,TACTIX_ONLINE_RECONFIGURE_RESET)==-1);
 choice.equalities[0].domain_count=3;
 assert(capi::tactix_online_configure_v15(solver,&program,301,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
 // Every borrowed nested role is copied before execution.
 for(auto &d:original)for(auto &v:d)v=0;
 for(auto &key:choice.equalities){key.left_domain=99;key.domains=nullptr;}
 for(auto &v:choice.equality_variables)v=99;
 capi::tactix_online_result out{};assert(capi::tactix_online_advance(solver,&out)==0);
 assert(out.status==TACTIX_ONLINE_COMPLETED&&out.rule_id==45&&out.made_rule_progress&&out.applications==1);
 capi::tactix_release(solver);std::fclose(journal);
}
}
int main(){assert(tactix_rule_id("NaryCspEqualityTransitivity")==45);typed_replay_declines_and_consumption();controller_selection();c15_deep_copy();}
