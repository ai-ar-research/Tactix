# Progress

`DEVELOPMENT_PLAN.md` owns scope, architecture, gates, and evaluation policy;
`docs/calculus.md` owns proof-rule semantics; `docs/implementation-notes.md`
owns engineering invariants; and `docs/scheduling-parity-ledger.md` owns N4.
Frozen manifests and reports own experimental detail. Git preserves the former
session narrative; this file is only the current resume surface.

## Resume anchor

- Work in `/home/hwu/SAT-solving` on branch `main`. The accepted source is the
  cumulative Phase-4.25 solver including the general little-endian Sweep phase-
  word representation repair based on predecessor `ec6ef1c`. The repair uses
  eight masked word values plus alignment-safe `memcpy` in
  `SubSolver::randomize_phases()` and retains the scalar mapping on other byte
  orders. Generator values, phase bytes, proof behavior, ticks, order,
  scheduling, learning, pruning, N1--N4, and the proof-rule/strategy boundary
  are unchanged. Evidence:
  `benchmarks/reports/2026-08-27-rank27-sweep-phase-word.md`.
- The complete promotion chain is terminal PASS: exact prefix array `1650366`;
  three paired Rank-27 timing waves in array `1650368`; full-width B28+C5
  array/aggregate `1650774`/`1650840`; and Claude-owned fuzz campaign
  `20260827-0040-sweep-phase-word-fuzz` with prep `1651241`, generated legs
  `1651243`/`1651244`/`1651247`, and reach `1651246`. The gate checked 17 SAT
  models and 16 UNSAT proofs per arm with no solve loss; Claude reported
  81/81 corpus and 30,000/30,000 generated cases, 13,914 checked SAT models,
  10,955 accepted native LRAT proofs, zero failures, and zero certificate or
  scratch residue.
- Decision-grade Rank-27 candidate/predecessor internal seconds are
  358.542/358.587, 357.916/358.759, and 357.941/359.460. Median paired ratio is
  0.997650, range 0.995774--0.999875; both arms have under-0.25% absolute
  spread and every candidate run is below 400 seconds. Full semantic SHA is
  `10372d94d2cb1ae86893408435e0c3ffa2338053b11b7012c2107a3bebf1ffd0`.
  Per project-owner direction, Rank 27 is now parked rather than treated as the
  next witness.
- **Next action:** freeze the newly accepted Tactix Release and accepted current
  instrumented Kissat comparator, re-evaluate frozen ranks 1--40 as independent
  one-solver jobs, preserve immutable scoring references, validate every SAT
  model and UNSAT proof, and select the UNSAT instance with the largest
  meaningful Tactix/Kissat slowdown. Open its earliest portable common-state
  worse-side divergence; do not return to Rank 27 or collection without new
  contradictory evidence.
- NOTHING IN FLIGHT. The interactive shell allocation, if present, is not a
  solver campaign. `.claude/worktrees/tierd-next10` is the sole historical
  secondary worktree and must remain clean unless a fresh audit proves it has
  been removed.

## Immutable authorities

- The frozen rank-selection authority is
  `benchmarks/runs/20260818-0142-tierd-ranks21-40-predeclared/manifest.json`;
  audit the corresponding ranks 1--20 authority before submitting the new
  1--40 comparison. Never infer membership from filenames or rerun a frozen
  scoring reference merely to fill a diagnostic field.
- Rank 27 remains immutable input
  `tierD24-26/5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz`,
  SHA-256 `5435b1b271b049ed9291ab1b9b297afb33b8d2d3c50ff1b48bfaed4ddf7bb4e9`.
  Accepted conflict-9450 semantic SHA is
  `3b87bb36da711bcef3ca877f3922e9fa5f8a3c6ae9d638c1ae18d4965bd92c5f`;
  accepted 64-row trace SHA is
  `7a6216bb86c236512dea03de76c4c9ab2ffdd745bd7ca7b9fa5507e708987cc1`.
- Current instrumented Kissat comparator authority is commit `8af8e56`, binary
  `b6a9b1d0...644d5`; it is diagnostic, not scoring. The once-only frozen
  Kissat 4.0.4 campaign
  `20260825-0010-tierd-kissat-4.0.4-16g-1800s` classifies all 1,166 tasks in
  `results.json` (758 valid; 408 timeout/resource/checker invalid), has zero
  residue, and must not be rerun or silently scored through invalid rows.
- User-canceled full Tier-D campaigns `1645240`/`1645241` and
  `1642950`/`1642951` are frozen non-scorable partial campaigns. Their
  `cancellation.json` files own exact classifications; pending, retained,
  scratch, and certificate residue are zero.
- The latest complete B28+C5 authority for the accepted Sweep increment is
  `benchmarks/runs/20260827-0010-BC-sweep-phase-word-guard/`. It is 66/66 valid,
  zero-residue, with candidate/predecessor PAR-2 1,362.562/1,361.275 seconds
  (1.000945x). Summary SHA-256 is
  `ac59145977a044af299d703c24ea5dc84bd25c25ded4ba85294891c5d6e4cae1`.
- Claude Code exclusively prepares, submits, monitors, inspects, and reports
  solver fuzz jobs under `DEVELOPMENT_PLAN.md` §8.2. Codex accepts Claude's
  terminal verdict and does not probe fuzz artifacts. Generated legs use 50
  physical cores when capacity permits: 100 logical CPUs, 400,000 MiB, and 50
  workers with an immutable read-only harness snapshot.

## Evaluation contract

- Every solver invocation is its own SLURM job or array task. Never serialize
  solver arms in one allocation. Full-width tier arrays have no throttle and no
  node constraint; targeted diagnostics prefer node018 when available.
- Decision-grade long timing uses three independent paired waves. Each job
  requests `-c 12` and exactly `--mem=48000M`, retains the 16 GiB process cap,
  binds one solver thread, and stages on verified node-local scratch. Decide
  from the median paired ratio and full range; reject absolute claims if either
  arm spans more than 2%. Fuzz and correctness gates are not triplicated.
- Before promoting a repair, run the complete frozen Tier B plus predeclared
  mechanism-relevant Tier C under the 16 GiB/1,200-second contract, independently
  validate every SAT model and UNSAT proof, require no unexplained solve loss,
  and leave zero certificate residue.
- Prioritize the earliest common-state point where Tactix performs equal
  normalized work materially slower or produces weaker learning, pruning,
  simplification, or retained state at comparable cost. Preserve exact rule and
  candidate order, tick/cost, schedule, and resulting state while common.
- Never introduce benchmark names, ranks, hashes, deadlines, per-instance arms,
  or tuned constants into solver behavior. A soundness defect, crash, sanitizer
  finding, rejected model, or rejected proof immediately triggers the mandatory
  bounded Claude repair workflow.

## Closed leads and reopening rules

- Sweep semantic/throughput parity, the counted directed bridge repair, BVE
  antecedent admission and flat-slice storage, decision-journal laziness,
  active-only decision selection, deferred Congruence collection,
  Substitute affected-owner traversal, import-rank inverse, reserved trail,
  Congruence first-literal/sign-mask/dense-slot indices, and the current Sweep
  phase-word repair are accepted general mechanisms.
- Collection is a secondary lead, not the main bottleneck: exact prefix work
  attributed about 4.91% to whole collection, and the rejected forwarding
  traversal only 0.33%. Current Tactix invokes collection more often than the
  accepted current-Kissat envelope, but fused-forward, arena-prepass, and
  generic key-layout prototypes are closed negative. Reopen only on a new
  exact call-reason owner with positive whole-run evidence.
- Residual BVE has no current comparable-state worse-side owner; Congruence's
  three-inline and two-inline table layouts and Substitute call-level deferral
  are rejected. Equal-score Rank-27 heap choices are neutral. Do not reopen any
  of these without contradictory exact source-backed evidence.

## Recent changes

- 2026-08-27 — Sweep phase-word expansion passes exact prefix, three-wave
  Rank-27 timing, complete B28+C5, and Claude-owned 30k fuzz; Rank 27 is parked.
- 2026-08-26 — Immutable fuzz snapshots and 50-physical-core generated-leg
  policy are active; checker-root and read-only-snapshot cleanup paths are fixed.
- 2026-08-26 — Timing policy uses `-c 12`, exactly 48,000 MiB, and three paired
  waves; whole-node exclusivity is prohibited.
- 2026-08-26 — Current collection frequency is measured but deprioritized as a
  secondary cost; closed negative collection arms remain closed.
- 2026-08-24 — Full Tier-D predecessor/current campaigns were canceled by user
  direction, classified non-scorable, and cleaned to zero residue.

<!-- track-sched:begin -->
N4 verification remains complete at the accepted parity pin. Living mechanism
dispositions and reopening conditions are in `docs/scheduling-parity-ledger.md`.
The newly accepted Sweep phase-word representation changes no schedule, rule,
candidate, tick, or policy state. The next witness is the largest validated
UNSAT slowdown among ranks 1--40, selected without rerunning frozen references.
<!-- track-sched:end -->

## Gates

- [x] T1: MiniSat correctness/performance bar.
- [x] T2: Glucose/API bar.
- [x] T3: matched Tier B/C Kissat coverage and at most 1.10x PAR-2.
- [ ] T4-K: Kissat coverage and at most 1.10x on SC24, SC25, SC26, and union.
- [ ] T4-VBS: exact status-sensitive frozen-VBS parity; blocks Phase 4.5.
- [ ] T5: exceed frozen VBS or ship the best verified effort.
