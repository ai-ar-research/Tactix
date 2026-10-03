// Fixed proof-feasibility controls using the existing native BDD engine.
#undef NDEBUG
#include "rules/lucky/signed_cardinality_proof.cpp"
#include "frontend/parse.hpp"
#include <fstream>
#include <iostream>
using namespace tactix;
using ProbeClause=std::vector<int32_t>;
int main(int argc,char **argv) {
 if(argc!=3)return 2;
 const uint32_t n=std::stoul(argv[2]);if(n<2 || n>117)return 2;
 const std::string prefix=argv[1];const uint32_t variables=n*n;
 std::vector<std::vector<ProbeClause>> rows(n),cols(n);
 for(uint32_t r=0;r<n;++r) {
  if(!r)for(uint32_t c=0;c<n;++c)rows[r].push_back({-int32_t(r*n+c+1)});
  for(uint32_t a=0;a<n;++a)for(uint32_t b=a+1;b<n;++b)
   rows[r].push_back({-int32_t(r*n+a+1),-int32_t(r*n+b+1)});
 }
 for(uint32_t c=0;c<n;++c){ProbeClause cover;for(uint32_t r=0;r<n;++r)cover.push_back(r*n+c+1);cols[c].push_back(cover);}
 size_t count=0;for(auto &g:rows)count+=g.size();for(auto &g:cols)count+=g.size();
 {std::ofstream out(prefix+".cnf");out<<"p cnf "<<variables<<' '<<count<<'\n';for(auto *groups:{&rows,&cols})for(auto &g:*groups)for(auto &cl:g){for(auto lit:cl)out<<lit<<' ';out<<"0\n";}}
 Kernel k;FILE *in=std::fopen((prefix+".cnf").c_str(),"r");assert(in);parse_dimacs(in,k);std::fclose(in);
 // Count-only preflight: no proof stream is opened.
 strategy::SignedCardinalityProofParameters p;p.maximum_bdd_nodes=1000000;p.maximum_proof_additions=10000000;
 Sink sink(k,false,p);BddProof bdd(sink);
 uint32_t completed_blocks=0;
 const auto finish=[&](bool complete){std::cout<<"{\"n\":"<<n<<",\"complete\":"<<(complete?"true":"false")<<",\"stopped\":"<<(sink.stopped?"true":"false")<<",\"completed_blocks\":"<<completed_blocks<<",\"bdd_nodes\":"<<sink.nodes<<",\"additions\":"<<sink.additions<<"}\n";return 0;};uint32_t totals[2]={bdd.truth(),bdd.truth()};
 for(uint32_t side=0;side<2;++side) {
  std::vector<std::pair<int32_t,int64_t>> accumulated;
  int64_t bound=0;
  for(uint32_t i=0;i<n;++i) {
   std::vector<uint32_t> roots;auto &group=side?cols[i]:rows[i];
   for(auto &cl:group){uint32_t ref;if(!bdd.validate_clause(cl,ref)||sink.stopped)return finish(false);roots.push_back(ref);}
   auto joint=bdd.conjunction(std::move(roots));std::vector<std::pair<int32_t,int64_t>> local;
   for(uint32_t j=0;j<n;++j)local.emplace_back(side?j*n+i+1:i*n+j+1,side?1:-1);
   const int64_t local_bound=side?1:(i?-1:0);
   auto constraint=bdd.build_constraint(local,local_bound);
   if(!bdd.imply_and_validate(joint,bdd.truth(),constraint)||sink.stopped)return finish(false);
   accumulated.insert(accumulated.end(),local.begin(),local.end());std::sort(accumulated.begin(),accumulated.end());bound+=local_bound;
   auto sum=bdd.build_constraint(accumulated,bound);
   if(!bdd.imply_and_validate(totals[side],constraint,sum)||sink.stopped)return finish(false);
   totals[side]=sum;++completed_blocks;
  }
 }
 if(!bdd.imply_and_validate(totals[0],totals[1],bdd.falsity())||sink.stopped)return finish(false);
 return finish(true);
}
