# Rank-27 cluster timing stability

## Decision

The 300--580 second Rank-27 spread is execution-environment noise, not solver
nondeterminism, and Kissat experiences the same effect.  The dominant
mechanism is shared-node cache/memory service rate: immutable runs execute the
same instructions and logical trajectory but consume materially more cycles at
lower IPC when memory-intensive solvers share an L3 domain.  CPU descheduling,
average core frequency, ordinary proofless I/O, and node identity do not
explain the spread.

The completed controlled isolation experiment proves the environmental cause,
but whole-node reservation is not a viable future policy for a two-node
cluster: reserving 96 physical cores for one single-threaded solver trades
timing stability for an unacceptable loss of throughput.  No future timing or
diagnostic job should request whole-node isolation.  Routine development
decisions instead reserve one L3 domain per timed solver with `-c 12` and a
48-GiB scheduler allocation while retaining the 16-GiB solver process limit,
use fixed-core affinity and local scratch, and use simultaneous paired
candidate/baseline jobs with replicated ratio/spread reporting on ordinarily
shared nodes.  Six such current-Tactix controls reproduce the exact accepted
trajectory in 367.045--372.354 internal seconds, median 368.307 and 1.441%
full spread.  All are below 400 seconds.  The user approved this protocol for
`DEVELOPMENT_PLAN.md`, including three paired waves only for decision-grade
performance evidence and explicitly excluding fuzzing from repetition.

Authoritative raw evidence is
`benchmarks/runs/20260826-1030-rank27-timing-stability/`.  Its immutable Tactix
pin is SHA-256 `4b3f7467...bc5e6a` from pushed commit `d8da6a1`, the Rank-27
input is SHA-256 `5435b1b2...f7bb4e9`, and terminal summary SHA-256 is
`99c43cf...abab0b`.  Kissat timing authority is
`benchmarks/runs/20260826-1430-rank27-kissat-c4-timing-variation/`, whose
terminal summary SHA-256 is `9957d825...8c9220`.

## Historical contention reconstruction

SLURM accounting was reconstructed across all partitions sharing node017 and
node018, not only the submitting partition.  Selected exact-binary controls are:

| Evidence | Node | Other allocated CPUs, time-weighted | Solver seconds |
|---|---:|---:|---:|
| exact `60fe8e...`, job 1647411 | 017 | 79.50 | 553.948 |
| exact `60fe8e...`, job 1647447 | 018 | 0.00 | 394.107 |
| exact BVE pin, job 1647569 | 017 | 60.92 | 419.772 |
| exact BVE pin, jobs 1647573/1647575/1647654 | 018 | 0.00 | 371.711 / 375.797 / 372.969 |
| current predecessor/candidate, jobs 1647806/1647807 | 017 | about 5 | 432.912 / 426.802 |
| exact Kissat 4.0.4, job 1624999 task 2150, `-c 4` | 018 | 88.00 | 278.187 |
| exact Kissat 4.0.4, job 1646252 task 430, `-c 2` | 017 | 73.90 | 320.307 |

The original accepted proof-producing job 1646031 ran on idle node017 and
solved in 399.152 seconds, so node017 is not intrinsically a 500-second node.
The selected historical slow episodes overlapped `cpu-dense-hwulab-q` jobs;
none of those specific intervals carried measured `cpu-dense-preempt-q` load.
Both partitions use the same nodes, however, so any future occupancy audit must
remain partition-independent.

Total CPU stayed approximately equal to wall time even in slow runs.  This
rules out loss of scheduled CPU time as the large owner, but not shared LLC,
memory-bandwidth, and turbo-budget interference.

The two frozen Kissat rows use binary SHA-256 `3aa8e41f...d49c` and the same
immutable Rank-27 input.  Both return valid UNSAT and generate the identical
1,320,603,112-byte proof at SHA-256 `ca29c36...d6d3f`; both proofs pass the
independent DRAT-to-Cake pipeline and leave zero residue.  Their 42.120-second
spread is 15.14%.  Because the faster run actually overlaps more allocated
CPUs, raw node occupancy is not a sufficient predictor.  CPU placement,
co-tenant working set, and cache-domain sharing must be recorded.

## Kissat reproduction and L3 mechanism

Campaign `20260826-1430-rank27-kissat-c4-timing-variation` repeats exact
current instrumented Kissat binary SHA-256 `b6a9b1d0...644d5` on the immutable
Rank-27 input.  Proof output is deliberately disabled and the CNF is staged on
local NVMe, isolating compute/memory service from proof and shared-filesystem
I/O.  All 15 jobs return UNSAT exit 20 and exactly reproduce 4,120,691
conflicts, 171,577,295 decisions, 2,665,923,683 propagations, and all selected
inprocessing counters.  Cleanup and certificate residue are zero.

| Kissat protocol | Wall seconds | Median | Full range |
|---|---|---:|---:|
| six simultaneous `-c 4` | 253.90, 259.66, 297.74, 298.48, 300.11, 321.32 | 298.11 | 67.42 (22.62%) |
| three temporally separated `-c 4` | 258.60, 260.02, 260.12 | 260.02 | 1.52 (0.585%) |
| six simultaneous `-c 12`, one L3 each | 252.75, 257.18, 258.87, 259.39, 259.50, 260.76 | 259.13 | 8.01 (3.09%) |

The topology probe supplies the missing causal detail.  Each EPYC 9454 L3
domain covers six physical cores.  Slurm packs ordinary `-c 4` jobs two
physical cores at a time.  On otherwise lightly allocated node018, solvers
pinned to cores 0, 2, and 4 share one L3 and take 297.74--300.11 seconds at
IPC 1.825--1.841; the simultaneous core-6 solver lands in the next L3 and
takes 259.66 seconds at IPC 2.110.  A separated core-0 repeat recovers to
258.60 seconds and IPC 2.120.  Across the six-job wave, perf instructions stay
at approximately 2.071 trillion while IPC spans 1.706--2.159.  This is exact
work receiving different cache/memory service, not different search.

`-c 12` exposes masks such as `0-5,96-101` and reserves exactly one complete
L3 domain.  Six simultaneous controls occupy separate domains across both
nodes and collapse the pathological spread while retaining ordinary shared
scheduling.  The 192-logical-CPU nodes contain 16 such domains, so up to 16
timed solvers still fit per node; this is not whole-node isolation.

## Current controlled experiment

The original twelve independent jobs and six post-terminal shared-L3 controls
each ran one solver invocation.  There was no node
constraint or array throttle.  The ambient pair used ordinary 16-GiB
allocations.  Isolated jobs used the partition-supported
`--exclusive=user --mem=0`; the all-memory reservation prevents another
same-user job fitting, while exclusive-user ownership excludes other users
across partitions.  A shell `ulimit` independently enforced the actual 16-GiB
process contract.  Bare `--exclusive` is unavailable on this partition.

Every solver returns UNSAT exit 20.  All proof-off runs reproduce normalized
no-timing SHA-256 `10372d94...f1ffd0`; all proof-on runs reproduce their
mode-specific SHA-256 `4cfab3ac...df3f0`.  Every result cleans its staged CNF,
proof, checker output, and scratch; certificate residue is zero.

### Contention mechanism

| Current-binary proof-off condition | Internal s | Cycles | Instructions | IPC |
|---|---:|---:|---:|---:|
| isolated median, 3 primary repeats | 367.312 | 1.390e12 | 2.509e12 | 1.805 |
| two simultaneous solvers on node017 | 419.659 | 1.588e12 | 2.509e12 | 1.580 |

Sharing with only one other Rank-27 solver adds 52.347 seconds and 14.231%
cycles while instructions change by only 0.025%.  Both observations have one
CPU migration and the same approximately 3.781-GHz average reported by perf.
Cache-miss count changes only modestly, but lower IPC shows that shared misses
and memory traffic receive slower service.  The cause is therefore shared
memory-hierarchy service rate, not changed search work, CPU migration,
descheduling, or average frequency.

### Shared L3-domain validation

After whole-node isolation was rejected as policy, array `1648205` ran six
current-Tactix proof-off jobs with ordinary shared scheduling and `-c 12`.
Every allocation owns one complete L3 domain; three land on each node.  Every
run reproduces normalized no-timing SHA-256 `10372d94...f1ffd0`, returns UNSAT
exit 20, and leaves zero residue.

| Shared `-c 12` current Tactix | Internal seconds | Wall seconds |
|---|---:|---:|
| six observations | 367.045, 372.354, 368.016, 368.450, 368.164, 368.731 | 367.55, 372.86, 368.54, 368.96, 368.67, 369.24 |
| median | 368.307 | 368.815 |
| full range | 5.309 (1.441%) | 5.310 (1.440%) |

All six observations are below 400 seconds.  Thus one-L3 reservation is a
measured shared-node solution, not merely an inference from the earlier
whole-node experiment.

### Diagnostic isolated calibration

| Mode/protocol | Internal seconds | Median | Full range |
|---|---|---:|---:|
| isolated proof-off, primary | 367.092, 371.637, 367.312 | 367.312 | 4.545 (1.237%) |
| isolated proof-on, primary | 370.893, 369.785, 371.407 | 370.893 | 1.622 (0.437%) |
| isolated proof-off, all CPU-allocation variants | seven runs, 365.171--371.637 | 367.295 | 6.466 (1.761%) |

All seven isolated proof-off observations and all three isolated proof-on
observations are below 400 seconds.  Both nodes appear in the primary series.
No best-of selection is used.

### `-c 2`, SMT, `-c 4`, and `-c 12`

The nodes are dual-socket AMD EPYC 9454 systems with 48 cores/socket and two
threads/core.  Effective cgroup masks prove:

- ordinary `-c 2`: `0,96`, one physical core;
- ordinary `-c 4`: `0-1,96-97`, two physical cores;
- `-c 2 --hint=nomultithread`: also `0-1,96-97` on this cluster.

Isolated ordinary-`-c 2` runs are 366.609 and 365.171 seconds.  Isolated
ordinary-`-c 4` runs are 368.081 and 367.295 seconds.  Their median difference
is 1.798 seconds (0.49%), within the complete isolated spread and not a
throughput effect.  Tactix is single-threaded.  `-c 4` is clearer than `-c 2`
for ordinary non-timing work: it explicitly reserves two physical cores and
corresponds to the partition's 16-GiB-per-two-core convention without relying
on hint interpretation.  It does not reserve the shared LLC,
memory channels, or socket power budget and therefore cannot provide stable
timing when multiple solvers are packed into one six-core L3 domain.  `-c 12`
is the minimum scheduler-visible allocation that reserves a complete L3
domain on these nodes.  It keeps the actual 16-GiB process contract, leaves the
node shared, and is required only for targeted timing—not full-width scoring
or correctness arrays.

## Proof production, checking, and I/O

The isolated median proof-production cost is 3.581 seconds (0.975%), 0.980%
cycles, and 1.336% instructions over proof-off.  All four proof jobs reproduce
the current candidate's 1,514,395,111-byte proof at SHA-256
`7bcb3d8d...ac47e`; pinned `drat-trim` accepts every proof.  The three isolated
checker walls are 112.12, 112.03, and 107.11 seconds, median 112.03.  Thus:

- stable proof-off solver headline: 367.312 seconds;
- stable proof-on solver headline: 370.893 seconds;
- proof validation is reported separately: median 112.03 seconds;
- median proof-on solver plus checker pipeline: 482.923 seconds.

The earlier same-allocation estimate of +28.532 seconds / 7.57% from job
1647782 was an upper bound contaminated by time-varying node conditions, not a
stable proof-cost estimate.

Scratch resolves to local NVMe XFS; the repository is NFS over RDMA.  Inputs,
proofs, and checker consumption remain on NVMe, while only small logs/results
reach NFS.  Proofless filesystem output is negligible.  Proof-on writes the
1.514-GB proof locally, adds about one second of system CPU, and records
negligible I/O PSI.  The checker reports no physical filesystem input because
the proof remains in page cache.  I/O is not the observed solver-time
bottleneck.  Local staging is still mandatory: writing proofs or decompressed
inputs to NFS would create a different, invalid timing protocol.

## Approved stable-timing policy

The user approved the following policy for the development plan:

1. Routine candidate/baseline timing uses separate one-solver jobs, `-c 12`, a
   48-GiB SLURM allocation, no whole-node exclusivity or all-memory reservation,
   and the applicable 16-GiB solver process limit.  The allocation matches six
   physical cores at the partition's 8-GiB-per-core capacity; it neither gives
   the solver more memory nor reserves memory bandwidth.  On the current nodes,
   require each resulting CPU mask to cover one complete six-core L3 domain.
   Launch the two arms in the same wave so they see the closest practical
   shared-node conditions; never serialize them in one allocation.  `-c 4`
   remains valid for ordinary functional work but is rejected for
   long-instance timing.
2. Bind each single solver thread to one granted hardware thread.  Record node
   occupancy across every partition, not only the submitting partition.  Node
   occupancy is a covariate and rejection diagnostic, not a reason to reserve
   an otherwise idle 96-core node.
3. Verify scratch is node-local.  Stage decompressed input, proof, and checker
   work there; copy only compact terminal evidence to the repository.  Record
   node, CPU mask, `/usr/bin/time`, perf cycles/instructions/IPC/migrations,
   load/PSI, proof bytes/hash, checker time, and cleanup.
4. Require three independent exact-state paired waves for a performance claim
   that will accept or reject a repair.  Each wave contains one separate
   candidate job and one separate baseline job running concurrently, with arm
   placement crossed over node/L3 positions when practical.  The primary
   development statistic is the median paired candidate/baseline ratio with
   its complete ratio range, never best-of.  An absolute headline is secondary
   and is allowed only when each arm's complete shared-node range is at most
   2%; otherwise the absolute observation is environmentally invalid and does
   not become a solver regression.  A stable paired slowdown remains solver
   evidence even if both arms move together relative to history.  This rule
   does not repeat fuzzing: a required fuzz checkpoint is one Claude-owned
   campaign.  It also does not multiply correctness gates or ordinary
   diagnostic probes, which keep their existing predeclared contracts.
5. Proof-on solver time is the competition-relevant headline.  Proof-off is a
   separate engineering diagnostic.  Never estimate proof cost by subtracting
   unmatched shared-node runs.  Report proof checking separately from proof
   production and from solver score.
6. Never request `--exclusive=user --mem=0` or another whole-node reservation
   for solver timing or diagnosis.  Diagnose interference from paired ratios,
   cross-partition occupancy, perf service-rate evidence, and naturally
   observed low/high-load windows.  Full-width tier arrays retain their
   existing no-throttle, no-node-constraint resource contract and are
   interpreted through aggregate paired evidence, not a single absolute
   runtime.

For the current accepted pin, the supported ordinary shared-node `-c 12`
calibration is 367.045--372.354 internal seconds, median 368.307.  Routine
measurements decide changes by paired ratios; a future exact-state absolute
run outside this band is investigated rather than compared to a cherry-picked
historical minimum.
