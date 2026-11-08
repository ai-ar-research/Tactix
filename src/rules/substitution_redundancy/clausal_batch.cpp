#include "rules/substitution_redundancy/clausal_batch.hpp"
#include "rules/structural_action_guard.hpp"
#include "proof/proof.hpp"
#include <algorithm>
#include <climits>
#include <cstdlib>

namespace tactix {
RuleEligibility clausal_batch_guard(const Kernel &k) {
  return root_action_guard(k,true).clausal_certificate()
      .fact("not-terminated",!kernel_terminating(k)).finish();
}
bool clausal_batch_prepare(const Kernel &k,int32_t first_fresh,uint32_t fresh,
    std::vector<ClausalBatchStep> steps,uint64_t maximum_work,
    ClausalBatchPlan &out,ClausalReplay::Effort &effort) {
  const uint64_t extent=k.external_to_internal.size();
  if (!clausal_batch_guard(k) || !maximum_work || steps.empty() ||
      k.next_clause_id>uint64_t(INT64_MAX) ||
      extent+fresh>INT32_MAX || first_fresh<=0 || uint64_t(first_fresh)!=extent+1 ||
      steps.back().kind!=ClausalBatchKind::Rup || !steps.back().clause.empty()) return false;
  ClausalReplay check(uint32_t(extent+fresh),maximum_work,k.stop);
  auto finish=[&](bool success) {effort=check.effort();return success;};
  auto import=[&](const Lit *lits,uint32_t size) {
    std::vector<int32_t> clause;clause.reserve(size);
    for (uint32_t i=0;i<size;++i) clause.push_back(kernel_externalize(k,lits[i]));
    return check.import(clause)!=0;
  };
  for (Lit l=0;l<2*k.nvars;++l) {
    if (!check.account()) return finish(false);
    for (auto watch:k.watches.entries(l)) {
      if (!check.account()) return finish(false);
      if (!watch.binary() || l>watch.other()) continue;
      const Lit pair[]={l,watch.other()};
      if (!import(pair,2)) return finish(false);
    }
  }
  for (auto [ref,clause]:arena_records(k.arena)) {
    (void)ref;
    if (!check.account()) return finish(false);
    if (!clause->garbage() && !import(clause->begin(),clause->size)) return finish(false);
  }
  std::vector<ClausalReplay::Handle> handles(steps.size());
  uint64_t additions=0;
  for (size_t index=0;index<steps.size();++index) {
    if (!check.account()) return finish(false);
    const auto &s=steps[index];
    if (s.kind==ClausalBatchKind::Delete) {
      if (!s.clause.empty() || !s.delete_step || s.delete_step>index ||
          !check.erase(handles[s.delete_step-1])) return finish(false);
      handles[s.delete_step-1]=0;
      continue;
    }
    if ((s.clause.empty() && index+1!=steps.size()) || s.delete_step || (s.kind!=ClausalBatchKind::Rup && s.kind!=ClausalBatchKind::Rat)) return finish(false);
    // Reject malformed/tautological requests before proof writing. The engine
    // normalizes independently; the immutable program keeps its RAT pivot.
    auto normalized=s.clause;std::sort(normalized.begin(),normalized.end());
    for (size_t j=0;j<normalized.size();++j) {
      const int32_t l=normalized[j];
      if (!l || l==INT32_MIN || (j && l==normalized[j-1])) return finish(false);
      if (std::binary_search(normalized.begin(),normalized.end(),-l)) return finish(false);
      const uint32_t v=uint32_t(l<0 ? -l:l);
      if (v>extent+fresh || (v<=extent &&
          (kernel_internalize(k,l)==INVALID_LIT ||
           kernel_var_retired(k,var_of(kernel_internalize(k,l)))))) return finish(false);
    }
    // Prior proof-only helpers need not be kernel clauses. A newly declared
    // pivot cannot occur in that earlier checker context, so its complete
    // opposing population consists exactly of this batch's live additions.
    if (s.kind==ClausalBatchKind::Rat && (s.clause.empty() ||
        uint64_t(s.clause.front()<0 ? -int64_t(s.clause.front()) : s.clause.front())<=extent))
      return finish(false);
    if (s.kind==ClausalBatchKind::Rat ? !check.rat(s.clause) : !check.rup(s.clause)) return finish(false);
    if (!(handles[index]=check.import(s.clause))) return finish(false);
    if (++additions>uint64_t(INT64_MAX)-k.next_clause_id) return finish(false);
  }
  ClausalBatchPlan plan;
  plan.epoch_=k.strategy_epoch;plan.next_id_=k.next_clause_id;plan.nvars_=k.nvars;
  plan.extent_=uint32_t(extent);plan.fresh_=fresh;plan.steps_=std::move(steps);
  out=std::move(plan);return finish(true);
}
ApplyResult clausal_batch_commit(Kernel &k,const ClausalBatchPlan &plan) {
  if (!clausal_batch_guard(k) || k.strategy_epoch!=plan.epoch_ ||
      k.next_clause_id!=plan.next_id_ || k.nvars!=plan.nvars_ ||
      k.external_to_internal.size()!=plan.extent_) return ApplyResult::NoOp;
  // Complete validation precedes this irreversible proof-only transaction.
  // Input clauses are never deleted: only earlier program additions may be
  // retired, by exact identity. Success is terminal, with no SAT continuation.
  k.close_startup_certificate();Var first;
  if (!kernel_reserve_proof_variables(k,plan.fresh_,first) || first!=plan.extent_) std::abort();
  std::vector<uint64_t> ids(plan.steps_.size());
  for (size_t index=0;index<plan.steps_.size();++index) {
    const auto &s=plan.steps_[index];
    const auto width=uint32_t(s.clause.size());
    if (s.kind==ClausalBatchKind::Delete) {
      const auto &source=plan.steps_[s.delete_step-1];
      if (k.proof && !k.proof->del_external(k.next_clause_id,ids[s.delete_step-1],
            source.clause.data(),uint32_t(source.clause.size()))) std::abort();
    } else {
      const uint64_t id=ids[index]=++k.next_clause_id;
      if (k.proof && !(s.kind==ClausalBatchKind::Rat
          ? k.proof->add_external_rat(id,s.clause.data(),width)
          : k.proof->add_external_rup(id,s.clause.data(),width))) std::abort();
    }
  }
  k.inconsistent=true;return ApplyResult::Unsat;
}
} // namespace tactix
