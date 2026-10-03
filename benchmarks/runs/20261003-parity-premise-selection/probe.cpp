#undef NDEBUG
#include "rules/lucky/parity_selection.hpp"
#include <cstdio>
using namespace tactix::parity_premises;
using namespace tactix::parity_certificate;
struct Sink {
  FILE *proof;
  unsigned fresh=9;
  unsigned additions=0;
  bool add(const SmallClause &c,bool) {
    ++additions;for(unsigned i=0;i<c.size;++i)std::fprintf(proof,"%d ",c.lits[i]);std::fprintf(proof,"0\n");return true;
  }
  bool erase(const SmallClause &c) {
    std::fprintf(proof,"d ");for(unsigned i=0;i<c.size;++i)std::fprintf(proof,"%d ",c.lits[i]);std::fprintf(proof,"0\n");return true;
  }
  bool new_variable(External &v) {v=++fresh;return true;}
};
int main(int argc,char **argv) {
  assert(argc==4);FILE *cnf=std::fopen(argv[1],"w"),*proof=std::fopen(argv[2],"w"),*trace=std::fopen(argv[3],"w");assert(cnf&&proof&&trace);
  std::fprintf(cnf,"p cnf 9 20\n");
  const std::vector<Equation> input={{{1,3,4,0},3,false},{{4,5,6,0},3,false},{{6,7,3,0},3,false},{{2,5,7,0},3,false}};
  Collector collector({16,4});unsigned occurrence=1;
  for(const auto &e:input)for(const auto &c:xor_equation(e.variables.data(),e.size,e.rhs)) {
    assert(collector.add(occurrence,c.lits.data(),c.size));
    std::fprintf(trace,"{\"kind\":\"input\",\"id\":%u,\"clause\":[",occurrence++);
    for(unsigned i=0;i<c.size;++i) {std::fprintf(cnf,"%d ",c.lits[i]);std::fprintf(trace,"%s%d",i?",":"",c.lits[i]);}
    std::fprintf(cnf,"0\n");std::fprintf(trace,"]}\n");
  }
  std::fprintf(cnf,"8 9 0\n8 -9 0\n-8 9 0\n-8 -9 0\n");std::fclose(cnf);
  std::vector<CertifiedEquation> certified;assert(collector.finish(certified));
  const auto selected=select(certified,true,{1000,16,16,2});assert(selected.consequences.size()==1);
  const auto &consequence=selected.consequences[0];assert((consequence.variables==std::vector<External>{1,2})&&!consequence.rhs);
  std::vector<Equation> equations;
  for(size_t index:consequence.sources) {
    const auto &e=certified[index];equations.push_back(e.equation);
    std::fprintf(trace,"{\"kind\":\"selected_equation\",\"equation\":%zu,\"rhs\":%s,\"variables\":[",index,e.equation.rhs?"true":"false");
    for(unsigned i=0;i<e.equation.size;++i)std::fprintf(trace,"%s%d",i?",":"",e.equation.variables[i]);
    std::fprintf(trace,"],\"premises\":[");for(size_t i=0;i<e.premises.size();++i)std::fprintf(trace,"%s%llu",i?",":"",(unsigned long long)e.premises[i]);std::fprintf(trace,"]}\n");
  }
  Sink sink{proof};Certificate certificate(sink,7);Clauses output;
  assert(certificate.accumulate(equations));assert(certificate.export_small_consequence(2,output));assert(output.size()==2);
  std::fprintf(proof,"8 0\n0\n");std::fclose(proof);std::fclose(trace);
  std::printf("certified equations %zu; source clauses 16; work %llu; prefix additions %u; fresh helpers %u\n",equations.size(),(unsigned long long)selected.work,sink.additions,sink.fresh-9);
}
