#undef NDEBUG
#include "diagnostics/shared_anf.hpp"
#include <cassert>
using namespace shared_anf;
static void gate(std::vector<Clause> &f,bool is_and,int o,int a,int b) {
  if(is_and){f.push_back({-o,a});f.push_back({-o,b});f.push_back({o,-a,-b});}
  else {f.push_back({o,a,-b});f.push_back({o,-a,b});f.push_back({-o,a,b});f.push_back({-o,-a,-b});}
}
static std::vector<Clause> fixture() {
  std::vector<Clause> f;
  gate(f,false,5,1,3);gate(f,false,6,2,4);gate(f,true,7,5,6);
  gate(f,false,8,1,4);gate(f,false,9,2,3);gate(f,true,10,8,9);
  return f;
}
static void algebra() {
  // All 256 Boolean functions on three variables, independent truth basis.
  const Term leaves{1,2,3};
  for(unsigned truth=0;truth<256;++truth) {
    Poly p;
    for(unsigned monomial=0;monomial<8;++monomial) {
      bool coefficient=false;
      for(unsigned subset=monomial;;subset=(subset-1)&monomial){coefficient^=(truth>>subset)&1u;if(!subset)break;}
      if(coefficient){Term t;for(unsigned j=0;j<3;++j)if(monomial&(1u<<j))t.push_back(j+1);p.push_back(std::move(t));}
    }
    std::sort(p.begin(),p.end());
    for(unsigned a=0;a<8;++a)assert(eval_polynomial(p,leaves,a)==bool((truth>>a)&1u));
    assert(add(p,p).empty());assert(negate(negate(p))==p);
    for(int v=1;v<=3;++v) {
      Work w;Poly product;assert(multiply(p,Poly{{v}},product,w));
      for(unsigned a=0;a<8;++a)assert(eval_polynomial(product,leaves,a)==(bool((truth>>a)&1u)&&bool(a&(1u<<(v-1)))));
    }
  }
}
static void signed_gate_encodings() {
  for(bool is_and:{false,true})for(int so:{-1,1})for(int sa:{-1,1})for(int sb:{-1,1}) {
    std::vector<Clause> f;gate(f,is_and,so*3,sa,sb*2);Work w;
    auto gates=authenticate(f,w);assert(gates.count(3));
    Limits limits;auto all=Discoverer(gates,limits,w).run();(void)all;
    const auto &g=gates.at(3);
    for(unsigned assignment=0;assignment<8;++assignment) {
      auto val=[&](int l){return bool(assignment&(1u<<(std::abs(l)-1)))==(l>0);};
      bool clauses=true;for(const auto &c:f){bool sat=false;for(int l:c)sat|=val(l);clauses&=sat;}
      bool inputs=g.is_and;for(int l:g.inputs)inputs=g.is_and ? inputs&&val(l):inputs!=val(l);
      assert(clauses==(val(g.output)==inputs));
    }
  }
}
static void authenticated_discovery() {
  auto f=fixture();Work w;Limits limits;auto d=discover(f,limits,w);
  assert(!w.exhausted&&d.gates==6&&d.cuts>=6&&d.novel_candidates>=2);
  assert(d.selected.size()==1&&d.selected[0].consumers.size()==2);
  assert(d.selected[0].shared==Poly({Term{1,2}})||d.selected[0].shared==Poly({Term{3,4}}));
  for(const auto &cut:d.selected[0].consumers) {
    Work validation;assert(truth_check(cut,validation));auto damaged=cut;
    damaged.polynomial=negate(damaged.polynomial);assert(!truth_check(damaged,validation));
  }
  // Removing the long AND clause removes its authenticated gate; no guessed
  // gate is allowed to stand in for missing CNF support.
  std::erase(f,Clause({7,-5,-6}));Work damaged_work;
  assert(!authenticate(f,damaged_work).count(7));
  // Malformed literals, tautologies and repeated literals cannot manufacture
  // a gate. Reordering clauses and literals preserves the selected function.
  f=fixture();std::reverse(f.begin(),f.end());for(auto &row:f)std::reverse(row.begin(),row.end());
  f.push_back({INT32_MIN,1});f.push_back({2,-2,1});Work reordered;
  assert(discover(f,limits,reordered).selected[0].shared==d.selected[0].shared);
  Work budget;budget.limit=1;assert(discover(f,limits,budget).selected.empty()&&budget.exhausted);
  volatile sig_atomic_t stop=1;Work interrupted;interrupted.stop=&stop;
  assert(discover(f,limits,interrupted).selected.empty()&&interrupted.exhausted);
  // Existing direct products are excluded from novelty even if many expanded
  // outputs contain them.
  f=fixture();gate(f,true,11,1,2);gate(f,true,12,3,4);Work represented;
  assert(discover(f,limits,represented).selected.empty());
}
int main(){algebra();signed_gate_encodings();authenticated_discovery();}
