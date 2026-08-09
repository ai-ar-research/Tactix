# Tier-A analysis-order follow-up

**Date:** 2026-08-09

**Fixture:** `tr-n280-s052` (UNSAT)

**References:** Kissat `8af8e56f174b` plus the diagnostic-only semantic-trace
overlay; baseline Tactix `build/pin-bumprate10`.

## Production result after the 50k-event repair

The exact 50,000-event prefix in job `1605897` was necessary but not
sufficient to explain the production regression.  Job `1605901` ran the old
Tactix pin, the cumulative watch/shrink repair, and shipped Kissat sequentially
on one node with a 40-second cap; all six runs returned UNSAT and all DRAT
proofs verified.

| instance | old Tactix | cumulative repair | Kissat |
|---|---:|---:|---:|
| `tr-n260-s002` | 4.83 s / 204,250 conflicts | 5.97 s / 240,154 | 3.85 s / 212,086 |
| `tr-n300-s055` | 24.92 s / 746,064 conflicts | 30.69 s / 839,777 | 22.75 s / 927,770 |

The old behavior is not a stronger algorithm: its learned clauses diverge
through missing reference state and it happened to take a luckier trajectory.
It must not be restored merely because these two samples are faster.

## The next exact divergence

Job `1605902` raised the trace cap to 100,000 events.  Raw 1-UIP, stable
level-sorted, post-shrink, and final learned clauses match through conflict
351.  At conflict 352 the raw and sorted clauses still match exactly, but the
post-shrink results are:

- Tactix: `-103,-89,-258,-97,116,...`
- Kissat: `-103,-258,116,...`

Kissat deletes `-89` and `-97`; its clause is a strict subclause, so this is a
fix-required mismatch under the development policy.  Both solvers returned
UNSAT and both proofs verified.

Two narrower hypotheses were rejected.  Pre-marking all learned literals
removable did not change the divergence (`1605903`).  Removing Tactix's
additional learned-level membership gate likewise left conflict 352 unchanged
(`1605907`), although that gate is independently weaker than Kissat and was
removed as `ANALYZE-MIN-1`; its focused fixture and the full 63-test suite pass
in `1605906`.

## Root cause and next increment

Both solvers produce the same clause sorted by descending decision level.
Their consumers walk it in opposite directions.  Kissat's `shrink.c:253-381`
starts at the end, applying the lowest-level block first; a successful block
marks every visited intermediate removable, and that state is deliberately
available to later, higher blocks.  Tactix starts at index 1 and therefore
applies the highest-level block before those anchors exist.

This state-flow difference explains why pre-marking only the clause variables
was neutral: the missing anchors are intermediate reason variables produced by
lower block walks, not necessarily literals in the original learned clause.
The next one-at-a-time increment is `ANALYZE-BLOCK-ORDER-1`: traverse blocks
from low to high while retaining the sorted learned-clause output order, then
require exact trace agreement past conflict 352 before rerunning the two cheap
production instances.  Tier B and Tier C remain out of scope until Tier A is
healthy.
