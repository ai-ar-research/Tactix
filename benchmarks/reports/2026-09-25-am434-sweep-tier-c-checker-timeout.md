# AM434: terminal AM427 matched Tier C and bounded checker-timeout diagnosis

Date: 2026-09-25. This is a failed correctness-gate classification, not an N4
or performance acceptance. The frozen matched Tier-C campaign is
`benchmarks/runs/20260925-141208-C-am427-sweep-lifecycle/`, array `1723299`,
dependent aggregate `1723300`. Both solver binaries and the AM369 no-option
DSR/Trestle checker pins are recorded in its manifest. The array tasks are
terminal; aggregate `1723300` deliberately exited `1:0` with a
`CORRECTNESS-BUG` marker for **two checker resource failures**.

All 60 task records are present, with 30 candidate/parent pairs, matching
verdicts and proof sizes. Each arm reports 15 SAT and 15 UNSAT verdicts; 15 SAT
models and 14 UNSAT certificates per arm are independently checked. The
remaining `tierC/aes_decry_2_rounds.debugged.cnf.xz` UNSAT pair is **not
verified**: both 238,948,969-byte native DSRs exceeded the campaign's
4,800-second checker budget (`proof_check_kind=checker-timeout`, not a reported
checker rejection). Solver times were 103.072 and 101.841 seconds; their
checker calls each ran about 4,800.15 seconds. The run cleaned both proof
artifacts after classification; equal sizes do not establish byte identity.
The aggregate's 29/30 solved score and PAR-2 numbers are unscorable as a
complete correctness/performance gate, and its single-case-sensitive A/B
headline must not be used to promote or reject the Sweep candidate.

The candidate and parent have equal `Sweep` and `SweepPartition` reached
counters on all 30 pairs; `SweepResume` differs on 16. This is state-machine
reachability evidence only. It does not prove the reference's lifecycle,
checked cumulative gates, or a stable runtime improvement.

To distinguish a certificate defect from the 4,800-second resource ceiling,
`benchmarks/probes/am434-aes-extended/prepare.py` prepared a **new**,
separate two-task diagnostic on the exact original compressed input and the
same candidate/parent binary hashes (`c3eafb2b...`/`b34f079e...`). Its
manifest pins the complete checker/runtime bundle, including frozen AM369
no-option DSR/Trestle; solve remains capped at 1,200 seconds with eight CPUs
and 32 GiB, while checker allowance rises to 45,000 seconds. It uses
anonymous node-local scratch and an unthrottled `0-1` array, with no node
constraint. `runtime_task.validate_runtime_pin_contract` passed before
submission. SLURM array `1723443` was submitted at 19:40 UTC and was running
at this report; `launch.json` records its exact manifest hash. Both
provisional solver records independently report UNSAT with exit 20 and the
**same 238,948,969-byte proof SHA-256**
`6a04b0150c06f05d4772afa170604e4308495c1565210bdda77ef32f30db4a62`.
These provisional records do not contain a checker verdict. This is a
bounded correctness diagnosis, not a performance rerun or a change to the
frozen Tier-C results. Preserve each terminal result, checker stage and
original-CNF proof outcome before interpreting N2 or reopening performance.

Current-source Debug job `1723391` and affected Claude-owned fuzz remain
separate pending obligations. H2's no-option proof failure also remains an
independent genuine N2 blocker. No Tier-D run was submitted.
