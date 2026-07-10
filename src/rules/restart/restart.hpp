// S1 Restart (plan §5.2): abandon the current assignment prefix and return
// to the root, keeping all learned clauses and heuristic state. Interval
// follows Knuth's reluctant-doubling (Luby) sequence. Proof delta: none.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool restart_guard(const Kernel &);
ApplyResult restart_apply(Kernel &);

} // namespace tactix
