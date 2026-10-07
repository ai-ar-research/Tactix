# Singleton-unit clean-build handoff

Frozen wrap-up snapshot, 2026-10-07. **No clean build of the new singleton-unit
admission repair is accepted.** The earlier b26 candidate completed all 353
Debug tests; its separate frozen terminal report is committed as `06b63069b`.
That result does not transfer to this repair.

The direct agent owns the three production/test files and coupled docs. Its
r3 focused source is `fc0e8285`, job **1755527**. Debug failed with exit 6 at
`tests/test_online_inprocess.cpp:3578`: the new singleton nonforcing-cut fixture
still disagrees with observed admission/decline behavior after allowing either
Complete or LimitReached. The owner canceled the ASan leg. The exact assertion
and failed artifacts are retained; do not infer a production defect or waive
the test from this evidence alone.

Clean packaging at `20261007-symmetry-units-clean-source-r3/source.tar` is
`615fc5f4`. All 6,589 regular focused-source files matched byte-for-byte,
including code, tests and coupled docs; 408 maintained ignored fixtures,
checker dependencies and reference sources were added. The source pin and
readiness record bind exact full hashes in `evidence.json`.

Release **1755536** and full Debug **1755537** were submitted with
`afterok:1755527`. They never executed. Root authorized canceling these
permanently unsatisfied dependencies for wrap-up, and both were canceled while
PENDING. No Release or Debug build pin was produced. Their scripts, manifests,
launch records and exact cancellation evidence remain intact. The prepared
Release would package only tactix, excluding native transport.

The intermediate b26 full44 original and two retries remain immutable. Root
canceled only the 35 still-pending tasks of Retry 2 (`1755462`) after the new
repair source froze; running reference tasks and results were preserved.
`reference-queue.txt` and `reference-coverage.json` are the precise snapshot.
Rank147 UNSAT and Rank401 SAT are independently checked in that snapshot;
the full44 arm is incomplete and independent fixed64 review remains a separate
acceptance gate. No further b26 retries are authorized.

Next: inspect actual counters in the failing fixture, resolve it coherently,
and pass a new immutable focused Debug/ASan plus independent proof gate. Then
freeze a new complete source and create fresh Release/full Debug pins. Do not
retarget or rewrite the canceled r3 pins. Root owns future regression, fuzz and
full44 submission decisions; the user requested stopping at this natural
checkpoint. No new broad campaigns were launched during wrap-up.
