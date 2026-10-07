#undef NDEBUG
#include <cassert>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>
namespace capi {
extern "C" {
#include "tactix.h"
}
}
#include "frontend/parse.hpp"
#include "policies/heuristics.hpp"
#include "policies/random_controller_policy.hpp"
#include "strategy/controller_observation.hpp"
#include "strategy/online_inprocess.hpp"
#include "strategy/random_online.hpp"
using namespace tactix;
using namespace tactix::strategy;

static bool same_formula_version(const StateVersion &a,const StateVersion &b) {
 return a.arena_generation==b.arena_generation && a.touch_epoch==b.touch_epoch;
}
static const std::vector<std::vector<int32_t>> clauses = {
 {1,2,3},{-1,-2},{-1,-3},{-2,-3},{4,5},{-4,-5},{11,12},{-11,-12},
 {-4,6},{-7,6},{-6,4,7},{8,9,10},{-8,1},{-8,-6},{-9,2},{-9,-6},{-10,3},{-10,-6}};
static DirectCspGuardedCoverParameters profile() {
 return {100,1000,100000,8,8,8,8,8,8,100000,100,1000,100000,100,100,100000};
}
static OnlineInvocation action() {
 OnlineInvocation s; s.id=901; s.rule="DirectCspSupport"; s.parameters=profile();
 s.granularity=InvocationGranularity::OneInstance; s.budget=WorkLimit::bounded(1);
 s.direct_csp_guarded_cover_instances={{{{1,2,3},{4,5},{11,12}}, {0,0,0},
   {8,9,10}, {{6,{4,7}}}, {{8,0,0,0},{9,0,1,0},{10,0,2,0}}}};
 return s;
}
static Kernel fixture() {
 FILE *f=std::tmpfile(); assert(f);
 std::fprintf(f,"p cnf 12 %zu\n",clauses.size());
 for (const auto &c:clauses) {for(auto l:c) std::fprintf(f,"%d ",l); std::fputs("0\n",f);}
 std::rewind(f); Kernel k; assert(!parse_dimacs(f,k)); std::fclose(f); return k;
}
static void exact_replay_and_validation() {
 OnlineStrategyProgram p; p.sequence={action()}; std::string error;
 assert(validate_online_strategy_program(p,&error));
 auto wire=serialize_online_strategy_program(p); OnlineStrategyProgram decoded;
 assert(wire.find(":direct-csp-guarded-cover2(")!=std::string::npos);
 assert(deserialize_online_strategy_program(wire,decoded,&error));
 assert(serialize_online_strategy_program(decoded)==wire);
 assert(decoded.sequence[0].direct_csp_guarded_cover_instances==p.sequence[0].direct_csp_guarded_cover_instances);
 p.sequence[0].direct_csp_support_instances={{{1,2},{4,5},0}};
 assert(!validate_online_strategy_program(p,&error));
 p.sequence={action()}; p.sequence[0].parameters=DirectCspSupportParameters{};
 assert(!validate_online_strategy_program(p,&error));
 p.sequence={action()}; p.sequence[0].granularity=InvocationGranularity::NativeProducerOffer;
 assert(!validate_online_strategy_program(p,&error,true));
 p.sequence={action()}; std::get<DirectCspGuardedCoverParameters>(p.sequence[0].parameters).maximum_or_inputs=0;
 assert(!validate_online_strategy_program(p,&error));
}
static void typed_execution_and_declines() {
 for (unsigned mode=0;mode<5;++mode) {
  Kernel k=fixture(); FILE *proof=std::tmpfile(); assert(proof); k.structural_provenance=proof;
  DirectCspGuardedCoverActionSession session; k.direct_csp_guarded_cover_action_session=&session;
  auto s=action(); if(mode==1) std::get<DirectCspGuardedCoverParameters>(s.parameters).maximum_recognition_work=1;
  if(mode==2) s.direct_csp_guarded_cover_instances[0].selected_values[1]=1;
  if(mode==3) { k.stop_fuzz=UINT32_MAX; k.stop_fuzz_state=UINT64_C(5721494226847080036); }
  if(mode==4) s.direct_csp_guarded_cover_instances[0].blockers[0].or_definition=99;
  OnlineStrategyProgram p; p.sequence={s}; OnlineStrategyExecutor exec;
  assert(exec.configure(p,901)); const auto before=state_version(k); const std::vector<uint32_t> arena(k.arena.data(), k.arena.data()+k.arena.size());
  auto result=exec.advance(k);
  if(mode==0) {
   assert(result.status==InvocationStatus::Completed && result.made_rule_progress && result.applications==1);
   assert(session.committed_keys()==1 && session.retained_owners()==1);
   assert(k.stats.direct_csp_support_applied==1 && k.stats.direct_csp_support_owner_commits==1);
   assert(k.stats.direct_csp_support_emitted_additions>0);
   OnlineStrategyExecutor duplicate; assert(duplicate.configure(p,902));
   const auto stable=state_version(k); auto again=duplicate.advance(k);
   assert(again.status==InvocationStatus::NoChange && same_formula_version(state_version(k),stable) && session.committed_keys()==1);
   // Continue through the registered public Decide/Propagate actions. The
   // installed tuple clause participates in ordinary propagation; the final
   // complete assignment is checked against every original input clause.
   const auto &decide=tactix_rules()[tactix_rule_id("Decide")];
   const auto &propagate=tactix_rules()[tactix_rule_id("Propagate")];
   for (int32_t decision : {11,1}) {
    assert(kernel_propagation_fixpoint(k) && !k.conflict_valid);
    assert(apply_rule_instance(decide,k,static_cast<uint32_t>(decision))==ApplyResult::Ok);
    const auto propagated=apply_rule(propagate,k);
    assert(propagated==ApplyResult::Ok || propagated==ApplyResult::Sat || propagated==ApplyResult::NoOp);
   }
   assert(kernel_external_value(k,internalize(-4))>0 && !k.unassigned && !k.conflict_valid);
   const auto &reason=k.assigned[var_of(kernel_internalize(k,-4))];
   assert(!reason.binary && reason.reason!=UNIT_REASON && reason.reason!=DECISION_REASON);
   const auto *used=clause_at(k.arena,reason.reason);
   std::vector<int32_t> cause;
   for(uint32_t i=0;i<used->size;++i) cause.push_back(kernel_externalize(k,used->lits[i]));
   std::sort(cause.begin(),cause.end());
   assert((cause==std::vector<int32_t>{-11,-4,-1}));
   for(const auto &c:clauses) {
    bool satisfied=false;
    for(auto l:c) satisfied |= kernel_external_value(k,internalize(l))>0;
    assert(satisfied);
   }
  } else {
   assert(result.status==(mode==1?InvocationStatus::ResourceExhausted:
                          mode==2?InvocationStatus::NoChange:
                          mode==3?InvocationStatus::Interrupted:InvocationStatus::InvalidRequest));
   assert(same_formula_version(state_version(k),before) && std::vector<uint32_t>(k.arena.data(),k.arena.data()+k.arena.size())==arena && session.committed_keys()==0);
   if(mode==1) assert(k.stats.direct_csp_support_resource_exhausted==1);
   if(mode==2) assert(k.stats.direct_csp_support_no_consequence==1);
   if(mode==3) assert(k.stats.stop_fuzz_fires==1 && k.stats.direct_csp_support_interrupted==1);
   if(mode==4) assert(k.stats.direct_csp_support_invalid==1);
  }
  std::fclose(proof);
 }
}
static void wrong_owner_is_stale() {
 Kernel first=fixture(), second=fixture(); FILE *proof=std::tmpfile(); assert(proof);
 first.structural_provenance=proof; second.structural_provenance=proof;
 DirectCspGuardedCoverActionSession session;
 first.direct_csp_guarded_cover_action_session=&session;
 second.direct_csp_guarded_cover_action_session=&session;
 OnlineStrategyProgram p; p.sequence={action()};
 OnlineStrategyExecutor a,b; assert(a.configure(p,901) && b.configure(p,902));
 assert(a.advance(first).status==InvocationStatus::Completed);
 const auto before=state_version(second);
 const std::vector<uint32_t> image(second.arena.data(),second.arena.data()+second.arena.size());
 assert(b.advance(second).status==InvocationStatus::StaleRequest);
 assert(same_formula_version(state_version(second),before) &&
        std::vector<uint32_t>(second.arena.data(),second.arena.data()+second.arena.size())==image);
 assert(session.belongs_to(first) && session.committed_keys()==1 &&
        second.stats.direct_csp_support_stale==1 && !second.stats.direct_csp_support_applied);
 std::fclose(proof);
}
static void controller_public_selection() {
 Kernel k=fixture(); FILE *proof=std::tmpfile(); assert(proof); k.structural_provenance=proof;
 DirectCspGuardedCoverActionSession session; k.direct_csp_guarded_cover_action_session=&session;
 RandomControllerConfig config; config.parameters.rules={"DirectCspSupport"};
 config.offered_actions["DirectCspSupport"]={action()};
 bool selected=false;
 for(uint64_t seed=1;seed<128 && !selected;++seed) {
  RandomControllerSelector selector(seed); controller::ActionRequest request; std::string error;
  assert(selector.choose(k,controller::observe_kernel(k),config,request,error));
  if(request.type!=controller::ActionType::Rule) continue;
  selected=true;
  assert(request.invocation.direct_csp_guarded_cover_instances==action().direct_csp_guarded_cover_instances);
  controller::RestartController executor(k,{});
  const auto out=executor.apply_rule(request,config.allowance("DirectCspSupport"));
  assert(out.invocation.status==InvocationStatus::Completed && out.invocation.made_rule_progress);
  assert(session.committed_keys()==1 && k.stats.direct_csp_support_applied==1);
 }
 assert(selected); std::fclose(proof);
}
static void c_deep_copy() {
 auto *solver=capi::tactix_init(); FILE *proof=std::tmpfile(); assert(solver&&proof);
 for(const auto &c:clauses){for(auto l:c)capi::tactix_add(solver,l);capi::tactix_add(solver,0);}
 assert(capi::tactix_bind_structural_provenance(solver,proof)==0);
 capi::tactix_online_step_v14 step{};
 auto &base=step.base.base.base.base.base.base.base.base.base.base.base.base.base;
 base.step_id=901; base.rule_id=35; base.granularity=TACTIX_ONLINE_GRANULARITY_ONE;
 base.repeat_mode=TACTIX_ONLINE_REPEAT_ONCE; base.repeat_limit=1;
 base.work_unit=TACTIX_ONLINE_WORK_APPLICATIONS; base.budget=1;
 step.direct_csp_profile=TACTIX_DIRECT_CSP_PROFILE_GUARDED_COVER_V2;
 step.guarded_cover_parameters={100,1000,100000,8,8,8,8,8,8,100000,100,1000,100000,100,100,100000};
 int32_t a[]={1,2,3},b[]={4,5},c[]={11,12},cover[]={8,9,10},inputs[]={4,7};
 uint32_t selected[]={0,0,0};
 capi::tactix_direct_csp_literal_array domains[]={{a,3},{b,2},{c,2}};
 capi::tactix_direct_csp_exact_or ors[]={{6,{inputs,2}}};
 capi::tactix_direct_csp_blocker blockers[]={{8,0,0,0},{9,0,1,0},{10,0,2,0}};
 capi::tactix_direct_csp_guarded_cover_selection payload{domains,selected,3,{cover,3},ors,1,blockers,3};
 step.guarded_cover_instances=&payload; step.guarded_cover_instance_count=1;
 capi::tactix_online_program_v14 p{&step,1,TACTIX_ONLINE_REPEAT_ONCE,1};
 step.direct_csp_profile=99;
 assert(capi::tactix_online_configure_v14(solver,&p,900,TACTIX_ONLINE_RECONFIGURE_RESET)==-1);
 step.direct_csp_profile=TACTIX_DIRECT_CSP_PROFILE_GUARDED_COVER_V2;
 assert(capi::tactix_online_configure_v14(solver,&p,901,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
 // Mutate every nested role after configuration; execution must use its copy.
 a[0]=0;selected[0]=99;cover[0]=0;inputs[0]=0;ors[0].output=0;blockers[0].literal=0;
 capi::tactix_online_result out{};
 assert(capi::tactix_online_advance(solver,&out)==0);
 assert(out.status==TACTIX_ONLINE_COMPLETED && out.rule_id==35 && out.made_rule_progress && out.applications==1);
 capi::tactix_release(solver);std::fclose(proof);
}
int main(){exact_replay_and_validation();typed_execution_and_declines();wrong_owner_is_stale();controller_public_selection();c_deep_copy();std::puts("GuardedCoverV2 public frontend PASS");}
