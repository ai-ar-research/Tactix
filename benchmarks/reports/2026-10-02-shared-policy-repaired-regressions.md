# Repaired shared-policy candidate: all four default gates pass

Source `f6f01982e08bfce6a8ef6dfcde1f439ce8ef81f0` passes B28+C5, Rank 1--50,
WV100 and WV2--89 under the maintained bounded regression contract. The
[summary](2026-10-02-shared-policy-repaired-regressions/summary.json) records
272 rows, 249 independently checked results, thirteen clean checker censorings
and ten retained solver-timeout exceptions. There are no scorer failures or
missing rows. **This accepts the default regression scope, not the full
engineering-preparation goal.** Full Debug and fresh Claude-owned acceptance
remain pending.

## Candidate and retained checking contract

The immutable Release binary is
`build/pin-shared-policy-feedback-release-f6f01982/tactix`, SHA256
`6283ba9b1936e3f9a690ae1b2cd26db0f0834c3e5461cb3cbf4dc857dc2c3e8a`.
The companion retains its PIN.json, each array's manifest, matrix, submission
and score, and hashes of all 272 raw result rows. All four run-local pin sets
verify against their manifests. The accepted baseline is
`benchmarks/baselines/controller-foundations-v1/results.json`, SHA256
`ef81cf019675a79a91ee431ab54e2c59630d9d069a03c2a55e44bd446c37bc9d`.

Arrays retain the accepted input sets, solver caps, memory/resources and
row-retention contract. They are full width, without throttles or node
constraints. Each selected proof pipeline has one 1,800-second wall deadline,
including conversion and final verification. A comparable completed baseline
check at time t allows candidate completion at t + max(0.30*t, 20 seconds).
No proof rejection or checker error is accepted.

| Set / job | Solver seconds | Ceiling | Checked SAT | Checked UNSAT | Censored proofs | Retained solver timeouts |
|---|---:|---:|---:|---:|---:|---:|
| B28+C5 / 1746500 | 955.943 | 999.87615 | 17 | 15 | 1 | 0 |
| Rank 1--50 / 1746531 | 7,531.471 | 8,521.52175 | 25 | 17 | 8 | 0 |
| WV100 / 1746534 | 2,760.257 | Per-row retention | 33 | 65 | 2 | 0 |
| WV2--89 / 1746535 | 4,021.675 | Per-row retention | 48 | 29 | 2 | 10 |

Times sum completed solves; WV2's ten solver timeouts are separate exceptions.
SAT models are checked against original formulas. Completed UNSAT results use
canonical independent pipelines. All 126 completed proof-cost comparisons
pass; twelve clean censorings remain within their baseline-derived bounds.
The remaining WV100 comparison has an already-censored baseline and is
inconclusive. Censored proofs are not independently verified, and no extended
proof recheck was launched to turn them into verified results.

SLURM accounts 249 task completions and 23 exit-1 failures. The latter are the
thirteen bounded checker deadlines and ten retained solver deadlines above;
the maintained row scorer distinguishes these from rejection or infrastructure
failure and accepts every set. The WV2 watcher therefore exited 1 when its
array became terminal; that scheduler observation is not a failed regression
gate.

## Broad default correspondence and individual timings

The retained [analysis](2026-10-02-shared-policy-repaired-regressions/analysis.json)
and [all-instance table](2026-10-02-shared-policy-repaired-regressions/instances.csv)
use existing results only. The helper differs from the predecessor by the
candidate run prefix; it executes no solver or checker and refuses to overwrite
an output directory. Every frozen baseline row's source digest verifies.

All **262 completed pairs** have byte-equal certificate SHA256 hashes and equal
normalized printed work statistics to both the accepted baseline and the
accepted controller-foundation candidate `12e3c4f0`. Normalization excludes
native timing/phase-cost lines and embedded fractional-second durations,
retaining work counters. All ten solver-timeout exceptions remain timeouts.
This supports unchanged recorded default behavior on the completed cases;
it does not prove every internal state or an unexercised interaction.

| Set | Baseline solved-pair seconds | Candidate | Change | Median ratio |
|---|---:|---:|---:|---:|
| B28+C5 | 960.046 | 955.943 | -4.103 | 0.9982 |
| Rank 1--50 | 7,658.828 | 7,531.471 | -127.357 | 0.9978 |
| WV100 | 2,747.495 | 2,760.257 | +12.762 | 1.0000 |
| WV2--89 | 4,047.450 | 4,021.675 | -25.775 | 1.0000 |

These are cross-window checkpoint comparisons, not matched causal measurements.
They pass the actual gate contracts; decreases do not establish an algorithmic
improvement. The largest absolute increases are:

| Set / rank | Baseline -> candidate seconds | Increase | Page faults, baseline -> candidate |
|---|---:|---:|---:|
| WV100 / 96 | 958.118 -> 1,008.042 | +49.924 | 474,151 -> 433,013 |
| WV2 / 168 | 482.234 -> 491.817 | +9.583 | 216,067 -> 2,241,218 |
| WV2 / 192 | 139.101 -> 148.449 | +9.348 | 9,554,052 -> 13,010,589 |
| Rank / 5 | 63.279 -> 71.292 | +8.013 | 1,836,163 -> 4,890,779 |
| Rank / 8 | 79.089 -> 86.976 | +7.887 | 399,832 -> 3,958,166 |
| WV2 / 247 | 35.721 -> 43.090 | +7.369 | 105,995 -> 3,678,099 |
| WV2 / 245 | 29.058 -> 36.192 | +7.134 | 101,041 -> 3,853,992 |

The largest row is 5.21% slower with equal recorded work/certificate and fewer
page faults. Its whole-process instruction count rises only about 0.014%,
while recorded cycles rise about 5.09%. Other rows have larger fault and
instruction counts with unchanged solver work. These are execution-cost leads,
not a causal attribution: whole-process counters do not separate minor/major
faults or isolate allocation, memory placement, throughput and scheduling.
In particular, unchanged search behavior does not imply all slowdown is page
faults. No tuning, profiling or unrelated campaign was launched from this
cross-window evidence.

## Feedback measurement and remaining acceptance

The repaired candidate's paired feedback measurement is already frozen in the
[prior checkpoint](2026-10-02-shared-policy-repair-revalidation-started.md):
128 runs on four synthetic inputs and both policies preserve normalized output
and certificate hashes, with independent oracles and strict enabled-stream
inspection. Additional median cost is 17.839--64.570 milliseconds. That scope
includes frontend hashing, snapshots, serialization and file writes; it does
not isolate callbacks or establish large-instance overhead.

Full Debug job 1746628 still requires the exact terminal 212-test JUnit registry,
successful exits and all 984 postflight bindings. Claude owns fresh policy legs
1746775/1746776 and targeted jobs 1746777/1746778; only scheduler/process
metadata is observed. Its terminal report must disposition fresh seeds, corpus,
effective action/configuration and lifecycle coverage, purity, replay and
independent completed proof/model oracles. Predecessor acceptance is not
substituted for this candidate.

Preserve all 795 active source bindings, immutable pins, frozen evidence and
unrelated job 1745040. No scalar reward, learning, fallback, rollback, forced
resumption, global loop budget or push is introduced. Final objective audit,
documentation reconciliation and the whole-goal Andrew handoff remain required.
