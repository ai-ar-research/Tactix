// M4 Backjump (plan §5.2): backtrack toward the pending learned clause's
// assertion level, then assert its UIP literal with the learned clause as
// reason. Owns all trail (τ) changes of the conflict-analysis pair.
// Proof delta: none.
//
// Instance space (API-freeze decision, 2026-07-14): the target level.
// pending.jump_level is the MINIMUM sound target (below it the clause is
// no longer asserting); every level in [jump_level, current level] is a
// legal instance — chronological backtracking (kissat backtrack.c
// chrono) is target = conflict level - 1, a pure policy choice here.
// The canonical instance is the minimum (classic non-chronological CDCL).
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool backjump_guard(const Kernel &);

// N1 instance surface: backtrack to `target_level` — clamped into
// [pending.jump_level, current level]; learned units always assert at
// the root — then assert the pending literal.
ApplyResult backjump_apply_to(Kernel &, uint32_t target_level);

// canonical instance: target = pending.jump_level
ApplyResult backjump_apply(Kernel &);

} // namespace tactix
