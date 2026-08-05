// V4 TransitiveReduce (plan §5.2): transitive reduction of the
// binary implication graph. A binary clause (src ∨ dst) IS the BIG edge
// ¬src → dst (and, by contraposition, ¬dst → src). The edge is TRANSITIVELY
// REDUNDANT when dst is still reachable from ¬src along a path of OTHER
// binaries: that path is a resolution chain ¬src ⊨ x₁ ⊨ … ⊨ dst, so the
// surviving formula entails (src ∨ dst) and deleting it is EQUIVALENCE-
// preserving (F \ {e} ≡ F — no witness, no model ever gained; the property
// SubsumeForward's binary-subsumer shortcut now rests on, see
// subsume_forward.hpp's V4 reconciliation).
//
// This is the first rule to DELETE binaries (kernel_delete_binary);
// its whole proof delta is `delete`, which is why it was chosen as the
// primitive's introduction point (V3b Substitute reuses it — plan).
// Deleting needs no justification from any checker (a deletion only shrinks
// the clause set), so V4 emits no `add` line and needs no LRAT chain:
//   DRAT  `d src dst 0`
//   LRAT  `<line_id> d <del_id> 0`   (chains occur only on ADDITION lines)
// The redundancy argument is therefore a SOLVER-soundness obligation (do not
// enlarge the model set), not a proof obligation — and it is discharged by
// the surviving-path discipline below, not by the checker.
//
// References (plan §5 "evidence, not templates"; MIT, transliterated in
// essence, no code copied):
//  - kissat 4.0.4 transitive.c (transitive_reduce): per-candidate bounded BIG
//    walk from ¬src, stopping on dst (redundant) or on a both-polarities-
//    reached contradiction (failed literal); tombstone-then-compact so the
//    list being scanned is never mutated mid-walk.
//  - cadical transred.cpp: the same skeleton over the clause vector, plus the
//    cheaper-direction search and an irredundant-path-only filter.
//  - CryptoMiniSat hyperengine.cpp: REJECTED as a model — its reduction is
//    fused into search propagation and keys off trail ancestry (varData
//    reason depth), which would leak search state into a rule; kissat's and
//    CaDiCaL's dedicated quiescent-root pass is what fits §5.3.
//  - Glucose: no transitive reduction at all (simp/ is MiniSat subsumption).
//
// Deviations recorded:
// (1) REACHABILITY IS A PURE MARKING BFS over the binary watches (the V3a
//     read-only walk idiom, substitute.cpp:42-49), NOT kissat's assign-and-
//     backtrack probe. It never touches the trail, values, or levels, so it
//     is side-effect-free and obviously sound under any N1 interleaving; we
//     have no need of the probe's by-product (kissat reuses the assignment
//     for hyper-binary resolution, which we do not do — deviation 4).
// (2) THE BFS EXCLUDES THE CANDIDATE CLAUSE BY PAIR KEY, BOTH DIRECTIONS —
//     kissat excludes only the one watch SLOT it is scanning (transitive.c:126
//     `p == q`). Slot exclusion alone would let a path run ¬src → … → ¬dst →
//     src → … → dst, i.e. justify the candidate BY ITSELF through its own
//     contrapositive edge. kissat is saved from this by its failed-literal
//     check firing first (reaching src while ¬src is assigned is a
//     contradiction), so its exclusion is sound but only *indirectly*. Ours
//     is direct: excluding the pair makes "no path uses the candidate clause"
//     a property of the walk itself rather than a corollary of another check.
// (3) NO EARLY EXIT when dst is reached (kissat transitive.c:130 breaks). We
//     run each walk to exhaustion so the failed-literal detection (deviation
//     5) sees the whole reachable set. That costs a full walk per candidate,
//     and the cost is PRICED (the walk charges cache-line traffic to
//     stats.transitive_ticks — a large share of cycles on walk-heavy
//     instances). The exhaustive reduction is load-bearing for trajectory, so
//     porting the early exit is measured NEGATIVE; see meter_transitive in
//     default_policy.cpp for the budgeting story. Dosing this rule per
//     instance is phase-6 exhibit material, not a static constant.
// (4) NO hyper-binary resolution and NO cheaper-direction search
//     (cadical transred.cpp:116-120, `min(|watches(¬src)|,|watches(dst)|)`):
//     both are pure cost optimizations; revisit if profiles show the walk
//     dominating.
// (5) FAILED LITERALS ARE NOT THIS RULE'S JOB (the delta stays delete-only).
//     If a
//     walk reaches some q together with ¬q, then ¬src implies a
//     contradiction, so src is a forced unit. kissat asserts it there and
//     then (transitive.c) and CaDiCaL builds an LRAT chain for it
//     (transred.cpp:232) — we instead ABORT that source (deleting nothing)
//     and leave the unit to Probe, which owns add-RUP units and their
//     reason-walk chains. Keeping the delta chain-free is what makes the N2
//     story here a one-liner.
// (6) NO irredundant-path-only filter (cadical transred.cpp:128,159; CMS
//     hyperengine.cpp:213). That filter exists to stop an irredundant clause
//     being deleted on the strength of a path that Reduce may later delete.
//     This kernel HAS no redundant binaries — kernel_add_binary_clause takes
//     no `redundant` param and the watch word has no spare bit — and Reduce
//     only touches arena clauses, so every path edge here is permanent unless
//     V4 itself deletes it (which the tombstone forbids). The filter would be
//     dead code; it becomes MANDATORY the day redundant binaries exist (the
//     same trigger recorded in subsume_forward.hpp).
// (7) NO steps/effort budget in the rule: WHEN and HOW MUCH are policy
//     territory . kissat's `transitive_ticks` and CaDiCaL's
//     propagation-scaled limit are slot/policy budgets in our architecture; a
//     policy wanting bounded work applies per-source instances. The rule DOES
//     charge its walk traffic to stats.transitive_ticks (bookkeeping, not a
//     budget — the same split as propagate charging stats.ticks), because a
//     work-priced meter is the only way a policy budget can be honest.
// (8) DUPLICATE PAIRS ARE NEVER DELETED. binary_ids holds ONE id per literal
//     PAIR, so a pair with two live copies cannot be deleted by pair name: the
//     `del` would retire the id the surviving copy is still cited by, and
//     every later LRAT chain through it would name a deleted clause. The
//     kernel primitive refuses such a pair outright (kernel.cpp); V4 also
//     drops it at candidate collection, so no walk is spent on an edge that
//     cannot be deleted. Duplicates are reachable — nothing dedups binaries
//     (a CNF may simply repeat a clause). This is also why kissat's
//     duplicate-binary sweep stays dropped (subsume_forward.hpp deviation 2).
//
// The round-local TOMBSTONE is the soundness crux, not an optimization.
// Within one source's scan an edge decided redundant is marked dead
// IMMEDIATELY, and every later walk from that source skips it, so no edge is
// ever justified by an edge that is itself being deleted. Without it, the
// mutual-dependency case (a ∨ b), (a ∨ c) with b ≡ c would delete BOTH — each
// justified by the other's path — leaving `a` unconstrained and enlarging the
// model set. Deletions are applied only AFTER the source's scan finishes
// (kernel_delete_binary mutates the very lists the walk reads); later sources
// then walk the physically updated graph, so the tombstone is per-source.
//
// Instance space (plan §5.1 v1): payload = SOURCE literal as a sign-extended
// DIMACS int (like Decide / V1 Probe / V2 Backbone). One instance = one
// source literal = all of that source's candidate edges, because a walk is
// rooted at ¬src and every candidate from that source re-walks the same
// graph; a per-EDGE instance would only re-derive the same rooting. The
// tombstone's per-source scope makes each instance sound standalone.
// INSTANCE_CANONICAL = the full round (every source literal).
//
// Guard: quiescent root (C1/C2/C3/V1/V2/V3's argument) — the BIG must already
// reflect all level-0 units before its edges mean anything, and quiescence is
// what makes kernel_delete_binary's in-place watch compaction safe (no
// propagation scan of those lists in flight).
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

bool transitive_reduce_guard(const Kernel &);
// canonical source order: active literals whose ¬src has >= 2 binary
// out-edges (with one out-edge there is no alternate path to find), ascending
// by literal code
void transitive_reduce_sources(const Kernel &, std::vector<Lit> &out);
// one source: walk every canonical candidate (src ∨ dst), delete the
// transitively redundant ones. Ok = something deleted, NoOp = nothing
ApplyResult transitive_reduce_try(Kernel &, Lit src);
ApplyResult transitive_reduce_apply(Kernel &); // canonical: the full round

} // namespace tactix
