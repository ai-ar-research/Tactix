#pragma once

// Typed native execution shared by policy schedulers. Requests describe the
// transition; scheduling, target selection, heuristic advice and clocks stay
// with the calling policy. These inline paths also serve static hot dispatch.
#include "rules/decide/decide.hpp"
#include "rules/restart/restart.hpp"

namespace tactix::execution {

enum class RestartPurpose : uint8_t { Scheduled, Preparation };

struct RestartRequest {
  uint32_t keep_level = 0;
  // Preparing a root-only operation abandons the trail without consuming a
  // scheduled-restart occasion. Preserve that distinction in native Stats.
  RestartPurpose purpose = RestartPurpose::Scheduled;
};

enum class DecisionOrigin : uint8_t { Policy, PublicAPI };

struct DecisionRequest {
  Lit literal = INVALID_LIT;
  DecisionOrigin origin = DecisionOrigin::Policy;
};

inline ApplyResult execute_action(Kernel &k, const RestartRequest &request) {
  return request.purpose == RestartPurpose::Scheduled
             ? restart_apply_to(k, request.keep_level)
             : restart_backtrack_to(k, request.keep_level);
}

inline ApplyResult execute_action(Kernel &k, const DecisionRequest &request) {
  return decide_apply(k, request.literal,
                      request.origin == DecisionOrigin::PublicAPI);
}

} // namespace tactix::execution
