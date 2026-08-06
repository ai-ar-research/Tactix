// X3 CCE (plan §5.2): covered clause elimination, in the ASYMMETRIC variant
// (ACCE) both references' literature calls "CCE". It generalises X1 BCE: rather
// than test the clause `C` for blockedness as written, GROW it — by asymmetric
// literal addition (ALA) and covered literal addition (CLA) — until the grown
// clause `C''` is blocked, then delete the ORIGINAL `C`.
//
//   ALA  if some `D ∈ F∖{C}` has `D∖{d} ⊆ C''`, add `¬d` to `C''`.
//        Equivalence-preserving: resolving `C'' ∪ {¬d}` with `D` on `d` gives
//        `C''` back. Exactly unit propagation of `¬C''` over `F∖{C}`.
//   CLA  if literal `o` occurs in EVERY clause of `F` containing `¬u`, for some
//        `u ∈ C''`, add `o` to `C''`. Satisfiability-preserving ONLY, so each
//        CLA step puts a witness segment on `ρ`.
//
// The three exits:
//   BLOCKED  every clause containing `¬u` resolves tautologically against `C''`
//            — delete `C`, witness = `u` over the recorded clause.
//   ATA      propagation falsified some `D ∈ F∖{C}` entirely, i.e. `D ⊆ C''`.
//            With no CLA step in front of it this means `C` is RUP against
//            `F∖{C}`, so it is deleted with NO witness at all.
//   nothing  the extension stalls; `C` stays.
//
// ▶ THE WITNESS AND WHY IT OMITS THE ALA LITERALS. Write `D_i` for the RECORDED
// clause after `i` CLA steps: `D_0 = C` and `D_i = D_{i-1} ∪ I_i`, where `I_i`
// is the i-th intersection. The ALA literals are NOT in any `D_i`. The segments
// pushed, in order, are `(u_1, D_0), (u_2, D_1), …` and finally `(w, D_last)`
// for the blocked exit. `kernel_extend_model` replays `ρ` in REVERSE, flipping a
// segment's witness whenever its clause is falsified, which is what makes this
// order the right one.
//
// ALA-CLOSURE LEMMA (the reason the smaller recorded clause is the correct
// trigger). Let `M ⊨ F∖{C}`. If `M` falsifies every literal of `D_i`, it
// falsifies every literal of the full extension `C''_i = D_i ∪ (ALA literals
// added by stage i)`. Proof: induct on the order the ALA literals were added.
// `¬d` was added because some `D ∈ F∖{C}` had `D∖{d}` inside the extension at
// that moment — CLA literals (all in `D_i`, false by hypothesis) and earlier
// ALA literals (false by the induction hypothesis). `M ⊨ D`, so `d` is true and
// the added literal `¬d` is false. ∎
//
// MODEL RECONSTRUCTION, step by step, using the lemma:
//  - blocked exit. `M ⊨ F∖{C}`, `M ⊭ D_last`. By the lemma `M` falsifies
//    `C''`. Blockedness says every `E ∋ ¬w` holds a literal `t ≠ ¬w` with
//    `¬t ∈ C''`; `¬t` is false so `t` is TRUE, so `E` survives the flip of `w`.
//    And `w ∈ D_last`, so the flip satisfies `D_last`.
//  - ATA exit after CLA steps. No final segment is needed: `D ⊆ C''` and
//    `M ⊨ D`, so `M` cannot falsify `C''`, so by the lemma it cannot falsify
//    `D_last` either.
//  - CLA step `i`, replayed after step `i+1`. `D_i` is satisfied and `D_{i-1}`
//    is falsified, so some `o ∈ I_i` is TRUE. Flipping `u_i` satisfies
//    `D_{i-1}`; every `E ∋ ¬u_i` either was skipped as already-resolving-
//    tautologically (same argument as the blocked exit, via the lemma) or
//    contributed to the intersection, hence contains that true `o`.
//  - the induction bottoms out at `(u_1, D_0 = C)`: flipping `u_1 ∈ C`
//    satisfies the deleted clause itself.
// Every flip only sets a witness literal TRUE, and each `D_j` for `j > i`
// contains `u_i`, so a later-replayed segment is never re-falsified.
//
// Proof delta: `delete` + `push-witness` (the ATA exit: `delete` alone). Nothing
// is ADDED, so there is no chain to build and no format-specific hazard — a
// deletion line only shrinks the clause set, making later RUP checks harder,
// never easier (X1 BCE's argument verbatim). The extension is a SOLVER-soundness
// obligation discharged by the two lemmas above, not a proof obligation.
//
// References (plan §5 "evidence, not templates"; transliterated, not copied):
//  - cadical `cover.cpp` — `Coveror`'s added/covered/intersection stacks, the
//    "restart the covered scan after every addition" control flow, the
//    double-satisfied skip, and the decision to record only the covered
//    literals on the extension stack are all its shape. cadical ships
//    `cover`=0; the landing convention here is ON (PROGRESS Track R).
//  - kissat 4.0.4 has no CCE (and no BCE), so cadical is the only live
//    reference. Heule-Järvisalo-Biere LPAR-10 / JAIR'15 is the source result.
//  - the witness form is X1 BCE's, so `ρ` needs no new machinery: CCE differs
//    only in pushing a SEQUENCE of segments for one deletion.
//
// Deviations recorded (docs/implementation-notes.md §1 carries them with
// removal paths):
// (1) IRREDUNDANT, WHOLLY UNASSIGNED, LARGE clauses are candidates. Redundant:
//     Reduce may drop them for free, so a witness for one is waste (BCE's
//     deviation 1). Wholly unassigned: it skips root-satisfied clauses and
//     guarantees the clause is no root reason (BCE's `candidate_clause`).
//     LARGE: binaries have no arena identity here and X1 already covers the
//     blocked-on-a-binary case.
// (2) THE TWO SIDES QUANTIFY DIFFERENTLY, and this is load-bearing.
//     CLA/blockedness runs over every live non-root-satisfied clause,
//     REDUNDANT ONES INCLUDED — over-quantifying only makes the test stricter,
//     and BCE's fuzz-2204 wrong-SAT came from filtering that side.
//     ALA runs over IRREDUNDANT clauses only: the ALA-closure lemma needs
//     `M ⊨ D` for the justifying clause, and Reduce deletes redundant clauses
//     with no witness, so a redundant `D` is not there to be satisfied when `ρ`
//     replays.
// (3) NO OCCURRENCE OR SIZE LIMIT (the §5.2 split, as in BCE deviation 3): the
//     rule charges `stats.cce_ticks` and offers per-literal instances; WHEN and
//     HOW MUCH is policy territory. The one internal bound is a per-CANDIDATE
//     tick cap, which exists so a single dense clause cannot own a whole dose —
//     cadical bounds only the round.
// (4) REPETITION + RESUMPTION run on the TOUCHED-EPOCH SUBSTRATE (Kernel::
//     cce_touch_mark, read two-sided as BCE's is), not on cadical's per-clause
//     `covered` flag: the round skips candidates nothing moved in and re-arms
//     the tail a budget cut never reached. Coarser than the reference on two
//     axes, both yield-only — the filter reads vars(C) rather than C's whole
//     ALA/CLA closure, and the substrate does not stamp the redundant clause
//     traffic the CLA side quantifies over (see changed_clause in cce.cpp).
//
// Instance space (plan §5.1 v1): payload = a literal, as everywhere else
// (Decide / V1 / V4 / X2 / X1). One instance = one literal = attempt CCE on
// every candidate clause containing it. Per-CLAUSE instances would need clause
// identity in the payload, which the v1 encoding does not carry (and `Ref`s
// move under GC). INSTANCE_CANONICAL = the full round.
//
// Guard: quiescent root (D-ROOT, the shared inprocessing argument) — every test
// here reads the live occurrence structure, which only means what it says once
// each level-0 unit is propagated.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

bool cce_guard(const Kernel &);
// canonical literal order: every root-unassigned literal, ascending by code
void cce_literals(const Kernel &, std::vector<Lit> &out);
// one literal: attempt CCE on every candidate clause containing it, witnessing
// each elimination. Ok = something deleted, NoOp = nothing
ApplyResult cce_try(Kernel &, Lit l);
ApplyResult cce_apply(Kernel &); // canonical: the full round

} // namespace tactix
