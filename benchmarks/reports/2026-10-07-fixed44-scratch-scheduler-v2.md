# Fixed-44 scratch scheduler correction (v2)

Status: scheduler-correct implementation checkpoint; fixed-44 submission remains
held for review. This supersedes only the unusable `--exclusive` scheduler
shape recorded in the frozen v1 report; its measured scratch bounds, route
lifecycle evidence, and held-launch status remain historical evidence.

The opt-in `fixed44-whole-memory-v2` profile binds exactly the 44-row selection
authority at
`benchmarks/reports/2026-10-07-fixed44-successor-campaign-preparation/selection.tsv`
(SHA-256 `74939b00a3395aa21d108d4ab20eb0925f569c1ac2bb501939e01432551a3333`).
It does not change the default Tier-D runner or solver configuration.

Each array task reserves 52 CPUs and all schedulable memory on one `cpu-q`
node, then starts one `srun --exact` step with four CPUs and 32 GiB. The outer
allocation leaves no schedulable memory for an ordinary colocated job while
the inner task retains the intended CPU and memory cgroup. Before its first
anonymous file is opened, the runtime
requires 400,000,000,000 available bytes on `/tmp`. It installs an inherited
60,000,000,000-byte `RLIMIT_FSIZE` and permits at most six live managed
`O_TMPFILE` handles. Thus managed anonymous files can occupy at most 360 GB,
leaving the declared 40 GB reserve at admission. The largest observed selected
certificate was 56,263,420,013 bytes, 3,736,579,987 bytes below the per-file
limit. The profile disables GF(2) telemetry and failed-proof retention and sets
`TMPDIR=/tmp` plus `PYTHONDONTWRITEBYTECODE=1`.

The witnessed binary-DSR route now allocates its text LSR only after
`dsr-trim` completes and the original proof is released. Binary and ASCII DSR,
witness-free DSR/DRAT, PBP, SAT model checking, and ordinary solver output fit
within six managed handles. Proof-stage stdout, stderr, and timing files use
generation-specific logical names, so descriptor-number reuse cannot make a
delayed duplicate release discard a newer artifact. Completed solve fields and
checker stages survive a resource interruption, but validity is false and the
terminal record has `completion.status=incomplete`. The scorer excludes only
the four declared scratch reasons; it never counts them as a verdict, proof
rejection, timeout, verification, or closure.

Focused controls cover exact admission pass/fail before open, exact and
over-limit writes from Python and a child, atomic seventh-handle refusal,
descriptor reuse, truncate failure, process-group reaping on `SIGXFSZ`, bounded
stderr inspection, terminal incomplete JSON, preservation of an already
completed solve and checker stage, and all proof-route handle peaks. Local
preflight on 2026-10-07 passed 49 `tools/bench` tests, 9 repository runtime
tests, the reference DSR-stage compatibility test, Python compilation, and
`git diff --check`. Immutable SLURM focused and resource-isolation pilot evidence
will be recorded separately before any fixed-44 submission.

The cluster rejects `--exclusive` on `cpu-q`: nodes expose 64 CPUs while the
partition permits at most 52 CPUs per job. Reserving the maximum 52 CPUs plus
all schedulable memory is the available scheduler-enforced isolation because
`/tmp` is the node root filesystem and the cluster reports no schedulable
`TmpDisk`. This is schedulable-memory isolation, not Slurm `--exclusive`; node
services or unmanaged writers can still consume root-disk space, so the 400 GB
admission threshold remains necessary. An admission poll, array throttle, or
lock among only our tasks cannot provide this isolation. The cost at the 5.75-hour campaign
ceiling is 253 node-hours for 44 tasks; on 52-core nodes it may reserve roughly
13,156 schedulable CPU-hours. The contract bounds files opened through
`AnonymousScratch`; immutable helper route audits must also show no auxiliary
writes before launch. No finite cap can guarantee completion for a future proof
larger than the observed bound, so such a task terminates as incomplete
infrastructure evidence.
