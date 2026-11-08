#pragma once
// Research-only, deterministic discovery of shared nonlinear functions in
// bounded, CNF-authenticated AND/XOR cuts. No solver or proof dependencies.
#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <cstdlib>
#include <iterator>
#include <tuple>
#include <cstdint>
#include <csignal>
#include <map>
#include <memory>
#include <set>
#include <unordered_set>
#include <vector>

namespace shared_anf {
using Clause=std::vector<int32_t>;
using Term=std::vector<int32_t>; // sorted positive variable identities
using Poly=std::vector<Term>;   // sorted distinct monomials; empty term = 1
struct Work {
  uint64_t used=0,limit=50000000;
  const volatile sig_atomic_t *stop=nullptr;
  bool exhausted=false;
  bool take(uint64_t n=1) {
    if (exhausted || (stop && *stop) || n>limit-used) {exhausted=true;return false;}
    used+=n;return true;
  }
};
struct Limits {
  unsigned leaves=6,depth=4,terms=64,groups=32,consumers=8;
  uint64_t cuts=200000;
};
inline Poly add(const Poly &a,const Poly &b) {
  Poly out;std::set_symmetric_difference(a.begin(),a.end(),b.begin(),b.end(),std::back_inserter(out));return out;
}
inline bool multiply(const Poly &a,const Poly &b,Poly &out,Work &work,unsigned cap=64) {
  std::set<Term> terms;
  for (const auto &x:a) for (const auto &y:b) {
    if (!work.take(x.size()+y.size()+1)) return false;
    Term t;std::set_union(x.begin(),x.end(),y.begin(),y.end(),std::back_inserter(t));
    auto [at,inserted]=terms.insert(std::move(t));if (!inserted) terms.erase(at);
  }
  if (terms.size()>cap) return false;
  out.assign(terms.begin(),terms.end());return true;
}
inline Poly negate(const Poly &p) {return add(p,Poly{Term{}});}
inline uint64_t pair_key(int32_t a,int32_t b) {
  auto code=[](int32_t l) {return 2u*(uint32_t(l<0 ? -l:l)-1u)+uint32_t(l<0);};
  uint32_t x=code(a),y=code(b);if(x>y)std::swap(x,y);return(uint64_t(x)<<32)|y;
}
struct Gate {
  bool is_and=true;
  int32_t output=0;
  Clause inputs;
  std::vector<uint32_t> sources;
};
inline std::map<int32_t,Gate> authenticate(const std::vector<Clause> &formula,Work &work) {
  std::map<int32_t,Gate> out;
  std::map<uint64_t,uint32_t> binaries;
  std::map<Clause,std::map<uint32_t,uint32_t>> patterns;
  std::vector<std::pair<uint32_t,Clause>> wide;
  for (uint32_t i=0;i<formula.size();++i) {
    auto c=formula[i];if(!work.take(c.size()+1))return {};
    if(std::any_of(c.begin(),c.end(),[](int32_t l){return !l||l==INT32_MIN;}))continue;
    std::sort(c.begin(),c.end());c.erase(std::unique(c.begin(),c.end()),c.end());
    bool taut=false;for(auto l:c)if(std::binary_search(c.begin(),c.end(),-l)){taut=true;break;}
    if(taut)continue;
    if(c.size()==2)binaries.emplace(pair_key(c[0],c[1]),i);
    if(c.size()<3||c.size()>5)continue;
    wide.push_back({i,c});Clause variables;for(auto l:c)variables.push_back(std::abs(l));
    std::sort(variables.begin(),variables.end());uint32_t mask=0;
    for(unsigned j=0;j<variables.size();++j)if(std::binary_search(c.begin(),c.end(),-variables[j]))mask|=1u<<j;
    patterns[variables].emplace(mask,i);
  }
  auto save=[&](Gate g) {
    const auto v=std::abs(g.output);auto found=out.find(v);
    auto order=[](const Gate &x) {
      unsigned backward=0;for(auto l:x.inputs)backward+=std::abs(l)<std::abs(x.output);
      return std::tuple(backward,x.is_and,x.inputs.size(),x.output,x.inputs);
    };
    if(found==out.end()||order(g)>order(found->second))out[v]=std::move(g);
  };
  for(const auto &[id,c]:wide)for(auto output:c) {
    if(!work.take())return {};
    Gate g;g.output=output;g.sources.push_back(id);
    for(auto l:c)if(l!=output) {
      auto found=binaries.find(pair_key(-output,-l));
      if(found==binaries.end())break;
      g.inputs.push_back(-l);g.sources.push_back(found->second);
    }
    if(g.inputs.size()+1==c.size()){std::sort(g.inputs.begin(),g.inputs.end());save(std::move(g));}
  }
  for(const auto &[variables,mapping]:patterns)for(unsigned parity=0;parity<2;++parity) {
    if(!work.take(1u<<variables.size()))return {};
    Gate g;g.is_and=false;g.output=parity ? variables.back():-variables.back();
    g.inputs.assign(variables.begin(),variables.end()-1);
    bool complete=true;
    for(uint32_t mask=0;mask<(1u<<variables.size());++mask)if((std::popcount(mask)&1u)==parity) {
      auto found=mapping.find(mask);if(found==mapping.end()){complete=false;break;}
      g.sources.push_back(found->second);
    }
    if(complete)save(std::move(g));
  }
  return out;
}
struct Expr {
  int32_t variable=0; // leaf when nonzero
  bool is_and=true,invert=false;
  Clause signs;
  std::vector<std::shared_ptr<const Expr>> children;
};
struct Cut {
  int32_t output=0;
  Poly polynomial;
  Term leaves;
  std::shared_ptr<const Expr> expression;
};
inline bool eval_expression(const Expr &e,const Term &leaves,uint64_t assignment) {
  if(e.variable) {
    auto at=std::lower_bound(leaves.begin(),leaves.end(),e.variable);
    if(at==leaves.end()||*at!=e.variable)std::abort();
    return(assignment>>(at-leaves.begin()))&1u;
  }
  bool value=e.is_and;
  for(size_t i=0;i<e.children.size();++i) {
    bool x=eval_expression(*e.children[i],leaves,assignment)^(e.signs[i]<0);
    value=e.is_and ? value&&x:value!=x;
  }
  return value^e.invert;
}
inline bool eval_polynomial(const Poly &p,const Term &leaves,uint64_t assignment) {
  bool value=false;
  for(const auto &term:p) {
    bool product=true;
    for(auto v:term) {
      auto at=std::lower_bound(leaves.begin(),leaves.end(),v);
      if(at==leaves.end()||*at!=v)std::abort();
      product&=(assignment>>(at-leaves.begin()))&1u;
    }
    value^=product;
  }
  return value;
}
inline bool truth_check(const Cut &cut,Work &work) {
  if(cut.leaves.size()>16||!cut.expression)return false;
  for(uint64_t a=0;a<(uint64_t(1)<<cut.leaves.size());++a) {
    if(!work.take(1+cut.polynomial.size()))return false;
    if(eval_expression(*cut.expression,cut.leaves,a)!=eval_polynomial(cut.polynomial,cut.leaves,a))return false;
  }
  return true;
}
struct Group {Poly shared;std::vector<Cut> consumers;uint64_t benefit=0;};
struct Discovery {
  uint64_t gates=0,cuts=0,novel_candidates=0,truth_checks=0;
  std::vector<Group> selected;
  std::map<Term,int32_t> monomials;
  std::map<Term,Cut> monomial_sources;
};
class Discoverer {
  const std::map<int32_t,Gate> &gates_;
  const Limits &limits_;
  Work &work_;
  std::map<std::pair<int32_t,unsigned>,Cut> memo_;
  Cut leaf(int32_t v) {
    auto e=std::make_shared<Expr>();e->variable=v;return{v,{{v}},{v},std::move(e)};
  }
  Cut expand(int32_t v,unsigned depth) {
    auto key=std::pair(v,depth);auto found=memo_.find(key);if(found!=memo_.end())return found->second;
    auto g=gates_.find(v);Cut out=leaf(v);
    if(!depth||g==gates_.end()||!work_.take())return out;
    auto e=std::make_shared<Expr>();e->is_and=g->second.is_and;e->invert=g->second.output<0;
    Poly p=e->is_and ? Poly{Term{}}:Poly{};Term leaves;
    bool valid=true;
    for(auto literal:g->second.inputs) {
      Cut input=expand(std::abs(literal),depth-1);Term joined;
      std::set_union(leaves.begin(),leaves.end(),input.leaves.begin(),input.leaves.end(),std::back_inserter(joined));leaves=std::move(joined);
      if(leaves.size()>limits_.leaves){valid=false;break;}
      Poly term=literal<0 ? negate(input.polynomial):input.polynomial;
      if(e->is_and){Poly next;if(!multiply(p,term,next,work_,limits_.terms)){valid=false;break;}p=std::move(next);}
      else p=add(p,term);
      if(p.size()>limits_.terms){valid=false;break;}
      e->signs.push_back(literal);e->children.push_back(input.expression);
    }
    if(valid&&!work_.exhausted) {
      if(e->invert)p=negate(p);
      if(p.size()<=limits_.terms)out={v,std::move(p),std::move(leaves),std::move(e)};
    }
    if(memo_.size()<limits_.cuts)memo_.emplace(key,out);
    return out;
  }
public:
  Discoverer(const std::map<int32_t,Gate> &gates,const Limits &limits,Work &work):gates_(gates),limits_(limits),work_(work){}
  Discovery run() {
    Discovery result;result.gates=gates_.size();
    std::vector<Cut> cuts;std::set<std::pair<int32_t,Poly>> seen;
    std::set<Poly> existing;
    for(const auto &[v,g]:gates_)for(unsigned depth=1;depth<=limits_.depth;++depth) {
      (void)g;if(!work_.take()||cuts.size()>=limits_.cuts)break;
      Cut cut=expand(v,depth);
      if(std::binary_search(cut.leaves.begin(),cut.leaves.end(),v)||!seen.emplace(v,cut.polynomial).second)continue;
      existing.insert(cut.polynomial);existing.insert(negate(cut.polynomial));
      auto positive=cut.polynomial;bool inverted=false;
      if(!positive.empty()&&positive.front().empty()){positive.erase(positive.begin());inverted=true;}
      if(positive.size()==1&&positive.front().size()>=2) {
        if(result.monomials.emplace(positive.front(),inverted ? -v:v).second)
          result.monomial_sources.emplace(positive.front(),cut);
      }
      cuts.push_back(std::move(cut));
    }
    result.cuts=cuts.size();
    std::map<Poly,std::map<int32_t,size_t>> candidates;
    for(size_t i=0;i<cuts.size();++i) {
      if(!work_.take(cuts[i].polynomial.size()+1))break;
      Poly nonlinear;
      for(const auto &term:cuts[i].polynomial)if(term.size()>=2) {
        nonlinear.push_back(term);const Poly single{term};
        if(!existing.count(single))candidates[single].emplace(cuts[i].output,i);
      }
      if(nonlinear.size()>=2&&!existing.count(nonlinear))candidates[nonlinear].emplace(cuts[i].output,i);
    }
    std::vector<Group> eligible;
    for(const auto &[expression,consumers]:candidates) {
      if(consumers.size()<2)continue;
      uint64_t cost=expression.size()-1;for(const auto &term:expression)cost+=term.size()-1;
      Group group;group.shared=expression;group.benefit=(consumers.size()-1)*cost;
      for(auto [v,i]:consumers) {
        (void)v;if(group.consumers.size()>=limits_.consumers)break;
        group.consumers.push_back(cuts[i]);
      }
      eligible.push_back(std::move(group));
    }
    result.novel_candidates=eligible.size();
    std::sort(eligible.begin(),eligible.end(),[](const Group &a,const Group &b){return a.benefit!=b.benefit ? a.benefit>b.benefit:a.shared<b.shared;});
    std::set<int32_t> covered;
    for(auto &group:eligible) {
      if(result.selected.size()>=limits_.groups||work_.exhausted)break;
      std::erase_if(group.consumers,[&](const Cut &c){return covered.count(c.output);});
      if(group.consumers.size()<2)continue;
      bool valid=true;for(const auto &cut:group.consumers){++result.truth_checks;if(!truth_check(cut,work_)){valid=false;break;}}
      if(!valid)continue;
      for(const auto &cut:group.consumers)covered.insert(cut.output);
      result.selected.push_back(std::move(group));
    }
    return result;
  }
};
inline Discovery discover(const std::vector<Clause> &formula,const Limits &limits,Work &work) {
  auto gates=authenticate(formula,work);return Discoverer(gates,limits,work).run();
}
} // namespace shared_anf
