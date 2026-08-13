// S1 Restart (plan §5.2): abandon a caller-selected assignment suffix while
// keeping all learned clauses. Restart timing and reuse depth are policy
// decisions. Proof delta: none.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

RuleEligibility restart_guard(const Kernel &);

// N1 instance surface: restart keeping exactly the first `keep_level`
// decision levels — every keep-level is a legal instance (clamped to the
// current level; 0 = full restart). The CHOICE of keep-level is the
// policy's; this module owns only HOW (plan §5).
ApplyResult restart_apply_to(Kernel &, uint32_t keep_level);

// canonical instance: full restart (keep level zero)
ApplyResult restart_apply(Kernel &);

} // namespace tactix
