#include "session/retained_clauses.hpp"

#include "engine/subsolver.hpp"
#include "kernel/types.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace tactix::session {

RetainedClauses::RetainedClauses(RetentionLimits limits) : limits_(limits) {}
RetainedClauses::~RetainedClauses() = default;

RetentionStatus RetainedClauses::prepare(const QueryHistory &history,
                                        const QueryRecord &query,
                                        const volatile sig_atomic_t *stop) {
  if (history_ && history_ != &history) return RetentionStatus::Stale;
  if (input_words_ > query.input_words) return RetentionStatus::Stale;
  if (query.formula_variables > limits_.maximum_variables ||
      query.input_words > limits_.maximum_input_words)
    return RetentionStatus::Exhausted;
  // The sub-solver's clause IDs and arena offsets are uint32_t. Bound their
  // complete import before allocating, including the certified old records.
  if (query.clauses >= UINT32_MAX ||
      clauses_.size() >= UINT32_MAX - query.clauses)
    return RetentionStatus::Exhausted;
  const uint64_t ids = query.clauses + clauses_.size();
  if (query.input_words > UINT32_MAX ||
      literals_ > UINT32_MAX ||
      query.input_words + 2 * ids + literals_ >= UINT32_MAX)
    return RetentionStatus::Exhausted;
  if (oracle_ && input_words_ == query.input_words) return RetentionStatus::Accepted;
  if (oracle_) {
    std::vector<SubLit> clause;
    // Commit complete original clauses only. An interrupted suffix can resume
    // from this cursor without reinstalling premises or discarding proof IDs.
    for (size_t at=input_words_;at<query.input_words;++at) {
      if (stop && *stop) return RetentionStatus::Interrupted;
      const int32_t literal=history.input()[at];
      if (literal) clause.push_back(internalize(literal));
      else {
        if (!oracle_->append_input(query.formula_variables,clause))
          return RetentionStatus::Exhausted;
        input_words_=at+1;
        clause.clear();
      }
    }
    return RetentionStatus::Accepted;
  }
  auto next = std::make_unique<SubSolver>();
  next->reset(query.formula_variables);
  std::vector<SubLit> clause;
  for (const int32_t literal : history.input().first(query.input_words)) {
    if (stop && *stop) return RetentionStatus::Interrupted;
    if (literal) clause.push_back(internalize(literal));
    else {
      next->add_clause(clause);
      clause.clear();
    }
  }
  for (const auto &record : clauses_) {
    if (stop && *stop) return RetentionStatus::Interrupted;
    if (record.source_input_words > query.input_words) return RetentionStatus::Stale;
    clause.clear();
    for (const int32_t literal : record.literals)
      clause.push_back(internalize(literal));
    next->add_clause(clause);
  }
  history_ = &history;
  input_words_ = query.input_words;
  oracle_ = std::move(next);
  return RetentionStatus::Accepted;
}

RetentionResult RetainedClauses::admit(const QueryHistory &history, uint64_t id,
                                      std::span<const int32_t> clause,
                                      uint64_t ticks_budget,
                                      const volatile sig_atomic_t *stop) {
  if (!limits_.enabled()) return {RetentionStatus::Disabled};
  if (history_ && history_ != &history) return {RetentionStatus::Stale};
  const QueryRecord *query = history.query(id);
  if (!query) return {RetentionStatus::Stale};
  if (input_words_ > query->input_words) return {RetentionStatus::Stale};
  if (stop && *stop) return {RetentionStatus::Interrupted};
  if (!ticks_budget || clause.size() > limits_.maximum_clause_literals)
    return {RetentionStatus::Exhausted};
  for (const int32_t literal : clause)
    if (!literal || literal == INT32_MIN ||
        external_variable_of(literal) > query->formula_variables)
      return {RetentionStatus::Invalid};
  std::vector<int32_t> normalized(clause.begin(), clause.end());
  std::sort(normalized.begin(), normalized.end(), external_literal_less);
  normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());
  for (size_t i = 1; i < normalized.size(); ++i)
    if (normalized[i - 1] == -normalized[i]) return {RetentionStatus::Tautology};
  for (const auto &old : clauses_)
    if (old.literals == normalized) return {RetentionStatus::AlreadyPresent};
  if (clauses_.size() >= limits_.maximum_clauses ||
      normalized.size() > limits_.maximum_literals - literals_ - recipe_literals_)
    return {RetentionStatus::Exhausted};
  const RetentionStatus prepared = prepare(history, *query, stop);
  if (prepared != RetentionStatus::Accepted) return {prepared};
  std::vector<SubLit> internal;
  internal.reserve(normalized.size());
  for (const int32_t literal : normalized) internal.push_back(internalize(literal));
  const uint64_t before = oracle_->ticks();
  const SubRupAdmission admitted = oracle_->admit_rup(internal, ticks_budget, stop);
  const uint64_t ticks = oracle_->ticks() - before;
  switch (admitted) {
  case SubRupAdmission::Accepted:
    literals_ += normalized.size();
    clauses_.push_back({query->id, query->input_words, std::move(normalized), {}});
    return {RetentionStatus::Accepted, ticks};
  case SubRupAdmission::NotRup: return {RetentionStatus::NotRup, ticks};
  case SubRupAdmission::Tautology: return {RetentionStatus::Tautology, ticks};
  case SubRupAdmission::Interrupted: return {RetentionStatus::Interrupted, ticks};
  case SubRupAdmission::Budget:
  case SubRupAdmission::Exhausted: return {RetentionStatus::Exhausted, ticks};
  default: return {RetentionStatus::Invalid, ticks};
  }
}

RetentionResult RetainedClauses::admit_parity(const QueryHistory &history,uint64_t id,
    std::span<const PbpParityEquation> equations,std::span<const int32_t> clause,
    uint64_t budget,const volatile sig_atomic_t *stop) {
  if (!limits_.enabled()) return {RetentionStatus::Disabled};
  const QueryRecord *query=history.query(id);
  if (!query || (history_ && history_!=&history) || input_words_>query->input_words)
    return {RetentionStatus::Stale};
  if (stop && *stop) return {RetentionStatus::Interrupted};
  if (!budget || equations.size()>limits_.maximum_clauses ||
      clause.size()>limits_.maximum_clause_literals) return {RetentionStatus::Exhausted};
  if (equations.empty()) return {RetentionStatus::Invalid};
  std::vector<int32_t> normalized(clause.begin(),clause.end());
  for (int32_t literal:normalized)
    if (!literal || literal==INT32_MIN || external_variable_of(literal)>query->formula_variables)
      return {RetentionStatus::Invalid};
  std::sort(normalized.begin(),normalized.end(),external_literal_less);
  normalized.erase(std::unique(normalized.begin(),normalized.end()),normalized.end());
  for (size_t i=1;i<normalized.size();++i)
    if (normalized[i]==-normalized[i-1]) return {RetentionStatus::Tautology};
  uint64_t words=0, source_clauses=0, source_words=0;
  std::set<int32_t> odd;
  bool rhs=false;
  for (const auto &row:equations) {
    if (!valid_parity_source(row,query->formula_variables)) return {RetentionStatus::Invalid};
    if (row.variables.size()>limits_.maximum_clause_literals) return {RetentionStatus::Exhausted};
    if (row.variables.size()+1>limits_.maximum_literals-std::min(words,limits_.maximum_literals))
      return {RetentionStatus::Exhausted};
    words+=row.variables.size()+1;
    const uint64_t count=uint64_t(1)<<(row.variables.size()-1);
    source_clauses+=count;source_words+=count*(row.variables.size()+2);
    rhs^=row.rhs;
    for (int32_t variable:row.variables)
      if (!odd.erase(variable)) odd.insert(variable);
  }
  bool falsifying=false;
  for (int32_t literal:normalized) {
    const int32_t variable=external_variable_of(literal);
    if (odd.erase(variable)) falsifying^=literal<0;
  }
  // F entails the sum equation. A falsifying assignment of this clause must
  // assign its entire support and violate its parity, including canceled extras.
  if (!odd.empty() || falsifying==rhs) return {RetentionStatus::Invalid};
  for (const auto &old:clauses_)
    if (old.literals==normalized) return {RetentionStatus::AlreadyPresent};
  const __uint128_t payload=__uint128_t(literals_)+recipe_literals_+normalized.size()+words;
  const __uint128_t import=__uint128_t(query->input_words)+literals_+
      2*(__uint128_t(query->clauses)+clauses_.size())+source_words;
  if (clauses_.size()>=limits_.maximum_clauses || payload>limits_.maximum_literals ||
      import>=UINT32_MAX || source_clauses+query->clauses+clauses_.size()>=UINT32_MAX)
    return {RetentionStatus::Exhausted};
  const RetentionStatus prepared=prepare(history,*query,stop);
  if (prepared!=RetentionStatus::Accepted) return {prepared};
  uint64_t ticks=0;
  RetentionStatus status=RetentionStatus::Accepted;
  for (const auto &row:equations) {
    if (!visit_parity_source(row,[&](const std::vector<int32_t> &source) {
      if (stop && *stop) {status=RetentionStatus::Interrupted;return false;}
      const uint64_t cost=source.size()+1;
      if (ticks>=budget || cost>=budget-ticks) {status=RetentionStatus::Exhausted;return false;}
      ticks+=cost;
      std::vector<SubLit> body;for (int32_t literal:source) body.push_back(internalize(literal));
      const uint64_t before=oracle_->ticks();
      const auto result=oracle_->admit_rup(body,budget-ticks,stop);
      ticks+=std::min(oracle_->ticks()-before,UINT64_MAX-ticks);
      switch (result) {
      case SubRupAdmission::Accepted:return true;
      case SubRupAdmission::NotRup:status=RetentionStatus::NotRup;break;
      case SubRupAdmission::Interrupted:status=RetentionStatus::Interrupted;break;
      case SubRupAdmission::Budget:
      case SubRupAdmission::Exhausted:status=RetentionStatus::Exhausted;break;
      default:status=RetentionStatus::Invalid;break;
      }
      return false;
    })) break;
  }
  // Partial source admissions are not retained knowledge. Rebuild the next
  // oracle from F and committed records, whether this recipe succeeds or fails.
  oracle_.reset();
  if (status!=RetentionStatus::Accepted) return {status,ticks};
  if (ticks>=budget || words+normalized.size()+1>budget-ticks) return {RetentionStatus::Exhausted,ticks};
  ticks+=words+normalized.size()+1;
  RetainedClause record{query->id,query->input_words,std::move(normalized),{equations.begin(),equations.end()}};
  literals_+=record.literals.size();recipe_literals_+=words;
  clauses_.push_back(std::move(record));
  return {RetentionStatus::Accepted,ticks};
}

} // namespace tactix::session
