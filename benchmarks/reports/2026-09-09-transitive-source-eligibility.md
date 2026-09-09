# Rank 32: missed source is active and unfixed

## Decision and verified evidence

The selected-pair trace confirms that the missing source936 is present,
active and unfixed at the first observed entry. Investigate the full scheduling
inputs and order before changing source selection or budgets; Tactix's endpoint
observer does not independently snapshot the second invocation's eligibility.
This is a diagnostic result, not a runtime gain or a causal proof that one
binary explains the later search divergence.

Authority: `benchmarks/runs/20260909-6345-rank32-selected-transitive/summary.json`,
validated by6349. Job1677574 since2026-09-09T05:28:56 is COMPLETED.
Both unchanged6305/6090 pins reproduce every ordinary current6340 parent trace
and partial proof byte/hash through3M conflicts. The selected trace intentionally
replaces the all-source census; its entire112 Tactix and235 native rows pass
schema, source/candidate/deletion, endpoint, admission and walk framing.

At the first observed entry at conflict1368343, both endpoints936 and-30097
are active with value0 in both solvers. Their live reference-literal positions
agree:290 and13651. Native additionally confirms this at its second entry.
Tactix publishes only its two endpoint observations at this conflict: no
admission, source, candidate or walk. Native publishes endpoints for both
invocations, admits one physical copy, then visits seven walk nodes and
deletes `(936 OR -30097)` at candidate ordinal3 in14 ticks. Its nodes are
-936,-30087,-30090,-30094,-30747,-30748,-30750.
Endpoint emission frequency is observer-specific (once per conflict versus
every invocation), not missing state. The prior all-source census already
proves Tactix never processed936, including a call where the pair was absent.

The prior6340 census begins this conflict with differently ordered equal-key
source variables: Tactix338094 before338088, native the reverse, while their
compact literal references85874/85872 agree. This points to sorting inputs,
key identities or sorting mechanics as hypotheses; it does not prove which.
The actual source audit preserves native's IDX-on-variable comparator, including
keys that may belong to inactive variables. No priority producer was found in
the earlier pinned audit; do not invent a new flag as a speculative repair.
CaDiCaL's per-clause checked-bit and propagation-budget scheme is distinct and
does not justify changing the current Kissat-shaped source schedule.

## Tested read-only follow-up

Authority: `benchmarks/runs/20260909-6350-transitive-schedule-build/manifest.json`.
Fresh clones of6305 and6090 add before/after-sort snapshots of every source's
compact/external identity, activity/value, actual second-IDX key identity,
exact score bits, queue stamp and native priority flag. Tactix's priority
column is0 because its comparator has no such input. Both raw ascending
sort output and its later reverse consumption must be distinguished.
The adapters never sort, mutate state, alter guards/budgets or emit proofs.

All105 Release and104 trace tests pass. Ordinary Release remains byte-identical
to6305: `33a94aec8df9a3be4f2a902e13d59dcc1527a412302f646140bab2ec5c4193e4`.
Actual C/C++ writer fixtures cover disabled/malformed/overflow/conflict filters,
identical tie-key bits and repeated snapshot framing; outputs are byte-identical.
The source adapters still require whole-instance observer-neutral replay.
Five new selected-trace tests bring the benchmark suite to56 passing tests;
13 SLURM tests remain the watcher regression gate. No test failure this increment.

Run6352/job1677576 since2026-09-09T05:46:18 uses the fresh trace/native pins,
12 logical CPUs,48000MiB,1200 solver-only seconds and25-minute diagnostic
allocations, one bound solver per task, full width, no hard node constraint.
All6345 ordinary AND selected trace hashes and partial proofs must replay
exactly per arm; schedule.tsv is the only new stream. Then test the snapshot
parser, require before/after multiset preservation for every invocation, and
compare source membership, key state, sorted position and the reached prefix.

## Gates and scope

The prior full50 runtime remains1.187863x fixed Kissat; added41--50 remain
2.800258x. Replicated6305/6269 effects on32/33/40 are unchanged; no duplicate
timing or reference run was launched. Final checkers continue asynchronously;
the new audit owns current checked counts. A rejection pauses dependent work.
No production or frozen source/pin/runner was changed, and no manual scratch
deletion occurred. Completed6345 partial-proof/input cleanup is runner-verified,
not final-proof acceptance; live checker/6352 scratch is untouched.
Source-specific Claude-owned fuzz, memory convergence and promotion remain
open; no fuzz artifacts or denied transfers/push were accessed. Rank50's
new-material tracker and private FlatLists redesign still await approval.
