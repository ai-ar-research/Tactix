#include "rules/lucky/ramsey_startup.hpp"
#include "proof/proof.hpp"
#include "proof/ramsey_certificate.hpp"
#include <cstdlib>
namespace tactix {
namespace {
struct NativeSink {
  using Clause=tactix::cardinality_certificate::Clause;
  bool stopped=false;
  uint32_t fresh;
  uint64_t work=0,nodes=0,additions=0,deletions=0,definitions=0,inequalities=0,sr=0;
  uint64_t work_limit=0,node_limit=0,addition_limit=0;
  uint32_t namespace_limit=INT32_MAX;
  bool writing=false, interrupted=false;
  Kernel &kernel;
  uint64_t action_id;
  uint64_t fingerprint=1469598103934665603ULL,logical_id;
  tactix::Proof *proof=nullptr;
  uint64_t *kernel_id=nullptr;
  FILE *records=nullptr;
  tactix::ramsey_certificate::Phase current=tactix::ramsey_certificate::Phase::StarOrder;
  uint32_t context=0;
  explicit NativeSink(Kernel &state,uint32_t extent,uint64_t initial_id,uint64_t action):fresh(extent),kernel(state),action_id(action),logical_id(initial_id){}
  void mix(uint64_t value){fingerprint^=value;fingerprint*=1099511628211ULL;}
  bool charge_work(uint64_t amount){if(!writing && kernel_terminating(kernel)){stopped=interrupted=true;return false;}if(stopped || work>work_limit || amount>work_limit-work){stopped=true;return false;}work+=amount;return true;}
  bool reserve(uint64_t adds,uint64_t dels,uint32_t helpers) {
    if(stopped || logical_id==UINT64_MAX || additions>addition_limit || adds>addition_limit-additions || fresh>namespace_limit || helpers>namespace_limit-fresh){stopped=true;return false;}
    additions+=adds;deletions+=dels;fresh+=helpers;return true;
  }
  bool io_ok(){if(records && std::ferror(records)){stopped=true;return false;}return true;}
  void event(const char *kind,const Clause &clause,bool rat=false) {
    if(!records)return;
    std::fprintf(records,"{\"action\":%llu,\"kind\":\"%s\",\"phase\":%u,\"context\":%u,\"logical_id\":%llu,\"proof_additions_through\":%llu,\"proof_deletions_through\":%llu,\"rat\":%s,\"clause\":[",(unsigned long long)action_id,kind,unsigned(current),context,(unsigned long long)logical_id,(unsigned long long)additions,(unsigned long long)deletions,rat?"true":"false");
    for(size_t i=0;i<clause.size();++i)std::fprintf(records,"%s%d",i?",":"",clause[i]);
    std::fputs("]}\n",records);
  }
  bool phase(tactix::ramsey_certificate::Phase p,uint32_t detail) {
    if(!charge_work(1)){stopped=true;return false;}
    current=p;context=detail;mix(1);mix(unsigned(p));mix(detail);return true;
  }
  bool new_variable(int32_t &out) {
    if(!charge_work(1) || nodes==node_limit || !reserve(0,0,1)){stopped=true;return false;}
    ++nodes;out=fresh;mix(2);mix(fresh);return true;
  }
  bool record_definition(int32_t auxiliary,int32_t condition,int64_t high,int64_t low) {
    if(!charge_work(1))return false;
    ++definitions;mix(3);mix(auxiliary);mix(condition);mix(high);mix(low);
    if(records)std::fprintf(records,"{\"action\":%llu,\"kind\":\"define-ite\",\"phase\":%u,\"context\":%u,\"auxiliary\":%d,\"condition\":%d,\"high\":%lld,\"low\":%lld}\n",(unsigned long long)action_id,unsigned(current),context,auxiliary,condition,(long long)high,(long long)low);
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
    const auto internal=[&](int32_t lit){const auto result=kernel_internalize(kernel,lit);if(result==INVALID_LIT)std::abort();return result;};
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
    if(records){std::fprintf(records,"{\"action\":%llu,\"kind\":\"witness\",\"mapping\":[",(unsigned long long)action_id);for(size_t i=0;i<mapping.size();++i)std::fprintf(records,"%s[%d,%d]",i?",":"",mapping[i].first,mapping[i].second);std::fputs("]}\n",records);}
    if(proof){*kernel_id=logical_id;if(!proof->add_sr(logical_id,lits.data(),lits.size(),nullptr,0,witness.data(),witness.size(),before,propagates)){stopped=true;return false;}}
    return io_ok();
  }
  bool inequality(const std::vector<std::pair<int32_t,int64_t>> &terms,int64_t bound,int64_t root,bool accumulated) {
    if(!charge_work(terms.size()+1))return false;
    ++inequalities;mix(8);mix(bound);mix(root);mix(accumulated);mix(terms.size());for(auto [v,c]:terms){mix(v);mix(c);}
    if(records){std::fprintf(records,"{\"action\":%llu,\"kind\":\"inequality\",\"phase\":%u,\"context\":%u,\"accumulated\":%s,\"bound\":%lld,\"root\":%lld,\"terms\":[",(unsigned long long)action_id,unsigned(current),context,accumulated?"true":"false",(long long)bound,(long long)root);for(size_t i=0;i<terms.size();++i)std::fprintf(records,"%s[%d,%lld]",i?",":"",terms[i].first,(long long)terms[i].second);std::fputs("]}\n",records);}
    return io_ok();
  }
  bool same(const NativeSink &other)const {
    return !stopped && !other.stopped && fresh==other.fresh && work==other.work && nodes==other.nodes && additions==other.additions && deletions==other.deletions && definitions==other.definitions && inequalities==other.inequalities && sr==other.sr && logical_id==other.logical_id && fingerprint==other.fingerprint;
  }
};

} // namespace
RuleEligibility ramsey_startup_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent",!k.inconsistent)
    .fact("original-input-boundary",k.startup_certificate_open)
    .fact("quiescent-original-root",!k.level && !k.conflict_valid && !k.pending.valid &&
        k.trail.empty() && !k.dense && !k.probing.recording && k.large_watches_connected)
    .fact("witness-format",!k.proof || k.proof->supports_sr());
}
RamseyStartupResult ramsey_startup_apply(Kernel &k,const RamseyStartupRequest &requested) {
  RamseyStartupRequest request=requested;
  RamseyStartupResult result;
  if(!ramsey_startup_guard(k) || !request.records || !request.action_id ||
      !request.recognition.clauses || !request.recognition.variables ||
      !request.recognition.pair_cells || !request.recognition.work ||
      !request.certificate_work || !request.maximum_nodes || !request.maximum_additions ||
      !request.maximum_external_extent ||
      k.external_to_internal.size()>request.maximum_external_extent ||
      k.external_to_internal.size()>=INT32_MAX ||
      k.input_unit_clause_count || k.input_binary_clause_count ||
      !k.input_nonunit_uniform || k.input_uniform_nonunit_width!=6 ||
      k.input_nonunit_clause_count>request.recognition.clauses ||
      k.startup_input.empty() || !k.input_restatements.empty())return result;
  if(kernel_has_work_budget(k)) {
    request.recognition.work=std::min(request.recognition.work,k.work_budget);
    if(!request.recognition.work){result.exhausted=true;return result;}
  }
  const uint32_t extent=static_cast<uint32_t>(k.external_to_internal.size());
  std::vector<ramsey_recovery::Clause> original;
  ramsey_recovery::Clause clause;
  uint64_t snapshot_work=0;
  for(int32_t literal:k.startup_input) {
    if(kernel_terminating(k)){result.interrupted=true;return result;}
    if(snapshot_work==request.recognition.work){result.exhausted=true;return result;}
    ++snapshot_work;result.recognition_work=snapshot_work;
    if(literal) {if(clause.size()==6)return result;clause.push_back(literal);}
    else {if(clause.size()!=6 || original.size()==request.recognition.clauses)return result;
      std::sort(clause.begin(),clause.end());original.push_back(std::move(clause));clause.clear();}
  }
  if(!clause.empty() || original.size()!=k.input_nonunit_clause_count)return result;
  // A startup marker does not substitute for equality with the actual live
  // database. Detect unclosed direct-kernel edits as well as stale snapshots.
  std::multiset<ramsey_recovery::Clause> remaining(original.begin(),original.end());
  for(Ref ref=0;ref<k.arena.size();) {
    const Clause *c=clause_at(k.arena,ref);ref+=clause_actual_words(c);
    if(kernel_terminating(k)){result.interrupted=true;return result;}
    if(snapshot_work==request.recognition.work){result.exhausted=true;return result;}
    ++snapshot_work;result.recognition_work=snapshot_work;
    if(c->garbage())continue;
    if(c->size!=6 || c->redundant())return result;
    ramsey_recovery::Clause live;
    for(Lit lit:*c)live.push_back(kernel_externalize(k,lit));
    std::sort(live.begin(),live.end());const auto found=remaining.find(live);
    if(found==remaining.end())return result;
    remaining.erase(found);
  }
  if(!remaining.empty())return result;
  for(Lit lit=0;lit<2*k.nvars;++lit) {
    if(kernel_terminating(k)){result.interrupted=true;return result;}
    if(snapshot_work==request.recognition.work){result.exhausted=true;return result;}
    ++snapshot_work;result.recognition_work=snapshot_work;
    for(const auto watch:k.watches.entries(lit))if(watch.binary())return result;
  }
  auto limits=request.recognition;limits.work-=snapshot_work;
  const auto recovered=ramsey_recovery::recognize(original,limits,[&](){return kernel_terminating(k);});
  result.recognition_work=snapshot_work+recovered.work;result.interrupted=recovered.interrupted;
  result.exhausted=recovered.exhausted;
  if(!recovered.recognized)return result;
  result.recognized=true;result.vertices=static_cast<uint32_t>(recovered.edges.size());
  // Current certificate admission scope is the independently checked 18/19
  // vertex cases. Larger complete inputs need a separate engineering review.
  if(result.vertices<18 || result.vertices>19)return result;
  if(kernel_has_work_budget(k)) {
    const uint64_t remaining=result.recognition_work<k.work_budget
        ? k.work_budget-result.recognition_work : 0;
    request.certificate_work=std::min(request.certificate_work,remaining);
  }
  NativeSink planned(k,extent,k.next_clause_id,request.action_id);
  planned.work_limit=request.certificate_work;planned.node_limit=request.maximum_nodes;
  planned.addition_limit=request.maximum_additions;
  planned.namespace_limit=std::min(request.maximum_external_extent,uint32_t(INT32_MAX));
  const bool ready=ramsey_certificate::derive(recovered.edges,original,extent,planned);
  result.certificate_work=planned.work;result.interrupted|=planned.interrupted;
  result.nodes=planned.nodes;
  if(!ready){result.exhausted|=!planned.interrupted;return result;}
  if(kernel_terminating(k)){result.interrupted=true;return result;}
  const uint32_t helpers=planned.fresh-extent;
  // No mutating operation or provenance output precedes complete preflight.
  Var first;
  if(!kernel_reserve_proof_variables(k,helpers,first))return result;
  if(first!=extent)std::abort();
  FILE *out=request.records;
  std::fprintf(out,"{\"action\":%llu,\"kind\":\"begin-ramsey-startup\",\"vertices\":%u,\"original_extent\":%u,\"fresh_begin\":%u,\"fresh_end\":%u,\"recognition_work\":%llu,\"certificate_work_limit\":%llu,\"node_limit\":%llu,\"addition_limit\":%llu,\"external_extent_limit\":%u,\"reconstruction\":\"terminal-unsat-no-model\"}\n",
    (unsigned long long)request.action_id,result.vertices,extent,extent+1,planned.fresh,
    (unsigned long long)result.recognition_work,(unsigned long long)request.certificate_work,
    (unsigned long long)request.maximum_nodes,(unsigned long long)request.maximum_additions,request.maximum_external_extent);
  std::fprintf(out,"{\"action\":%llu,\"kind\":\"recognition-parameters\",\"clauses\":%llu,\"variables\":%llu,\"pair_cells\":%llu,\"work\":%llu}\n",(unsigned long long)request.action_id,(unsigned long long)request.recognition.clauses,(unsigned long long)request.recognition.variables,(unsigned long long)request.recognition.pair_cells,(unsigned long long)request.recognition.work);
  for(size_t i=0;i<original.size();++i) {
    std::fprintf(out,"{\"action\":%llu,\"kind\":\"original-premise\",\"occurrence\":%zu,\"clause\":[",(unsigned long long)request.action_id,i+1);
    for(size_t j=0;j<original[i].size();++j)std::fprintf(out,"%s%d",j?",":"",original[i][j]);
    std::fputs("]}\n",out);
  }
  for(const auto &source:recovered.sources)
    std::fprintf(out,"{\"action\":%llu,\"kind\":\"k4-source\",\"vertices\":[%u,%u,%u,%u],\"positive_occurrence\":%llu,\"negative_occurrence\":%llu}\n",
      (unsigned long long)request.action_id,source.vertices[0],source.vertices[1],source.vertices[2],source.vertices[3],
      (unsigned long long)(source.positive_clause+1),(unsigned long long)(source.negative_clause+1));
  for(uint32_t a=0;a<result.vertices;++a)for(uint32_t b=a+1;b<result.vertices;++b)
    std::fprintf(out,"{\"action\":%llu,\"kind\":\"edge-map\",\"vertices\":[%u,%u],\"literal\":%d}\n",(unsigned long long)request.action_id,a,b,recovered.edges[a][b]);
  if(std::ferror(out))std::abort();
  NativeSink emitted(k,extent,k.next_clause_id,request.action_id);
  emitted.writing=true;emitted.proof=k.proof;emitted.kernel_id=&k.next_clause_id;emitted.records=out;
  emitted.work_limit=planned.work;emitted.node_limit=planned.nodes;
  emitted.addition_limit=planned.additions;emitted.namespace_limit=planned.fresh;
  if(!ramsey_certificate::derive(recovered.edges,original,extent,emitted) || !emitted.same(planned))std::abort();
  // Advance logical IDs identically without a proof object as with one.
  k.next_clause_id=emitted.logical_id;
  std::fprintf(out,"{\"action\":%llu,\"kind\":\"complete-ramsey-startup\",\"work\":%llu,\"additions\":%llu,\"deletions\":%llu,\"helpers\":%u}\n",(unsigned long long)request.action_id,(unsigned long long)emitted.work,(unsigned long long)emitted.additions,(unsigned long long)emitted.deletions,helpers);
  if(std::fflush(out) || std::ferror(out))std::abort();
  if(k.proof)k.proof->flush();
  k.close_startup_certificate();k.inconsistent=true;
  result.helpers=helpers;result.additions=emitted.additions;result.deletions=emitted.deletions;
  result.result=ApplyResult::Unsat;
  return result;
}

ApplyResult ramsey_startup_action(Kernel &k) {
  const auto &p = k.ramsey_startup_parameters;
  if (!ramsey_startup_guard(k) || !p.valid() || !k.structural_provenance ||
      k.structural_action_sequence == UINT64_MAX) return ApplyResult::NoOp;
  const uint64_t action = ++k.structural_action_sequence;
  FILE *out = k.structural_provenance;
  std::fprintf(out, "{\"action\":%llu,\"kind\":\"action-context\",\"version\":1,\"rule\":\"Lucky\",\"instance\":22,\"strategy_epoch\":%llu,\"arena_generation\":%llu,\"touch_epoch\":%llu,\"proof_id_before\":%llu,\"external_extent\":%zu,\"original_variables\":%u,\"reconstruction\":\"terminal-unsat;proof-only-helpers\"}\n",
      (unsigned long long)action, (unsigned long long)k.strategy_epoch,
      (unsigned long long)k.arena_generation, (unsigned long long)k.touch_epoch,
      (unsigned long long)k.next_clause_id, k.external_to_internal.size(),
      kernel_num_output_vars(k));
  std::fprintf(out, "{\"action\":%llu,\"kind\":\"action-parameters\",\"recognition_clauses\":%llu,\"recognition_variables\":%llu,\"recognition_pair_cells\":%llu,\"recognition_work\":%llu,\"certificate_work\":%llu,\"maximum_nodes\":%llu,\"maximum_additions\":%llu,\"maximum_external_extent\":%u}\n",
      (unsigned long long)action, (unsigned long long)p.recognition_clauses,
      (unsigned long long)p.recognition_variables, (unsigned long long)p.recognition_pair_cells,
      (unsigned long long)p.recognition_work, (unsigned long long)p.certificate_work,
      (unsigned long long)p.maximum_nodes, (unsigned long long)p.maximum_additions,
      p.maximum_external_extent);
  if (std::ferror(out)) std::abort();
  const auto result = ramsey_startup_apply(k, {{p.recognition_clauses,
      p.recognition_variables, p.recognition_pair_cells, p.recognition_work},
      p.certificate_work, p.maximum_nodes, p.maximum_additions,
      p.maximum_external_extent, action, out});
  ++k.stats.ramsey_startup_attempts;
  k.stats.ramsey_startup_recognized += result.recognized;
  k.stats.ramsey_startup_recognition_work += result.recognition_work;
  k.stats.ramsey_startup_certificate_work += result.certificate_work;
  k.stats.ramsey_startup_nodes += result.nodes;
  k.stats.ramsey_startup_additions += result.additions;
  k.stats.ramsey_startup_deletions += result.deletions;
  k.stats.ramsey_startup_helpers += result.helpers;
  k.stats.ramsey_startup_interrupted += result.interrupted;
  k.stats.ramsey_startup_exhausted += result.exhausted;
  k.stats.ramsey_startup_unsat += result.result == ApplyResult::Unsat;
  std::fprintf(out, "{\"action\":%llu,\"kind\":\"action-complete\",\"result\":%u,\"recognized\":%u,\"recognition_work\":%llu,\"certificate_work\":%llu,\"nodes\":%llu,\"additions\":%llu,\"deletions\":%llu,\"helpers\":%u,\"interrupted\":%u,\"exhausted\":%u,\"proof_id_after\":%llu}\n",
      (unsigned long long)action, static_cast<unsigned>(result.result),
      unsigned(result.recognized), (unsigned long long)result.recognition_work,
      (unsigned long long)result.certificate_work, (unsigned long long)result.nodes,
      (unsigned long long)result.additions, (unsigned long long)result.deletions,
      result.helpers, unsigned(result.interrupted), unsigned(result.exhausted),
      (unsigned long long)k.next_clause_id);
  if (std::fflush(out) || std::ferror(out)) std::abort();
  return result.result;
}
} // namespace tactix
