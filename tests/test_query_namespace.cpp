#undef NDEBUG
#include "query_test_support.hpp"
#include "temporary_file.hpp"
#include "proof/proof.hpp"
#include "rules/bce/bce.hpp"
#include "rules/factor/factor.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include <cstdio>
#include <string>
using namespace tactix;
using namespace tactix::session;

static void run(const std::string &which, const std::string &prefix, const std::string &mode) {
  QueryHistory h;
  query_test::clause(h, {1,2});
  if (which=="fresh")
    for (int32_t start : {7,11})
      for (int32_t f : {start,start+1})
        for (int32_t r : {start+2,start+3}) query_test::clause(h,{f,r});
  if (which=="repeat") query_test::clause(h, {3,4});
  for (auto clause : {std::vector<int32_t>{5,6},{-5,6},{5,-6},{-5,-6}}) {
    for (int32_t literal : clause) assert(h.add(literal)==InputStatus::Ok);
    assert(h.add(0)==InputStatus::Ok);
  }
  if (which=="replay") assert(h.assume(-2)==InputStatus::Ok);
  assert(h.begin_query()==InputStatus::Ok);
  QueryKernel session(NativeQueryLimits{{256,4096,1024},{64,4096,1024,8192,64,1024,8192},1000000});
  const auto writer = [&]() {
    std::unique_ptr<Proof> p;
    if (mode!="none") {
      p=std::make_unique<Proof>();
      if (mode.starts_with("auto")) assert(p->open_automatic((prefix+".proof").c_str(), false));
      else assert(p->open((prefix+".proof").c_str(), mode=="pbp" ? ProofFormat::PbpStreaming : ProofFormat::DsrAscii));
    }
    return p;
  };
  assert(session.replay(h, which=="replay" ? writer() : nullptr)==ReplayStatus::Ok);
  Kernel &k=*session.kernel(session.ticket());
  Heuristics *const guidance=k.h;
  if (which=="cross") assert(bce_try(k, internalize(1))==ApplyResult::Ok && k.stats.bce_deleted==1);
  if (which!="replay") {
    assert(h.finish_query(1,QueryResult::Unknown)==InputStatus::Ok);
    assert(h.assume(-2)==InputStatus::Ok);
    if (which=="repeat") assert(h.assume(-4)==InputStatus::Ok);
    assert(h.begin_query()==InputStatus::Ok);
    assert(session.advance(h, writer()).status==NativeAdvanceStatus::Ok);
    assert(&k==session.kernel(session.ticket()) && k.h==guidance);
    assert(k.proof->certificate_offset()==0); // RUP-only entry needs no copies
  }
  query_test::export_query(h, prefix);
  if (which!="cross") assert(bce_try(k, internalize(1))==ApplyResult::Ok && k.stats.bce_deleted==1);
  if (which=="fresh") {
    for (int32_t start : {7,11}) {
      FactorGrid grid;
      grid.factors={kernel_internalize(k,start),kernel_internalize(k,start+1)};
      grid.remainders={{kernel_internalize(k,start+2)},{kernel_internalize(k,start+3)}};
      assert(factor_apply_grid(k,grid)==ApplyResult::Ok);
      assert(!k.proof->query_certificate_state().generations);
      assert(k.proof->certificate_offset()==0); // either RAT polarity stays beyond stale prefix
    }
    assert(k.stats.factorings==2);
  }
  SubstitutionRedundancyInstance sr;
  sr.clause={kernel_internalize(k,-1)}; sr.witness=sr.clause;
  assert(substitution_redundancy_apply_detailed(k,sr).rule_result==ApplyResult::Ok);
  assert(k.proof->certificate_offset()>0); // first witness isolates live sources
  if (which=="repeat") {
    assert(bce_try(k,kernel_internalize(k,3))==ApplyResult::Ok && k.stats.bce_deleted==2);
    sr.clause={kernel_internalize(k,-3)}; sr.witness=sr.clause;
    assert(substitution_redundancy_apply_detailed(k,sr).rule_result==ApplyResult::Ok);
    assert(k.proof->query_certificate_state().generations>=2);
  } else if (which!="cross") assert(k.proof->query_certificate_state().generations>=1);
  assert(query_test::drive(session,h,false)==20);
  assert(session.flush_proof(session.ticket()));
  std::printf("result=20 bce=%llu decisions=%llu conflicts=%llu rho=%zu generations=%llu work=%llu\n",
      (unsigned long long)k.stats.bce_deleted,(unsigned long long)k.stats.decisions,
      (unsigned long long)k.stats.conflicts,k.reconstruction.size(),
      (unsigned long long)k.proof->query_certificate_state().generations,
      (unsigned long long)k.proof->query_certificate_state().work);
}

int main(int argc, char **argv) {
  if (argc==4) { run(argv[1],argv[2],argv[3]); return 0; }
  assert(argc==1);
  for (const std::string which : {"cross","within","repeat","replay","fresh"}) {
    TemporaryFile file; run(which,file.path,"none");
    assert(std::remove((file.path+".cnf").c_str())==0);
  }
}
