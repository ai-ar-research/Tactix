# Bound advancement after elimination quota cuts

## Outcome and objective

The Rank 1–40 goal remains at most 1.01x Kissat, with unchanged B28+C5 gates.
It is not met: production is checked at 1.2787518x; the fully checked Rephase
candidate is 1.202033658x. No runtime gain is yet claimed for the newest repair.
Production source remains unchanged. Isolated candidate **5459** restores the
already documented SP-FACTOR-1 bound/retry lifecycle after a quota cut, retaining
5450's independent strict resolution-limit stop.

The preceding [phase-budget report](2026-09-07-eliminate-phase-budget.md) owns
5450, its 12 fixed phase scenarios, and the 7,918,162-row common decision prefix.

## Evidence selecting the repair

Diagnostic observer 5456 changes only filtered event publication. Its 81/81
trace tests include a fixed event-window regression. Campaign 5457 has two
clean controlled stops at conflict 32074; both complete 7,921,492-row streams
match their respective 5453 predecessors. Partial proofs were hashed and
deleted by the runner, not claimed as complete checked certificates.

At conflict 32073, both Factor calls have exactly matching initial schedules
and scores: 117141 then 117154 entries. The first call has the same three seeds
and two grids. Native and Tactix emit member observations in reverse orders;
the supplemental ordinal-normalized comparison proves the grid members equal.
The frozen original comparator's raw `members.equal=false` is not a semantic
grid discrepancy. [Localization](../runs/20260907-5457-rank37-factor-window/bound-localization.json)
preserves both observations without editing the original summary.

The second call begins at seed -145 with 34922 remainders in both arms.
Tactix accepts a two-factor/four-remainder grid of gain 2. Native declines
that seed and proceeds to -488187, accepting a gain-55 grid. The cause selected
by source inspection is the coupled bound, not initial heap order.

Pinned Kissat's `eliminate_variables` releases the round schedule before final
completion bookkeeping. Its outer `last_round_eliminated` stays zero while an
inner variable shadows it. Therefore every consistent phase takes the completed
bound transition, including a resolution-limit exit. Nonterminal bounds advance
and rearm all variables; terminal bounds latch admission watermarks without
clearing unvisited retry flags. Factor requires gain strictly above this bound.
CaDiCaL instead requires actual phase completion; the repository's previously
documented choice is pinned Kissat's behavior, expressed directly without
reproducing the shadowing. Native verbose campaign 5458 remains an independent
pending runtime confirmation of the expected 0→1→2 transitions.

Commit 03133b31 introduced an incomplete-round latch and retry clearing that
contradict that existing SP-FACTOR-1 contract. Candidate 5459 removes only this
latch/clearing and restores the unconditional consistent engaged-phase step.
Other changes from that commit, current-round cleanup and 5450's strict stop
are retained. No public guard, rule effect, instance surface, proof delta,
reconstruction obligation, effort constant or formula-specific selector changes.
Accordingly, the calculus and guard ledger require no semantic edit; the
scheduling ledger is reopened with the isolated repair and pending gates.

## Tests and immutable identity

[5459 authority](../runs/20260907-5459-bound-advance/manifest.json) owns patches,
source hashes and logs. The actual predecessor-object fixture passes its old
expectations: both bound modes stay at zero and clear remaining retries.
The repaired fixture covers an irregular advancing ladder and a terminal
bound, checking Factor coupling, retry bits, admission watermarks, the public
BVE guard, root fixpoint and kernel invariants. Existing scheduler expectations
and the 12 phase-budget cases now require the restored next-phase retry work.

Release 5459 passes **82/82 in 7.18 s**; independent BC 5461 passes **82/82 in
9.36 s**; trace 5460 passes **81/81 in 5.72 s**. Both Release binaries share SHA
`84300eafeef6c973c7f815abe3358331f3cd34a6cadf3282bdc244cd000e197a`;
trace SHA is `509877b2c8f2fb7fbcd2faac6cdcf90cbae3eca77dd1eccce5e1816a0222e9ef`.

The user's new plan requirement makes test consideration explicit for every
change. This implementation adds the cut-bound regression and updates affected
fixtures. The documentation-only contract needs no solver unit test; existing
documentation size, reference, and contract checks cover its changed surface.
The new witness runner retains the existing proof/model oracle and adds only
diagnostic log preservation; syntax checks and its nine checked tasks validate
that path. Full model/proof and fuzz gates remain separate and unpassed.

## Submitted diagnostics and gates

| Campaign | Job | Scope |
|---|---|---|
| 5458 | 1670509 | Native verbose bound confirmation; controlled stop. |
| 5462 | 1670635 | Three short B instances × 5450/5459/Kissat, full proof/model checks. |
| 5463 | 1670637 | Rank-37 complete 100000-conflict streams; test Factor boundary closure. |
| 5464 | 1670636 | Unchanged B28+C5 candidate/production gate. |
| 5465 | 1670638 | Unchanged full Rank 1–40 candidate/Kissat view. |

Short witnesses are rbsat-v760c43649gyes10, square.2.0.i.smt2-cvc4, and
pyhala-braun-sat-35-4-04. Prior 5435 scouting showed roughly 4.6–6.3 s solves
with productive BVE/Factor and bound rejections. This establishes a useful
selection rationale, not that they reproduce the exact cut or prove a speedup.
The manifests predeclare identities and preserve original tier composition.

All new arrays are full width, without a throttle or node constraint, with one
single-threaded solver per 12-logical-CPU/48000-MiB task. Normal gates keep the
existing atomic solve/check/cleanup contract and asynchronous continuation.
No proof checker was stopped to permit development. The full-view preparation
first encountered a generated predecessor `__pycache__` directory, then a
read-only copied patch during scaffold completion. Both were preparation-only
errors, before solver execution; the verified unsubmitted scaffold was completed
by skipping directories and already-identical copies. The missing-launch-file
submission created no job; 1670638 is the sole successful full-view submission.

## Other results, limitations and next action

[5447 timing snapshot](../runs/20260907-5447-replacement-cursor-rank35-runtime/solver-checkpoint.json)
now has three exact-statistics and exact-proof-byte pairs. Median candidate /
predecessor is **0.976661583x**, range **0.976171498–0.989633235**; both arm
absolute ranges are below 2%. This approximately 2.3% local implementation gain
does not establish aggregate closure. All six proofs were pending at snapshot.

Background Claude owner session 30094 is still live and tests only 5438. Its
terminal response is the sole authority; no fuzz job, log or artifact was
inspected. The next periodic checkpoint must cover 5445, 5450 and 5459.
Any reported solver/model/proof failure pauses dependent work. Ordinary pending
checks do not block development and are never labeled verified acceptance.

Next inspect 5458, compare 5463 completely, and determine whether the short B
witnesses reach the cut/bound transition. Then inspect all per-instance runtime,
work and memory in 5454/5465 and close only checked/clean B28+C5 gates. Retain
raw causal evidence, submitted sources/pins and checker-owned scratch. No manual
deletion, production promotion, push retry or separate-checker contract change
occurred. Push remains unapproved. The companion validation index and Andrew
report own this checkpoint's final audits.
