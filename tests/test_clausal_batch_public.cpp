#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "kernel/kernel.hpp"
#include "proof/direct_php_certificate.hpp"
#include <algorithm>
#include <cassert>
#include <map>
using namespace tactix;
namespace php=tactix::direct_php_certificate;

static void invalid_atomic() {
  auto *s=tactix_init();for (int l:{1,2,0}) tactix_add(s,l);
  assert(tactix_status(s)==0);
  auto &k=api_detail::kernel_for_fixed_testing(s);
  const auto id=k.next_clause_id,epoch=k.strategy_epoch;
  const auto extent=k.external_to_internal.size();
  int negative[]={-3}, positive[]={3};
  tactix_clausal_step steps[]={{TACTIX_CLAUSAL_RAT,negative,1,0},
    {TACTIX_CLAUSAL_RAT,positive,1,0},{TACTIX_CLAUSAL_RUP,nullptr,0,0}};
  tactix_clausal_effort effort{};
  assert(tactix_refute_clausal(s,3,1,steps,3,UINT64_MAX,&effort)==-1);
  assert(effort.rat_resolvents==1); // complete opposing-pivot check rejects it
  assert(tactix_refute_clausal(s,4,1,steps,3,UINT64_MAX,nullptr)==-1);
  assert(tactix_refute_clausal(s,3,1,steps,3,1,nullptr)==-1);
  tactix_clausal_step delete_twice[]={{TACTIX_CLAUSAL_RAT,negative,1,0},
    {TACTIX_CLAUSAL_DELETE,nullptr,0,1},{TACTIX_CLAUSAL_DELETE,nullptr,0,1},
    {TACTIX_CLAUSAL_RUP,nullptr,0,0}};
  assert(tactix_refute_clausal(s,3,1,delete_twice,4,UINT64_MAX,nullptr)==-1);
  assert(k.next_clause_id==id && k.strategy_epoch==epoch &&
         k.external_to_internal.size()==extent && !k.inconsistent);
  assert(tactix_solve(s)==10);tactix_release(s);
}
static void public_cubic(unsigned n,bool corrupt) {
  auto *s=tactix_init();php::Matrix matrix(n+1,std::vector<int32_t>(n));int32_t v=1;
  for (auto &row:matrix) {for (auto &x:row) {x=v++;tactix_add(s,x);}tactix_add(s,0);}
  for (unsigned j=0;j<n;++j) for (unsigned i=0;i<=n;++i)
    for (unsigned k=i+1;k<=n;++k) {
      tactix_add(s,-matrix[i][j]);tactix_add(s,-matrix[k][j]);tactix_add(s,0);
    }
  const int first=tactix_fresh_variable_start(s);assert(first==v);
  php::Plan plan;assert(php::plan(matrix,first,plan)==php::Status::Ok);
  std::vector<tactix_clausal_step> program;
  std::map<php::Clause,std::vector<uint32_t>> ids;
  for (auto &event:plan.events) {
    auto key=event.clause;std::sort(key.begin(),key.end());
    if (event.kind==php::EventKind::Delete) {
      if (event.role==php::EventRole::RetireOriginalAmo) continue;
      auto &list=ids[key];assert(!list.empty());
      program.push_back({TACTIX_CLAUSAL_DELETE,nullptr,0,list.back()});list.pop_back();
    } else {
      program.push_back({event.kind==php::EventKind::AddRat ? TACTIX_CLAUSAL_RAT : TACTIX_CLAUSAL_RUP,
                         event.clause.data(),int(event.clause.size()),0});
      ids[key].push_back(uint32_t(program.size()));
    }
  }
  if (corrupt) program.push_back({TACTIX_CLAUSAL_RUP,nullptr,0,0}); // early conclusion forbidden
  auto &k=api_detail::kernel_for_fixed_testing(s);
  const auto id=k.next_clause_id,epoch=k.strategy_epoch;const auto vars=k.nvars;
  const int result=tactix_refute_clausal(s,first,uint32_t(plan.accounting.fresh_variables),
      program.data(),int(program.size()),UINT64_MAX,nullptr);
  if (corrupt) {
    assert(result==-1 && !k.inconsistent && k.next_clause_id==id &&
           k.strategy_epoch==epoch && k.nvars==vars &&
           tactix_fresh_variable_start(s)==first);
  } else {
    assert(result==20 && k.inconsistent && k.nvars==vars &&
           k.next_clause_id==id+plan.accounting.additions && k.strategy_epoch==epoch+1);
    assert(tactix_stat(s,TACTIX_STAT_DECISIONS)==0);
  }
  tactix_release(s);
}
int main() {
  invalid_atomic();
  for (unsigned n=2;n<=6;++n) {public_cubic(n,false);public_cubic(n,true);}
}
