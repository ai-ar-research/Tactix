#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/substitute/substitute.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/bve_invocation.hpp"
#include <cassert>
#include <cstdlib>
using namespace tactix;
static tactix_c_handle *fixture() {
 auto *s=tactix_init();for(int l:{1,2,3,0})tactix_add(s,l);
 assert(tactix_status(s)==0);return s;
}
static void advice_and_selected_substitution() {
 auto *s=fixture();auto &k=api_detail::kernel_for_fixed_testing(s);
 int q=tactix_define_extension(s,TACTIX_EXTENSION_OR,1,1);assert(q==4);
 assert(tactix_variable_retained(s,q)==1);
 Var v=var_of(kernel_internalize(k,q));double before=k.h->scores.score[v];
 assert(!tactix_bump_variable(s,q,3));assert(k.h->scores.score[v]>before&&k.h->queue_last==v);
 assert(!tactix_bump_variable(s,q,1)); // no staged-candidate dependency
 int candidate=0;assert(tactix_decide_candidates(s,&candidate,1)==1);
 assert(std::abs(candidate)==q);assert(!tactix_decide(s,candidate));
 assert(tactix_stat(s,TACTIX_STAT_DECISIONS_EXTENSION)==1);
 assert(!tactix_support(s,TACTIX_SUPPORT_PREPARATION_RESTART,0));
 const auto size=k.variable_retention.size();
 const auto epoch=k.strategy_epoch;
 assert(tactix_set_variable_retention(s,77,1)==-1&&k.variable_retention.size()==size);
 assert(k.strategy_epoch==epoch);
 assert(tactix_bump_variable(s,q,0)==-1);
 // DefaultPolicy's explicit selection keeps q; the mathematical rule remains
 // available without that selection after the startup deferral.
 k.substitute_retention=&k.variable_retention;
 assert(substitute_apply(k)!=ApplyResult::Unsat);assert(!kernel_var_retired(k,v));
 k.substitute_retention=nullptr;
 const auto release_epoch=k.strategy_epoch;
 assert(!tactix_set_variable_retention(s,q,0));assert(tactix_variable_retained(s,q)==0);
 assert(k.strategy_epoch==release_epoch+1);
 assert(substitute_apply(k)!=ApplyResult::Unsat);assert(kernel_var_retired(k,v));
 tactix_release(s);
}
static void initial_elimination() {
 auto *s=fixture();auto &k=api_detail::kernel_for_fixed_testing(s);
 int q=tactix_define_extension(s,TACTIX_EXTENSION_AND,1,2);assert(q==4);
 const Var v=var_of(kernel_internalize(k,q));
 k.substitute_retention=&k.variable_retention;
 DefaultPolicy p;p.inprocess().build(k);
 assert(p.inprocess().run_initial_fast_elimination(k,nullptr)==0);
 assert(!kernel_var_retired(k,v));
 k.substitute_retention=nullptr;
 const auto &bve=tactix_rules()[tactix_rule_id("BVE")];
 assert(bve.guard(k));(void)bve.apply_instance(k,lit_instance(k,kernel_internalize(k,q)));
 assert(kernel_var_retired(k,v));
 tactix_release(s);
}
static void compact_retention() {
 auto *s=tactix_init();for(int l:{1,0,2,0,3,4,5,0})tactix_add(s,l);
 assert(tactix_status(s)==0);auto &k=api_detail::kernel_for_fixed_testing(s);
 const int q=tactix_define_extension(s,TACTIX_EXTENSION_XOR,3,4);assert(q==6);
 const Var before=var_of(kernel_internalize(k,q));
 assert(kernel_compact_variables(k,true));
 const Var after=var_of(kernel_internalize(k,q));assert(after<before);
 assert(tactix_variable_retained(s,q)==1);
 assert(k.variable_retention.count(k.internal_to_external[after]));
 assert(!tactix_bump_variable(s,q,1));
 assert(tactix_solve(s)==10);assert(tactix_variable_retained(s,q)==0);
 tactix_release(s);
}
static void generated_elimination() {
 auto *s=fixture();auto &k=api_detail::kernel_for_fixed_testing(s);
 const int q=tactix_define_extension(s,TACTIX_EXTENSION_AND,1,-1);assert(q==4);
 const Lit literal=kernel_internalize(k,q);const uint64_t instance=lit_instance(k,literal);
 const auto &bve=tactix_rules()[tactix_rule_id("BVE")];
 for(auto scope:{strategy::BveCandidateScope::All,strategy::BveCandidateScope::PureOnly,
                 strategy::BveCandidateScope::PureClosure}) {
  auto parameters=strategy::BveParameters::from_kernel(k);parameters.candidate_scope=scope;
  std::vector<uint64_t> selected;k.substitute_retention=&k.variable_retention;
  strategy::bve_complete_instances(k,bve,parameters,selected);
  assert(std::find(selected.begin(),selected.end(),instance)==selected.end());
  k.substitute_retention=nullptr;
  strategy::bve_complete_instances(k,bve,parameters,selected);
  assert(std::find(selected.begin(),selected.end(),instance)!=selected.end());
 }
 // Retention selects automatic candidates, never bans a mathematical step.
 k.substitute_retention=&k.variable_retention;assert(bve.guard(k));
 (void)bve.apply_instance(k,instance);assert(kernel_var_retired(k,var_of(literal)));
 k.substitute_retention=nullptr;tactix_release(s);
}
static void interrupted_startup() {
 auto *s=fixture();auto &k=api_detail::kernel_for_fixed_testing(s);
 const int q=tactix_define_extension(s,TACTIX_EXTENSION_AND,1,2);assert(q==4);
 volatile sig_atomic_t stop=1;DefaultPolicy p;assert(p.solve(k,&stop)==0);
 assert(!k.substitute_retention&&tactix_variable_retained(s,q)==1);
 assert(tactix_solve(s)==10);assert(tactix_variable_retained(s,q)==0);
 tactix_release(s);
}
int main(){advice_and_selected_substitution();initial_elimination();compact_retention();generated_elimination();interrupted_startup();}
