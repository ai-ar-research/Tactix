#undef NDEBUG
#include "query_test_support.hpp"
#include "proof/proof.hpp"
#include "policies/heuristics.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include <cstdio>
#include <string>
using namespace tactix;
using namespace tactix::session;
static const auto limits = query_test::large_limits;
static void fixture(bool rat,bool hard,const std::string &prefix,const std::string &mode,bool dynamic) {
  QueryHistory h;
  for(int base:rat?std::vector<int>{0,3}:std::vector<int>{0,4}) {
    if(rat) {
      query_test::clause(h,{base+1,base+2});query_test::clause(h,{-base-1,base+2});
      query_test::clause(h,{-base-2,base+3});
    } else {
      query_test::clause(h,{base+1,base+2});query_test::clause(h,{-base-1,-base-2});
      query_test::clause(h,{-base-1,base+3});query_test::clause(h,{-base-2,base+4});
    }
  }
  if(hard) for(auto c:{std::vector<int32_t>{9,10},{-9,10},{9,-10},{-9,-10}}) {
    for(int32_t l:c)assert(h.add(l)==InputStatus::Ok);
    assert(h.add(0)==InputStatus::Ok);
  }
  QueryKernel s(limits());Kernel *identity=nullptr;Heuristics *guidance=nullptr;
  for(unsigned q=1;q<=(hard?2u:4u);++q) {
    if(q==2) {assert(h.assume(rat?3:4)==InputStatus::Ok);assert(h.assume(rat?6:8)==InputStatus::Ok);}
    if(q==3) {query_test::clause(h,{11});assert(h.assume(-11)==InputStatus::Ok);}
    if(q==4) assert(h.assume(11)==InputStatus::Ok);
    assert(h.begin_query()==InputStatus::Ok);
    std::string stem=prefix.empty()?"":prefix+"-"+std::to_string(q);
    query_test::export_query(h, stem);
    auto proof = query_test::writer(stem, mode);
    if(q==1) {
      assert(s.replay(h,std::move(proof))==ReplayStatus::Ok);identity=s.kernel(s.ticket());guidance=identity->h;
      for(int base:rat?std::vector<int>{0,3}:std::vector<int>{0,4}) {
        if(rat) {
          const Lit x=internalize(base+1);const uint64_t id=++identity->next_clause_id;
          identity->proof->add_rat(id,&x,1);identity->unit_ids[var_of(x)]=id;
          kernel_assign_unit(*identity,x);heuristics_deactivate_var(*identity,var_of(x));
        } else {
          SubstitutionRedundancyInstance sr;sr.clause={internalize(base+1)};
          sr.witness={internalize(-base-2),internalize(base+3)};
          assert(substitution_redundancy_apply_detailed(*identity,sr).rule_result==ApplyResult::Ok);
        }
        propagate_service(*identity);
      }
      const int32_t x[]{1};RetainedClauses permanent(limits().proof);
      assert(permanent.admit(h,1,x,1000000).status==RetentionStatus::NotRup);
      assert(h.finish_query(1,QueryResult::Unknown)==InputStatus::Ok);continue;
    }
    const auto advanced=s.advance(h,std::move(proof));
    assert(advanced.status==NativeAdvanceStatus::Ok && s.kernel(s.ticket())==identity && identity->h==guidance);
    assert(identity->proof->certificate_offset()==0); // clausal steps compose without copying F
    if(!rat && q==2) {
      SubstitutionRedundancyInstance sr;sr.clause={kernel_internalize(*identity,4),kernel_internalize(*identity,8)};
      assert(substitution_redundancy_apply_detailed(*identity,sr).rule_result==ApplyResult::Ok);
    }
    const int answer=hard||q==3?20:10;
    assert(query_test::drive(s,h,dynamic)==answer);
    if (answer==10) query_test::export_model(h, *identity, stem);
    assert(s.flush_proof(s.ticket()));
    std::printf("q=%u result=%d base=%d ids=%llu conflicts=%llu decisions=%llu\n",q,answer,identity->inconsistent,
      (unsigned long long)identity->next_clause_id,(unsigned long long)identity->stats.conflicts,(unsigned long long)identity->stats.decisions);
    assert(h.finish_query(q,answer==10?QueryResult::Sat:QueryResult::Unsat)==InputStatus::Ok);
  }
}
static void invalidated(bool append,const std::string &prefix,const std::string &mode,bool dynamic) {
  QueryHistory h;query_test::clause(h,{1,2});query_test::clause(h,{-1,-2});
  query_test::clause(h,{-1,3});query_test::clause(h,{-2,4});
  assert(h.begin_query()==InputStatus::Ok);QueryKernel s(limits());assert(s.replay(h)==ReplayStatus::Ok);
  Kernel *old=s.kernel(s.ticket());Heuristics *guidance=old->h;
  SubstitutionRedundancyInstance sr;sr.clause={internalize(1)};sr.witness={internalize(-2),internalize(3)};
  assert(substitution_redundancy_apply_detailed(*old,sr).rule_result==ApplyResult::Ok);
  propagate_service(*old);
  assert(old->value(internalize(3))>0 && h.finish_query(1,QueryResult::Unknown)==InputStatus::Ok);
  if(append)query_test::clause(h,{-3});else assert(h.assume(-3)==InputStatus::Ok);
  assert(h.begin_query()==InputStatus::Ok);
  const auto values=old->values;const auto rho=old->reconstruction;const auto ids=old->next_clause_id;
  volatile sig_atomic_t stopped=1;
  const auto interrupted=s.advance(h,{},&stopped);
  assert(interrupted.status==NativeAdvanceStatus::Interrupted);
  // A rejected preflight leaves the old native state intact, without a valid
  // result ticket for the new query.
  assert(!s.kernel(s.ticket()));
  assert(old->values==values && old->reconstruction==rho && old->next_clause_id==ids);
  // Force exhaustion after restriction/fact validation but before committing
  // the prepared native repair. Removing the artificial limit permits retry.
  old->next_clause_id=UINT64_MAX-8;
  assert(s.advance(h).status==NativeAdvanceStatus::Exhausted);
  assert(old->values==values && old->reconstruction==rho && old->next_clause_id==UINT64_MAX-8);
  old->next_clause_id=ids;
  query_test::export_query(h, prefix);
  auto proof = query_test::writer(prefix, mode);
  const auto advanced=s.advance(h,std::move(proof));
  assert(advanced.status==NativeAdvanceStatus::Ok && advanced.retracted>=2);
  assert(s.kernel(s.ticket())==old && old->h==guidance);
  assert(query_test::drive(s,h,dynamic)==10);
  Kernel &k=*s.kernel(s.ticket());assert(k.model_values[internalize(3)]<0);
  query_test::export_model(h, k, prefix);
  assert(s.flush_proof(s.ticket())&&h.finish_query(2,QueryResult::Sat)==InputStatus::Ok);
  kernel_check_invariants(k);
  std::printf("recovery=native result=10 p=false conflicts=%llu decisions=%llu\n",
      (unsigned long long)k.stats.conflicts,(unsigned long long)k.stats.decisions);
}
int main(int argc,char **argv) {
  if(argc==5) {
    const std::string c=argv[1];const bool dynamic=std::string(argv[4])=="dynamic";
    if(c=="assumption"||c=="append")invalidated(c=="append",argv[2],argv[3],dynamic);
    else fixture(c.starts_with("rat"),c.ends_with("unsat"),argv[2],argv[3],dynamic);
    return 0;
  }
  assert(argc==1);
  for(bool rat:{false,true})for(bool hard:{false,true})fixture(rat,hard,"","none",false);
  for(bool append:{false,true})invalidated(append,"","none",false);
}
