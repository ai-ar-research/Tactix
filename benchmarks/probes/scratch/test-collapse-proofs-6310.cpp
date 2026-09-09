// Exact frontend certificates plus exhaustive reconstruction after actual V3.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "rules/substitute/substitute.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
using namespace tactix;
static unsigned models() {
  unsigned checked=0;
  for(unsigned mask=0;mask<8;++mask) {
    if(!(mask&1) && !(mask&4))continue;
    Kernel k;kernel_reserve_vars(k,3);
    kernel_add_binary_clause(k,internalize(-1),internalize(2));
    kernel_add_binary_clause(k,internalize(1),internalize(-2));
    const Lit lits[]={internalize(1),internalize(2),internalize(3)};
    kernel_add_large_clause(k,lits,3,false,0);assert(substitute_apply(k)==ApplyResult::Ok);
    for(Var v=0;v<3;++v) {
      k.values[make_lit(v,false)]=(mask&(1u<<v))?1:-1;
      k.values[make_lit(v,true)]=-k.values[make_lit(v,false)];
    }
    kernel_extend_model(k);
    assert(k.value(internalize(1))==k.value(internalize(2)));
    assert(k.value(internalize(1))>0 || k.value(internalize(2))>0 || k.value(internalize(3))>0);
    ++checked;
  }
  return checked;
}
int main(int argc,char **argv) {
  assert(argc==6);const unsigned copies=std::atoi(argv[1]);const bool lrat=std::atoi(argv[2]),explicit_action=std::atoi(argv[3]);
  const auto checked=models();
  std::vector<std::vector<int>> clauses={{-1,2},{1,-2}};
  for(unsigned i=0;i<copies;++i)clauses.push_back({1,2,3});
  for(int p=0;p<4;++p)clauses.push_back({4+3*p,5+3*p,6+3*p});
  for(int h=0;h<3;++h)for(int a=0;a<4;++a)for(int b=a+1;b<4;++b)clauses.push_back({-(4+3*a+h),-(4+3*b+h)});
  FILE *cnf=std::fopen(argv[4],"wx");assert(cnf);std::fprintf(cnf,"p cnf 15 %zu\n",clauses.size());
  for(const auto &row:clauses){for(int lit:row)std::fprintf(cnf,"%d ",lit);std::fputs("0\n",cnf);}std::fclose(cnf);
  Kernel k;Proof proof;assert(proof.open(argv[5],lrat?ProofFormat::Lrat:ProofFormat::DratAscii));k.proof=&proof;
  FILE *input=std::fopen(argv[4],"r");assert(input);assert(!parse_dimacs(input,k));std::fclose(input);
  std::fill(k.bve_retry.begin(),k.bve_retry.end(),0);
  const auto &rule=tactix_rules()[tactix_rule_id("Substitute")];uint64_t payload[8];assert(rule.guard(k)&&rule.instances(k,payload,8)>0);
  assert((explicit_action?rule.apply_instance(k,payload[0]):rule.apply(k))==ApplyResult::Ok);
  assert(k.bve_retry[2] && kernel_binary_live(k,internalize(1),internalize(3)));
  assert(default_solve(k)==20);k.proof=nullptr;proof.flush();
  std::printf("PASS: %u exhaustive reconstructed images and original-CNF refutation\n",checked);
}
