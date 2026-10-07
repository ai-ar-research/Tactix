#undef NDEBUG
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/semantic_extension/affine_image.hpp"
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include <cassert>
#include <fstream>
#include <sstream>
#include <cstdio>
using namespace tactix;

struct Fixture {
  uint32_t variables;
  std::vector<std::vector<int32_t>> clauses;
};
Fixture mixed(uint32_t pigeons,uint32_t holes) {
  Fixture f{pigeons*holes,{}};
  auto cell=[&](uint32_t p,uint32_t h){return int32_t(1+p*holes+h);};
  for(uint32_t p=0;p<pigeons;++p) {
    std::vector<int32_t> cover;
    for(uint32_t h=0;h<holes;++h)cover.push_back(cell(p,h));
    f.clauses.push_back(cover);
  }
  for(uint32_t h=0;h<holes;++h)for(uint32_t p=0;p<pigeons;++p)
    for(uint32_t q=p+1;q<pigeons;++q)f.clauses.push_back({-cell(p,h),-cell(q,h)});
  // Shared original variables, no fresh-variable constructor: elimination of
  // 7/13 derives x1 xor x19 xor x20 = 1; the guard literals remain in reasons.
  for(const auto &variables:{std::vector<int32_t>{1,7,13},std::vector<int32_t>{7,13,19,20}})
    for(auto &clause:affine_semantic_extension::clausal_form(variables,variables.front()==7))f.clauses.push_back(clause);
  f.clauses.push_back({-1,19}); // One-sided CNF link, not another XOR equation.
  // Delayed forced gates defeat the uniform forward/backward Lucky probes.
  // The gadget is satisfiable, has no root unit, and requires ordinary search
  // to justify its gates; both ends precede the auxiliaries in one probe order.
  const int32_t n=f.variables;
  f.variables+=6;
  for (int32_t gate : {n+1,-(n+2),n+5,-(n+6)})
    for (int32_t a : {n+3,-(n+3)}) for (int32_t b : {n+4,-(n+4)})
      f.clauses.push_back({gate,a,b});
  return f;
}
std::string cnf(const Fixture &f) {
  std::ostringstream out;out<<"p cnf "<<f.variables<<' '<<f.clauses.size()<<'\n';
  for(auto &c:f.clauses){for(int32_t x:c)out<<x<<' ';out<<"0\n";}
  return out.str();
}
DefaultStrategyConfig config(bool enabled) {
  DefaultStrategyConfig c;c.affine_search.enabled=enabled;
  c.affine_root_closure.enabled=false;
  c.affine_model.enabled=false;c.parity_model.enabled=false;c.onehot_model.enabled=false;
  c.ramsey_startup.enabled=false;c.signed_cardinality_proof.enabled=false;
  c.balanced_signature_model.enabled=false;
  c.exact_cover_model.enabled=false;c.independent_set_model.enabled=false;
  c.fermat_model.enabled=false;c.cerny_model.enabled=false;
  c.lookup_circuit_model.enabled=false;c.simon_model.enabled=false;c.stconn_proof.enabled=false;
  c.inprocess.program.clear();c.inprocess.append_unlisted_rules=false;
  c.inprocess.structured_root_stages.clear();
  c.inprocess.structured_root_bounded_backtracking_stages.clear();
  c.inprocess.structured_root_large_domain_stages.clear();
  c.inprocess.initial_fast_elimination=false;
  c.rephase.walk_warmup=false;c.rephase.late_lucky=false;
  c.mode.force=1;c.branching.focused_phase_switch_span=1;
  c.branching.focused_phase_pattern={-1}; // General fixed phase arm, no decision script.
  return c;
}
void run(const Fixture &f,bool with_proof,bool enabled,const std::string &stem="",ProofFormat format=ProofFormat::DsrAscii) {
  Kernel kernel;auto text=cnf(f);FILE *input=std::tmpfile();assert(input);
  assert(std::fwrite(text.data(),1,text.size(),input)==text.size());std::rewind(input);
  assert(!parse_dimacs(input,kernel));std::fclose(input);
  AffineMatrixSearchSession session;kernel.affine_matrix_search_session=&session;
  FILE *records=std::tmpfile();assert(records);kernel.structural_provenance=records;
  const std::string suffix=format==ProofFormat::DsrAscii ? ".dsr" : ".drat";
  std::string proof_path=stem.empty()?"/tmp/tactix-default-affine-fixed"+suffix:stem+suffix;
  Proof proof;
  if(with_proof) {
    assert(proof.open(proof_path.c_str(),format));
    proof.bind_variable_map(&kernel.internal_to_external);
    proof.bind_external_variable_map(&kernel.external_to_internal);kernel.proof=&proof;
  }
  DefaultPolicy policy(config(enabled));int status=policy.solve(kernel);
  const auto &stats=policy.affine_search_diagnostics();
  if(f.variables==26) {
    assert(status==10);kernel_extend_model(kernel);
    std::ofstream model;if(!stem.empty())model.open(stem+".model");
    for(auto &c:f.clauses) {
      bool satisfied=false;
      for(int32_t x:c)if(kernel.model_values[internalize(x)]>0)satisfied=true;
      assert(satisfied);
    }
    if(model){model<<"s SATISFIABLE\nv ";for(uint32_t v=1;v<=f.variables;++v)model<<(kernel.model_values[internalize(v)]>0?int(v):-int(v))<<' ';model<<"0\n";}
  } else assert(status==20);
  if(enabled && with_proof) {
    assert(stats.initializations==1 && stats.admitted_sources>=2);
    assert(kernel.stats.decisions>0 && stats.search_services>0);
    assert(stats.reasons_published>0 && stats.widest_reason>=2);
    assert(stats.implications_consumed+stats.conflicts_consumed>0);
    if(f.variables==26)assert(stats.conflicts_consumed>0);
    if(f.variables==36)assert(stats.implications_consumed>0);
    assert(stats.proof_additions>0 && stats.initialization_work>0);
  }
  if(!with_proof)assert(!session.persistent() && stats.reasons_published==0);
  if(!enabled)assert(stats.initializations==0 && stats.search_services==0);
  if(!stem.empty()){std::ofstream out(stem+".cnf");out<<text;}
  session.disable(kernel);
  kernel.proof=nullptr;kernel.structural_provenance=nullptr;std::fclose(records);
  if(stem.empty() && with_proof)std::remove(proof_path.c_str());
}
int main(int argc,char **argv) {
  assert(argc==1 || argc==2);
  Kernel k;kernel_reserve_vars(k,30);auto c=config(true);
  auto p=default_affine_search_parameters(k,c);assert(p && p->valid());
  assert(p->maximum_arity>3 && p->basis_maximum_intermediate_width==30);
  c.affine_search.maximum_retained_bytes=1;assert(!default_affine_search_parameters(k,c));
  c=config(true);assert(c.serialize().find("affine_search.enabled=1")!=std::string::npos);
  assert(!DefaultStrategyConfig{}.affine_search.enabled);
  for(bool sat:{true,false}) {
    auto f=mixed(sat?4:6,5);std::string stem=argc==2?std::string(argv[1])+(sat?"/sat":"/unsat"):"";
    run(f,true,true,stem);
    run(f,true,true,stem.empty()?"":stem+"-drat",ProofFormat::DratAscii);
    run(f,false,true);
    run(f,false,false);
  }
}
