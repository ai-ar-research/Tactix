# Rank35 branch-state replay

## Question and retained constraints

Rank35 remains a major UNSAT loss: current6579 takes506.019 seconds against
fixed Kissat306.699, with8942669 versus4751990 conflicts in the complete6596
view. This checkpoint diagnoses the late divergence from earlier Tactix6305;
it does not substitute that solver for the fixed native denominator.

The compact-stamp repair6357 must remain: its actual/native fixtures and full
Rank32 closure establish a real ordering-key defect. Rank35's repeated adverse
effect does not justify dropping that repair or adding benchmark-specific
conditions. Later activation repair6424 and material scheduling6579 remain
frozen. Current and6424 full Rank35 work/statistics/certificate match exactly;
the new replay separately tests whether the earlier diagnostic applies now.

Cached6467 locates the first6424/6305 decision split at3074183, decision161631566,
level1122, literals-482577/-486307, after161899386 common rows from the start.
The preceding Reorder at3074062 is in stable mode. Neutral6483 replays all own
ordinary traces and partial proofs through4.4M; its selected Vivify candidates,
physical fields and cutoffs match. The extra52 ticks are not a demonstrated
cause of the branch split.

## Experiment and validation

Campaign6619/job1678417, submitted2026-09-09T17:27:06 EDT, has two independent
tasks: frozen current6579 and earlier6305. It reuses6467's bounded runner and
adds existing standalone observers, without a broad event tracer or source
edit. Capture the full stable score heap after decision161631565, before its
subsequent propagation, and variable486307's assignment/reason state at every
decision at3074183. The complete branch queue is unnecessary in this verified
stable window; existing choice rows retain queue context but it is not the
selection mechanism here.

Validator6621 first binds every own ordinary trace and partial-proof byte/hash
to6467 through4.4M. A changed current prefix is a failed applicability premise,
not a solver crash; it must be localized before interpreting the new snapshots.
Then validate the entire selected choice window against the complete decision
stream and compare all heap entries by identity, score, assignment, activity
and position. The heap schema has no declared row count, so framing alone cannot
detect removal of a complete final row; the actual closed observer output is
also bound to the runner's hash. No final-proof acceptance or formula equivalence
is inferred from this diagnostic.

Authoritative result: `benchmarks/runs/20260909-6619-material-rank35-choice/summary-6621.json`.

Both tasks completed cleanly, and every per-arm ordinary trace and partial
proof matches6467. This confirms current applicability as well as observer
neutrality. All176 selected choice rows per arm match the complete decision
stream. Both heaps contain49690 identical members with identical assignment
and activity flags. One score differs: variable1373 is4.92578125 current versus
5.92578125 earlier. There are2407 displaced positions. Variables482577 and486307
are both active, unassigned, score43; their positions1/4 are exchanged.

At the last common decision121349, wanted486307 is unassigned in both arms;
at the split it remains unassigned in current and is explicitly the earlier
arm's decision, not an implication. This rules out assigning the split to a
forced wanted variable. The incoming topology already differs before the last
common decision's propagation. It does not establish the first heap mutation
or that the single score residual causes all position differences.

New6624/job1678419, submitted17:44:00 EDT, reuses both6619 pins and adds the existing standalone score history
for1373 from conflict0 through4.4M. First require all6619 ordinary, heap and
choice traces plus partial proof bytes to replay exactly. Then compare aligned
macro histories, separating score, position, value and activity; preserve
repeated labels and unmatched sites. Locate the earliest score difference and
inspect Reorder3074062 specifically. Source inspection confirms weighted live
clause counts enter Reorder, so the exact1.0 residual could reflect input work;
do not call it a scoring arithmetic defect, restore binaries or change ties
without a causal witness. The detailed lifecycle remains the next action.

## Tests, gates and handoff

New maintained branch-state readers have seven tests covering empty/complete
heap framing, duplicate membership, position order, finite scores and flags,
separate comparison categories, selected choice identity/window, contiguous
counters, assignment/reason consistency and missing/truncated/extra rows.
All167 benchmark-tool tests and13 watcher tests pass. No solver or proof-rule
implementation changed, so this diagnostic does not require duplicate fuzzing;
existing current-source fuzz remains Claude-owned/unavailable.

Full6594 remains50 expected outcomes,8401.066/7239.010=1.160526923. At the
pre-report read48/50 are independently checked/clean; two UNSAT checks remain
asynchronous. Audit6622 owns later exact statuses. B/C6584 remains66/66 checked,
and all six Rank38 and all six Rank50 timing certificates are checked. Do not
rerun any completed gate or timing denominator. No aggregate gain is claimed.

Preserve Rank31's known omitted preconnection witness and rejected5920 adverse
gate, Rank4/32 closures, stronger Rank33 closure6519, N1--N4 and SP-SLOT-5.
No FlatLists implementation, fuzz access, denied transfer/push retry, promotion,
manual deletion or active-campaign mutation. Standard diagnostics retain
12 logical CPUs/48000MiB,1200 solver seconds,25-minute allocation, no hard node
or throttle. Tasks hash/delete partial certificates and their own scratch before
publishing; live checker scratch is not inspected. Docs retain fixed caps.
Andrew's new local TeX/PDF follows generate-report and is not sent.
