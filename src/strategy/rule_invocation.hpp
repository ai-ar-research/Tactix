#pragma once

// Policy-neutral vocabulary for one safe rule invocation.  A completed rule
// action remains atomic: budgets and cancellation are observed only between
// applications, never in the middle of a proof/reconstruction transaction.
#include "rules/registry.hpp"
#include "strategy/execution_budget.hpp"

#include <cstdint>

namespace tactix::strategy {

enum class InvocationGranularity : uint8_t {
  OneInstance,
  SelectedBatch,
  CompleteCurrentPass,
  CanonicalApplication,
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
  // Monotonic across shared registry and public-API transitions. It prevents
  // an explicit selection from surviving an ABA return to the same trail or
  // formula shape between advance calls.
  uint64_t strategy_epoch = 0;
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
  // Intermediate producer progress which changed no solver state. A caller
  // may use these deterministic units to extend a discovery allowance and
  // retry the same pending step.
  uint64_t discovery_progress_units = 0;
  // The pending generic graph producer has not yet received its single entry
  // window.  The sequence remains on this step while its owner decides whether
  // to extend the shared allowance.
  bool discovery_entry_required = false;
  // Generic graph discovery exhausted its useful allowance without an
  // unpublished growing prefix.
  bool discovery_exhausted = false;
  // True means every application selected at this request's boundary was
  // consumed.  It is deliberately not a fixpoint claim.
  bool selection_complete = false;
  StateVersion before;
  StateVersion after;
  uint64_t configuration_revision = 0;
};

} // namespace tactix::strategy
