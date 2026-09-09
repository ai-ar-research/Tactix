# Queue-stamp repair: complete50 outcome, decision closure and Release cost

## Decision

Do not promote6357 or claim an aggregate gain. It repairs an independently
reproduced guidance defect and closes Rank32's observed decision divergence,
but its complete50 solver view regresses. Retain it only as an isolated
reference-backed development branch while measuring the adverse changes and
investigating execution cost. Production remainsc2a95a6.

Primary full table: `2026-09-09-compact-stamp-full50-6371/{summary.json,comparison.tsv}`
in this directory. All50 expected verdicts returned (25SAT/25UNSAT), no timeout
or solve loss;36 candidate certificates/models were checked at that frozen
snapshot, and all50 fixed references are checked. Pending results remain
unverified. The current audit owns later checked counts, not this snapshot.

| Scoring view |6357 seconds|6305 seconds|Fixed Kissat seconds|6357/Kissat|
|---|---:|---:|---:|---:|
|All50|8705.330|8598.949|7239.010|1.202558|
|Original40|7479.942|7397.795|6810.066|1.098366|
|Added10|1225.388|1201.154|428.944|2.856755|

These are cross-window views, not isolated repair effects. Candidate total is
106.381 seconds above its predecessor. Certificate/non-timing-stat changes
occur on20 ranks; none is excluded. Rank33 improves181.735 seconds and Rank32
29.060; Rank35 worsens171.018, Rank37 98.029, Rank24 70.455 and Rank45 27.101.
The largest remaining runtime gaps are Rank50(+376.739s),31(+214.930),
35(+199.855),21(+152.270) and49(+134.654). The added ten contribute796.444
seconds of the1466.320-second net gap. The older fully checked6169 view,
1.160932x, remains better than this branch; it is not a source promotion.

Memory is not converged: e.g. Rank21 candidate3,108,036/Kissat1,436,392KiB,
Rank35 588,108/269,360 and Rank50 72,192/18,740. Parent-relative RSS changes
mix changed work, sampling and representation; they do not isolate the new
temporary link-vector allocation. The proposed private FlatLists redesign is
still unapproved. No new storage design was implemented this increment.

## Rank32 closure and what it does not establish

`benchmarks/runs/20260909-6365-compact-stamp-rank32-prefix`, job1677644 since
2026-09-09T06:18:41, completed. Validator6368 checks all full-file hashes,
prefix schema, inherited96,083,379-decision hash33974c0cb2f02138, one boundary
overlap row and the complete57,093,593-row suffix against cached native6303.
All153,176,971 selected decisions match through3M conflicts. The old1405411
split is closed. Both stopped proofs were hashed/deleted by their runners;
this is not final-proof acceptance.

The complete current solve also has exactly native's4,636,494 conflicts and
182,202,021 decisions. Tactix's reported propagations differ (2,797,668,319 vs
3,028,091,302); scopes/currencies differ and counts alone do not establish
equal operations. The earlier Sweep19375 binary count remains213757/213761,
and the trace stops before the full solve. Do not claim whole-formula, full-run
trace, propagation-work or unit-cost identity from these observations.

Current full runtime329.772 seconds versus fixed274.456 motivates cost
diagnosis after the decision closure; another speculative scheduler change is
not selected for this Rank32 boundary.

## Neutral sampled Release pair

Campaign6372/job1677695 since2026-09-09T06:29:55 samples ordinary Release6357
and clean current Kissat at199Hz cycles:u with the established dwarf8192
protocol,12logicalCPUs/48000MiB, one bound solver per independent task,
1200solver seconds and16000checker seconds in a5-hour allocation. This one
native diagnostic does not refresh the fixed comparison vector.

`solver-summary-6378.json` validates both solver outcomes as soon as they are
published, without waiting for certificates. Each arm exactly replays its
unprofiled certificate bytes/hash and three work counters; candidate all
non-timing statistics also agree. Both profile reports have zero lost samples
and hash-bound compressed data. No final certificate was checked at this
frozen profile snapshot. This is observer/work neutrality, not a checked gate.

Candidate self samples include kernel_propagate34.38%, probing propagation
4.61%, heap_bubble_down4.39%, backtrack2.95%, rebuilding large watches2.26%,
garbage collection2.19%, and Vivify scheduling1.71% plus sorting descendants.
The profile's candidate inprocessing call tree is37.59%. Native functions
aggregate work differently: do not compare raw percentages as operation costs.

Source follow-up inspected the native/Tactix heap descent and found the same
hole-moving algorithm; no heap repair is selected just because it is sampled.
Vivify is a bounded engineering candidate: Tactix's comparator repeatedly loads
its first occurrence count through the literal-key pool. Native irredundant
`countref` caches that count. A private cached first-key experiment could avoid
those repeated loads while preserving every comparison result; field placement
may use existing record padding on the target ABI. This is a proposal, not an
implemented or proven speedup. Require actual old/new ordering/state/proof
equivalence, unchanged structure size, repeated/scope/mixed-priority/zero-count
fixtures and broad gates. CaDiCaL's lexicographic occurrence-order mechanism
was also inspected; no reference ordering or comparator semantics should change.

## Adverse control and repeated timing

Campaign6374 predeclares Rank32(primary),33(productive) and35(largest adverse)
from the complete50 view. Three independent six-task waves compare6357 with
6305; each arm must exactly reproduce its own proof and all non-timing stats.
The middle wave reverses arm order; no node constraint or throttle is used.

Wave0/job1677697 since06:32:33 has all6 expected outcomes and exact own-arm
oracles. Ratios are Rank32=.905987,33=.625260,35=1.518734. One wave is not
repeated acceptance. Wave1/job1677705 since06:42:23 is running; submit wave2
only after12 expected solver outcomes, not after their proof checks. Final
acceptance still needs18 checked/clean records and each arm's range/median<=2%.

Independent Rank35 campaign6376/job1677703 since06:34:51 compares current6357
with6305 through100K conflicts. Validator6379 verifies both full prefix/trace
hashes and cleanup. All40,636,523 selected decisions match, and both retain the
earlier Rephase common hash a6eb1df4f4e0e591 at18,516,218 decisions. Thus the
adverse change begins later than100K; the earlier Rephase witness is not broken.
Extend that current/previous prefix if choosing the adverse scheduling branch.
No new native Rank35 run was needed.

## Gates, ownership and next action

All new solvers keep the standard12CPU/48000MiB resource contract and independent
budgets. The matched41–50 reference6108 remains20/20checked; cpu-q5784,
production, submitted source trees/pins and fixed baselines are unchanged.
No new solver-source edit or fuzz operation occurred this increment. Existing
6357 tests remain106Release twice105trace,24fixed/12actual-native controls.
Maintained benchmark and watcher tests cover the reused protocols; no checker,
acceptance parser or observer implementation was changed.

An asynchronous approval follow-up for the previously proposed Rank50
subsumption-material tracker was sent. It remains unapproved unless explicitly
answered; general continuation is not permission. Current-source fuzz remains
Claude-owned/unavailable. No denied transfer or push retry, no manual scratch
deletion, and no source promotion. Pending checks allow development only.

Next: submit timing wave2 after wave1 solvers, revisit all checker outcomes,
then implement/test a source-backed cost increment such as the private Vivify
key cache, or the larger Rank50 tracker if approved. Continue scoring all50 and
memory, and retain the Rank35 adverse obligation. Do not substitute the Rank32
closure or local timing gains for the unmet aggregate goal.
