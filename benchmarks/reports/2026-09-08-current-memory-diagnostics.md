# Current-source memory ownership checkpoint

## Decision and hardware authority

The runtime target remains the approved Rank 1–50 aggregate ≤1.01× Kissat.
The matched rank-41–50 extension6108/job1676783 used the same development
hardware class and allocation contract as ranks1–40: cpu-dense-hwulab-q,
12 logical CPUs, 48000 MiB, one bound solver thread, 1200 solver-only seconds,
separate checker budget and five-hour allocation. Tasks ran on node017/node018;
each final record retains its CPU/L3 mask. No node was forced. At this checkpoint
19/20 certificates are checked; only Tactix rank43 remains pending. All ten
Kissat checks are complete. The fixed50 denominator remains7239.010 seconds,
preserving the original40 values; cpu-q measurements were not mixed into it.

The separate user-requested fresh accepted-production/current-Kissat reevaluation
6121/job1676920 remains running. Successor6132 retains all50 rows with74 solver
records,58 independently checked/clean records,16 pending validations and26
missing solver records. There is no final aggregate or full zero-residue claim.
No invalidity is published in this snapshot. Pending tasks remain unresolved.

## What warrants memory diagnosis

These are the largest measured checked-pair memory excesses in6132, not a claim
that every rank has been observed. RSS is KiB; seconds are solver wall time.

| Rank | Accepted Tactix RSS | Kissat RSS | Excess | Accepted Tactix s | Kissat s | Cumulative6107 RSS | Cumulative6107 s |
|---|---:|---:|---:|---:|---:|---:|---:|
|21|4725892|1436392|3289500|111.092|96.326|3034340|250.419|
|25|3550256|888820|2661436|259.989|242.870|3409704|250.627|
|9|3725956|1326232|2399724|93.762|68.836|3226172|96.078|

Accepted measurements come from6121. The cumulative experimental measurements
come from checked SAT records020/024/008 of6115. These are different windows
and different solver work: reduced memory does **not** establish a runtime win.
In particular, cumulative rank21 takes substantially longer; retain this cost
in the convergence decision. No repair is promoted on this comparison.

Earlier dense-entry repairs already removed redundant sparse-watch publication
in the cumulative lineage. Repeating that fix on production would not identify
the remaining owner. New campaign6129/job1676999 therefore measures current6107
slot ownership on these three ranks, Tactix-only, with the existing read-only
event-trace observer. All three tasks use independent checked-runner executions
under the same12CPU/48000MiB/1200s contract. Their trace paths are task-specific.
They are queued at submission; no ownership result is claimed yet.

## Implemented and validated scope

No solver source changed. Build6127 pins unchanged6107 with event tracing;
build6128 pins it with the rule journal. Complete fixed suites pass98/98 and
99/99 respectively. Source inventory, build logs and binary hashes are sealed
in6127's summary. Submitted release and other diagnostic pins are untouched.

New maintained `tools/bench/slot_memory_task.py` wraps only the solver's measured
call. It preserves checker execution, timing limits, abnormal solver results
before the first trace row, and restores the observer environment even on an
exception. It refuses to overwrite existing evidence. Six fixed fake-runner
tests cover task isolation, checker exclusion, environment restoration, missing
trace, existing evidence and malformed task IDs. All39 benchmark tests pass.
No real solver executes in these harness tests.

The new6131 sealer requires all three independently checked SAT models and
cleaned final records, exact full non-timing statistics and emitted proof bytes
against each corresponding6115 Release run, matching provisional/final solve
records, immutable pins, and hash-bound trace files. Only then does it publish
peak-owner snapshots. Six malformed-trace fixtures and timing/work comparison
controls pass. SAT model validity is independently checked, not inferred from
proof-byte equality; this is not an assertion of byte-identical model output.

The observer inventories retained capacities and allocator/current/peak RSS at
slot boundaries. It does not exhaustively inventory transient rule-local
scratch. Instrumented time and RSS are diagnostic, not release timing or a
hard-memory acceptance test. No duplicate fuzz campaign is needed for unchanged
source, but latest-source Claude-owned fuzz is still required before promotion.
The previously denied fuzz transfer and push are not retried or bypassed.

## Next action and evidence

Finish6131 against exact job1676999, then use its first high-memory boundary to
choose a general representation or lifecycle repair and an old-code-failing
unit regression. If any full-work/proof replay differs, diagnose observer
neutrality before attributing memory. Continue fresh6121 classification and
pending6108/6114/6115 proof checks asynchronously. Rank4 is already checked UNSAT
on both fresh arms; it is not a reproduced correctness failure. Keep Rank32 and
portable convergence next in priority; preserve established general closures.

No manual deletion, campaign cancellation, source promotion or fixed-reference
replacement occurred. Final-record cleanup and immutable campaign protection
are audited at wrap-up; live checker scratch and traces remain untouched.
All living documents remain within required line budgets, with little reserve.

- `benchmarks/reports/2026-09-08-fresh-rank50-6132/summary.json`
- `benchmarks/reports/2026-09-08-fresh-rank50-6132/comparison.md`
- `benchmarks/runs/20260908-6127-publication-diagnostic-builds/summary.json`
- `benchmarks/runs/20260908-6129-publication-memory/manifest.json`
- `benchmarks/probes/scratch/seal-publication-memory-6131.py`
- Preceding memory closure: `benchmarks/reports/2026-09-07-dense-entry-cost.md`
