#undef NDEBUG
#include "query_test_support.hpp"
#include "session/core_certificate.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/substitute/substitute.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include <cstdio>
#include <string>
using namespace tactix;
using namespace tactix::session;
static const auto limits = query_test::large_limits;
using query_test::writer;
using query_test::export_query;
// The result view a client reads: failed membership and the signed model value.
static bool failed(const QueryKernel &s,int32_t lit) {
  const auto *r=s.result_snapshot();return r&&r->failed_literal(lit);
}
static int32_t model(const QueryKernel &s,int32_t lit) {
  const auto *r=s.result_snapshot();const auto v=uint32_t(lit<0?-lit:lit);
  if(!r||!v||r->result!=QueryResult::Sat||v>r->model.size()||!r->model[v-1])return 0;
  return r->model[v-1]>0?int32_t(v):-int32_t(v);
}
static int solve(QueryKernel &s,QueryHistory &h,const std::string &schedule,DefaultPolicy &p) {
  if(schedule!="default")return query_test::drive(s,h,schedule=="registry");
  auto r=p.solve_query(s);assert(r.status==QuerySolveStatus::Complete);return r.result;
}
static void core_proof(QueryHistory &h,QueryKernel &s,const std::string &stem,const std::string &mode) {
  if(stem.empty())return;
  const auto *r=s.result_snapshot();assert(r&&r->result==QueryResult::Unsat);
  FILE *f=std::fopen((stem+".core").c_str(),"w");assert(f);
  for(int32_t lit:r->core.assumptions)assert(std::fprintf(f,"%d ",lit)>0);
  assert(std::fputs("0\n",f)>=0&&!std::fclose(f));
  if(r->core.kind!=CoreKind::LicensedSubset || mode=="none")return;
  assert(write_core_certificate(h,*r,stem+"-core"));
}
static void scenario(const std::string &name,const std::string &prefix,const std::string &mode,const std::string &schedule) {
  QueryHistory h;bool subset=true;
  if(name=="clash") {for(int a:{7,1,-1,6})assert(h.assume(a)==InputStatus::Ok);}
  else if(name=="alias" || name=="negative-alias") {
    const int y=name=="alias"?2:-2;
    query_test::clause(h,{-1,y});query_test::clause(h,{1,-y});
    for(int a:{7,1,-y})assert(h.assume(a)==InputStatus::Ok);
  } else if(name=="large") {
    query_test::clause(h,{-1,-2,3});for(int a:{7,1,2,-3})assert(h.assume(a)==InputStatus::Ok);
  } else if(name=="root") {
    query_test::clause(h,{1});for(int a:{7,-1})assert(h.assume(a)==InputStatus::Ok);
  } else if(name=="base") {
    query_test::clause(h,{1});query_test::clause(h,{-1});assert(h.assume(7)==InputStatus::Ok);
  } else if(name=="learned") {
    for(auto c:{std::vector<int32_t>{-1,2,3},{-1,2,-3},{-1,-2,3},{-1,-2,-3}}) {
      for(int l:c)assert(h.add(l)==InputStatus::Ok);
      assert(h.add(0)==InputStatus::Ok);
    }
    for(int a:{7,1})assert(h.assume(a)==InputStatus::Ok);
    // Default's root transformations can hide the native RUP dependency chain.
    // Optional minimization must then keep the independently sound full cube.
    subset=schedule!="default";
  } else if(name=="restriction") {
    query_test::clause(h,{1,2});for(int a:{7,-1,-2})assert(h.assume(a)==InputStatus::Ok);subset=false;
  } else {
    assert(name=="binary" || name=="quota" || name=="disabled");
    query_test::clause(h,{-1,2});query_test::clause(h,{-2,3});
    for(int a:{7,1,-3,1})assert(h.assume(a)==InputStatus::Ok);
    subset=name=="binary";
  }
  assert(h.begin_query()==InputStatus::Ok);export_query(h,prefix);
  QueryKernel s(limits());assert(s.replay(h,writer(prefix,mode))==ReplayStatus::Ok);
  auto t=s.ticket();Kernel *identity=s.kernel(t);auto &k=*identity;auto *guidance=k.h;
  k.jump_reasons_bigbig=true;
  if(name=="alias"||name=="negative-alias")assert(substitute_apply(k)==ApplyResult::Ok);
  if(name=="restriction") {
    SubstitutionRedundancyInstance x;x.clause={kernel_internalize(k,1)};
    x.substitution={{kernel_internalize(k,1),kernel_internalize(k,2)},{kernel_internalize(k,2),kernel_internalize(k,1)}};
    assert(substitution_redundancy_apply_detailed(k,x).rule_result==ApplyResult::Ok);
    assert(!s.projection_state(t).restore);
  }
  DefaultPolicy policy;assert(solve(s,h,schedule,policy)==20);
  assert(!s.result_snapshot());
  CoreLimits core{limits().proof,16000000};
  if(name=="quota")core.proof.maximum_variables=1;
  if(name=="disabled")core.ticks=0;
  volatile sig_atomic_t stop=1;
  assert(s.finish_query(h,QueryResult::Unsat,core,&stop)==ResultCaptureStatus::Interrupted);
  assert(h.active_query()&&s.kernel(t)==identity&&!s.result_snapshot());
  assert(s.finish_query(h,QueryResult::Unsat,core)==ResultCaptureStatus::Ok);
  assert(h.state()==InputState::Unsat&&!s.kernel(t));
  const auto *r=s.result_snapshot();assert(r&&r->query==1&&r->generation==t.generation);
  assert((r->core.kind==CoreKind::LicensedSubset)==subset);
  for(int lit:r->core.assumptions)assert(failed(s,lit));
  assert(!failed(s,0)&&!failed(s,INT32_MIN)&&!failed(s,-7)&&!model(s,1));
  assert(failed(s,7)==!subset);
  if(name=="restriction")assert(failed(s,-1)&&failed(s,-2)&&r->core.traced);
  if(name=="base")assert(r->core.assumptions.empty());
  if(name=="quota")assert(r->core.exhausted);
  core_proof(h,s,prefix,mode);
  std::printf("core=%s size=%zu traced=%d exhausted=%d ticks=%llu conflicts=%llu decisions=%llu\n",
    subset?"licensed":"query",r->core.assumptions.size(),r->core.traced,r->core.exhausted,
    (unsigned long long)r->core.ticks,(unsigned long long)k.stats.conflicts,(unsigned long long)k.stats.decisions);
  assert(h.begin_query()==InputStatus::Ok&&!s.result_snapshot()&&!failed(s,7));
  assert(s.advance(h).status== (k.inconsistent?NativeAdvanceStatus::ReplayRequired:NativeAdvanceStatus::Ok));
  if(k.inconsistent)assert(s.replay(h)==ReplayStatus::Ok);
  (void)guidance;
}
static void lifecycle() {
  QueryHistory h,other;query_test::clause(h,{1,2});assert(h.assume(-1)==InputStatus::Ok&&h.begin_query()==InputStatus::Ok);
  QueryKernel s(limits());assert(s.replay(h)==ReplayStatus::Ok);auto t=s.ticket();Kernel *k=s.kernel(t);
  assert(s.finish_query(other,QueryResult::Sat)==ResultCaptureStatus::WrongHistory);
  assert(s.finish_query(h,static_cast<QueryResult>(12))==ResultCaptureStatus::InvalidResult);
  assert(s.finish_query(h,QueryResult::Unsat)==ResultCaptureStatus::NotReady);
  assert(query_test::drive(s,h,false)==10);
  assert(s.finish_query(h,QueryResult::Sat)==ResultCaptureStatus::Ok);
  assert(model(s,1)==-1&&model(s,-1)==-1&&model(s,2)==2&&model(s,-2)==2);
  const auto *result=s.result_snapshot();assert(result);
  k->model_values.clear();assert(model(s,2)==2&&s.result_snapshot()==result);
  assert(h.add(INT32_MIN)==InputStatus::InvalidLiteral&&s.result_snapshot()==result);
  assert(h.add(3)==InputStatus::Ok&&!s.result_snapshot()&&!model(s,2));
  assert(h.add(0)==InputStatus::Ok&&h.assume(-3)==InputStatus::Ok&&h.begin_query()==InputStatus::Ok);
  assert(s.advance(h).status==NativeAdvanceStatus::Ok&&s.kernel(s.ticket())==k);
  assert(query_test::drive(s,h,false)==20);
  assert(s.finish_query(h,QueryResult::Unsat)==ResultCaptureStatus::Ok&&failed(s,-3));
  assert(h.assume(3)==InputStatus::Ok&&!s.result_snapshot()&&!failed(s,-3));
  assert(h.begin_query()==InputStatus::Ok&&s.advance(h).status==NativeAdvanceStatus::Ok);
  assert(s.finish_query(h,QueryResult::Unknown)==ResultCaptureStatus::Ok&&h.state()==InputState::Input&&!s.result_snapshot());
  assert(h.pending_assumptions().empty());
}
int main(int argc,char **argv) {
  if(argc==5)scenario(argv[1],argv[2],argv[3],argv[4]);
  else {assert(argc==1);for(const char *name:{"binary","large","root","clash","alias","negative-alias","learned","base","restriction","quota","disabled"})scenario(name,"","none","direct");lifecycle();}
}
