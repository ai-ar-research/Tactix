# Rank32: first later split at BVE386968

The [6183 extension](../runs/20260909-6183-rank32-current-prefix/summary.json)
is terminal on both arms, with the requested one-million-conflict stops and
partial-certificate/input cleanup. Both reproduce the earlier69828932-row
rolling hash10d4b4ca139060f5. The253 overlapping decisions at conflict300000
are byte-identical to the cached tail and counted only once. Complete bound
suffix files have29103368 candidate and26254700 native rows.

The first differing decision follows78180275 matching rows from startup:

| Field | Tactix6107 semantic trace | Kissat |
|---|---:|---:|
| Conflict |386968|386968|
| Decision counter |78026757|78026757|
| Level |225|225|
| Chosen literal |−23007|−101153|

This is the observed choice boundary, not yet its cause. An older four-copy
binary-count difference after Sweep19375 remains visible, although decisions
match far beyond it. Do not discard that discrepancy or attribute the late
choice to it without evidence.

At386968, incoming portable counts agree through the first Strengthen and BVE
entry. The first BVE pass leaves Tactix72915 active variables/500959 irredundant
clauses versus Kissat72916/500955. Later passes further diverge. Equal incoming
counts do not prove identical formulas, queues, budgets or candidate order;
an extra elimination is not automatically weaker. The existing general root
closures must be retained until a source-backed cause and stronger repair are
established.

New [6188 candidate census](../runs/20260909-6188-rank32-bve-candidates/manifest.json),
job1677174, uses the same immutable binaries and adds only the existing direct
BVE-candidate observer at386968. Before interpreting its extra rows, require
exact complete6183 suffix/boundary streams and partial-proof bytes/hash per arm.
Then identify the first candidate/order/result/effort difference and trace its
producer. No heap-order tuning or default parameter change is authorized by
these counts.

Separately, the full sorting candidate6181 now has all50 expected results:
8403.998s/7239.010s =1.160931951x fixedKissat, still outside the1.01x goal.
[6190 checkpoint](../runs/20260909-6181-learned-sort-ranks1-50/solver-checkpoint.json)
binds all50 exact predecessor non-timing statistics and proof bytes/hash, the
independent-check count and per-rank memory. The prior6107 full window was
8559.858s; that cross-window difference is not a measured isolated gain. The
checked three-pair Rank4 improvement remains0.961681x exact predecessor.
Checking continues in the background; no promotion or memory-gate closure.
