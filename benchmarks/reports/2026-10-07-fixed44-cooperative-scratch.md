# Fixed-44 cooperative scratch profile

Status: implementation and bounded mechanics checks complete; the fixed-44
launch remains held. The profile is opt-in and does not change ordinary Tier-D
or solver defaults.

The cluster cannot provide the previously proposed timely whole-node shape.
`cpu-q` rejects `--exclusive` because each node has 64 CPUs while the partition
permits at most 52 per job. A 52-CPU/all-memory pilot was admitted only with an
October 9 estimate and was cancelled before start after its submitted inputs
changed. Dense-partition mechanics job 1754715 used no node constraint but
correctly declined before opening scratch: node017 had 292,519,759,872 bytes
available, below the 400,000,000,000-byte threshold. Dense hardware is EPYC
9454; `cpu-q` is EPYC 7513, so dense timing would not be comparable.

The replacement `fixed44-cooperative-v1` profile uses an ordinary `cpu-q`
allocation with four CPUs and 32 GiB. An exact four-CPU/32-GiB `srun` step
provides the effective task cgroup. Each participating job opens the same
stable lock inode under a private per-user, repository-wide directory on the
node. It uses `O_NOFOLLOW` and a nonblocking exclusive `flock`, retains the
inode across owners, and holds the descriptor until all anonymous handles are
closed. Contention produces `completion.status=incomplete`, reason
`scratch_node_busy`, and `retryable=true`. The lock coordinates only jobs using
this profile. Foreign jobs, older jobs, node services, and other unmanaged
writers remain external interference.

After taking the lock and before the first `O_TMPFILE`, runtime requires
400,000,000,000 bytes available on `/tmp`. It installs an inherited
60,000,000,000-byte `RLIMIT_FSIZE`, permits six runner-managed live handles,
and classifies `ENOSPC`, native `SIGXFSZ`, admission, contention, and handle or
file-limit failures as incomplete infrastructure evidence. Such records are
invalid, block comparison eligibility, and censor PAR-2 instead of being
counted as timeouts. The profile verifies four-CPU affinity and the tightest
inherited cgroup memory limit, walking ancestor cgroups because Slurm leaves
the task leaf at `memory.max=max` while enforcing 34,359,738,368 bytes at its
user and job ancestors.

Final focused job 1754860 passed 15 anonymous-scratch tests, 35 runtime tests,
six scorer tests, ten repository runtime/builder tests, Python compilation,
CLI help, whitespace checking, and pre/post source hashes. This includes the
native-SIGXFSZ, cgroup-ancestry, and PAR-2-censoring supplements. Earlier
focused jobs are retained because they exposed packaging/postcheck or cgroup
inspection mistakes; job 1754811 was fully green before the final
native-signal and scorer supplement.

Exact mechanics pilot 1754816 completed on unconstrained `cpu-q` node006 in
an ordinary four-CPU/32-GiB allocation. Its inner step had four CPUs and an
effective 34,359,738,368-byte memory limit. `/tmp` had 402,851,180,544 bytes
available. Contention declined, reacquisition retained device 2051/inode
271069710, the seventh handle declined, and `RLIMIT_FSIZE` was exactly
60,000,000,000 bytes. Witnessed DSR, witness-free DSR-to-DRAT, and PBP all
verified independently; each route peaked at six managed handles. Strace saw
84 write opens across 16 traces, all to `O_TMPFILE`, proc-fd paths, the stable
lock, or pilot JSON; it found no unexpected helper writes. The frozen pilot
inventory SHA-256 is
`8e550928694aa9cbb58f80c807739b9fc7c0c4b72ee27cfc097b235e04ee59e5`.
Pilot 1754793 established that the batch leaf itself is unbounded, and audit
1754805 located the ancestor limit; these failed diagnostics are preserved.

Two independent-review findings still block a fixed-44 launch. Selected rank
754 previously emitted a 59,630,088,192-byte proof stream, leaving less than
0.7% below the 60 GB file limit, while observed derived LRAT expansion can
exceed that limit. The solver also creates internal anonymous tmpfiles which
are not counted by the runner's six-handle bound. Therefore 360 GB is only the
runner-managed maximum, and the nominal 40 GB difference is not a total-task
reserve. Before launch, use a bounded successor rank754 probe plus a
`/proc/<solver>/fd` size/high-water sampler, or replace the flat per-file limit
with a justified stage/total budget. A resource-incomplete fixed44 summary and
retry policy are also required. No target, regression, or full-44 job was
submitted by this work.
