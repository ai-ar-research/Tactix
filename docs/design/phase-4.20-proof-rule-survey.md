# Phase 4.20 proof-rule survey

**Status:** design report; no candidate is implemented. **Scope:** Tactix plus
the frozen SC24–26 winners in `winners/`. Existing rule semantics remain solely
in `docs/calculus.md`.

## Result

Generic definitional extension passed its proof-format feasibility gate.
Symmetry breaking is strategically important but blocked on a native
DRAT/LRAT translation. Richer Sweep harvesting is an optimization/generalization
of existing RUP addition, not yet evidence for another public rule. LymphoSAT's
family provers are deliberately not imported as benchmark-specific rules.

| Candidate | Calculus novelty | Verdict |
|---|---|---|
| Generic definitional extension | new fresh-variable rule beyond E2's grid | certificate-feasible; API design next |
| One-way local implication | already RUP clause addition used by V5/X6 | shared primitive + strategy, not new rule |
| Symmetry-breaking addition | equisatisfiable, generally not entailed | blocked on DRAT/LRAT translation |
| Family-specific direct proof | domain reductions / PB proof systems | reject from generic calculus |

## P1. Generic definitional extension

Start with one concrete atom rather than an unrestricted extended-resolution
schema. For active literals `l₁…lₖ` and fresh `x`, define
`x ↔ ∧ᵢ lᵢ`.

- **Abstract precondition:** `x ∉ vars(F)`; input literals are well-formed.
- **Effect:** add base `B=(x ∨ ¬l₁ ∨ … ∨ ¬lₖ)` and implications
  `Iᵢ=(¬x ∨ lᵢ)`. Nothing is deleted; `τ` and `ρ` are unchanged.
- **DRAT delta:** add `B` RAT on fresh `x`, then each `Iᵢ` RAT on `¬x`.
  Resolving `Iᵢ` with `B` is tautological because it contains `lᵢ,¬lᵢ`.
- **LRAT delta:** the same `add_rat` ordering, with a vacuous first line and one
  partner group naming `B` for each implication; the tautological resolvent
  needs no positive RUP hint.
- **Model contract:** projection drops `x`; no witness is needed. Every model of
  the extension restricted to old variables models the unchanged `F`.
- **N1/API issue:** a legal definition has variable-length content and does not
  fit the v1 `uint64_t` payload. Do not hide selection inside a canonical apply.
  Either add a reviewed compound-instance API or expose only definitions
  discovered by a complete, stable-ID gate catalog. The latter must define
  completeness precisely.
- **Ownership:** the rule validates/emits one supplied definition. Strategy
  chooses which definition, when, and whether later rules can exploit it.

**Certificate gate (green 2026-08-13):** a four-clause, non-unit UNSAT core was
kept independent of free `l₁,x`, so neither definition clause was RUP.
`drat-trim -f -v` classified both as RAT and verified; native LRAT lines
`B: ... 0 0` and `I₁: ... 0 -B 0` verified with both lrat-check and cake_lpr.
Remaining gates are malformed/non-fresh rejection, random legal schedules,
model projection, and public enumeration, after the API decision.

## P2. One-way local implications from Sweep

For literals `a,b`, if `F ∧ a ∧ ¬b ⊢₁ ⊥`, then
`F ⊨ (¬a ∨ b)`.

- **Effect/proof:** add `(¬a ∨ b)` by `add_rup` using the main-formula replay
  chain or a translated embedded-solver chain. No deletion or witness.
- **Overlap:** V5 already emits both directions for an equivalence; X6 emits a
  binary consequence from a replay. V1/V2 cover the unit special case.
- **Disposition:** factor out a shared `RupConsequence` verifier/emitter, then
  let Sweep strategy rank candidate units, one-way implications, and pairs.
  Adding another public rule before removing this duplication would create two
  names for the same semantic transition.

The strategy experiment must measure new binary consequences, downstream
propagation/conflicts, proof bytes/check time, and lost Sweep equivalences under
the same budget. Kill on no solve movement or excessive proof/check cost.

## P3. Symmetry-breaking addition

Satsuma+Kissat runs `satsuma fix --bsr --proof-file ...` before Kissat and checks
the combined certificate with dsr-trim (`winners/satsuma-iter-kissat-sc2026/.../run_base.sh`).
Its added clauses select representatives of formula symmetries: satisfiability
is preserved, but an individual clause need not be entailed or RUP.

- **Desired abstract rule:** given a verified automorphism `π` of `F` and a
  precisely specified symmetry-breaking constraint `S(π)`, add `S(π)` while
  preserving satisfiability. No SAT reconstruction is needed because any model
  of `F∧S` already models `F`.
- **Certificate blocker:** the winner emits DSR, while R1 requires DRAT and
  native LRAT. A DSR-success claim is not a DRAT/LRAT proof delta. The report
  does not assume every BSR step has a compact RAT or RUP translation.
- **Required design evidence:** formal BSR schema, automorphism witness,
  per-step DRAT construction or checked DSR→DRAT/LRAT translation, proof-size
  bound, and independent checking on the Satsuma fixtures.

Until that evidence exists, symmetry detection may be trace-only. It must not
mutate `F`, narrow the instance surface, or enter Stage D as an unchecked
preprocessor.

## P4. LymphoSAT direct provers

LymphoSAT routes to 126 specialized programs. Several reconstruct SAT models
from recovered generators; several UNSAT paths emit VeriPB `rup`/`red` proofs
or large domain-specific proof trees. These are valuable strategy evidence but
not generic Tactix proof rules: their preconditions encode a problem family,
their certificates use a different calculus, and some explicitly support SAT
only. Stage E may learn content-derived scheduling features, but benchmark
family names and direct arithmetic/graph solvers do not enter the rule catalog.

## Ordered next decisions

1. Design P1's N1 compound-instance surface before implementation.
2. Refactor/measure P2 only when a Stage-A divergence points at missed local
   consequences.
3. Obtain a formal DSR/BSR translation story before any P3 formula mutation.
4. Keep LymphoSAT as SAT strategy evidence, consistent with its Stage-E scope.
