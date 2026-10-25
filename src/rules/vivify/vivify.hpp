// C3 Vivify (docs/dev/calculus.md §5 C3): clause distillation by unit propagation.
// Assume the negations of a candidate's literals one level at a time; an
// implied literal, an implied negation or a probe conflict proves a strictly
// stronger clause, which replaces the candidate. Proof delta: add-RUP (the
// distilled clause, LRAT chain from a reason walk of the probe trail) + delete
// (the original); a root-satisfied candidate is deleted. Scope: irredundant
// plus the redundant tiers tier1/tier2/tier3, selected by
// Kernel::vivify_scope, each with its own budget share (kissat vivify.c,
// CaDiCaL vivify.cpp, MIT). Instance payload = candidate arena Ref; canonical
// = the full round; guard = quiescent root. Trail reuse and its invariants:
// docs/design/2026-08-05-vivify-trail-reuse.md; open deviations:
// docs/dev/implementation-notes.md row D-VIVIFY-SCHED.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Static fallback only: the live bound is `Kernel::tier2`, recomputed from the
// glue distribution at every reduce. This is that field's initial value, and
// also kissat's `TIER2` constant.
RuleEligibility vivify_guard(const Kernel &);
// Complete per-clause instance surface in the canonical candidate order.
void vivify_candidates(const Kernel &, std::vector<Ref> &out);
// Exact in-place watch selection used after a large shrink.  Kept visible so
// the conflict-backtrack/unassigned-tail behavior can be unit tested.
void vivify_pose_replacement_watches(Kernel &, std::vector<Lit> &);
// Match Kissat's post-terminal watched-clause invariant: a false watched pair
// can force a further retained-prefix backtrack after watch selection.
void vivify_reestablish_watch_invariant(Kernel &, const Lit *, uint32_t);
// Exact prefix trim that precedes the watch selection for a large rewrite.
void vivify_trim_replacement_prefix(Kernel &, Lit implied,
                                    uint32_t replacement_size);

ApplyResult vivify_try(Kernel &, Ref candidate); // one candidate
ApplyResult vivify_apply(Kernel &);              // canonical: full round

} // namespace tactix
