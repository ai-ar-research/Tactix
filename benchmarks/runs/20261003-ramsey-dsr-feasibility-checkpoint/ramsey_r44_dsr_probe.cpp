// Isolated use of Tactix's existing BDD proof engine. No new production rule.
#undef NDEBUG
#include "rules/lucky/signed_cardinality_proof.cpp"
#include "frontend/parse.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <iostream>
#include <set>

using namespace tactix;
using ProbeClause = std::vector<int32_t>;

static void subsets(const std::vector<int32_t> &literals, size_t need,
                    size_t at, ProbeClause &partial, std::vector<ProbeClause> &out) {
  if (!need) { out.push_back(partial); return; }
  for(size_t i=at;i+need<=literals.size();++i) {
    partial.push_back(literals[i]);subsets(literals,need-1,i+1,partial,out);partial.pop_back();
  }
}
int main(int argc,char **argv) {
  if(argc!=4)return 2;
  const uint32_t original_n=uint32_t(std::stoul(argv[2])), n=9, degree=3;
  if(original_n<18 || original_n>19)return 2; // Bounded feasibility, not rule guard.
  std::vector<std::vector<int32_t>> global(original_n,std::vector<int32_t>(original_n));
  uint32_t variables=0;
  for(uint32_t a=0;a<original_n;++a)for(uint32_t b=a+1;b<original_n;++b)
    global[a][b]=global[b][a]=++variables;
  std::vector<std::vector<int32_t>> edges(n,std::vector<int32_t>(n));
  for(uint32_t a=0;a<n;++a)for(uint32_t b=a+1;b<n;++b)
    edges[a][b]=edges[b][a]=global[a+1][b+1];
  std::vector<std::vector<ProbeClause>> groups;
  uint64_t clause_count=0;
  for(uint32_t v=0;v<n;++v)for(bool lower:{true,false}) {
    std::vector<int32_t> incident;for(uint32_t u=0;u<n;++u)if(u!=v)incident.push_back(lower?edges[v][u]:-edges[v][u]);
    std::vector<ProbeClause> clauses;ProbeClause partial;
    subsets(incident,lower?n-degree:degree+1,0,partial,clauses);
    clause_count+=clauses.size();groups.push_back(std::move(clauses));
  }
  // Recover the complete signed K4 clause set, using no filename dispatch.
  std::vector<ProbeClause> original;
  std::vector<int32_t> vertices(original_n);std::iota(vertices.begin(),vertices.end(),0);
  std::vector<ProbeClause> sets;ProbeClause selected;subsets(vertices,4,0,selected,sets);
  for(int32_t sign:{1,-1})for(const auto &set:sets) {
    ProbeClause clause;
    for(size_t a=0;a<set.size();++a)for(size_t b=a+1;b<set.size();++b)
      clause.push_back(sign*global[set[a]][set[b]]);
    std::sort(clause.begin(),clause.end());original.push_back(std::move(clause));
  }
  std::sort(original.begin(),original.end());
  const std::string prefix=argv[1];
  { std::ifstream in(argv[3],std::ios::binary);std::ofstream out(prefix+".cnf",std::ios::binary);
    if(!in)return 2;out<<in.rdbuf(); }
  Kernel kernel;FILE *input=std::fopen((prefix+".cnf").c_str(),"r");assert(input);
  assert(!parse_dimacs(input,kernel));std::fclose(input);
  if(kernel.nvars!=variables || kernel.input_unit_clause_count || kernel.input_binary_clause_count)return 2;
  std::vector<ProbeClause> found;
  for(tactix::Ref ref=0;ref<kernel.arena.size();) {
    const auto *clause=clause_at(kernel.arena,ref);
    if(!clause->garbage()) {
      ProbeClause row;
      for(Lit lit:*clause) {const int32_t external=int32_t(kernel.internal_to_external[var_of(lit)]+1);row.push_back(negated(lit)?-external:external);}
      std::sort(row.begin(),row.end());found.push_back(std::move(row));
    }
    ref+=clause_actual_words(clause);
  }
  std::sort(found.begin(),found.end());if(found!=original)return 2;
  Proof proof;assert(proof.open((prefix+".dsr").c_str(),ProofFormat::DsrAscii));kernel.proof=&proof;
  strategy::SignedCardinalityProofParameters parameters;
  parameters.maximum_bdd_nodes=1000000;parameters.maximum_proof_additions=10000000;
  const auto started=std::chrono::steady_clock::now();
  const auto internal=[](int32_t external){return make_lit(uint32_t(std::abs(external)-1),external<0);};
  const auto symmetry=[&](const ProbeClause &clause,const std::vector<uint32_t> &permutation,bool complement,bool propagates) {
    std::vector<Lit> literals;for(int32_t lit:clause)literals.push_back(internal(lit));
    std::vector<DsrLiteralMapping> witness;
    for(uint32_t a=0;a<original_n;++a)for(uint32_t b=a+1;b<original_n;++b) {
      const int32_t from=global[a][b],to=(complement?-1:1)*global[permutation[a]][permutation[b]];
      if(from!=to)witness.push_back({internal(from),internal(to)});
    }
    return proof.add_sr(++kernel.next_clause_id,literals.data(),uint32_t(literals.size()),nullptr,0,
      witness.data(),uint32_t(witness.size()),variables,propagates);
  };
  // Adjacent prefix rotations sort a 17-edge star. Every permutation covers
  // the entire original formula, including vertices outside this star.
  for(uint32_t position=1;position<17;++position) {
    std::vector<uint32_t> permutation(original_n);std::iota(permutation.begin(),permutation.end(),0);
    permutation[1]=position+1;
    for(uint32_t j=2;j<=position+1;++j)permutation[j]=j-1;
    if(!symmetry({global[0][position],-global[0][position+1]},permutation,false,false))return 3;
  }
  std::vector<uint32_t> complement_reverse(original_n);std::iota(complement_reverse.begin(),complement_reverse.end(),0);
  for(uint32_t j=1;j<=17;++j)complement_reverse[j]=18-j;
  if(!symmetry({global[0][9]},complement_reverse,true,true))return 3;
  // The witnessed-unit helper deletes both temporary auxiliary clauses before
  // BDD allocation; reserve that identity anyway in this feasibility driver.
  Sink sink(kernel,true,parameters);++sink.fresh;BddProof bdd(sink);
  for(uint32_t v=1;v<=9;++v)if(!sink.add({global[0][v]},false))return 3;
  for(uint32_t a=0;a<n;++a)for(uint32_t b=a+1;b<n;++b)for(uint32_t c=b+1;c<n;++c)
    if(!sink.add({-edges[a][b],-edges[a][c],-edges[b][c]},false))return 3;
  // Derive each degree clause, before using the generic sum certificate.
  // Four red neighbors contradict a red triangle or the blue four-clique.
  // Six blue neighbors induce the conditional R(3,3) contradiction.
  for(uint32_t v=0;v<n;++v) {
    for(const auto &clause:groups[2*v+1])if(!sink.add(clause,false))return 3;
    std::vector<int32_t> neighbors;for(uint32_t u=0;u<n;++u)if(u!=v)neighbors.push_back(int32_t(u));
    std::vector<ProbeClause> six_sets;ProbeClause partial;subsets(neighbors,6,0,partial,six_sets);
    for(const auto &six:six_sets) {
      ProbeClause context;for(int32_t u:six)context.push_back(edges[v][u]);
      const int32_t pivot=six.front();
      std::vector<int32_t> others(six.begin()+1,six.end());
      std::vector<ProbeClause> triples;subsets(others,3,0,partial,triples);
      std::vector<uint32_t> conditional_roots;
      for(int32_t sign:{1,-1})for(const auto &triple:triples) {
        ProbeClause clause=context;for(int32_t u:triple)clause.push_back(sign*edges[pivot][u]);
        // Negating the context and the three incident literals propagates
        // a monochromatic triangle, checked independently as RUP.
        if(!sink.add(clause,false))return 3;
        uint32_t root=0;if(!bdd.validate_clause(clause,root) || sink.stopped)return 3;
        conditional_roots.push_back(root);
      }
      const auto antecedent=bdd.conjunction(std::move(conditional_roots));
      const auto consequent=bdd.build_clause(context);
      if(!bdd.imply_and_validate(antecedent,bdd.truth(),consequent) ||
         !sink.add(context,false) || sink.stopped)return 3;
    }
  }
  uint32_t totals[2]={bdd.truth(),bdd.truth()};
  std::vector<int64_t> coefficients[2]={std::vector<int64_t>(variables+1),std::vector<int64_t>(variables+1)};
  for(uint32_t v=0;v<n;++v)for(uint32_t side=0;side<2;++side) {
    const int64_t sign=side?-1:1;
    std::vector<uint32_t> roots;
    for(const auto &clause:groups[2*v+side]) {
      uint32_t root=0;if(!bdd.validate_clause(clause,root) || sink.stopped)return 3;
      roots.push_back(root);
    }
    const auto conjunction=bdd.conjunction(std::move(roots));
    std::vector<std::pair<int32_t,int64_t>> local;
    for(uint32_t u=0;u<n;++u)if(u!=v){local.emplace_back(edges[v][u],sign);coefficients[side][edges[v][u]]+=sign;}
    std::sort(local.begin(),local.end());
    const auto degree_root=bdd.build_constraint(local,sign*degree);
    if(!bdd.imply_and_validate(conjunction,bdd.truth(),degree_root) || sink.stopped)return 3;
    std::vector<std::pair<int32_t,int64_t>> sum;
    for(uint32_t variable=1;variable<=variables;++variable)if(coefficients[side][variable])sum.emplace_back(variable,coefficients[side][variable]);
    const auto sum_root=bdd.build_constraint(sum,sign*int64_t(degree)*(v+1));
    if(!bdd.imply_and_validate(totals[side],degree_root,sum_root) || sink.stopped)return 3;
    totals[side]=sum_root;
  }
  if(!bdd.imply_and_validate(totals[0],totals[1],bdd.falsity()) || sink.stopped)return 3;
  const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
  kernel.proof=nullptr;
  std::cout<<"{\"vertices\":"<<original_n<<",\"degree\":"<<degree<<",\"variables\":"<<variables
           <<",\"clauses\":"<<original.size()<<",\"bdd_nodes\":"<<sink.nodes
           <<",\"proof_additions\":"<<sink.additions<<",\"seconds\":"<<seconds<<"}\n";
}
