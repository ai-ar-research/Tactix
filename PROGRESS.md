# Progress

[DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) owns stable scope and acceptance
criteria. This file is the compact resume anchor; Git, run manifests and frozen
reports own detailed history.

## Current objective

The online-configurable strategy/N1 implementation is complete. Final N2–N4
acceptance is being rechecked after Claude fuzzing exposed inherited root
propagation debt between independent probes and slow SIGTERM handling inside
Sweep. Codex repaired both boundaries. Inprocessing programs, Lucky, and Walk
operate at level zero. A search-time strategy can explicitly apply `Restart`
before them; the executor does not suspend or restore non-root trails.

The post-gate winner-VBS set is now frozen and its matched rerun is complete:
ten LymphoSAT-only SAT wins and ten Satsuma-only UNSAT wins over genuine Tactix
timeouts. S5 Lucky now has two general structural model mechanisms: instance 6
derives GF(2) candidate guidance from ternary groups, and instance 7 recognizes
exact one-hot GF(3) affine encodings. Both accept only a model checked against
the complete live CNF. Kissat-only tuning and sequential portfolios remain out
of scope.

## Authority and candidate

| Role | Authority |
|---|---|
| Refactor base | `0060215a` (`Keep inprocessing and Walk at root`) |
| Refactor closure | `b1165641`; Release pin `916eae4d7e87f5602649035ac15a8c2985e83deaec2250e338a47972b998e9bf` |
| Current source | `8f9161f4`; Release SHA `eea4de53e617e89ca1223a6ff254b9457ccfbf6f627f72dc3e60147445a5620f` |
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
- Lucky instance 6 exposes variable, group, total-constraint, equation,
  free-variable and candidate limits as validated scoped settings through the
  C++ and C online APIs. Production random programs vary, serialize and replay
  the same settings and can explicitly select the structural instance.
- Lucky instance 7 exposes the corresponding one-hot domain, GF(3) equation
  arity, free-dimension and candidate settings. Equation recognition requires
  complete exactly-one and forbidden-tuple encodings; public and random-online
  callers reach the same implementation.
- Independent probing scopes now close retained level-zero implications before
  opening the next assumption. Sweep and BVE embedded solves observe external
  stops for finite as well as unlimited budgets; Sweep also polls model-flip,
  detection and proof-emission loops without consuming interrupted items.

## Current validation

- Current source `8f9161f4` passes **147/147** Release tests excluding the long
  scripted demo and **17/17** focused ASan+UBSan tests. The preceding GF(3)
  source passed **147/147** Debug tests under the same exclusion.
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
- The GF(2) short gate solved both parity targets with independent model checks;
  B28+C5 ran 33/33 with identical semantic counters at 1.0069x retained time,
  and Rank1–50 ran 50/50 at 1.0167x with identical semantic counters. Certificate
  checks remain background work.
- The GF(3) short gate `1686852` solved all eight remaining selected SAT targets
  in 0.034 seconds each. Every original-CNF model verified, all eight reached
  400 domains and 360 rank-360 equations, and anonymous cleanup was complete.
- Its B28+C5 gate `1686861` produced 33/33 expected solver results in 953.067
  seconds: **1.0032x** the retained refactor build and **0.9964x** the parity
  build, with identical semantic counters. Certificate checks remain background
  work; candidate-only Rank1–50 job `1686894` is running under the retained
  resource contract.
- Claude checkpoint `1686850` found two Debug replay invariant failures and one
  release interruption failure. Exact Codex repros `1686953` and `1686954` now
  replay cleanly; final SIGTERM repro `1686958` exits `UNKNOWN` in 1.195 seconds
  without SIGKILL or named formula/proof/trace residue. A corrected bounded
  Claude checkpoint and final performance gates remain acceptance work.

## Active operations

- Storage guard PID `1076798` checks `/home/hwu/SAT-solving` every ten minutes.
  Above 700 GiB it holds only pending jobs whose Slurm `WorkDir` is this repo;
  it never suspends running jobs. Below the threshold it releases only jobs it
  held. Its latest completed check was 114 GiB with no holds.
- Post-refactor Tier-D array `1685130` covers all 1,166 instances from commit
  `b1165641` with the same `cpu-q`, four-CPU, 32-GiB and 1200-second solver
  contract. Validation job `1685119` passed SAT/model, UNSAT/DRAT, forced-crash
  and forced-timeout cleanup before submission. The array has no throttle or
  node constraint and uses the same fail-closed anonymous scratch runner.
- Historical proof-checking and already-running validation jobs continue in
  the background. Do not wait for them before doing independent work and do not
  rerun retained reference solvers.
- The fail-closed winner selector waited for complete Tier-D outcomes before
  freezing the set. It froze the required 10+10 targets, and matched job `1686574`
  reproduced all 20 Tactix timeouts plus all 20 checked winner solves with zero
  cleanup residue; see the [target report](benchmarks/reports/2026-09-13-winner-vbs-targets.md).

## Next actions

1. Leave performance proof/model checks and the storage guard backgrounded.
2. Run one corrected bounded Claude checkpoint and current-source B28+C5 and
   Rank1–50 default-preservation gates.
3. Once those pass, submit the latest committed Tactix over Tier D with the
   validated anonymous-scratch runner and retained four-CPU/32-GiB/1200-second
   contract.
4. Then study proof-producing symmetry mechanisms for the ten selected UNSAT
   losses; do not add a constraint without a native DRAT/LRAT justification.

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
- Froze and reran the exact 10 SAT + 10 UNSAT winner-VBS loss set; began the
  general Boolean-parity mechanism from two structurally recovered SAT cases.
- Added exact one-hot GF(3) recognition and model search, covering the other
  eight selected SAT losses with independently checked models.
- Closed root propagation before independent probes and made long Sweep proof,
  model-flip and embedded-solver work interruptible and safely resumable.
