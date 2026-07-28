// X1 BCE (plan §5.2, rev 72): blocked clause elimination. A clause `C` is
// BLOCKED on a literal `l ∈ C` when every resolvent of `C` with a clause
// containing `¬l`, resolved on `l`, is TAUTOLOGICAL. Such a `C` may be deleted:
// the formula is not equivalent afterwards (a model of `F \ {C}` need not
// satisfy `C`), but it is EQUISATISFIABLE, and any model is repaired by setting
// `l` true — which is exactly a reconstruction witness. Kissat 4 dropped BCE;
// CaDiCaL keeps it (`block.cpp`), and the user directive (rev 72) is to carry
// every reference family, so it lands here.
//
// Proof delta: `delete` + `push-witness`. Nothing is ADDED, so there is no
// chain to build and no format-specific hazard — a deletion line is accepted
// unconditionally by every checker (it only shrinks the clause set, making
// later RUP checks harder, never easier). The blockedness argument is therefore
// a SOLVER-soundness obligation discharged by the guard below, exactly as V4
// TransitiveReduce's redundancy argument is — not a proof obligation. The
// difference from V4 is that V4's deletion is equivalence-preserving (no
// witness) while BCE's only preserves satisfiability (witness required, and
// `ρ` must record it or SAT answers become wrong).
//
// References (plan §5 "evidence, not templates"; transliterated in essence):
//  - cadical block.cpp: candidate literals from occurrence lists, per-literal
//    marking of `C`'s literals, then the tautology test against every
//    `¬l`-occurrence. Its `block_covered` / covered-literal extension is X3
//    CCE's business, not X1's.
//  - kissat: no BCE in 4.0.4 (removed) — CaDiCaL is the only live reference.
//  - Eén–Biere's original blocked-clause result is the soundness source; the
//    witness form is the same one E1 BVE already pushes for its removed
//    clauses (`kernel_push_witness`), so `ρ` needs no new machinery.
//
// Deviations recorded:
// (1) IRREDUNDANT CLAUSES ONLY. A redundant (learned) clause may be deleted
//     freely by Reduce with no witness, so blocking it buys nothing and would
//     put a needless segment on `ρ`. cadical likewise blocks only irredundant.
// (2) BINARIES ARE CANDIDATES, LARGE CLAUSES ARE CANDIDATES, UNITS ARE NOT.
//     A root unit is already propagated into the formula; "deleting" it would
//     drop a level-0 assignment the rest of the kernel assumes.
// (3) NO OCCURRENCE-LIMIT / EFFORT BUDGET IN THE RULE (the §5.2 rev-17 split,
//     as in V4 deviation 7 and X2 deviation 2): the rule charges its scanning
//     to `stats.bce_ticks` and offers per-literal instances; WHEN and HOW MUCH
//     is policy territory.
// (4) NO PURE-LITERAL FAST PATH. A literal `l` with NO `¬l`-occurrence blocks
//     every clause containing it vacuously (the pure-literal rule), and both
//     references special-case it. Ours falls out of the same loop — the "for
//     every `¬l`-clause" quantifier is vacuously true — so it needs no separate
//     code, only the note that BCE subsumes pure-literal elimination here.
//
// Instance space (plan §5.1 v1): payload = the BLOCKING literal as a
// sign-extended DIMACS int (like Decide / V1 / V4 / X2). One instance = one
// literal = delete every irredundant clause blocked on it. Per-CLAUSE
// instances would need clause identity in the payload, which the v1 encoding
// does not carry (and `Ref`s move under GC).
// INSTANCE_CANONICAL = the full round (every literal).
//
// Guard: quiescent root (D-ROOT, the shared inprocessing argument) — the
// tautology test reads the live occurrence structure, which only means what it
// says once every level-0 unit is propagated.
#pragma once

#include "rules/registry.hpp"

#include <vector>

namespace tactix {

bool bce_guard(const Kernel &);
// canonical literal order: literals with at least one irredundant occurrence,
// ascending by literal code
void bce_literals(const Kernel &, std::vector<Lit> &out);
// one literal: delete every irredundant clause blocked on it, pushing a
// witness per deletion. Ok = something deleted, NoOp = nothing
ApplyResult bce_try(Kernel &, Lit blocking);
ApplyResult bce_apply(Kernel &); // canonical: the full round

} // namespace tactix
