// M4 Backjump (plan §5.2): backtrack toward the pending learned clause's
// assertion level, then assert its UIP literal with the learned clause as
// reason. Owns all trail (τ) changes of the conflict-analysis pair.
// Proof delta: none. Not a registered rule — fused into M3 ConflictAnalysis;
// see docs/calculus.md §3 and docs/implementation-notes.md §4.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool backjump_guard(const Kernel &);

// N1 instance surface: under true-level assignment every level in
// [pending.jump_level, level-1] is a sound target, so `target_level` is
// clamped into that range and every instance stays total.
ApplyResult backjump_apply_to(Kernel &, uint32_t target_level);

// canonical instance: target = pending.jump_level
ApplyResult backjump_apply(Kernel &);

} // namespace tactix
