#undef NDEBUG
#include "rules/lucky/parity_live.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include <cassert>
#include <cstdio>
#include <string>
using namespace tactix;
using namespace tactix::parity_certificate;
static void run(const std::string &dir,const char *name,std::vector<Equation> equations,unsigned variables,unsigned width,bool root_conflict,bool compact) {
  Kernel k;kernel_reserve_vars(k,variables);std::vector<SmallClause> inputs;
  for(const auto &e:equations)for(const auto &c:xor_equation(e.variables.data(),e.size,e.rhs)) {
    inputs.push_back(c);Lit lits[4];for(unsigned i=0;i<c.size;++i)lits[i]=internalize(c.lits[i]);
    if(c.size==2)kernel_add_binary_clause(k,lits[0],lits[1],++k.next_clause_id);
    else kernel_add_large_clause(k,lits,c.size,false,0,++k.next_clause_id);
  }
  if(root_conflict) {
    SmallClause c;c.size=1;c.lits[0]=1;inputs.push_back(c);k.unit_ids[0]=++k.next_clause_id;kernel_assign_unit(k,internalize(1));assert(propagate_service(k)==ApplyResult::Ok);
  }
  if(compact) {k.var_status.assign(k.nvars,VarStatus::Active);k.var_status[0]=VarStatus::Eliminated;assert(kernel_compact_variables(k));}
  const auto base=dir+"/"+(dir=="/tmp"?"tactix-parity-boundary-":"")+name;
  FILE *cnf=std::fopen((base+".cnf").c_str(),"w");assert(cnf);std::fprintf(cnf,"p cnf %u %zu\n",variables,inputs.size());
  for(const auto &c:inputs) {for(unsigned i=0;i<c.size;++i)std::fprintf(cnf,"%d ",c.lits[i]);std::fprintf(cnf,"0\n");}std::fclose(cnf);
  Proof proof;assert(proof.open((base+".dsr").c_str(),ProofFormat::DsrAscii));proof.bind_variable_map(&k.internal_to_external);proof.bind_external_variable_map(&k.external_to_internal);k.proof=&proof;
  FILE *trace=std::fopen((base+".jsonl").c_str(),"w");assert(trace);
  ParityLiveRequest request{{100,100},{10000,32,32,width},true,100000,100,0,1,trace};
  auto limited=request;limited.maximum_tree_variables=1;
  auto map=k.external_to_internal;const auto id=k.next_clause_id;
  assert(parity_live_apply(k,limited).result==ApplyResult::NoOp);assert(k.external_to_internal==map&&k.next_clause_id==id);
  request.action_id=2;const auto result=parity_live_apply(k,request);
  if(root_conflict)assert(result.result==ApplyResult::Unsat&&k.inconsistent);
  else {
    assert(result.result==ApplyResult::Ok&&!k.inconsistent);
    if(width==1)assert(k.value(kernel_internalize(k,compact?-2:-1))==1);
    if(width>=3)assert(result.published==(1u<<(width-1)));
    map=k.external_to_internal;request.action_id=3;
    assert(parity_live_apply(k,request).result==ApplyResult::NoOp);assert(k.external_to_internal==map);
  }
  proof.flush();k.proof=nullptr;std::fclose(trace);
  std::printf("%s: result %u, published %llu, helpers %u\n",name,unsigned(result.result),(unsigned long long)result.published,result.helpers);
}
int main(int argc,char **argv) {
  const std::string dir=argc>1?argv[1]:"/tmp";
  run(dir,"ternary",{{{1,2,4,0},3,false},{{3,4,0,0},2,false}},4,3,false,false);
  run(dir,"quaternary",{{{1,2,5,0},3,false},{{3,4,5,0},3,false}},5,4,false,false);
  run(dir,"unit",{{{1,2,3,0},3,false},{{2,3,0,0},2,false}},3,1,false,false);
  run(dir,"root-conflict",{{{1,2,3,0},3,false},{{2,3,0,0},2,false}},3,1,true,false);
  run(dir,"compacted-unit",{{{2,3,4,0},3,false},{{3,4,0,0},2,false}},4,1,false,true);
  std::puts("test_parity_live_boundaries: all passed");
}
