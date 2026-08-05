// X4 Instantiate (plan §5.2, rev 72 — Track R item R3): variable
// instantiation, the removal of a RARE literal from a clause. For a clause
// C = D ∨ l, assume l TRUE and every literal of D FALSE and propagate: a
// conflict proves F ⊨ ¬l ∨ D, which resolves against C = l ∨ D to give D.
// So C may be replaced by the strictly shorter D.
//
// Proof delta: add-RUP (D) + delete (C) — C2 Strengthen's pair, no witness
// and no new primitive. D is genuinely RUP against F ALONE, which is the
// point worth stating because the probe assumed something D's negation does
// not: assuming ¬D leaves C = D ∨ l unit, so C ITSELF propagates l, and the
// probe's state is reached. The LRAT chain therefore has to cite C at the
// position where l entered the trail (cadical does the same, guarded on
// whether the conflict analysis reached l at all) — see deviation (5).
//
// Value: the literal removed is one whose variable occurs (in that polarity)
// at most INST_OCCLIM times, so removing it tends to make the variable pure
// or nearly pure — i.e. this rule feeds E1 BVE rather than paying by itself.
//
// References (plan §5 "evidence, not templates"; transliterated in essence,
// no code copied):
//  - cadical instantiate.cpp (`instantiate_candidate`): the assume-and-
//    propagate test, the candidate filters (occurrence limit, minimum clause
//    size, ≥ 3 unassigned to avoid learning units), and the LRAT chain
//    construction. cadical triggers it at the END of an elimination round,
//    over the elim occurrence lists.
//  - kissat 4.0.4 has NO instantiation rule at all, so CaDiCaL is the only
//    reference here (as with X2).
//  - CONTEXT WORTH KEEPING (it calibrates what to expect): cadical ships
//    `instantiate` **default OFF** (options.hpp: 0). The idea survives in its
//    default configuration only as `vivifyinst` — instantiating the LAST
//    literal during vivification. So the reference itself does not schedule
//    the standalone rule. That prior held for us too when the rule was measured
//    ALONE (Tier B 13.08 vs 8.05 shipped) — but measuring it alone turned out to
//    be the wrong experiment: with X1 BCE and X2 HyperTernaryRes on beside it the
//    trio costs 9.22, far less than any member alone (X1/X2/X4 alone are
//    21.79/53.26/13.08 on that same Tier B(14) list, though X2's arm there is
//    ~93 % one instance), because the families repair each other — so a member
//    of a family must be judged WITH its family, never on its own arm.
//    **SCHEDULED since 2026-07-29** — every implemented proof
//    rule is ON (user directive); `TACTIX_SLOT_DISABLE=Instantiate` is the
//    ablation arm.
//
// Deviations recorded:
// (1) SCOPE = IRREDUNDANT CLAUSES, matching cadical (its occurrence lists are
//     irredundant-only). Strengthening a redundant clause would be equally
//     sound; distilling learned tiers is C3 Vivify's job in this architecture
//     and duplicating it here would just double the probe traffic.
// (2) ONE REWRITE PER CANDIDATE CLAUSE PER ROUND (C3 Vivify deviation 2
//     verbatim): the literals of a clause are tried in clause order and the
//     first success rewrites and returns. cadical keeps going within the
//     clause; our rounds repeat through the slot schedule instead. This also
//     keeps the arena stable for the duration of one probe.
// (3) NO `instantiated` CLAUSE FLAG (cadical's `instantiateonce` marks a
//     clause tried and never retries it). That is a scheduling memo, not a
//     precondition — under N1 the same clause must stay applicable at any
//     time, and a per-clause "already tried" bit hidden inside the rule would
//     shrink RandomPolicy's and the phase-6 learner's action space (the
//     §5.3 instance-surface rule; the same argument that moved BVE's retry
//     filter out to the policy). Cost: repeated rounds re-probe failures.
// (4) NO STEPS BUDGET IN THE RULE, the §5.2 rev-17 split: WHEN and HOW MUCH is
//     policy territory. The canonical round is budget_aware — it reads
//     `Kernel::work_budget` in propagation ticks and stops mid-round, the
//     arrangement Track R's round 2 showed these families need.
// (5) THE LRAT CHAIN CITES THE CANDIDATE CLAUSE ITSELF at l's trail position,
//     and only when the conflict's reason walk actually reached l. This is
//     cadical's guard (`if (flags (lit).seen) lrat_chain.push_back (c->id)`)
//     with our sorted-by-trail-position chain machinery doing the placement:
//     hints replay in trail order, and l entered the trail before anything
//     propagated from it.
// (6) The root-falsified literals of C are KEPT in D (cadical's
//     `strengthen_clause (c, lit)` removes only the candidate literal). C3
//     Vivify drops them; doing it here would mix two strengthenings into one
//     delta for no new reachable state — the next Vivify round removes them.
//
// Instance space (plan §5.1 v1): payload = candidate arena Ref (C3 Vivify's
// convention). One instance = one clause = all of its eligible literals, the
// X2 argument: every removal a per-(clause,literal) surface could reach is
// reached by applying that clause. INSTANCE_CANONICAL = the full round.
//
// Guard: quiescent root (D-ROOT, the C1/C2/C3/V1/V2/V3/V4/X1/X2 argument) —
// occurrence counts and the "unassigned" filters only mean anything once
// every level-0 unit is propagated into the clause set.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

// cadical options.hpp defaults: instantiateclslim 3, instantiateocclim 1
constexpr uint32_t INST_CLSLIM = 3;  // minimum candidate clause size
constexpr uint32_t INST_OCCLIM = 1;  // maximum occurrences of the literal

bool instantiate_guard(const Kernel &);
// canonical candidate order: arena order (a total order, allocation-free)
void instantiate_candidates(const Kernel &, std::vector<Ref> &out);
ApplyResult instantiate_try(Kernel &, Ref candidate); // one clause
ApplyResult instantiate_apply(Kernel &);              // canonical: full round

} // namespace tactix
