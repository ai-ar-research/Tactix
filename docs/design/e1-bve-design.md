# E1 BVE — frozen compact design record

**Status:** implemented. Current semantics are in `docs/calculus.md`; guards,
scope, scheduling, and deviations are in `docs/implementation-notes.md`.

## 1. Durable rule contract

### B1. Bound before mutation

For a pivot `x`, collect the live positive and negative parents, construct the
unique non-tautological resolvent set, and test the configured growth bound
before mutating the formula. A declined pivot leaves formula, proof, and
reconstruction state unchanged.

For an accepted pivot, emit every resolvent as RUP from its two parents, delete
the pivot clauses only after all additions are live, and append the ordered
witness segments needed to reconstruct `x`. Root-false literals in parents
must contribute their unit ids to native LRAT chains.

## 2. Landed engineering

The implementation includes syntactic and definition gate extraction, backward
subsumption/strengthening over fresh resolvents, extension-variable exclusion
in DefaultPolicy, private effort accounting, and the BVE-bound/Factor coupling.
These are not specified here; the scheduling ledger owns their defaults,
accepted deviations, counters, and reopening paths.

Gate extraction has two distinct clause views. Resolution uses the active
root-filtered parent and cites stripped root units in LRAT chains. General
definition extraction follows Kissat's physical occurrence export: it removes
only the pivot and retains root-false literals as embedded Kitten variables.
AND/OR membership likewise follows the physical binary-watch stream, including
duplicate-copy mark consumption and root-shortened physical large bases. These
views must not be collapsed into one convenience representation.

## 3. Retired material

The original pre-implementation inventory, global-tick scheduler, staged
`add_rat` discussion, and proposed retirement protocol are historical. E1
uses RUP additions plus deletion and reconstruction witnesses; native LRAT,
retirement, and model-extension behavior are covered by focused tests and the
living docs.
