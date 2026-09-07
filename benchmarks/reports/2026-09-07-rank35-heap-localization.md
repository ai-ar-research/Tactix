# Rank-35 search-state localization

## Decision and outcome

Continue the Rank 1–40 ≤1.01x Kissat objective from `92aa5b07` by identifying
the largest remaining work gaps, not by extending the earlier unsupported
percentage attribution. The [runtime review](2026-09-07-runtime-review.md)
owns the audit of Claude's earlier work and retained/rejected experiments.
The [dense-entry report](2026-09-07-dense-entry-cost.md) owns the latest
isolated semantic candidate and its mixed runtime/memory evidence.

Production is unchanged at `c2a95a6`, Release SHA-256
`de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f`.
Its complete checked Rank 1–40 ratio remains **1.2787518x**. The objective is
not met. This increment changes only isolated diagnostic observers and their
runners; it does not promote a solver repair or claim a new speedup.

Rank 35 now has an exact first decision split, preceded by identical heap
membership, scores and assignment state but different physical heap order.
That redirects investigation toward heap lifecycle producers rather than a
different score formula or an already-assigned branch variable. It does not
yet establish which producer is wrong or price the downstream effect.

## Why these witnesses

The checked pairs already complete in 5383 identify substantial excess on
Ranks 27, 35 and 32. These are per-instance facts, not a partial aggregate:

| Rank | Tactix seconds | Kissat seconds | Tactix conflicts | Kissat conflicts |
|---|---:|---:|---:|---:|
| 27 | 499.203 | 244.873 | 7,296,232 | 4,120,691 |
| 35 | 558.714 | 306.599 | 8,681,274 | 4,751,990 |
| 32 | 462.905 | 274.225 | 7,378,565 | 4,636,494 |

Both larger work and representation cost remain plausible contributors.
Different inprocessing timer scopes prevent subtraction into a clean cost
decomposition. The full view, including slow outstanding proof checks, remains
necessary before accepting the cumulative candidate. The later coupled 5395
view is a distinct source/window; its unfinished observations are not scored.

## Prefix evidence

Every real-instance diagnostic ran through an immutable full-width SLURM
array: one proof-on solver per task, 12 logical CPUs, 48,000 MiB, one bound
thread, no throttle, node constraint, exclusivity or address-space limit.
Intentional conflict stops do not produce a complete certificate or verdict.
Partial certificates are hashed and deleted by the task; no stopped timing
is production-cost evidence. Manifests and runners own exact pins and input
hashes. Legacy wrapped `start_decisions` is never an alignment key.

| Campaign | Job | Scope and verified result |
|---|---|---|
| 5396 | 1669333 | Rank 27 through 25,000 conflicts: all 7,154,988 decision rows match among preceding Vivify, coupled dense-entry and native Kissat pins. |
| 5397 | 1669336 | Same three arms through 100,000: all 28,605,461 decision rows match. Normalized post-startup coarse boundaries also match. |
| 5398 | 1669338 | Rank 35 coupled candidate/native through 100,000: first split after 18,516,149 matching decision rows. |
| 5400 | 1669345 | Rank-35 focused choice state; independently validates the new stream filter against retained 5398 bytes and cumulative hashes. |
| 5403 | 1669443 | Direct post-decision heap snapshot; both complete stopped traces reproduce 5400 exactly. |
| 5404 | 1669485 | Six stable-mode heap checkpoints, 12 clean stops; all cumulative prefixes reproduce 5398. |
| 5405 | 1669514 | Three narrower checkpoints plus retirement order; six clean stops, all prefix hashes match. |

5396's original comparator zipped different startup phase inventories and
reported a startup boundary difference. Excluding conflict-zero startup and
comparing common phase names gives 116/116 matching post-startup rows. This is
a stated normalization, not a repaired frozen result or evidence of startup
semantic identity. 5397's comparator applies that normalization explicitly.

At Rank 35's first split both reach conflict **13,832**, ordinary decision
counter **18,445,057**, level **11,292**. Tactix chooses **−390547**; Kissat
chooses **−483970**. The first differing coarse inprocessing entry is later:
Strengthen at conflict 15,344. Thus attributing the split to that entry would
mistake a downstream symptom for its owner.

5400 compares the exact 13,800–13,833 conflict window with the corresponding
5398 bytes: 83,886 candidate and 83,885 native rows. Independent full-stream
recalculation agrees with all cumulative counts/hashes, including common hash
`699aacf2488e36b4` at the 18,516,149-row shared prefix. Boundary prefixes also
match their own earlier arms. At the first choice-state row (decision
18,441,404), variable 483970 is unassigned in both, score
`9.1349071080092326e-130`, but at heap position 168814 versus 168813.

## Exact heap witness

5403 snapshots both heaps **after decision 18,441,404 at conflict 13,832**.
It validates pin/trace hashes, clean stops and scratch deletion, and exact
decision-window, cumulative-prefix, boundary and choice bytes against 5400.
Both contain **349,376 unique variables**, with **zero membership, score or
assignment differences**. Both satisfy the numeric max-heap invariant.
The active column has different retired/fixed representation definitions and
is deliberately not used as a semantic equality predicate.

| Variable | Tactix position | Kissat position | Score in both |
|---|---:|---:|---:|
| 483970 | 168814 | 168813 | 9.1349071080092326e-130 |
| 390547 | 168813 | 337630 | 9.1349071080092326e-130 |

The first positional difference is index 10 (463573 versus 390930). Matching
sets and scores do not imply matching tie behavior. Source inspection finds
the same strict comparison directions in both heap insertion/removal paths;
the next question is the earlier order of lifecycle operations. Compaction,
root retirement, focused-to-stable reinsertion and phase-transition work are
leads, not established defects. No heap algorithm is changed on that basis.

5404 brackets the discrepancy much more tightly. At selected conflicts 3,006,
3,072, 8,001, 10,553 and **11,838**, both complete heap arrays agree in portable
identity, score and assignment. At **13,565**, membership, scores and assignments
still agree, but 197 positions differ. Both heaps satisfy the invariant at all
six checkpoints. The ordinary decision prefixes remain identical throughout.
Rephase at 13,563 is inside the unresolved interval, not yet a demonstrated
cause; direct retirement and earlier post-decision checkpoints are the next
discriminating evidence. Startup and the 9,457 Probe interval cannot simply be
blamed for a discrepancy absent at the later observed common heap.

5405 finds complete heap identity again at conflicts **12,501** and
**13,001**. At the first ordinary post-Rephase decision, conflict **13,563**,
decision **17,798,664**, 75 heap positions differ; membership, scores and
assignments still agree. The requested 13,562 checkpoint advances to 13,563
because there is no ordinary decision at 13,562. It is not a pre-Rephase
snapshot. Both heaps satisfy the invariant at every checkpoint.

All **42 portable retirement rows** from 11,838 through 13,563 match after
excluding Tactix's representation-local trail offset. Exactly one follows the
last observed common heap: variable **109797**, root-fixed while in the heap,
at conflict **13,414**. This supplies the next bounded probe: compare that
retirement relative to heap pops, score bumps and backtrack reinsertion, then
separate pre-Rephase, warm-up and post-Rephase states. Matching retirement
rows alone does not establish their ordering relative to other operations.
No cause or repair is inferred solely from this correlation.

## Observer validity and operational limits

5399 isolates `SAT_DECISION_STREAM_CONFLICT_FROM`, default zero, in both
trace-only sources. It filters written decision rows without filtering the
cumulative prefix hash. All 76 fixed trace CTests pass (21.22 s); 5400 then
proves exact retained-byte equivalence at the relevant real-instance boundary.
An initial sealer expected a different successful CTest summary phrase; its
predicate was corrected before freezing. Native configure was reset to the
original `-O3 -DNDEBUG` before compilation. Neither was a solver failure.

5401 (job 1669397) attempted the existing broad event observer. It was too
expensive for this question and was deliberately cancelled, not scored.
Its inherited runner lacked a cancellation signal handler and left two
node-local scratch directories. Exact node, creation window, file inventory,
input hash and absence of live file users were verified before deleting four
files and the empty directories: **86,205,224 bytes**. The retained
`5401/cancellation.json` owns the exact paths and recovery statement.

5402 adds only a direct call from the post-decision observer to the existing
one-shot heap snapshot. It avoids enabling broad event payloads. Initial
Tactix compilation found an ambiguous forward declaration in the wrong
namespace; that declaration was corrected before execution. Final 76/76
fixed CTests pass (6.29 s), and 5403 proves unchanged measured trajectory.
No reproduced soundness/crash/unit-test failure occurred in this increment;
no Claude fuzz campaign was launched or inspected. New runners print
job-specific scratch paths and handle SIGTERM/SIGINT through cleanup.

## Completed regression gate and retained limits

5389, job 1669095, is now terminal with all **66 checked/clean results**, no
timeouts and no solve losses. Both arms solve B28 (14 SAT/14 UNSAT) and C5
(3 SAT/2 UNSAT). Candidate/production time is 359.626/409.752 = **0.877667x**
on B, 702.278/707.668 = **0.992383x** on C, combined **0.950318x**.
This cumulative publication-only candidate remains unfuzzed and unpromoted;
the gate does not establish that publication alone caused the total gain.
The isolated publication witness comparison remains neutral at 0.997513x.

Frozen source/patch/log authorities remain in campaigns 5396–5404. Huge full
decision streams and heap images remain local with hashes; small launchers,
comparators, source patches and endpoint evidence are retained durably.
The current artifact index records live jobs, pin identity, dependencies,
residue and retention without inspecting Claude-owned fuzz material.

Living-document compaction uses the explicit user request. Stable requirements,
N1–N4, pure rule semantics and gate definitions are unchanged. Historical
details remain in their existing reports and Git; PROGRESS remains only a
resume surface. All 30 length budgets pass, with four near-cap warnings.

Final production fixed validation passes **77/77 CTests (5.07 s)**. The
rule/policy-boundary checker passes; all 418 reference options are accounted
for and all 42 section references resolve. The progress checker's hardware-L3
false positives and old-tier freshness warning are pre-existing and do not
change benchmark authority. The new Andrew TeX/PDF and validation hashes are
retained separately; previous delivered reports are unchanged.
