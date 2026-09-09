// Fixed five-variable public Congruence regression, not a benchmark solver run.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/registry.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
using namespace tactix;
static bool satisfied(int literal,unsigned mask) {
  bool value=(mask>>(std::abs(literal)-1))&1;
  return literal>0 ? value : !value;
}
int main(int argc,char **argv) {
  assert(argc==6);
  int mode=std::atoi(argv[1]),position=std::atoi(argv[2]),missing=std::atoi(argv[3]);
  assert(mode>=0 && mode<2 && position>=0 && position<4 && missing>=0 && missing<2);
  std::vector<std::vector<int>> input;
  for (int output : {1,4})
    for (unsigned signs=0;signs<8;++signs) {
      if (__builtin_popcount(signs)%2!=1) continue;
      if (output==4 && missing && signs==7) continue;
      std::vector<int> clause={output,2,3};
      for (unsigned i=0;i<3;++i) if (signs&(1u<<i)) clause[i]=-clause[i];
      if (output==4) clause.insert(clause.begin()+position,5);
      input.push_back(clause);
    }
  input.push_back({1,4}); input.push_back({-1,-4}); input.push_back({-5});
  Kernel k; kernel_reserve_vars(k,5);
  std::ofstream cnf(std::string(argv[4])+".cnf"); assert(cnf);
  cnf<<"p cnf 5 "<<input.size()<<'\n';
  for (const auto &row:input) {
    std::vector<Lit> lits;
    for (int lit:row) { cnf<<lit<<' '; lits.push_back(internalize(lit)); }
    cnf<<"0\n";
    uint64_t id=++k.next_clause_id;
    if (lits.size()==1) { k.unit_ids[var_of(lits[0])]=id; kernel_assign_unit(k,lits[0]); }
    else if (lits.size()==2) kernel_add_binary_clause(k,lits[0],lits[1],id);
    else kernel_add_large_clause(k,lits.data(),lits.size(),false,0,id);
  }
  cnf.close(); assert(!kernel_propagate(k));
  unsigned original=0;
  for (unsigned mask=0;mask<32;++mask) {
    bool model=true;
    for (const auto &row:input) {
      bool clause=false; for (int lit:row) clause|=satisfied(lit,mask); model&=clause;
    }
    if (model) original|=1u<<mask;
  }
  assert(bool(original)==bool(missing));
  Proof proof; assert(proof.open(argv[5],mode?ProofFormat::Lrat:ProofFormat::DratAscii));
  proof.bind_variable_map(&k.internal_to_external); k.proof=&proof;
  const int id=tactix_rule_id("Congruence"); assert(id>=0);
  const auto &rule=tactix_rules()[id]; uint64_t instance=0;
  assert(rule.instances(k,&instance,1)==1);
  const auto result=rule.apply_instance(k,instance);
  if (!missing) assert(result==ApplyResult::Unsat && k.inconsistent);
  else {
    assert(result!=ApplyResult::Unsat && !k.inconsistent);
    unsigned after=0;
    for (unsigned mask=0;mask<32;++mask) {
      bool model=true;
      for (Lit lit=0;lit<10;++lit) {
        if (k.value(lit)>0) model&=satisfied(kernel_externalize(k,lit),mask);
        for (const WatchEntry w:k.watches.entries(lit)) if (w.binary())
          model&=satisfied(kernel_externalize(k,lit),mask)||satisfied(kernel_externalize(k,w.other()),mask);
      }
      for (Ref ref=0;ref<k.arena.size();) {
        const Clause *cl=clause_at(k.arena,ref);
        if (!cl->garbage()) {
          bool holds=false; for (Lit lit:*cl) holds|=satisfied(kernel_externalize(k,lit),mask);
          model&=holds;
        }
        ref+=clause_actual_words(cl);
      }
      if (model) after|=1u<<mask;
    }
    assert(after==original);
  }
  proof.flush(); k.proof=nullptr;
  std::printf("PASS mode=%d position=%d missing=%d models=%u\n",mode,position,missing,original);
}
