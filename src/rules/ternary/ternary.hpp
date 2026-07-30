// X2 HyperTernaryRes (plan §5.2, rev 72): hyper ternary resolution. Two
// TERNARY clauses (p ∨ a ∨ b) and (¬p ∨ c ∨ d) resolve on p to (a ∨ b ∨ c ∨ d);
// the resolvent is kept only when it COLLAPSES to size ≤ 3, i.e. when the
// antecedents share literals — (p ∨ a ∨ b),(¬p ∨ a ∨ c) ⊢ (a ∨ b ∨ c) and
// (p ∨ a ∨ b),(¬p ∨ a ∨ b) ⊢ (a ∨ b). Adding it is model-preserving in the
// strong direction that needs no witness: a resolvent is ENTAILED by its two
// parents, so F ∧ r ≡ F and no reconstruction entry is required (the same
// argument E1 BVE rests on, plan rev 45).
//
// Proof delta: add-RUP, chain = [id(C), id(D)] — the resolvent is RUP from its
// two parents (assume its negation: every literal of C except p is falsified,
// so C forces p; every literal of D except ¬p is falsified, so D forces ¬p;
// conflict). This is the SAME two-parent chain BVE emits for its resolvents,
// so X2 needs no new proof primitive and no witness — the cheapest genuine
// rule in the reference gap, which is why Track R starts here.
//
// References (plan §5 "evidence, not templates"; transliterated in essence,
// no code copied):
//  - cadical ternary.cpp (`hyper_ternary_resolve`): the resolvent construction
//    and its three rejections — size > 3, tautological, already present. Our
//    construction is the same walk with the same rejections.
//  - kissat 4.0.4: has NO hyper ternary resolution (dropped before 4.0), so
//    there is no kissat arrangement to match here; CaDiCaL is the only
//    reference, and it schedules ternary INSIDE its preprocessing rounds under
//    a steps budget.
//  - Both references ADD the resolvents as REDUNDANT clauses (cadical marks
//    `r->hyper = true` so Reduce may drop them again). We do the same: a
//    ternary resolvent is entailed, so keeping it forever would only grow the
//    formula — see deviation (3).
//
// Deviations recorded:
// (1) NO SUBSUMPTION OF THE ANTECEDENTS. cadical (ternary.cpp:215) notices when
//     a resolvent subsumes both parents and deletes them. That is C1/C2's job
//     in this architecture — a rule owns ONE proof delta, and mixing `delete`
//     into X2 would duplicate SubsumeForward's occurrence bookkeeping. The
//     resolvents are added irredundant-adjacent (deviation 3) and the next
//     subsumption pass removes what they subsume.
// (2) NO STEPS BUDGET IN THE RULE (the §5.2 rev-17 split, identical to V4
//     deviation 7): WHEN and HOW MUCH is policy territory. The rule charges
//     its occurrence traffic to `stats.ternary_ticks` so a policy budget can
//     be honest, and offers per-pivot instances so a policy can dose it.
// (3) RESOLVENTS ARE ADDED REDUNDANT (glue = size), so Reduce can retire them.
//     Hyper ternary resolution is a CLAUSE-ADDING rule on an already-entailed
//     consequence; adding irredundant would make the formula monotonically
//     grow across rounds with no reclamation path. Binary resolvents are the
//     exception — this kernel has no redundant binaries (V4 deviation 6), so a
//     binary resolvent is permanent, which is also what makes it valuable.
// (4) PAIRS ARE ENUMERATED PER PIVOT OVER THE DENSE INDEX, not over a
//     per-literal "ternary" flag cache (cadical maintains `ternary` var flags
//     and a candidate queue). The dense occurrence index (`Kernel::dense`,
//     kissat dense.c analogue) already exists for E1/C1 and is attached for
//     the whole simplification phase, so X2 reuses it rather than adding a
//     second index. Cost: we rescan a pivot's occurrences to find the ternary
//     ones instead of having them pre-bucketed.
// (5) DUPLICATE DETECTION IS EXACT FOR BINARIES, BOUNDED FOR TERNARIES.
//     `binary_exists` answers the size-2 case outright. For size 3 we scan the
//     occurrence list of the resolvent's rarest literal — the same shape as
//     cadical's `ternary_find_ternary_clause`, and the reason the rule stays
//     quadratic-with-a-small-constant rather than needing a hash index.
//
// Instance space (plan §5.1 v1): payload = PIVOT literal as a sign-extended
// DIMACS int (like Decide / V1 Probe / V4). One instance = one pivot = all
// resolutions of its positive ternaries against its negative ones. A per-PAIR
// instance would multiply the surface by the occurrence product for no new
// reachable state — every pair of a pivot is reached by applying that pivot.
// INSTANCE_CANONICAL = the full round (every pivot with ternaries on both
// sides).
//
// Guard: quiescent root (the C1/C2/C3/V1/V2/V3/V4 argument — D-ROOT). The
// occurrence index and the "already present" checks only mean anything once
// every level-0 unit is propagated into the clause set.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

bool ternary_guard(const Kernel &);
// canonical pivot order: variables carrying at least one ternary occurrence in
// EACH polarity (no pair exists otherwise), ascending by literal code
// `total_clauses` (optional out) is the live clause count from the same arena
// pass, the base for the max-add governor (Kernel::ternary_max_add_percent).
void ternary_pivots(const Kernel &, std::vector<Lit> &out,
                    uint64_t *total_clauses = nullptr);
// one pivot: add every collapsing resolvent. Ok = something added, NoOp = none
ApplyResult ternary_try(Kernel &, Lit pivot);
ApplyResult ternary_apply(Kernel &); // canonical: the full round

} // namespace tactix
