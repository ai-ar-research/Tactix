# Progress

[DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) owns stable scope and acceptance
criteria. This file is the compact resume anchor; Git, run manifests and frozen
reports own detailed history.

## Current objective

The online-configurable strategy/N1 refactor gate is complete with N2–N4,
reference-default behavior and runtime preserved. A Claude fuzz checkpoint
found and Codex repaired one totality defect in noncanonical Walk scoring; all
former failures and the bounded broad recheck now pass. Inprocessing programs,
Lucky, and Walk operate at level zero. A search-time strategy can explicitly
apply `Restart` before them; the executor does not suspend or restore non-root
trails.

After this gate, freeze ten SAT and ten UNSAT 1200-second/32-GiB winner-VBS
losses, favoring rare baseline coverage and LymphoSAT-only SAT wins. Rerun
Tactix and every fastest qualifying baseline on hwu, then develop general solver
mechanisms. Kissat-only tuning and sequential portfolios remain out of scope.

## Authority and candidate

| Role | Authority |
|---|---|
| Refactor base | `0060215a` (`Keep inprocessing and Walk at root`) |
| Refactor closure | `b1165641`; Release pin `916eae4d7e87f5602649035ac15a8c2985e83deaec2250e338a47972b998e9bf` |
| Current source | `690322ab`; Release SHA `40b6b348adfbfd75736b45edc2c229463deb54876719ef11bfda4057eccaf9e8` |
| Design | [online proposal](docs/design/2026-09-11-online-strategy-proposal.md) and [preservation addendum](docs/design/2026-09-11-strategy-behavior-preservation-addendum.md) |
| Claude review | [7528 disposition](benchmarks/reports/2026-09-11-online-strategy-review-7528/disposition.md) and [D-NOOP recheck](benchmarks/reports/2026-09-12-claude-d-noop-recheck.md) |
| Prior gate evidence | [refactor/N1 checkpoint](benchmarks/reports/2026-09-12-refactor-n1-gates.md) and [state-boundary follow-up](benchmarks/reports/2026-09-12-n1-state-boundary-followup.md) |
| Current closure | [configurable-strategy closure](benchmarks/reports/2026-09-13-configurable-strategy-closure.md) |

The root checkout is authoritative. `.claude` contains workflow instructions,
not implementation or project-document changes.

## Implemented state

- The shared typed C++/C interface covers all 26 registered actions with
  validated per-call parameters, explicit instances, selected batches,
  complete passes, rounds, closures, reordered and repeated sequences, scoped
  fixpoints, budgets/stops, transactional reconfiguration and resumable state.
- Complete semantic surfaces include C1/C2 victims, CCE large and duplicate
  binary victims, HyperBinary/Ternary resolvents, exact Factor grids, and
  individual Congruence equivalences, units and supported clashes.
- `DefaultPolicy` owns cohesive typed branching, analysis, restart, database,
  guidance and inprocessing settings. Rule-side values that affect scheduling,
  admission or work are configurable; rule modules retain semantic,
  representation and diagnostic constants.
- Reduce owns its victim fraction, tier protection and collection choice; Walk
  owns its ProbSAT curve; Reorder owns its target, size and weighting controls.
  Default rephase, focused-phase, BVE-ladder and tier-source choices are typed,
  validated strategy data and are serialized by trace v3.
- `--random-policy` now generates typed online programs and executes them with
  `OnlineStrategyExecutor`. The separate legacy random scheduler is removed.
  Trace v3 stores typed programs; v1/v2 action traces remain replayable.
- Random programs vary orders, repetitions, parameters, granularity, budgets,
  phases and non-prefix instances. Generated fixpoints are finite; the public
  API still supports an external stop for an unbounded fixpoint.
- All inprocessing actions, Lucky and Walk reject non-root states. Restart and
  Reduce expose their pure legal guards. There is no retained non-root
  inprocessing suspension or cube replay.
- Retired-variable queue handling, zero-budget subsolver interruption, complete
  HyperBinary enumeration stops, zero-program replay and unbounded Sweep-cap
  allocation are repaired. Sweep retains the default 1024-clause initial table
  without an extra whole-formula scan.
- Obsolete solver-configuration macros, superseded test-only compatibility
  branches and the completed Rank31/34 conflict-stop/tick/pose/heap/warmup
  probe bundle are removed. Remaining production compile guards cover
  assertions and bounded event, journal and proof-shadow diagnostics.
- Proof-population shadowing distinguishes compiled availability from active
  checking. Parsing seeds the complete original clause multiset before arming;
  directly assembled kernels remain inert until explicitly armed.
- Exact Factor application and complete enumeration admit every legal
  nontrivial grid, including zero-gain 2x2 equality. Canonical matching retains
  the profitable frontier, preserving the reference hot path and trajectory.
- Walk scoring is total over every validated positive break base: increasing
  weights normalize per selected clause, and decaying tables stop at their
  floating-point fixed point without narrowing the public parameter domain.

## Current validation

- Release: **146/146** tests passed.
- Current source `690322ab` passes **146/146** Release tests and **7/7** focused
  ASan+UBSan Walk, API and random-strategy tests. Original-CNF model, native
  DRAT and native LRAT checks pass for the typed settings and zero-gain Factor
  boundary; the earlier complete sanitizer suite remains background work.
- B28+C5 candidate `20260913-0337-walkfix-gate-BC`: **33/33** solver
  results, 949.997 seconds versus 951.866 pre-fix, **0.998036x**, and 958.538
  retained, **0.991090x**.
- Rank1–50 candidate `20260913-0337-walkfix-gate-ranks1-50`: **50/50** solver
  results, 8146.381 seconds versus 8135.359 pre-fix, **1.001355x**, and 8278.533
  retained, **0.984037x**. All 83 verdicts, certificates, non-timing statistics
  and peak-RSS values match the pre-fix pin.
- Completed final records are valid and report deleted formula, certificate and
  derived scratch. Remaining proof checks continue in the background and do
  not block independent work.
- Claude job `1686458` found no oracle failure but OOMed on nonterminating Walk
  score tables; its [report](benchmarks/reports/2026-09-13-configurable-strategy-fuzz-0daf2ded.md)
  isolated positive bases at and below one. Codex repaired the full domain in
  `690322ab`. Claude's [single-job recheck](benchmarks/reports/2026-09-13-walk-scoring-closure-fuzz-690322ab.md)
  `1686614` passed all 676 former failures, 2000 fresh typed programs and 600
  square cases with every required oracle and reachability target satisfied.

## Active operations

- Storage guard PID `1076798` checks `/home/hwu/SAT-solving` every ten minutes.
  Above 700 GiB it holds only pending jobs whose Slurm `WorkDir` is this repo;
  it never suspends running jobs. Below the threshold it releases only jobs it
  held. Its latest completed check was 106 GiB with no holds.
- Post-refactor Tier-D array `1685130` covers all 1,166 instances from commit
  `b1165641` with the same `cpu-q`, four-CPU, 32-GiB and 1200-second solver
  contract. Validation job `1685119` passed SAT/model, UNSAT/DRAT, forced-crash
  and forced-timeout cleanup before submission. The array has no throttle or
  node constraint and uses the same fail-closed anonymous scratch runner.
- Historical proof-checking and already-running validation jobs continue in
  the background. Do not wait for them before doing independent work and do not
  rerun retained reference solvers.
- The fail-closed winner selector waits for complete Tier-D outcomes before
  freezing any set. The matched 40-task hwu rerun tool and its Satsuma and
  LymphoSAT adapters passed pinning, launch, proof/model and residue validation;
  see the [environment report](benchmarks/reports/2026-09-12-winner-environment-validation.md).

## Next actions

1. Leave performance proof/model checks and the storage guard backgrounded.
2. When all 1,166 Tier-D solver records exist, freeze the ten SAT and ten
   UNSAT losses, prepare and validate the matched hwu campaign, then submit it.

## Acceptance

- N1 exposes every legal action and instance at its declared state boundary;
  strategy defaults do not narrow the public calculus.
- N2 proof/model paths pass native DRAT/LRAT and original-input model checks.
- N3/N4 default scheduling and performance remain broad and within the measured
  B28+C5, Rank1–50, full50, memory/runtime and abstraction-overhead controls.
- Online random coverage is reproducible, replayable, interruptible and broad,
  with effective-transition and negative controls.
- The landed commit, reports and documentation describe the same verified
  candidate.

<!-- track-sched:begin -->
- Kissat creation order remains the production default; `TACTIX_ARENA_ORDER=1`
  is a CaDiCaL diagnostic arm. Unsupported formula-dependent profiles stay removed.
- Retain sound general repairs through downstream regressions; preserve N1–N4.
<!-- track-sched:end -->

## Recent changes

- Retired the legacy random scheduler for typed online programs and added trace-v3
  typed-program replay while preserving v1/v2 compatibility.
- Retained root-only phase boundaries and removed obsolete configuration guards.
- Restored both performance gates and submitted the validated `b1165641`
  Tier-D array.
- Moved the remaining Reduce, Walk, Reorder and default-schedule heuristics into
  validated strategy settings and completed exact zero-gain Factor N1 coverage.
