// Public strategy adapter for the existing pure cubic PHP event planner.
// No Kernel, Proof or internal rule headers: every event is checked by the
// generic terminal clausal-program API before any proof is published.
#include "php_cubic.h"
#include "tactix_php_certificate.hpp"
#include <algorithm>
#include <climits>
#include <map>
namespace php=tactix_strategy::php_certificate;
extern "C" int php_cubic(tactix *s,const int *matrix,int holes,
                          uint64_t *additions,tactix_clausal_effort *effort) {
  const int first=tactix_fresh_variable_start(s);
  if (first<0 || holes<2) return -1;
  php::Matrix input(size_t(holes)+1,std::vector<int32_t>(holes));
  for (int i=0;i<=holes;++i)
    for (int j=0;j<holes;++j) input[i][j]=matrix[size_t(i)*holes+j];
  php::Plan plan;
  if (php::plan(input,first,plan)!=php::Status::Ok || plan.events.size()>INT_MAX)
    return -1;
  std::vector<tactix_clausal_step> program;program.reserve(plan.events.size());
  std::map<php::Clause,std::vector<uint32_t>> handles;
  for (const auto &event:plan.events) {
    auto key=event.clause;std::sort(key.begin(),key.end());
    if (event.kind==php::EventKind::Delete) {
      // Keep original clauses. The generic batch may retire only its own
      // additions; complete RAT validation accounts for all retained input.
      if (event.role==php::EventRole::RetireOriginalAmo) continue;
      auto found=handles.find(key);
      if (found==handles.end() || found->second.empty()) return -1;
      const uint32_t source=found->second.back();found->second.pop_back();
      if (found->second.empty()) handles.erase(found);
      program.push_back({TACTIX_CLAUSAL_DELETE,nullptr,0,source});
    } else {
      program.push_back({event.kind==php::EventKind::AddRat ? TACTIX_CLAUSAL_RAT : TACTIX_CLAUSAL_RUP,
                          event.clause.data(),int(event.clause.size()),0});
      handles[std::move(key)].push_back(uint32_t(program.size()));
    }
  }
  *additions=plan.accounting.additions;
  return tactix_refute_clausal(s,first,uint32_t(plan.accounting.fresh_variables),
      program.data(),int(program.size()),UINT64_MAX,effort);
}
