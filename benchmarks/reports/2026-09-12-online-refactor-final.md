# Online strategy and N1 closure — 2026-09-12

This report closes the post-`0060215a` online strategy follow-up. The candidate
Release executable has SHA-256
`916eae4d7e87f5602649035ac15a8c2985e83deaec2250e338a47972b998e9bf`.
It is the immutable executable used by the final matched performance runs and
Claude's bounded fuzz checkpoint.

## Result

The legacy direct random scheduler is removed. `--random-policy` now generates
validated typed programs and runs them through `OnlineStrategyExecutor`, the
same composition surface exposed to callers. Typed-program trace v3 records and
flushes each configuration before execution and replays through the executor;
legacy v1/v2 action traces remain supported.

The random generator varies rule order and repetition, action parameters,
instance granularity, explicit non-prefix selections, budgets and branch phases.
Generated fixpoints are finite. The explicit API still supports an unbounded
fixpoint when the caller supplies an external stop. Targeted stress schedules
also execute through the shared executor and interleave ordinary service actions
so proof/model oracles can reach terminal states.

The final audit repaired four general boundary defects found by the broader
surface:

- Only variables represented in the decision queue are removed and reinserted
  during demotion; retired disconnected variables remain disconnected.
- Rule advice marks only newly substituted events, rather than replaying the
  complete historical substitution log into the live queue.
- Subsolver calls have a zero-overhead ordinary path and a separately polled
  interruptible path. Zero external budgets use the latter; finite default
  calls retain their previous branch structure.
- Complete HyperBinary enumeration observes the external stop. Sweep's
  unbounded clause cap no longer requests sentinel-sized storage and starts at
  the reference 1024-clause table without scanning the whole formula per seed.

Inprocessing, Lucky and Walk remain root-only. A non-root program must apply the
already-public Restart transition before those actions. No suspension or replay
of a non-root decision trail remains.

## Local correctness

The exact candidate passed all 146 Release tests. ASan+UBSan passed all 145 core
tests under the established demo3 exclusion. Focused tests cover queue
retirement, finite and interrupted subsolver calls, complete enumeration stops,
Sweep's unbounded cap, explicit online sequencing, random parameter and
non-prefix variety, effective transitions, trace-v3 replay, v1/v2 compatibility,
zero-program traces and interruption.

## Default performance

Both gates use the established `cpu-dense-hwulab-q` contract: twelve logical
CPUs, 48,000 MiB, one solver core, 1,200 solver seconds and 16,000 checker
seconds. Only Tactix was rerun. Formula, proof, model and checker scratch use
anonymous node-local `O_TMPFILE` objects.

| Set | Candidate | Retained refactor | Ratio | Outcomes | Certificate hashes | Instances over 1.10x |
|---|---:|---:|---:|---:|---:|---:|
| B28+C5 | 958.538 s | 962.588 s | 0.995793x | 33/33 identical | 33/33 identical | 0 |
| Rank 1–50 | 8278.533 s | 8327.103 s | 0.994167x | 50/50 identical | 50/50 identical | 0 |

The manifests are
`benchmarks/runs/20260912-2105-online-refactor-final-BC` and
`benchmarks/runs/20260912-2105-online-refactor-final-ranks1-50`. All 42 SAT
models passed original-input checking. At report time, 31 UNSAT proofs had
finished independent checking; their records are valid and cleanup-clean. The
remaining proof checks continue in the background and are not needed for the
runtime conclusion.

## Independent fuzz checkpoint

Claude Code owns the final checkpoint in Slurm job `1685021`, a single
non-array allocation capped at 50 CPUs, 200000 MiB and one hour. Its immutable
campaign is `benchmarks/runs/20260912-online-final-fuzz-4f21a6cf`. It combines
saved queue-corruption and interruption cases with fresh random typed programs,
DRAT/LRAT and original-model oracles, replay and determinism, explicit
interruption, effective-transition and negative checker controls.

Claude's [independent disposition](2026-09-12-online-refactor-final-fuzz.md) is
**PASS**: the job completed `0:0` in 14:38, every
saved and fresh audit case passed, every applicable negative control was
rejected, and its node-local scratch was absent after the exit trap. The fresh
audit exercised all 26 registry actions in 144,908 typed programs, including
138,018 effective transitions and 61,687 non-prefix selected batches. It
verified 940 DRAT and 641 native LRAT proofs, checked 2,557 models, replayed
1,740 traces identically on each build and observed a changed outcome for all
1,760 seed-plus-one comparisons. The four CLI fuzz legs contributed 12,991
checked verdicts with no failure; bounded timeouts did not count as checked
cases.

The job's focused-test wrapper dropped its command, so its six focused test
invocations are not evidence. This is a disclosed harness defect rather than a
candidate failure. The exact candidate's complete local Release and sanitizer
suites above include those focused binaries and `trace_replay.sh`; the much
larger job audit independently covered replay, determinism, empty-program
traces and interruption. No follow-up fuzz job was submitted.

## Operational controls

Tier-D job `1683876` remains unchanged on committed `7a668e76`, with four CPUs,
32 GiB and a 1,200-second solver limit. The separately validated experimental
runner keeps all large formula and certificate objects anonymous and node-local
and releases them on success, failure, timeout and signal exits.

A repository storage guard checks exact usage every ten minutes. Above 700 GiB
it holds only pending jobs whose Slurm `WorkDir` belongs to this checkout. It
never suspends running jobs and releases only its own holds after usage falls
below the threshold.
