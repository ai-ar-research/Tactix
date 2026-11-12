#include "session/failed_core.hpp"
#include "kernel/kernel.hpp"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace tactix::session {

CoreCandidate trace_failed_core(const Kernel &k,std::span<const int32_t> effective,
    int32_t failed_literal,size_t failed_index,bool base_unsat,uint64_t budget,
    const volatile sig_atomic_t *stop) {
  CoreCandidate out;
  const auto charge=[&](uint64_t n=1) {
    if(stop&&*stop) {out.interrupted=true;return false;}
    if(n>budget-out.ticks) {out.exhausted=true;return false;}
    out.ticks+=n;return true;
  };
  if(!charge())return out;
  if(base_unsat) {out.complete=true;return out;}
  std::unordered_map<int32_t,size_t> indices;
  for(size_t i=0;i<effective.size();++i) {
    if(!charge())return out;
    if(!failed_literal) {
      if(auto p=indices.find(-effective[i]);p!=indices.end()) {
        out.indices={p->second,i};out.complete=true;return out;
      }
    }
    indices.emplace(effective[i],i);
  }
  if(!failed_literal || failed_index>=effective.size() || effective[failed_index]!=failed_literal)return out;
  out.indices.push_back(failed_index);
  const Lit failed=kernel_internalize(k,failed_literal);
  if(failed==INVALID_LIT) {
    out.complete=kernel_external_value(k,internalize(failed_literal))<0;
    return out; // archived root, independently checked by the F-only license
  }
  if(k.value(failed)>=0)return out;
  std::vector<Lit> queue{negate(failed)};
  std::unordered_set<Var> seen;
  const auto antecedent=[&](Lit lit) {
    if(lit>=k.values.size() || k.value(lit)>=0)return false;
    queue.push_back(negate(lit));return true;
  };
  for(size_t next=0;next<queue.size();++next) {
    if(!charge())return out;
    const Lit lit=queue[next];const Var v=var_of(lit);
    if(v>=k.nvars || k.value(lit)<=0)return out;
    if(!seen.insert(v).second)continue;
    const Assigned &a=k.assigned[v];
    if(!a.level)continue;
    // No lazy-reason materialization or native/proof mutation during export.
    if(a.affine)return out;
    if(a.reason==DECISION_REASON && !a.binary) {
      auto p=indices.find(kernel_externalize(k,lit));
      if(p==indices.end())return out;
      out.indices.push_back(p->second);continue;
    }
    if(a.binary) {
      if(!charge() || !antecedent(a.reason))return out;
      if(a.jumped && (!charge() || v>=k.jumped_over.size() || !antecedent(k.jumped_over[v])))return out;
      continue;
    }
    if(a.reason==UNIT_REASON || a.reason>k.arena.size() ||
        k.arena.size()-a.reason<CLAUSE_HEADER_WORDS)return out;
    const Clause *c=clause_at(k.arena,a.reason);
    if(c->garbage() || c->detached_reason() || c->size<3 ||
        c->size>k.arena.size()-a.reason-CLAUSE_HEADER_WORDS || !charge(c->size))return out;
    bool self=false;
    for(Lit other:*c) {
      if(other==lit)self=true;
      else if(!antecedent(other))return out;
    }
    if(!self)return out;
  }
  std::sort(out.indices.begin(),out.indices.end());
  out.indices.erase(std::unique(out.indices.begin(),out.indices.end()),out.indices.end());
  out.complete=true;return out;
}

FailedCore prepare_failed_core(const Kernel &k,const QueryHistory &history,const QueryRecord &query,
    std::span<const int32_t> effective,int32_t failed_literal,size_t failed_index,
    bool base_unsat,std::span<const DerivationRecord> records,const CoreLimits &limits,
    const volatile sig_atomic_t *stop) {
  FailedCore out;
  std::unordered_set<int32_t> all;
  for(int32_t lit:query.assumptions)if(all.insert(lit).second)out.assumptions.push_back(lit);
  if(!limits.ticks)return out;
  if(effective.size()!=query.assumptions.size())return out;
  const auto traced=trace_failed_core(k,effective,failed_literal,failed_index,base_unsat,limits.ticks,stop);
  out.ticks=traced.ticks;out.traced=traced.complete;
  out.exhausted=traced.exhausted;out.interrupted=traced.interrupted;
  if(!traced.complete)return out;
  std::vector<int32_t> candidate,clause;
  std::unordered_set<int32_t> selected;
  bool contradictory=false;
  for(size_t i:traced.indices) {
    const int32_t lit=query.assumptions[i];
    contradictory|=selected.count(-lit)!=0;
    if(selected.insert(lit).second) {candidate.push_back(lit);clause.push_back(-lit);}
  }
  if(candidate.size()>=out.assumptions.size())return out;
  if(contradictory) {
    out.assumptions=std::move(candidate);out.kind=CoreKind::LicensedSubset;return out;
  }
  if(!limits.proof.enabled())return out;
  if(out.ticks>=limits.ticks) {out.exhausted=true;return out;}
  RetainedClauses oracle(limits.proof);
  const auto admit=[&](const DerivationRecord *record) {
    if(out.ticks>=limits.ticks) {out.exhausted=true;return RetentionStatus::Exhausted;}
    // Conservative import/copy allowance, including a rebuild after a checked
    // XOR recipe. This bounds work even when the private oracle has a cache.
    const __uint128_t imports=__uint128_t(query.input_words)+2*__uint128_t(query.clauses)+
        oracle.literals()+3*__uint128_t(oracle.clauses().size());
    if(imports>limits.ticks-out.ticks) {out.exhausted=true;return RetentionStatus::Exhausted;}
    out.ticks+=uint64_t(imports);
    if(out.ticks>=limits.ticks) {out.exhausted=true;return RetentionStatus::Exhausted;}
    const auto result=record && record->kind==DerivationKind::ParityReason
        ? oracle.admit_parity(history,query.id,record->parity_equations,record->clause,limits.ticks-out.ticks,stop)
        : oracle.admit(history,query.id,record?std::span<const int32_t>(record->clause):std::span<const int32_t>(clause),limits.ticks-out.ticks,stop);
    out.ticks+=std::min(result.ticks,UINT64_MAX-out.ticks);
    out.interrupted|=result.status==RetentionStatus::Interrupted;
    out.exhausted|=result.status==RetentionStatus::Exhausted;
    return result.status;
  };
  auto status=admit(nullptr);
  if(status==RetentionStatus::NotRup) {
    // Re-establish global support only. Assumption inputs, SR/RAT tags and
    // deleted-source IDs convey no authority. No A enters this oracle.
    for(const auto &record:records) {
      if(record.kind==DerivationKind::Input || record.kind==DerivationKind::Delete || record.clause.empty())continue;
      (void)admit(&record);
      if(out.interrupted || out.exhausted)return out;
    }
    status=admit(nullptr);
  }
  if(status!=RetentionStatus::Accepted && status!=RetentionStatus::AlreadyPresent && status!=RetentionStatus::Tautology)return out;
  out.assumptions=std::move(candidate);out.kind=CoreKind::LicensedSubset;
  out.evidence.assign(oracle.clauses().begin(),oracle.clauses().end());
  return out;
}
} // namespace tactix::session
