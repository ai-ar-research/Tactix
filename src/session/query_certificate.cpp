#include "session/query_certificate.hpp"
#include "engine/subsolver.hpp"
#include "kernel/types.hpp"
#include <cstdio>
#include <cstring>

namespace tactix::session {
QueryCertificateStatus write_query_up_certificate(const QueryHistory &history,uint64_t query,
    const std::string &prefix,const volatile sig_atomic_t *stop) {
  const auto *q=history.query(query);
  if(!q || q->result!=QueryResult::Unsat || !q->completed)return QueryCertificateStatus::Error;
  const std::string path=prefix+".proof";
  FILE *existing=std::fopen(path.c_str(),"rb");if(!existing)return QueryCertificateStatus::Error;
  char signature[15];const size_t count=std::fread(signature,1,sizeof signature,existing);
  const bool read=!std::ferror(existing),closed=std::fclose(existing)==0;
  if(!read||!closed)return QueryCertificateStatus::Error;
  if((count==2&&!std::memcmp(signature,"0\n",2)) ||
      (count>=14&&!std::memcmp(signature,"pseudo-Boolean",14)))return QueryCertificateStatus::Unchanged;
  SubSolver oracle;oracle.reset(q->query_variables);
  std::vector<SubLit> clause;
  for(int32_t literal:history.input().first(q->input_words)) {
    if(stop&&*stop)return QueryCertificateStatus::Interrupted;
    if(literal)clause.push_back(internalize(literal));
    else {oracle.add_clause(clause);clause.clear();}
  }
  for(int32_t literal:q->assumptions)oracle.add_clause({internalize(literal)});
  const auto status=oracle.propagate_assumptions_only({},UINT64_MAX,stop);
  if(status==SubPropagationResult::Interrupted)return QueryCertificateStatus::Interrupted;
  if(status!=SubPropagationResult::Conflict)return QueryCertificateStatus::Unchanged;
  const std::string temporary=prefix+".up-proof";
  FILE *proof=std::fopen(temporary.c_str(),"wb");if(!proof)return QueryCertificateStatus::Error;
  const bool written=std::fputs("0\n",proof)>=0,finished=std::fclose(proof)==0;
  if(!written||!finished||std::rename(temporary.c_str(),path.c_str()))return QueryCertificateStatus::Error;
  return QueryCertificateStatus::Written;
}
}
