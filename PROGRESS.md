# Progress

[DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) owns stable scope and acceptance
criteria. This file is the compact resume anchor; Git, immutable run manifests,
and frozen reports own detailed history.

## Current objective

The online-configurable strategy/N1 refactor is accepted at `a08f9f6a`. Native
DSR is now Tactix's canonical whole-solver certificate. Proof production remains
mandatory; DRAT and native LRAT remain compatibility formats for the older rule
vocabulary.

The next increment is the first general direct-DSR substitution-redundancy rule
and orbitopal producer for the Satsuma symmetry path. Preserve N1--N4 and the
reference default, then resume the fixed WV100 mechanism program. Do not submit a new full Tier-D run at the DSR
boundary. Kissat-only tuning, benchmark dispatch, and sequential portfolios are
out of scope; MAB scheduling remains an open design choice.

## Authority

| Item | Authority |
|---|---|
| Accepted refactor source | `a08f9f6a`; Release SHA `b63f30031229a36b9fea46e37590e892ab4a178a1239357ff3f80557548e1413` |
| Refactor evidence | [online refactor/N1 closure](benchmarks/reports/2026-09-13-online-refactor-n1-final.md) |
| Strategy design | [online proposal](docs/design/2026-09-11-online-strategy-proposal.md) and [preservation addendum](docs/design/2026-09-11-strategy-behavior-preservation-addendum.md) |
| DSR references | [B27+C5](benchmarks/baselines/b27c5-tactix-a08f9f6a-v1/manifest.json) and [Rank1--40](benchmarks/baselines/rank1-40-tactix-a08f9f6a-v1/manifest.json) |
| Winner target | [fixed WV100 set](benchmarks/tiers/WV100.tsv), 33 SAT / 67 UNSAT |
| WV100 protocol | [mechanism integration and experiment design](docs/design/2026-09-13-wv100-mechanism-integration.md) |
| Satsuma evidence | [transition screen](benchmarks/reports/2026-09-13-wv100-satsuma-transition.md), [orbitopal G0](benchmarks/reports/2026-09-13-wv100-orbitopal-sr-g0.md), and [symmetry G2](benchmarks/reports/2026-09-13-wv100-structured-symmetry-g2.md) |

The root checkout is authoritative. `.claude` contains workflow instructions,
not implementation or project-document changes.

## Accepted refactor state

- `DefaultPolicy` is decomposed into typed branching/analysis, restart/mode,
  database, guidance, and inprocessing settings while preserving static hot
  dispatch and the reference schedule.
- The C++/C interface covers all accepted rules with validated per-call
  parameters, legal explicit instances, selected batches, passes, rounds,
  closures, reordered/repeated sequences, scoped fixpoints, budgets, stops, and
  online reconfiguration without restarting the solver.
- The N1 surface includes complete C1/C2 victims, CCE large and duplicate-binary
  victims, HyperBinary/Ternary resolvents, exact Factor grids, and individual
  Congruence equivalences, units, and supported clashes.
- Every scheduling, admission, effort, search, and heuristic value is strategy
  data. Rule modules retain soundness, representation, and diagnostic constants.
- `--random-policy` uses the shared typed executor. Its seeded trace varies
  order, repetition, settings, granularity, budgets, phases, and non-prefix
  instances and is replayable. The legacy random scheduler is retired.
- All inprocessing actions, Lucky, and Walk operate at level zero. A search-time
  caller explicitly composes `Restart` before invoking them.

The accepted performance gate remained within noise: B28+C5 was 950.837 s
versus 948.484 s (**1.002481x**) and Rank1--50 was 8,123.812 s versus 8,041.173 s
(**1.010277x**), with all expected outcomes and matching certificate/statistic
checks. Claude job `1687182` passed the final refactor fuzz checkpoint.

## Native DSR status

- Tactix contains streaming binary/ASCII DSR, typed `add_sr`, CLI/C
  API selection, and one combined certificate for ordinary CDCL,
  inprocessing, and future substitution-redundancy additions/deletions.
- The default checker path is `dsr-trim` to LSR followed by pinned Trestle
  `srcheck`. Benchmark, runtime, and fuzz launchers use anonymous scratch with
  cleanup traps for normal exit, timeout, crash, cancellation, and signals.
- Unit/API/oracle checks pass; Slurm job `1688419` passed the complete proof
  micro suite, `1688422` passed 148/148 Release tests, and `1688423` passed all
  147 applicable ASan/UBSan tests.
- B27+C5 paired gate `1688455` completed 64/64 tasks. Candidate DSR solver time
  was 926.873 s versus 931.337 s for the accepted reference (**0.995207x**) and
  928.581 s for its same-source DRAT control (**0.998161x DSR/DRAT**).
- Rank1--40 paired gate `1688424` has 76/80 completed tasks and four running
  tasks. Its complete solver-time vector is already available: DSR 7,516.228 s
  versus 7,589.858 s for the accepted reference (**0.990299x**) and 7,507.764 s
  for same-source DRAT (**1.001127x DSR/DRAT**). Final checker completion remains
  background work.

Claude's [terminal audit](benchmarks/reports/2026-09-13-native-dsr-claude-fuzz/terminal-report.md)
of native-DSR fuzz job `1688465` is a recorded **FAIL**:
23 generated UNSAT cases stopped in `dsr-trim`; DRAT/LRAT, models, differential
results, replay, negative controls, sanitizer lanes, and scratch cleanup were
clean. The retained `d000848` reproducer showed the fault in upstream
`dsr-trim` commit `ffe486`: its formula hash initialization revisited input
tautology slots already deleted by parsing and raised `Clause 3 was already
deleted`, even for an empty DSR stream.

The [checker repair report](benchmarks/reports/2026-09-13-dsr-trim-tautology-hash-fix.md) records the diagnosis. The repository pins
[`dsr-trim-ffe486-tautology-hash.patch`](tools/checkers/patches/dsr-trim-ffe486-tautology-hash.patch).
It skips deleted formula slots during hash initialization and does not change
proof semantics. The old proof then elaborates successfully and Trestle accepts
the LSR against the unchanged raw CNF. Checker setup/self-test job `1688572`
passed DRAT, LRAT, ordinary DSR, and raw-tautology DSR-to-LSR-to-Trestle tests.
Replacement Claude-owned fuzz checkpoint `1688573` [passed terminal audit](benchmarks/reports/2026-09-13-native-dsr-claude-fuzz-r2/terminal-report.md)
with the same solver binaries and the patched checker: 0 failures in all five
audit modes and all stock fuzz legs, all required keys reached, compatible
proof/model/differential/replay/negative/sanitizer checks clean, and no scratch
or node residue. A fixed micro proof checked one direct non-RAT SR witness and
rejected an incomplete witness; the pinned pre-SR solver emitted no SR step, so
the new X7 rule still requires its own Claude fuzz checkpoint.

## Operations and fixed experiment contracts

- Existing proof-checking jobs continue in the background and do not block
  independent implementation. Do not rerun retained reference solvers.
- The completed Tier-D array `1687184` is frozen at
  [`20260913-0905-tierd-refactor-final-a08f9f6a`](benchmarks/runs/20260913-0905-tierd-refactor-final-a08f9f6a/).
  It used `cpu-q`, c4, 32 GiB, and a 1,200-second solver timeout with anonymous
  scratch cleanup. It gives current Tactix 17/100 checked effective WV100
  coverage. No replacement full Tier-D run is planned now.
- Focused WV100 development uses `cpu-dense-hwulab-q`, c8, 32 GiB, and a
  300-second solver timeout. General B28+C5 and Rank1--50 regression gates use
  that partition with c12, 32 GiB, and 1,200 seconds. The DSR landing gate is
  the fixed B27+C5 and Rank1--40 subset under the same c12 resources.
- Storage guard PID `1458620` checks repository use every ten minutes. Above
  700 GiB it holds only pending jobs from this repository and releases only its
  own holds after use drops below the threshold. It never pauses running jobs.
  The latest check was 123 GiB and clear.
- WV100 permanently freezes the first 100 all-configuration ranking rows, each
  solved within 1,200 seconds and 32 GiB by exactly one checked winner: 34
  LymphoSAT, 57 Satsuma+Kissat, seven AE-Kissat-MAB, and two kissat-sc2024.
  Solved Tactix rows remain in the denominator.

## Next actions

1. Keep the four remaining Rank checker tasks in the background and freeze their
   report when they finish.
2. Implement and gate direct SR/orbitopal symmetry through the shared online
   interfaces, native DSR, Claude fuzz, and default-performance controls.
3. Resume WV100 by mechanism/signature groups while preserving prior coverage;
   implement general proof-supported mechanisms rather than portfolios.

<!-- track-sched:begin -->
- Kissat creation order remains the production default; `TACTIX_ARENA_ORDER=1`
  is a CaDiCaL diagnostic arm. Unsupported formula-dependent profiles stay
  removed.
- Retain sound general repairs through downstream regressions; every commit must
  preserve N1--N4.
<!-- track-sched:end -->
