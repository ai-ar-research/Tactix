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
  // A checked active proof tree can be resumed without re-emitting its
  // prefix.  Invalid tree shapes must leave the new certificate reusable.
  { const auto input=equations();
    Sink resumed_sink;Certificate first(resumed_sink,7);
    assert(first.accumulate({input[0],input[1]}));
    Certificate<Sink>::ActiveTree tree;assert(first.active_tree(tree));
    Certificate second(resumed_sink,7);
    auto bad=tree;bad.variable_count=8;
    assert(!second.resume_active_tree(bad));
    bad=tree;bad.root_unit.lits[0]=-bad.root_unit.lits[0];
    assert(!second.resume_active_tree(bad));
    bad=tree;bad.definitions.front().position=0;
    assert(!second.resume_active_tree(bad));
    bad=tree;bad.definitions.front().clauses.front().lits[0] *= -1;
    assert(!second.resume_active_tree(bad));
    assert(second.resume_active_tree(tree));
    assert(second.accumulate({input[2],input[3]}));
    Clauses result;assert(second.export_small_consequence(2,result));
    check_binary(result,false);
    Sink whole_sink;Certificate whole(whole_sink,7);
    assert(whole.accumulate(input));
    Clauses whole_result;assert(whole.export_small_consequence(2,whole_result));
    assert(resumed_sink.additions==whole_sink.additions);
  }
  // The previous source set need not be a subset: XORing a source twice
  // removes it.  This is the row-operation case needed by overlapping CDCL
  // reasons, whose prior sets seldom form exact subsets.
  { const auto input=equations();
    Sink sink;Certificate first(sink,7);
    assert(first.accumulate({input[0],input[1],input[2]}));
    Certificate<Sink>::ActiveTree tree;assert(first.active_tree(tree));
    const uint64_t prior_additions=sink.additions;
    Certificate next(sink,7);assert(next.resume_active_tree(tree));
    assert(next.accumulate({input[0],input[3]}));
    Certificate<Sink>::ActiveTree result;assert(next.active_tree(result));
    assert((result.variables==std::vector<External>{2,3,4}));
    assert(!result.rhs);
    Clauses exported;assert(next.export_small_consequence(3,exported));
    const External variables[3]={2,3,4};
    const Clauses expected=xor_equation(variables,3,false);
    assert(exported.size()==expected.size());
    for(size_t i=0;i<expected.size();++i) {
      assert(exported[i].size==expected[i].size);
      for(uint32_t j=0;j<expected[i].size;++j)
        assert(exported[i].lits[j]==expected[i].lits[j]);
    }
    Sink independent;Certificate from_scratch(independent,7);
    assert(from_scratch.accumulate({input[1],input[2],input[3]}));
    Clauses scratch_export;assert(from_scratch.export_small_consequence(3,scratch_export));
    std::printf("symmetric-difference incremental additions %llu vs fresh %llu\n",
      (unsigned long long)(sink.additions-prior_additions),
      (unsigned long long)independent.additions);
  }
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
  // The scalable continuing-action view retains only the final live XOR tree
  // and its implied root unit.  Earlier transition helpers stay proof-only.
  { Sink s;Certificate c(s,7);assert(c.accumulate(equations()));
    Certificate<Sink>::ActiveTree tree;assert(c.active_tree(tree));
    assert((tree.variables==std::vector<External>{1,2}));
    assert(tree.definitions.size()==1);
    const auto &d=tree.definitions.front();
    assert(d.output>7&&d.clauses.size()==4);
    assert(std::min(d.first,d.second)==1&&std::max(d.first,d.second)==2);
    assert(tree.root_unit.size==1);
    assert(std::abs(tree.root_unit.lits[0])==d.output);
    Clauses out;assert(c.export_small_consequence(2,out));
    Certificate<Sink>::ActiveTree after;assert(!c.active_tree(after)); }
  // Wider authenticated source equations use the same certificate.  Their
  // final live tree is compact even though transition helpers were needed.
  { Sink s;Certificate c(s,8);Equation row{{1,2,3,4,5,6,7,8},8,true};
    assert(c.accumulate({row}));Certificate<Sink>::ActiveTree tree;
    assert(c.active_tree(tree));
    assert((tree.variables==std::vector<External>{1,2,3,4,5,6,7,8}));
    assert(tree.definitions.size()==7);
    assert(tree.root_unit.size==1&&tree.root_unit.lits[0]>8);
    for(size_t i=0;i<tree.definitions.size();++i) {
      const auto &d=tree.definitions[i];
      if(i)assert(tree.definitions[i-1].output<d.output);
      assert(d.first<d.output&&d.second<d.output&&d.clauses.size()==4);
    } }
  { Sink s;Certificate c(s,8);Equation row{{1,2,3,4,5,6,7,8},8,false};
    assert(c.accumulate({row}));Certificate<Sink>::ActiveTree tree;
    assert(c.active_tree(tree));assert(!tree.rhs);
    assert(tree.root_unit.size==1&&tree.root_unit.lits[0]<-8); }
  // The derivation bound follows the authenticated row width, rather than
  // counting transient proof helpers as independent branch decisions.
  { Equation first{{1,2,3,4,5,6,7,8},8,false};
    Equation second{{1,9,10,11,12,13,14,15},8,true};
    Sink bounded;bounded.fresh=15;Certificate legacy(bounded,15);
    assert(!legacy.accumulate({first,second}));
    Sink s;s.fresh=15;Certificate c(s,15,UINT32_MAX);
    assert(c.accumulate({first,second}));
    Certificate<Sink>::ActiveTree tree;assert(c.active_tree(tree));
    assert((tree.variables==std::vector<External>{2,3,4,5,6,7,8,
                                                   9,10,11,12,13,14,15}));
    assert(tree.rhs&&tree.definitions.size()==13);
    assert(tree.root_unit.size==1&&tree.root_unit.lits[0]>15); }
  if(argc==3) {
    auto e=equations();FILE *cnf=std::fopen(argv[1],"w");assert(cnf);
    std::fprintf(cnf,"p cnf 9 20\n");
    for(const auto &row:e)for(const auto &cl:xor_equation(row.variables.data(),row.size,row.rhs)) {
      for(unsigned i=0;i<cl.size;++i)std::fprintf(cnf,"%d ",cl.lits[i]);
      std::fprintf(cnf,"0\n"); }
    std::fprintf(cnf,"8 9 0\n8 -9 0\n-8 9 0\n-8 -9 0\n");std::fclose(cnf);
    Sink sink;sink.proof=std::fopen(argv[2],"w");assert(sink.proof);
    Certificate prefix(sink,7);assert(prefix.accumulate({e[0],e[1],e[2]}));
    Certificate<Sink>::ActiveTree tree;assert(prefix.active_tree(tree));
    Certificate resumed(sink,7);assert(resumed.resume_active_tree(tree));
    Clauses out;assert(resumed.accumulate({e[0],e[3]}));
    assert(resumed.export_small_consequence(3,out));
    std::fprintf(sink.proof,"8 0\n0\n");std::fclose(sink.proof);
    std::printf("prefix additions %llu; fresh proof variables %u\n",(unsigned long long)sink.additions,sink.fresh-9);
  }
  std::puts("test_parity_certificate: all passed");
}
