---
name: slurm-jobs
description: Submit, size, monitor, and clean up SLURM jobs on the cluster — tier evaluations as job arrays, fuzz legs as multi-core jobs, one-off probes via sbatch --wrap. Use whenever running ANY solver execution (there is no other sanctioned way to run one), sizing --time/--mem, checking job status, or cancelling jobs.
---

# SLURM job submission and hygiene

Authoritative protocol: DEVELOPMENT_PLAN.md §8.1 (revs 64/65 — the cluster
evaluation environment). This skill operationalizes it: the concrete commands,
sizing rules, and cleanup discipline. Rules marked **(user)** are direct user
directives (2026-07-23).

## The one rule that governs everything

**(user) No solver ever executes outside a submitted job.** Not on the login
node, not in the session shell — the interactive session is itself a SLURM job
on the default partition (`cpu-q`), so anything run "locally" executes inside
that allocation, on the wrong partition. **Never use `srun` from the session**:
it becomes a job *step* of the session's own job (wrong partition, hangs
waiting for resources that never match). `sbatch` only.

The line is **measurement**: anything whose output is a time, a PAR-2, a verdict
on a real instance, or a fuzz leg goes through `sbatch` — including "quick" spot
checks. Builds, doc/tooling work, and the sub-second fixed-input test harnesses
(`ctest`, `tools/compete/check.sh`, the micro-proof tests) run in the session
shell.

## Submission patterns — pick by workload

All jobs go to **`--partition=cpu-dense-hwulab-q`** (2 × 192-core nodes) with
**`--mem-per-cpu=8G`** (user rule: 8 GB per core).

| Workload | Pattern | Command |
|---|---|---|
| Tier evaluation (any solver set) | **Job array**, one task per solver×instance **(user)** | `python3 tools/bench/run.py --tier C --slurm --solver build/release/tactix --baseline kissat [--throttle N]` |
| Fuzz leg | **One multi-core job**, harness threads = allocation **(user)**; default **50 cpus**, quick=10k / nightly=200k cases (rev 66) | `tools/fuzz/slurm.sh [--time T] --name <leg> -- --quick --solver build/debug/tactix [...]` |
| One-off probe (single solver × single instance) | Single `sbatch --wrap` | `sbatch --partition=cpu-dense-hwulab-q --ntasks=1 --cpus-per-task=1 --mem-per-cpu=8G --time=<cap+slack> --wrap '...'` |

- The bench runner submits the array **plus a dependency-chained aggregate
  job**; results land in `benchmarks/runs/<stamp>-<tier>-<name>/` as per-task
  JSONs merged into the standard `results.json`. `--aggregate <rundir>`
  re-merges any time; `--dry-run` writes the manifest + sbatch scripts without
  submitting (inspect `array.sbatch` there).
- `cpus-per-task = ceil(mem_cap / 8 GB)` — Tiers A–C: 1 core / 8 GB; Tier D
  (16 GB competition cap): 2 cores.
- Record and compare only identical **allocation shapes** (partition,
  cpus/mem per task, throttle) — §8.1's replacement for the jobs count.
  `--throttle N` caps concurrent array tasks for timing-precision gate runs.

## Sizing --time

Solver timeouts are enforced *inside* the task by the runner (wall timeout +
RLIMIT_CPU backstop) — SLURM `--time` is a wider backstop, never the
mechanism. The bench runner computes it as
**solve timeout + max(60 s, 2×timeout) drat-trim budget + 120 s slack**:

| Tier | solver timeout | per-task --time |
|---|---|---|
| A | 30 s | 01:02:00 |
| B | 300 s | 01:07:00 |
| C | 1200 s | 01:42:00 |
| D | 5000 s | 07:00:00 |

The proof-check term is load-bearing and it dominates the small tiers: since
plan rev 72 the checker budget is `max(3600 s, 4 × solve timeout)` (was
`2 × timeout`, which scored `urq45` unsolved for a checker-budget reason the
competition would not have applied), and **every arm writes proofs now**, so
baselines carry the same term. For manual `--wrap` probes, size the same
way: inner `timeout` cap + slack. Fuzz wrapper default is 24 h (nightly-leg
scale); pass `--time` for smaller legs.

## Monitoring and results

- **Arm a watcher for EVERY job you submit (user rule, 2026-07-23)** — a
  submitted-and-forgotten job is how results get lost. Immediately after
  `sbatch`, start a background wait that fires one completion notification
  and prints the result summary, e.g.:
  `until [ -s <resultlog> ] && [ "$(squeue -u hwu -h -o '%j' | grep -c <name>)" = "0" ]; do sleep 30; done; cat <resultlog>`
  (Bash `run_in_background`; poll ~15–60 s by expected runtime; make the
  condition cover FAILURE too — job gone from queue ⇒ fires regardless of
  outcome, then read sacct/logs). For bench arrays, watch the aggregate log;
  for fuzz legs, the `slurm-<name>-<jobid>.log` tail. If a session ends with
  jobs still running, the resume anchor must say which jobs + where results
  land (the wrap-up skill's queue check).
- `squeue -u hwu` — live state; array tasks show as `<jobid>_<index>`.
- `sacct -j <jobid> -o JobID,JobName,Partition,State,Elapsed,NodeList` —
  post-mortem (partition + elapsed per task).
- Bench: aggregate summary at `<rundir>/logs/aggregate.log`; per-task logs in
  `<rundir>/logs/task-<i>.log`. Missing task results (node death) are scored
  MISSING, warned loudly, exit code 2 — resubmit those indices, then re-run
  `run.py --aggregate <rundir>`.
- Fuzz: `tools/fuzz/slurm-<name>-<jobid>.log` (tracked in git only when cited
  as evidence in PROGRESS, like the nightly-*.log convention).

## Build-path ownership is ENFORCED (`tools/bench/buildlock.py`)

A submitted array owns its binary path until it finishes (plan §8.1, CLAUDE.md) —
otherwise a rebuild swaps the binary out from under tasks still to be scheduled,
and the window silently measures two different solvers. The rule was broken **six
times** by people who knew it, so it is now a mechanism: the owned tree is made
**read-only** and a `BUILD-DIR-OWNED` marker names the owning job, so *any* build
into it fails — `cmake --build`, `make`, `ninja`, `cp` — with a message naming the
owner and how to reclaim it.

- **`run.py --slurm` does this for you**: it `--gc`s stale locks, `--acquire`s
  every arm's build dir after `sbatch`, and the dependent aggregate job
  `--release`s when the array finishes. Nothing to remember on the normal path.
- **A hand-rolled `sbatch` probe must lock its own binary:**
  `python3 tools/bench/buildlock.py --acquire build/pin-x --job $JID`
- **A same-window Tier B + Tier C pair needs TWO pin dirs**, one per array — the
  second `--acquire` correctly refuses a path another live job owns. Build both
  from the same commit and verify the binary shas match. Two concurrent arrays
  sharing one binary path is exactly the hazard the rule exists to prevent; it
  never was safe, so pointing both at one dir is not the shortcut.
- **Reclaiming a directory:**
  | situation | command |
  |---|---|
  | why is this locked? | `buildlock.py --check build/pin-x` |
  | job finished, give it back | `buildlock.py --release build/pin-x` |
  | clear everything a dead job left | `buildlock.py --gc` |
  | show all locks | `buildlock.py --list` |
  `--release` refuses only while the job is genuinely in `squeue`; `--force`
  overrides it and is right only when the array's results are being discarded.
- **It cannot wedge a directory.** Every ambiguous case — job gone, squeue
  unavailable, marker corrupt, acquire interrupted — resolves toward releasing,
  and `chmod` always works for the owner anyway.
- **Before sweeping stale `build/pin-*`, run `--gc`** — a locked tree is
  read-only and `rm -rf` fails on it.

## Cleanup discipline

- **(user) Cancel jobs that are no longer needed, promptly** — superseded
  runs, probes whose question is already answered, stuck submissions.
- **(user) Cancel ONLY by explicit job id** (`scancel <jobid>` /
  `scancel <jobid>_<index>`). **NEVER account-wide commands**
  (`scancel -u hwu` is forbidden): the user runs long-lived jobs of their own
  (e.g. the chvatal-8 family) on this account, and the session's own shell is
  a job — a blanket cancel kills both.
- Before ending a session: `squeue -u hwu` must show only the user's own jobs
  (+ the session shell). Anything this session submitted must be COMPLETED or
  explicitly cancelled by id.
- **Also check for LEAKED LOCAL PROCESSES, not just SLURM jobs**
  (`ps -u hwu -o pid,etime,cmd --sort=-etime`). This shell aliases `cp` to
  `cp -i`, so a bare `cp` over an existing file in a scripted chain blocks
  FOREVER on an "overwrite?" prompt nothing will answer (one sat 18 h).
  **Never `cp` onto an existing path in a chain — use `cat src > dst` (or
  `cp -f`)** — and when you kill such a process, expect the REST of its command
  line to run: check `squeue` immediately afterwards for a stale `sbatch`.
- Never cancel the session's own interactive job (named `bash`, on `cpu-q`) —
  that kills the running Claude session.

**Log retention:**
- Bench run dirs (`benchmarks/runs/<stamp>-…/`): the aggregate step deletes
  `logs/task-*.log` automatically on a fully-clean run (all tasks present, no
  mismatch/proof-reject) — per-instance verdict/time/node survive in
  `results.json`. On ANY failure the task logs are kept: they are the
  debugging record. `results.json` + `aggregate.log` are kept indefinitely
  (small; the measurement archive reports cite).
- Fuzz `tools/fuzz/slurm-<name>-<jobid>.log`: commit ONLY logs cited as
  evidence in PROGRESS (the nightly-*.log convention); delete uncited green
  ones at session end (wrap-up).
- One-off probe logs: write them under the session job dir
  (`$CLAUDE_JOB_DIR/tmp`), never into the repo — they vanish with the session.

**`$TMPDIR` IS `/tmp` AND IS SHARED PER NODE — not per job.** Two concurrent
probes decompressing to the same `$TMPDIR` filename CLOBBER each other, silently
and with plausible-looking numbers (a two-instance sweep sharing `$TMPDIR/x.cnf`
returned 0.03 s / 1 conflict / UNSATISFIABLE for both SAT instances). Same-instance
collisions are benign — identical bytes — which is why the hazard hides until two
DIFFERENT instances collide. Always make scratch job-unique:
`cnf=$TMPDIR/<name>.$SLURM_JOB_ID.cnf`, and **sanity-check every probe against the
expected verdict**: a wrong verdict, a 1-conflict solve, or a sub-second time on a
hard instance means the input, not the solver.

## Environment facts (verified 2026-07-23)

- Partition `cpu-dense-hwulab-q`: node017–018, 192 cores / ~750 GB each;
  `MaxArraySize=100000` — even Tier D (793 instances x arms) needs no array chunking.
- Toolchain: gcc/13.1.0 module (loaded by default; `export CC=gcc CXX=g++` —
  bare `cc` is 11.5); cmake/ninja are pip user installs in `~/.local/bin`
  (no system/module cmake exists).
- `/home` is NFS-shared with compute nodes; per-task scratch goes to
  node-local `$TMPDIR` (both wrappers already do this — proofs and fuzz case
  dirs never churn the shared FS).
