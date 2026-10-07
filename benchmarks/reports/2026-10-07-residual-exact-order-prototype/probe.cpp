#undef NDEBUG
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include <string>
#include "policies/heuristics.hpp"
#include "rules/propagate/propagate.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace tactix;
static void parse(Kernel &k,const char *text) {
  FILE *f=std::tmpfile();assert(f);assert(std::fputs(text,f)>=0);std::rewind(f);
  assert(!parse_dimacs(f,k));std::fclose(f);
}
static std::vector<unsigned> external(const Kernel &k,const std::vector<Var> &a) {
  std::vector<unsigned> out;for(auto v:a)out.push_back(k.internal_to_external[v]+1);return out;
}
static void array(const std::vector<unsigned> &a) {
  std::printf("[");for(size_t i=0;i<a.size();i++)std::printf("%s%u",i?",":"",a[i]);std::printf("]");
}
int main(int argc,char **argv) {
  assert(argc==2);
  unsigned mismatches=0;
  for(unsigned prefix:{1u,3u})for(unsigned first:{1u,3u}) {
    const unsigned second=4-first;
    char original[100],serialized[100];
    std::snprintf(original,sizeof original,"p cnf 5 2\n%u 4 5 0\n%u %u 0\n",prefix,first,second);
    std::snprintf(serialized,sizeof serialized,"p cnf 5 2\n%u %u 0\n4 0\n",first,second);
    Kernel integrated,fresh;parse(integrated,original);parse(fresh,serialized);
    const auto old=external(integrated,integrated.input_activation_order);
    const Var fixed=integrated.external_to_internal[3];
    kernel_assign_unit(integrated,make_lit(fixed,false));
    assert(propagate_apply(integrated)==ApplyResult::Ok);
    assert(!integrated.h&&!fresh.h);
    heuristics_prepare_residual_activation_order(integrated);
    const auto reconstructed=external(integrated,integrated.input_activation_order);
    const auto expected=external(fresh,fresh.input_activation_order);
    assert((expected==std::vector<unsigned>{first,second,4}));
    assert(reconstructed==expected);
    const auto seed=integrated.input_activation_order;
    ScopedHeuristics ih(integrated),fh(fresh);
    heuristics_seed_residual_import_order(integrated,seed);
    assert(external(integrated,integrated.compact_import_order)==reconstructed);
    assert(external(fresh,fresh.compact_import_order)==expected);
    for(const auto *h:{integrated.h,fresh.h})
      for(size_t v=0;v<h->saved.size();v++)assert(!h->saved[v]&&!h->target[v]&&!h->best[v]);
    const unsigned iq=integrated.internal_to_external[heuristics_queue_next_unassigned(integrated)]+1;
    const unsigned fq=fresh.internal_to_external[heuristics_queue_next_unassigned(fresh)]+1;
    assert(iq==second&&fq==second);
    const bool differs=reconstructed!=expected;mismatches+=differs;
    assert(!differs);
    std::printf("{\"prefix\":%u,\"binary_first\":%u,\"old_activation\":",prefix,first);array(old);
    std::printf(",\"reconstructed\":");array(reconstructed);std::printf(",\"fresh_parse\":");array(expected);
    std::printf(",\"integrated_next\":%u,\"fresh_next\":%u,\"phases_all_zero\":true,\"mismatch\":%s}\n",iq,fq,differs?"true":"false");
  }
  // Isolate clause-order loss from binary literal-direction reconstruction.
  for(const auto format:{ProofFormat::DsrBinary,ProofFormat::DratBinary,ProofFormat::Lrat}) {
    const char *name=format==ProofFormat::Lrat?"lrat":format==ProofFormat::DsrBinary?"dsr":"drat";
    Proof proof;assert(proof.open((std::string(argv[1])+"/"+name+".proof").c_str(),format));
    Kernel integrated,fresh;integrated.proof=&proof;
    parse(integrated,"p cnf 8 3\n4 5 8 0\n6 7 8 0\n1 3 0\n");
    parse(fresh,"p cnf 8 3\n6 7 8 0\n1 3 0\n4 0\n");
    const auto id=kernel_binary_id(integrated,internalize(1),internalize(3));
    assert(integrated.binary_ids.tracks_values()==(format==ProofFormat::Lrat));
    assert(id==(format==ProofFormat::Lrat?3u:1u));
    kernel_assign_unit(integrated,internalize(4));assert(propagate_apply(integrated)==ApplyResult::Ok);
    heuristics_prepare_residual_activation_order(integrated);
    const auto reconstructed=external(integrated,integrated.input_activation_order);
    const auto expected=external(fresh,fresh.input_activation_order);
    assert((expected==std::vector<unsigned>{6,7,8,1,3,4}));
    const std::vector<unsigned> collapsed={1,3,6,7,8,4};
    assert(reconstructed==expected);
    const auto seed=integrated.input_activation_order;ScopedHeuristics ih(integrated),fh(fresh);
    heuristics_seed_residual_import_order(integrated,seed);
    assert(external(integrated,integrated.compact_import_order)==reconstructed);
    assert(external(fresh,fresh.compact_import_order)==expected);
    std::printf("{\"proof_mode\":\"%s\",\"binary_id\":%llu,\"source_clause_id\":3,\"reconstructed\":",name,(unsigned long long)id);array(reconstructed);
    std::printf(",\"fresh_parse\":");array(expected);std::printf("}\n");
  }
  {
    Kernel k;parse(k,"p cnf 5000 1\n3 1 0\n");
    const auto original=k.input_activation_order;
    assert(!k.residual_order_diagnostic.complete);
    assert(heuristics_prepare_residual_activation_order(k)==0);
    assert(k.input_activation_order==original);
    std::puts("PASS: incomplete/out-of-envelope capture retains original activation");
  }
  assert(mismatches==0);
  std::printf("PASS: four cases; all exact-order fixtures now match fresh parse; no solve invoked\n");
}
