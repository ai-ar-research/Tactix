// V4 TransitiveReduce (plan §5.2): delete a binary (src ∨ dst) — the binary
// implication graph edge ¬src → dst — when dst is still reachable from ¬src
// along other binaries. Proof delta: `del` only, chain-free, so the redundancy
// argument is a solver-soundness obligation discharged by the walk rather than
// a proof obligation. Precondition, effect, and soundness:
// `docs/calculus.md` §V4; binary lifecycle: `docs/implementation-notes.md` §14.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root — the graph must reflect every level-0 unit before its
// edges mean anything, and quiescence is what makes kernel_delete_binary's
// in-place watch compaction safe.
bool transitive_reduce_guard(const Kernel &);

// Instance space (plan §5.1): payload = a source literal, sign-extended DIMACS
// int; one instance = one source = all its candidate edges, since a walk is
// rooted at ¬src. Canonical order: active literals whose ¬src has >= 2 binary
// out-edges, ascending by literal code. INSTANCE_CANONICAL = the full round.
void transitive_reduce_sources(const Kernel &, std::vector<Lit> &out);

// One source: walk every candidate (src ∨ dst), delete the transitively
// redundant ones. Two invariants beyond the abstract precondition. The
// round-local tombstone is a soundness requirement, not an optimization: an
// edge found redundant is marked dead at once, so no edge is justified by one
// being deleted in the same round — without it (a ∨ b),(a ∨ c) with b ≡ c
// delete each other. Its scope is one source, because kernel_delete_binary
// compacts the lists the walk reads. And the candidate is excluded by literal
// pair in both directions, so no path runs through its own contrapositive edge
// (kissat transitive.c:126 excludes only the watch slot, relying on its
// failed-literal check firing first).
ApplyResult transitive_reduce_try(Kernel &, Lit src);

// Canonical: the full round. Failed literals are left to V1 Probe, keeping the
// delta delete-only, and a duplicated pair is never deleted since `binary_ids`
// holds one id per literal pair. No effort budget in the rule, though the walk
// charges cache-line traffic to stats.transitive_ticks so a policy budget can
// be honest, and it runs to exhaustion rather than stopping at dst — the early
// exit is measured negative (`Kernel::transitive_early_exit`).
ApplyResult transitive_reduce_apply(Kernel &);

} // namespace tactix
