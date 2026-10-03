#undef NDEBUG
#include "rules/lucky/parity_certificate.hpp"
#include "proof/proof.hpp"
#include <cstdio>
using namespace tactix;
using namespace tactix::parity_certificate;
struct NativeSink {
  Proof &proof;
  uint64_t id=20;
  int32_t fresh=9;
  bool add(const SmallClause &c,bool rat) {
    return rat?proof.add_external_rat(++id,c.lits.data(),c.size):proof.add_external_rup(++id,c.lits.data(),c.size);
  }
  bool erase(const SmallClause &c) {return proof.del_external(id,0,c.lits.data(),c.size);}
  bool new_variable(External &v) {v=++fresh;return true;}
};
int main(int argc,char **argv) {
  assert(argc==2);Proof proof;assert(proof.open(argv[1],ProofFormat::DsrAscii));NativeSink sink{proof};
  const std::vector<Equation> equations={{{1,3,4,0},3,false},{{4,5,6,0},3,false},{{6,7,3,0},3,false},{{2,5,7,0},3,false}};
  for(unsigned i=0;i<2;++i) {
    Certificate certificate(sink,7);Clauses out;
    assert(certificate.accumulate(equations));assert(certificate.export_small_consequence(2,out));
    assert(certificate.retire_helpers());assert(out.size()==2);
  }
  SmallClause unit;unit.size=1;unit.lits[0]=8;assert(sink.add(unit,false));SmallClause empty;assert(sink.add(empty,false));proof.flush();
  std::printf("native proof writer: two retired derivations; final helper ID %d\n",sink.fresh);
}
