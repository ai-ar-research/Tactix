#undef NDEBUG
#include "rules/lucky/parity_certificate.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace tactix::parity_certificate;
struct Sink {
  uint32_t fresh = 9;
  uint64_t additions = 0, limit = UINT64_MAX;
  FILE *proof = nullptr;
  bool add(const SmallClause &c, bool) {
    if (additions == limit) return false;
    ++additions;
    if (proof) { for (uint32_t i=0;i<c.size;++i) std::fprintf(proof,"%d ",c.lits[i]); std::fprintf(proof,"0\n"); }
    return true;
  }
  bool erase(const SmallClause &c) {
    if (proof) { std::fprintf(proof,"d "); for (uint32_t i=0;i<c.size;++i) std::fprintf(proof,"%d ",c.lits[i]); std::fprintf(proof,"0\n"); }
    return true;
  }
  bool new_variable(External &v) { v=++fresh;return true; }
};
static std::vector<Equation> equations() {
  return {{{1,3,4,0},3,false},{{4,5,6,0},3,false},{{6,7,3,0},3,false},{{2,5,7,0},3,false}};
}
static void check_binary(const Clauses &out,bool opposite) {
  assert(out.size()==2);
  for(unsigned mask=0;mask<4;++mask) {
    bool satisfied=true;
    for(const auto &c:out) {
      bool value=false;assert(c.size==2);
      for(unsigned i=0;i<c.size;++i) {
        const int l=c.lits[i];assert(l==1||l==-1||l==2||l==-2);
        value |= bool(mask&(1u<<(std::abs(l)-1)))==(l>0);
      }
      satisfied &= value;
    }
    assert(satisfied == (bool((mask&1)^((mask>>1)&1))==opposite));
  }
}
int main(int argc,char **argv) {
  for(bool opposite:{false,true}) {
    auto input=equations();input.back().rhs=opposite;
    Sink sink;Certificate certificate(sink,7);Clauses output;
    assert(certificate.accumulate(input));assert(!certificate.refuted());
    const auto before=sink.additions;
    assert(!certificate.export_small_consequence(1,output));assert(sink.additions==before);
    assert(certificate.export_small_consequence(2,output));check_binary(output,opposite);
    assert(!certificate.accumulate(input));
  }
  { Sink s;Certificate c(s,7);auto e=equations();e.push_back({{2,0,0,0},1,true});Clauses out;
    assert(c.accumulate(e));assert(c.export_small_consequence(1,out));assert(out.size()==1&&out[0].size==1&&out[0].lits[0]==1); }
  { Sink s;Certificate c(s,7);auto e=equations();e.push_back({{1,2,0,0},2,false});Clauses out;
    assert(c.accumulate(e));assert(c.export_small_consequence(0,out));assert(out.empty()); }
  { Sink s;Certificate c(s,7);auto e=equations();e.push_back({{1,2,0,0},2,true});
    assert(c.run(e));assert(c.refuted());Clauses out;assert(!c.export_small_consequence(2,out)); }
  { Sink s;s.limit=1;Certificate c(s,7);assert(!c.accumulate(equations()));Clauses out;assert(!c.export_small_consequence(2,out)); }
  { Sink s;Certificate c(s,7);auto e=equations();e.back().variables[0]=8;assert(!c.accumulate(e));assert(!s.additions); }
  for(uint8_t width:{uint8_t(3),uint8_t(4)}) {
    Sink s;Certificate c(s,7);Equation row{{1,2,3,4},width,true};Clauses out;
    assert(c.accumulate({row}));assert(c.export_small_consequence(width,out));
    const auto expected=xor_equation(row.variables.data(),width,true);
    assert(out.size()==expected.size());
    for(size_t i=0;i<out.size();++i) {
      assert(out[i].size==expected[i].size);
      for(unsigned j=0;j<out[i].size;++j)assert(out[i].lits[j]==expected[i].lits[j]);
    }
  }
  { Sink s;Certificate c(s,7);Clauses out;assert(c.accumulate(equations()));
    s.limit=s.additions;assert(!c.export_small_consequence(2,out));assert(out.empty()); }
  if(argc==3) {
    auto e=equations();FILE *cnf=std::fopen(argv[1],"w");assert(cnf);
    std::fprintf(cnf,"p cnf 9 20\n");
    for(const auto &row:e)for(const auto &cl:xor_equation(row.variables.data(),row.size,row.rhs)) {
      for(unsigned i=0;i<cl.size;++i)std::fprintf(cnf,"%d ",cl.lits[i]);
      std::fprintf(cnf,"0\n"); }
    std::fprintf(cnf,"8 9 0\n8 -9 0\n-8 9 0\n-8 -9 0\n");std::fclose(cnf);
    Sink sink;sink.proof=std::fopen(argv[2],"w");assert(sink.proof);
    Certificate c(sink,7);Clauses out;assert(c.accumulate(e));assert(c.export_small_consequence(2,out));check_binary(out,false);
    std::fprintf(sink.proof,"8 0\n0\n");std::fclose(sink.proof);
    std::printf("prefix additions %llu; fresh proof variables %u\n",(unsigned long long)sink.additions,sink.fresh-9);
  }
  std::puts("test_parity_certificate: all passed");
}
