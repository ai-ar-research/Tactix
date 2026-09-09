// Fixed tiny formulas only: no benchmark or random-policy execution.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "policies/default_policy.hpp"
#include "rules/substitute/substitute.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace tactix;

static void late_fixed_endpoint() {
  Kernel k; kernel_reserve_vars(k, 2);
  for (const auto &pair : std::vector<std::pair<int,int>>{{-1,2},{1,-2},{1,2}})
    kernel_add_binary_clause(k, internalize(pair.first), internalize(pair.second));
  std::fill(k.bve_retry.begin(),k.bve_retry.end(),0);
  const auto before=k.stats.bve_marks;
  assert(substitute_apply_equivalences(k, {{internalize(1),internalize(2)}})==ApplyResult::Ok);
  assert(k.value(internalize(1))>0 && k.value(internalize(2))==0);
  assert(!k.bve_retry[0] && k.bve_retry[1]);
  assert(k.stats.bve_marks-before==3);
}

static void reconstruction() {
  unsigned checked=0;
  for (unsigned mask=0;mask<16;++mask) {
    Kernel k; kernel_reserve_vars(k,4);
    const std::vector<std::pair<int,int>> original={{-1,2},{1,-2},{2,3},{3,4}};
    for (const auto &[a,b]:original) kernel_add_binary_clause(k,internalize(a),internalize(b));
    assert(substitute_apply(k)==ApplyResult::Ok);
    for (Var v=0;v<4;++v) {
      k.values[make_lit(v,false)]=(mask&(1u<<v))?1:-1;
      k.values[make_lit(v,true)]=-k.values[make_lit(v,false)];
    }
    if (k.value(internalize(1))<0 && k.value(internalize(3))<0) continue;
    if (k.value(internalize(3))<0 && k.value(internalize(4))<0) continue;
    kernel_extend_model(k);
    for (const auto &[a,b]:original) assert(k.value(internalize(a))>0 || k.value(internalize(b))>0);
    ++checked;
  }
  assert(checked==10);
}

int main(int argc,char **argv) {
  assert(argc==5);
  late_fixed_endpoint(); reconstruction();
  const unsigned copies=std::atoi(argv[1]);
  const bool lrat=std::atoi(argv[2]);
  std::vector<std::vector<int>> clauses;
  for (unsigned i=0;i<copies;++i) { clauses.push_back({-1,2}); clauses.push_back({1,-2}); }
  clauses.push_back({2,3}); clauses.push_back({3,4});
  // Independent PHP(4,3): no binary SCC refutes before the witnessed rewrite.
  for (int p=0;p<4;++p) clauses.push_back({5+3*p,6+3*p,7+3*p});
  for (int h=0;h<3;++h)
    for (int a=0;a<4;++a)
      for (int b=a+1;b<4;++b) clauses.push_back({-(5+3*a+h),-(5+3*b+h)});
  FILE *cnf=std::fopen(argv[3],"wx"); assert(cnf);
  std::fprintf(cnf,"p cnf 16 %zu\n",clauses.size());
  Kernel k; kernel_reserve_vars(k,16);
  for (const auto &clause:clauses) {
    std::vector<Lit> lits;
    for (int x:clause) { std::fprintf(cnf,"%d ",x); lits.push_back(internalize(x)); }
    std::fputs("0\n",cnf);
    if (lits.size()==2) kernel_add_binary_clause(k,lits[0],lits[1]);
    else kernel_add_large_clause(k,lits.data(),lits.size(),false,0);
  }
  std::fclose(cnf);
  Proof proof; assert(proof.open(argv[4],lrat?ProofFormat::Lrat:ProofFormat::DratAscii)); k.proof=&proof;
  const auto before=k.stats.bve_marks;
  assert(substitute_apply(k)==ApplyResult::Ok);
  assert(k.stats.bve_marks-before==4*copies);
  assert(kernel_var_retired(k,1));
  assert(default_solve(k)==20);
  k.proof=nullptr; proof.flush();
  std::puts("PASS: late-fixed removal, ten reconstructed models and witnessed Substitute proof");
}
