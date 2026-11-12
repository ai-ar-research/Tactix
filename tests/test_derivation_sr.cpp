#undef NDEBUG
#include "query_test_support.hpp"
#include "proof/proof.hpp"
#include "temporary_file.hpp"
#include "rules/bce/bce.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include <cstdio>
#include <string>
using namespace tactix;using namespace tactix::session;
static void fixture(const std::string &prefix,const std::string &mode,bool needed){
 QueryHistory h;
 const auto formula=needed ? std::vector<std::vector<int>>{{1,2,3},{5,6},{-5,6},{5,-6},{-5,-6}} : std::vector<std::vector<int>>{{1,2,3},{1,2,-3},{1,-2,4},{1,-2,-4},{5,6},{-5,6},{5,-6},{-5,-6}};
 for(const auto &c:formula){for(int l:c)assert(h.add(l)==InputStatus::Ok);assert(h.add(0)==InputStatus::Ok);}
 assert(h.begin_query()==InputStatus::Ok);QueryKernel s(NativeQueryLimits{{256,4096,1024},{64,4096,1024,8192,64,1024,8192},1000000});assert(s.replay(h)==ReplayStatus::Ok);Kernel&k=*s.kernel(s.ticket());
 if(needed){
  Lit u=internalize(5);auto ui=++k.next_clause_id;k.proof->add_derived_unit(ui,u);
  Lit c[]={internalize(1),internalize(2)};auto ci=++k.next_clause_id;k.proof->add_rup(ci,c,2);kernel_add_binary_clause(k,c[0],c[1],ci);
  k.proof->del(++k.next_clause_id,ui,&u,1);
 }else{
  Lit a[]={internalize(1),internalize(2)},b[]={internalize(1),internalize(-2)},u=internalize(1);
  auto ai=++k.next_clause_id;k.proof->add_rup(ai,a,2);auto bi=++k.next_clause_id;k.proof->add_rup(bi,b,2);auto ui=++k.next_clause_id;k.proof->add_derived_unit(ui,u);
  k.proof->del(++k.next_clause_id,ui,&u,1);k.proof->del(++k.next_clause_id,bi,b,2);k.proof->del(++k.next_clause_id,ai,a,2);
  assert(bce_try(k,internalize(1))==ApplyResult::Ok&&k.stats.bce_deleted==4&&!kernel_var_retired(k,0));
 }
 assert(h.finish_query(1,QueryResult::Unknown)==InputStatus::Ok);
 if(needed) assert(h.assume(-1)==InputStatus::Ok&&h.assume(-2)==InputStatus::Ok);
 else assert(h.assume(7)==InputStatus::Ok);
 assert(h.begin_query()==InputStatus::Ok);
 query_test::export_query(h, prefix);
 auto p=query_test::writer(prefix, mode);
 assert(s.advance(h,std::move(p)).status==NativeAdvanceStatus::Ok);
 if(!needed){SubstitutionRedundancyInstance sr;sr.clause={internalize(-1)};sr.witness=sr.clause;assert(substitution_redundancy_apply_detailed(k,sr).rule_result==ApplyResult::Ok);}
 assert(query_test::drive(s,h,false)==20);assert(s.flush_proof(s.ticket()));
 std::printf("result=20 conflicts=%llu decisions=%llu ids=%llu rho=%zu\n",(unsigned long long)k.stats.conflicts,(unsigned long long)k.stats.decisions,(unsigned long long)k.next_clause_id,k.reconstruction.size());
}

int main(int argc,char**argv){
 if(argc==4){fixture(argv[2],argv[3],std::string(argv[1])=="needed");return 0;}
 assert(argc==1);for(bool needed:{false,true}){TemporaryFile temp;fixture(temp.path,"none",needed);assert(std::remove((std::string(temp.path)+".cnf").c_str())==0);}
}
