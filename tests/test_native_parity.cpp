#undef NDEBUG
#include "query_test_support.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/affine_image.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include <cstdio>
#include <string>
using namespace tactix;
using namespace tactix::session;

static NativeQueryLimits limits() {
  return {{1024,32768,2048},{64,8192,2048,32768,128,2048,32768},4000000};
}
static void fixture(bool hard,const std::string &prefix,const std::string &mode,bool dynamic) {
  QueryHistory history;
  const std::vector<PbpParityEquation> equations{{{1,2,3},false},{{1,2,4},false}};
  for(const auto &equation:equations)
    for(const auto &clause:affine_semantic_extension::clausal_form(equation.variables,equation.rhs)) {
      for(int32_t literal:clause) assert(history.add(literal)==InputStatus::Ok);
      assert(history.add(0)==InputStatus::Ok);
    }
  if(hard) for(const auto &clause:{std::vector<int32_t>{5,6},{-5,6},{5,-6},{-5,-6}}) {
    for(int32_t literal:clause) assert(history.add(literal)==InputStatus::Ok);
    assert(history.add(0)==InputStatus::Ok);
  }
  QueryKernel session(limits()); Kernel *identity=nullptr; Heuristics *guidance=nullptr;
  for(unsigned query=1;query<=(hard?3u:4u);++query) {
    if(query==2) assert(history.assume(3)==InputStatus::Ok && history.assume(-4)==InputStatus::Ok);
    if(query==3) assert(history.assume(-3)==InputStatus::Ok && history.assume(-4)==InputStatus::Ok);
    if(query==4) assert(history.assume(3)==InputStatus::Ok && history.assume(4)==InputStatus::Ok);
    assert(history.begin_query()==InputStatus::Ok);
    const std::string stem=prefix.empty()?"":prefix+"-"+std::to_string(query);
    std::unique_ptr<Proof> proof;
    if(!stem.empty()) {
      FILE *cnf=std::fopen((stem+".cnf").c_str(),"w");assert(cnf && history.write_query_dimacs(query,cnf) && !std::fclose(cnf));
      if(mode!="none") {
        proof=std::make_unique<Proof>();
        if(mode=="auto") assert(proof->open_automatic((stem+".proof").c_str(),false));
        else assert(proof->open((stem+".proof").c_str(),mode=="pbp"?ProofFormat::PbpStreaming:ProofFormat::DsrAscii));
      }
    }
    if(query==1) {
      assert(session.replay(history,std::move(proof))==ReplayStatus::Ok);
      identity=session.kernel(session.ticket());guidance=identity->h;
      const int32_t reason[]{-3,4};
      RetainedClauses rup(limits().proof);
      assert(rup.admit(history,1,reason,1000000).status==RetentionStatus::NotRup);
      assert(identity->proof->commit_clause_stream());
      PbpParityBoundsCache cache;PbpParityReasonPlan plan;
      assert(identity->proof->plan_pbp_parity_reason(equations,reason,2,identity->external_to_internal.size(),cache,plan));
      const uint64_t id=++identity->next_clause_id;
      assert(identity->proof->add_prevalidated_pbp_parity_reason(id,equations,reason,2,cache,plan));
      kernel_add_binary_clause(*identity,kernel_internalize(*identity,-3),kernel_internalize(*identity,4),id);
      assert(history.finish_query(1,QueryResult::Unknown)==InputStatus::Ok);
      continue;
    }
    const auto advanced=session.advance(history,std::move(proof));
    if(advanced.status!=NativeAdvanceStatus::Ok) std::fprintf(stderr,"native parity q%u status=%u\n",query,unsigned(advanced.status));
    assert(advanced.status==NativeAdvanceStatus::Ok);
    assert(identity==session.kernel(session.ticket()) && guidance==identity->h);
    if(query==3) {
      SubstitutionRedundancyInstance sr;sr.clause={kernel_internalize(*identity,1),kernel_internalize(*identity,2)};
      sr.substitution={{kernel_internalize(*identity,1),kernel_internalize(*identity,-1)},
                       {kernel_internalize(*identity,2),kernel_internalize(*identity,-2)}};
      assert(substitution_redundancy_apply_detailed(*identity,sr).rule_result==ApplyResult::Ok);
      assert(!session.projection_state(session.ticket()).restore);
    }
    const int expected=query==2 || hard ? 20:10;
    assert(query_test::drive(session,history,dynamic)==expected);
    if(expected==10 && !stem.empty()) {
      FILE *model=std::fopen((stem+".model").c_str(),"w");assert(model);
      for(uint32_t v=1;v<=history.active_query()->query_variables;++v)
        assert(std::fprintf(model,"%d ",identity->model_values[internalize(v)]>0?int(v):-int(v))>0);
      assert(std::fputs("0\n",model)>=0 && !std::fclose(model));
    }
    assert(session.flush_proof(session.ticket()));
    std::printf("q=%u result=%d base=%d ids=%llu conflicts=%llu decisions=%llu\n",query,expected,identity->inconsistent,
        (unsigned long long)identity->next_clause_id,(unsigned long long)identity->stats.conflicts,(unsigned long long)identity->stats.decisions);
    assert(history.finish_query(query,expected==10?QueryResult::Sat:QueryResult::Unsat)==InputStatus::Ok);
  }
}
int main(int argc,char **argv) {
  if(argc==5) {fixture(std::string(argv[1])=="unsat",argv[2],argv[3],std::string(argv[4])=="dynamic");return 0;}
  assert(argc==1);for(bool hard:{false,true})fixture(hard,"","none",false);
}
