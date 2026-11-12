#include "session/core_certificate.hpp"
#include "session/knowledge_proof.hpp"
#include <cstdio>

namespace tactix::session {
bool write_core_certificate(const QueryHistory &history,const QueryResultSnapshot &result,
                            const std::string &prefix) {
  if(result.result!=QueryResult::Unsat || result.core.kind!=CoreKind::LicensedSubset)return false;
  const auto *q=history.query(result.query);
  if(!q || q->input_words!=result.input_words)return false;
  FILE *cnf=std::fopen((prefix+".cnf").c_str(),"w");if(!cnf)return false;
  bool good=std::fprintf(cnf,"p cnf %u %llu\n",q->query_variables,
      (unsigned long long)(q->clauses+result.core.assumptions.size()))>0;
  for(int32_t l:history.input().first(q->input_words))good=(std::fprintf(cnf,l?"%d ":"%d\n",l)>0)&&good;
  for(int32_t l:result.core.assumptions)good=(std::fprintf(cnf,"%d 0\n",l)>0)&&good;
  good=(std::fclose(cnf)==0)&&good;if(!good)return false;
  Proof proof;if(!proof.open_automatic((prefix+".proof").c_str(),false))return false;
  std::vector<Lit> clause;uint64_t id=0;
  for(int32_t l:history.input().first(q->input_words)) {
    if(l)clause.push_back(internalize(l));
    else {proof.seed_population(++id,clause.data(),clause.size());clause.clear();}
  }
  for(int32_t l:result.core.assumptions) {const Lit literal=internalize(l);proof.seed_population(++id,&literal,1);}
  proof.finish_input_transaction();PbpParityBoundsCache cache;
  for(const auto &step:result.core.evidence) {
    if(step.literals.empty())continue;
    if(!emit_query_knowledge(proof,step,id,q->query_variables,cache,[](uint64_t) {return true;}))return false;
  }
  proof.add_empty(++id);proof.flush();return true;
}
}
