# Tier-A OTFS watch and reason-lifetime repair

**Date:** 2026-08-09

**Gap:** `ANALYZE-OTFS-WATCH-1`

## Mismatch and strength judgment

After `ANALYZE-OTFS-COUNT-1`, the 200,000-record `tr-n280-s052` trace had
identical shared semantic events but one extra Tactix watch scan at conflict
490.  The clause was not absent from Kissat.  It was the strengthened version
of conflict 486's learned reason, present in both formulas:

`-235,-218,-211,-203,-152,-133,-113,-63,-22,-9,16,23,31,32,38,49,60,73,181,197,258,270`.

Kissat `strengthen.c:6-77` shrinks the reason in place.  Its surviving old
watch remains in the same watch-list position, only the removed literal's
watch is replaced, and the old clause identity now denotes the stronger
clause.  Tactix instead allocated a fresh stronger clause and kept the old
reason indefinitely.  Thus later propagation could scan both the original and
the replacement; in the localized prefix that produced the extra scan.

Keeping both clauses is not a strict improvement.  The stronger replacement
logically entails the old clause, so the duplicate adds no formula strength
and consumes propagation and reduction work.  Tactix cannot copy Kissat's
in-place mutation directly: arena records are variable-sized, every arena walk
uses the size to locate the next record, and the old clause must remain a valid
reason until its propagated literal is unassigned.  Those are representation
constraints, not a reason to retain the duplicate after conflict analysis.

## Repair

OTFS still allocates the replacement, preserving a valid kernel state during
Analyze.  For a non-root surviving watch, it now follows Kissat's exact watch
choice: preserve that watch, choose the first highest-level replacement on a
tie, and exchange the old and new Ref entries in the shared watch list so the
stronger clause inherits the old list position.  Both clauses remain fully
watched and pass kernel invariants during the internal Analyze/Backjump seam.

Backjump then unassigns the analysis-level propagated literal.  Before the
atomic `ConflictAnalysis` rule returns, Tactix unwatches and proof-deletes the
old reason.  An assertion-enabled check pins that no trail assignment still
cites it; this has zero production-path scan cost.  The delayed Ref list is
forwarded by garbage collection, and an eager-recent entry transfers to the
large replacement (or is vacated for a binary replacement).

Tactix continues to remove a root-false surviving watch when constructing the
replacement.  That produces a strict subclause under the solver's explicit
root units and is therefore the stronger accepted deviation; when it fires,
there is no reference watch position to preserve.

## Verification

An assertion-enabled focused fixture covers a large reason: both old and new
clauses satisfy watch invariants between Analyze and Backjump, then the old
reason is garbage and unwatched after Backjump.  The existing binary fixture
now pins the same lifetime boundary.  Kernel invariants additionally require
every large-watch blocking hint to be a member of the clause it can suppress;
this caught and prevented a reversed-blocker wiring during pre-commit review.
The exact final focused Analyze fixture passed in job `1605931`, and the
release suite passed 63/63, including the demo, in job `1605930`.

Job `1605928` produced the repaired 200,000-record Tactix trace.  Comparing it
to the frozen detailed Kissat trace from job `1605915` reports no macro-order
divergence and no semantic-trajectory divergence.  The conflict-490 scan over
literal 63 now has the same 12 records in the same order; the duplicate
strengthened-reason record is gone.  At sequence 130280 the strengthened
clause now also carries Kissat's blocker `32`, rather than the removed reason
literal.  Both full runs returned UNSAT and both DRAT proofs verified.  The
state comparator still reports the known detailed-record schema artifact
(`irred=0` on Tactix watch records), which neither changes nor obscures the
semantic comparison.

The release binary is `build/pin-kissat-otfs-retire-v3/tactix`, SHA-256
`97ca5087d1f5bdf627cf26a52124523a8a31785fe71a5710d1c3734dddd98205`.
It contains no event-trace symbols, environment strings or stage labels.

## Two-instance Tier-A gate

Job `1605929` ran the prior OTFS-counter pin, the exact final replacement pin,
and Kissat sequentially on node017.  All six runs returned UNSAT and all six
DRAT proofs verified.

| instance | prior Tactix | repaired Tactix | Kissat |
|---|---:|---:|---:|
| `tr-n260-s002` | 7.25 s / 284,783 conflicts | 4.81 s / 204,614 | 3.85 s / 212,086 |
| `tr-n300-s055` | 29.95 s / 854,300 conflicts | 28.76 s / 815,632 | 22.70 s / 927,770 |

The first instance's abnormal trajectory is repaired: Tactix now uses fewer
conflicts than Kissat and is within 0.96 s.  The second improves only modestly
and remains 6.06 s slower despite fewer conflicts, pointing to per-conflict
work and/or a later mismatch rather than excess conflicts alone.  Continue on
that single Tier-A instance; Tier B and Tier C remain deferred.
