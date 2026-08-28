# SAT Factor marks and search localization

Date: 2026-08-27

## Outcome

The largest clean SAT throughput gap, Rank 6, was not caused by a strategy
choice. Tactix performed the same 43 factorizations and the same 733,290,791
logical Factor ticks as the accepted binary, but paid representation costs that
Kissat's dense Factor path does not have. The complete source-backed repair
reduces Factor time from 855.471 to 24.180 seconds. Kissat's Factor component is
about 23 seconds, so the Factor cost is closed to about 1.05x. The first checked
candidate nevertheless took 155.172 process-wall seconds because Tactix's
solver-internal timer excluded an 82-second input/watch construction cost.
Two general representation repairs reduce the final checked wall to 73.679
seconds versus Kissat's 54.454 seconds, or 1.353x. The independently checked SAT
model satisfies all 11,099,040 clauses. Decisions, conflicts, logical Factor
work, and the selected formula transformation are unchanged.

The production strategy remains formula invariant: BVE 100 permille, Factor
50 permille, forward subsumption 100 permille, and Vivify 100 permille. No
formula-, density-, benchmark-, rank-, or identity-based gate was added.

## Source and profile localization

Campaign `20260827-2600-tierd-sat-rank6-factor-perf`, job 1653055, profiled the
byte-identical accepted Release binary at 199 Hz with DWARF call graphs. It
collected 196,544 cycle samples with zero lost samples. `factor_apply_seeded`
accounted for 82.19 percent of self samples. Within that function, three
instructions forming one factor-vector membership loop accounted for 95.16
percent of local period (47.07 percent compare, 31.98 percent increment, 16.11
percent end test).

Pinned Kissat `factor.c` uses `marks[next] & FACTOR` for the same exclusion
predicate in `next_factor`. Tactix now uses one byte per literal, written for the
seed and each committed next factor and cleared over the selected factor set.
This changes the representation of the predicate, not its truth value or
encounter order.

The profile separately attributed 2.45 percent of self samples to
`build_factor_index`. Tactix copied each four-byte kernel binary watch into a
16-byte Factor record on every application; Kissat scans its current dense
watch stack. The Factor-local index now stores only large occurrences. Binary
remainders, next-factor sources, adjacency marks, and trace records scan the
kernel watch slice first, preserving the exact binary-then-large order.

The residual profiles exposed four more source-level costs. Tactix retained
mixed sparse watch lists at Factor entry, while Kissat's null-stack dense entry
flushes large watches. Its hot candidate loop reconstructed ACTIVE from value
and retirement arrays instead of reading Kissat's cached flag. The first
constant-time membership implementation used a four-byte epoch word instead of
Kissat's one-byte mark. Finally, removal of a selected, known-unique binary grid
member rescanned and generally compacted both watch lists rather than using
Kissat's find-and-move suffix operation. The final implementation uses the
null-stack flush, a byte-sized cached active image, byte membership marks, and a
known-count eager deletion path. Duplicate or mixed-list binary deletion keeps
the general stable compaction fallback.

The repairs were staged and checked without changing the logical trajectory:
borrowed binary traversal plus constant-time membership reduced Factor to
49.738 seconds; packed binary traversal to 36.646; cached ACTIVE to 31.897; and
byte marks plus eager known-unique deletion to 24.180. One intermediate ACTIVE
diagnostic wrote beyond the fixed initial-literal boundary and terminated
UNKNOWN; it carries no correctness or performance verdict. The corrected code
updates only the first admissible fresh positive literal, matching Kissat's
fixed `next_factor` boundary.

## Checked Rank-6 Factor result

The exact candidate binary is SHA-256
`4ae51f0758b982ade62ea9165fbba8135fe8f0952e85a72bb00419eb00327aad`.
Campaign `20260827-2730-tierd-sat-rank6-factor-byte-eager-checked`, job 1653143,
returned SAT and independently verified all 11,099,040 clauses. It left no
certificate or scratch residue.

| Metric | accepted profile | candidate | result |
|---|---:|---:|---:|
| Factor applications | 21 | 21 | identical |
| Factorings | 43 | 43 | identical |
| Factor ticks | 733,290,791 | 733,290,791 | identical |
| Decisions | 2,867,414 | 2,867,414 | identical |
| Conflicts | 331,898 | 331,898 | identical |
| Factor seconds | 855.471 | 24.180 | 35.379x faster |
| Solver-internal seconds | 903.802 | 72.953 | 12.389x faster |
| Peak RSS KiB | 1,634,304 | 944,948 | 42.2% lower |

The table's internal time is not comparable to Kissat's process wall. The same
checked candidate's actual process wall was 155.172 seconds. Factor itself is
approximately 1.05x the Kissat component; the remaining discrepancy was in the
general input/watch construction path.

## End-to-end input/watch repair

Campaign `20260827-2770-tierd-sat-rank6-overall-perf` localized the hidden
pre-solve wall to repeated whole-arena packing while DIMACS import grew many
literal watch slices in lockstep. Deferring global packing across the bounded
input scope, then performing one stable final pack, reduced checked wall from
155.172 to 92.612 seconds without changing any directed watch order.

Campaign `20260827-2790-tierd-sat-rank6-import-residual-perf` then localized
the residual import cost to two linear live-pair scans for every unique input
binary. No deletion can occur during initial import, so the retained
`BinaryIdMap` keys are the exact live-set image. The parser now uses that O(1)
membership authority and calls a known-new insertion surface; the general
post-import API retains its live-pair scan.

Final candidate SHA-256 is
`de49726f72df9f27d3c34568685b1bcec3d2ea06db9c4ca03787d3a207ca6374`.
Campaign `20260827-2800-tierd-sat-rank6-import-live-map-checked`, job 1653256,
returns SAT in 73.679 process-wall seconds and independently checks all
11,099,040 clauses. It preserves exactly 2,867,414 decisions, 331,898 conflicts,
43 factorings, and 733,290,791 Factor ticks. This closes the reported 2x
end-to-end gap on Rank 6 to 1.353x without changing strategy, effort, ordering,
or formula-dependent behavior.

## SAT search-quality localization

Rank 16 remains a separate pure-search witness: Tactix spends about 95 percent
outside inprocessing and reaches 54,069,422 decisions and 20,020,422 conflicts,
versus Kissat's 14,441,406 decisions and 4,777,402 conflicts.

Campaign `20260827-2620-tierd-sat-rank16-effort-owner` ran the exact accepted
binary under the invariant default, BVE 5 permille, Factor 5 permille, forward
subsumption 5 permille, and their combined former `Dose5` setting. All five
arms produced exactly 54,069,422 decisions and 20,020,422 conflicts. Their
875.167--882.425 second walls differ only by noise. On this witness the former
formula profile is trajectory-dead and provides no explanation or remedy for
the SAT gap. The diagnostic checker wrapper omitted its adjacent module, so
these runs are mechanism evidence only; the identical default trajectory's
SAT model was independently verified in the homogeneous authority.

Campaign `20260827-2640-tierd-sat-rank16-no-factor-lifecycle` disables the
entire Factor entry as an explicit diagnostic arm. It also produces exactly
54,069,422 decisions and 20,020,422 conflicts, solves in 875.250 seconds, and
independently verifies the SAT model. Thus Rank 16 is not owned by Factor
selection, output, effort, or the no-op dense/sparse lifecycle. Factor remains
enabled at 50 permille in production.

The paired trace agrees through conflict 1008 and learns from the same conflict
clause at conflict 1009. Campaign
`20260827-2760-tierd-sat-rank16-propagate-c1008-1009` (job 1653251) captures
the preceding watch scans. The first causal mismatch is the physical watch
order for propagated literal `677`: Tactix reaches
`(-803,-677,-671)` before `(-740,-677,-674)`, while Kissat reaches those two
clauses in the opposite order. This enqueues `-671,-674` versus `-674,-671`,
then `672,673` versus `673,672`; Kissat additionally implies `-694`. During
analysis, the first reason-walk mismatch is correspondingly after resolving
`-704`: Tactix next resolves `-703`, while Kissat resolves `-676`.

This watch order is the already documented global arena-order deviation, not a
Factor, effort, or formula-profile discrepancy. The Kissat creation-order arm
matches the bounded Rank-16 trajectory but failed the cumulative guard and
therefore cannot replace the default or become an instance gate. Rank-16
localization is closed at that owner; further SAT work targets general
representation costs that can improve the single default without choosing an
order by formula or benchmark.

## Correctness and gates

Focused Factor, Factor-N1, and binary-deletion tests pass under both Release and
ASAN (`detect_leaks=0`), and the complete Release suite passes 76/76. The
implementation changes no strategy field and preserves N1--N4.

The final frozen B28+C5 campaign is
`20260827-2810-BC-factor-import-final` (array job 1653257, aggregate job
1653275). The homogeneous one-candidate Rank 1--40 campaign is
`20260827-2820-tierd-ranks1-40-factor-import-final` (job 1653277). Both pin
candidate SHA-256
`de49726f72df9f27d3c34568685b1bcec3d2ea06db9c4ca03787d3a207ca6374`.
UNSAT proof completion is non-blocking and assumed passing per user instruction;
any contrary checker result reopens correctness. Final aggregate figures are
recorded when the solver/model lanes complete, without waiting for a slow proof
lane.
