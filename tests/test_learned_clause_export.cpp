#undef NDEBUG
#include "query_test_support.hpp"
#include "session/learned_clause_export.hpp"
#include "proof/proof.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include <cstdio>
#include <string>
using namespace tactix;
using namespace tactix::session;
static NativeQueryLimits limits() {return {{4096,65536,4096},{128,65536,4096,65536,128,4096,65536},16000000};}
struct Collected {
  std::vector<std::vector<int>> clauses;
  static void learn(void *state,int *body) {
    auto &out=*static_cast<Collected *>(state);std::vector<int> copy;
    for(unsigned i=0;;++i) {assert(i<129);if(!body[i])break;copy.push_back(body[i]);}
    out.clauses.push_back(std::move(copy));
    body[0]=INT32_MAX; // no alias into native/proof/oracle storage
  }
};
static void reject_query_and_sr_facts() {
  QueryHistory h;query_test::clause(h,{1,2});assert(h.assume(1)==InputStatus::Ok&&h.begin_query()==InputStatus::Ok);
  QueryKernel q(limits());Collected got;q.set_learn(&got,16,Collected::learn);assert(q.replay(h)==ReplayStatus::Ok);
  auto t=q.ticket();auto &k=*q.kernel(t);
  // A is a checker premise, so this proof event is legal for F+A. It is not
  // licensed by F and must never become an IPASIR learned unit.
  const Lit one=kernel_internalize(k,1);k.proof->add_rup(++k.next_clause_id,&one,1);
  q.drain_learned(t);assert(got.clauses.empty()&&q.learning_snapshot().rejected);
  SubstitutionRedundancyInstance sr;sr.clause={one};sr.witness=sr.clause;
  assert(substitution_redundancy_apply_detailed(k,sr).rule_result==ApplyResult::Ok);
  q.drain_learned(t);assert(got.clauses.empty());
  // Even when a prefix is inconsistent, helper names may not escape.
  DerivationRecord private_name;private_name.query=1;private_name.clause={77};
  LearnedClauseExport separate(limits().proof);separate.set(&got,100,Collected::learn);
  separate.drain(h,1,1,{&private_name,1},0);assert(got.clauses.empty()&&separate.snapshot().rejected==1);
}
static void parity_and_budget() {
  QueryHistory h;const std::vector<PbpParityEquation> rows{{{1,2,3},false},{{1,2,4},false}};
  for(const auto &row:rows)assert(visit_parity_source(row,[&](const auto &body) {for(int l:body)assert(h.add(l)==InputStatus::Ok);return h.add(0)==InputStatus::Ok;}));
  assert(h.begin_query()==InputStatus::Ok);
  DerivationRecord record;record.query=1;record.kind=DerivationKind::ParityReason;record.parity_equations=rows;record.clause={-3,4};
  Collected got;LearnedClauseExport service(limits().proof);service.set(&got,2,Collected::learn);
  service.drain(h,1,1,{&record,1},0);assert((got.clauses==std::vector<std::vector<int>>{{-3,4}}));
  assert(service.snapshot().licensed==1&&service.snapshot().exported==1&&service.snapshot().ticks);
  service.drain(h,1,1,{&record,1},0);assert(got.clauses.size()==1); // cursor, no duplicate callback
  record.parity_equations[1].rhs=true;record.clause={3,4};
  LearnedClauseExport wrong(limits().proof);wrong.set(&got,2,Collected::learn);wrong.drain(h,1,2,{&record,1},0);
  assert(wrong.snapshot().rejected==1&&got.clauses.size()==1);
  record.parity_equations=rows;record.clause={-3,4};
  LearnedClauseExport bounded(limits().proof);bounded.set(&got,2,Collected::learn);bounded.set_budget(0);
  bounded.drain(h,1,1,{&record,1},0);assert(bounded.snapshot().incomplete&&!bounded.snapshot().ticks);
  bounded.set_budget(UINT64_MAX);bounded.drain(h,1,1,{&record,1},0);assert(!bounded.snapshot().exported);
  assert(h.finish_query(1,QueryResult::Unknown)==InputStatus::Ok&&h.begin_query()==InputStatus::Ok);record.query=2;
  bounded.drain(h,2,2,{&record,1},0);assert(bounded.snapshot().exported==1&&got.clauses.size()==2);
  LearnedClauseExport stopped(limits().proof);stopped.set(&got,2,Collected::learn);volatile sig_atomic_t stop=1;
  stopped.drain(h,2,1,{&record,1},0,&stop);assert(stopped.snapshot().interrupted&&!stopped.snapshot().ticks);
}
static void fixture(const std::string &prefix,const std::string &mode,bool connected,bool restriction=false) {
  QueryHistory h;query_test::clause(h,{1,2});query_test::clause(h,{1,-2});
  assert(h.assume(-1)==InputStatus::Ok&&h.begin_query()==InputStatus::Ok);
  query_test::export_query(h, prefix);
  auto writer = query_test::writer(prefix, mode);
  QueryKernel q(limits());Collected got;if(connected)q.set_learn(&got,1,Collected::learn);
  unsigned polls=0;q.set_terminate(&polls,[](void *state) {++*static_cast<unsigned *>(state);return 0;});
  assert(q.replay(h,std::move(writer))==ReplayStatus::Ok);const auto t=q.ticket();auto &k=*q.kernel(t);
  if(restriction) {
    SubstitutionRedundancyInstance sr;sr.clause={kernel_internalize(k,2)};sr.witness=sr.clause;
    assert(substitution_redundancy_apply_detailed(k,sr).rule_result==ApplyResult::Ok);
    assert(propagate_service(k)==ApplyResult::Ok);
  } else {
    assert(decide_apply(k,kernel_internalize(k,-1),false)==ApplyResult::Ok);
    assert(propagate_service(k)==ApplyResult::Conflict);
  }
  assert(query_test::drive(q,h,false)==20);q.drain_learned(t);
  assert(q.finish_query(h,QueryResult::Unsat,CoreLimits{})==ResultCaptureStatus::Ok);
  if(connected)assert((got.clauses==std::vector<std::vector<int>>{{1}}));else assert(got.clauses.empty());
  if(!prefix.empty()) {FILE *f=std::fopen((prefix+".learned").c_str(),"w");assert(f);
    for(const auto &body:got.clauses) {for(int l:body)std::fprintf(f,"%d ",l);std::fputs("0\n",f);}assert(!std::fclose(f));}
  std::printf("conflicts=%llu decisions=%llu roots=%llu polls=%u\n",(unsigned long long)k.stats.conflicts,(unsigned long long)k.stats.decisions,(unsigned long long)k.stats.root_assignments,polls);
}
int main(int argc,char **argv) {
  if(argc==4||argc==5)fixture(argv[1],argv[2],std::string(argv[3])=="1",argc==5&&std::string(argv[4])=="sr");
  else {assert(argc==1);reject_query_and_sr_facts();parity_and_budget();fixture("","none",true);fixture("","none",false);fixture("","none",true,true);}
}
