# Rank 17 regression localization: per-candidate BVE H lifecycle

Date: 2026-08-31

## Outcome

Rank 17's current SAT regression is localized to the transition that publishes
root assignments, pivot retirements, and propagation roots to the decision
heap after each BVE candidate. The immutable pre-repair trajectory solves in
104.637 seconds; the post-repair trajectory solves in 385.402 seconds. The
formula-independent repair remains retained because this is the lifecycle used
by pinned Kissat and because the same transition closed Rank 34's exact
candidate-removal and heap-order discrepancy.

This is a behavioral divergence, not the direct cost of heap maintenance. Peak
memory is unchanged at 1,789,248 KiB, while conflicts grow 73,551 to 288,992,
decisions 2.531M to 3.744M, and propagations 347.5M to 2.460B. The next owner is
therefore either the state Tactix presents to that reference lifecycle or the
downstream heap/search consumer. No rollback arm, rank predicate, formula
classification consumer, or production effort change was added.

## Strengthen is exonerated

Campaign `20260831-3670-tierd-rank17-strengthen-boundary`, Slurm job `1657616`,
tests immutable Trace binaries immediately before and after removal of
Strengthen's unsupported whole-arena root-false flush.

| Stage | Seconds | Decisions | Conflicts | Propagations |
|:--|--:|--:|--:|--:|
| before `3220` | 218.502 | 3,466,100 | 251,252 | 1,024,955,441 |
| after `3240` | 203.878 | 3,686,096 | 176,478 | 888,129,188 |

Both SAT models independently verify and both tasks leave zero residue. The
Kissat-correct Strengthen repair improves this Rank 17 comparison, so Rank 17
does not share Rank 19's immediate owner.

## Late-repair split

Campaign `20260831-3680-tierd-rank17-late-repair-ladder`, job `1657618`,
compares the post-BVE-retry Release checkpoint with the later inline-H Release
checkpoint.

| Stage | Seconds | Decisions | Conflicts | Propagations |
|:--|--:|--:|--:|--:|
| BVE retry `3310` | 96.715 | 2,531,182 | 73,551 | 347,500,435 |
| inline H `3520` | 354.015 | 3,743,516 | 288,992 | 2,460,152,023 |

Both models verify and residue is zero. The excellent `3310` result also shows
that the earlier BVE gate, occurrence, compaction, immediate-unit, definition,
and retry repairs do not own this regression.

## Exact lifecycle boundary

Campaign `20260831-3690-tierd-rank17-eliminate-lifecycle-ladder`, job
`1657620`, separates Eliminate sparse-resume cleanup from per-candidate H
publication. All four immutable Trace arms return model-verified SAT and clean
their artifacts.

| Stage | Seconds | Decisions | Conflicts | Propagations |
|:--|--:|--:|--:|--:|
| before sparse resume `3320` | 104.427 | 2,531,182 | 73,551 | 347,500,435 |
| after sparse resume `3330` | 104.830 | 2,531,182 | 73,551 | 347,500,435 |
| before per-candidate H `3500` | 104.637 | 2,531,182 | 73,551 | 347,500,435 |
| after per-candidate H `3510` | 385.402 | 3,743,516 | 288,992 | 2,460,152,023 |

The first three arms have the identical complete trajectory. Sparse reconnect,
Eliminate cleanup, and the intervening lifecycle repairs are exonerated. The
only trajectory-changing interval is `3500 -> 3510`.

## Why the repair remains

Kissat mutates its variable heap during elimination at candidate granularity:
direct units become fixed, an eliminated pivot is removed, propagation may fix
additional variables, and those mutations affect the physical indexed heap
before the next equal-score candidate is selected. Tactix previously deferred
the event streams to the slot boundary, batching arbitrary removals. Production
now consumes direct roots, retirements, and propagation roots around each BVE
candidate in that order.

Rank 34 supplied the causal reference evidence: after this repair its complete
571-removal sequence and 568 successful classifications match pinned Kissat.
Reverting it for Rank 17 would restore a known general mechanism discrepancy
and violate the no-rank/no-formula-gate requirement.

## First-BVE schedule comparison

A global event trace was too expensive to be a valid observer: both arms of
campaign `3700` timed out even after extending the bound to 900 seconds and
produced only the header. The replacement is a Trace-only standalone observer
which performs no tracing work before the first BVE schedule, writes one
summary, and exits before the first candidate. It has no Release symbol,
production option, or behavioral consumer.

Campaigns `3710` and `3720`, jobs `1657630` and `1657632`, compare the exact
first schedule. Tactix schedules 328,081 variables and Kissat 328,795. They
share 322,663 members; 5,418 occur only in Tactix and 6,132 only in Kissat.
Among common members, 72,684 have different physical positive/negative
occurrence counts, every score differs, and 320,706 heap positions differ.
The per-candidate H repair therefore receives an already-different schedule;
it does not create that difference.

Campaign `3730`, job `1657634`, records the state that owns those schedules.
The first Eliminate conflict boundary is essentially shared (18,268 versus
18,269), but Tactix has 616,255 retained root-fixed identities on its trail,
1,910,024 irredundant clauses, and 66,106 physical binaries. Kissat has 5,593
trail entries, 1,699,758 irredundant clauses, and 254,564 binaries. Tactix's
old 944,336 `active` value was exactly its 328,081 unassigned population plus
the retained fixed population; Kissat's 328,795 active variables are already
deactivated on fixing.

## General active-state repair

Pinned Kissat `flags.c` sets `fixed`, immediately clears `active`, decrements
`solver->active`, and removes the variable from its queue and heap. Tactix
must retain the root assignment and stable identity for reconstruction, but
its shared `kernel_active_variables` helper formerly treated retirement as the
only inactive state. It now excludes semantic level-zero fixed variables while
continuing to count non-root search assignments.

Campaign `3740`, job `1657636`, is the causal first-entry check. It changes
only `active=944336` to `active=328081`; every schedule, formula, score, and
search field is otherwise identical. Campaign `3750`, retry job `1657638`,
then returns model-verified SAT in 311.993 seconds, 30.5% faster than the
449.150-second current baseline, at 1,836,916 KiB solver peak RSS. The
homogeneous campaign `3760`, job `1657639`, reproduces Rank 17 at 314.130
seconds and solves all 40 ranks provisionally.

The full candidate totals 10,290.269 seconds versus 10,573.791 previously;
geometric mean improves 2.6%. SAT time improves 10.2%, while UNSAT time
regresses 4.2%, led by Rank 33. Rank 19 improves 452.403 to 166.486 seconds,
Rank 31 improves 628.742 to 461.093, and Rank 34 improves 807.219 to 692.338.
The general repair is therefore retained. Rank 17 remains 2.58x Kissat and its
upstream physical formula/root-compaction/search-state discrepancy remains
open; Rank 33's downstream regression is a separate next localization owner.

N1--N4 and the proof-rule/strategy boundary remain unchanged. No production
option, classification consumer, formula/rank/benchmark selector, or
effort/admission gate was introduced.
