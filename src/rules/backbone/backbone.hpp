// V2 Backbone (plan §5.2): the binary-implication backbone — failed-literal
// probing restricted to binary-only propagation. Assume a head literal p of the
// binary implication graph, propagate through that graph alone
// (`kernel_propagate_binary`); a conflict proves p cannot hold, so {¬p} is a
// RUP root unit. Same shape, proof delta and round structure as V1 Probe, the
// one difference being the propagation engine, so its units are a SUBSET of
// V1's: the value is speed — binary propagation touches no large clause — which
// makes it a distinct, cheaper scheduling arm, not a source of extra units.
// Precondition, effect, soundness: `docs/calculus.md` §V2; references and the
// lineage split: `docs/design/independent-lineage-survey.md`.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root; `!conflict_valid` is abstract, as in V1.
bool backbone_guard(const Kernel &);

// Instance space (plan §5.1): payload = probe literal, sign-extended DIMACS
// int. Canonical order: BIG-head literals, var-index ascending, positive phase
// before negative — the same set as V1 Probe.
// INSTANCE_CANONICAL = the full round.
void backbone_candidates(const Kernel &, std::vector<Lit> &out);

ApplyResult backbone_try(Kernel &, Lit p); // one binary probe

// Canonical: the full round. Every round property V1 Probe records applies
// verbatim (probe.hpp): no rule-side budget, both polarities probed
// independently under a once-forced-per-round mark, and a collect-then-batch
// round against one clean root that leaves the cascade, the root-unit id
// emission and any refutation to M1 Propagate.
ApplyResult backbone_apply(Kernel &);

} // namespace tactix
