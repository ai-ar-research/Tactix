// C3 Vivify (docs/dev/calculus.md §5 C3): distillation by unit propagation. An
// implied literal, implied negation or conflict proves a stronger clause that
// replaces the candidate (add-RUP + delete); root-satisfied candidates are
// deleted. Scope via Kernel::vivify_scope over irredundant and tier1/2/3, each
// budgeted (kissat vivify.c, CaDiCaL vivify.cpp). Payload = candidate Ref;
// canonical = the full round. Trail reuse:
// docs/design/2026-08-05-vivify-trail-reuse.md; deviations: D-VIVIFY-SCHED.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

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
