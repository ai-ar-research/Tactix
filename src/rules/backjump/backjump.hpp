// M4 Backjump (plan §5.2): backtrack toward the pending clause's assertion
// level and assert its UIP. Owns τ changes of conflict analysis; fused into M3
// ConflictAnalysis (calculus §3, implementation-notes §4).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool backjump_guard(const Kernel &);

// Learned-unit assertion shared by Backjump and the level-one failed-literal
// batch; the unit leaves the decision structures before the bump.
void backjump_assign_learned_unit(Kernel &, Lit);

// N1 instance surface: targets clamp into [pending.jump_level, level-1].
ApplyResult backjump_apply_to(Kernel &, uint32_t target_level);

// canonical instance: target = pending.jump_level
ApplyResult backjump_apply(Kernel &);

} // namespace tactix
