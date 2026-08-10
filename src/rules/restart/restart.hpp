// S1 Restart (plan §5.2): abandon the current assignment prefix (a
// heuristically-reusable prefix is kept), keeping all learned clauses and
// heuristic state. WHEN to restart is policy state (focused = EMA
// trigger, stable = reluctant doubling, both in DefaultPolicy). Proof
// delta: none.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool restart_guard(const Kernel &);

// instance ranking (§5.1): the canonical keep-level — the trail prefix the
// active heuristic (VMTF stamps / EVSIDS scores) would immediately
// re-decide, so abandoning it is pure waste (kissat restart.c
// reuse_focused_trail / reuse_stable_trail). Mutates only the ranking's
// search caches (VMTF cursor, lazy heap pops) — heuristic H-state.
uint32_t restart_reuse_level(Kernel &);
// Policy-supplied counterpart: score reuse against the variable selected by
// its active brancher. DefaultPolicy needs this because Kissat's restart path
// calls `kissat_next_decision_variable`, which can consume a random draw.
uint32_t restart_reuse_level_for(Kernel &, Var next);

// N1 instance surface: restart keeping exactly the first `keep_level`
// decision levels — every keep-level is a legal instance (clamped to the
// current level; 0 = full restart). The CHOICE of keep-level is the
// policy's; this module owns only HOW (plan §5).
ApplyResult restart_apply_to(Kernel &, uint32_t keep_level);

// canonical instance: keep the reusable prefix
ApplyResult restart_apply(Kernel &);

} // namespace tactix
