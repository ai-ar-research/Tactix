#pragma once

#include "rules/sweep/sweep.hpp"

namespace tactix::execution {

// Profile and seeds are policy choices; no standalone reference ramp.
struct SweepRequest {
  const strategy::SweepParameters &parameters;
  uint64_t seed = INSTANCE_CANONICAL;
};

inline ApplyResult execute_action(Kernel &k, const SweepRequest &request) {
  return request.seed == INSTANCE_CANONICAL
             ? sweep_apply_with_limits(k, request.parameters)
             : sweep_try_with_limits(k, static_cast<Var>(request.seed),
                                     request.parameters);
}

} // namespace tactix::execution
