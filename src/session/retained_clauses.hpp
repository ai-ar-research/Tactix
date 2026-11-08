#pragma once

#include "session/query_history.hpp"
#include "proof/parity_equation.hpp"

#include <csignal>
#include <memory>
#include <span>

namespace tactix {
class SubSolver;
namespace session {

// Caller-owned retention/capture allowances, not rule guards. Zero disables
// the optional service. Import is bounded separately from BCP validation work.
struct RetentionLimits {
  uint32_t maximum_variables = 0;
  uint64_t maximum_input_words = 0;
  uint32_t maximum_clauses = 0;
  uint64_t maximum_literals = 0;
  uint32_t maximum_clause_literals = 0;
  uint32_t maximum_pending_clauses = 0;
  uint64_t maximum_pending_literals = 0;
  bool enabled() const {
    return maximum_variables && maximum_input_words && maximum_clauses &&
           maximum_literals && maximum_clause_literals &&
           maximum_pending_clauses && maximum_pending_literals;
  }
};

enum class RetentionStatus : uint8_t {
  Accepted,
  AlreadyPresent,
  NotRup,
  Tautology,
  Invalid,
  Disabled,
  Exhausted,
  Interrupted,
  Stale
};

struct RetentionResult {
  RetentionStatus status = RetentionStatus::Invalid;
  uint64_t ticks = 0;
};

struct RetainedClause {
  uint64_t source_query = 0;
  size_t source_input_words = 0;
  std::vector<int32_t> literals;
  std::vector<PbpParityEquation> parity_equations; // empty denotes ordinary RUP
};

// Knowledge independently derived from permanent input plus earlier accepted
// records, by RUP or checked complete-XOR recipes. Assumptions, kernel facts and unchecked
// candidate clauses never enter the oracle as premises. Records retain their
// admission order, making their RUP deltas replayable in later query proofs.
// The bound QueryHistory must outlive this owner. Returned spans are borrowed
// and may be invalidated by a successful admission.
class RetainedClauses {
  friend class RestrictionKnowledge; // consume the checked prefix for SR validation
public:
  explicit RetainedClauses(RetentionLimits);
  ~RetainedClauses();
  RetainedClauses(const RetainedClauses &) = delete;
  RetainedClauses &operator=(const RetainedClauses &) = delete;
  RetentionResult admit(const QueryHistory &, uint64_t query,
                        std::span<const int32_t> clause, uint64_t ticks_budget,
                        const volatile sig_atomic_t *stop = nullptr);
  RetentionResult admit_parity(const QueryHistory &,uint64_t query,
      std::span<const PbpParityEquation>,std::span<const int32_t> clause,
      uint64_t ticks_budget,const volatile sig_atomic_t *stop=nullptr);
  std::span<const RetainedClause> clauses() const { return clauses_; }
  uint64_t literals() const { return literals_; }
  uint64_t recipe_literals() const { return recipe_literals_; }
  const RetentionLimits &limits() const { return limits_; }

private:
  RetentionStatus prepare(const QueryHistory &, const QueryRecord &,
                          const volatile sig_atomic_t *stop);
  RetentionLimits limits_;
  const QueryHistory *history_ = nullptr;
  size_t input_words_ = 0;
  uint64_t literals_ = 0;
  uint64_t recipe_literals_ = 0;
  std::unique_ptr<SubSolver> oracle_;
  std::vector<RetainedClause> clauses_;
};

} // namespace session
} // namespace tactix
