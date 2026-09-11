# Realloc: repeated memory benefit and two adverse effects

Frozen decision checkpoint, 2026-09-11. Source7267 and its submitted binaries
remain unchanged. Retained7144 and productionc2a95a6 remain unchanged.

## Complete targeted result

[7281 assessment](2026-09-11-dense-realloc-assessment-7281.json) includes all30
predeclared outcomes: three concurrent candidate/actual7253 pairs on each of
9,25,31,35,38, reversing the middle wave. Every non-timing statistic, including
dense physical moves/defrags, and every certificate fingerprint matches.
Independent checks were16/30 complete at the frozen assessment; later published
checks are separate. No outcome or repetition was excluded.

| Rank | Median runtime ratio | Complete ratio range | RSS ratio | RSS change MiB |
|---:|---:|---:|---:|---:|
|9|0.991959049|0.991207154--0.993075265|0.898698409|-268.18|
|25|0.996009413|0.982551165--0.997928715|0.869630463|-363.79|
|31|0.997133097|0.991806378--1.004160051|1.136853840|+66.18|
|35|0.996164619|0.993525516--0.997407143|0.977631449|-11.46|
|38|1.056273864|1.053445332--1.056983066|0.946253474|-4.00|

RSS is exact across all three repeats within each arm. Every complete within-arm
time spread is under2%; the largest is1.61% on parent25. This is a mixed
tradeoff, not an overall speedup. Rank38 outside-slot median is1.060989242 while
its slot median is0.991081046. Its BVE timer is only about37ms, so a percentage
change in that timer cannot explain an approximately19-second whole-run loss.

## Rank38 propagation localization

[7276 static inventory](../runs/20260911-7276-realloc-propagation-code/summary.json)
verifies identical kernel.cpp and watch.hpp sources. Nevertheless the ordinary
propagation function changes from3616bytes/829instructions to3123bytes/719
instructions. Watch-growth inlining/call sites and register allocation differ;
the change is not merely relocation of an identical instruction body.

The independently pinned7278 matched Rank38 profile pair has exact own Release
work/certificates, no lost samples and complete period conservation. Its proof
checks remain asynchronous. All three phase calls were source- and assembly-
reviewed; missing phase ancestry remains explicitly named, not equated to CDCL.

[7279 envelope](../runs/20260911-7279-realloc-rank38-samples/wave0/summary.json)
measures complete ordinary propagation at931495926342 versus856765834224 sampled
periods:1.0872234735943753x. Probe is0.995681361x; Eliminate0.993645389x; the
no-phase-frame complement1.059782996x. This corroborates the repeated outside-
slot slowdown, not an inprocessing allocation-callee penalty.

[7280 own-PC localization](2026-09-11-realloc-propagation-cost-7280.json) binds
every selected leaf PC to its own pinned instruction inventory. Self periods
are926755662469 versus851248140032 (1.088702129x); nearly all the envelope
increase is inside propagation itself. Both profiles concentrate at the watch
tag/value dispatch. Different bodies are never paired by instruction index or
offset. Sampling/skid, changed code generation and allocator-address history do
not by themselves establish a cache, predictor or compiler-heuristic cause.
No alignment tuning or speculative propagation rewrite was performed.

## Rank31 observer limitation

[7277 memory pair](../runs/20260911-7277-realloc-rank31-memory/summary.json)
completed both whole solves within the existing solver budget, with exact own
Release work/certificates. Independent UNSAT checks remain pending. The wrapper
selects only the existing read-only memory observer; new fixed tests cover both
solver aliases, non-solver bypass, one invocation and environment restoration.

[7282 comparison](../runs/20260911-7277-realloc-rank31-memory/comparison-7282.json)
matches all10538 structural rows: owner capacities, word counts, event identities
and semantic markers. Only the seven explicitly named RSS/allocator-occupancy
fields may differ. But trace RSS is505724KiB candidate versus509956KiB parent,
whereas Release repeats are562920 versus495156KiB. Thus the trace pair does not
reproduce the Release regression. It would be wrong to assign the Release peak
to the trace candidate's Collect-entry peak or the trace parent's BVE-growth
peak. A Release-faithful low-overhead observer or allocator-history experiment
is required for that attribution; the measured Release penalty stays scored.

## Full50 decision and remaining work

The repeated9/25 savings justify measuring breadth across the complete approved
set, while retaining the31/38 regressions explicitly.7283/job1681113, submitted
05:40:56EDT, runs all50 with the immutable7267 Release binary, unchanged full50
runner/matrix and fixed Kissat denominator. It requests12CPU,48000MiB,
1200solver-only seconds,16000checker seconds and5h allocation, fullwidth with no
node constraint. No old campaign, runner or pin was changed.

All physical/non-timing counters must match own7253. The reader records both
parent and candidate validation states: pending parent proofs permit provisional
comparison, not accepted correctness. Preserve every RSS/time increase, solve
loss, invalidity and shelved21. No whole-set allocator score is available at
this checkpoint. The prepared manifest contains one transcribed profile-ratio
decimal corrected by its additive metadata-erratum.json; the already hash-bound
7280 numerical authority, resources, gates and all measurements were correct.

The last complete single-image view remains8057.066s / fixed7239.010s =
1.113006613x, with745.6659s still required to reach1.01x. This allocator experiment
is neither promoted nor rejected solely because benefits and costs differ.
Full50, Release31 memory attribution, Rank38 cost cause, independent checks/zero
residue, current-source Claude-owned fuzz and consolidation remain open. Codex
debugs directly; no denied fuzz access/transfer or push is retried.

Only1180017428bytes of verified temporary7279 perf expansions were removed.
Compressed raw profiles, extracted streams, source/code inventories, tests and
cleanup receipts are retained. No solver source was edited during this stage.
