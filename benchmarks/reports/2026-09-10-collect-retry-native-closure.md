# Collection retry repair closes Rank35's observed native prefix

Isolated7030 matches **100,845,524 native decision rows through1M conflicts**.
This is the concrete causal result of the [collection-advice repair](2026-09-10-collect-retry-repair.md),
not a speedup claim. The current full50 reference remains unchanged at
8300.164/7239.010=1.146588277x,50/50 checked. Three paired runtime waves are
being used to decide whether the repair reduces the large31/35 gaps.

## Guard and diagnostic evidence

[7036](../runs/20260910-7034-collect-retry-BC-gate/summary-7036.json) validates
all66 expected B28+C5 solver outcomes, with64 checked/clean at sealing. The two
remaining proof checks do not prevent continued investigation. One diagnostic
wave sums948.979s current versus946.211s candidate; this is not decision-grade
timing acceptance. There is no solve loss or unexpected termination.

[7043](../runs/20260910-7034-collect-retry-BC-gate/work-classification-7043.json)
refines the21 differing non-timing-statistics pairs:20 differ only in the
removal-mark/epoch line;12 pairs are fully unchanged. Only B28 oddball changes
other work: BVE resolutions301030 to300654 and definition checks9095 to9090,
with fewer definition ticks. Its proof differs;32/33 certificates match exactly.
Reported CDCL search counts remain identical in all33 pairs. Thus these counter
differences must not be described as21 changed search trajectories.

[7038](../runs/20260910-7038-collect-retry-prefix/manifest.json),job1679734
since09:06:53EDT, uses the unchanged7030 trace pin and existing observers, with
a1M-conflict stop and decision streaming from100K. It finished cleanly and
deleted its529530880-byte partial proof and task scratch. The immutable native
7011 evidence is reused; no native baseline or diagnostic was rerun.

[7040](../runs/20260910-7038-collect-retry-prefix/summary-7040.json) validates
complete emitted traces and partial-proof metadata, then the inherited
40,636,523-row hash`6a3ebeb3a46d7ad5`. Its60,209,001 suffix rows equal native
byte-for-byte; full-prefix hash is`51999aa02944bbe8`. Against old6730, the first
changed choice is exactly the former native split at387254/D79046028,
level140: new-9706 versus old-24036. The preceding79,224,352 decisions remain
unchanged. This is changed-source causal evidence, not an observer-neutrality
claim, a final UNSAT certificate, or proof of full-run search equality.

[7045](../runs/20260910-7038-collect-retry-prefix/boundary-closure-7045.json)
reuses the frozen boundary reader. All eight projected Strengthen/BVE boundaries
at387253 now match native, including both BVE exits. It retains the earlier
Sweep-at19365 binary-copy mismatch204961/204963. Counts and matching decisions
do not prove formula identity. No additional heap instrumentation is needed
merely to reproduce this already-demonstrated prefix closure.

## Runtime decision in progress

[7041](../runs/20260910-7041-collect-retry-runtime/manifest.json) predeclares
three independent four-task waves on31 and35: tasks0–3,4–7,8–11. Wave0 is
job1679735 since09:10:07EDT. Each task owns an unchanged candidate or predecessor
pin, with12 logical CPUs,48000MiB,1200 solver-only seconds,16000 checker seconds
and5hours. Middle-wave arm order reverses; full width, no node constraint or
throttle. Existing campaigns, including cpu-q and fixed native vectors, are
hash-protected and unchanged.

7042 advances only after all preceding expected solver outcomes, exact own
work/certificate repetition, and no newly rejected B/C result. Previous arms
must also match frozen6730 records. Candidate work may differ from previous;
its wave0 is the repetition oracle for later waves. 7044 validates every wave,
reports all paired ratios and their median/range, plus complete within-arm
spread; an absolute runtime claim requires both ranges within2%. Pending proof
checks permit continuation but not checked acceptance or promotion.

The new own-repeat projection has fixed timing-exclusion, five changed-field
and three malformed-statistics controls. Existing prefix/suffix tests cover
unchanged observers/readers;7043 is a direct complete-line comparison preserving
all differences, not a new counter parser. No new solver code changed this
checkpoint;7030's40-state regression,113/113/112 suites and16 strong proof checks
remain the source gates. Required fuzz/consolidation and full50/memory impact
remain open. Preserve4/32/33/49/46, N1–N4, shelved21 and rejected5920. No
promotion, fuzz access, FlatLists redesign or denied push/transfer retry.

## First paired wave redirects attention to adverse Rank31 transfer

All four wave0 solvers return expected UNSAT; strict7044 validates each previous
arm's full non-timing statistics/certificate against frozen6730. These are
provisional single-wave observations, not accepted three-wave performance:

|Rank|Previous seconds|Candidate seconds|Previous conflicts|Candidate conflicts|
|---|---:|---:|---:|---:|
|31|483.891|532.868|9019812|10993050|
|35|510.679|359.576|8942669|4751990|

Rank35 also ends with210767815 decisions, exactly native's final conflicts and
decisions. Its128.236s slot and231.111s outside timers are same-run measurements,
not native unit-cost attribution. Rank31 makes236596608 decisions versus
225625259 previous. The pair total improves, but the regression remains a real
adverse effect that must not be hidden behind35's gain. Wave1/job1679740 since
09:19:31EDT proceeds while all first-wave proofs continue; wave2 is not yet run.

[7046](../runs/20260910-7046-collect-retry-rank31/manifest.json),job1679745 since
09:22:57EDT, is a separate two-arm complete0..300K Rank31 replay of unchanged7030
and6730 against cached native5735.7048 verifies full row accounting, every
emitted trace hash and clean partial-proof deletion, then compares all three
pairs and preceding boundaries. This distinguishes a new earlier
collection-induced split from known omitted preconnection behavior or a later
regression. No inherited native equality is assumed, and no native rerun is
needed. If both Tactix arms remain equal through300K, extend later; do not revive
5920 solely to close its already-known early native mismatch. Existing
full-prefix/boundary fixtures cover the unchanged runner/observers; no solver
code is changed by this probe.

7046 finished cleanly. [7048](../runs/20260910-7046-collect-retry-rank31/summary-7048.json)
validates96,317,845 identical candidate/previous decisions through300K, hash
`4b05ccd7dd7247d2`. Both retain the old native179823/D76834833/level68 split,
-177741 versus-173977. Thus the new adverse transfer begins later; this is not
a new justification to restore the old preconnection pass. New
[7050](../runs/20260910-7050-collect-retry-rank31-late/manifest.json),job1679752
since09:33:24EDT, extends both unchanged pins to1M, streaming from300K and
requiring each own complete cached-prefix hash plus every actual overlap row.
The cached Tactix stream does contain decisions at exactly300K; assuming native's
old zero-overlap case here would be incorrect.7052 checks actual overlap bytes
and complete suffix accounting before localizing the first later difference.

Wave1 reproduced each arm's own non-timing statistics and certificate exactly:
31 previous483.375/candidate530.512s;35 previous507.775/candidate359.521s.
All eight outcomes are expected UNSAT, still proof-pending at this checkpoint.
Final wave2/job1679748 since09:28:57EDT was running at that checkpoint.

## Completed three-wave decision and later Rank31 localization

[7044](../runs/20260910-7041-collect-retry-runtime/solver-checkpoint-7044.json)
now validates all12 expected UNSAT outcomes and exact per-arm work/certificate
repetition. Rank31's paired median is1.098354313x (range1.097516421–1.101214943);
Rank35's is0.708032101x (range0.704113543–0.711170704). Every arm's complete
runtime spread is below1%. The gain35/regression31 tradeoff is reproducible,
not a best-wave selection. All12 proofs were pending at7044 sealing; the
successor audit records published checker progress. The repair is not promoted.

[7052](../runs/20260910-7050-collect-retry-rank31-late/summary-7052.json)
validates both clean1M stops, exact inherited300K hashes, all three actual
threshold-overlap rows and complete suffix accounting. All123,392,789 decisions
from startup match between7030 and6730, hash`55ff551e76a16a32`. Both complete
macro and inprocessing boundary files are also byte-identical. The changed
full-run Rank31 work therefore originates later, not at the old native179823
mismatch. Matching boundary files still do not prove complete hidden-state
identity or explain the later regression.

[7055](../runs/20260910-7055-collect-retry-rank31-2m/manifest.json),job1679779
since09:46:39EDT, extends the unchanged paired trace to2M conflicts, streaming
from1M.7057 reuses the validated suffix reader with the actual threshold overlap
and the full inherited1M hash. No source, observer, budget or policy change.
It targets a substantial adverse transfer, not arbitrary native trace identity.

[7053](../runs/20260910-7053-collect-retry-ranks1-50/manifest.json),job1679755
since09:45:54EDT, measures the candidate alone over all50 approved ranks against
the unchanged fixed Kissat vector and6730 full-set results. Full width12CPU,
48000MiB,1200 solver-only seconds,16000 checker seconds,5hours, no node
constraint or reference rerun. Its preflight requires all66 B/C and12 timing
outcomes without rejection, and binds the three-wave tradeoff explicitly.
Classify every invalidity/solve loss before scoring; report per-rank time and
memory, including regressions. This is measurement only: source fuzz remains
unavailable and memory/consolidation gates remain open. Existing benchmark
273-test and SLURM22-test suites pass; unchanged runner/readers cover these
configuration-only extensions. No new solver unit-test behavior is introduced.
