# Caller repair: complete runtime result

Full5858/job1674410 has40 expected solves:7737.487 seconds against the fixed
6810.066-second Kissat vector, **1.136183849x**.29 certificates are checked at
the first complete solver snapshot; the rest continue asynchronously. The
1.01x goal is not met. This is essentially unchanged from5830's7744.440s
(1.137205x), not a demonstrated aggregate improvement across timing windows.

| Rank |5849 seconds|5830 seconds|Kissat seconds|5849/native conflicts|
|---|---:|---:|---:|---:|
|32|588.327|589.745|274.456|2.501209|
|31|502.337|496.501|264.532|2.106051|
|37|489.248|494.405|346.372|1.190294|

Rank31 retains exactly the parent full decision/conflict counts; the caller
repair changes cleanup/proof bookkeeping, not that instance's observed search
trajectory through300K. Rank32's entire300K decisions and partial proof also
replay the parent, but its full work/proof changes later. Do not extrapolate a
bounded replay to the complete run. Full elapsed time alone does not separate
unit cost, scheduler work and search quality.

B28+C5 has66 expected solves and64 checked at the snapshot: B305.821/409.937
=0.746020x production; C658.343/708.933=0.928639x; combined0.861730x. The
previous C regression is absent in this screen, but that improvement does not
establish TierD competitiveness or final acceptance.

The independent fixed and real evidence still supports the general repair:
full connected closure now precedes BVE, and Rank31's three-clause forward-end
residual is gone. Next priority is the actual first BVE difference, not assuming
that residual caused the loss. Paired5862/job1674504 records schedule membership,
scores, physical queue order and candidate application/propagation/budget at
179823. Its native replay fills missing diagnostic fields, never refreshes the
runtime denominator. Require exact per-arm ordinary trace and partial proof
replay before interpreting the new observers. Rank32's independent frontier
remains stable Reduce/Reorder225987.

Authorities:

- `benchmarks/runs/20260908-5858-forward-caller-ranks1-40/solver-checkpoint.json`
- `benchmarks/runs/20260908-5858-forward-caller-ranks1-40/next-losses.json`
- `benchmarks/reports/2026-09-08-forward-caller-checkpoint.json`
- `benchmarks/runs/20260908-5849-forward-caller/manifest.json`

40 fixed DRAT/LRAT proofs and four complete test suites pass. Current-source
fuzz remains unverified under the previously denied external-transfer approval;
no retry or Claude-owned artifact inspection. No promotion or goal completion.
All submitted sources, pins and runners remain immutable; proof checking and
the separate cpu-q breadth campaign continue. No manual deletion or push.
