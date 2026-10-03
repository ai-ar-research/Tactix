#pragma once
#include "proof/proof.hpp"
#include "proof/ramsey_certificate.hpp"
#include <cstdio>
// Research adapter: bounds and provenance for the pure composition. The native
// action will own kernel reservation, eligibility, interruption and publication.
struct RamseyEventSink {
  using Clause=tactix::cardinality_certificate::Clause;
  bool stopped=false;
  uint32_t fresh;
  uint64_t work=0,nodes=0,additions=0,deletions=0,definitions=0,inequalities=0,sr=0;
  uint64_t work_limit=10000000,node_limit=1000000,addition_limit=10000000;
  uint32_t namespace_limit=INT32_MAX;
  int stop_phase=-1;
  uint64_t fingerprint=1469598103934665603ULL,logical_id;
  tactix::Proof *proof=nullptr;
  uint64_t *kernel_id=nullptr;
  FILE *records=nullptr;
  tactix::ramsey_certificate::Phase current=tactix::ramsey_certificate::Phase::StarOrder;
  uint32_t context=0;
  explicit RamseyEventSink(uint32_t extent,uint64_t initial_id):fresh(extent),logical_id(initial_id){}
  void mix(uint64_t value){fingerprint^=value;fingerprint*=1099511628211ULL;}
  bool charge_work(uint64_t amount){if(stopped || work>work_limit || amount>work_limit-work){stopped=true;return false;}work+=amount;return true;}
  bool reserve(uint64_t adds,uint64_t dels,uint32_t helpers) {
    if(stopped || additions>addition_limit || adds>addition_limit-additions || fresh>namespace_limit || helpers>namespace_limit-fresh){stopped=true;return false;}
    additions+=adds;deletions+=dels;fresh+=helpers;return true;
  }
  bool io_ok(){if(records && std::ferror(records)){stopped=true;return false;}return true;}
  void event(const char *kind,const Clause &clause,bool rat=false) {
    if(!records)return;
    std::fprintf(records,"{\"kind\":\"%s\",\"phase\":%u,\"context\":%u,\"logical_id\":%llu,\"proof_additions_through\":%llu,\"proof_deletions_through\":%llu,\"rat\":%s,\"clause\":[",kind,unsigned(current),context,(unsigned long long)logical_id,(unsigned long long)additions,(unsigned long long)deletions,rat?"true":"false");
    for(size_t i=0;i<clause.size();++i)std::fprintf(records,"%s%d",i?",":"",clause[i]);
    std::fputs("]}\n",records);
  }
  bool phase(tactix::ramsey_certificate::Phase p,uint32_t detail) {
    if(!charge_work(1) || int(p)==stop_phase){stopped=true;return false;}
    current=p;context=detail;mix(1);mix(unsigned(p));mix(detail);return true;
  }
  bool new_variable(int32_t &out) {
    if(!charge_work(1) || nodes==node_limit || !reserve(0,0,1)){stopped=true;return false;}
    ++nodes;out=fresh;mix(2);mix(fresh);return true;
  }
  bool record_definition(int32_t auxiliary,int32_t condition,int64_t high,int64_t low) {
    if(!charge_work(1))return false;
    ++definitions;mix(3);mix(auxiliary);mix(condition);mix(high);mix(low);
    if(records)std::fprintf(records,"{\"kind\":\"define-ite\",\"phase\":%u,\"context\":%u,\"auxiliary\":%d,\"condition\":%d,\"high\":%lld,\"low\":%lld}\n",unsigned(current),context,auxiliary,condition,(long long)high,(long long)low);
    return io_ok();
  }
  bool add(const Clause &clause,bool rat) {
    if(!charge_work(clause.size()+1) || !reserve(1,0,0))return false;
    ++logical_id;mix(rat?4:5);mix(clause.size());for(auto l:clause)mix(l);
    event("add",clause,rat);
    if(proof) {
      *kernel_id=logical_id;
      if(!(rat?proof->add_external_rat(logical_id,clause.data(),clause.size()):proof->add_external_rup(logical_id,clause.data(),clause.size()))){stopped=true;return false;}
    }
    return io_ok();
  }
  bool erase(const Clause &clause) {
    if(!charge_work(clause.size()+1) || !reserve(0,1,0))return false;
    ++logical_id;mix(6);mix(clause.size());for(auto l:clause)mix(l);
    event("delete",clause);
    if(proof){*kernel_id=logical_id;if(!proof->del_external(logical_id,0,clause.data(),clause.size())){stopped=true;return false;}}
    return io_ok();
  }
  bool symmetry(const Clause &clause,const std::vector<std::pair<int32_t,int32_t>> &mapping,bool propagates) {
    if(!charge_work(clause.size()+2*mapping.size()+1))return false;
    const auto internal=[](int32_t lit){return tactix::make_lit(uint32_t(std::abs(lit)-1),lit<0);};
    std::vector<tactix::Lit> lits;for(auto l:clause)lits.push_back(internal(l));
    std::vector<tactix::DsrLiteralMapping> witness;
    for(const auto &[from,to]:mapping){assert(from>0);witness.push_back({internal(from),internal(to)});}
    const auto plan=tactix::plan_dsr_witness(lits.data(),lits.size(),nullptr,0,witness.data(),witness.size());
    using Form=tactix::DsrWitnessForm;
    const bool helper=plan.form==Form::Extension || (propagates && plan.form!=Form::Rup);
    const uint32_t before=fresh;
    if(!reserve(helper?3:plan.form==Form::Lemma?2:1,helper?2:plan.form==Form::Lemma?1:0,helper))return false;
    ++logical_id;++sr;mix(7);mix(clause.size());for(auto l:clause)mix(l);
    mix(mapping.size());for(const auto &[from,to]:mapping){mix(from);mix(to);}mix(propagates);mix(unsigned(plan.form));mix(before);
    event("sr",clause);
    if(records){std::fputs("{\"kind\":\"witness\",\"mapping\":[",records);for(size_t i=0;i<mapping.size();++i)std::fprintf(records,"%s[%d,%d]",i?",":"",mapping[i].first,mapping[i].second);std::fputs("]}\n",records);}
    if(proof){*kernel_id=logical_id;if(!proof->add_sr(logical_id,lits.data(),lits.size(),nullptr,0,witness.data(),witness.size(),before,propagates)){stopped=true;return false;}}
    return io_ok();
  }
  bool inequality(const std::vector<std::pair<int32_t,int64_t>> &terms,int64_t bound,int64_t root,bool accumulated) {
    if(!charge_work(terms.size()+1))return false;
    ++inequalities;mix(8);mix(bound);mix(root);mix(accumulated);mix(terms.size());for(auto [v,c]:terms){mix(v);mix(c);}
    if(records){std::fprintf(records,"{\"kind\":\"inequality\",\"phase\":%u,\"context\":%u,\"accumulated\":%s,\"bound\":%lld,\"root\":%lld,\"terms\":[",unsigned(current),context,accumulated?"true":"false",(long long)bound,(long long)root);for(size_t i=0;i<terms.size();++i)std::fprintf(records,"%s[%d,%lld]",i?",":"",terms[i].first,(long long)terms[i].second);std::fputs("]}\n",records);}
    return io_ok();
  }
  bool same(const RamseyEventSink &other)const {
    return !stopped && !other.stopped && fresh==other.fresh && work==other.work && nodes==other.nodes && additions==other.additions && deletions==other.deletions && definitions==other.definitions && inequalities==other.inequalities && sr==other.sr && logical_id==other.logical_id && fingerprint==other.fingerprint;
  }
};
