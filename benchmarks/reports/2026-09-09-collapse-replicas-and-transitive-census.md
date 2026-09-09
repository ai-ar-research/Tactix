# Replicated collapse timing and Rank 32 transitive census

## Decision

Retain the source-backed6305 retry repair provisionally, but do not promote it
or call it an aggregate breakthrough. Three paired waves confirm substantial
changed-work improvements on Ranks32/33 and a real adverse effect on Rank40.
The next Rank32 diagnostic is the census-identified binary `(936 OR -30097)`;
do not add speculative transitive priority state.

## Matched timing

Authority: `benchmarks/runs/20260909-6332-collapse-three-wave-runtime/solver-checkpoint-6343.json`.
All18 expected UNSAT solver outcomes exactly reproduce each arm's own full-run
non-timing statistics and certificate bytes/hash. Only3 certificates were
checked/clean at this frozen checkpoint; the rest continue asynchronously.
This is6305 versus6269, not versus Kissat and not isolated instruction cost.

| Rank | Three candidate/previous ratios | Median | Candidate/previous spread |
|---|---|---|---|
|32|0.633727,0.635267,0.628785|0.633727|0.194% /0.886%|
|33|0.713787,0.705451,0.707691|0.707691|0.482% /0.864%|
|40|1.141360,1.134171,1.144362|1.141360|0.451% /0.659%|

Spread is range/median within each arm; all are below the predeclared2% limit.
Six independent tasks per wave, middle wave reversed arm order. Jobs1677553
(04:49:13),1677560 (05:02:38),1677568 (05:14:30), all September9EDT,
use12 logical CPUs/48000MiB/1200 solver-only seconds/16000 checker seconds/5h,
one bound solver thread, full width and no hard node constraints.
Observed nodes are017/018; missing provisional affinity/context stays unknown.

The full50 cross-window score remains8598.949/7239.010=1.187862567x.
Added41--50 contribute1201.154/428.944=2.800258309x, hence772.210 of
the net1359.939-second gap. Their matched12CPU baseline6108 already contains
all20 independently checked outcomes. The4CPU cpu-q timing is not mixed in,
and neither the fixed reference nor the old cpu-q runner was modified.

## Census and rejected accounting assumption

Authority: `benchmarks/runs/20260909-6340-rank32-transitive-census/summary.json`.
Job1677566 since2026-09-09T05:05:46 completed both unchanged6305/6090
diagnostic pins. Every ordinary current-parent trace and partial proof
bytes/hash replays exactly through3M conflicts. The added all-source trace
contains6,784,613 candidate rows and8,128,209 native rows; all are parsed to
EOF with schema, identity, sequence, event, effort and physical-deletion checks.
These are trace-row counts, not search decisions or benchmark timing.

Initial validator6342 passed replay/framing, then failed its assumption that
all reductions at conflict1368343 must total1/2. The full boundary logs show
two invocations at that conflict: first121/121 deletions, then1/2. Successor6344
retains6342 unchanged and validates both begin/end pairs, no failed-source unit
effects, and totals122/123. This was a comparator expectation failure, not
a solver failure. Two regression tests cover repeated-conflict invocation
accounting and malformed boundary pairing; all51 benchmark and13SLURM tests pass.

Across both calls, the complete physical-copy deletion multisets differ only
by native's one copy of `(936 OR -30097)`. The candidate census has no source936
row at this conflict; native processes10 candidates there and deletes this one
at ordinal3, spending14 walk ticks. The census localizes a missed source, not
yet its eligibility/order/budget cause or whole-formula equality. It does not
prove that this single deletion causes the later selected-decision divergence.

Source audit also disfavored a proposed native transitive-priority flag repair:
the pinned flags start cleared, and the audited source contains reads/clears
but no producer setting this flag. Tactix's cursor acts before its reference
sort. These observations do not justify a new scheduler state or reopening
the historical SP-TRANSITIVE-ORDER-1 closure without causal current evidence.

## Next action and safety

Selected-pair successor6345/job1677574 since2026-09-09T05:28:56 uses the same
pins/resources and existing endpoint/admission/walk observer. Require full
non-transitive parent replay and partial-proof identity before interpretation;
add selected-event parser tests as needed. Original census/proof/input cleanup
is runner-verified; partial proof hashes are not final-proof acceptance.
No manual deletion and no live checker scratch alteration in this increment.

Production remains unchanged. Source-specific Claude-owned fuzz, independent
memory convergence, full checked timing and cumulative promotion remain open.
No fuzz artifact inspection, denied transfer retry or push was attempted.
The Rank50 added-material tracker and private FlatLists redesign still require
approval. Continue final checkers without blocking development; a rejection
pauses affected dependent work. The wrap-up audit owns the later job states.
