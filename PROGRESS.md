# Progress

[DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) owns stable scope and acceptance
criteria. This file is the compact resume anchor; Git, run manifests and frozen
reports own detailed history.

## Current objective

The online-configurable strategy/N1 refactor gate is complete with N2–N4,
reference-default behavior, and runtime preserved. Inprocessing programs,
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
| Refactor closure | Current checkout; Release binary `916eae4d7e87f5602649035ac15a8c2985e83deaec2250e338a47972b998e9bf` |
| Design | [online proposal](docs/design/2026-09-11-online-strategy-proposal.md) and [preservation addendum](docs/design/2026-09-11-strategy-behavior-preservation-addendum.md) |
| Claude review | [7528 disposition](benchmarks/reports/2026-09-11-online-strategy-review-7528/disposition.md) and [D-NOOP recheck](benchmarks/reports/2026-09-12-claude-d-noop-recheck.md) |
| Prior gate evidence | [refactor/N1 checkpoint](benchmarks/reports/2026-09-12-refactor-n1-gates.md) and [state-boundary follow-up](benchmarks/reports/2026-09-12-n1-state-boundary-followup.md) |

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
- Obsolete solver-configuration macros are removed. Remaining production
  compile guards cover assertions and bounded event, journal, proof-shadow and
  conflict-stop diagnostics.

## Current validation

- Release: **146/146** tests passed.
- ASan+UBSan: **145/145** core tests passed (the established demo3 exclusion).
- B28+C5 candidate `20260912-2105-online-refactor-final-BC`: **33/33** solver
  results, 958.538 seconds versus 962.588 retained, **0.995793x**; no verdict,
  expected-result or certificate-hash mismatch and no instance over 1.10x.
- Rank1–50 candidate `20260912-2105-online-refactor-final-ranks1-50`:
  **50/50** solver results, 8278.533 seconds versus 8327.103 retained,
  **0.994167x**; no verdict, expected-result or certificate-hash mismatch and
  no instance over 1.10x.
- Completed final records are valid and report deleted formula, certificate and
  derived scratch. Remaining proof checks continue in the background and do
  not block independent work.
- Claude's [bounded final fuzz checkpoint](benchmarks/reports/2026-09-12-online-refactor-final-fuzz.md),
  Slurm job `1685021`, passed in 14:38
  with zero audit, model, proof, differential, replay, interruption or negative-
  control failures. Its fresh leg applied all 26 actions across 144,908 typed
  programs; 940 DRAT and 641 native LRAT proofs and 2,557 models checked. The
  disclosed focused-test wrapper gap is covered by the complete local suites.
  Codex retains implementation and debugging ownership.

## Active operations

- Storage guard PID `1076798` checks `/home/hwu/SAT-solving` every ten minutes.
  Above 700 GiB it holds only pending jobs whose Slurm `WorkDir` is this repo;
  it never suspends running jobs. Below the threshold it releases only jobs it
  held. Its latest completed check was 103 GiB with no holds.
- Committed-source Tier-D array `1683876` remains unchanged on `cpu-q`, four
  CPUs, 32 GiB and a 1200-second solver timeout. Its anonymous node-local
  `O_TMPFILE` runner removes input, proof, model and checker scratch on every
  exit path.
- Post-refactor Tier-D array `1685130` covers all 1,166 instances from commit
  `b1165641` with the same `cpu-q`, four-CPU, 32-GiB and 1200-second solver
  contract. Validation job `1685119` passed SAT/model, UNSAT/DRAT, forced-crash
  and forced-timeout cleanup before submission. The array has no throttle or
  node constraint and uses the same fail-closed anonymous scratch runner.
- Historical proof-checking and already-running validation jobs continue in
  the background. Do not wait for them before doing independent work and do not
  rerun retained reference solvers.
- The fail-closed winner selector now recognizes a final timeout with no
  `/usr/bin/time` peak only when the final record names the matching Slurm task
  cgroup contract. A partial dry run on the older campaign finds 354 genuine
  timeouts and 72 SAT plus 91 UNSAT checked VBS wins, but freezes nothing while
  outcomes are missing. Its provisional SAT top ten are LymphoSAT-only and its
  UNSAT top ten are Satsuma-only.
- The matched winner rerun tool is ready to pin a final 40-task, instance-major
  matrix on hwu. It independently recomputes selection evidence and uses
  anonymous formula, proof, model, solver-stream and checker scratch, including
  direct exact-binary adapters for Satsuma and LymphoSAT. Compute-node job
  `1685454` passed the pinned environment, launch binding, every adapter,
  proof/model pipeline, crash/timeout cleanup and residue check; see the
  [environment report](benchmarks/reports/2026-09-12-winner-environment-validation.md).

## Next actions

1. Continue inspecting background Tier-D, proof/model, cleanup and storage-guard
   state without blocking independent work.
2. When all 1,166 latest Tactix solver records exist, freeze the ten SAT and ten
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
- Specialized finite and interruptible subsolver paths and repaired queue state.
- Removed obsolete configuration guards and retained root-only phase boundaries.
- Restored both performance gates and submitted the validated post-refactor
  Tier-D array from `b1165641`.
- Repaired cgroup-timeout selection and validated environment-pinned,
  launch-bound matched reruns with anonymous proof/model pipelines.
