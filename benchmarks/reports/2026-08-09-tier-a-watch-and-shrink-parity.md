# Tier-A watch and analysis-shrink parity

**Date:** 2026-08-09

**Fixture:** `tr-n280-s052` (UNSAT)

**Kissat source:** `8af8e56f174b` plus diagnostic-only trace overlay

## Outcome

Two non-stronger Tactix mismatches were found and repaired one at a time:

1. input clauses and post-preprocessing large watches did not reproduce
   Kissat's sparse watch order;
2. block shrinking did not treat a lower-level literal already in the learned
   clause as an anchor.

After both repairs, SLURM job `1605897` recorded 50,000 events with byte-equal
payload streams: 76,216 watch visits, 20,644 reasoned implications, 370
conflicts, and 370 raw, level-sorted, post-shrink, and final learned clauses.
Both solvers returned UNSAT and both DRAT proofs verified with `drat-trim`.
The comparator's remaining record-3 report is a schema artifact: detailed
Tactix records intentionally omit snapshot clause counts while the Kissat
overlay fills them. Macro order and semantic trajectory report no divergence.

## Watch-order localization

Job `1605886` showed the first early-Lucky watch scan already differed because
Tactix sorted stored input clauses. Kissat's `internal.c:266-365` keeps the
input stack order. Although `clause.c:130-135` calls `kissat_sort_literals`,
the implementation in `sort.c:8-79` returns immediately when the candidate is
unassigned, so the first two file literals remain watched. Tactix now sorts a
temporary copy only for tautology/dedup membership and stably deduplicates the
stored file order.

Job `1605887` then matched all 3,366 early-Lucky visits and first differed at
late Lucky. Kissat's dense simplifiers resume through `dense.c:199-225`, which
flushes and reconnects large watches in arena order. Tactix retained the
mutated early-Lucky lists. `kernel_rebuild_large_watches` now performs the same
sparse resume after every Probe/Eliminate phase, whether or not garbage
collection ran. Job `1605889` matched all 16,258 visits under the 20,000-event
cap and matched the first 55 conflicts and learned clauses.

A rejected intermediate hypothesis is recorded to prevent recurrence:
`kissat_sort_literals` is a watch selector, not a numeric/internal-rank clause
sort. Commit `69336c1` made that incorrect interpretation and was immediately
reverted by `cca4300` before it entered the validated result.

## Analysis localization

The 100,000-event diagnostic (`1605890`) first diverged at conflict 128 while
the selected conflict and every earlier learned clause still matched. The
analysis-stage overlay split each learned clause into raw 1-UIP, level-sorted,
post-shrink, and final records. Job `1605896` proved that the raw and sorted
clauses at conflict 128 were identical and only the shrink result differed.

Kissat default `shrink=3` calls `kissat_minimize_clause`; `minimize.c:143-157`
marks every learned variable removable before returning to `shrink.c`.
Consequently a lower-level reason literal already in the learned clause closes
that branch of the block walk. Tactix used `SEEN` for learned membership but
tested only `REMOVABLE` in this exact branch. Accepting `SEEN | REMOVABLE`
restored the block-local UIP collapse and produced the exact 50,000-event
prefix in `1605897`.

This was not classified as a stronger deviation: it lost a reference-valid
block collapse solely through missing membership state. By contrast, Tactix's
singleton-block minimization removes a proven-redundant literal and produces a
strict subclause. That genuinely stronger behavior remains, while its completed
`TACTIX_MINIMIZE_SINGLETONS` comparison arm is removed.

## Gates and next step

- Focused parser, kernel, scheduler, and Analyze regressions cover stored input
  order, arena-order watch rebuild, and lower-learned-literal shrinking.
- The full 63-test suite and documentation/configuration checks must remain
  green after arm removal.
- The next measurement is production-only and Tier A only: compare the fixed
  binary cumulatively on the cheap hard instances before considering Tier B or
  Tier C.
