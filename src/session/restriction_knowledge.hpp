#pragma once
#include "session/derivation_history.hpp"
#include "session/retained_clauses.hpp"

namespace tactix::session {

struct RestrictionStep {
  RetainedClause consequence;
  bool restriction = false;
  std::vector<int32_t> witness;
  std::vector<std::pair<int32_t,int32_t>> mappings;
};

// Query-entry proof program, never a permanent-F retention pool. Each
// restriction must repair its expanded prefix and preserve the new cube;
// subsequent consequences inherit that restriction's scope. No assumptions
// are imported as native roots or as premises of the private RUP oracle.
// The source/query stay stable during this owner's query-entry lifetime;
// consuming the query invalidates further admissions.
class RestrictionKnowledge {
public:
  RestrictionKnowledge(const QueryHistory &,const QueryRecord &,RetentionLimits);
  RetentionResult admit(const DerivationRecord &,uint64_t,
                        const volatile sig_atomic_t *stop=nullptr);
  RetentionResult admit(std::span<const int32_t>,uint64_t,
                        const volatile sig_atomic_t *stop=nullptr);
  std::span<const RestrictionStep> steps() const {return steps_;}
private:
  RetentionStatus synchronize(uint64_t &,uint64_t,const volatile sig_atomic_t *);
  const QueryHistory &source_;
  const QueryRecord &query_;
  RetentionLimits limits_;
  uint64_t words_=0;
  std::vector<RestrictionStep> steps_;
  std::unique_ptr<QueryHistory> prefix_;
  std::unique_ptr<RetainedClauses> oracle_;
};
} // namespace tactix::session
