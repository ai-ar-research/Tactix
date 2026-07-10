// M4 Backjump (plan §5.2): non-chronological backtracking to the pending
// learned clause's assertion level, then asserting its UIP literal with
// the learned clause as reason. Owns all trail (τ) changes of the
// conflict-analysis pair. Proof delta: none.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool backjump_guard(const Kernel &);
ApplyResult backjump_apply(Kernel &);

} // namespace tactix
