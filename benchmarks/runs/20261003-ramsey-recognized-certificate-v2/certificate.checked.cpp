// Isolated recognized/signed Ramsey certificate. No production rule or policy.
#undef NDEBUG
#include "rules/lucky/signed_cardinality_proof.cpp"
#include "frontend/parse.hpp"
#include "recognize.hpp"
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
  if(argc!=3)return 2;
  const uint32_t n=9, degree=3;
  const std::string prefix=argv[1];
  { std::ifstream in(argv[2],std::ios::binary);std::ofstream out(prefix+".cnf",std::ios::binary);
    if(!in || !out)return 2;out<<in.rdbuf(); }
  Kernel kernel;FILE *input=std::fopen((prefix+".cnf").c_str(),"r");assert(input);
  assert(!parse_dimacs(input,kernel));std::fclose(input);
  if(kernel.input_unit_clause_count || kernel.input_binary_clause_count || !kernel.startup_certificate_open)return 2;
  std::vector<ProbeClause> original;
  for(tactix::Ref ref=0;ref<kernel.arena.size();) {
    const auto *clause=clause_at(kernel.arena,ref);
    if(!clause->garbage()) {
      ProbeClause row;
      for(Lit lit:*clause) {const int32_t external=int32_t(kernel.internal_to_external[var_of(lit)]+1);row.push_back(negated(lit)?-external:external);}
      std::sort(row.begin(),row.end());original.push_back(std::move(row));
    }
    ref+=clause_actual_words(clause);
  }
  const auto recovered=ramsey_recovery::recognize(original,{10000,200,40000,10000000});
  if(!recovered.recognized)return 2;
  const auto &global=recovered.edges;
  const uint32_t original_n=uint32_t(global.size());
  if(original_n<18 || original_n>19)return 2; // Declared feasibility scope.
  const uint32_t variables=uint32_t(kernel.external_to_internal.size());
  std::vector<std::vector<int32_t>> edges(n,std::vector<int32_t>(n));
  for(uint32_t a=0;a<n;++a)for(uint32_t b=a+1;b<n;++b)
    edges[a][b]=edges[b][a]=global[a+1][b+1];
  std::vector<std::vector<ProbeClause>> groups;
  for(uint32_t v=0;v<n;++v)for(bool lower:{true,false}) {
    std::vector<int32_t> incident;for(uint32_t u=0;u<n;++u)if(u!=v)incident.push_back(lower?edges[v][u]:-edges[v][u]);
    std::vector<ProbeClause> clauses;ProbeClause partial;
    subsets(incident,lower?n-degree:degree+1,0,partial,clauses);
    groups.push_back(std::move(clauses));
  }
  // Recognition provenance; certificate-event semantics remain a migration task.
  std::ofstream provenance(prefix+".recognition.jsonl");if(!provenance)return 2;
  provenance<<"{\"kind\":\"recognition\",\"vertices\":"<<original_n<<",\"work\":"<<recovered.work<<"}\n";
  for(uint32_t a=0;a<original_n;++a)for(uint32_t b=a+1;b<original_n;++b)
    provenance<<"{\"kind\":\"edge\",\"a\":"<<a<<",\"b\":"<<b<<",\"literal\":"<<global[a][b]<<"}\n";
  for(const auto &source:recovered.sources) {
    provenance<<"{\"kind\":\"k4\",\"vertices\":[";
    for(uint32_t i=0;i<4;++i)provenance<<(i?",":"")<<source.vertices[i];
    provenance<<"],\"positive_source\":"<<source.positive_clause<<",\"negative_source\":"<<source.negative_clause<<"}\n";
  }
  provenance.flush();if(!provenance)return 2;
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
      const int32_t positive_from=std::abs(from);
      const int32_t positive_to=from<0?-to:to;
      if(positive_from!=positive_to)witness.push_back({internal(positive_from),internal(positive_to)});
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
  // The full original database was present for every witnessed symmetry step.
  // With all negative triangles derived, keep exactly the positive K4 clauses
  // on the selected nine vertices. Deletion weakens the proof database and
  // cannot introduce a premise; the remaining R34 derivation refutes it.
  std::set<ProbeClause> retained_clauses;
  for(uint32_t a=0;a<n;++a)for(uint32_t b=a+1;b<n;++b)
    for(uint32_t c=b+1;c<n;++c)for(uint32_t d=c+1;d<n;++d) {
      ProbeClause clause{edges[a][b],edges[a][c],edges[a][d],edges[b][c],edges[b][d],edges[c][d]};
      std::sort(clause.begin(),clause.end());retained_clauses.insert(std::move(clause));
    }
  uint64_t deleted=0, retained=0;
  for(const auto &clause:original) {
    const bool keep=retained_clauses.count(clause);
    if(keep){++retained;continue;}
    if(!proof.del_external(++kernel.next_clause_id,0,clause.data(),uint32_t(clause.size())))return 3;
    ++deleted;
  }
  assert(retained==126 && deleted+retained==original.size());
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

      for(int32_t sign:{1,-1})for(const auto &triple:triples) {
        ProbeClause clause=context;for(int32_t u:triple)clause.push_back(sign*edges[pivot][u]);
        // Negating the context and the three incident literals propagates
        // a monochromatic triangle, checked independently as RUP.
        if(!sink.add(clause,false))return 3;

      }
      // Resolve the tiny pigeonhole contradiction directly. Each leaf has
      // three same-colored incident edges, contradicting one of the twenty
      // already certified context clauses. Emit only internal resolvents.
      std::vector<int32_t> assumptions;
      const auto resolve=[&](auto &&self,size_t at,uint32_t red,uint32_t blue)->bool {
        if(red>=3 || blue>=3)return true;
        assert(at<others.size());
        const int32_t literal=edges[pivot][others[at]];
        assumptions.push_back(literal);
        if(!self(self,at+1,red+1,blue))return false;
        assumptions.back()=-literal;
        if(!self(self,at+1,red,blue+1))return false;
        assumptions.pop_back();
        ProbeClause consequence=context;
        for(int32_t assumption:assumptions)consequence.push_back(-assumption);
        return sink.add(consequence,false);
      };
      if(!resolve(resolve,0,0,0) || sink.stopped)return 3;
    }
  }
  uint32_t totals[2]={bdd.truth(),bdd.truth()};
  int64_t accumulated_bounds[2]={0,0};
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
    if(!bdd.imply_and_validate(conjunction,bdd.truth(),degree_root) || sink.stopped)return 3;
    std::vector<std::pair<int32_t,int64_t>> sum;
    for(uint32_t variable=1;variable<=variables;++variable)if(coefficients[side][variable])sum.emplace_back(variable,coefficients[side][variable]);
    const auto sum_root=bdd.build_constraint(sum,accumulated_bounds[side]);
    if(!bdd.imply_and_validate(totals[side],degree_root,sum_root) || sink.stopped)return 3;
    totals[side]=sum_root;
  }
  if(!bdd.imply_and_validate(totals[0],totals[1],bdd.falsity()) || sink.stopped)return 3;
  const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
  kernel.proof=nullptr;
  std::cout<<"{\"vertices\":"<<original_n<<",\"degree\":"<<degree<<",\"variables\":"<<variables
           <<",\"clauses\":"<<original.size()<<",\"deleted_original_clauses\":"<<deleted<<",\"retained_original_clauses\":"<<retained<<",\"bdd_nodes\":"<<sink.nodes
           <<",\"proof_additions\":"<<sink.additions<<",\"seconds\":"<<seconds<<"}\n";
}
