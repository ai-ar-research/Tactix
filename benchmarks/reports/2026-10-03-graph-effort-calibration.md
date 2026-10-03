# Native graph constructor and effort-meter calibration

Date: 2026-10-03. Status: diagnostic evidence complete; production/N4 acceptance OPEN.
Source baseline: d210d222 plus the unchanged uncommitted constructor/guidance snapshot.

## Decision

Do not derive a production GraphWork allowance from one constant work/second rate.
These measurements show material dependence on stage and graph structure. Retain the
explicit experimental profile as a probe input, with production enablement off.
No new target is closed: rank 991 remains the one independently checked candidate
among the 36/44 targets. No historical benchmark comparison was rerun.

## Constructor control

SLURM array 1747589: 12 paired cases, all tasks COMPLETED, exit 0:0. Each task
used one CPU, 1 GiB, 12-minute wall limit and 300 seconds per arm on cpu-q,
without array throttling or node selection. This is a standalone graph calculation,
not a SAT target run; n <= 384 justifies its smaller allocation.

Compile the unmodified LymphoSAT constructor as a diagnostic control alongside the
native header implementation. Neither reference code nor an external solver stage
is added to production. Reference source SHA-256:
`053e1974f5f6e62144f9244a7ef54e78a4244e362bfc83d984fd0da387c0d837`.
The immutable binary is `build/pin-graph-constructor-calibration-d210d222-01/calibrate`,
SHA-256 `399e366ccee7aeca2d58684d105d58ab6cf2ab2aa01bb7d685c1fdc2d76e9ee9`.

The preregistered matrix has n = 64, 128, 256, 384 and three graph types:
planted sparse/dense (separation floor(n/3), permitted-edge sampling 1/16 and 1/4),
and complete graphs with separation 2 (impossible). Planted witnesses were never
passed to either arm. Arm order alternates by case. Native stage allowances are
unchanged explicit experimental inputs, not tuned defaults: construction 100M,
window 5B, repair 1B, anneal 500M, final repair 1B and reflection 1M work units;
50M DFS nodes, 12 starts, 28 early choices, maximum DFS n=140, and 100k stall
anneal attempts. The reference retains its original time/size scheduling.

Every returned labeling was independently rechecked as a permutation, with the
number of violated edges and squared-deficit cost recomputed from the input.
All 24 arms validated. Both implementations solve the same seven of eight planted
cases; neither solves an impossible graph. On dense n=384 both are incomplete,
with 318 violated edges native versus 421 reference. No reference-only success
appeared in this finite set; this is not universal parity or target SAT evidence.

| Case | n | Graph | Reference seconds / bad edges | Native seconds / bad edges |
|---|---:|---|---:|---:|
| 0 | 64 | planted_sparse | 11.158 / 0 | 8.636 / 0 |
| 1 | 64 | planted_dense | 17.366 / 0 | 8.790 / 0 |
| 2 | 64 | complete_graph | 72.000 / 63 | 15.200 / 63 |
| 3 | 128 | planted_sparse | 26.517 / 0 | 9.160 / 0 |
| 4 | 128 | planted_dense | 46.170 / 0 | 10.082 / 0 |
| 5 | 128 | complete_graph | 72.137 / 127 | 13.908 / 127 |
| 6 | 256 | planted_sparse | 9.815 / 0 | 0.677 / 0 |
| 7 | 256 | planted_dense | 4.008 / 0 | 5.636 / 0 |
| 8 | 256 | complete_graph | 36.060 / 255 | 11.117 / 255 |
| 9 | 384 | planted_sparse | 1.159 / 0 | 0.573 / 0 |
| 10 | 384 | planted_dense | 54.032 / 421 | 13.789 / 318 |
| 11 | 384 | complete_graph | 54.329 / 383 | 10.886 / 383 |

Timing limitations: the reference timer includes conversion into its Instance;
the native Shape already exists. Reference scheduling uses wall time, native uses
effort, and tie/random trajectories differ. Native counts exclude Kernel metering
in this first study. Consequently these numbers do not establish solver speedups
or faithful production scheduling. Synthetic solved counts do not choose defaults.

## Meter control

SLURM array 1747601, submitted 2026-10-03 15:50:20 America/New_York
(19:50:20 UTC): six paired cases, all tasks COMPLETED, exit 0:0, same resources.
The date-filtered accounting distinguishes recycled older records bearing the
same job ID. No task was restarted. The maintained watcher terminated normally.

The same native algorithm/input/seed/quotas runs under a simple counter and a
Kernel interruption/cumulative-budget/statistics meter. The latter binary,
`build/pin-graph-meter-calibration-d210d222-01/calibrate`, SHA-256
`7ec7208fd54cb88b622728abf721b328ca1b58ff1728ae6a12e6f61423229c93`, links verified
unchanged solver objects and replaces only the measurement driver. The generated
source and full compile/link provenance are retained. No production source changed.

All 12 arms have independently checked permutations/objectives. In all six pairs,
all output fields except elapsed times agree exactly, including labels, work,
exhaustion, nodes, rounds and annealing attempts. Four complete graphs measure
saturated repair phases; two single-edge graphs with separation n are impossible
and also saturate window search. The sparse cases are cost stressors, not a
production recognizer/solve-quality distribution.

| Case | n | Graph | Simple seconds | Kernel seconds | Kernel/simple |
|---|---:|---|---:|---:|---:|
| 0 | 64 | complete_graph | 13.611 | 15.695 | 1.153 |
| 1 | 128 | complete_graph | 12.119 | 13.963 | 1.152 |
| 2 | 256 | complete_graph | 12.337 | 15.480 | 1.255 |
| 3 | 384 | complete_graph | 12.192 | 14.527 | 1.192 |
| 4 | 64 | impossible_single_edge | 75.397 | 89.426 | 1.186 |
| 5 | 128 | impossible_single_edge | 74.650 | 86.274 | 1.156 |

With the Kernel meter, dense-graph repair processes 159.7–186.0 million units/s,
versus 35.6–38.4 million on the sparse stressors. Annealing is 155.4–170.8 versus
31.6 million units/s. Sparse window search is 254.4–274.4 million units/s.
A work unit charges differing primitive operations; sparse annealing spends more
of its time in per-attempt random/control work and fewer charged adjacency visits.
This source-based explanation is consistent with the timings, not a sampled
instruction-level attribution. No unbounded loop or accounting failure was shown.

The paired total increase is 15.2–25.5%. It measures the compiled metered driver,
including resulting compiler/code-layout effects, rather than a universal isolated
per-poll overhead. Kernel state has no formula. Recognition, formula/cache effects,
Boolean completion, proof/model publication and full-solver instrumentation remain
excluded. This is one observation per pair, not a statistical timing guarantee.

## Next engineering decision and acceptance

Reference scheduling uses a 70-second DFS bound (also 50M nodes), then base repair
time 60/30/45 seconds by size, allocated 70% repair, 30% anneal and 20% final repair;
stalled repair invokes up to one second of annealing. Native staged work limits
and 100k stall attempts are not yet justified equivalents. Next review a
deterministic structural dose or richer cost accounting against these mechanisms;
validate real solver cost and useful work before production adoption. Preserve
bounded interruption, public profile selection and raw observation/replay.
Do not infer N4 acceptance from seven successful synthetic controls.

The diagnostic full Debug run still has 216/217 passing with the final scripted
example live; it is not immutable current-pin acceptance. Claude's designated
owner report remains PREPARING and reports no accepted fuzz evidence. Queue inspection also shows job 1747588 (`wgf-smoke`) running; its owner
disposition is pending and private artifacts were not inspected. Source
remains frozen until the owner binds its immutable snapshot. Full current-pin
Debug, effective both-policy fuzz/replay, N1–N4 review and B28+C5, Rank1–50, WV100,
WV2–89 preservation remain required. Reward and training remain deferred.

## Evidence and retention

- `benchmarks/runs/20261003-graph-constructor-calibration/`: manifest, 12 paired
  results, summary, original graphs, arm logs, terminal accounting, build manifest,
  retained driver/runner and artifact hashes.
- `benchmarks/runs/20261003-graph-meter-calibration/`: equivalent six-pair evidence,
  generated-build provenance, driver builder/runner and artifact hashes.
- Both immutable pins and their source/link bindings are retained. All run bindings
  were verified during collection. These pure-graph runs created no CNF/proof
  residue; no files were deleted. Private fuzz artifacts were not inspected.
- Andrew handoff: `report-to-andrew/20261003T195500Z-graph-effort-calibration/`.
