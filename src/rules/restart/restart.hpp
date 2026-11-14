// S1 Restart (plan §5.2): abandon a caller-selected assignment suffix while
// keeping all learned clauses. Restart timing and reuse depth are policy
// decisions. Proof delta: none.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

RuleEligibility restart_guard(const Kernel &);

// Shared trail abandonment for non-Restart callers (Kissat Rephase); no restart
// statistics or reuse accounting.
ApplyResult restart_backtrack_to(Kernel &, uint32_t keep_level);

// N1 instance surface: keep the first `keep_level` levels (clamped; 0 = full).
ApplyResult restart_apply_to(Kernel &, uint32_t keep_level);

// canonical instance: full restart (keep level zero)
ApplyResult restart_apply(Kernel &);

} // namespace tactix
