#pragma once
// Source-pin-only research adapter. Normal-form identities are authenticated
// semantically, without generating a derivation in a current proof format.
// The installer forbids named proof output in every nonbaseline arm.
#include "diagnostics/shared_anf.hpp"
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/strategy_settings.hpp"
#include "rules/extended_resolution/extended_resolution.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/semantic_variable_advice.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace tactix::shared_nonlinear_probe {
inline bool no_proof_authorized=false;
inline const char *arm() {
  const char *s=strategy_setting("TACTIX_NONLINEAR_ARM");return s ? s:"baseline";
}
inline bool active() {return std::strcmp(arm(),"baseline")!=0;}
struct Session {
  bool enabled=false,first_decision=false;
  uint64_t gates=0,cuts=0,candidates=0,groups=0,definitions=0,links=0,work=0;
  uint64_t priority_count=0,priority_sum=0,summary_decisions=0,helper_decisions=0;
  uint64_t first_live=0,nanos=0;
  uint32_t first_external=0,last_external=0;
  std::set<uint32_t> summaries;
};
inline thread_local Session session;
inline void emit(const Kernel &k,const char *stage) {
  if(!session.enabled)return;
  uint64_t live=0;for(auto ext:session.summaries) {
    Lit lit=kernel_internalize(k,int32_t(ext));
    live+=lit!=INVALID_LIT&&!kernel_var_retired(k,var_of(lit));
  }
  std::fprintf(stdout,"c nonlinear stage=%s arm=%s gates=%llu cuts=%llu candidates=%llu groups=%llu definitions=%llu links=%llu work=%llu nanos=%llu priority-count=%llu priority-sum=%llu summaries=%zu live=%llu first-live=%llu summary-decisions=%llu helper-decisions=%llu\n",stage,arm(),
    (unsigned long long)session.gates,(unsigned long long)session.cuts,(unsigned long long)session.candidates,
    (unsigned long long)session.groups,(unsigned long long)session.definitions,(unsigned long long)session.links,
    (unsigned long long)session.work,(unsigned long long)session.nanos,(unsigned long long)session.priority_count,
    (unsigned long long)session.priority_sum,session.summaries.size(),(unsigned long long)live,
    (unsigned long long)session.first_live,(unsigned long long)session.summary_decisions,(unsigned long long)session.helper_decisions);
  std::fflush(stdout);
}
inline void decision(const Kernel &k,Lit l) {
  if(!session.enabled||l==INVALID_LIT)return;
  if(!session.first_decision) {
    session.first_decision=true;
    for(auto ext:session.summaries) {
      Lit x=kernel_internalize(k,int32_t(ext));session.first_live+=x!=INVALID_LIT&&!kernel_var_retired(k,var_of(x));
    }
  }
  uint32_t ext=uint32_t(std::abs(kernel_externalize(k,l)));
  if(session.summaries.count(ext))++session.summary_decisions;
  else if(ext>=session.first_external&&ext<=session.last_external&&session.first_external)++session.helper_decisions;
}
inline bool snapshot(const Kernel &k,std::vector<shared_anf::Clause> &out,shared_anf::Work &work) {
  auto append=[&](const Lit *lits,uint32_t size) {
    if(!work.take(size+1))return false;
    shared_anf::Clause row;row.reserve(size);
    for(uint32_t i=0;i<size;++i) {
      if(kernel_var_retired(k,var_of(lits[i]))||kernel_root_value(k,lits[i]))return true;
      row.push_back(kernel_externalize(k,lits[i]));
    }
    out.push_back(std::move(row));return true;
  };
  for(Lit l=0;l<2*k.nvars;++l) {
    if(!work.take())return false;
    for(auto w:k.watches.entries(l)) {
      if(!work.take())return false;
      if(!w.binary()||l>w.other())continue;
      const Lit pair[]={l,w.other()};if(!append(pair,2))return false;
    }
  }
  for(auto [ref,c]:arena_records(k.arena)) {
    (void)ref;if(!work.take())return false;
    if(!c->garbage()&&!c->redundant()&&c->size<=5&&!append(c->begin(),c->size))return false;
  }
  return true;
}
inline int32_t define(Kernel &k,ExtensionGate op,int32_t a,int32_t b) {
  int32_t output=0;
  const auto &rule=tactix_rules()[tactix_rule_id("ExtendedResolution")];
  apply_rule_structured_preserve_startup(rule,k,"shared-nonlinear-research",[&]{
    output=extended_resolution_define(k,{op,a,b});return output ? ApplyResult::Ok:ApplyResult::NoOp;
  });
  if(!output)std::abort();
  ++session.definitions;
  if(propagate_apply(k)==ApplyResult::Unsat)k.inconsistent=true;
  return output;
}
class Materializer {
  Kernel &k_;
  std::map<shared_anf::Term,int32_t> terms_;
  std::map<shared_anf::Poly,int32_t> expressions_;
  const shared_anf::Discovery &known_;
  shared_anf::Work &work_;
public:
  static constexpr int32_t TRUE=INT32_MIN; // zero = false
  Materializer(Kernel &k,const shared_anf::Discovery &known,shared_anf::Work &work):k_(k),known_(known),work_(work){}
  static int32_t inverted(int32_t x){return !x ? TRUE:x==TRUE ? 0:-x;}
  int32_t combine(ExtensionGate kind,int32_t a,int32_t b) {
    if(kind==ExtensionGate::And) {
      if(!a||!b)return 0;if(a==TRUE)return b;if(b==TRUE||a==b)return a;if(a==-b)return 0;
    } else {
      if(!a)return b;if(!b)return a;if(a==TRUE)return inverted(b);if(b==TRUE)return inverted(a);
      if(a==b)return 0;if(a==-b)return TRUE;
    }
    return define(k_,kind,a,b);
  }
  int32_t term(const shared_anf::Term &t) {
    auto found=terms_.find(t);if(found!=terms_.end())return found->second;
    auto known=known_.monomials.find(t);
    if(known!=known_.monomials.end()&&!work_.exhausted) {
      if(shared_anf::truth_check(known_.monomial_sources.at(t),work_))return terms_[t]=known->second;
      if(!work_.exhausted)std::abort();
    }
    int32_t value=TRUE;for(auto v:t)value=combine(ExtensionGate::And,value,v);
    return terms_[t]=value;
  }
  int32_t expression(const shared_anf::Poly &p) {
    auto found=expressions_.find(p);if(found!=expressions_.end())return found->second;
    int32_t value=0;for(const auto &t:p)value=combine(ExtensionGate::Xor,value,term(t));
    return expressions_[p]=value;
  }
};
inline bool add_equivalence(Kernel &k,int32_t a,int32_t b) {
  if(b==Materializer::TRUE||!b) {
    Lit unit=kernel_internalize(k,b ? a:-a);
    if(k.value(unit)<0){k.inconsistent=true;return false;}
    if(!k.value(unit))kernel_assign_unit(k,unit);
  } else if(a==-b) {
    k.inconsistent=true;return false;
  } else if(a!=b) {
    const Lit x=kernel_internalize(k,a),y=kernel_internalize(k,b);
    // Research-only semantic consequence, justified by exact authenticated
    // gate substitution, Boolean-ring normalization and independent truth
    // evaluation of the selected cut. No proof-format assertion is emitted.
    kernel_add_binary_clause(k,negate(x),y,++k.next_clause_id);
    kernel_add_binary_clause(k,x,negate(y),++k.next_clause_id);
    ++session.links;
  }
  ++k.strategy_epoch;
  if(propagate_apply(k)==ApplyResult::Unsat){k.inconsistent=true;return false;}
  return true;
}
inline void bump(Kernel &k,const std::vector<strategy::SemanticVariable> &variables) {
  if(variables.empty()||k.inconsistent)return;
  strategy::SemanticVariableAdvice advice;
  advice.action_id=1;advice.source=strategy::state_version(k);advice.variables=variables;
  uint64_t sum=0;for(const auto &v:variables)sum+=v.priority;
  const auto result=strategy::commit_semantic_variable_advice(k,advice,{uint32_t(variables.size()),sum});
  if(result.status!=strategy::SemanticVariableAdviceStatus::Applied)std::abort();
  session.priority_count=result.variables;session.priority_sum=result.priority_sum;
}
inline bool run(Kernel &k) {
  session={};if(!active())return true;
  session.enabled=true;
  const auto begin=std::chrono::steady_clock::now();
  auto finish=[&]{session.nanos=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count();emit(k,"injected");return !k.inconsistent;};
  if(!no_proof_authorized||!k.h||!extended_resolution_guard(k)){emit(k,"inapplicable");return true;}
  shared_anf::Limits limits;
  if(const char *p=strategy_setting("TACTIX_NONLINEAR_GROUPS"))limits.groups=std::strtoul(p,nullptr,10);
  if(const char *p=strategy_setting("TACTIX_NONLINEAR_DEPTH"))limits.depth=std::strtoul(p,nullptr,10);
  shared_anf::Work work;work.stop=k.stop;
  std::vector<shared_anf::Clause> formula;
  if(!snapshot(k,formula,work)){session.work=work.used;return finish();}
  auto discovered=shared_anf::discover(formula,limits,work);
  session.work=work.used;session.gates=discovered.gates;session.cuts=discovered.cuts;
  session.candidates=discovered.novel_candidates;
  if(std::strcmp(arm(),"audit")==0)return finish();
  const bool control=std::strcmp(arm(),"control")==0;
  const bool priority=std::strcmp(arm(),"priority")==0;
  const Var first=k.nvars;session.first_external=uint32_t(k.external_to_internal.size()+1);
  std::vector<strategy::SemanticVariable> advice;
  std::set<uint32_t> controls;
  Materializer materializer(k,discovered,work);
  for(const auto &group:discovered.selected) {
    if(kernel_terminating(k)||k.inconsistent)break;
    ++session.groups;
    const uint32_t dose=uint32_t(group.consumers.size()); // reuse count, same dose in the control
    if(control) {
      for(const auto &cut:group.consumers)if(controls.insert(uint32_t(cut.output)).second) {
        advice.push_back({uint32_t(cut.output),strategy::SemanticVariableRole::ExistingTheory,dose});break;
      }
      continue;
    }
    const int32_t summary=materializer.expression(group.shared);
    if(summary==Materializer::TRUE||!summary)std::abort();
    if(uint32_t(std::abs(summary))<session.first_external)std::abort(); // novelty witness
    session.summaries.insert(uint32_t(std::abs(summary)));
    for(const auto &cut:group.consumers) {
      const auto rest=shared_anf::add(cut.polynomial,group.shared);
      const int32_t value=materializer.combine(ExtensionGate::Xor,summary,materializer.expression(rest));
      if(!add_equivalence(k,cut.output,value))break;
    }
    if(priority) {
      Lit lit=kernel_internalize(k,summary);
      if(!kernel_root_value(k,lit)&&heuristics_var_active(k,var_of(lit)))
        advice.push_back({uint32_t(std::abs(summary)),strategy::SemanticVariableRole::IntroducedTheory,dose});
    }
  }
  if(k.nvars>first)heuristics_demote_fresh_vars(k,first,k.nvars);
  heuristics_deactivate_inactive_vars(k);
  std::erase_if(advice,[&](const strategy::SemanticVariable &v){
    Lit lit=kernel_internalize(k,int32_t(v.external));
    return lit==INVALID_LIT||kernel_root_value(k,lit)||!heuristics_var_active(k,var_of(lit));
  });
  bump(k,advice);
  session.work=work.used;session.last_external=uint32_t(k.external_to_internal.size());
  return finish();
}
} // namespace tactix::shared_nonlinear_probe
