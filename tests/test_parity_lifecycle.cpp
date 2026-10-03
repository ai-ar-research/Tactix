#undef NDEBUG
#include "rules/lucky/parity_certificate.hpp"
#include <cstdio>
#include <set>
using namespace tactix::parity_certificate;
using ClauseKey = std::vector<External>;
static ClauseKey key(const SmallClause &c) {
  ClauseKey result(c.lits.begin(),c.lits.begin()+c.size);
  std::sort(result.begin(),result.end());return result;
}
struct Sink {
  uint32_t fresh=9;
  uint64_t additions=0,deletions=0;
  bool fail_erase=false;
  FILE *proof=nullptr;
  std::multiset<ClauseKey> population;
  bool add(const SmallClause &c,bool) {
    ++additions;population.insert(key(c));
    if(proof) {for(unsigned i=0;i<c.size;++i)std::fprintf(proof,"%d ",c.lits[i]);std::fprintf(proof,"0\n");}
    return true;
  }
  bool erase(const SmallClause &c) {
    if(fail_erase)return false;
    auto found=population.find(key(c));assert(found!=population.end());population.erase(found);++deletions;
    if(proof) {std::fprintf(proof,"d ");for(unsigned i=0;i<c.size;++i)std::fprintf(proof,"%d ",c.lits[i]);std::fprintf(proof,"0\n");}
    return true;
  }
  bool new_variable(External &v) {v=++fresh;return true;}
  void no_helpers() const {
    for(const auto &c:population)for(External l:c)assert(l>=-9&&l<=9);
  }
};
static std::vector<Equation> equations() {
  return {{{1,3,4,0},3,false},{{4,5,6,0},3,false},{{6,7,3,0},3,false},{{2,5,7,0},3,false}};
}
static void import(Sink &s,const std::vector<Equation> &equations) {
  for(const auto &e:equations)for(const auto &c:xor_equation(e.variables.data(),e.size,e.rhs))s.population.insert(key(c));
}
int main(int argc,char **argv) {
  const auto input=equations();
  Sink sink;import(sink,input);
  if(argc==3) {
    FILE *cnf=std::fopen(argv[1],"w");assert(cnf);std::fprintf(cnf,"p cnf 9 20\n");
    for(const auto &e:input)for(const auto &c:xor_equation(e.variables.data(),e.size,e.rhs)) {
      for(unsigned i=0;i<c.size;++i)std::fprintf(cnf,"%d ",c.lits[i]);
      std::fprintf(cnf,"0\n");
    }
    std::fprintf(cnf,"8 9 0\n8 -9 0\n-8 9 0\n-8 -9 0\n");std::fclose(cnf);
    sink.proof=std::fopen(argv[2],"w");assert(sink.proof);
  }
  for(unsigned repetition=0;repetition<2;++repetition) {
    Certificate certificate(sink,7);Clauses out;
    assert(!certificate.retire_helpers());assert(certificate.accumulate(input));
    assert(!certificate.retire_helpers());assert(certificate.export_small_consequence(2,out));
    assert(certificate.retire_helpers());sink.no_helpers();
    for(const auto &clause:out)assert(sink.population.count(key(clause)));
    const auto deleted=sink.deletions;assert(certificate.retire_helpers());assert(sink.deletions==deleted);
    assert(!certificate.export_small_consequence(2,out));
    // Deleted helper unit assignments may remain cached by proof checkers.
    // Never reuse these variable IDs; keep the sink high-water mark monotone.
  }
  if(sink.proof) {std::fprintf(sink.proof,"8 0\n0\n");std::fclose(sink.proof);}
  // Preserve a unit on an original variable through cleanup even when it is
  // also the tree's accumulated root unit.
  { auto e=input;e.push_back({{2,0,0,0},1,true});Sink s;import(s,e);Certificate c(s,7);Clauses out;
    assert(c.accumulate(e));assert(c.export_small_consequence(1,out));assert(c.retire_helpers());s.no_helpers();
    assert(s.population.count(ClauseKey{1})); }
  // A failed deletion never becomes successful through a second cleanup call.
  { Sink s;import(s,input);Certificate c(s,7);Clauses out;assert(c.accumulate(input));assert(c.export_small_consequence(2,out));
    s.fail_erase=true;assert(!c.retire_helpers());s.fail_erase=false;assert(!c.retire_helpers()); }
  std::printf("test_parity_lifecycle: passed; %llu additions, %llu deletions, two retired derivations with monotone helper IDs\n",(unsigned long long)sink.additions,(unsigned long long)sink.deletions);
}
