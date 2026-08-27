# Rank-5 deferred state and guidance

Date: 2026-08-27
Phase: 4.25 Tier-D convergence
Disposition: promoted portable lifecycle repair

## Objective and outcome

Fresh Rank-5 evidence showed accepted Tactix consuming materially more memory
than current instrumented Kissat on a 58,633,530-variable SAT input that early
Lucky solves before preprocessing or search. Boundary tracing attributed the
excess to per-variable state allocated before its first possible consumer.

The promoted repair defers both images on the shipped DefaultPolicy frontend:

- 40 bytes per declared variable of BVE retry, touch epochs, Backbone priority,
  analysis marks, and probing stamps; and
- roughly 35 bytes per declared variable of VMTF/EVSIDS/phase guidance and the
  compact import permutation.

Startup propagation and early Lucky advance the same event counters and epochs
without materializing cold storage. If early Lucky returns unknown, the kernel
reconstructs the first-round predicates, then `ScopedHeuristics` consumes the
parser-retained activation order before sparse collection and every later rule.
Random, replay, stress, bare-kernel/API, and caller-supplied heuristic sessions
retain eager construction.

The final three-wave median peak is 10,075,752 KiB, down 4,292,292 KiB
(29.873878%) from accepted Tactix's 14,368,044-KiB trace and 3.144256% below
current Kissat's 10,402,844-KiB median. The repair also preserves the early
terminal advantage: median Tactix/current-Kissat wall time is 21.550/29.913
seconds (0.717080x).

## Design and boundary

The change is allocation-lifetime only. It introduces no benchmark identity,
rank, input hash, deadline, per-instance behavior, or tuned threshold. It does
not change the calculus, proof emission, rule admission, instance surface,
candidate order, budgets, scheduling state, logical transition, or N1--N4
disposition.

Cold-state events remain exact while deferred. A failed early opportunity is
the single materialization boundary; assertions require every consumer
watermark still to be zero. Every variable has an activation touch, so filling
the first `touched` image with the current epoch is predicate-equivalent to its
individual historical activation stamp for the first round. Parsing and early
Lucky cannot create removal or kept-band events. Reader helpers assert that
storage has materialized.

The DefaultPolicy frontend also leaves `H` unattached through the same terminal
opportunity. Root assignments made before attachment are fixed and removed from
the later decision surface. On a failed opportunity, the existing
`ScopedHeuristics` path sizes every guidance image and consumes the exact
first-activation order retained by the parser.

## Evidence

### Localization and target confirmation

- `benchmarks/runs/20260827-0950-tierd-rank5-memory-boundaries`: accepted
  Tactix valid SAT/model-verified at 14,368,044 KiB; the DIMACS header alone
  commits 9,111,268 KiB and early Lucky returns before an inprocessing slot.
- `benchmarks/runs/20260827-1010-tierd-rank5-cold-state`: the first isolated
  arm is valid and zero-residue at 12,079,484 KiB, exactly removing the
  40-byte-per-variable cold-rule image.
- `benchmarks/runs/20260827-1040-tierd-rank5-deferred-guidance`: the complete
  candidate is valid SAT/model-verified and zero-residue at 10,075,752 KiB.
- `benchmarks/runs/20260827-1100-tierd-rank5-deferred-guidance-three-wave`:
  six of six SAT results independently verify. Tactix peak RSS is
  `[10075752,10075752,10075752]` KiB and Kissat is
  `[10402020,10402844,10402844]` KiB; median paired RSS ratio is 0.968557,
  range 0.968557--0.968634. Tactix wall time is
  `[21.550,21.450,21.550]` seconds and Kissat is
  `[30.062,29.913,29.863]`; median paired ratio is 0.717080, range
  0.716852--0.721629. Both arms satisfy the 2% stable-timing policy.

### N4, tests, and broad guard

- Release tests are 76/76 PASS; the event-trace build is 75/75 PASS. New tests
  cover an early terminal with both images absent, first-round touch/BVE
  predicate reconstruction, and an eager-versus-deferred failed-early path
  with equal decisions, conflicts, propagations, and checked extended model.
- `benchmarks/runs/20260827-1045-rank4-deferred-guidance-prefix`: the complete
  33,138-line candidate trace through 100 conflicts is byte-identical to
  accepted Tactix, stronger than the predeclared portable-detail guard.
- `benchmarks/runs/20260827-1110-BC-rank5-deferred-state-guidance-gate`:
  jobs 1652381/1652402 are 66/66 PASS, comprising 34 independently checked SAT
  models and 32 independently verified UNSAT proofs. There is no solve loss,
  invalidity, cleanup failure, or certificate residue. Candidate/predecessor
  PAR-2 is 1,326.154/1,329.271 seconds (0.997655x), below the frozen 1.10
  ceiling.

### Claude-owned fuzz

Claude Code's authoritative terminal report for
`benchmarks/runs/20260827-1120-rank5-deferred-state-guidance-fuzz` is PASS.
Codex did not inspect or reinterpret fuzz artifacts. Claude reports 81/81
corpus replays and 30,000/30,000 generated cases with zero failures; 16,041
SAT models independently checked against original CNFs; 11,035 UNSAT results
accepted by native LRAT checking; differential agreement on 27,076 verdict
cases; and zero crash, ASan/UBSan, invariant, model, proof, or differential
failure.

Direct exact-pin reach reported eight early-Lucky SAT terminals and eight
startup root refutations with cold state and `H` absent, plus twelve failed
early opportunities crossing both materialization boundaries before later
work. Twelve random/stress negative controls showed no deferral and positively
witnessed the eager lifecycle. Claude reports zero certificate, scratch,
snapshot, cache, or new failure-bundle residue. No repair workflow was needed.

## Limitations and next actions

The repair closes the observed Rank-5 memory excess; it is not a general claim
that every Tactix resident structure matches Kissat on every formula. The fresh
Rank 1--40 campaign remains non-scorable until current-Kissat Rank 18 finishes
its independent proof check. Rank 32 remains a distinct next target: fresh
accepted Tactix/current-Kissat evidence is 410.828/292.637 seconds and
582,204/299,988 KiB, while the previously rejected Congruence-admission arm
must remain closed under N4.

After landing this isolated repair, finish and classify the fresh screen,
produce its complete per-rank table, and then investigate Rank-32 memory and
downstream search/throughput owners without reopening closed mechanisms absent
contradictory exact evidence.
