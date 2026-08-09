# Tier-A OTFS conflict-accounting repair

**Date:** 2026-08-09

**Gap:** `ANALYZE-OTFS-COUNT-1`

## Localization and repair

The 200,000-record extension of the `tr-n280-s052` Kissat/Tactix trace first
diverged in solver state at propagation conflict 487.  Both solvers reported
the same conflicting clause and derived the same on-the-fly-strengthened
clause.  Kissat then counted that clause as conflict 488 before restarting
1-UIP analysis; Tactix repeated the analysis while leaving its conflict count
at 487.

This is executable reference behavior, not a reporting convention.  Kissat
`analyze.c:542-558` increments `conflicts` immediately after
`kissat_deduce_first_uip_clause` returns a strengthened conflict.  Glue-tier
maintenance is checked at the same site, and the conflict counter is the clock
for restart, reduction, mode switching, rephase, reorder and inprocessing
deadlines.  Tactix's missing increment could therefore only shift reference
schedules; it could not produce a strictly stronger application order.

Tactix now increments `Stats::conflicts` on the OTFS restart edge, before
resetting the aborted analysis round.  A focused regression extends the
existing OTFS fixture and pins the transition from one propagation conflict to
two counted conflicts after reanalysis.

## Trace and correctness evidence

SLURM job `1605915` reran both traced solvers to 200,000 detailed records.
At the localized OTFS event the conflict count and the raw, level-sorted,
post-shrink and final learned clauses now match.  No shared semantic event
differs through the cap, and both complete runs returned UNSAT with verified
DRAT proofs.

The trace has one remaining length difference at the cap: Tactix scans one
additional watch for the freshly allocated strengthened OTFS clause.  Kissat
strengthens the active reason in place, whereas Tactix currently preserves the
old reason so that every public N1-visible kernel state remains valid.  This is
a separate representation mismatch, not a consequence of conflict
accounting.  It must be kept only if the stronger public-state contract cannot
be retained without the redundant scan; otherwise it is the next repair.

The first full suite run exposed a trajectory-sensitive demo coverage fixture:
the scripted policy no longer crossed a reduction threshold.  Its own contract
allows resizing when an ordinary search shift clears every threshold.  The
deterministic fixture was changed from 152 variables / 647 clauses to 160 / 681;
the scripted run then reaches three reductions, and the corrected full suite
passes 63/63 in job `1605918`.

The production binary is
`build/pin-kissat-otfs-conflict-fix-v2/tactix`, SHA-256
`ce3e506e8cabb1aebf364f2ee11cdb42c3ca3cbaf3a1b0ea06455befb6bd2d6f`.
It contains no event-trace symbols, environment strings or stage labels.

## Two-instance Tier-A check

Job `1605919` ran the old baseline, the cumulative repaired Tactix binary and
Kissat sequentially on node017.  All six runs returned UNSAT and all six DRAT
proofs verified.

| instance | old Tactix | cumulative repaired Tactix | Kissat |
|---|---:|---:|---:|
| `tr-n260-s002` | 4.81 s / 204,250 conflicts | 7.09 s / 284,783 | 3.90 s / 212,086 |
| `tr-n300-s055` | 24.71 s / 746,064 conflicts | 29.97 s / 854,300 | 22.76 s / 927,770 |

The repair does not close the Tier-A performance gap.  It is nevertheless
required: the old counter lost a reference event and made all downstream
conflict schedules incorrect.  Tier B and Tier C remain deferred.  Continue
one fixture at a time by classifying the extra OTFS watch, then raise the trace
cap again only after that mismatch is either repaired or justified as a strict
improvement.
