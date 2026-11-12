#pragma once

// One safe rule invocation; budgets apply between atomic applications.
#include "rules/registry.hpp"
#include "strategy/execution_budget.hpp"

#include <cstdint>

namespace tactix::strategy {

enum class InvocationGranularity : uint8_t {
  OneInstance,
  SelectedBatch,
  CompleteCurrentPass,
  CanonicalApplication,
  // Controller-only placeholder, resolved by the native producer.
  NativeProducerOffer,
};

enum class InvocationStatus : uint8_t {
  Completed,
  ScopedFixpoint,
  NoChange,
  Inapplicable,
  BudgetExhausted,
  Interrupted,
  Conflict,
  Sat,
  Unsat,
  InvalidRequest,
  StaleRequest,
  ResourceExhausted,
  AccountingError,
};

struct StateVersion {
  uint64_t arena_generation = 0;
  uint64_t touch_epoch = 0;
  // Monotonic epoch so a selection cannot survive an ABA return.
  uint64_t strategy_epoch = 0;
  bool operator==(const StateVersion &) const = default;
};

inline StateVersion state_version(const Kernel &k) {
  return {k.arena_generation, k.touch_epoch, k.strategy_epoch};
}

struct InvocationResult {
  InvocationStatus status = InvocationStatus::InvalidRequest;
  ApplyResult rule_result = ApplyResult::NoOp;
  BudgetStatus budget_status = BudgetStatus::Ready;
  WorkUnit work_unit = WorkUnit::Applications;
  uint64_t work = 0;
  uint64_t applications = 0;
  uint64_t step_id = 0;
  uint32_t rule_id = UINT32_MAX;
  uint64_t instance = INSTANCE_CANONICAL;
  bool made_rule_progress = false;
  // Producer progress without state change; may extend an allowance.
  uint64_t discovery_progress_units = 0;
  // The generic graph producer still awaits its entry window.
  bool discovery_entry_required = false;
  // Generic graph discovery exhausted its useful allowance.
  bool discovery_exhausted = false;
  // Every selected application was consumed (not a fixpoint claim).
  bool selection_complete = false;
  StateVersion before;
  StateVersion after;
  uint64_t configuration_revision = 0;
};

} // namespace tactix::strategy
