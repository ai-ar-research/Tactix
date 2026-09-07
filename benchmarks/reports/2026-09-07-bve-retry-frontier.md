# BVE retry eligibility: extended Congruence-stack checkpoint

## Outcome and decision

The same isolated Congruence candidate now matches **786,346 decision records**
before its next Rank-34 difference at conflict 345034. The preceding BVE pass
at 343785 starts with 12 fewer scheduled variables in Tactix, despite equal
occurrence counts for every shared member. The next diagnostic must trace
retry-mark producers, not change heap update order or branch selection.
No solver source, guard, policy constant or production default changed here.

The runtime goal remains unmet: production Rank 1–40 campaign 5314 is
1.2787518x Kissat. The candidate remains **UNFUZZED and unpromoted**. Its
previous checked three-witness screen is 0.860414x the preceding candidate,
0.963749x production and 1.639026x Kissat, one wave only. The
[Congruence report](2026-09-07-congruence-prefix-frontier.md) owns that repair,
fixture and runtime evidence; the [runtime review](2026-09-07-runtime-review.md)
owns the review of Claude's work and rejected unsupported attributions.

## Immutable diagnostic evidence

All four campaigns reuse the exact 5370 repaired Tactix and observer-native
pins. Each is a full-width two-task SLURM array, 12 logical CPUs and 48000 MiB,
one bound proof-on solver per task, no throttle or node constraint. Every task
stops intentionally with exit zero and deletes its scratch. These partial
proofs are not checked final verdicts and their runtimes are not gate data.
Campaign manifests retain input/pin/patch/runner hashes; submission records own
job IDs. Prior diagnostic streams are validated as byte-identical prefixes.

| Campaign / job | Result |
|---|---|
| 5373 / 1668911 | Million-conflict replay: first changed decision after 786,346 identical records. |
| 5374 / 1668913 | First 12 BVE attempts match; candidate 13 differs. Tactix later eliminates 9959 while native retains it. |
| 5375 / 1668915 | Initial schedule: 1245 Tactix members versus 1257 native; all shared occurrence counts agree. |
| 5376 / 1668917 | Selected missing member 10700 has seven identical prior attempts, including exact entry/exit ticks. |

`compare-million-prefix-5373.py` and `seal-bve-frontier-5374.py` under
`benchmarks/probes/scratch/` validate the retained summaries and hashes.
The sealer initially assumed `job_id` in older submission records, which use
`job`; this parser-only error was corrected before sealing, without changing
any submitted artifact. No solver correctness failure was observed.

## Formula-to-choice frontier

At conflict 345034, ordinary decision 783732, level 257, warm-up flag zero,
Tactix chooses -6 and native chooses -9959. The preceding shared choice is
-7041. The count of matching stream records includes warm-up records; it is
not the ordinary decision counter. Legacy prefix counters are not alignment
keys.

Normalizing native rule names and excluding its outer/search-only boundaries
gives 204 paired rule-boundary records through the split. All slot/conflict
keys agree. The first active-variable or irredundant-clause count difference
is the first BVE end at 343785. Its preceding Strengthen end agrees at
8446 active variables, 104998 irredundant clauses, 17223 redundant clauses,
802 binaries. BVE ends at 8386/8387 active variables, 105591/105576 irredundant
clauses and 684 binaries. After a second Strengthen/BVE pass, Tactix retains
one fewer active variable and one fewer irredundant clause. Redundant counts
agree at all 204 compared boundaries. Earlier transient Sweep binary-count
differences at 132816 and 200611 reconverge at Substitute and remain separate
leads, not established causes.

Candidate comparison uses only `begin`/`end`: native emits `postflush` after
every attempt while Tactix emits it only for applied/root-changing attempts.
Comparing raw row ordinals would manufacture an earlier discrepancy. The first
24 normalized records agree in candidate, stage, conflict, ticks, limit and
outcome. Candidate 13 is Tactix 11215 versus native 10700, both entering at
4422306 ticks. The previous successful elimination is 20107; initial schedule
evidence rules out needing a speculative dynamic-update repair to explain
10700's absence.

Later, Tactix applies elimination of 9959 at ticks 4437490–4437570; native
declines it at 4437554–4437620. The choice observer runs after assignment.
Therefore the preceding common choice, not the changed choice row, establishes
9959 was unassigned in both arms. At that preceding choice Tactix's retired
9959 is absent from heap/queue; native retains it (heap position 4943, previous
10093, next 9701). This explains availability, not a standalone branch-policy
defect. An extra elimination is not automatically weaker or slower.

## Initial eligibility and the next producer

Native-only initial members are 6649, 9355, 10700, 13407, 19389, 20877, 21051,
21323, 21325, 21551, 21553 and 22022. Tactix has no unique member. All 1245
shared positive/negative occurrence counts agree. Shared scores differ by
Tactix-minus-native -1 for 1244 members and zero for top member 22227. That
offset is separate from the missing candidate set; it does not justify a
compensating score edit.

Both implementations initially admit active variables with their BVE retry
bit set. In the isolated source this is `BveCandidateHeap::insert_initial`
in `src/policies/default_policy.cpp`; the bit is `Kernel::bve_retry`.
Native's corresponding flag is `flags.eliminate`, set by
`kissat_mark_removed_literal` in `src/inline.h`. The diagnostic conclusion is
an eligibility discrepancy; its precise producer is not established yet.

Member 10700's seven previous attempts occur at 12503, 25008, 52118, 95799,
157864 and twice at 240040. All normalized entry/exit ticks, limits and
outcomes agree. The last attempt exits at tick 4194568 with retry zero in
both arms. A fresh read-only observer should identify the mark/reset lifecycle
between that attempt and the first schedule at 343785. Preserve native and
Tactix source snapshots and verify observer neutrality before proposing a
general repair. No diagnostic literal/conflict becomes a solver condition.

## Independent cost lead and gate state

Existing checked 5371 Rank-17 counters show repaired Tactix taking 153.164 s
versus native 113.652 s, with 165557/186237 conflicts and
1016171447/1055776860 propagations. Tactix attributes 74.491 s of its
152.492 s solve timer to inprocessing. These differing trajectories do not
permit direct unit-cost attribution or subtraction of native rule timers.
They justify matched-state inprocessing profiling as a parallel line of
investigation, not a claim that all remaining runtime is search quality or
language overhead. Rank 21's preceding/repaired candidate work is exactly
unchanged; native's work is not identical. Rank 34 still performs materially
more full-solve work than native.

5372 B28+C5, job 1668833, remains pending with 64/66 final checked/clean
records; tasks 64/65 are checking UNSAT proofs (about 29 minutes elapsed at
the scheduler recheck). Watch that exact job and run
`seal-congruence-gates-5372.py` only after all final records exist. Do not count
partial checks as acceptance, resubmit, or disturb allocation 1668242.
No fuzz campaign was launched or inspected. Consolidation still requires the
plan's Claude-owned fuzz workflow and checked gates before promotion.

## Validation and hygiene

The [artifact index](2026-09-07-bve-retry-artifact-index.json) owns the scoped
5373–5376 inventory, parents and raw trace hashes. Small witnesses, summaries,
patch chains and runners are durable; large traces and binaries remain local.
All inspected campaigns have unique evidence. No manual deletion is justified;
terminal scratch cleanup is recorded by each runner. Pending 5372 checker
scratch remains job-owned. No frozen prior report is modified.

Living documentation remains within all 30 size budgets, with four near-cap
warnings. This checkpoint updates only the concise status surface; prior
requested compaction and preserved ownership remain intact. Push is still
unapproved after the earlier auto-review rejection; do not retry or bypass it.
Production CTests pass 77/77 in 5.30 s; the rule/policy boundary check passes.
The progress checker with scheduler access has zero errors and the same two
legacy warnings (hardware-L3 citation false positives and an old uncited tier
window). The new Andrew PDF compiles twice without overfull boxes. Whitespace
checks on editable documents/helpers pass; immutable unified-patch context and
native logs retain their exact hashed formatting.
