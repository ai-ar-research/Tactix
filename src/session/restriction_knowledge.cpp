#include "session/restriction_knowledge.hpp"
#include "engine/subsolver.hpp"
#include "kernel/types.hpp"
#include <algorithm>

namespace tactix::session {
namespace {
bool charge(uint64_t &ticks,uint64_t budget,uint64_t work) {
  if(ticks>=budget || work>budget-ticks)return false;
  ticks+=work;return true;
}
void add_ticks(uint64_t &ticks,uint64_t work) {ticks+=std::min(work,UINT64_MAX-ticks);}
bool interrupted(const volatile sig_atomic_t *stop) {return stop&&*stop;}
struct Substitution {
  std::vector<int8_t> constants;
  std::vector<int32_t> images;
  bool parse(const RestrictionStep &step,uint32_t variables) {
    constants.assign(variables,0);images.assign(variables,0);
    const int32_t pivot=step.consequence.literals.front();
    const auto valid=[&](int32_t l) {return l&&l!=INT32_MIN&&external_variable_of(l)<=variables;};
    constants[external_variable_of(pivot)-1]=pivot>0?1:-1;
    for(int32_t l:step.witness) {
      if(!valid(l))return false;
      const auto v=external_variable_of(l)-1;
      if(constants[v])return false;
      constants[v]=l>0?1:-1;
    }
    for(auto [from,to]:step.mappings) {
      if(!valid(from)||!valid(to))return false;
      const auto v=external_variable_of(from)-1;
      if(images[v] || (constants[v]&&v!=external_variable_of(pivot)-1))return false;
      const int32_t image=from<0?-to:to;
      if(image==int32_t(v+1))return false;
      constants[v]=0;images[v]=image;
    }
    return true;
  }
  // True means the image is satisfied (constant true or a tautology).
  bool image(std::span<const int32_t> clause,std::vector<int32_t> &out,bool &changed) const {
    out.clear();changed=false;
    for(int32_t l:clause) {
      const auto v=external_variable_of(l)-1;
      if(v<constants.size()&&constants[v]) {
        changed=true;
        if((constants[v]>0)==(l>0))return true;
        continue;
      }
      const int32_t mapped=v<images.size()&&images[v] ? (l<0?-images[v]:images[v]):l;
      changed|=mapped!=l;
      if(std::find(out.begin(),out.end(),-mapped)!=out.end())return true;
      if(std::find(out.begin(),out.end(),mapped)==out.end())out.push_back(mapped);
    }
    return false;
  }
};
} // namespace

RestrictionKnowledge::RestrictionKnowledge(const QueryHistory &source,const QueryRecord &query,RetentionLimits limits)
    :source_(source),query_(query),limits_(limits) {}

RetentionStatus RestrictionKnowledge::synchronize(uint64_t &ticks,uint64_t budget,const volatile sig_atomic_t *stop) {
  if(oracle_)return RetentionStatus::Accepted;
  __uint128_t words=query_.input_words,clauses=query_.clauses;
  for(const auto &step:steps_) {words+=step.consequence.literals.size()+1;++clauses;}
  if(query_.formula_variables>limits_.maximum_variables || query_.input_words>limits_.maximum_input_words ||
      words+2*clauses>=UINT32_MAX || !charge(ticks,budget,uint64_t(words)))return RetentionStatus::Exhausted;
  prefix_=std::make_unique<QueryHistory>();
  for(int32_t l:source_.input().first(query_.input_words)) {
    if(interrupted(stop))return RetentionStatus::Interrupted;
    if(prefix_->add(l)!=InputStatus::Ok)return RetentionStatus::Exhausted;
  }
  for(const auto &step:steps_) {
    if(interrupted(stop))return RetentionStatus::Interrupted;
    for(int32_t l:step.consequence.literals)if(prefix_->add(l)!=InputStatus::Ok)return RetentionStatus::Exhausted;
    if(prefix_->add(0)!=InputStatus::Ok)return RetentionStatus::Exhausted;
  }
  if(prefix_->begin_query()!=InputStatus::Ok)return RetentionStatus::Exhausted;
  auto local=limits_;
  // The private input is F plus this already checked program, never caller
  // input or assumptions. Its derived portion shares the existing quota.
  local.maximum_input_words=std::max<uint64_t>(1,uint64_t(words));
  oracle_=std::make_unique<RetainedClauses>(local);
  return RetentionStatus::Accepted;
}

RetentionResult RestrictionKnowledge::admit(std::span<const int32_t> clause,uint64_t budget,const volatile sig_atomic_t *stop) {
  if(!limits_.enabled())return {RetentionStatus::Disabled};
  if(source_.active_query()!=&query_)return {RetentionStatus::Stale};
  if(interrupted(stop))return {RetentionStatus::Interrupted};
  if(!budget||clause.size()>limits_.maximum_clause_literals)return {RetentionStatus::Exhausted};
  DerivationRecord record;record.clause.assign(clause.begin(),clause.end());
  return admit(record,budget,stop);
}

RetentionResult RestrictionKnowledge::admit(const DerivationRecord &record,uint64_t budget,const volatile sig_atomic_t *stop) {
  if(!limits_.enabled())return {RetentionStatus::Disabled};
  if(source_.active_query()!=&query_)return {RetentionStatus::Stale};
  if(interrupted(stop))return {RetentionStatus::Interrupted};
  if(!budget || record.clause.size()>limits_.maximum_clause_literals)return {RetentionStatus::Exhausted};
  if(record.kind==DerivationKind::Input || record.kind==DerivationKind::Delete)return {RetentionStatus::Invalid};
  std::vector<int32_t> normalized;
  if(const auto status=normalize_external_clause(record.clause,query_.formula_variables,normalized);
      status!=RetentionStatus::Accepted)return {status};
  uint64_t ticks=0;
  for(const auto &step:steps_) {
    if(!charge(ticks,budget,step.consequence.literals.size()+1))return {RetentionStatus::Exhausted,ticks};
    if(std::is_permutation(normalized.begin(),normalized.end(),step.consequence.literals.begin(),step.consequence.literals.end()))
      return {RetentionStatus::AlreadyPresent,ticks};
  }
  RestrictionStep step;
  step.consequence={query_.id,query_.input_words,normalized,{}};
  __uint128_t payload=normalized.size();
  if(record.kind==DerivationKind::ParityReason)
    for(const auto &row:record.parity_equations)payload+=row.variables.size()+1;
  if(steps_.size()>=limits_.maximum_clauses || payload>limits_.maximum_literals-words_ ||
      !charge(ticks,budget,uint64_t(payload)+1))return {RetentionStatus::Exhausted,ticks};
  if(record.kind==DerivationKind::ParityReason)step.consequence.parity_equations=record.parity_equations;
  const auto prepared=synchronize(ticks,budget,stop);
  if(prepared!=RetentionStatus::Accepted)return {prepared,ticks};
  const auto checked=record.kind==DerivationKind::ParityReason
      ? oracle_->admit_parity(*prefix_,1,record.parity_equations,normalized,budget-std::min(budget,ticks),stop)
      : oracle_->admit(*prefix_,1,normalized,budget-std::min(budget,ticks),stop);
  add_ticks(ticks,checked.ticks);
  if(checked.status==RetentionStatus::Accepted) {
    words_+=uint64_t(payload);steps_.push_back(std::move(step));
    return {RetentionStatus::Accepted,ticks};
  }
  if(checked.status!=RetentionStatus::NotRup ||
      (record.kind!=DerivationKind::Rat&&record.kind!=DerivationKind::SubstitutionRedundancy))
    return {checked.status,ticks};
  if(normalized.empty())return {RetentionStatus::Invalid,ticks};
  if(record.kind==DerivationKind::Rat&&(!record.witness.empty()||!record.mappings.empty()))return {RetentionStatus::Invalid,ticks};
  payload+=record.witness.size()+2*__uint128_t(record.mappings.size());
  if(payload>limits_.maximum_literals-words_ || !charge(ticks,budget,record.witness.size()+2*record.mappings.size()+1))
    return {RetentionStatus::Exhausted,ticks};
  // Keep the recorded first literal as the implicit-true pivot. It is not
  // necessarily first in canonical external literal order.
  const auto pivot=std::find(step.consequence.literals.begin(),step.consequence.literals.end(),record.clause.front());
  std::iter_swap(step.consequence.literals.begin(),pivot);
  step.restriction=true;step.witness=record.witness;step.mappings=record.mappings;
  Substitution substitution;
  if(!substitution.parse(step,query_.formula_variables))return {RetentionStatus::Invalid,ticks};
  for(auto &[from,to]:step.mappings)if(from<0) {from=-from;to=-to;}
  // The failed RUP check leaves exactly F plus already checked conclusions,
  // with its speculative trail retracted. Consume that oracle for SR checks;
  // failure still cannot leak the candidate into the next admission.
  auto prefix=std::move(prefix_);
  auto oracle=std::move(oracle_);
  std::vector<std::span<const int32_t>> sources;
  auto input=source_.input().first(query_.input_words);size_t begin=0;
  for(size_t at=0;at<input.size();++at)if(!input[at]) {sources.push_back(input.subspan(begin,at-begin));begin=at+1;}
  for(const auto &old:steps_)sources.push_back(old.consequence.literals);
  __uint128_t import=0;
  for(auto clause:sources)import+=clause.size()+3;
  if(import>=UINT32_MAX || !charge(ticks,budget,uint64_t(import)))return {RetentionStatus::Exhausted,ticks};
  SubSolver &formula=*oracle->oracle_;
  RetentionStatus status=RetentionStatus::Accepted;
  const auto refutes=[&](const std::vector<SubLit> &assumptions) {
    if(interrupted(stop)) {status=RetentionStatus::Interrupted;return false;}
    if(ticks>=budget) {status=RetentionStatus::Exhausted;return false;}
    const uint64_t before=formula.ticks();
    const auto result=formula.propagate_assumptions_only(assumptions,budget-ticks,stop);
    add_ticks(ticks,formula.ticks()-before);
    if(result==SubPropagationResult::Interrupted)status=RetentionStatus::Interrupted;
    else if(result==SubPropagationResult::Budget)status=RetentionStatus::Exhausted;
    else if(result!=SubPropagationResult::Conflict)status=RetentionStatus::NotRup;
    return result==SubPropagationResult::Conflict;
  };
  std::vector<SubLit> negated;
  for(int32_t l:step.consequence.literals)negated.push_back(internalize(-l));
  std::vector<int32_t> image;
  const auto check=[&](std::span<const int32_t> clause,bool candidate=false,bool cube=false) {
    if(!charge(ticks,budget,clause.size()+1)) {status=RetentionStatus::Exhausted;return false;}
    bool changed=false;
    if(substitution.image(clause,image,changed))return true;
    if(!changed&&!candidate)return true;
    if(cube&&image.size()==1&&std::find(query_.assumptions.begin(),query_.assumptions.end(),image[0])!=query_.assumptions.end())return true;
    std::vector<SubLit> assumptions=negated;for(int32_t l:image)assumptions.push_back(internalize(-l));
    return refutes(assumptions);
  };
  // Prime root BCP before reading fixed literals. A global root conflict would
  // already have admitted C by RUP in the preceding check.
  if(refutes({}))return {RetentionStatus::NotRup,ticks};
  if(status!=RetentionStatus::NotRup)return {status,ticks};
  status=RetentionStatus::Accepted;
  if(!check(step.consequence.literals,true))return {status,ticks};
  for(auto clause:sources)if(!check(clause))return {status,ticks};
  for(uint32_t v=1;v<=query_.formula_variables;++v) {
    if(!charge(ticks,budget,1))return {RetentionStatus::Exhausted,ticks};
    const int8_t value=formula.fixed_literal(internalize(v));
    if(value) {const int32_t unit=value>0?int32_t(v):-int32_t(v);if(!check({&unit,1}))return {status,ticks};}
  }
  for(int32_t assumption:query_.assumptions)if(!check({&assumption,1},false,true))return {status,ticks};
  words_+=uint64_t(payload);steps_.push_back(std::move(step));
  return {RetentionStatus::Accepted,ticks};
}
} // namespace tactix::session
