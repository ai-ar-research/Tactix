#include "rules/semantic_extension/row_cover.hpp"
#include "rules/semantic_extension/row_occupancy.hpp"
#include "proof/pigeonhole_certificate.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include <cstdlib>

namespace tactix {
namespace {
struct CertificateSink {
  Kernel &k;
  const RowCoverRequest &request;
  uint32_t extent;
  bool writing;
  uint64_t additions=0, helpers=0;
  bool stopped=false;
  bool fresh(int32_t &literal) {
    if(stopped || extent>=request.maximum_external_extent || extent>=INT32_MAX)
      return !(stopped=true);
    literal=static_cast<int32_t>(++extent);++helpers;
    return true;
  }
  bool add(const std::vector<int32_t> &clause,bool rat,const char *role) {
    if(stopped || additions==request.maximum_additions || (!writing&&kernel_terminating(k)))
      return !(stopped=true);
    ++additions;
    if(!writing)return true;
    const uint64_t id=++k.next_clause_id;
    if(k.proof && !(rat ? k.proof->add_external_rat(id,clause.data(),clause.size())
                       : k.proof->add_external_rup(id,clause.data(),clause.size())))std::abort();
    std::fprintf(request.records,"{\"action\":%llu,\"kind\":\"%s\",\"id\":%llu,\"clause\":[",
        (unsigned long long)request.action_id,role,(unsigned long long)id);
    for(size_t i=0;i<clause.size();++i)std::fprintf(request.records,"%s%d",i?",":"",clause[i]);
    std::fputs("]}\n",request.records);
    return true;
  }
};
} // namespace

RowCoverResult derive_row_cover(Kernel &k,const RowCoverRequest &request) {
  RowCoverResult result;
  if(!request.records || !request.action_id || k.inconsistent || k.level ||
      k.conflict_valid || k.pending.valid || k.dense || k.probing.recording ||
      !k.large_watches_connected || !kernel_propagation_fixpoint(k) ||
      (k.proof&&(k.proof->lrat()||k.proof->terminal_pbp())) ||
      k.external_to_internal.size()>=INT32_MAX ||
      k.external_to_internal.size()>=request.maximum_external_extent)return result;
  if(request.original_input&&(!k.startup_certificate_open||k.startup_input.empty()))return result;
  uint64_t literals=0;
  std::vector<row_occupancy::Premise> source;
  auto charge=[&](uint64_t amount) {
    if(kernel_terminating(k)||amount>request.recognition_work-result.recognition_work)return false;
    result.recognition_work+=amount;return true;
  };
  auto take=[&](const Lit *clause,uint32_t size) {
    if(source.size()==request.maximum_clauses || size>request.maximum_literals-literals || !charge(size+1))return false;
    row_occupancy::Premise p{source.size()+1,{}};p.literals.reserve(size);
    for(uint32_t i=0;i<size;++i) {
      if(kernel_var_retired(k,var_of(clause[i])))return false;
      p.literals.push_back(kernel_externalize(k,clause[i]));
    }
    literals+=size;source.push_back(std::move(p));return true;
  };
  for(Ref ref=0;ref<k.arena.size();) {
    const Clause *c=clause_at(k.arena,ref);ref+=clause_actual_words(c);
    if(!charge(1)||(!c->garbage()&&!take(c->lits,c->size)))return result;
  }
  for(Lit lit=0;lit<2*k.nvars;++lit)for(const WatchEntry w:k.watches.entries(lit)) {
    if(!charge(1))return result;
    if(w.binary()&&lit<w.other()) {const Lit pair[]={lit,w.other()};if(!take(pair,2))return result;}
  }
  if(request.original_input) {
    std::map<std::vector<int32_t>,bool> residuals;
    for(const auto &p:source) {
      if(!charge(p.literals.size()+1))return result;
      std::vector<int32_t> clause;bool satisfied=false;
      for(int32_t lit:p.literals) {
        const int value=k.value(kernel_internalize(k,lit));satisfied|=value>0;
        if(!value)clause.push_back(lit);
      }
      if(!satisfied) {std::sort(clause.begin(),clause.end());residuals.emplace(std::move(clause),true);}
    }
    std::vector<row_occupancy::Premise> original;
    std::vector<int32_t> clause;
    uint64_t occurrence=0,total=0;
    for(int32_t external:k.startup_input) {
      if(!charge(1))return result;
      if(external) {
        if(total==request.maximum_literals)return result;
        ++total;clause.push_back(external);continue;
      }
      ++occurrence;
      if(original.size()==request.maximum_clauses)return result;
      std::sort(clause.begin(),clause.end());clause.erase(std::unique(clause.begin(),clause.end()),clause.end());
      bool satisfied=false,tautology=false;
      std::vector<int32_t> residual;
      for(int32_t lit:clause) {
        if(!charge(1)||!lit||lit==INT32_MIN)return result;
        if(std::binary_search(clause.begin(),clause.end(),-lit))tautology=true;
        const Lit internal=kernel_internalize(k,lit);
        if(internal==INVALID_LIT||kernel_var_retired(k,var_of(internal)))return result;
        const int value=k.value(internal);
        satisfied|=value>0;
        if(!value)residual.push_back(lit);
      }
      if(!tautology) {
        // An original clause is usable only if current root units satisfy it,
        // or its exact residual still exists. Restating it is then RUP even
        // when parser simplification deleted the original checker occurrence.
        if(!satisfied&&!residuals.count(residual))return result;
        original.push_back({occurrence,clause});
      }
      clause.clear();
    }
    if(!clause.empty())return result;
    source=std::move(original);
  }
  row_occupancy::Image image;
  if(!row_occupancy::recognize(source,request.maximum_width,charge,image))return result;
  const uint32_t n=image.width;result.width=n;
  std::vector<bool> needed(n,true);
  uint64_t wanted=0,arena_words=0;
  for(uint32_t row=0;row<n;++row) {
    auto target=image.rows[row];std::sort(target.begin(),target.end());
    for(int32_t external:target) {
      const Lit lit=kernel_internalize(k,external);
      if(lit==INVALID_LIT||kernel_var_retired(k,var_of(lit)))return result;
      if(k.value(lit)>0)needed[row]=false;
    }
    for(const auto &premise:source) {
      if(!charge(1))return result;
      if(premise.literals.size()!=target.size())continue;
      if(!charge(target.size()))return result;
      auto clause=premise.literals;std::sort(clause.begin(),clause.end());
      if(clause==target){needed[row]=false;break;}
    }
    wanted+=needed[row];
    if(needed[row]&&target.size()>2)arena_words+=target.size()+CLAUSE_HEADER_WORDS;
  }
  if(!wanted||arena_words+k.arena.size()>=INVALID_REF)return result;
  std::vector<std::vector<int32_t>> x(n,std::vector<int32_t>(n));
  for(uint32_t column=0;column<n;++column)for(uint32_t row=0;row<n;++row)
    x[column][row]=image.primary[size_t(row)*n+column];
  const uint32_t prior_extent=k.external_to_internal.size();
  CertificateSink preflight{k,request,prior_extent,false};
  pigeonhole_certificate::Certificate<CertificateSink> counted(preflight);
  std::vector<int32_t> preflight_occupied;
  if(!counted.derive_occupancy(x,preflight_occupied))return result;
  result.preflight_additions=preflight.additions;
  uint64_t missing=0;
  for(const auto &g:image.gates)missing+=(g.reverse_left.size()==2)+(g.reverse_right.size()==2);
  const uint64_t extra=missing+wanted+1+(request.original_input?source.size():0);
  if(extra>request.maximum_additions || preflight.additions>request.maximum_additions-extra ||
      k.next_clause_id>uint64_t(INT64_MAX)-preflight.additions-extra-1 || kernel_terminating(k))return result;
  Var reserved;
  if(!kernel_reserve_proof_variables(k,preflight.helpers,reserved)||reserved!=prior_extent)std::abort();
  k.close_startup_certificate();
  result.helpers=preflight.helpers;
  std::fprintf(request.records,"{\"action\":%llu,\"kind\":\"source-view\",\"original_input\":%s}\n",
      (unsigned long long)request.action_id,request.original_input?"true":"false");
  std::fprintf(request.records,"{\"action\":%llu,\"kind\":\"row-cover-parameters\",\"maximum_clauses\":%llu,\"maximum_literals\":%llu,\"recognition_work\":%llu,\"maximum_width\":%u,\"maximum_external_extent\":%u,\"maximum_additions\":%llu}\n",
      (unsigned long long)request.action_id,(unsigned long long)request.maximum_clauses,
      (unsigned long long)request.maximum_literals,(unsigned long long)request.recognition_work,
      request.maximum_width,request.maximum_external_extent,(unsigned long long)request.maximum_additions);
  std::fprintf(request.records,"{\"action\":%llu,\"kind\":\"balanced-permutation-theory\",\"width\":%u,\"proof_helper_first\":%u,\"proof_helper_count\":%llu,\"projection\":\"proof-only-helpers;original-model-unchanged\"}\n",
      (unsigned long long)request.action_id,n,reserved+1,(unsigned long long)preflight.helpers);
  for(const auto &p:source) {
    std::fprintf(request.records,"{\"action\":%llu,\"kind\":\"source\",\"occurrence\":%llu,\"clause\":[",(unsigned long long)request.action_id,(unsigned long long)p.occurrence);
    for(size_t i=0;i<p.literals.size();++i)std::fprintf(request.records,"%s%d",i?",":"",p.literals[i]);
    std::fputs("]}\n",request.records);
  }
  CertificateSink emitted{k,request,prior_extent,true};
  if(request.original_input)for(const auto &premise:source)
    if(!emitted.add(premise.literals,false,"revalidated-input-restatement"))std::abort();
  for(const auto &g:image.gates) {
    std::fprintf(request.records,"{\"action\":%llu,\"kind\":\"interval\",\"output\":%d,\"row\":%u,\"first\":%u,\"last\":%u,\"defining\":%llu,\"left\":%d,\"right\":%d,\"reverse_left\":[",
        (unsigned long long)request.action_id,g.interval.absence,g.interval.row,g.interval.first,g.interval.last,(unsigned long long)g.defining,g.left,g.right);
    for(size_t i=0;i<g.reverse_left.size();++i)std::fprintf(request.records,"%s%llu",i?",":"",(unsigned long long)g.reverse_left[i]);
    std::fputs("],\"reverse_right\":[",request.records);
    for(size_t i=0;i<g.reverse_right.size();++i)std::fprintf(request.records,"%s%llu",i?",":"",(unsigned long long)g.reverse_right[i]);
    std::fputs("]}\n",request.records);
    for(bool right:{false,true}) {
      const auto &parents=right?g.reverse_right:g.reverse_left;
      if(parents.size()!=2)continue;
      const int32_t input=right?g.right:g.left;
      if(!emitted.add({-input,-g.interval.absence},false,"interval-reverse"))std::abort();
      kernel_add_binary_clause(k,kernel_internalize(k,-input),kernel_internalize(k,-g.interval.absence),k.next_clause_id);
      ++result.reverse_implications;
    }
  }
  for(uint32_t row=0;row<n;++row)for(uint32_t a=0;a<n;++a)for(uint32_t b=a+1;b<n;++b)
    std::fprintf(request.records,"{\"action\":%llu,\"kind\":\"row-exclusion\",\"row\":%u,\"first\":%u,\"second\":%u,\"premise\":%llu}\n",
        (unsigned long long)request.action_id,row,a,b,(unsigned long long)image.row_exclusions[(size_t(row)*n+a)*n+b]);
  const uint64_t before=emitted.additions;
  pigeonhole_certificate::Certificate<CertificateSink> certificate(emitted);
  std::vector<int32_t> occupied;
  if(!certificate.derive_occupancy(x,occupied)||emitted.extent!=preflight.extent||
      occupied!=preflight_occupied||emitted.additions-before!=preflight.additions)std::abort();
  for(uint32_t row=0;row<n;++row) {
    if(!needed[row])continue;
    const auto &cover=image.rows[row];
    if(!emitted.add(cover,false,"publish-row-cover"))std::abort();
    std::vector<Lit> lits;for(int32_t external:cover)lits.push_back(kernel_internalize(k,external));
    if(lits.size()==1) {
      if(k.value(lits[0])<0) {
        if(!emitted.add({},false,"root-conflict"))std::abort();
        k.inconsistent=true;break;
      }
      if(!k.value(lits[0])) {k.unit_ids[var_of(lits[0])]=k.next_clause_id;kernel_assign_unit(k,lits[0]);}
    } else if(lits.size()==2)kernel_add_binary_clause(k,lits[0],lits[1],k.next_clause_id);
    else kernel_add_large_clause(k,lits.data(),lits.size(),false,0,k.next_clause_id);
    ++result.published;
  }
  result.additions=emitted.additions;
  result.result=k.inconsistent||propagate_service(k)==ApplyResult::Unsat?ApplyResult::Unsat:ApplyResult::Ok;
  std::fprintf(request.records,"{\"action\":%llu,\"kind\":\"row-cover-complete\",\"result\":%u,\"recognition_work\":%llu,\"preflight_additions\":%llu,\"additions\":%llu,\"reverse_implications\":%llu,\"helpers\":%llu,\"published\":%llu}\n",
      (unsigned long long)request.action_id,static_cast<unsigned>(result.result),
      (unsigned long long)result.recognition_work,(unsigned long long)result.preflight_additions,
      (unsigned long long)result.additions,(unsigned long long)result.reverse_implications,
      (unsigned long long)result.helpers,(unsigned long long)result.published);
  if(std::fflush(request.records)||std::ferror(request.records))std::abort();
  return result;
}
} // namespace tactix
