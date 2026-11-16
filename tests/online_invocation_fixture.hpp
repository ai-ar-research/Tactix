#pragma once
// Native online-invocation setup shared by composition and rule API tests.
#include "strategy/online_inprocess.hpp"

#include <utility>

inline tactix::strategy::OnlineInvocation online_invocation(
    tactix::strategy::StepId id, std::string rule) {
  tactix::strategy::OnlineInvocation step;
  step.id = id;
  step.rule = std::move(rule);
  return step;
}
