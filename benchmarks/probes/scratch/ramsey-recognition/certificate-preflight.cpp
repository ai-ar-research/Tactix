// Isolated recognized/signed Ramsey certificate. No production rule or policy.
#undef NDEBUG
#include "event_sink.hpp"
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "recognize.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <iostream>
#include <set>
#include <filesystem>

using namespace tactix;
using ProbeClause = std::vector<int32_t>;

int main(int argc,char **argv) {
  if(argc!=3)return 2;
  const std::string prefix=argv[1];
  std::error_code alias_error;
  if(std::filesystem::equivalent(argv[2],prefix+".cnf",alias_error))return 2;
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
  const auto next_before=kernel.next_clause_id;
  const auto arena_before=kernel.arena.size(), trail_before=kernel.trail.size();
  const auto untouched=[&](){return kernel.next_clause_id==next_before && kernel.arena.size()==arena_before && kernel.trail.size()==trail_before && kernel.external_to_internal.size()==variables && !kernel.proof && !kernel.inconsistent;};
  const auto derive=[&](RamseyEventSink &sink){return ramsey_certificate::derive(global,original,variables,sink);};
  RamseyEventSink planned(variables,next_before);
  if(!derive(planned)){assert(untouched());return 3;}
  assert(untouched());
  RamseyEventSink exact(variables,next_before);
  exact.work_limit=planned.work;exact.node_limit=planned.nodes;
  exact.addition_limit=planned.additions;exact.namespace_limit=planned.fresh;
  assert(derive(exact) && exact.same(planned) && untouched());
  for(unsigned which=0;which<4;++which) {
    RamseyEventSink limited(variables,next_before);
    if(which==0)limited.work_limit=planned.work-1;
    if(which==1)limited.node_limit=planned.nodes-1;
    if(which==2)limited.addition_limit=planned.additions-1;
    if(which==3)limited.namespace_limit=planned.fresh-1;
    assert(!derive(limited) && limited.stopped && untouched());
  }
  for(int phase=0;phase<=int(ramsey_certificate::Phase::Contradiction);++phase) {
    RamseyEventSink interrupted(variables,next_before);interrupted.stop_phase=phase;
    assert(!derive(interrupted) && interrupted.stopped && untouched());
  }
  Proof proof;assert(proof.open((prefix+".dsr").c_str(),ProofFormat::DsrAscii));
  FILE *records=std::fopen((prefix+".events.jsonl").c_str(),"w");assert(records);
  RamseyEventSink emitted(variables,next_before);emitted.proof=&proof;
  emitted.kernel_id=&kernel.next_clause_id;emitted.records=records;
  const auto started=std::chrono::steady_clock::now();
  assert(derive(emitted) && emitted.same(planned));
  assert(!std::fclose(records));
  const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
  std::cout<<"{\"vertices\":"<<original_n<<",\"variables\":"<<variables<<",\"clauses\":"<<original.size()
    <<",\"work\":"<<emitted.work<<",\"nodes\":"<<emitted.nodes<<",\"physical_additions\":"<<emitted.additions
    <<",\"physical_deletions\":"<<emitted.deletions<<",\"reserved_helpers\":"<<emitted.fresh-variables
    <<",\"inequalities\":"<<emitted.inequalities<<",\"sr_events\":"<<emitted.sr
    <<",\"fingerprint\":"<<emitted.fingerprint<<",\"preflight_matches\":true,\"bounded_declines\":4,\"interrupted_phases\":7,\"seconds\":"<<seconds<<"}\n";
}
