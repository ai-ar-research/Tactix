#pragma once

#include "diagnostics/rule_journal.hpp"
#include "rules/registry.hpp"
#include "strategy/work_settings.hpp"

#include <optional>

namespace tactix::execution {

struct WorkSettingsRequest {
  std::optional<uint64_t> budget;
  std::optional<uint64_t> offset;
  std::optional<bool> bounded;
  std::optional<bool> midround;
};

inline void execute_action(Kernel &k, const WorkSettingsRequest &request) {
  if (request.budget) k.work_budget = *request.budget;
  if (request.offset) k.work_budget_offset = *request.offset;
  if (request.bounded) k.work_budget_bounded = *request.bounded;
  if (request.midround) k.midround_budget = *request.midround;
}

enum class WorkSettingsExit { Restore, ClearBudget };

// Policy chooses wires and lifetime. Native default doses clear owned budget
// wires; online applications restore the caller. Midround always restores.
class WorkSettingsScope {
public:
  WorkSettingsScope(Kernel &k, WorkSettingsRequest request,
                    WorkSettingsExit exit = WorkSettingsExit::Restore)
      : k_(k), request_(request), exit_(exit),
        saved_{k.work_budget, k.work_budget_offset, k.work_budget_bounded,
               k.midround_budget} {
    execute_action(k_, request_);
  }
  ~WorkSettingsScope() {
    const bool clear = exit_ == WorkSettingsExit::ClearBudget;
    if (request_.budget) k_.work_budget = clear ? 0 : *saved_.budget;
    if (request_.offset) k_.work_budget_offset = clear ? 0 : *saved_.offset;
    if (request_.bounded) k_.work_budget_bounded = clear ? false : *saved_.bounded;
    if (request_.midround) k_.midround_budget = *saved_.midround;
  }
  WorkSettingsScope(const WorkSettingsScope &) = delete;
  WorkSettingsScope &operator=(const WorkSettingsScope &) = delete;

private:
  Kernel &k_;
  WorkSettingsRequest request_;
  WorkSettingsExit exit_;
  WorkSettingsRequest saved_;
};

struct MaterialSettingsRequest {
  std::optional<bool> touch_filter;
  std::optional<bool> likely_kept_redundant;
};

inline void execute_action(Kernel &k, const MaterialSettingsRequest &request) {
  if (request.touch_filter) k.touch_filter = *request.touch_filter;
  if (request.likely_kept_redundant)
    k.touch_redundant = *request.likely_kept_redundant;
}

// Preserve native DefaultPolicy's clear-on-exit sink. A nested borrower may
// explicitly request restoration instead; this is lifetime, not scheduling.
class TickSinkScope {
public:
  TickSinkScope(Kernel &k, uint64_t Stats::*counter, bool restore = false)
      : k_(k), saved_(k.tick_sink), restore_(restore) {
    k_.tick_sink = counter ? &(k_.stats.*counter) : nullptr;
  }
  ~TickSinkScope() { k_.tick_sink = restore_ ? saved_ : nullptr; }
  TickSinkScope(const TickSinkScope &) = delete;
  TickSinkScope &operator=(const TickSinkScope &) = delete;

private:
  Kernel &k_;
  uint64_t *saved_;
  bool restore_;
};

#ifdef TACTIX_ENABLE_RULE_JOURNAL
class RuleJournalMeterScope {
public:
  explicit RuleJournalMeterScope(RuleJournalMeter meter) {
    rule_journal_set_effort_meter(meter);
  }
  ~RuleJournalMeterScope() { rule_journal_set_effort_meter(nullptr); }
  RuleJournalMeterScope(const RuleJournalMeterScope &) = delete;
  RuleJournalMeterScope &operator=(const RuleJournalMeterScope &) = delete;
};
#endif

template <class Apply>
ApplyResult metered_rule_apply(Kernel &k, uint64_t (*meter)(const Kernel &),
                               Apply &&apply) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  RuleJournalMeterScope journal_meter(meter);
#else
  (void)meter;
#endif
  (void)k;
  return apply();
}

} // namespace tactix::execution

namespace tactix::strategy {

inline std::optional<bool> material_override(SettingOverride value) {
  if (value == SettingOverride::Disable) return false;
  if (value == SettingOverride::Enable) return true;
  return std::nullopt;
}

class ScopedRuleMaterialSettings {
public:
  ScopedRuleMaterialSettings(Kernel &k, const RuleMaterialSettings &settings)
      : k_(k), saved_filter_(k.touch_filter), saved_redundant_(k.touch_redundant) {
    execution::execute_action(k_, execution::MaterialSettingsRequest{
        material_override(settings.touch_filter),
        material_override(settings.likely_kept_redundant)});
  }
  ~ScopedRuleMaterialSettings() {
    k_.touch_filter = saved_filter_;
    k_.touch_redundant = saved_redundant_;
  }
  ScopedRuleMaterialSettings(const ScopedRuleMaterialSettings &) = delete;
  ScopedRuleMaterialSettings &operator=(const ScopedRuleMaterialSettings &) = delete;

private:
  Kernel &k_;
  bool saved_filter_, saved_redundant_;
};

class ScopedRuleWorkBudget {
public:
  ScopedRuleWorkBudget(Kernel &k, const RuleWorkBudget &budget)
      : scope_(k, request(budget)) {}
  ScopedRuleWorkBudget(const ScopedRuleWorkBudget &) = delete;
  ScopedRuleWorkBudget &operator=(const ScopedRuleWorkBudget &) = delete;

private:
  static execution::WorkSettingsRequest request(const RuleWorkBudget &budget) {
    if (budget.mode == RuleWorkBudgetMode::Inherit) return {};
    const bool bounded = budget.mode == RuleWorkBudgetMode::Bounded;
    return {bounded ? budget.amount : 0, 0, bounded, bounded};
  }
  execution::WorkSettingsScope scope_;
};

} // namespace tactix::strategy
