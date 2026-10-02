# Shared-policy candidate: default regressions and feedback measurement

2026-10-02. Candidate source `0531c2f1fb153f0d6be240041eda41fa835427a8`.
The four DefaultPolicy regression gates pass their agreed bounded-checking
contract. The paired frontend feedback measurement completes with preserved
deterministic signatures. Full Debug and the Claude-owned fresh fuzz/coverage
acceptance remain pending; this is not whole-goal acceptance.

This checkpoint collects already-submitted work and analyzes saved results.
It changes no solver, action contract, reward, checker or benchmark composition
and launches no additional experiment. The submission and source-boundary
review are frozen in the [candidate preparation report](2026-10-02-shared-policy-validation-started.md).

## Candidate and comparison authority

The immutable production Release solver SHA256 is
`f263a6afb42ecefba8003634295c191d4ff501f37eb451d72ee20db95ef38769`.
The frozen baseline is `benchmarks/baselines/controller-foundations-v1/results.json`,
SHA256 `ef81cf019675a79a91ee431ab54e2c59630d9d069a03c2a55e44bd446c37bc9d`.
The baseline's original per-instance result hashes were verified. Comparison
also includes accepted controller-foundation source `12e3c4f0`.

The [companion](2026-10-02-shared-policy-regression-feedback/) preserves all
272 candidate result bytes in four archives, passing maintained-scorer output,
run manifests, matrices, launch/submission records, terminal accounting,
per-instance analysis and overhead evidence. Its `source-sha256.json` binds the
original files; the manifests bind solver, checker, runtime and input identities.
The copied measurement runner retains its exact submitted bytes.

## Four default regression gates

All 272 array tasks are terminal and have the expected result identity. The
maintained `tools/bench/controller_regression.py score` verifies frozen pins,
resources, row retention, independent outcomes, checker cost and exact aggregate
ceilings. All four scores have `accepted: true`, no missing rows and no failures.

| Set / array | Rows | SAT models checked | UNSAT proofs checked | Clean checker censoring | Solver timeouts | Completed solver seconds | Ceiling |
|---|---:|---:|---:|---:|---:|---:|---:|
| B28+C5 / 1746173 | 33 | 17 | 15 | 1 | 0 | 958.672 | 999.87615 |
| Rank 1–50 / 1746174 | 50 | 25 | 17 | 8 | 0 | 7575.148 | 8521.52175 |
| WV100 / 1746175 | 100 | 33 | 65 | 2 | 0 | 2805.256 | — |
| WV2–89 / 1746176 | 89 | 48 | 29 | 2 | 10 | 4078.255 | — |

Total: **249 independently verified results, 13 censored proofs and 10 retained
solver timeouts**. All completed solver verdicts match expectations. There are
no lost retained solves, new solver-timeout transitions, model rejections,
proof rejections or checker failures. The 23 tasks with SLURM exit `1:0` are
exactly the 13 checker deadlines and 10 retained solver timeouts; scheduler
failure labels are not additional unexplained failures.

Each proof uses one 1800-second wall deadline for its entire selected pipeline.
The recorded maximum is 1800.001 seconds, including millisecond reporting and
deadline cleanup. Stage budgets are not reset. Candidate checking must remain
within baseline plus `max(0.30 * baseline, 20 seconds)` when a comparable
completed baseline exists. All 126 completed UNSAT pipelines are within that
bound; 12 censored pipelines expire within their baseline-derived bounds.
The remaining WV100 comparison has a censored baseline and is inconclusive.
There is no significant checker-cost regression.

Censored ranks are B28+C5 33; Rank 18, 22, 27, 31, 32, 33, 35, 37;
WV100 60, 90; WV2 145, 197. The retained WV2 solver-timeout ranks are
149, 155, 162, 166, 196, 199, 201, 203, 221, 224. These remain in the gate's
denominator. Censored proofs are not independently verified, and this bounded
acceptance is not formal competition/full-certificate acceptance.

## Default correspondence and individual timings

The read-only [analysis script](2026-10-02-shared-policy-regression-feedback/analyze.py)
verifies baseline source hashes and candidate/foundation binary identities.
It emits [all-instance comparisons](2026-10-02-shared-policy-regression-feedback/instances.csv)
and [saved counters and provenance](2026-10-02-shared-policy-regression-feedback/analysis.json).
Reproduction requires `--output NEW_DIRECTORY` and refuses an existing output
directory, preserving frozen artifacts. It executes neither solvers nor checkers.
All **262 completed pairs** have
identical certificate SHA256 hashes and normalized printed work statistics to
both the frozen baseline and accepted controller foundation. Normalization
removes timing/phase-cost lines and embedded fractional-second durations;
recorded work counters remain compared. This is broad default behavior evidence,
not proof of every internal state or unexercised operation.

| Set | Baseline solved-pair seconds | Candidate | Change | Same counters / certificate |
|---|---:|---:|---:|---:|
| B28+C5 | 960.046 | 958.672 | −0.14% | 33 / 33 |
| Rank 1–50 | 7658.828 | 7575.148 | −1.09% | 50 / 50 |
| WV100 | 2747.495 | 2805.256 | +2.10% | 100 / 100 |
| WV2–89 | 4047.450 | 4078.255 | +0.76% | 79 / 79 |

These are cross-window checkpoint comparisons, not matched causal measurements.
The baseline used throttled arrays; this candidate uses full-width placement.
Node, CPU placement and co-runners differ. Ten retained timeouts are excluded
from completed-pair sums, but not regression scoring. Aggregate changes are
not evidence of algorithmic improvement or a page-fault explanation.

Seven rows with baseline time at least one second are at least 20% slower.
This selects investigation candidates, not a new solver acceptance threshold:

| Set / rank | Baseline → candidate seconds | Increase | Page faults, baseline → candidate |
|---|---:|---:|---:|
| Rank / 5 | 63.279 → 76.738 | 21.3% | 1,836,163 → 4,849,379 |
| Rank / 8 | 79.089 → 103.884 | 31.4% | 399,832 → 8,690,283 |
| WV100 / 82 | 6.927 → 11.034 | 59.3% | 204,994 → 868,370 |
| WV2 / 192 | 139.101 → 210.520 | 51.3% | 9,554,052 → 33,060,738 |
| WV2 / 236 | 9.027 → 10.886 | 20.6% | 46,849 → 1,271,548 |
| WV2 / 240 | 13.383 → 21.304 | 59.2% | 96,972 → 2,321,417 |
| WV2 / 244 | 23.346 → 40.786 | 74.7% | 97,309 → 5,832,372 |

Recorded work and certificates are unchanged for all seven. Six have zero
decisions and conflicts; WV100 rank 82 has zero decisions and one conflict.
The timing differences therefore do not reflect additional recorded search work.
Higher page-fault counts and lower instruction throughput support a memory/unit-
cost lead. Whole-process counters do not separate minor and major faults,
and placement differences prevent causal attribution. In particular, unchanged
solver work does not establish that all slowdown is page faults.

WV100 rank 96 contributes the largest absolute increase, 958.118 → 1043.099
seconds (+84.981, +8.9%), with unchanged 3,936,628 decisions, 1,220,557 conflicts,
7,077,999,954 propagations and 39,296 restarts. Aggregate stability includes
offsets: Rank adds 80.490 seconds on slower rows and saves 164.170 on faster
rows. No default heuristic change or additional profiling campaign is inferred
from these cross-window results.

## Measured frontend feedback cost

Job **1746397** completed `0:0` on node018. Four declared synthetic inputs and
both policies produce eight comparisons: seven measured off/on pairs plus one
discarded warmup pair each, with alternating order in one allocation. All
128 solver runs complete; none times out. The random controller uses seed 37
and restart period 2. Original-CNF SAT models are checked every time; both
distinct UNSAT certificates pass their independent pipelines. All 64 enabled
streams pass strict inspection. Recomputed summaries and per-pair identities
agree with the measurement's raw rows.

| Input | Policy | Off median ms | On median ms | Added ms | Ratio of medians |
|---|---|---:|---:|---:|---:|
| Binary chain SAT | Default | 6.018 | 45.843 | 39.825 | 7.62 |
| Binary chain SAT | Random | 5.959 | 23.402 | 17.443 | 3.93 |
| Planted 3-SAT | Default | 5.835 | 28.972 | 23.137 | 4.97 |
| Planted 3-SAT | Random | 6.301 | 26.559 | 20.258 | 4.22 |
| PHP 4/5 SAT | Default | 5.939 | 36.168 | 30.229 | 6.09 |
| PHP 4/5 SAT | Random | 5.632 | 24.668 | 19.036 | 4.38 |
| PHP 6/5 UNSAT | Default | 5.781 | 36.259 | 30.478 | 6.27 |
| PHP 6/5 UNSAT | Random | 38.932 | 103.316 | 64.384 | 2.65 |

Recording the complete frontend stream costs **17.443–64.384 ms** in median
additional wall time. Enabled streams contain 420,226–12,735,482 bytes.
Large ratios on short solves are real full-recording cost and must not be
described as negligible. Measurement includes frontend provenance hashing,
snapshots, serialization and anonymous-file writes; it does not isolate callback
cost or establish large-instance feedback performance. Checker and inspector
execution is outside the measured solver process. Blocking wait with a wall
alarm avoids short-run polling quantization.

Both arms have identical native timing-output settings and the same environment.
The runner records `TACTIX_STATS_TIMING=1`, but that name has no reader in
`src/main.cpp`; it is not evidence of a special timing mode. Timing output is
excluded from deterministic signature comparison. All off/on repeats retain
identical printed non-timing statistics/model tokens and certificate hashes per
input/policy. Pins were verified before and after execution and again during
collection. This satisfies the declared measurement scope; broader extra-observe,
request-selection and lifecycle purity still require the owner's targeted
acceptance. No scalar reward is computed.

## Remaining acceptance and handoff

Full Debug **1746172** remains running, with 211/212 tests observed passed;
`demo3_scripted_default` is still outstanding. Suite acceptance requires the
terminal JUnit registry, exit status and postflight pin verification, not the
observed pass count. Claude's fresh default/controller jobs **1746357/1746358**
are running, with targeted coverage **1746454** pending at the saved queue
observation. The parent has not inspected private campaign content. A passing
self-contained terminal owner report must disposition both ≥100,000-seed legs,
retained corpus and every declared coverage/purity/replay target.

Next collect those terminal authorities and repair demonstrated gaps without
weakening gates. Reuse these unchanged candidate regressions and measurement;
rerun affected acceptance if a solver repair changes their snapshot or scope.
Complete the full-objective evidence audit before the final Andrew handoff.
The [interim requirement crosswalk](2026-10-02-shared-policy-regression-feedback/goal-audit.json)
records thirteen objective requirements without claiming universal acceptance.
All fifteen documentation/source-boundary checks pass in 19.74 seconds; host
progress validation has zero errors/warnings. Six existing documentation-size
reserve warnings remain without automatic compaction. Andrew's three-page
TeX/PDF compiles cleanly on both final passes.
Reward choices and learning, unmet formal targets and deferred Tier-D checker
investigation remain as documented in the plan. No push, source change, active
pin rebuild, private campaign inspection or benchmark deletion occurs here.
