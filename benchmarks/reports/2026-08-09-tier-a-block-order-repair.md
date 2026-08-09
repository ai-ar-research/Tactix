# Tier-A block-order repair

**Date:** 2026-08-09

**Gap:** `ANALYZE-BLOCK-ORDER-1`

## Repair and correctness

Tactix now applies the blocks of a descending-level learned clause from the
tail toward the asserting literal.  This matches Kissat `shrink.c:253-381`:
lower blocks run first and publish their walked variables as removable anchors
for later, higher blocks.  The stored learned-clause order remains descending.

A focused Analyze fixture makes a higher block depend on an intermediate made
removable by a lower block.  The corrected tail-to-head order learns the strict
subclause; the old head-to-tail order retains one literal.  The full 63-test
suite passed in SLURM job `1605910`.

Job `1605909` then traced `tr-n280-s052` to 100,000 records.  Tactix and Kissat
matched 75,730 watch visits, 21,097 reasoned implications, 380 conflicts, 505
decisions, and all 380 raw, sorted, post-shrink, and final learned clauses.
The semantic comparator reports no trajectory or macro-order divergence.  Its
record-3 state report remains the known detailed-record clause-count schema
artifact.  Both full runs returned UNSAT and both DRAT proofs verified.

In particular, conflict 352 now matches exactly:
`-103,-258,116,-211,-227,-11,-235,-106,-236,-250,-46,-102,-150,-56,-192,-243,-96`.

## Production check

Job `1605911` reran only the two cheap Tier-A hard cases sequentially on
node017 with proof generation and verification:

| instance | old Tactix | cumulative repaired Tactix | Kissat |
|---|---:|---:|---:|
| `tr-n260-s002` | 4.87 s / 204,250 conflicts | 5.99 s / 240,661 | 3.84 s / 212,086 |
| `tr-n300-s055` | 24.83 s / 746,064 conflicts | 35.92 s / 960,672 | 22.91 s / 927,770 |

All six proofs verified.  The comparison does not justify reverting the
repair: the replaced behavior loses a Kissat-valid strict-subclause derivation
and is therefore not strictly stronger.  It shows that another trajectory
mismatch remains after the exact 380-conflict prefix.  The next step is to
raise the same single-fixture trace cap and localize that mismatch; Tier B and
Tier C remain blocked on Tier-A diagnosis.
