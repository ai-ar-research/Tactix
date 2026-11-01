// V1 Probe (plan §5.2): failed-literal probing producing root units. The
// canonical round schedules BIG heads; the public instance surface offers
// every unassigned literal. Assume p at a fresh level and propagate fully; a
// conflict proves p cannot hold, so {¬p} is a RUP unit.
// Proof delta: add-RUP unit only, the LRAT chain being a reason walk of the
// conflict. Derived units are asserted un-propagated at the root and the round
// ends; M1 Propagate runs each unit's cascade, root-unit LRAT emission and any
// refutation, as C2/C3 also leave a derived unit for the policy to propagate.
// Precondition, effect, soundness: `docs/dev/calculus.md` §V1. References and the
// lineage split: `docs/design/independent-lineage-survey.md` (kissat keeps the
// same loop in backbone.c/proprobe.c; cadical probe.cpp confirms the
// single-decision UIP degenerates to ¬p, so no UIP analysis is needed).
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// Guard: quiescent root. `!conflict_valid` is abstract here — failed-literal
// reasoning assumes p atop a consistent trail.
RuleEligibility probe_guard(const Kernel &);

// Instance space (plan §5.1): payload = probe literal, zero-extended DIMACS
// int. Public enumeration is var-index ascending, positive phase first, for
// every unassigned literal. The canonical BIG-head schedule is only policy
// advice: root-fixed literals can make large clauses propagate without a
// physical binary watch. V2 Backbone is a different binary-only regime.
void probe_candidates(const Kernel &, std::vector<Lit> &out);

// Canonical: the full round. Both polarities of a variable are probed
// independently and the variable is skipped once one fails; the both-fail
// refutation is caught when M1 propagates the batch, not by cross-probe
// reasoning, which would be lifting and is V2's job. A round collects every
// failed literal against the SAME clean root, with no re-propagation between
// probes, then asserts the batch and ends — that is what keeps the cascade and
// the root-unit id emission entirely M1's, since an in-round re-propagation
// would leave cascade literals with unit_ids == 0 and break the chains C2's
// flush pre-pass builds. No clause of any kind is added (cadical's probehbr
// path is rule X6 here) and no effort budget lives in the rule.
ApplyResult probe_apply(Kernel &);

} // namespace tactix
