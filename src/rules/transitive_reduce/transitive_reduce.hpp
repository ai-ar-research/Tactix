// V4 TransitiveReduce (plan §5.2): delete a binary (src ∨ dst) — the binary
// implication graph edge ¬src → dst — when dst is still reachable from ¬src
// along other binaries. Proof delta: chain-free `del` for redundancy; when the
// same walk reaches both polarities, add the RUP unit {src} using the BFS's
// binary-reason order. Precondition, effect, and soundness:
// `docs/dev/calculus.md` §V4; binary lifecycle: `docs/dev/implementation-notes.md` §14.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root — the graph must reflect every level-0 unit before its
// edges mean anything, and quiescence is what makes kernel_delete_binary's
// in-place watch compaction safe.
RuleEligibility transitive_reduce_guard(const Kernel &);

// Instance space (plan §5.1): payload = a source literal, sign-extended DIMACS
// int; one instance = one source = all its candidate edges, since a walk is
// rooted at ¬src. The instance space contains both unfixed polarities of every
// active variable, including sources with no admissible edge: Kissat charges
// their source-list scan before returning. Each undirected pair is owned by its
// lower live reference-compact endpoint through Kernel's compact import map;
// policy-neutral callers fall back to permanent import order.
// INSTANCE_CANONICAL = the full round.
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

// Canonical: the full round. Failed-source units are committed and propagated
// before the next source, matching pinned Kissat; any earlier redundant
// candidates from that source remain deleted. Duplicate physical copies are
// processed independently; removing a non-final copy retains its proof clause
// and pair id. No effort budget is computed in the rule, though the walk
// charges cache-line traffic to stats.transitive_ticks so a policy budget can
// be honest. As in Kissat, the walk stops as soon as dst is reached; the
// `Kernel::transitive_early_exit` diagnostic arm can restore exhaustive failed-
// literal discovery, but that is a deliberate non-reference experiment.
ApplyResult transitive_reduce_apply(Kernel &);

} // namespace tactix
