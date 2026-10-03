#undef NDEBUG
#include "proof/cardinality_certificate.hpp"
#include <cstdio>
#include <limits>
#include <map>
#include <string>

using namespace tactix::cardinality_certificate;
struct Definition {
  External auxiliary, condition;
  int64_t high, low;
  bool operator==(const Definition &) const = default;
};
struct TestSink {
  bool stopped = false;
  uint32_t fresh = 3;
  uint64_t work = 0, nodes = 0, additions = 0;
  uint64_t work_limit = UINT64_MAX, node_limit = UINT64_MAX,
           addition_limit = UINT64_MAX;
  std::vector<Definition> definitions;
  std::vector<std::pair<Clause, bool>> proof;
  bool charge_work(uint64_t amount) {
    if (stopped || amount > work_limit - work) { stopped = true; return false; }
    work += amount; return true;
  }
  bool new_variable(External &out) {
    if (stopped || nodes == node_limit || fresh == INT32_MAX) {
      stopped = true; return false;
    }
    ++nodes; out = ++fresh; return true;
  }
  bool add(const Clause &clause, bool rat) {
    if (stopped || additions == addition_limit) { stopped = true; return false; }
    ++additions; proof.emplace_back(clause, rat); return true;
  }
  bool record_definition(External auxiliary, External condition,
                         int64_t high, int64_t low) {
    definitions.push_back({auxiliary, condition, high, low}); return true;
  }
};
using Engine = BddProof<TestSink>;

static bool value(int64_t literal, const std::map<External, bool> &values) {
  if (literal == INT64_MAX) return true;
  if (literal == INT64_MIN) return false;
  return values.at(static_cast<External>(std::abs(literal))) == (literal > 0);
}
static std::map<External, bool> extend(const TestSink &s, unsigned assignment) {
  std::map<External, bool> values;
  for (External v = 1; v <= 3; ++v) values[v] = assignment & (1u << (v-1));
  for (const auto &d : s.definitions) {
    assert(!values.count(d.auxiliary));
    const bool high = value(d.high, values), low = value(d.low, values);
    values[d.auxiliary] = values.at(d.condition) ? high : low;
  }
  for (const auto &[clause, rat] : s.proof) if (rat) {
    bool satisfied = false;
    for (const External lit : clause) satisfied |= value(lit, values);
    assert(satisfied); // conservative definitions for every old assignment
  }
  return values;
}
static void semantic_controls() {
  for (const auto &terms : std::vector<std::vector<std::pair<External,int64_t>>>{
      {}, {{1,1},{2,1},{3,1}}, {{1,-1},{2,2},{3,-3}},
      {{1,INT64_MIN},{2,INT64_MAX}}}) {
    for (const int64_t bound : {INT64_MIN, int64_t(-3), int64_t(-1),
                                int64_t(0), int64_t(1), int64_t(3), INT64_MAX}) {
      TestSink sink; Engine engine(sink);
      const auto root = engine.build_constraint(terms,bound); assert(!sink.stopped);
      for (unsigned mask=0;mask<8;++mask) {
        __int128 sum = 0;
        for (const auto &[variable, coefficient] : terms)
          if (mask & (1u << (variable-1))) sum += coefficient;
        assert(value(engine.root_literal(root),extend(sink,mask)) == (sum>=bound));
      }
    }
  }
  for (unsigned width=0;width<=3;++width) for (unsigned signs=0;signs<(1u<<width);++signs) {
    Clause c;for (unsigned i=0;i<width;++i)c.push_back((signs&(1u<<i))?-int(i+1):int(i+1));
    TestSink sink;Engine engine(sink);const auto root=engine.build_clause(c);
    assert(!sink.stopped);
    for (unsigned mask=0;mask<8;++mask) {
      bool expected=false;
      for (int lit:c)expected|=bool(mask&(1u<<(std::abs(lit)-1)))==(lit>0);
      assert(value(engine.root_literal(root),extend(sink,mask))==expected);
    }
  }
}
static void invalid_controls() {
  for (const Clause &c : std::vector<Clause>{{0},{INT32_MIN},{INT32_MAX},{1,1},{1,-1}}) {
    TestSink s;Engine e(s);e.build_clause(c);
    assert(s.stopped && !s.nodes && !s.additions);
  }
  for (const auto &terms : std::vector<std::vector<std::pair<External,int64_t>>>{
      {{0,1}},{{-1,1}},{{INT32_MAX,1}},{{1,0}},{{2,1},{1,1}},{{1,1},{1,2}},
      {{1,INT64_MAX},{2,1}},{{1,INT64_MIN},{2,-1}}}) {
    TestSink s;Engine e(s);e.build_constraint(terms,0);
    assert(s.stopped && !s.nodes && !s.additions);
  }
}
static const std::vector<Clause> odd_input{
  {1,2},{-1,-2},{1,3},{-1,-3},{2,3},{-2,-3}};
static bool odd_certificate(TestSink &sink) {
  Engine engine(sink);uint32_t totals[2]={engine.truth(),engine.truth()};
  int64_t coefficients[2][4]{};
  for (unsigned v=0;v<3;++v)for(unsigned side=0;side<2;++side) {
    const Clause &premise=odd_input[2*v+side];uint32_t root=0;
    if(!engine.validate_clause(premise,root))return false;
    const int64_t sign=side?-1:1;
    std::vector<std::pair<External,int64_t>> local;
    for(int lit:premise){local.emplace_back(std::abs(lit),sign);coefficients[side][std::abs(lit)]+=sign;}
    const auto degree=engine.build_constraint(local,sign);
    if(!engine.imply_and_validate(root,engine.truth(),degree))return false;
    std::vector<std::pair<External,int64_t>> sum;
    for(External var=1;var<=3;++var)if(coefficients[side][var])sum.emplace_back(var,coefficients[side][var]);
    const auto total=engine.build_constraint(sum,sign*(v+1));
    if(!engine.imply_and_validate(totals[side],degree,total))return false;
    totals[side]=total;
  }
  return engine.imply_and_validate(totals[0],totals[1],engine.falsity()) && !sink.stopped;
}
static void bound_controls(const TestSink &complete) {
  for(unsigned kind=0;kind<3;++kind)for(bool short_budget:{false,true}) {
    TestSink sink;
    if(kind==0)sink.work_limit=complete.work-short_budget;
    if(kind==1)sink.node_limit=complete.nodes-short_budget;
    if(kind==2)sink.addition_limit=complete.additions-short_budget;
    const bool ok=odd_certificate(sink);
    assert(ok==!short_budget);
    if(ok)assert(sink.proof==complete.proof && sink.definitions==complete.definitions);
    else {
      const auto before=sink.proof.size();Engine stopped(sink);
      assert(!stopped.unit(stopped.truth()));assert(sink.proof.size()==before);
    }
  }
  TestSink zero;zero.work_limit=0;assert(!odd_certificate(zero));
  assert(zero.stopped && !zero.nodes && !zero.additions);
}
static TestSink equivalence() {
  TestSink sink;Engine engine(sink);std::vector<uint32_t> roots;
  for(size_t i=0;i<4;++i){uint32_t r=0;assert(engine.validate_clause(odd_input[i],r));roots.push_back(r);}
  const auto all=engine.conjunction(roots);
  for(const Clause &target:std::vector<Clause>{{-2,3},{2,-3}}) {
    const auto root=engine.build_clause(target);
    assert(engine.imply_and_validate(all,engine.truth(),root));assert(sink.add(target,false));
  }
  assert(!sink.stopped);return sink;
}
static void write_case(const std::string &base,const std::vector<Clause> &input,const TestSink &s) {
  FILE *cnf=std::fopen((base+".cnf").c_str(),"w");assert(cnf);
  std::fprintf(cnf,"p cnf 3 %zu\n",input.size());
  for(const Clause &c:input){for(int lit:c)std::fprintf(cnf,"%d ",lit);std::fputs("0\n",cnf);}
  std::fclose(cnf);
  FILE *proof=std::fopen((base+".dsr").c_str(),"w");assert(proof);
  for(const auto &[c,rat]:s.proof){(void)rat;for(int lit:c)std::fprintf(proof,"%d ",lit);std::fputs("0\n",proof);}
  std::fclose(proof);
  FILE *records=std::fopen((base+".definitions.jsonl").c_str(),"w");assert(records);
  for(const auto &d:s.definitions)std::fprintf(records,"{\"auxiliary\":%d,\"condition\":%d,\"high\":%lld,\"low\":%lld}\n",d.auxiliary,d.condition,(long long)d.high,(long long)d.low);
  std::fclose(records);
}
int main(int argc,char **argv) {
  semantic_controls();invalid_controls();TestSink odd;assert(odd_certificate(odd));
  bound_controls(odd);const auto eq=equivalence();
  assert(odd.definitions.size()==odd.nodes && eq.definitions.size()==eq.nodes);
  if(argc==2){write_case(std::string(argv[1])+"/odd",odd_input,odd);write_case(std::string(argv[1])+"/equivalence",{odd_input.begin(),odd_input.begin()+4},eq);}
  std::printf("cardinality_certificate: passed; odd work=%llu nodes=%llu additions=%llu; equivalence additions=%llu\n",(unsigned long long)odd.work,(unsigned long long)odd.nodes,(unsigned long long)odd.additions,(unsigned long long)eq.additions);
}
