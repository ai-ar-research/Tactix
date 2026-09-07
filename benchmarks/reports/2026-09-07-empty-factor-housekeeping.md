# Empty Factor housekeeping and the next focused-queue frontier

## Decision

Retain an isolated, general Factor scheduling repair and its differential
regression, but **do not promote the cumulative candidate**. It is UNFUZZED and
does not improve the checked three-witness runtime screen. Production stays at
`c2a95a6`, Release SHA-256
`de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f`.
The Rank 1–40 goal remains unmet: production campaign 5314 measured 1.2787518x
Kissat, against the required ≤1.01x. Memory is independently open.

This closes the producer left open by
[the BVE choice report](2026-09-07-bve-choice-frontier.md). It does not revive
the invalid mixed-proof-mode attribution rejected by
[the runtime review](2026-09-07-runtime-review.md).

## Causal evidence

Campaigns 5352–5355 use Rank 34, immutable proof-on stopped observers, 12 logical
CPUs and 48000 MiB per task, full-width arrays, and one bound solver per task.
Jobs 1668720/22/24/26 all stopped with exit zero and deleted their scratch.
These are diagnostic traces, not complete verdicts or runtime measurements.
The assertion-bearing
[comparator](../probes/scratch/compare-factor-stamp-checkpoint-5355.py)
reproduces the retained summaries.

1. [5352 queue snapshots](../runs/20260907-5352-queue-stamps/summary.json)
   preserve the exact 5351 BVE schedule. Both solvers compact at startup,
   conflict 1000 and 53839. Initial stamp gaps are overwritten by productive
   Factor; startup compaction alone is not the later cause.
2. [5353 Factor boundaries](../runs/20260907-5353-factor-stamps/summary.json)
   preserve previous observations and expose 15 executed Factor passes, eight
   with no fresh variable. Tactix skips housekeeping on all eight; Kissat
   densely renumbers the queue and resets its search cursor every time.
   At the first empty pass, conflict 7705, Tactix's queue stamp stays 707952;
   Kissat's changes from 707951 to 14381. A second empty pass further reduces
   the native stamp to 14378. At 79951 native again restamps while Tactix does not.
3. [5354 repaired schedule](../runs/20260907-5354-empty-factor/summary.json)
   matches all 13003 candidate identities, positions and positive/negative
   occurrence counts. Every numeric score still differs by uniform −1.0:
   Tactix retains an extra queue member, external 22152. Do not claim full
   numeric-state parity or remove that identity without tracing its lifecycle.
4. [5355 decision/BVE comparison](../runs/20260907-5355-empty-factor-prefix/summary.json)
   verifies full-stream FNV digests and an unchanged native stream. All 15386
   BVE candidates at conflict 95799 match, including outcomes and logical ticks
   (30772 symmetric begin/end records). The common decision prefix grows from
   254416 to **328665**. The next first ordinary mismatch is conflict 133243,
   ordinary decision 327447, level 37: Tactix 21863 versus Kissat 21954.

## Reference review and minimal repair

Kissat `factor.c::adjust_scores_and_phases_of_fresh_variables` unconditionally
restamps the queue and resets its cursor, including an empty fresh stack.
CaDiCaL `factor.cpp::adjust_scores_and_phases_of_fresh_variables` explicitly
returns for an empty stack. Kissat owns this tie-break: its focused BVE score
consumes numeric queue relevance. Copying only CaDiCaL's empty-case shortcut
does not preserve that coupled mechanism.

The four-file [isolated patch](../runs/20260907-5354-empty-factor/empty-factor.patch)
always publishes the existing fresh-variable interval advice at the end of an
executed Factor pass. The heuristic consumer admits an empty interval and runs
its existing restamp/reset code. Fresh-variable demotion counters retain their
old meaning; zero demotions no longer imply zero heuristic effect. The typed
disabled arm still discards the advice without changing heuristic state.

The rule does not directly mutate heuristic state. No guard, formula effect,
proof delta, reconstruction obligation, budget, public action surface or new
kernel primitive changes. Calculus E2 is unchanged; implementation notes and
`SP-FACTOR-QUALITY-1` record the remaining production deviation. The patch is
general: no rank, formula, conflict, literal or variable identity condition.

[The fixed differential fixture](../runs/20260907-5354-empty-factor/fixture.json)
calls the actual native helper with an empty stack and runs a genuine Tactix
NoOp Factor followed by policy consumption. Native restamps 4→3 and passes;
old Tactix leaves 7→7 and fails the expected postcondition; repaired Tactix
restamps 7→3 and passes. A focused CTest also checks deferred consumption,
cursor reset, disabled-arm behavior and unchanged counters.

Harness correction: the initial fixture compilation omitted the trace build's
feature defines, producing an ABI-incompatible Kernel layout. That invocation
is invalid solver evidence. The retained replay imports the linked build's exact
defines/includes and records its commands and input/object hashes. Native,
old-negative and candidate-positive outcomes above use the corrected harness.

## Checked runtime screen

[5356](../runs/20260907-5356-empty-factor-runtime/summary.json), job 1668728,
finished with **12/12 independently checked SAT models**, no timeout and all
certificate/input scratch cleaned. Same-window, instance-major, four proof-on
Release arms use the same resources and a 1200-second solver limit.

| Rank | Production | Previous cumulative | + empty Factor | Kissat |
|---|---:|---:|---:|---:|
| 17 | 249.974 | 202.718 | 201.439 | 113.700 |
| 21 | 111.947 | 135.141 | 135.134 | 96.618 |
| 34 | 563.341 | 683.870 | 695.455 | 330.646 |
| Total seconds / PAR-2 sum | 925.262 | 1021.729 | 1032.028 | 540.964 |

The candidate is 1.11539x production, 1.01008x the preceding cumulative arm,
and 1.90776x Kissat on this subset. A single wave cannot distinguish the small
candidate/previous timing difference from measurement variation. The screen
does establish **no demonstrated runtime win**; greater trajectory fidelity
is not sufficient evidence for promotion. Maximum observed RSS is 4713520 KiB
for the candidate versus 1436392 KiB for Kissat; no memory acceptance follows.
Candidate Release SHA-256 is
`64a6bf16b86d249c3555e23c08689f51b2661ee4f3e88c625ed0492e019d5d89`.

The cumulative B28+C5 campaign 5357, job 1668740, uses the exact frozen 5315
membership and separately built B/C candidates with identical binary hashes.
At this checkpoint 64/66 final records are checked/clean. Tasks 64/65 have
returned UNSAT for T105.2.0 and are still checking proofs; no aggregate gate
result is declared. The predecessor's checks took about 55 minutes, so this
wait alone is not a failure. Continue the exact job and run
`seal-empty-factor-gates-5357.py` after both final records arrive.

## Next first difference

[5358](../runs/20260907-5358-next-choice/summary.json), job 1668806, finds that
21954 is unassigned in both solvers before the new split, with identical heap
position 9582 and EVSIDS score 3.9852014013666375e-15. This does not explain a
focused-mode decision.

[5359](../runs/20260907-5359-focused-choice/summary.json), job 1668808, appends
read-only queue fields. All previous state columns and decision/prefix bytes
are unchanged. Both are focused. At ordinary decision 327446 both select 21867;
Kissat places 21954 immediately before it (stamp 43793, neighbors 21863/21867).
Tactix places 21954 earlier (stamp 43758, neighbors 21281/21957). The subsequent
choice follows that existing queue difference. Trace the first unequal bump,
publication or reordering of this queue neighborhood, including the preceding
inprocessing phase at 132816. Do not modify the choice operation to compensate.

## Validation and preservation

The trace candidate passed 76/76 CTests. At this checkpoint, production passes
77/77 in 4.87 seconds and the independent Release candidate passes 77/77 in
3.46 seconds; [validation record](../runs/20260907-5359-focused-choice/validation.json)
and complete logs retain exact hashes. Pins are not rebuilt after submission.
These tests and checked gates are not a substitute for required Claude-owned
consolidation fuzz. No fuzz campaign or valid solver correctness failure was
handled in this increment; production is not changed.

The scoped artifact audit preserves unique manifests, patches, runners,
comparators, summaries, logs and small witnesses for 5352–5359. Large streams
and executable pins remain local with hashes. No manual deletion is warranted.
Living docs remain within their 30 configured budgets; the next queue producer
is handed off in PROGRESS rather than added as campaign chronology.
