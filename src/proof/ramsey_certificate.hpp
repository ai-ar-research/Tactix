// Complete Ramsey proof composition over an already certified complete signed
// K4 encoding. Recognition and original-database admission belong to the caller.
#pragma once
#include "proof/cardinality_certificate.hpp"
#include <numeric>
#include <set>
namespace tactix::ramsey_certificate {
using Clause = cardinality_certificate::Clause;
using Graph = std::vector<std::vector<int32_t>>;
enum class Phase : uint8_t { StarOrder, ColorChoice, Neighborhood,
  ReduceOriginal, DegreeClauses, CountDegree, Contradiction };
// In addition to the counting sink contract, the caller supplies symmetry,
// erase, phase and inequality callbacks. symmetry owns physical DSR accounting
// and any fresh serializer helper reservation in both preflight and emission.
// No kernel state or output stream is owned here. All premises and mappings must
// be validated against the original database before calling this composition.
// A failed preflight is discarded; a committed certificate must run atomically.
template <class Sink>
bool subsets(const std::vector<int32_t> &literals,size_t need,size_t at,
             Clause &partial,std::vector<Clause> &out,Sink &sink) {
  if(sink.stopped || !sink.charge_work(1)){sink.stopped=true;return false;}
  if(!need){out.push_back(partial);return true;}
  for(size_t i=at;i+need<=literals.size();++i) {
    partial.push_back(literals[i]);
    if(!subsets(literals,need-1,i+1,partial,out,sink))return false;
    partial.pop_back();
  }
  return true;
}
template <class Sink>
bool derive(const Graph &global,const std::vector<Clause> &original,
            uint32_t variables,Sink &sink) {
  const auto fail=[&](){sink.stopped=true;return false;};
  if(sink.stopped || !sink.charge_work(1))return fail();
  const size_t original_n=global.size();
  if(original_n<18 || original_n>UINT32_MAX || variables>=INT32_MAX)return fail();
  // The certified graph guarantees distinct, nonzero signed DIMACS identities.
  // Keep dimensional mistakes from becoming out-of-range accesses in adapters.
  if(original_n*(original_n-1)/2>variables)return fail();
  for(const auto &row:global)if(row.size()!=original_n)return fail();
  std::set<int32_t> identities;
  for(uint32_t a=0;a<original_n;++a) {
    if(global[a][a])return fail();
    for(uint32_t b=a+1;b<original_n;++b) {
      if(!sink.charge_work(1))return fail();
      const int32_t edge=global[a][b];
      if(!edge || edge==INT32_MIN || global[b][a]!=edge ||
          uint32_t(std::abs(edge))>variables ||
          !identities.insert(std::abs(edge)).second)return fail();
    }
  }
  const uint32_t n=9,degree=3;
  std::vector<std::vector<int32_t>> edges(n,std::vector<int32_t>(n));
  for(uint32_t a=0;a<n;++a)for(uint32_t b=a+1;b<n;++b)
    edges[a][b]=edges[b][a]=global[a+1][b+1];
  std::vector<std::vector<Clause>> groups;
  for(uint32_t v=0;v<n;++v)for(bool lower:{true,false}) {
    std::vector<int32_t> incident;for(uint32_t u=0;u<n;++u)if(u!=v)incident.push_back(lower?edges[v][u]:-edges[v][u]);
    std::vector<Clause> clauses;Clause partial;
    if(!subsets(incident,lower?n-degree:degree+1,0,partial,clauses,sink))return fail();
    groups.push_back(std::move(clauses));
  }
  const auto symmetry=[&](const Clause &clause,const std::vector<uint32_t> &permutation,bool complement,bool propagates) {
    std::vector<std::pair<int32_t,int32_t>> witness;
    for(uint32_t a=0;a<original_n;++a)for(uint32_t b=a+1;b<original_n;++b) {
      if(!sink.charge_work(1))return false;
      const int32_t from=global[a][b],to=(complement?-1:1)*global[permutation[a]][permutation[b]];
      const int32_t positive_from=std::abs(from);
      const int32_t positive_to=from<0?-to:to;
      if(positive_from!=positive_to)witness.push_back({positive_from,positive_to});
    }
    return sink.symmetry(clause,witness,propagates);
  };
  // Adjacent prefix rotations sort a 17-edge star. Every permutation covers
  // the entire original formula, including vertices outside this star.
  if(!sink.phase(Phase::StarOrder,0))return fail();
  for(uint32_t position=1;position<17;++position) {
    std::vector<uint32_t> permutation(original_n);std::iota(permutation.begin(),permutation.end(),0);
    permutation[1]=position+1;
    for(uint32_t j=2;j<=position+1;++j)permutation[j]=j-1;
    if(!symmetry({global[0][position],-global[0][position+1]},permutation,false,false))return fail();
  }
  std::vector<uint32_t> complement_reverse(original_n);std::iota(complement_reverse.begin(),complement_reverse.end(),0);
  for(uint32_t j=1;j<=17;++j)complement_reverse[j]=18-j;
  if(!sink.phase(Phase::ColorChoice,0))return fail();
  if(!symmetry({global[0][9]},complement_reverse,true,true))return fail();
  cardinality_certificate::BddProof<Sink> bdd(sink);
  if(!sink.phase(Phase::Neighborhood,0))return fail();
  for(uint32_t v=1;v<=9;++v)if(!sink.add({global[0][v]},false))return fail();
  for(uint32_t a=0;a<n;++a)for(uint32_t b=a+1;b<n;++b)for(uint32_t c=b+1;c<n;++c)
    if(!sink.add({-edges[a][b],-edges[a][c],-edges[b][c]},false))return fail();
  // The full original database was present for every witnessed symmetry step.
  // With all negative triangles derived, keep exactly the positive K4 clauses
  // on the selected nine vertices. Deletion weakens the proof database and
  // cannot introduce a premise; the remaining R34 derivation refutes it.
  std::set<Clause> retained_clauses;
  for(uint32_t a=0;a<n;++a)for(uint32_t b=a+1;b<n;++b)
    for(uint32_t c=b+1;c<n;++c)for(uint32_t d=c+1;d<n;++d) {
      Clause clause{edges[a][b],edges[a][c],edges[a][d],edges[b][c],edges[b][d],edges[c][d]};
      std::sort(clause.begin(),clause.end());retained_clauses.insert(std::move(clause));
    }
  if(!sink.phase(Phase::ReduceOriginal,0))return fail();
  uint64_t deleted=0, retained=0;
  for(const auto &clause:original) {
    if(!sink.charge_work(1))return fail();
    const bool keep=retained_clauses.count(clause);
    if(keep){++retained;continue;}
    if(!sink.erase(clause))return fail();
    ++deleted;
  }
  if(retained!=126 || deleted+retained!=original.size())return fail();
  // Derive each degree clause, before using the generic sum certificate.
  // Four red neighbors contradict a red triangle or the blue four-clique.
  // Six blue neighbors induce the conditional R(3,3) contradiction.
  for(uint32_t v=0;v<n;++v) {
    if(!sink.phase(Phase::DegreeClauses,v))return fail();
    for(const auto &clause:groups[2*v+1])if(!sink.add(clause,false))return fail();
    std::vector<int32_t> neighbors;for(uint32_t u=0;u<n;++u)if(u!=v)neighbors.push_back(int32_t(u));
    std::vector<Clause> six_sets;Clause partial;if(!subsets(neighbors,6,0,partial,six_sets,sink))return fail();
    for(const auto &six:six_sets) {
      Clause context;for(int32_t u:six)context.push_back(edges[v][u]);
      const int32_t pivot=six.front();
      std::vector<int32_t> others(six.begin()+1,six.end());
      std::vector<Clause> triples;if(!subsets(others,3,0,partial,triples,sink))return fail();

      for(int32_t sign:{1,-1})for(const auto &triple:triples) {
        Clause clause=context;for(int32_t u:triple)clause.push_back(sign*edges[pivot][u]);
        // Negating the context and the three incident literals propagates
        // a monochromatic triangle, checked independently as RUP.
        if(!sink.add(clause,false))return fail();

      }
      // Resolve the tiny pigeonhole contradiction directly. Each leaf has
      // three same-colored incident edges, contradicting one of the twenty
      // already certified context clauses. Emit only internal resolvents.
      std::vector<int32_t> assumptions;
      const auto resolve=[&](auto &&self,size_t at,uint32_t red,uint32_t blue)->bool {
        if(!sink.charge_work(1))return false;
        if(red>=3 || blue>=3)return true;
        assert(at<others.size());
        const int32_t literal=edges[pivot][others[at]];
        assumptions.push_back(literal);
        if(!self(self,at+1,red+1,blue))return false;
        assumptions.back()=-literal;
        if(!self(self,at+1,red,blue+1))return false;
        assumptions.pop_back();
        Clause consequence=context;
        for(int32_t assumption:assumptions)consequence.push_back(-assumption);
        return sink.add(consequence,false);
      };
      if(!resolve(resolve,0,0,0) || sink.stopped)return fail();
    }
  }
  uint32_t totals[2]={bdd.truth(),bdd.truth()};
  int64_t accumulated_bounds[2]={0,0};
  std::vector<int64_t> coefficients[2]={std::vector<int64_t>(variables+1),std::vector<int64_t>(variables+1)};
  for(uint32_t v=0;v<n;++v)for(uint32_t side=0;side<2;++side) {
    if(!sink.phase(Phase::CountDegree,2*v+side))return fail();
    const int64_t sign=side?-1:1;
    std::vector<uint32_t> roots;
    for(const auto &clause:groups[2*v+side]) {
      uint32_t root=0;if(!bdd.validate_clause(clause,root) || sink.stopped)return fail();
      roots.push_back(root);
    }
    const auto conjunction=bdd.conjunction(std::move(roots));
    std::vector<std::pair<int32_t,int64_t>> local;
    int64_t bound=sign*degree;
    for(uint32_t u=0;u<n;++u)if(u!=v) {
      const int32_t literal=edges[v][u], variable=std::abs(literal);
      const int64_t coefficient=literal<0?-sign:sign;
      if(literal<0)bound-=sign; // sign*(1-x) contributes this constant.
      local.emplace_back(variable,coefficient);coefficients[side][variable]+=coefficient;
    }
    accumulated_bounds[side]+=bound;
    std::sort(local.begin(),local.end());
    const auto degree_root=bdd.build_constraint(local,bound);
    if(!bdd.imply_and_validate(conjunction,bdd.truth(),degree_root) || sink.stopped)return fail();
    if(!sink.inequality(local,bound,bdd.root_literal(degree_root),false))return fail();
    std::vector<std::pair<int32_t,int64_t>> sum;
    for(uint32_t variable=1;variable<=variables;++variable)if(coefficients[side][variable])sum.emplace_back(variable,coefficients[side][variable]);
    const auto sum_root=bdd.build_constraint(sum,accumulated_bounds[side]);
    if(!bdd.imply_and_validate(totals[side],degree_root,sum_root) || sink.stopped)return fail();
    if(!sink.inequality(sum,accumulated_bounds[side],bdd.root_literal(sum_root),true))return fail();
    totals[side]=sum_root;
  }
  if(!sink.phase(Phase::Contradiction,0))return fail();
  if(!bdd.imply_and_validate(totals[0],totals[1],bdd.falsity()) || sink.stopped)return fail();
  return !sink.stopped;
}
} // namespace tactix::ramsey_certificate
