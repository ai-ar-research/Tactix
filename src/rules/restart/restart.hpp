// S1 Restart (plan §5.2): abandon the current assignment prefix (a
// heuristically-reusable prefix is kept), keeping all learned clauses and
// heuristic state. WHEN to restart is policy state (rev 17: focused = EMA
// trigger, stable = reluctant doubling, both in DefaultPolicy). Proof
// delta: none.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool restart_guard(const Kernel &);
ApplyResult restart_apply(Kernel &);

} // namespace tactix
