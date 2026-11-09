#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "query_test_support.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/bve/bve.hpp"

using namespace tactix;
using namespace tactix::session;
using query_test::clause;

static void ranked_reuse(bool stable) {
  QueryHistory history; QueryKernel query;
  for (auto pair : {std::pair{1,2}, {3,4}, {5,6}, {7,8}})
    clause(history,{pair.first,pair.second});
  assert(history.assume(1)==InputStatus::Ok);
  assert(history.begin_query()==InputStatus::Ok && query.replay(history)==ReplayStatus::Ok);
  const auto ticket=query.ticket(); Kernel &k=*query.kernel(ticket);
  assert(query.assigned_assumption_floor(ticket)==0);
  assert(query.decide_assumption(ticket,0) && propagate_service(k)==ApplyResult::Ok);
  for (int literal : {3,5}) {
    assert(decide_apply(k,kernel_internalize(k,literal),false)==ApplyResult::Ok);
    assert(propagate_service(k)==ApplyResult::Ok);
  }
  k.h->stable=stable;
  for (int literal : {1,5,7,3}) heuristics_queue_bump(k,var_of(kernel_internalize(k,literal)));
  for (const auto [literal,rank] : {std::pair{1,0u}, {3,1u}, {5,10u}, {7,5u}})
    heuristics_score_set(k,var_of(kernel_internalize(k,literal)),rank);
  const Var next=var_of(kernel_internalize(k,7));
  assert(heuristics_restart_reuse_level_for(k,next)==0);
  assert(heuristics_restart_reuse_level_for(k,next,1)==(stable?1u:2u));
  assert(heuristics_restart_reuse_level_for(k,next,3)==3);
  DefaultPolicy policy; std::vector<std::pair<std::string,std::string>> options;
  auto *view=static_cast<tactix_c_handle *>(api_detail::create_query_execution(query,policy,options,nullptr));
  const auto trail=k.trail;
  const auto frames=k.frames;
  const auto scores=k.h->scores.score;
  const auto decisions=k.stats.decisions, ticks=k.stats.ticks;
  uint32_t result=99;
  for (unsigned i=0;i<8;++i) {
    assert(tactix_query_assumption_floor(view,&result)==0 && result==1);
    assert(tactix_restart_reuse_level(view,7,0,&result)==0 && result==0);
    assert(tactix_restart_reuse_level(view,-7,1,&result)==0 && result==(stable?1u:2u));
    assert(tactix_restart_reuse_level(view,7,3,&result)==0 && result==3);
  }
  assert(k.trail==trail && k.frames==frames && k.h->scores.score==scores);
  assert(k.stats.decisions==decisions && k.stats.ticks==ticks);
  for (int invalid : {0, INT32_MIN, 1000, 1}) {
    result=99;
    assert(tactix_restart_reuse_level(view,invalid,0,&result)==-1 && result==99);
  }
  assert(tactix_restart_reuse_level(view,7,4,&result)==-1 && result==99);
  auto *projection=k.query_projection; k.query_projection=nullptr;
  assert(tactix_query_assumption_floor(view,&result)==-1 && result==99);
  k.query_projection=projection;
  assert(tactix_query_assumption_floor(view,nullptr)==-1);
  assert(tactix_restart_reuse_level(view,7,1,nullptr)==-1);
  api_detail::destroy_query_execution(view);
  assert(restart_apply_to(k,0)==ApplyResult::Ok);
  assert(query.assigned_assumption_floor(ticket)==0);
}

static void retired_and_foreign() {
  QueryHistory history; QueryKernel query;
  clause(history,{1,2}); assert(history.assume(1)==InputStatus::Ok);
  assert(history.begin_query()==InputStatus::Ok && query.replay(history)==ReplayStatus::Ok);
  const auto ticket=query.ticket(); Kernel &k=*query.kernel(ticket);
  const auto &bve=tactix_rules()[tactix_rule_id("BVE")];
  assert(bve.apply_instance(k,lit_instance(k,kernel_internalize(k,1)))==ApplyResult::Ok);
  assert(query.assigned_assumption_floor(ticket)==0);
  assert(decide_apply(k,internalize(1),true)==ApplyResult::Ok);
  assert(query.assigned_assumption_floor(ticket)==1);
  assert(restart_apply_to(k,0)==ApplyResult::Ok);
  assert(decide_apply(k,internalize(-1),true)==ApplyResult::Ok);
  assert(query.assigned_assumption_floor(ticket)==0); // false A is not certified true
  assert(query.next_assumption(ticket).status==AssumptionStatus::Propagate);
  assert(propagate_service(k)==ApplyResult::Ok);
  assert(query.next_assumption(ticket).status==AssumptionStatus::Backtrack);
}

static void implied_and_fixed() {
  QueryHistory history; QueryKernel query;
  clause(history,{-5,1}); assert(history.assume(1)==InputStatus::Ok);
  assert(history.begin_query()==InputStatus::Ok && query.replay(history)==ReplayStatus::Ok);
  const auto ticket=query.ticket(); Kernel &k=*query.kernel(ticket);
  assert(decide_apply(k,kernel_internalize(k,5),false)==ApplyResult::Ok);
  assert(propagate_service(k)==ApplyResult::Ok);
  assert(query.next_assumption(ticket).status==AssumptionStatus::Ready);
  assert(query.assigned_assumption_floor(ticket)==1); // includes the foreign reason level
  assert(restart_apply_to(k,0)==ApplyResult::Ok);
  assert(query.assigned_assumption_floor(ticket)==0);
  QueryHistory fixed_history; QueryKernel fixed;
  clause(fixed_history,{1}); assert(fixed_history.assume(1)==InputStatus::Ok);
  assert(fixed_history.begin_query()==InputStatus::Ok && fixed.replay(fixed_history)==ReplayStatus::Ok);
  assert(fixed.assigned_assumption_floor(fixed.ticket())==0);
  kernel_compact_variables(*fixed.kernel(fixed.ticket()),true);
  assert(fixed.assigned_assumption_floor(fixed.ticket())==0);
}

static void stale_ticket() {
  QueryHistory history; QueryKernel query;
  clause(history,{1,2}); assert(history.begin_query()==InputStatus::Ok);
  assert(query.replay(history)==ReplayStatus::Ok);
  DefaultPolicy policy; std::vector<std::pair<std::string,std::string>> options;
  auto *view=static_cast<tactix_c_handle *>(api_detail::create_query_execution(query,policy,options,nullptr));
  uint32_t out=99;
  assert(tactix_query_assumption_floor(view,&out)==0 && out==0);
  assert(tactix_restart_reuse_level(view,1,0,&out)==0 && out==0);
  // Invalidate the ticket without destroying the still-borrowed epoch storage.
  assert(history.finish_query(query.ticket().query,QueryResult::Unknown)==InputStatus::Ok);
  out=99;
  assert(tactix_query_assumption_floor(view,&out)==-1 && out==99);
  assert(tactix_restart_reuse_level(view,1,0,&out)==-1 && out==99);
  api_detail::destroy_query_execution(view);
}

static void cold_plain_guidance() {
  auto *plain=tactix_init(); assert(plain);
  Kernel &k=api_detail::kernel_for_fixed_testing(plain);
  kernel_defer_cold_state(k);
  kernel_reserve_vars(k,2);
  assert(k.cold_state_deferred && k.bve_retry.empty());
  uint32_t out=99;
  assert(tactix_restart_reuse_level(plain,1,0,&out)==0 && out==0);
  assert(k.cold_state_deferred && k.bve_retry.empty());
  tactix_release(plain);
}

int main() {
  uint32_t result=99;
  auto *plain=tactix_init(); assert(plain);
  assert(tactix_query_assumption_floor(plain,&result)==-1 && result==99);
  assert(tactix_query_assumption_floor(nullptr,&result)==-1 && result==99);
  assert(tactix_restart_reuse_level(nullptr,1,0,&result)==-1 && result==99);
  tactix_release(plain);
  ranked_reuse(false); ranked_reuse(true); retired_and_foreign(); implied_and_fixed(); stale_ticket(); cold_plain_guidance();
}
