# Online strategy and N1 refactor closure — 2026-09-13

The accepted solver source is commit `a08f9f6a841c22f006af872f6f162ca606cef3bd`.
Its Release executable has SHA-256
`b63f30031229a36b9fea46e37590e892ab4a178a1239357ff3f80557548e1413`.
The online-configurable strategy and N1 refactor is closed at this candidate;
the current Tier-D run is the background N3 scale check.

## Result

- `DefaultPolicy` is decomposed into typed branching/analysis, restart/mode,
  database, guidance and inprocessing settings while retaining its static hot
  search path.
- The shared C++ and C executor reaches all 26 registered actions with
  validated per-call settings, explicit legal instances, selected batches,
  passes, rounds, closures, reordered or repeated sequences, scoped fixpoints,
  budgets, stops, and safe boundary-level reconfiguration/resumption.
- Inprocessing, Lucky and Walk have a level-zero action boundary. A caller at a
  search state explicitly composes `Restart`; the executor does not suspend or
  reconstruct a non-root decision trail.
- Rule-side heuristic thresholds, limits and scheduling choices are typed
  strategy data. Rule modules retain semantic, representation and diagnostic
  constants. The static configuration, ownership and option audits pass.
- `--random-policy` is the sole production random strategy. It generates and
  replays typed online programs across settings, orders, repetitions,
  granularities, budgets, phases, non-prefix instances and effective
  transitions. Trace v3 owns typed replay; historical v1/v2 traces remain
  readable.
- Explicit N1 surfaces include C1/C2 victims, CCE large and duplicate-binary
  victims, complete HyperBinary/HyperTernary results, exact Factor grids and
  individual Congruence equivalences, units and supported clashes.
- Lucky instances 6 and 7 add bounded general GF(2) and one-hot GF(3) model
  search. Candidate SAT answers are accepted only after the complete live CNF
  is satisfied.

The detailed architectural history remains in the
[refactor gate](2026-09-12-refactor-n1-gates.md),
[state-boundary follow-up](2026-09-12-n1-state-boundary-followup.md), and
[configurable-strategy closure](2026-09-13-configurable-strategy-closure.md).

## Final correctness checkpoint

Claude-owned job `1687182` ran one immutable full checkpoint on
`cpu-dense-hwulab-q` with 100 logical CPUs, 400000M and a one-hour limit. It
completed `0:0` in 16:12. The authoritative
[terminal report](2026-09-13-structural-resume-claude-fuzz/terminal-report.md)
records PASS with no solver failure and no missing required feature.

The checkpoint closed every defect from job `1687094`:

- the excluded one-hot candidate `d000437` and every other S1/S2 case now
  produce checked verdicts without invalid models or Debug invariants;
- all 646 mode-5 cases preserve the assignment accounting invariant, including
  383 nonterminal reconfigurations split across Lucky 6 and 7;
- all 866 online SAT exports reflect status 10, are stable across repeated
  export, and preserve solver state for a subsequent API advance.

It also passed all four F1 exact replay cases, credited all F2 interruption
lanes, every H4 admission target, both structural-model families, negative
controls, 18 focused sanitizer runs and all four stock fuzz legs. It recorded
4,612 identical random replays and transitions, 2,833 verified DRAT proofs,
2,406 verified native LRAT proofs, and valid models in every verdict-bearing
lane. No proof was rejected and no checker timed out. Node cleanup found zero
new scratch entries, session residue or failure bundles.

Codex separately reproduced the original failures and repaired them in
`a08f9f6a`: one-hot candidates may not override a root-false selected
indicator, online model export no longer mutates kernel assignments, and
online status recognizes a complete active-variable assignment. Release
passed 148/148 tests. The sanitizer-enabled Debug suite passed all 147 relevant
tests; the explicitly long scripted demo was stopped after 1:48 rather than
duplicated as an acceptance wait.

## Default preservation

Only Tactix was rerun. Both arrays used the established
`cpu-dense-hwulab-q` contract: 12 logical CPUs, 48000M, one solver thread, a
1,200-second solver limit and a 16,000-second checker limit. Formula, proof,
model, streams and checker intermediates used anonymous node-local files.

| Set | Prior `8f9161f4` | Final `a08f9f6a` | Ratio | Solver outcomes |
|---|---:|---:|---:|---:|
| B28+C5 | 948.484 s | 950.837 s | 1.002481x | 33/33 expected |
| Rank 1–50 | 8,041.173 s | 8,123.812 s | 1.010277x | 50/50 expected |

Every certificate hash and every non-timing solver statistic is identical for
all 83 pairs. B28+C5 has no per-instance drift above 5%. Rank 1–50 has three
timing-only drifts above 5% (ranks 6, 20 and 28); their identical trajectories,
certificates and peak RSS make the aggregate change consistent with runtime
noise. Maximum solver RSS is unchanged at 3,257,904 KiB and 16,016,628 KiB.
All B28+C5 final checks and 42 of 50 Rank checks were complete when this report
was written; remaining proof checks continue in the background by explicit
user direction and do not block independent work.

## Tier-D submission and storage

Cleanup validation job `1687183` passed checked SAT, checked UNSAT, forced
crash and forced timeout paths on node002. Every formula, proof, model and
checker file used Linux `O_TMPFILE`; no named CNF or certificate remains.

Array `1687184` now runs the complete 1,166-instance Tier D from the accepted
commit. Its immutable manifest is under
`benchmarks/runs/20260913-0905-tierd-refactor-final-a08f9f6a/`. It requests
`cpu-q`, four CPUs, 32 GiB, a 1,200-second solver timeout and a 16,000-second
checker timeout, with no array throttle or node constraint. Existing Tier-D
and proof-checking jobs remain unchanged.

Storage guard PID `1458620` measures the complete repository every ten minutes.
Above 700 GiB it holds only pending jobs submitted from this checkout; it never
pauses running work and releases only its own holds after recovery. Usage was
114 GiB with no holds at submission.

## Deferred work

The frozen winner set contains ten LymphoSAT-only SAT and ten Satsuma-only
UNSAT losses. The matched rerun reproduced all 20 Tactix timeouts and all 20
checked winner solves. General GF(2) and GF(3) mechanisms now cover the SAT
side. The next solver work is a proof-producing general mechanism for the
UNSAT symmetry families; benchmark dispatch, Kissat-only tuning and sequential
portfolios remain excluded.
