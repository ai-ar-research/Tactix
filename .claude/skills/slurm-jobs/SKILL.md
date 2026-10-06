---
name: slurm-jobs
description: Submit, monitor, wait for, and clean up Tactix solver measurements on SLURM. Use for every real-instance solver run, benchmark, fuzz leg, or performance probe; exact resources belong to the invoking runner, not this skill.
---

# SLURM jobs

All real solver execution uses `sbatch`; fixed micro-tests and builds may run in
the session shell. The runner owns partition, memory, time, array shape, log
paths, and other cluster parameters—do not duplicate cluster facts here.

Use the partition selected by the owning runner and the user, subject to the
plan's allocation contract. Let SLURM place array tasks across eligible nodes;
do not pass `--nodelist` or add a `#SBATCH --nodelist` directive unless the user
explicitly authorizes a constraint or a documented correctness/resource need
requires it, as permitted by `CLAUDE.md`.

Submit arrays at full width by default. Do not pass an array throttle (`%N` or
the runner's `--throttle`) merely to reduce contention or out of habit: make
every task eligible and let SLURM use the available partition. A throttle is
permitted only when the user explicitly requests it or a documented
correctness/resource constraint requires it. Record the exception and its
reason in the run manifest/report.

## Workflow

1. Pin an immutable build and record its commit and binary hash. Never rebuild a
   directory owned by a submitted job. Concurrent candidate/baseline work uses
   distinct pins. Compute nodes cannot see login-node `/tmp` worktrees or output;
   freeze an isolated commit into a hashed source archive under the shared
   repository pin, extract it in job-local scratch, and write retained artifacts
   back to the shared pin.
2. Submit through the owning wrapper (`tools/bench/run.py`, fuzz wrapper, or a
   narrowly quoted `sbatch --wrap`). Capture the parsable job ID and log paths.
   Hash and retain every executable wrapper as well as the final solver binary;
   record wrapper-injected flags, referenced programs/profiles and their hashes.
   A manifest command that names a wrapper is not evidence of the solver's
   effective argument vector.
   Batch scripts must not assume that `SLURM_TMPDIR` exists; under `set -u`, use
   a task-specific fallback such as
   `scratch="${SLURM_TMPDIR:-/tmp}/name-${SLURM_JOB_ID}"`.
3. For a task that must finish before Claude continues, launch the wrapper's
   `--wait` command in the background and use the tool-owned watcher
   `tools/slurm/wait.py --job <job-id>` rather than hand-written polling loops.
   Record the submission timestamp and pass `--since` when needed to
   disambiguate recycled job IDs.
4. Inspect terminal state and logs. Success means the expected artifacts and
   validations exist, not merely that SLURM reports COMPLETED.
5. Cancel obsolete jobs, remove only explicitly identified temporary job
   directories, and retain evidence needed by an active report or failure.

Never run `srun` from the session, infer success from queue disappearance, reuse
a mutable build across active jobs, or delete broad paths. If a pin lock is
stale, first prove no queued/running job references it; then remove that exact
lock and rebuild deliberately.
