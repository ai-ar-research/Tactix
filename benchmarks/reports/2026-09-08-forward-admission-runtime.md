# Admission repair: full runtime result

This updates the earlier [implementation checkpoint](2026-09-08-forward-admission-and-caller-debt.md),
without altering it or its delivered Andrew report.

All40 candidate5830 solves have expected verdicts in
[5838's first complete solver snapshot](../runs/20260908-5838-forward-admission-ranks1-40/solver-checkpoint.json):
**7744.440s /6810.066s =1.13720484x Kissat**,29/40 checked at the snapshot.
The1% goal remains unmet. This wave is0.90% slower in aggregate than5798's
7675.246s. Cross-window timing does not establish a precise causal cost, but
the changed work below cannot be dismissed as timing noise.

|Rank|5798 seconds|5830 seconds|Fixed Kissat seconds|5830 conflict ratio|
|---|---:|---:|---:|---:|
|32|626.521|589.745|274.456|2.499889|
|31|391.113|496.501|264.532|2.106051|
|37|489.930|494.405|346.372|1.190294|

Rank32 improves36.776s, but Rank31 costs105.388s more. Rank31's conflict ratio
was1.403354x before the change. Both ranks change proof bytes and search work.
Some other ranks preserve proof bytes and search counters while total published
statistics differ because the eliminated root-mark events are counted; do not
claim complete non-timing-statistics identity. Exact per-rank work/memory data
are in [next-losses](../runs/20260908-5838-forward-admission-ranks1-40/next-losses.json).

5836 B28+C5 has66 expected solver outcomes,64 checked at this snapshot:
B305.671/409.648s=0.746180x production; C733.843/710.402s=**1.032997x**;
combined0.928096x. C's remaining regression is explicit, not hidden by B.
Parent5806 is now66/66 independently checked and clean; its C ratio1.076240x
remains part of the record. Neither pending checks nor a favorable combined
screen authorizes promotion.

The [causal replay](../runs/20260908-5837-forward-admission-rank32-prefix/causal-followup.json)
does confirm the intended repair:61,154,590 native decisions and40 portable
forward/BVE boundaries match. The first split moves from179463 to225987 after
stable Reduce/Reorder. Two cached stop-conflict rows overlap between the early
and suffix streams; exact overlap is verified and counted once. Three small
overlap controls accompany the comparator correction.

Keep5830 as an isolated, validated experiment, not an aggregate performance
win. [New Rank31 replay5845/job1674241](../runs/20260908-5845-forward-admission-rank31-prefix/launch.json)
locates its first effect against cached5798 and native before attributing the
regression to the independently proven post-forward caller-closure defect.
That complete closure repair and Rank32's Reduce/Reorder frontier are the next
general implementation targets. No Kissat runtime refresh was submitted.

Both Release builds pass94/94, trace93/93, eight new admission cases and28
independently checked DRAT/LRAT proofs. Correctness checking stays asynchronous.
Current-source fuzz still needs the previously denied Claude-network approval;
no retry, bypass or fuzz-artifact inspection occurred. Production and cpu-q5784
remain unchanged. All documentation stays within its required length. The
new Andrew runtime addendum is local, not sent; the prior report is frozen.
