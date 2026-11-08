#include "session/query_history.hpp"

#include <algorithm>
#include <cinttypes>
#include <limits>
#include <utility>

namespace tactix::session {
namespace {

bool valid_literal(int32_t literal) {
  return literal != 0 && literal != std::numeric_limits<int32_t>::min();
}

uint32_t variable(int32_t literal) {
  // All callers have excluded INT32_MIN, so negation is representable.
  return static_cast<uint32_t>(literal < 0 ? -literal : literal);
}

} // namespace

InputStatus QueryHistory::add(int32_t literal_or_zero) {
  if (state_ == InputState::Solving)
    return InputStatus::InvalidState;
  if (literal_or_zero == std::numeric_limits<int32_t>::min())
    return InputStatus::InvalidLiteral;
  if (literal_or_zero) {
    pending_clause_.push_back(literal_or_zero);
  } else {
    if (clauses_ == std::numeric_limits<uint64_t>::max() ||
        input_.size() == input_.max_size() ||
        pending_clause_.size() > input_.max_size() - input_.size() - 1)
      return InputStatus::Exhausted;
    input_.insert(input_.end(), pending_clause_.begin(), pending_clause_.end());
    input_.push_back(0);
    for (const int32_t literal : pending_clause_)
      formula_variables_ = std::max(formula_variables_, variable(literal));
    pending_clause_.clear();
    ++clauses_;
  }
  state_ = InputState::Input;
  return InputStatus::Ok;
}

InputStatus QueryHistory::assume(int32_t literal) {
  if (state_ == InputState::Solving)
    return InputStatus::InvalidState;
  if (!valid_literal(literal))
    return InputStatus::InvalidLiteral;
  pending_assumptions_.push_back(literal);
  state_ = InputState::Input;
  return InputStatus::Ok;
}

InputStatus QueryHistory::begin_query() {
  if (state_ == InputState::Solving)
    return InputStatus::InvalidState;
  if (!pending_clause_.empty())
    return InputStatus::UnterminatedClause;
  if (queries_.size() == std::numeric_limits<uint64_t>::max() ||
      pending_assumptions_.size() >
          std::numeric_limits<uint64_t>::max() - clauses_)
    return InputStatus::Exhausted;
  QueryRecord record;
  record.id = static_cast<uint64_t>(queries_.size()) + 1;
  record.clauses = clauses_;
  record.input_words = input_.size();
  record.formula_variables = formula_variables_;
  record.query_variables = formula_variables_;
  for (const int32_t literal : pending_assumptions_)
    record.query_variables = std::max(record.query_variables, variable(literal));
  record.assumptions.swap(pending_assumptions_);
  queries_.push_back(std::move(record));
  state_ = InputState::Solving;
  return InputStatus::Ok;
}

InputStatus QueryHistory::finish_query(uint64_t id, QueryResult result) {
  if (state_ != InputState::Solving)
    return InputStatus::InvalidState;
  if (id != queries_.back().id)
    return InputStatus::InvalidQuery;
  InputState next;
  switch (result) {
  case QueryResult::Unknown: next = InputState::Input; break;
  case QueryResult::Sat: next = InputState::Sat; break;
  case QueryResult::Unsat: next = InputState::Unsat; break;
  default: return InputStatus::InvalidResult;
  }
  queries_.back().result = result;
  queries_.back().completed = true;
  state_ = next;
  return InputStatus::Ok;
}

const QueryRecord *QueryHistory::query(uint64_t id) const {
  if (!id || id > queries_.size())
    return nullptr;
  return &queries_[static_cast<size_t>(id - 1)];
}

const QueryRecord *QueryHistory::active_query() const {
  return state_ == InputState::Solving ? &queries_.back() : nullptr;
}

const QueryRecord *QueryHistory::result_query() const {
  return state_ == InputState::Sat || state_ == InputState::Unsat
             ? &queries_.back() : nullptr;
}

bool QueryHistory::write_query_dimacs(uint64_t id, FILE *out) const {
  const QueryRecord *record = query(id);
  if (!record || !out)
    return false;
  const uint64_t count = record->clauses + record->assumptions.size();
  if (std::fprintf(out, "p cnf %" PRIu32 " %" PRIu64 "\n",
                   record->query_variables, count) < 0)
    return false;
  for (size_t i = 0; i < record->input_words; ++i) {
    const int32_t literal = input_[i];
    if (std::fprintf(out, literal ? "%" PRId32 " " : "%" PRId32 "\n",
                     literal) < 0)
      return false;
  }
  for (const int32_t literal : record->assumptions)
    if (std::fprintf(out, "%" PRId32 " 0\n", literal) < 0)
      return false;
  return !std::ferror(out);
}

} // namespace tactix::session
