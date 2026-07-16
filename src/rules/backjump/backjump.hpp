// M4 Backjump (plan §5.2): backtrack toward the pending learned clause's
// assertion level, then assert its UIP literal with the learned clause as
// reason. Owns all trail (τ) changes of the conflict-analysis pair.
// Proof delta: none.
//
// Instance space (API-freeze decision, 2026-07-14; narrowed rev 31):
// pending.jump_level is the assertion level and, under this kernel's
// current-level assignment, the ONLY sound target — a target above it
// stamps the asserting literal too deep, and a later partial restart can
// then strand its reason clause as an escaped unit (corpus 005). Genuine
// chronological backtracking (kissat backtrack.c chrono, target =
// conflict level - 1) needs true-level assignment and returns with the
// §5.4 kernel candidate. For now canonical == only sound == jump_level.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool backjump_guard(const Kernel &);

// N1 instance surface: backtrack toward `target_level` — clamped to the
// sound assertion level pending.jump_level (learned units assert at the
// root) — then assert the pending literal. See the instance-space note
// above for why higher targets are deferred to §5.4.
ApplyResult backjump_apply_to(Kernel &, uint32_t target_level);

// canonical instance: target = pending.jump_level
ApplyResult backjump_apply(Kernel &);

} // namespace tactix
