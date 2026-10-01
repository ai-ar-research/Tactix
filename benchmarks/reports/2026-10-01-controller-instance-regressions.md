# Controller DefaultPolicy: individual-instance timing investigation

2026-10-01. Analysis of existing results only; no solver, checker, profiling,
ablation or repeat experiment was launched. The decision is whether observed
slowdowns justify a DefaultPolicy/search change during controller foundations.

All **262 completed solver pairs have identical certificate SHA256 hashes and
identical normalized saved statistics**. There is no observed increase in search
or inprocessing work. Four nontrivial instances are more than 20% slower; their
saved hardware counters show much more page fault activity and lower IPC.
This is a memory/throughput investigation lead, not a demonstrated heuristic
regression or proof that page faults explain every timing difference. No solver
change or additional experiment is justified by these measurements alone.

## Authority and reproducibility

The accepted comparison is
`benchmarks/baselines/controller-foundations-v1/results.json`, SHA256
`ef81cf019675a79a91ee431ab54e2c59630d9d069a03c2a55e44bd446c37bc9d`.
Its raw September 28 result hashes were checked against the frozen authority.
Baseline solver SHA256 is
`2fc7421f4d66ed013756e176bd2d6cd0f0bda4876270830704ea2029ed0eb970`;
the manifests preserve source `d170b4a5` plus the pinned source patch.
Candidate source is `12e3c4f0e7098af15e855f995da23f6317cd0d58`, binary
`ad7e4da8fb0801b15c5916c94a536176f69f2def2a8fd477d59953d5409dfe41`.
Candidate result authorities are the four
`benchmarks/runs/20261001-controller-regression-12e3c4f0-SET/` directories.

[Read-only analysis script](2026-10-01-controller-instance-regressions/analyze.py)
recreates [all 272 per-instance comparisons](2026-10-01-controller-instance-regressions/instances.csv)
and [counters, timings, hashes and provenance](2026-10-01-controller-instance-regressions/analysis.json).
Run it with Python from any directory. It reads existing JSON only and overwrites
its two derived outputs; it never runs a solver or checker. Statistics comparison
removes timing/phase-cost lines and normalizes embedded fractional seconds;
all remaining lines, including rule counters and the verdict, match exactly.
The script verifies candidate and earlier binary identities against their manifests.

Both checkpoint and candidate use 12 CPUs, 32 GiB, cpu-dense-hwulab-q, automatic
proof generation and one solver per task. Solve limits are 1200 seconds for the
first three sets and 600 for WV2. The solver is pinned to one eligible CPU.
These are **cross-window measurements**, with different nodes/CPU placement and
co-runners; the baseline was throttled to ten tasks per array and the candidate
used full-width arrays. They do not satisfy a matched causal comparison.
Proof limits also differ: historical checks could run 16000/57600 seconds;
candidate checks use the agreed 1800-second whole-pipeline limit.

## Aggregate and individual changes

Times below are solver wall seconds, summed over pairs that completed on both
versions. Ten retained WV2 solver timeouts remain in the denominator of the
regression gate but are excluded from solved-pair ratios and summed times.
There are no solver timeout transitions in any set.

| Set | Solved pairs | Baseline | Candidate | Change | Same counters / certificate |
|---|---:|---:|---:|---:|---:|
| B28+C5 | 33 | 960.046 | 957.244 | -0.29% | 33 / 33 |
| Rank 1–50 | 50 | 7658.828 | 7662.910 | +0.05% | 50 / 50 |
| WV100 | 100 | 2747.495 | 2757.409 | +0.36% | 100 / 100 |
| WV2–89 | 79 | 4047.450 | 4066.257 | +0.46% | 79 / 79 |

Aggregate stability masks offsets: Rank loses 115.938 seconds on slower rows
and gains 111.856 on faster rows. Its largest speedup is rank 37,
`oski15a01b39s_opt`, 407.182 → 322.728 seconds (-84.454, -20.7%), again with
the same counters and certificate. Speedups deserve the same causal caution.

| Set / rank | Instance suffix (full identity in CSV) | Baseline → candidate | Added | Slowdown |
|---|---|---:|---:|---:|
| WV100 / 96 | snw_16_9_CCSpre | 958.118 → 1009.870 | 51.752 | 5.4% |
| Rank / 10 | 128_125.sanitized | 110.548 → 136.655 | 26.107 | 23.6% |
| Rank / 8 | 32_325.sanitized | 79.089 → 97.446 | 18.357 | 23.2% |
| Rank / 32 | oski15a01b01s_opt | 320.851 → 338.606 | 17.755 | 5.5% |
| WV2 / 197 | linked_list_swap_contents_safety_unwind76 | 83.378 → 100.730 | 17.352 | 20.8% |
| Rank / 5 | 9.normalised | 63.279 → 76.466 | 13.187 | 20.8% |

These are the only four rows with baseline ≥1 second and slowdown ≥20%:
Rank 5/8/10 and WV2 rank 197. This threshold selects investigation candidates;
it is not a new acceptance gate. The agreed max(30%, 20 seconds) allowance is
a **checker-cost** rule, not a newly imposed solver runtime rule.

## What the counters explain

Whole-process `perf stat` uses unqualified task-clock, cycles, instructions,
context-switches, cpu-migrations and page-faults events. These include work beyond
the internally timed solve, and instruction counts are not solver rule counts.
Page faults are not separated into minor/major faults; neither swap nor huge-page
policy was recorded. Increased faults do not by themselves prove disk paging.

| Case | Peak RSS, baseline → candidate (GiB) | Instructions change | IPC, baseline → candidate | Page faults, baseline → candidate |
|---|---:|---:|---:|---:|
| Rank 10 | 14.97 → 14.97 | +4.06% | 2.42 → 2.03 | 1,072,294 → 7,990,982 |
| Rank 8 | 11.39 → 11.38 | +4.22% | 2.47 → 2.09 | 399,832 → 5,622,104 |
| Rank 5 | 14.96 → 14.96 | +4.69% | 2.11 → 1.82 | 1,836,163 → 5,631,928 |
| WV2 197 | 2.98 → 2.97 | +2.26% | 2.44 → 2.06 | 220,080 → 3,266,647 |
| WV100 96 | 1.63 → 1.62 | +0.005% | 1.42 → 1.35 | 474,151 → 468,551 |

Task-clock/wall is approximately one CPU and measured cycles/task-clock are
about 3.8 GHz on these pairs. The slowdowns mainly appear as greater CPU cost,
not lost scheduled CPU time or a measured frequency reduction. Decomposing
cycles into instructions × cycles/instruction gives Rank 10 roughly
1.0406 × 1.1887 = 1.237, consistent with its wall ratio of 1.236.
The fault/instruction/IPC correlation is a strong lead but not a causal attribution.

Rank 5/8/10 have **zero decisions, zero conflicts and zero restarts** on both
versions. Their slowdown cannot be extra branching/restarts. For Rank 10,
internally timed solve increases 84.725 → 105.769 seconds: the inprocessing slot
adds 15.072 seconds and search/scaffolding outside it adds 5.972. Congruence's
one unchanged application adds 8.210 seconds; Reduce adds 2.566. The remaining
5.063 seconds of wall difference fall outside this internal timer; saved data
does not attribute them to a specific activity. Rank 8 similarly adds 14.069
seconds inside its slot and 3.165 outside it. Rank 5's unchanged early path is
slower across structured preprocessing, reduction and other scaffolding.

WV2 rank 197 retains 708131 decisions, 11952 conflicts, 691301782 propagations
and 335 restarts. Its timed solve adds 14.882 seconds, split approximately evenly
between the slot (+7.511) and search/scaffolding (+7.371). Congruence adds 3.891
seconds without adding any applications. No increased proof volume is observed.

WV100 rank 96 is different: essentially identical instructions and slightly
fewer faults, but ~5.3% more cycles/instruction. The timed solve adds 51.717
seconds, mostly outside the inprocessing slot (+46.354); the slot adds 5.362.
All 7077999954 propagations, 1220557 conflicts and 39296 restarts are unchanged.
Page faults do not explain this row from the available measurements.

## Cross-check, checker costs and next action

The already completed `eba47800` controller campaigns are a diagnostic third
point, not a replacement baseline. They also match all 262 completed certificates
and normalized statistics. Rank 10 previously took 107.997 seconds on node018
with 271264 faults; the current run on node018 took 136.655 with 7990982 faults.
That rules out a simple claim that node018 is uniformly slower. Across cases
with baseline ≥10 seconds, current node017 and node018 median ratios are 1.0011
and 0.9999 respectively. Kernel memory conditions, page granularity, placement,
co-runners, allocation/layout changes and per-instruction code costs remain
unseparated hypotheses; the snapshots cannot settle them.

Solver times above exclude checker execution. All four existing terminal
regression scores remain ACCEPTED, with zero checker failures/rejections or
significant checker-cost regressions. They contain 249 independently checked
rows, 13 clean censored proofs and ten retained solver timeouts. A censored proof
is not independently verified. For example Rank 32 and WV2 rank 197 have identical
UNSAT proof bytes but historical checks took 3106.504 and 2626.569 seconds;
current checks are cleanly censored at 1800 seconds. This does not show faster
checking or a checker failure.

Keep the DefaultPolicy gate accepted and make no heuristic change from this
analysis. If a later authorized performance investigation is needed, start with
Rank 5/8/10 and WV2 rank 197's memory/fault behavior, then WV100 rank 96's search
throughput. A matched window with memory/placement diagnostics would be needed
before assigning causality. No such experiment is launched or newly authorized
by this report. Full Debug completion is a separate pending acceptance check.
