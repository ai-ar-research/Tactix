---
name: slurm-jobs
description: Submit, monitor, wait for, and clean up Tactix solver measurements on SLURM. Use for every real-instance solver run, benchmark, fuzz leg, or performance probe; exact resources belong to the invoking runner, not this skill.
---

# SLURM jobs

All real solver execution uses `sbatch`; fixed micro-tests and builds may run in
the session shell. The runner owns partition, memory, time, array shape, log
paths, and other cluster parameters—do not duplicate cluster facts here.

## Workflow

1. Pin an immutable build and record its commit and binary hash. Never rebuild a
   directory owned by a submitted job. Concurrent candidate/baseline work uses
   distinct pins.
2. Submit through the owning wrapper (`tools/bench/run.py`, fuzz wrapper, or a
   narrowly quoted `sbatch --wrap`). Capture the parsable job ID and log paths.
3. For a task that must finish before Claude continues, launch the wrapper's
   `--wait` command in the background and use the tool-owned watcher
   `tools/slurm/wait.py <job-id>` rather than hand-written polling loops.
4. Inspect terminal state and logs. Success means the expected artifacts and
   validations exist, not merely that SLURM reports COMPLETED.
5. Cancel obsolete jobs, remove only explicitly identified temporary job
   directories, and retain evidence needed by an active report or failure.

Never run `srun` from the session, infer success from queue disappearance, reuse
a mutable build across active jobs, or delete broad paths. If a pin lock is
stale, first prove no queued/running job references it; then remove that exact
lock and rebuild deliberately.
