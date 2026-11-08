#pragma once

#include "session/derivation_history.hpp"
#include "session/retained_clauses.hpp"
#include <memory>
#include <set>

namespace tactix::session {

struct LearningSnapshot {
  uint64_t query = 0, considered = 0, licensed = 0, exported = 0;
  uint64_t rejected = 0, ticks = 0;
  bool incomplete = false, interrupted = false;
};

// Optional query service. Validate in a private F-only oracle and notify only at
// caller-selected action boundaries, never from a proof writer's callback.
// Neither accepted knowledge nor callback-buffer writes alter native search.
class LearnedClauseExport {
public:
  explicit LearnedClauseExport(RetentionLimits limits) : limits_(limits) {}
  void set(void *state, int maximum_length, void (*learn)(void *, int *)) {
    state_ = state; maximum_length_ = maximum_length; learn_ = learn;
  }
  void set_budget(uint64_t ticks) { budget_ = ticks; }
  bool enabled() const { return learn_ && maximum_length_ >= 0; }
  LearningSnapshot snapshot() const { return snapshot_; }
  void drain(const QueryHistory &, uint64_t query, uint64_t stream,
             std::span<const DerivationRecord>, uint64_t dropped,
             const volatile sig_atomic_t *stop = nullptr,
             bool (*poll)(void *) = nullptr, void *poll_state = nullptr);

private:
  RetentionLimits limits_;
  std::unique_ptr<RetainedClauses> oracle_;
  void *state_ = nullptr;
  void (*learn_)(void *, int *) = nullptr;
  int maximum_length_ = -1;
  uint64_t budget_ = UINT64_MAX, active_budget_ = 0, stream_ = 0;
  size_t cursor_ = 0;
  bool draining_ = false, exhausted_ = false;
  LearningSnapshot snapshot_;
  std::set<std::vector<int32_t>> emitted_;
};
} // namespace tactix::session
