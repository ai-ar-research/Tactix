#include "unit_test.hpp"
#include "query_test_support.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/condition/condition.hpp"
#include "rules/substitute/substitute.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
using namespace tactix::session;

static const auto limits = query_test::large_limits;
using query_test::writer;
using query_test::export_query;
using query_test::export_model;
static std::string bytes(const std::string &stem,const std::string &mode) {
  if(stem.empty() || mode=="none")return {};
  FILE *f=std::fopen((stem+".proof").c_str(),"rb");assert(f);
  std::string result;char buffer[4096];
  while(const auto n=std::fread(buffer,1,sizeof buffer,f))result.append(buffer,n);
  assert(!std::ferror(f)&&!std::fclose(f));
  return result;
}
static int solve(QueryKernel &s,QueryHistory &h,const std::string &schedule,DefaultPolicy &policy) {
  if(schedule!="default")return query_test::drive(s,h,schedule=="registry");
  const auto result=policy.solve_query(s);
  assert(result.status==QuerySolveStatus::Complete);
  return result.result;
}
static void hard_false(QueryHistory &h,int g,int a,int b) {
  query_test::clause(h,{-g,a,b});query_test::clause(h,{-g,a,-b});
  query_test::clause(h,{-g,-a,b});query_test::clause(h,{-g,-a,-b});
}
static void sr(Kernel &k) {
  SubstitutionRedundancyInstance x;x.clause={kernel_internalize(k,1)};
  x.witness={kernel_internalize(k,-2),kernel_internalize(k,3)};
  assert(substitution_redundancy_apply_detailed(k,x).rule_result==ApplyResult::Ok);
  propagate_service(k);
}

static void repair(const std::string &family,bool hard,const std::string &prefix,
                   const std::string &mode,const std::string &schedule) {
  QueryHistory h;
  query_test::clause(h,{1,2});
  if(family=="sr") {
    query_test::clause(h,{-1,-2});query_test::clause(h,{-1,3});query_test::clause(h,{-2,4});
    assert(h.assume(-3)==InputStatus::Ok);
  } else if(family=="alias-binding") {
    query_test::clause(h,{-1,2});query_test::clause(h,{1,-2});
    assert(h.assume(2)==InputStatus::Ok);
  } else assert(h.assume(-1)==InputStatus::Ok);
  if(hard) {hard_false(h,5,6,7);assert(h.assume(5)==InputStatus::Ok);}
  assert(h.begin_query()==InputStatus::Ok);export_query(h,prefix);
  const auto input=std::vector<int32_t>(h.input().begin(),h.input().end());
  const auto assumptions=h.active_query()->assumptions;
  QueryKernel s(limits());assert(s.replay(h,writer(prefix.empty()?"":prefix+"-discarded",mode))==ReplayStatus::Ok);
  const auto old=s.ticket();Kernel *identity=s.kernel(old);auto &k=*identity;Heuristics *guidance=k.h;
  assert(s.repair(h).status==NativeAdvanceStatus::NotRequired && s.kernel(old)==identity);
  if(family=="bve") {
    const auto &r=tactix_rules()[tactix_rule_id("BVE")];
    assert(rule_guard(r,k)&&r.apply_instance(k,lit_instance(k,kernel_internalize(k,1)))==ApplyResult::Ok);
  } else if(family=="condition") {
    ConditionInstance c;c.clause={kernel_internalize(k,1),kernel_internalize(k,2)};c.witness={kernel_internalize(k,1)};
    assert(condition_apply_explicit(k,c));
  } else if(family=="sr")sr(k);
  else if(family=="rat") {
    const Lit x=kernel_internalize(k,1);const auto id=++k.next_clause_id;
    k.proof->add_rat(id,&x,1);k.unit_ids[var_of(x)]=id;kernel_assign_unit(k,x);
    k.query_projection->restrict_constant(kernel_external_lit(k,x));propagate_service(k);
  } else {
    assert(family=="binding" || family=="alias-binding");
    if(family=="alias-binding") {
      assert(substitute_apply(k)==ApplyResult::Ok);
      assert(s.projection_state(old).translated_assumptions);
    }
    k.query_projection=nullptr;
  }
  assert(s.next_assumption(old).status==AssumptionStatus::Restore);
  if(family=="bve") {
    k.stats={};
    assert(s.next_assumption(old).status==AssumptionStatus::Restore);
    assert(s.check_model(old).status!=ModelStatus::Valid);
  }
  if(family=="binding" || family=="alias-binding")
    assert(s.check_model(old).status==ModelStatus::InvalidQuery);
  DefaultPolicy policy;
  if(schedule=="default")assert(policy.solve_query(s).status==QuerySolveStatus::RestorationRequired);
  const auto values=k.values;const auto rho=k.reconstruction;const auto id=k.next_clause_id;
  volatile sig_atomic_t stop=1;
  assert(s.repair(h,{},&stop).status==NativeAdvanceStatus::Interrupted);
  assert(s.repair(h,std::make_unique<Proof>()).status==NativeAdvanceStatus::ProofUnavailable);
  assert(s.kernel(old)==identity && k.values==values && k.reconstruction==rho && k.next_clause_id==id);
  k.next_clause_id=UINT64_MAX-1;
  assert(s.repair(h).status==NativeAdvanceStatus::Exhausted);
  assert(s.kernel(old)==identity && k.values==values && k.reconstruction==rho && k.next_clause_id==UINT64_MAX-1);
  k.next_clause_id=id;
  const auto result=s.repair(h,writer(prefix,mode));
  assert(result.status==NativeAdvanceStatus::Ok && !s.kernel(old) && s.kernel(s.ticket())==identity && k.h==guidance);
  assert(s.ticket().query==1 && h.queries()==1 && h.active_query()->assumptions==assumptions);
  assert(std::vector<int32_t>(h.input().begin(),h.input().end())==input && !s.projection_state(s.ticket()).restore);
  assert(s.repair(h).status==NativeAdvanceStatus::NotRequired);
  const int answer=hard?20:10;assert(solve(s,h,schedule,policy)==answer);
  if(answer==10)export_model(h,k,prefix);
  assert(s.flush_proof(s.ticket()));kernel_check_invariants(k);
  std::printf("result=%d query=1 retracted=%llu restored=%llu conflicts=%llu decisions=%llu\n",
    answer,(unsigned long long)result.retracted,(unsigned long long)result.restored,
    (unsigned long long)k.stats.conflicts,(unsigned long long)k.stats.decisions);
}

static void transition(const std::string &origin,bool append,const std::string &prefix,
                       const std::string &mode,const std::string &schedule) {
  QueryHistory h;query_test::clause(h,{1,2});query_test::clause(h,{-1,-2});
  query_test::clause(h,{-1,3});query_test::clause(h,{-2,4});
  query_test::clause(h,{5,-5});query_test::clause(h,{6,-6});query_test::clause(h,{7});
  if(origin=="unsat") {hard_false(h,8,9,10);assert(h.assume(8)==InputStatus::Ok);}
  else assert(h.assume(4)==InputStatus::Ok);
  assert(h.begin_query()==InputStatus::Ok);
  const auto stem=[&](unsigned q) {return prefix.empty()?"":prefix+"-"+std::to_string(q);};
  export_query(h,stem(1));QueryKernel s(limits());assert(s.replay(h,writer(stem(1),mode))==ReplayStatus::Ok);
  Kernel *identity=s.kernel(s.ticket());auto &k=*identity;auto *guidance=k.h;DefaultPolicy policy;
  sr(k);
  const Lit bin[]{kernel_internalize(k,3),kernel_internalize(k,5)};
  for(unsigned i=0;i<2;++i) {const auto id=++k.next_clause_id;k.proof->add_rup(id,bin,2);kernel_add_binary_clause(k,bin[0],bin[1],id);}
  const Lit large[]{bin[0],bin[1],kernel_internalize(k,6)};
  const auto id=++k.next_clause_id;k.proof->add_rup(id,large,3);kernel_add_large_clause(k,large,3,true,3,id);
  int answer=0;
  if(origin=="unknown") {assert(decide_apply(k,kernel_internalize(k,6),false)==ApplyResult::Ok);propagate_service(k);}
  else {answer=origin=="sat"?10:20;assert(solve(s,h,schedule,policy)==answer);if(answer==10)export_model(h,k,stem(1));}
  assert(!k.inconsistent && s.flush_proof(s.ticket()));
  const auto sealed=bytes(stem(1),mode);
  std::printf("q=1 result=%d level=%u conflicts=%llu decisions=%llu\n",answer,k.level,
    (unsigned long long)k.stats.conflicts,(unsigned long long)k.stats.decisions);
  assert(h.finish_query(1,static_cast<QueryResult>(answer))==InputStatus::Ok);
  for(unsigned q=2;q<=4;++q) {
    if(q==2 && append)query_test::clause(h,{-3});
    if(q<=3 && !append)assert(h.assume(-3)==InputStatus::Ok);
    if(q==3)assert(h.assume(1)==InputStatus::Ok);
    assert(h.begin_query()==InputStatus::Ok);export_query(h,stem(q));
    const auto advanced=s.advance(h,writer(stem(q),mode));
    assert(advanced.status==NativeAdvanceStatus::Ok && s.kernel(s.ticket())==identity && k.h==guidance);
    assert(bytes(stem(1),mode)==sealed);
    if(q==2)assert(advanced.retracted>=2 && !kernel_external_value(k,internalize(6)));
    answer=q==3?20:10;assert(solve(s,h,schedule,policy)==answer);
    if(answer==10)export_model(h,k,stem(q));
    assert(s.flush_proof(s.ticket()));kernel_check_invariants(k);
    std::printf("q=%u result=%d retracted=%llu conflicts=%llu decisions=%llu frozen=1\n",q,answer,
      (unsigned long long)advanced.retracted,(unsigned long long)k.stats.conflicts,(unsigned long long)k.stats.decisions);
    assert(h.finish_query(q,static_cast<QueryResult>(answer))==InputStatus::Ok);
  }
}
int main(int argc,char **argv) {
  if(argc==6) {
    const std::string name=argv[1];
    if(name.starts_with("from-"))transition(name.substr(5),std::string(argv[2])=="append",argv[3],argv[4],argv[5]);
    else repair(name,std::string(argv[2])=="unsat",argv[3],argv[4],argv[5]);
  } else {
    assert(argc==1);
    for(const char *name:{"bve","condition","sr","rat","binding","alias-binding"})
      for(bool hard:{false,true})repair(name,hard,"","none","direct");
    for(const char *name:{"sat","unsat","unknown"})
      for(bool append:{false,true})transition(name,append,"","none","direct");
  }
}
