#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "ipasir.h"
#include "tactix_ipasir.h"
#include "api/api_internal.hpp"
#include "strategy/action_execution.hpp"
#include "strategy/bve_schedule.hpp"
#include "strategy/bve_invocation.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using Formula=std::vector<std::vector<int>>;
static const std::vector<Formula> formulas{
  {{1,2,3},{-1,2,4},{1,-2,5},{-1,-2,-3},{3,4},{-3,5},{6,7},{-6,8},{-7,8},{-8,4}},
  {{1,2},{1,-2},{-1,2},{-1,-2}},
  {{1,2},{1,3},{-1,2,4},{-1,4,5},{-2,6},{-4,-6,7},{-7,8},{-8,5}},
  {{1,2,3},{-2,4},{-3,4},{1}},
  {{1,2,3},{-1,2,4},{1,-2,5},{-1,-2,-3},{3,4},{-3,5},{6,7},{-6,8},{-7,8},{-8,4}},
  {{1,2},{1,-2},{-1,2},{-1,-2},{3,4,5},{3}}
};
static int rule(const char *name) {
  for(int i=0;i<tactix_rule_count();++i)if(std::string(tactix_rule_name(i))==name)return i;
  assert(false);return -1;
}
static unsigned variables(const Formula &f) {
  unsigned n=0;for(const auto &c:f)for(int l:c)n=std::max(n,unsigned(std::abs(l)));return n;
}
static void profile(tactix_c_handle *s,uint32_t occurrences) {
  tactix_online_parameters p{};p.kind=TACTIX_ONLINE_PARAMETERS_BVE;
  p.value.bve.growth_bound=16;p.value.bve.occurrence_limit=occurrences;
  p.value.bve.clause_size_limit=p.value.bve.resolvent_size_limit=UINT32_MAX;
  p.value.bve.xor_arity_limit=tactix::strategy::BveParameters::XOR_ARITY_DEFAULT;
  p.value.bve.definition_cores=tactix::strategy::BveParameters::DEFINITION_CORES_DEFAULT;
  assert(!tactix_install_parameters(s,rule("BVE"),&p));
}
static tactix_c_handle *create(const Formula &f,const std::string &proof="",int format=0,bool stable=false) {
  auto *s=tactix_init();assert(s);
  for(const auto &c:f) {for(int l:c)assert(!tactix_add_checked(s,l));assert(!tactix_add_checked(s,0));}
  if(format)assert(!tactix_proof(s,proof.c_str(),format));
  profile(s,f.size());
  assert(!tactix_set_guidance_publication(s,TACTIX_GUIDANCE_NATIVE));
  assert(tactix_support(s,TACTIX_SUPPORT_ROOT_PREPARATION,0)==0);
  if(stable) {
    assert(!tactix_apply(s,rule("SwitchMode")));
    assert(!tactix_bump_variable(s,1,2));
    assert(tactix_stat(s,TACTIX_STAT_STABLE)==1);
  }
  assert(tactix_support(s,TACTIX_SUPPORT_ENTER_INPROCESS_PHASE,TACTIX_PHASE_ELIMINATE)==0);
  assert(tactix_support(s,TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION,0)==0);
  return s;
}
static bool applicable(tactix_c_handle *s,int r) {
  std::vector<int> ids(tactix_rule_count());
  const int n=tactix_rules_applicable(s,ids.data(),ids.size());
  return std::find(ids.begin(),ids.begin()+n,r)!=ids.begin()+n;
}
static std::string run(const std::string &prefix,int format,bool native) {
  std::ostringstream trace;
  for(size_t index=0;index<formulas.size();++index) {
    const auto &f=formulas[index];const unsigned n=variables(f);
    const std::string stem=prefix.empty()?"":prefix+"-"+std::to_string(index);
    if(!stem.empty()) {
      std::ofstream cnf(stem+".cnf");cnf<<"p cnf "<<n<<' '<<f.size()<<'\n';
      for(const auto &c:f) {for(int l:c)cnf<<l<<' ';cnf<<"0\n";}
    }
    auto *s=create(f,stem+".proof",format,index==4);
    auto &k=tactix::api_detail::kernel_for_fixed_testing(s);
    std::vector<uint64_t> candidates;for(unsigned v=1;v<=n;++v)candidates.push_back(TACTIX_LIT_INSTANCE(v));
    tactix::strategy::InvocationBudget budget(tactix::strategy::WorkUnit::BveTicks,
        tactix::strategy::WorkLimit::unlimited());
    std::unique_ptr<tactix::strategy::BveInvocationSession> direct;
    tactix_bve_order *order=nullptr;
    if(native) {
      const auto &r=tactix::tactix_rules()[rule("BVE")];
      direct=std::make_unique<tactix::strategy::BveInvocationSession>(r,k,
          tactix::strategy::BveParameters::from_kernel(k),budget);
      assert(direct->valid());direct->order_by_score_heap(candidates.data(),candidates.size());
    }
    else {order=tactix_bve_order_create(s,candidates.data(),candidates.size());assert(order);}
    if(index==3||index==5) {
      assert(k.startup_certificate_open);
      const auto before=k.live_irredundant_large;
      if(native) {
        tactix::execution::execute_action(k,tactix::execution::CloseStartupCertificateRequest{});
        direct->flush_units_while_connected(0);
      } else assert(!tactix_bve_order_flush_units(s,order,0));
      assert(!k.startup_certificate_open&&k.live_irredundant_large<before);
      trace<<"case="<<index<<" first_flush=0\n";
    }
    bool nonzero_relevance=false,applied_bve=false;
    for(unsigned steps=0;;++steps) {
      assert(steps<1000);
      uint64_t instance=0;uint32_t begin=0;
      if(native) {if(direct->ordering_empty())break;instance=direct->next_ordered();begin=k.propagate_head;}
      else {const int code=tactix_bve_order_next(s,order,&instance,&begin);assert(code>=0);if(!code)break;}
      tactix_bve_candidate_state observed{};assert(!tactix_read_bve_candidate(s,instance,&observed));
      nonzero_relevance|=observed.relevance>0;
      const uint32_t skip=TACTIX_BVE_CANDIDATE_ASSIGNED|TACTIX_BVE_CANDIDATE_RETIRED|
          TACTIX_BVE_CANDIDATE_RETAINED|TACTIX_BVE_CANDIDATE_AFFINE|TACTIX_BVE_CANDIDATE_QUERY;
      int result=1,propagated=1;
      if(!(observed.flags&skip)&&(observed.flags&TACTIX_BVE_CANDIDATE_RETRY)) {
        if(native) {
          const auto applied=direct->apply_one_with(instance,[&] {
            result=tactix_apply_instance(s,rule("BVE"),instance);
            return result==20?tactix::ApplyResult::Unsat:result==2?tactix::ApplyResult::Conflict:
                result==1?tactix::ApplyResult::NoOp:tactix::ApplyResult::Ok;
          });
          assert(applied.applications==1);
        } else result=tactix_bve_order_apply(s,order,rule("BVE"),instance);
        assert(result>=0);applied_bve|=result==0;
        if(result!=20&&applicable(s,rule("Propagate"))) {
          propagated=native?tactix_apply(s,rule("Propagate")):
              tactix_bve_order_apply(s,order,rule("Propagate"),TACTIX_INSTANCE_CANONICAL);
          assert(propagated>=0);
        }
        if(result!=20&&propagated!=20) {
          if(native)direct->flush_units_while_connected(begin);
          else assert(!tactix_bve_order_flush_units(s,order,begin));
        }
      }
      trace<<"case="<<index<<" pop="<<instance<<" flags="<<observed.flags
          <<" pos="<<observed.positive_occurrences<<" neg="<<observed.negative_occurrences
          <<" begin="<<begin<<" result="<<result<<" propagated="<<propagated<<'\n';
      if(result==20||propagated==20)break;
    }
    if(index==4)assert(nonzero_relevance);
    direct.reset();tactix_bve_order_release(order);
    const int left=tactix_support(s,TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE,0);assert(left==0||left==20);
    assert(tactix_stat(s,TACTIX_STAT_BVE_ELIMINATED)>0 ||
        ((index==1||index==5)&&tactix_stat(s,TACTIX_STAT_BVE_RESOLUTIONS)>0));
    if(index!=1&&index!=5)assert(applied_bve); // before any DefaultPolicy fallback
    const int answer=tactix_solve(s);assert(answer==(index==1||index==5?20:10));
    trace<<"case="<<index<<" answer="<<answer<<" eliminated="<<tactix_stat(s,TACTIX_STAT_BVE_ELIMINATED)
        <<" resolutions="<<tactix_stat(s,TACTIX_STAT_BVE_RESOLUTIONS)<<" ticks="<<tactix_stat(s,TACTIX_STAT_TICKS)<<'\n';
    if(answer==10&&!stem.empty()) {
      std::ofstream model(stem+".model");for(unsigned v=1;v<=n;++v)model<<tactix_value(s,v)<<' ';model<<"0\n";
    }
    tactix_release(s);
  }
  return trace.str();
}

static void lifetime_and_invalidation() {
  auto *s=create(formulas[0]);auto &k=tactix::api_detail::kernel_for_fixed_testing(s);
  uint64_t instances[]{1,2,3,4,5,6,7,8};
  assert(!tactix_bve_order_create(s,nullptr,1));
  const uint64_t bad[]{TACTIX_INSTANCE_CANONICAL};assert(!tactix_bve_order_create(s,bad,1));
  auto *a=tactix_bve_order_create(s,instances,8),*b=tactix_bve_order_create(s,instances,8);assert(a&&b);
  std::fill(std::begin(instances),std::end(instances),TACTIX_INSTANCE_CANONICAL);
  assert(!k.bve_occurrence_count_image&&!k.bve_score_update_hook);
  uint64_t x=0,y=0;uint32_t bx=0,by=0;
  assert(tactix_bve_order_next(s,a,&x,nullptr)==-1&&x==0);
  assert(tactix_bve_order_next(s,a,&x,&bx)==1);
  const auto epoch=k.strategy_epoch;
  tactix_bve_candidate_state observation{};assert(!tactix_read_bve_candidate(s,x,&observation));
  (void)applicable(s,rule("BVE"));(void)tactix_stat(s,TACTIX_STAT_BVE_MARKS);
  assert(k.strategy_epoch==epoch);
  assert(tactix_bve_order_apply(s,a,rule("Decide"),x)==-1);
  assert(tactix_bve_order_flush_units(s,a,UINT32_MAX)==-1);
  assert(tactix_bve_order_next(s,b,&y,&by)==1&&x==y&&bx==by);
  for(unsigned i=0;i<8;++i)instances[i]=i+1;
  assert(tactix_bve_order_apply(s,a,rule("BVE"),x)>=0);
  y=1234;by=5678;
  assert(tactix_bve_order_next(s,b,&y,&by)==-1&&y==1234&&by==5678);
  if(applicable(s,rule("Propagate")))
    assert(tactix_bve_order_apply(s,a,rule("Propagate"),TACTIX_INSTANCE_CANONICAL)>=0);
  assert(!tactix_bve_order_flush_units(s,a,bx));
  tactix_bve_order_release(a);tactix_bve_order_release(b);
  // Pending detached orders never hide the otherwise legal collection action.
  a=tactix_bve_order_create(s,instances,8);assert(a);
  assert(tactix_support(s,TACTIX_SUPPORT_COLLECT_GARBAGE,0)==0);
  assert(tactix_bve_order_next(s,a,&y,&by)==-1);
  tactix_bve_order_release(a);
  a=tactix_bve_order_create(s,instances,8);assert(a);
  tactix_set_bve_bound(s,17);assert(tactix_bve_order_next(s,a,&y,&by)==-1);
  tactix_bve_order_release(a);
  a=tactix_bve_order_create(s,instances,8);assert(a);
  const auto posed=tactix::strategy::BveParameters::from_kernel(k);
  tactix_online_parameters changed{};changed.kind=TACTIX_ONLINE_PARAMETERS_BVE;
  changed.value.bve={posed.gate_mask,posed.growth_bound,posed.occurrence_limit-1,posed.clause_size_limit,
      posed.xor_arity_limit,posed.resolvent_size_limit,posed.backward_occurrence_limit,
      posed.definition_cores,posed.definition_core_budget_multiplier,posed.definition_budget,
      posed.backward,int(posed.candidate_scope),posed.resolvent_count_limit,int(posed.traversal),
      posed.on_the_fly_strengthening};
  const auto before=k.strategy_epoch;
  assert(!tactix_install_parameters(s,rule("BVE"),&changed)&&k.strategy_epoch==before);
  assert(tactix_bve_order_next(s,a,&y,&by)==-1); // profile check, not epoch
  tactix_bve_order_release(a);
  a=tactix_bve_order_create(s,instances,8);assert(a);
  tactix_release(s); // closes the phase; the order retains no hook into it
  assert(tactix_bve_order_next(s,a,&y,&by)==-1);
  tactix_bve_order_release(a);
}

static void physical_reinsertion_and_first_closure() {
  auto *s=create({{1,2},{-2,3}});
  const uint64_t selected[]{1};auto *o=tactix_bve_order_create(s,selected,1);assert(o);
  uint64_t instance=0;uint32_t begin=0;
  assert(tactix_bve_order_next(s,o,&instance,&begin)==1&&instance==1);
  assert(tactix_bve_order_next(s,o,&instance,&begin)==0);
  assert(tactix_bve_order_apply(s,o,rule("BVE"),2)==0);
  bool reinserted=false;
  while(tactix_bve_order_next(s,o,&instance,&begin)==1)reinserted|=instance==1;
  assert(reinserted); // the removed clause re-arms an already-popped endpoint
  tactix_bve_order_release(o);tactix_release(s);
  s=create({{1,2},{-2,3}});
  assert(!tactix_support(s,TACTIX_SUPPORT_RETIRE_BVE_CANDIDATE,1));
  const uint64_t seed[]{2};o=tactix_bve_order_create(s,seed,1);assert(o);
  assert(tactix_bve_order_next(s,o,&instance,&begin)==1&&instance==2);
  assert(tactix_bve_order_apply(s,o,rule("BVE"),instance)==0);
  bool ghost=false;
  while(tactix_bve_order_next(s,o,&instance,&begin)==1)if(instance==1) {
    tactix_bve_candidate_state observed{};
    assert(!tactix_read_bve_candidate(s,instance,&observed));
    ghost=true; // real clause removal rearms retry advice before notifying the heap
  }
  assert(ghost); // real reinsertion extends the initial seed set
  tactix_bve_order_release(o);tactix_release(s);
  s=create({{1,2},{-2,3}});
  auto &posed=tactix::api_detail::kernel_for_fixed_testing(s);
  const auto variable=tactix::var_of(tactix::kernel_internalize(posed,1));
  assert(!tactix_support(s,TACTIX_SUPPORT_RETIRE_BVE_CANDIDATE,1));
  {
    // Isolate the notification contract from clause removal's retry rearm.
    tactix::strategy::BveOrdering direct(posed,nullptr,0);
    assert(direct.empty()&&!posed.bve_retry[variable]);
    tactix::kernel_notify_bve_score(posed,variable,true);
    assert(!direct.empty()&&direct.next()==1&&!posed.bve_retry[variable]);
  }
  tactix_release(s);
  s=create(formulas[3]);auto &k=tactix::api_detail::kernel_for_fixed_testing(s);
  assert(k.startup_certificate_open);
  assert(tactix_support(s,TACTIX_SUPPORT_FORWARD_CLOSURE,0)>=0);
  assert(!k.startup_certificate_open);tactix_release(s);
}

static void propagation_boundary() {
  auto *s=create({{1},{1,2},{2,3}});
  auto &k=tactix::api_detail::kernel_for_fixed_testing(s);
  assert(tactix::kernel_propagation_fixpoint(k));
  k.unflushed_root=1; // a preceding batch may finish its own propagation
  const uint64_t selected[]{2};
  auto *a=tactix_bve_order_create(s,selected,1),*b=tactix_bve_order_create(s,selected,1);
  assert(a&&b);
  assert(tactix_apply(s,rule("Propagate"))==-1&&k.unflushed_root==1);
  assert(!tactix_bve_order_propagate(s,a)&&!k.unflushed_root);
  assert(!k.bve_score_update_hook&&!k.bve_occurrence_count_image);
  uint64_t instance=1234;uint32_t begin=5678;
  assert(tactix_bve_order_next(s,b,&instance,&begin)==-1&&instance==1234&&begin==5678);
  assert(tactix_bve_order_next(s,a,&instance,&begin)==1&&instance==2);
  tactix_bve_order_release(a);tactix_bve_order_release(b);tactix_release(s);
}

static void stable_order_oracle() {
  auto *s=create({{1,2},{-1,-2}});
  assert(!tactix_apply(s,rule("SwitchMode")));
  auto &k=tactix::api_detail::kernel_for_fixed_testing(s);
  const auto a=tactix::var_of(tactix::kernel_internalize(k,1));
  const auto b=tactix::var_of(tactix::kernel_internalize(k,2));
  // Pose an equal-score stable fixture, independently of the shared ordering.
  k.h->scores.score[a]=k.h->scores.score[b]=0;
  const uint64_t seed[]{1,2};
  const auto first=[&] {
    auto *o=tactix_bve_order_create(s,seed,2);assert(o);
    uint64_t instance=0;uint32_t begin=0;
    assert(tactix_bve_order_next(s,o,&instance,&begin)==1);
    tactix_bve_order_release(o);return instance;
  };
  assert(first()==1); // equal populations/relevance: initial compact-order tie
  assert(!tactix_bump_variable(s,1,2));
  assert(k.h->scores.score[a]>k.h->scores.score[b]);
  assert(first()==2); // lower relevance gives the smaller elimination score
  tactix_release(s);
}

struct Closing {
  tactix_bve_order *order=nullptr,*detached=nullptr;
  tactix_c_handle *view=nullptr;
  unsigned releases=0;
  bool propagation=false;
};
static void close_from_learn(void *opaque,int *clause) {
  auto &state=*static_cast<Closing *>(opaque);
  if(!state.order||(state.propagation&&(clause[0]!=3||clause[1])))return;
  auto *order=state.order;state.order=nullptr;++state.releases;
  tactix_bve_order_release(order);
}
static int closing_client(void *opaque,void *,tactix_c_handle *view,unsigned) {
  auto &state=*static_cast<Closing *>(opaque);
  profile(view,state.propagation?3:4);
  assert(!tactix_set_guidance_publication(view,TACTIX_GUIDANCE_NATIVE));
  assert(!tactix_support(view,TACTIX_SUPPORT_ROOT_PREPARATION,0));
  assert(!tactix_support(view,TACTIX_SUPPORT_ENTER_INPROCESS_PHASE,TACTIX_PHASE_ELIMINATE));
  assert(!tactix_support(view,TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION,0));
  const uint64_t candidates[]{state.propagation?2u:1u,2};
  const unsigned count=state.propagation?1:2;
  state.view=view;
  state.detached=tactix_bve_order_create(view,candidates,count);assert(state.detached);
  state.order=tactix_bve_order_create(view,candidates,count);assert(state.order);
  uint64_t candidate=0;uint32_t begin=0;
  assert(tactix_bve_order_next(view,state.order,&candidate,&begin)==1);
  const int result=tactix_bve_order_apply(view,state.order,rule("BVE"),candidate);
  if(state.propagation) {
    assert(result==0&&!state.releases&&state.order);
    assert(tactix_bve_order_propagate(view,state.order)==0);
  } else assert(result==20);
  assert(state.releases==1&&!state.order);
  assert(tactix_support(view,TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE,0)==(state.propagation?0:20));
  return state.propagation?tactix_status(view):20;
}
static void callback_release() {
  for(bool propagation:{false,true}) {
    const Formula input=propagation?Formula{{1,2},{1,-2},{-1,3}}:formulas[1];
    void *session=ipasir_init();assert(session);
    for(const auto &c:input) {for(int l:c)ipasir_add(session,l);ipasir_add(session,0);}
    Closing state;state.propagation=propagation;
    ipasir_set_learn(session,&state,2,close_from_learn);
    assert(!tactix_ipasir_set_strategy(session,&state,closing_client));
    assert(ipasir_solve(session)==(propagation?10:20)&&state.releases==1);
    if(propagation)for(const auto &clause:input) {
      bool satisfied=false;for(int l:clause)satisfied|=ipasir_val(session,std::abs(l))==l;
      assert(satisfied);
    }
    ipasir_release(session);
    uint64_t instance=1234;uint32_t begin=5678;
    assert(tactix_bve_order_next(state.view,state.detached,&instance,&begin)==-1);
    assert(instance==1234&&begin==5678);tactix_bve_order_release(state.detached);
  }
}

int main(int argc,char **argv) {
  if(argc==4) {
    const int format=std::string(argv[2])=="dsr"?TACTIX_PROOF_DSR_ASCII:
        std::string(argv[2])=="pbp"?TACTIX_PROOF_PBP:0;
    std::fputs(run(argv[1],format,std::string(argv[3])=="native").c_str(),stdout);return 0;
  }
  assert(argc==1);lifetime_and_invalidation();physical_reinsertion_and_first_closure();stable_order_oracle();propagation_boundary();callback_release();
  assert(run("",0,true)==run("",0,false));
}
