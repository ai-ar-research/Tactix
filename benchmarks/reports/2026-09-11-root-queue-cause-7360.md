# Root-storage regression: numeric queue stamps change BVE priorities

Frozen September 11, 2026. This advances7355 without changing production,
solver source, submitted campaigns, acceptance gates or the fixed denominator.

## Decision

Do not reject an improvement because it is small. Preserve the independently
useful collector correction and the clause-arena memory/runtime tradeoff.
The latter still has no demonstrated runtime gain: the fully checked fifty
take8124.477s, versus8096.416s in the preceding full view; all five targeted
three-wave medians are adverse. This is1.122319x fixed Kissat7239.010s, not the
1.01x goal. A fresh native denominator is neither needed nor substituted.

The root-storage prototype's short B14 regression is changed search work,
not an isolated unit-cost slowdown:312090 to768682 conflicts and14.791 to26.413s.
Isolation7348 already excludes the collector correction as its cause. B14 here
means Tier-B FmlaEquivChain, not Tier-D Rank14. Memory saving on the actual
Rank25 instance remains unmeasured for this prototype.

## New measured mechanism

7356/job1681401 adds only the existing selected BVE heap observer to the frozen
7302/7340 Trace binaries. Both complete solves reproduce their own Release
statistics and certificates. All four entire7353 compressed trace archives
replay exactly. Both original-formula proofs are checked and residue-clean.
Analysis7357 validates every selected-subject row:42 parent and54 candidate.

At BVE entry C26295, external22861 has21 positive and15 negative physical
occurrences in both arms, and the same initial heap position36789. Its negated
heap score is0x1.e0c35p+21 parent versus0x1.e0c438p+21 candidate, a difference
of+29. Focused BVE uses the queue stamp additively with the occurrence score;
therefore the candidate's relevance is29 lower. The parent eventually stops
with this subject still at position3; the candidate pops it. This observer
reports changes to the selected subject, not every heap pop, so row ordinals
must not be mistaken for aligned solver events.

7358/job1681403 then adds only the existing full queue snapshot observer.
Analysis7359 validates the complete five7356 archives before reading the new
queue archive. Both solves again reproduce their own Release work/certificates.
7359 was written while the candidate proof was pending; both proofs subsequently
checked clean. That frozen pending snapshot is not rewritten; audit7360 records
the terminal state. No proof checking blocked the analysis.

The first thirteen complete queue snapshots agree. Snapshot14, Reduce compaction
at C14492/D1037401, is the first difference. The incoming snapshot is exact,
including all41753 ordered members and their numeric stamps. Parent keeps41753
members; candidate retains41724, deleting exactly29 with no additions. Every
remaining member stays in exactly the same relative order. Restamping gives:

| Retained population | Candidate minus parent stamp |
|---:|---:|
|17439|0|
|24285|-29|

External22861 changes from the same incoming74944 to40384 parent/40355 candidate.
Thus compaction changes the spacing of numeric priorities, not merely a common
offset. Later focused decisions can initially remain identical because their
relative queue order is unchanged, while BVE's mixed relevance/occurrence score
changes. The earlier7349 result of1343955 identical choices before the restart
split is compatible with this mechanism; restarting is downstream, not the
place to patch the discrepancy.

The29 deleted identities are explicitly retained in7359. They form the queued
block1333--1367 with gaps. The queue observer does not report assignment values.
Source inspection supplies a narrower inference: the only extra deletions in
the root-fold arm are unreferenced Active fixed roots in the root trail prefix.
Identical incoming queues and identical retirement filtering therefore point
to stale fixed members, but the first missed deactivation is not yet observed.
Do not invent its originating rule or preserve stale entries merely to recover
the parent's faster trajectory.

## Reference contract and next repair decision

Tactix `heuristics_compact_vars` filters through the physical map and restamps
the retained linked queue. `BveCandidateHeap::schedule_score` uses focused
`links[v].stamp` in the same additive formula as Kissat's `eliminate.c`.
Kissat's `inlineassign.h` marks root assignments fixed and `flags.c` deactivates
them; `compact.c::compact_queue` restamps its remaining members. CaDiCaL's
`compact.cpp` explicitly removes inactive queue members while remapping.
These sources support auditing the fixed-variable lifecycle, not treating the
parent's stale queue as the semantic oracle.

Next: identify when those29 variables become fixed and which DefaultPolicy
boundary fails to consume that transition. Add an actual-consumer unit test
covering root assignment/deactivation, compaction and BVE numeric relevance;
retain both folded and legacy storage arms. If a lifecycle repair is supported,
repair at its owner, preserve the public rule surface and proof/model paths,
then rerun the short gate before Rank25/31 timing waves or another full50.
No scheduling constant, threshold, restart policy or source was changed here.

## Bookkeeping and Rank25 answer remain unchanged

Audit7185 found optimized heavy trace/journal/shadow/stop OFF, direct CDCL
untimed, but optional counters, rule clocks and dormant guards still present.
Their joint ablation did not consistently help. This is not a claim of zero
intrinsic overhead. Diagnostic7356/7358 use Trace builds only and contribute
no runtime claim to the optimized score.

Rank25's ordinary224.225s is below native246.184s despite2484880 versus888820KiB
peak RSS. Longer runtime is therefore not the explanation in that comparison.
Snapshot7328 finds3015698 fixed Active slots among3586539 internal variables;
eight selected arrays occupy294.15MiB. Reconstruction allocation overlap and
the dense occurrence arena remain larger storage leads. The snapshot is not a
complete or observer-neutral decomposition of the1.52GiB excess.

## Validation and handoff

The two new parsers include17 negative schema controls, exact ordered identity
checks, hexadecimal score retention and absent-member handling. Existing replay
and independent proof checks are unchanged. Benchmark377 and SLURM22 unit tests
pass; documentation checks and immutable source/hash bindings are in audit7360.
No new solver code means no claim of a newly validated implementation.

Full7312 is50/50 checked-clean. BC7347 is65/66 checked at this checkpoint; only
task64 remains checking. Full Debug7342 still runs its final demo after122 prior
passes; it is not declared complete. Preserve sessions72197 and59078. Current
source fuzz/consolidation and synchronization remain open under the previously
recorded denied access/transfer/push boundary; no retry or bypass was attempted.
No benchmark files were deleted in this increment. Completed trace archives,
all successful writers and all previous evidence remain frozen.

Andrew's new local, unsent report is
`report-to-andrew/20260911-1102-root-queue/report.pdf`. Productionc2a95a6 is
untouched. Required documentation line caps remain unchanged and respected.
