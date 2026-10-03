#include "rules/lucky/parity_live.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include <cstdlib>

namespace tactix {
namespace {
using namespace parity_certificate;
struct Journal {
  FILE *out;
  uint64_t action;
  void clause(const char *kind,uint64_t id,const External *lits,uint32_t size) {
    std::fprintf(out,"{\"action\":%llu,\"kind\":\"%s\",\"id\":%llu,\"clause\":[",(unsigned long long)action,kind,(unsigned long long)id);
    for(uint32_t i=0;i<size;++i)std::fprintf(out,"%s%d",i?",":"",lits[i]);
    std::fprintf(out,"]}\n");
    if(std::ferror(out))std::abort();
  }
};
struct Sink {
  Kernel &k;
  Journal &journal;
  bool writing;
  uint32_t original_bound,fresh;
  uint64_t limit,additions=0,deletions=0;
  struct Kept {SmallClause clause;uint64_t id;};
  std::vector<Kept> units,exported;
  std::map<std::vector<External>,std::vector<uint64_t>> active_ids;
  bool exporting=false,refuted=false;
  Sink(Kernel &kernel,Journal &records,bool emit,uint32_t bound,uint64_t maximum)
      :k(kernel),journal(records),writing(emit),original_bound(bound),fresh(bound),limit(maximum) {}
  bool add(const SmallClause &c,bool rat) {
    if(additions==limit)return false;
    if(!writing && kernel_terminating(k))return false;
    ++additions;
    if(!c.size)refuted=true;
    uint64_t id=additions;
    if(writing) {
      id=++k.next_clause_id;
      if(k.proof && !(rat?k.proof->add_external_rat(id,c.lits.data(),c.size):k.proof->add_external_rup(id,c.lits.data(),c.size)))return false;
      journal.clause(rat?"define-xor-rat":"derive-xor-rup",id,c.lits.data(),c.size);
    }
    std::vector<External> key(c.lits.begin(),c.lits.begin()+c.size);std::sort(key.begin(),key.end());active_ids[key].push_back(id);
    if(c.size==1 && static_cast<uint32_t>(c.lits[0]<0?-c.lits[0]:c.lits[0])<=original_bound)
      units.push_back({c,id});
    if(exporting) exported.push_back({c,id});
    return true;
  }
  bool erase(const SmallClause &c) {
    // Keep every derived original-variable unit visible to both checker and
    // kernel. Only proof-only helper units may disappear from the stream.
    if(c.size==1 && static_cast<uint32_t>(c.lits[0]<0?-c.lits[0]:c.lits[0])<=original_bound)return true;
    std::vector<External> key(c.lits.begin(),c.lits.begin()+c.size);std::sort(key.begin(),key.end());
    auto found=active_ids.find(key);if(found==active_ids.end()||found->second.empty())return false;
    const uint64_t deleted=found->second.back();found->second.pop_back();if(found->second.empty())active_ids.erase(found);
    ++deletions;
    if(writing) {
      if(k.proof&&!k.proof->del_external(k.next_clause_id,0,c.lits.data(),c.size))return false;
      journal.clause("delete-xor-helper",deleted,c.lits.data(),c.size);
    }
    return true;
  }
  bool new_variable(External &v) {
    if(fresh==INT32_MAX)return false;
    v=static_cast<External>(++fresh);
    if(writing)journal.clause("fresh-proof-xor",fresh,&v,1);
    return true;
  }
};
bool derive(Sink &sink,const std::vector<Equation> &equations,uint32_t width) {
  uint32_t bound=0;
  for(const auto &e:equations)for(uint32_t i=0;i<e.size;++i)bound=std::max(bound,static_cast<uint32_t>(e.variables[i]));
  Certificate certificate(sink,bound);
  if(!certificate.accumulate(equations))return false;
  if(certificate.refuted())return true;
  sink.exporting=true;Clauses output;
  if(!certificate.export_small_consequence(width,output))return false;
  sink.exporting=false;
  // Deriver may emit intermediate export lemmas. Publish only target clauses;
  // all original-variable units are published independently below.
  std::vector<Sink::Kept> targets;
  for(const auto &c:output) {
    auto found=std::find_if(sink.exported.rbegin(),sink.exported.rend(),[&](const auto &item){return item.clause.size==c.size&&std::equal(c.lits.begin(),c.lits.begin()+c.size,item.clause.lits.begin());});
    if(found==sink.exported.rend())return false;
    targets.push_back(*found);
  }
  sink.exported=std::move(targets);
  return certificate.retire_helpers();
}
} // namespace

ParityLiveResult parity_live_apply(Kernel &k,const ParityLiveRequest &request) {
  ParityLiveResult result;
  if(!request.records||!request.action_id||request.selection.output_width>4||
     k.inconsistent||k.level||k.conflict_valid||k.pending.valid||k.dense||
     k.probing.recording||!k.large_watches_connected||k.propagate_head!=k.trail.size()||
     (k.proof&&(k.proof->lrat()||k.proof->pb())))return result;
  Journal journal{request.records,request.action_id};
  std::fprintf(request.records,"{\"action\":%llu,\"kind\":\"begin\",\"clauses_limit\":%llu,\"groups_limit\":%llu,\"selection_work\":%llu,\"intermediate_width\":%u,\"source_limit\":%u,\"output_width\":%u,\"descending\":%s,\"proof_limit\":%llu,\"consequence\":%zu}\n",(unsigned long long)request.action_id,(unsigned long long)request.premises.clauses,(unsigned long long)request.premises.groups,(unsigned long long)request.selection.work,request.selection.intermediate_width,request.selection.source_count,request.selection.output_width,request.descending?"true":"false",(unsigned long long)request.maximum_proof_additions,request.consequence);
  parity_premises::Collector collector(request.premises);
  auto take=[&](const Lit *lits,uint32_t size) {
    if(kernel_terminating(k))return false;
    ++result.clauses;
    if(size<2||size>4)return collector.add(result.clauses,nullptr,0);
    External external[4];for(uint32_t i=0;i<size;++i)external[i]=kernel_externalize(k,lits[i]);
    journal.clause("snapshot-premise",result.clauses,external,size);
    return collector.add(result.clauses,external,size);
  };
  for(Ref ref=0;ref<k.arena.size();) {
    const Clause *c=clause_at(k.arena,ref);ref+=clause_actual_words(c);
    if(!c->garbage()&&!take(c->lits,c->size))return result;
  }
  for(Lit a=0;a<2*k.nvars;++a)for(const WatchEntry w:k.watches.entries(a)) {
    if(!w.binary()||a>=w.other())continue;
    const Lit pair[]={a,w.other()};if(!take(pair,2))return result;
  }
  std::vector<parity_premises::CertifiedEquation> certified;
  if(!collector.finish(certified))return result;
  result.equations=certified.size();
  auto selected=parity_premises::select(certified,request.descending,request.selection);
  result.work=selected.work;
  if(request.consequence>=selected.consequences.size())return result;
  const auto &choice=selected.consequences[request.consequence];
  const Clauses expected=xor_equation(choice.variables.data(),choice.variables.size(),choice.rhs);
  bool all_present=!expected.empty();
  for(const auto &c:expected) {
    Lit lits[4];for(uint32_t i=0;i<c.size;++i)lits[i]=kernel_internalize(k,c.lits[i]);
    bool present=false;
    if(c.size==1)present=k.value(lits[0])>0;
    else if(c.size==2)present=kernel_binary_live(k,lits[0],lits[1]);
    else if(c.size>2)for(Ref ref=0;ref<k.arena.size();) {
      const Clause *existing=clause_at(k.arena,ref);ref+=clause_actual_words(existing);
      if(existing->garbage()||existing->size!=c.size)continue;
      bool same=true;for(uint32_t i=0;i<c.size;++i)same &= std::find(existing->lits,existing->lits+existing->size,lits[i])!=existing->lits+existing->size;
      if(same){present=true;break;}
    }
    all_present &= present;
  }
  if(all_present)return result;
  std::vector<Equation> equations;
  for(size_t index:selected.consequences[request.consequence].sources) {
    equations.push_back(certified[index].equation);
    std::fprintf(request.records,"{\"action\":%llu,\"kind\":\"selected-equation\",\"index\":%zu,\"rhs\":%s,\"premises\":[",(unsigned long long)request.action_id,index,certified[index].equation.rhs?"true":"false");
    for(size_t i=0;i<certified[index].premises.size();++i)std::fprintf(request.records,"%s%llu",i?",":"",(unsigned long long)certified[index].premises[i]);
    std::fprintf(request.records,"]}\n");
  }
  uint32_t tree_variables=0;
  for(const auto &e:equations)for(uint32_t i=0;i<e.size;++i)tree_variables=std::max(tree_variables,static_cast<uint32_t>(e.variables[i]));
  if(tree_variables>request.maximum_tree_variables)return result;
  std::fprintf(request.records,"{\"action\":%llu,\"kind\":\"tree-bound\",\"variables\":%u,\"limit\":%u}\n",(unsigned long long)request.action_id,tree_variables,request.maximum_tree_variables);
  const uint32_t original=static_cast<uint32_t>(k.external_to_internal.size());
  Sink preflight{k,journal,false,original,request.maximum_proof_additions};
  if(!derive(preflight,equations,request.selection.output_width)||kernel_terminating(k))return result;
  Var first;
  result.helpers=preflight.fresh-original;
  if(!kernel_reserve_proof_variables(k,result.helpers,first))return result;
  k.close_startup_certificate();
  Sink committed{k,journal,true,original,request.maximum_proof_additions};
  if(!derive(committed,equations,request.selection.output_width)||committed.additions!=preflight.additions||committed.deletions!=preflight.deletions||committed.fresh!=preflight.fresh)std::abort();
  result.additions=committed.additions;result.deletions=committed.deletions;
  auto publish=[&](const Sink::Kept &item) {
    const auto &c=item.clause;Lit lits[4];
    for(uint32_t i=0;i<c.size;++i) {lits[i]=kernel_internalize(k,c.lits[i]);if(lits[i]==INVALID_LIT)std::abort();}
    if(!c.size) {k.inconsistent=true;return;}
    if(c.size==1) {
      if(k.value(lits[0])<0) {k.inconsistent=true;const uint64_t empty_id=++k.next_clause_id;if(k.proof)k.proof->add_empty(empty_id,nullptr,0);journal.clause("root-conflict",empty_id,nullptr,0);return;}
      if(!k.value(lits[0])) {k.unit_ids[var_of(lits[0])]=item.id;kernel_assign_unit(k,lits[0]);++result.published;}
    } else if(c.size==2) {
      if(!kernel_binary_live(k,lits[0],lits[1])) {kernel_add_binary_clause(k,lits[0],lits[1],item.id);++result.published;}
    } else {kernel_add_large_clause(k,lits,c.size,true,c.size,item.id);++result.published;}
    journal.clause("publish-xor-consequence",item.id,c.lits.data(),c.size);
  };
  if(committed.refuted)k.inconsistent=true;
  for(const auto &item:committed.units) {if(k.inconsistent)break;publish(item);}
  for(const auto &item:committed.exported) {if(k.inconsistent)break;publish(item);}
  if(k.inconsistent)result.result=ApplyResult::Unsat;
  else if(propagate_service(k)==ApplyResult::Unsat)result.result=ApplyResult::Unsat;
  else result.result=result.published?ApplyResult::Ok:ApplyResult::NoOp;
  std::fflush(request.records);if(std::ferror(request.records))std::abort();
  return result;
}
} // namespace tactix
