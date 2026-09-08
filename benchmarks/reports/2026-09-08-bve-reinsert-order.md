# BVE reinsertion order: concrete clause witness

The runtime objective remains unmet: retained candidate5962's full5970 view is
7603.117/6810.066 seconds, **1.116453x fixed Kissat**. No new solver change,
timing claim, production promotion or fuzz run belongs to this checkpoint.

## Decision-changing evidence

Diagnostic5992/job1675996 completed both tasks. Its `heap-validation.json`
verifies complete ordinary300K traces, partial certificate bytes and BVE journals.
The first49 selected5409 states agree after stage-name normalization. Its next
actual insertion has size9003/position9002 in Tactix, versus size9004/position2250
in native, at identical score `0x1.e841fp+21`, positive6 and negative40.
These are both completed pushes, not native's separate pre-push score update.

Independent5996/job1676156 reuses frozen5949 observer binaries and adds existing
retry-producer output, selecting eight subjects including5409,5415,5419,5232.
Validator5998 proves all ordinary streams, partial proof bytes and complete BVE
journals replay. Both tasks exit0 at the requested stop and own scratch cleanup.

| Completed insertion | Tactix | Kissat |
|---|---|---|
| Heap size9003, position9002 |5409|5415|
| Heap size9004, position2250 |5415|5409|

Both variables have identical scores and occurrence counts across arms.
Subsequent5419 and5232 pushes move the position2250 member down to4502 then9006.
Thus the neighboring insertions exchange which subject follows that path.

Tactix's actual `delete-clause` producer for5409 records retry0→1, unassigned,
not retired, at conflict271079/decision67954244 with physical literal order:

`-246596,-47,49,-5409,5415,148944,238853`

This is exactly the seven-literal incoming5937 clause, canonical hash
`3d497c2ff92711f2`. Native's incoming pose is:

`-246596,5415,-47,-5409,49,148944,238853`

The adjacent eleven-literal clause has the same incoming pose in both arms.
Source inspection shows clause-removal scheduling traverses physical literals.
The candidate journal places the insertion burst between pivot148944's begin/end
and the next331390 pop. The evidence strongly implicates incoming clause pose;
native's producer has no clause payload, so its exact deletion pose is not
directly recorded. Do not claim that the origin of the pose difference is solved.
No evidence justifies changing heap scores, ties or watch-replacement rules.

## Validation and continuation

5998 adds producer schema/window controls and completed-push controls rejecting
missing, duplicated, pre-push, absent-member and nonconsecutive-size sequences.
Existing selected-heap parser controls, full replay and22 benchmark unit tests
pass. No solver source changed; therefore no new solver unit or fuzz campaign
is warranted for this diagnostic-only increment. Future fixes still require
explicit unit-test consideration, four applicable suites, proof/model controls,
B28+C5 gates and full Rank1–40 scoring.

New6000/job1676209 captures the **first** Strengthen begin at271079 with unchanged
5937 pins;6002 requires full replay and ordinal0 boundary counts. Compare its
selected seven-literal clause against5937's first Strengthen exit. An incoming
pose difference directs the next observer earlier in sparse propagation/phase
transitions; an equal pose directs it into forward simplification. Use5962, not
diagnostic5985/5900, as the base for any implementation repair.

Fresh normal-job audit: `2026-09-08-bve-reinsert-order-validation.json`.
Proof checks continue asynchronously, pending results remain unverified. The
cpu-q5784 runner/manifests/pins are unchanged. No manual deletion or fuzz-owner
artifact inspection; denied fuzz transfer and push are not retried.

Primary authorities:

- `benchmarks/runs/20260908-5992-bve-subtree-ancestry/heap-validation.json`
- `benchmarks/runs/20260908-5996-bve-reinsert-order/heap-validation.json`
- `benchmarks/runs/20260908-5996-bve-reinsert-order/manifest.json`
- `benchmarks/runs/20260908-6000-bve-clause-entry/manifest.json`
- `benchmarks/probes/scratch/validate-bve-reinsert-order-5998.py`
- `benchmarks/probes/scratch/compare-bve-clause-entry-6002.py`
