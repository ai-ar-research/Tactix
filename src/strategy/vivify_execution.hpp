#pragma once

#include "strategy/rule_execution.hpp"
#include "strategy/vivify_parameters.hpp"
#include "strategy/work_execution.hpp"

namespace tactix::execution {

// The caller selects the typed band (VivifyParameters scope plus its band
// ledger) and native currency. One band performs the same registered rule
// action as an online canonical request under those parameters.
struct VivifyBandRequest {
  const strategy::VivifyParameters &band;
  uint64_t Stats::*tick_counter = nullptr;
  uint64_t (*meter)(const Kernel &) = nullptr;
};

class VivifyDoseScope {
public:
  explicit VivifyDoseScope(Kernel &k)
      : k_(k), work_(k, {k.work_budget, k.work_budget_offset, true, std::nullopt},
                     WorkSettingsExit::ClearBudget) {}
  ~VivifyDoseScope() { k_.vivify_scope = 0; }
  VivifyDoseScope(const VivifyDoseScope &) = delete;
  VivifyDoseScope &operator=(const VivifyDoseScope &) = delete;

private:
  Kernel &k_;
  WorkSettingsScope work_;
};

inline ApplyResult execute_action(Kernel &k, const RuleVTable &rule,
                                  const VivifyBandRequest &request) {
  assert(request.band.band);
  k.vivify_scope = request.band.scope;
  execute_action(k, WorkSettingsRequest{request.band.band_budget,
                                       request.band.band_offset, true,
                                       std::nullopt});
  TickSinkScope sink(k, request.tick_counter);
  return metered_rule_apply(k, request.meter, [&] {
    return execute_action(k, RegisteredRuleRequest{rule});
  });
}

} // namespace tactix::execution
