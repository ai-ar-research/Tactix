// Persistent input/query ownership shared by incremental frontends and all
// strategies. This is an input ledger, not a solver or a certificate checker.
// It is constructed only for an incremental session; the ordinary CLI and C
// API retain their current allocation and hot-loop behavior.
#pragma once

#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>

namespace tactix::session {

enum class InputState : uint8_t { Input, Solving, Sat, Unsat };
enum class QueryResult : uint8_t { Unknown = 0, Sat = 10, Unsat = 20 };
enum class InputStatus : uint8_t {
  Ok,
  InvalidLiteral,
  InvalidState,
  UnterminatedClause,
  InvalidQuery,
  InvalidResult,
  Exhausted
};

enum class ModelStatus : uint8_t {
  Valid,
  InvalidQuery,
  InvalidValue,
  FalsifiedClause,
  FalsifiedAssumption
};

struct ModelCheck {
  ModelStatus status = ModelStatus::Valid;
  // One-based raw clause or assumption index for a falsified constraint.
  uint64_t index = 0;
};

struct QueryRecord {
  uint64_t id = 0;
  uint64_t clauses = 0;
  size_t input_words = 0;
  uint32_t formula_variables = 0;
  uint32_t query_variables = 0;
  std::vector<int32_t> assumptions;
  // A result records the solver's answer only. Independent certificate/model
  // verification is a separate obligation and cannot be inferred from this.
  QueryResult result = QueryResult::Unknown;
  bool completed = false;
};

class QueryHistory {
public:
  // A history belongs to exactly one session. Copying/moving a live history
  // would invalidate pending query/reason ownership in its eventual adapters.
  QueryHistory() = default;
  QueryHistory(const QueryHistory &) = delete;
  QueryHistory &operator=(const QueryHistory &) = delete;
  QueryHistory(QueryHistory &&) = delete;
  QueryHistory &operator=(QueryHistory &&) = delete;

  InputState state() const { return state_; }
  uint64_t clauses() const { return clauses_; }
  uint32_t formula_variables() const { return formula_variables_; }
  std::span<const int32_t> input() const { return input_; }
  std::span<const int32_t> pending_clause() const { return pending_clause_; }
  std::span<const int32_t> pending_assumptions() const {
    return pending_assumptions_;
  }
  // Abort an adapter's queued cube when query admission failed. Never retract
  // an active query's premises; its owner must finish that query normally.
  bool discard_pending_assumptions() {
    if(state_==InputState::Solving)return false;
    pending_assumptions_.clear();return true;
  }
  size_t queries() const { return queries_.size(); }

  // Preserve exact order, multiplicity and clause boundaries. No normalization
  // is allowed here: proof input must still name duplicates and tautologies.
  // Only zero commits a clause to the permanent prefix. Successful add/assume
  // invalidates the previous model/core view, even for an empty clause.
  InputStatus add(int32_t literal_or_zero);
  InputStatus assume(int32_t literal);

  // Reject incomplete clauses without consuming assumptions or a query ID.
  // On success the assumptions move into this query, including for UNKNOWN.
  InputStatus begin_query();
  InputStatus finish_query(uint64_t id, QueryResult result);

  // Borrowed views remain valid until the next successful mutation. A record
  // ID always denotes its captured prefix, even after later clause additions.
  const QueryRecord *query(uint64_t id) const;
  const QueryRecord *active_query() const;
  const QueryRecord *result_query() const;

  // Canonical self-contained query input for model/proof checking: the exact
  // permanent prefix followed by assumptions as unit clauses. The stream is
  // caller-owned; success covers writing, not caller flush/close or checking.
  bool write_query_dimacs(uint64_t id, FILE *out) const;

  // Check reconstructed values against the original query, independently of
  // the simplified kernel or a propagator's approval. `value(variable)` reads
  // an external positive variable ID and must return -1 or +1 consistently.
  // Unmentioned variables need no value. Theory constraint model checking is
  // a separate required stage; this checks only CNF and assumptions.
  template <class Value> ModelCheck check_model(uint64_t id, Value &&value) const {
    const QueryRecord *record = query(id);
    if (!record)
      return {ModelStatus::InvalidQuery, 0};
    bool satisfied = false;
    uint64_t clause_index = 1;
    for (size_t i = 0; i < record->input_words; ++i) {
      const int32_t literal = input_[i];
      if (!literal) {
        if (!satisfied)
          return {ModelStatus::FalsifiedClause, clause_index};
        ++clause_index;
        satisfied = false;
        continue;
      }
      const auto assignment = value(literal < 0 ? -literal : literal);
      if (assignment != -1 && assignment != 1)
        return {ModelStatus::InvalidValue, 0};
      satisfied |= (literal > 0) == (assignment > 0);
    }
    for (size_t i = 0; i < record->assumptions.size(); ++i) {
      const int32_t literal = record->assumptions[i];
      const auto assignment = value(literal < 0 ? -literal : literal);
      if (assignment != -1 && assignment != 1)
        return {ModelStatus::InvalidValue, 0};
      if ((literal > 0) != (assignment > 0))
        return {ModelStatus::FalsifiedAssumption, static_cast<uint64_t>(i) + 1};
    }
    return {};
  }

private:
  InputState state_ = InputState::Input;
  uint64_t clauses_ = 0;
  uint32_t formula_variables_ = 0;
  std::vector<int32_t> input_;
  std::vector<int32_t> pending_clause_;
  std::vector<int32_t> pending_assumptions_;
  std::vector<QueryRecord> queries_;
};

} // namespace tactix::session
