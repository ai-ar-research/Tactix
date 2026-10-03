#undef NDEBUG
#include "rules/lucky/parity_premises.hpp"
#include <cassert>
#include <cstdio>
using namespace tactix::parity_premises;
using tactix::parity_certificate::xor_equation;
int main() {
  // Complete encodings, both polarities, all supported widths and permuted
  // literal order preserve the relation and exact source occurrence identities.
  for (uint8_t width = 2; width <= 4; ++width) {
    for (bool rhs : {false, true}) {
      const External variables[] = {11, 23, 47, 89};
      const auto clauses = xor_equation(variables, width, rhs);
      Collector collector({100, 10});
      uint64_t id = 1;
      for (auto clause : clauses) {
        std::reverse(clause.lits.begin(), clause.lits.begin()+clause.size);
        assert(collector.add(id++, clause.lits.data(), clause.size));
      }
      // A second occurrence must not inflate coverage or replace provenance.
      assert(collector.add(id, clauses[0].lits.data(), clauses[0].size));
      std::vector<CertifiedEquation> out;
      assert(collector.finish(out));assert(out.size()==1);
      assert(out[0].equation.rhs==rhs && out[0].equation.size==width);
      assert(out[0].premises.size()==clauses.size());
      for(uint64_t premise:out[0].premises) assert(premise>0 && premise<id);
      for(unsigned i=0;i<width;++i) assert(out[0].equation.variables[i]==variables[i]);
      // Removing any required clause invalidates the equation, not the input.
      for(size_t missing=0;missing<clauses.size();++missing) {
        Collector partial({100,10});
        for(size_t i=0;i<clauses.size();++i) if(i!=missing)
          assert(partial.add(i+1,clauses[i].lits.data(),clauses[i].size));
        assert(partial.finish(out));assert(out.empty());
      }
    }
  }
  const External vars[]={1,2,3};
  Collector both({8,1});uint64_t id=1;
  for(bool rhs:{false,true})for(const auto &c:xor_equation(vars,3,rhs))
    assert(both.add(id++,c.lits.data(),c.size));
  std::vector<CertifiedEquation> out;assert(both.finish(out));assert(out.size()==2);
  assert(out[0].equation.rhs!=out[1].equation.rhs);
  { Collector c({1,1});const External a[]={1,2};assert(c.add(1,a,2));
    assert(!c.add(2,a,2));assert(c.status()==Status::Exhausted);assert(!c.finish(out));assert(out.empty()); }
  { Collector c({10,0});const External a[]={1,2};assert(!c.add(1,a,2));assert(c.status()==Status::Exhausted); }
  for(const auto bad: {External(0),External(INT32_MIN)}) {
    Collector c({10,1});const External a[]={1,bad};assert(!c.add(1,a,2));assert(c.status()==Status::Invalid);
  }
  { Collector c({10,1});const External a[]={1,2};assert(c.add(1,a,2));assert(!c.add(1,a,2));assert(!c.finish(out)); }
  { Collector c({10,1});const External a[]={1,2};assert(!c.add(0,a,2)); }
  { Collector c({10,1});const External a[]={1,1};const External b[]={1,-1};
    assert(c.add(1,a,2));assert(c.add(2,b,2));assert(c.finish(out));assert(out.empty());assert(c.groups_seen()==0); }
  std::puts("test_parity_premises: all passed");
}
