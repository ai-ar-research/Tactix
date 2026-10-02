#pragma once

#include "rules/sweep/sweep.hpp"

namespace tactix::execution {

// Discovery profile and seed selection are policy choices. Canonical execution
// uses the exact supplied profile, without the standalone reference ramp.
struct SweepRequest {
  const strategy::SweepParameters &parameters;
  uint64_t seed = INSTANCE_CANONICAL;
};

inline ApplyResult execute_action(Kernel &k, const SweepRequest &request) {
  return request.seed == INSTANCE_CANONICAL
             ? sweep_apply_exact_limits(k, request.parameters)
             : sweep_try_with_limits(k, static_cast<Var>(request.seed),
                                     request.parameters);
}

} // namespace tactix::execution
