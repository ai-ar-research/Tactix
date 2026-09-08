# Reduce preparation root guidance — 2026-09-08

## Decision and hypothesis

The goal remains Rank1–40 ≤1.01× fixed12-CPU Kissat through general repairs,
with B28+C5 and memory/correctness gates. It is unmet.5870's full1.165359× and
C1.015549× regressions are not accepted as progress on performance. New5900 is
an independent successor of the better5849 branch (full1.136184×), without5870's
preconnection cleanup. Hypothesis: consume roots discovered by Reduce's own
preparation before its next decision-guidance operation, eliminating excess
work caused by stale root identities. No runtime gain is claimed before gates.

## Exact cause

5891/job1674840 is terminal and replay-neutral against cached300K controls:
all ordinary trace hashes and partial-proof bytes match. Immediately after
Reorder at225987, Tactix has90617 heap members versus90616. Only three records
differ: Tactix retains assigned/root202958 at65643;80373 occupies32821 rather
than65643;471444 remains at90616 rather than32821. Every shared score and
assignment flag agrees. The native202958 score excludes structural weight;
its queue links are disconnected and it is absent from the heap.

The earlier last-common snapshot5883 shows how this small physical difference
expands to14282 displaced positions before first unequal choice61017805. Native
chooses116746, still free in Tactix, so its assignment does not explain that
choice. Reduce49's complete clause metadata and victim selection already match.

Source inspection identifies the missed caller: `reduce_prepare` backtracks
and propagates retained roots, but DefaultPolicy called it without consuming
new root assignments into queue/heap. Native's assignment primitive performs
that lifecycle during preparation. Reorder correctly uses its live guidance
population; changing its score/tie logic would treat the symptom.
CaDiCaL likewise propagates out-of-order root units before reduction and records
fixing at assignment (`reduce.cpp::propagate_out_of_order_units`, `propagate.cpp`).

## General repair and validation

5900 introduces a DefaultPolicy preparation wrapper around unchanged raw S2
preparation. It snapshots cumulative root assignments, then consumes exactly
the new final trail suffix. The incoming trail size is unsafe: preparation
can truncate a retained search trail before appending new roots. No full-state
scan, option, effort constant, formula identity selector, guard, kernel primitive
or proof vocabulary was added. Raw public Reduce remains policy-independent.

New fixed tests cover two chained root implications after a retained-trail
backtrack, shorter final trail, both decision structures, idempotence, no-root
no-op, untouched non-root variables, public-rule independence, original SAT
model and three refutation shapes in DRAT/LRAT. Actual old and new object-linked
fixtures and native `kissat_flush_and_mark_reason_clauses` reproduce the exact
distinction: old caller retains roots;new policy/native remove them. This is a
scheduling defect, not a solver soundness failure or crash.

All complete suites pass96/96 Release twice,95/95 trace,96/96 journal.
46 independently checked proofs pass:6 new Reduce witnesses plus40 inherited
continuation/caller/edge/active-journal cases. The actual nine inherited journal
outcomes match. Checker invocation/acceptance did not change. Current-source
fuzz remains blocked by previously denied external-transfer approval; no retry,
inspection or generated campaign. Candidate is unfuzzed investigation material.

Authority: `benchmarks/runs/20260908-5900-reduce-root/manifest.json` binds the
three changed files, four binaries, patches, complete suite logs,46-proof
summary and causal5891 snapshot. `controls/summary.json` binds actual old/new/
native controls and their object/library provenance. Submitted pins are immutable.

## Measurements and separate Rank31 continuation

New5906/job1674929 B28+C5,5907/job1674930 complete Rank32 prefix,5908/job1674992
candidate-only full40. All use12CPU/48000MiB and1200s solver time; checked gates
allow16000s checker within5h, diagnostics25min. No throttle, hard node placement,
reference refresh, old-runner change or promotion. Runtime comparisons use fixed
Kissat6810.066s and retain cross-window limitations. Expected verdicts permit
provisional development while proofs run, not checked acceptance.

Separately,5870 Rank31 extended5893/job1674849 is terminal. Both common89336421-
decision hashes reproduce300K. The suffix matches7,400,769 rows before first
split394397/96579679/level6205: Tactix+487794 versusnative+87835. The first later
forward/BVE count/timing difference is at528304/528305, so it does not locate
the earlier choice's cause. Stable entry is393805.5910/job1675028 now captures
the heap after preceding common decision96579678 and87835 assignment reasons;
require every ordinary1M trace and partial-proof byte to replay5893 first.
This diagnostic remains on5870; do not imply5900 includes that regressing fix.

Next decisions: validate5907, score both B and C plus full40, then retain or
reject5900 according to actual outcomes. Interpret5910 independently before
composing repairs. Production and the cpu-q campaign remain unchanged. Known
5784/tasks828/829 checker-status false negatives remain invalid/unscored under
their frozen contract. No manual deletion or push.
