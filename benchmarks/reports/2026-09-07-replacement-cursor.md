# Replacement membership cost and complete phase-entry view

7 September 2026. Production remains `c2a95a6`, checked Rank 1–40 ratio
1.2787518x. The objective of at most 1.01x Kissat remains unmet. This checkpoint
adds a general, isolated cost optimization, not a production promotion.

## Full requested view and newly checked evidence

[Phase-entry 5434](../runs/20260907-5434-inprocess-root-ranks1-40/solver-checkpoint.json)
has all 80 expected solver results: both arms solve 40, 22 SAT/18 UNSAT, no
timeouts. Solver/PAR-2 totals are **8112.221/6728.896 seconds = 1.205579786x**;
60/80 certificates were checked at the snapshot. Max RSS across each arm is
15,967,668/15,348,304 KiB. Pending certificates remain provisional, and these
maxima do not establish per-instance memory competitiveness.

The preceding Rephase view was 1.202033658x. Different measurement windows
prevent interpreting the ratio difference as a paired slowdown; neither closes
the goal. Phase-entry's largest losses remain Rank 37 (+285.870 s), 32
(+183.630), 31 (+159.516), 40 (+125.659), and 27 (+118.945). Its conflicts and
decisions change substantially on Ranks 27, 33 and 39; Ranks 31 and 37 retain
those two counters and differ by one propagation. The missing root-closure
repair therefore does not explain the main remaining losses. Preserve the
queued Rank-37 prefix and Rank-35 earlier Sweep-boundary diagnostics.

[5425 profiles](../runs/20260907-5425-rephase-root-profile/summary.json) are now
both proof-checked and clean. [5428 metadata timing](../runs/20260907-5428-watch-truncate-rank35-runtime/summary.json)
has all six checked certificates: exact paired work/proof identity, median
0.993882186x predecessor, range 0.989911575–0.999527763. This small measured
gain remains independent of phase-entry/Vivify, with its B28+C5 tail pending.

## Cost attribution changes the next implementation

Read-only annotation [5444](../runs/20260907-5444-inprocessing-annotation/manifest.json)
uses the existing clean 5425 candidate profile and verified immutable binary.
Large-clause replacement has 1313 samples and 1.84% of total self periods.
Its three membership-search loops account for 33.09%, 32.90%, and 32.72% of
replacement-local periods: **98.71% combined**, subject to printed rounding
and sampling uncertainty. Address intervals are `[0x421d20,0x421d53)`,
`[0x421e58,0x421e8d)`, and `[0x421f68,0x421f9d)` in that exact binary.
These are measured sample locations, not a claim that all 1.84% is removable.

Attempt 5443 used prefix-only symbol names; `perf annotate` returned exit zero
with empty output and “no samples.” It was not usable evidence. The corrected
helper uses complete demangled names and requires nonempty output and absence
of that error. Both attempts retain their diagnostics; decompressed temporary
profile data was removed by their exact temporary-directory scopes.

## Isolated candidate 5445

[The source authority](../runs/20260907-5445-replacement-cursor/manifest.json)
descends from Vivify 5438, excluding metadata 5426. Only the private membership
queries inside `kernel_replace_large_clause_in_place` change. Each starts
after the previous successful match, searches to the end, then wraps across
the prefix. These disjoint ranges cover the same complete array, so membership
answers are unchanged for arbitrary order, absent literals and repeated calls.

Ordered unchanged runs avoid rescanning every preceding literal. This is not
a general linear-time set algorithm: adverse orderings or many absent literals
can still be quadratic. There is no allocation, shared scratch, marking,
sorting, new kernel primitive, API, guard, proof operation or scheduling change.
All existing occurrence removal/addition, watch retention, retry advice and
notification order remain unchanged. Calculus and production ledger therefore
need no semantic or disposition change.

Native Kissat's `sweep.c:substitute_connected_clauses` constructs images while
retaining unchanged runs; CaDiCaL's `subsume.cpp:strengthen_clause` uses stable
removal before shrinking. Tactix's general transition must still accept
arbitrary images, hence the complete wraparound fallback rather than imposing
an ordering restriction. The preceding [Vivify report](2026-09-07-vivify-max-selection.md)
owns the parent change and background-fuzz workflow.

## Fixed validation and submitted gates

The new fixture executes 840 fixed cases, two replacements each: five lengths
(3, 4, 7, 32, 129), seven image shapes, both clause kinds, sparse/dense/ordered
dense, added-material on/off and retained-watch on/off. It covers ordinary
shrinking, all-new literals, rotation, reversal and mixed replacement. Actual
immutable 5438 objects and both new builds produce the same state digest,
**`f86da0be45055db3`**. Checks include arena span/image, physical watch storage,
dense order/counts, retry/touch marks and BVE notification order.

Independent Release builds 5445 and 5446 pass **80/80 tests**, respectively
6.57 and 7.05 seconds. Both binary SHA-256 values are
`b92994ec05c61c3197985f22a45d3df82da4a8ec9301d2d1c99663e46dbac99c`.
Proof/model and public-rule microtests remain in the aggregate suite; the
structural fixture is not itself a semantic solver proof.

- [5447 timing](../runs/20260907-5447-replacement-cursor-rank35-runtime/manifest.json),
  job `1670198`: three exact-predecessor 5438 pairs. Require identical full
  non-timing statistics and proof bytes for every pair; report median and full
  spread. Absolute timing headlines require both arm ranges within 2%.
- [5448 B28+C5](../runs/20260907-5448-replacement-cursor-BC-gate/manifest.json),
  job `1670199`: unchanged cumulative 66-task regression gate against production.

Both are immutable full-width arrays, one solver per task, 12 logical CPUs and
48000 MiB, no throttle or node constraint. No candidate runtime benefit is
claimed before measurement. These gates do not substitute for all 40 ranks.

## Continuation and hygiene

Claude owner session `30094` is still running the user-requested background
fuzz checkpoint on 5438. No fuzz job/artifact was inspected. It does not cover
5445: include the new kernel change in the next periodic checkpoint, and do
not promote it while unfuzzed. Any reported correctness failure interrupts
dependent work. Existing proof checking remains asynchronous.

The scoped validation index owns current runtime-job and pin identities,
production test evidence, documentation checks and Andrew report hashes.
The generic progress check's all-job/all-build scans remain deferred to avoid
probing fuzz-owned resources; citations and explicit non-fuzz audits run.
Living documents remain within budget. No material or active campaign was
deleted; submitted pins and user allocation `1668242` are untouched. Push
and separate future solver/checker job contracts remain unapproved.

Next: inspect complete 5436/5437 traces; evaluate 5440/5447 exact-work timing;
revisit final regression/proof/fuzz results. The full runtime goal remains active.
