# Preconnection repair: full runtime and next divergence — 2026-09-08

The full gate contradicts acceptance of5870 as a performance improvement:
all40 expected solves take7936.169/6810.066s=**1.165359× fixed Kissat**, versus
parent5849's7737.487s/1.136184×.29 certificates were checked at the frozen solver
snapshot. The goal is not met. Keep5870 isolated and unpromoted; do not substitute
bounded trajectory parity for the requested runtime outcome.

Rank31 owns the regression:702.344s versus parent502.337s and native264.532s.
Full work is252,938,560 decisions,12,578,711 conflicts,5,520,509,582 propagations;
native198,706,607/4,540,757/2,864,694,340. Ratios1.272925/2.770179/1.927085.
Peak-RSS ratio2.190222 is also unresolved. Cross-window seconds per heterogeneous
counter are descriptive, not isolated unit-cost attribution. The proved first
300K trajectory closure does not persist to the full solve. New5893/job1674849
compares300K..1M complete decision streams and portable boundaries, requiring
the cumulative common89336421-decision hash to reproduce each cached300K prefix.
The native replay supplies a missing later trace, not a replacement denominator.

Other full losses:Rank32 587.303/274.456s,Rank37 491.422/346.372s,
Rank27 341.063/245.726s,Rank39 421.995/366.837s.32 has the same published work
counts as5849, although proof bytes change.37/27/39 preserve parent proof bytes
and non-timing statistics. No aggregate win is hidden by the larger Rank31 loss.

B28+C5 has66 expected solves,64 checked at snapshot: B0.740077× production,
C1.015549×,combined0.914649×. T105.2.0 alone changes C proof versus5849 and
takes373.025s versus311.915s. Keep both the C and full40 regressions visible.
Primary scoring authorities are5880 `solver-checkpoint.json`/`next-losses.json`
and5878 `solver-checkpoint.json`; never rerun those snapshot helpers.

Rank32 diagnostic5883/job1674781 is terminal and independently replay-neutral:
every cached ordinary trace hash and partial proof byte agrees through300K.
The full heap after last common decision61017804 has78397 versus78396 members.
All shared scores,assignment flags and active flags match;14282 positions differ.
Tactix alone retains assigned202958. At first unequal choice61017805, native's
116746 is still unassigned in Tactix. The selected-variable position first
differs at decision61016792, while decisions still match. This rules out a
score or selected-variable-assignment mismatch at this snapshot, not all prior
propagation or heap lifecycle causes. New5891/job1674840 moves the full heap
snapshot to first post-Reorder decision61013974 and traces202958's assignment.
Require replay before interpretation. No solver change or broad hot-loop trace.

Correctness controls, observer-stop failure5862 and checker-status anomaly
5784/tasks828/829 remain as documented in the earlier preconnection report.
The two checker cases remain invalid/unscored under the immutable runner.
Latest-source fuzz is blocked on denied external-transfer approval;no retry.
Pending expected verdicts permit investigation while proofs run,not promotion.

The next decision is whether the later Rank31 mismatch supplies a general
repair that recovers the regressing full/C gates, or whether5870 should remain
only a diagnostic branch. Do not accumulate reference-like changes without
recovering the actual performance objective. Production and old campaigns stay
unchanged; fixedKissat remains6810.066s. No manual deletion or push.
