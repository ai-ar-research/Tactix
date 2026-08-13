# E2 Factor remainder scope (`factorsize`)

**Status:** landed and frozen. Rule semantics are in `docs/calculus.md` E2;
implementation constraints are in `docs/implementation-notes.md` §5. The
motivating evidence is
`benchmarks/reports/2026-07-30-tierB-timeouts-diagnosis.md` §5a′/R5.

## Durable design

Factor generalizes the binary grid `{f ∨ r | f ∈ F, r ∈ R}` by making each
remainder `r` a sub-clause. For every live irredundant grid clause `C` containing
factor literal `f`, the matcher uses `r = C \ {f}` with
`1 ≤ |r| ≤ factorsize - 1`. Remainders are sorted and interned into dense,
apply-local identifiers; equal content compares cheaply while clause occurrences
remain distinct. Scratch is proportional to material visited and is cleared by
walking touched identifiers.

The proof rule does not change. Add fresh-variable dividers `(x ∨ f)` RAT on
`x`, then quotients `(¬x ∨ r)` RAT on `¬x`; each quotient/divider resolvent is
the original grid clause `{f} ∪ r`. Delete the grid last. The rule needs no
reconstruction witness because `x` is an extension variable projected from the
model.

`factor_size=2` is the binary-only identity setting; the shipped default is 5.
The implementation indexes live irredundant clauses only, keeps factor literals
below the extension boundary, and preserves a deterministic occurrence order.
Selection crosses `Kernel::factor_selection`; proof-bearing mutation remains in
Factor.

## Landed validation contract

- `factor_size=2` remains trajectory-identical to the pre-generalization
  implementation on the pinned Factor fixtures.
- Hand-built multi-literal grids cover proof order, repeated remainder content,
  extension boundaries, and SAT model projection.
- DRAT/LRAT, random-policy fuzzing, and targeted Factor stress cover the same
  implementation.
- Candidate-refinement rounds, redundant grid clauses, the gain function, and
  density admission are separate strategy questions; this design does not own
  them.
