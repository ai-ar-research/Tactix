#undef NDEBUG
#include "rules/lucky/parity_selection.hpp"
#include <cstdio>
#include <set>
using namespace tactix::parity_premises;
static void validate(const std::vector<CertifiedEquation> &inputs,const Selection &result) {
  for(const auto &row:result.consequences) {
    std::set<External> variables;bool rhs=false;
    for(size_t source:row.sources) {
      assert(source<inputs.size());const auto &e=inputs[source].equation;rhs^=e.rhs;
      for(unsigned i=0;i<e.size;++i) if(!variables.erase(e.variables[i]))variables.insert(e.variables[i]);
    }
    assert(std::vector<External>(variables.begin(),variables.end())==row.variables);assert(rhs==row.rhs);
  }
}
int main() {
  const std::vector<Equation> equations={{{1,3,4,0},3,false},{{4,5,6,0},3,false},{{6,7,3,0},3,false},{{2,5,7,0},3,false}};
  Collector collector({16,4});uint64_t occurrence=1;
  for(const auto &e:equations)for(const auto &c:tactix::parity_certificate::xor_equation(e.variables.data(),e.size,e.rhs))
    assert(collector.add(occurrence++,c.lits.data(),c.size));
  std::vector<CertifiedEquation> inputs;assert(collector.finish(inputs));assert(inputs.size()==4);
  const Selection full=select(inputs,true,{1000,16,16,2});validate(inputs,full);
  assert(!full.exhausted&&full.consequences.size()==1);
  assert((full.consequences[0].variables==std::vector<External>{1,2}));
  assert((full.consequences[0].sources==std::vector<size_t>{0,1,2,3}));
  // A different pivot order can find fewer useful short rows, never false ones.
  const Selection ascending=select(inputs,false,{1000,16,16,2});validate(inputs,ascending);
  assert(ascending.consequences.empty());
  for(uint64_t work=0;work<=full.work;++work) {
    auto bounded=select(inputs,true,{work,16,16,2});validate(inputs,bounded);assert(bounded.work<=work);
    assert(bounded.exhausted==(work<full.work));
  }
  auto narrow=select(inputs,true,{1000,2,16,2});validate(inputs,narrow);assert(narrow.width_declines==4&&narrow.consequences.empty());
  auto sources=select(inputs,true,{1000,16,1,2});validate(inputs,sources);assert(sources.source_declines>0&&sources.consequences.empty());
  // Recognize contradictory complete parity encodings and retain both sets of
  // premises. The selector must return their contradiction with provenance.
  Collector conflict({8,1});const External vars[]={1,2,3};occurrence=1;
  for(bool rhs:{false,true})for(const auto &c:tactix::parity_certificate::xor_equation(vars,3,rhs))
    assert(conflict.add(occurrence++,c.lits.data(),c.size));
  assert(conflict.finish(inputs));const auto unsat=select(inputs,true,{1000,16,16,2});validate(inputs,unsat);
  assert(unsat.consequences.size()==1&&unsat.consequences[0].variables.empty()&&unsat.consequences[0].rhs);
  std::printf("test_parity_selection: all passed; cancellation work %llu\n",(unsigned long long)full.work);
}
