// Isolated use of Tactix's existing BDD proof engine. No new production rule.
#undef NDEBUG
#include "rules/lucky/signed_cardinality_proof.cpp"
#include "frontend/parse.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <iostream>

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
  const uint32_t n=uint32_t(std::stoul(argv[2])), degree=uint32_t(std::stoul(argv[3]));
  if(n<3 || n>19 || degree>=n || !(uint64_t(n)*degree%2))return 2;
  std::vector<std::vector<int32_t>> edges(n,std::vector<int32_t>(n));
  uint32_t variables=0;
  for(uint32_t a=0;a<n;++a)for(uint32_t b=a+1;b<n;++b)edges[a][b]=edges[b][a]=++variables;
  std::vector<std::vector<ProbeClause>> groups;
  uint64_t clause_count=0;
  for(uint32_t v=0;v<n;++v)for(bool lower:{true,false}) {
    std::vector<int32_t> incident;for(uint32_t u=0;u<n;++u)if(u!=v)incident.push_back(lower?edges[v][u]:-edges[v][u]);
    std::vector<ProbeClause> clauses;ProbeClause partial;
    subsets(incident,lower?n-degree:degree+1,0,partial,clauses);
    clause_count+=clauses.size();groups.push_back(std::move(clauses));
  }
  const std::string prefix=argv[1];
  {
    std::ofstream out(prefix+".cnf");out<<"p cnf "<<variables<<' '<<clause_count<<'\n';
    for(const auto &group:groups)for(const auto &clause:group){for(int32_t lit:clause)out<<lit<<' ';out<<"0\n";}
  }
  Kernel kernel;FILE *input=std::fopen((prefix+".cnf").c_str(),"r");assert(input);
  assert(!parse_dimacs(input,kernel));std::fclose(input);
  Proof proof;assert(proof.open((prefix+".dsr").c_str(),ProofFormat::DsrAscii));kernel.proof=&proof;
  strategy::SignedCardinalityProofParameters parameters;
  parameters.maximum_bdd_nodes=1000000;parameters.maximum_proof_additions=10000000;
  Sink sink(kernel,true,parameters);BddProof bdd(sink);
  const auto started=std::chrono::steady_clock::now();
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
  std::cout<<"{\"vertices\":"<<n<<",\"degree\":"<<degree<<",\"variables\":"<<variables
           <<",\"clauses\":"<<clause_count<<",\"bdd_nodes\":"<<sink.nodes
           <<",\"proof_additions\":"<<sink.additions<<",\"seconds\":"<<seconds<<"}\n";
}
