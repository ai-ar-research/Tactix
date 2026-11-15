#pragma once

#include "strategy/policy_feedback.hpp"

#include <vector>

// Copies action feedback in arrival order; ignores search intervals.
struct ActionFeedbackCapture final
    : tactix::strategy::controller::FeedbackReceiver {
  std::vector<tactix::strategy::controller::ActionFeedback> actions;
  void after_action(
      const tactix::strategy::controller::ActionFeedback &record) override {
    actions.push_back(record);
  }
  void search_interval(
      const tactix::strategy::controller::SearchIntervalFeedback &) override {}
};
