// V2 Backbone (plan §5.2): the binary-implication backbone — failed-literal
// probing restricted to binary-only propagation.  Unlike V1 Probe, successful
// assumptions remain on one round trail.  A later binary conflict is resolved
// backwards to its first repeated implication predecessor; that false endpoint
// is a RUP root unit, with the reversed binary-reason path as its LRAT chain.
// Units are replayed at root after the round and the schedule repeats to a
// bounded fixpoint.  This is the chained binary-UIP algorithm in Kissat
// `backbone.c` and CaDiCaL `backbone.cpp`, not independent failed-literal scans.
// Precondition, effect, soundness: `docs/calculus.md` §V2; references and the
// lineage split: `docs/design/independent-lineage-survey.md`.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root; `!conflict_valid` is abstract, as in V1.
RuleEligibility backbone_guard(const Kernel &);

// Instance space (plan §5.1): payload = probe literal, sign-extended DIMACS
// int. Canonical order: BIG-head literals, var-index ascending, positive phase
// before negative — the same set as V1 Probe.
// INSTANCE_CANONICAL = the full round.
void backbone_candidates(const Kernel &, std::vector<Lit> &out);

ApplyResult backbone_try(Kernel &, Lit p); // one binary probe

// Canonical: chained binary-UIP rounds, internally respecting `work_budget` in
// private Backbone ticks.  The literal instance surface remains the independent
// single-probe operation required by N1; DefaultPolicy admits this canonical
// batch so its trail persists across candidates.
ApplyResult backbone_apply(Kernel &);

} // namespace tactix
