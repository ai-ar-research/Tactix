#pragma once

#include "policies/default_inprocess.hpp"
#include "strategy/controller_observation.hpp"
#include "strategy/policy_completion.hpp"

// Factor bounds published in the phase-entry record's high 32 argument bits.
struct PhaseEntryBounds final : tactix::strategy::controller::FeedbackReceiver {
  std::vector<uint32_t> seen;
  void after_action(
      const tactix::strategy::controller::ActionFeedback &record) override {
    if (record.identity == "InprocessPhaseEntry")
      seen.push_back(static_cast<uint32_t>(record.instance >> 32));
  }
  void search_interval(
      const tactix::strategy::controller::SearchIntervalFeedback &) override {}
};

inline int observed_phase(tactix::DefaultInprocessScheduler &scheduler,
                          tactix::Kernel &k, tactix::SlotGroup group,
                          PhaseEntryBounds &bounds) {
  namespace controller = tactix::strategy::controller;
  tactix::execution::PolicyCompletion completion(k, &bounds,
      [](const tactix::Kernel &kernel, const void *) {
        return controller::observe_kernel(kernel);
      }, nullptr);
  completion.begin();
  const int result = scheduler.run_phase(k, group, nullptr, &completion);
  completion.finish(result, result == 20 ? controller::StopReason::Unsat
                                         : controller::StopReason::Completed);
  return result;
}
