# Complete Sweep-stack view and causal witnesses

## Full Rank 1–40 result

The frozen [5485 solver snapshot](../runs/20260908-5485-sweep-cursor-ranks1-40/solver-checkpoint.json)
has all 80 solver records: Tactix 7859.744 s / Kissat 6744.611 s =
**1.165336889x**, with 40 expected solves each (22 SAT, 18 UNSAT), no timeout or
verdict loss, and 59/80 certificates independently checked/clean at snapshot.
Pending checks guide investigation provisionally; they are not final acceptance.
The ≤1.01 objective, B28+C5 closure, latest-source fuzz, memory and N4 gates remain.

The [remaining-loss analysis](../runs/20260908-5485-sweep-cursor-ranks1-40/next-losses.json)
validates every snapshot record hash. All 40 Tactix runs preserve complete
non-timing statistics and certificate bytes from bound5459; native controls
preserve their three extracted work counters and certificate bytes. Thus no
unexpected work change was introduced by watch append5466/Sweep cursor5473.
Different campaign windows prevent attributing the aggregate ratio change
from 1.173817x to those edits; their separate exact-work replica gains remain
the causal unit-cost evidence.

| Rank | Tactix / Kissat seconds | Excess seconds | Conflicts ratio | Propagations ratio |
|---|---:|---:|---:|---:|
| 37 | 694.600 / 341.998 | 352.602 | 2.114745 | 1.265941 |
| 31 | 409.078 / 262.335 | 146.743 | 1.542264 | 1.164893 |
| 32 | 408.971 / 273.054 | 135.917 | 1.367353 | 1.263398 |
| 27 | 351.061 / 243.959 | 107.102 | 1.239047 | 1.107322 |
| 39 | 432.945 / 361.686 | 71.259 | 1.115283 | 0.926696 |

These are mixed work/cost losses, not interchangeable per-unit benchmarks.
Rank39 does fewer propagations/decisions yet runs longer; Rank37 does much
more search work. Ranks37/31/32 retain roughly twice native peak RSS, which
does not itself identify live-memory ownership. General causal repairs remain
the priority; no ranking-dependent policy, new budget constant or source promotion.

## Rank32: retained seed witness

[5490 observer validation](../runs/20260908-5490-rank32-sweep/observer-validation.json)
checks every observed decision against its longer5479 parent: 18,593,185 Tactix
rows and 18,593,182 native rows, stopping at19376. First divergence remains
19375/18523001 after18,583,703 common rows. Both partial proofs were hashed and
scratch deleted; this stopped diagnostic is not a complete checked solve.

[Call-localized evidence](../runs/20260908-5490-rank32-sweep/sweep-localization.json)
separates the two Sweep calls at19375. In the first, the first14 seed identities,
environment sizes, effort deltas and solve counts match. Tactix then processes836
(72 variables/115 clauses,1914 ticks/11 solves) while native advances to1236
(256/427,7155 ticks/11 solves). First-call totals are11020/11023 seed events and
six/seven unit events; native additionally derives -103918 later. That later
unit is not yet causally assigned to the skip. Second-call sequences already
start differently, so do not align all events merely by conflict number.

Source: native `sweep.c::reschedule_previously_remaining` rechecks both-polarity
occurrences/caps, then adds fresh eligible seeds; Tactix's resumable path retains
its existing list without that refresh. This is a hypothesis, not a proved cause.
New [5505](../runs/20260908-5505-rank32-sweep-eligibility/manifest.json), job1672156,
captures836/1236 binary/large clauses at first entry and preceding829 queries/vars.
Require whole per-arm decision/prefix/boundary replay against5490 before use.
Live-clause counts alone do not prove physical watch eligibility or active identity.

Original comparator5490 failed its schedule/window expectation: Tactix does
not enumerate a fresh schedule while a tail remains. Native `sweep.c` emits
schedule/seed/environment/effort directly without detail-window filtering;
431046 schedule rows and2814 rows of each other family precede19375.
Successor5503 preserves and separately counts only those source-backed earlier
native families; ten positive/negative fixtures reject unowned or future rows.
Both complete parent streams still replay. This is not a solver failure.

## Square: differing auxiliary definitions, not phase priority

5487 passes complete per-arm parent replay and the eight existing boundary
normalization fixtures. Its selected implication reasons identify native positive
assignments through(3270,43773) and Tactix negative through(-43773,-20046).
Phase-save and implication hooks report different stages of the conflict counter;
the raw negative implication row is493, while the earlier phase-save row is492.

[5498 definition snapshots](../runs/20260908-5498-square-definition-selectors/definition-snapshots.json)
replay all decisions/prefixes/boundaries/phase events against5482. Auxiliary43773
is absent before Factor and has five binaries/no large clauses afterward:
Tactix negative partners20079/20060/20046, positive partners-42389/20045;
native negative partners16793/16674/16558, positive partners-42392/3270.
Equal external auxiliary IDs therefore do not imply equal definitions.

[5502 Factor localization](../runs/20260908-5502-square-factor-fresh/factor-localization.json)
has1555 extensions per arm with exact parent replay. First942 accepted
seed/member records match. Extension943 (fresh43330) is Tactix seed-4143,
member-4147 versus native seed-4171,member-4175; both2 factors,3 remainders,gain1.
Next inspect incoming physical formula and heap/update order at this boundary.
Earlier binary multiplicities differ, so this is not automatically a Factor bug.

Retained observer failures:5496 IMPLY_ONLY suppresses detail/snapshot callbacks;
5497 PHASE_ONLY captures Tactix but native uses `factor_end`, not named slot
payloads;5498 corrects selectors, then its original parser rejects native lucky
`unknown` payloads.5501 accepts only those two source-backed lucky end events,
with12 framing fixtures. No failed solve/proof/model oracle occurred.

## Validation and continuation

No solver code changed. New executable validation logic has30 fixtures:
12 snapshot,10 Sweep-window,8 Factor-payload (positive and negative cases).
The full-view5499 guard initially mistook native timing/rate log rows for
non-timing statistics;5500 keeps exact Tactix comparison, compares native's
existing three-counter extraction, and preserves all80 certificate identities.
Its earlier failure remains frozen. Other helpers reuse pin/result/hash/replay
checks; actual complete-record assertions cover this measurement-only change.

Claude-owned fuzz5492 remains under resumed terminal session34566; no fuzz
artifacts were inspected and no duplicate campaign was submitted. Proof checks
continue asynchronously. No manual deletion, allocation-contract change, push
retry or promotion. Existing runners own partial-proof/checker cleanup; preserve
all normal campaign pins, raw traces and failed observers as cited evidence.

The [06:17 UTC audit](2026-09-08-full-causal-validation.json) revalidates scoped
pins, result/trace hashes, cleanup and citations; accounting has recovered.
Production passes77/77 in6.18 seconds. Documentation checks have zero errors
and four reserve warnings. The two-page Andrew report compiles twice cleanly.
The raw CTest log retains four trailing-space lines emitted by its tests;
whitespace checks exclude that immutable log only, not code or documentation.

At final closure, both [watch5469](../runs/20260907-5469-watch-pair-BC-gate/summary.json)
and [Sweep5476](../runs/20260908-5476-sweep-cursor-BC-gate/summary.json) are66/66
checked/clean, no timeout/solve loss: B28 has14 SAT/14 UNSAT per arm, C5 has3/2.
Candidate/production Tactix B/C/combined ratios are0.906953/0.987630/0.958073
for5469 and0.899706/0.985314/0.953757 for5476 (not Kissat ratios).
All six5475 timing proofs are independently checked/clean and its final
summary is sealed. Preceding full5454 is also80/80 checked at1.170283x.
Full5485 has66/80 checked at the audit; its frozen59/80 snapshot is unchanged.
