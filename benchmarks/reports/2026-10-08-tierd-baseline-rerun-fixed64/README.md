# Tier D owner-baseline rerun after the DSR checker change

Status: runner, governor and smoke runs are done. Submitting (A) and (B) waits on
the storage decision in §5.

## 1. Why

The frozen campaign `benchmarks/baselines/sc24-26-32g-5000s` checked
Satsuma+Kissat's witnessed DSR with Satsuma's bundled 32-bit `dsr-trim`. That
build's capacity growth overflows on multi-GB proofs, as documented in
[the checker-width report](../2026-10-08-dsr-checker-width-and-trust-boundary.md).
The accepted `long64-fixed` profile replaces it. User instruction (2026-10-08):

- **(A)** Rerun all 1,166 Satsuma+Kissat Tier D rows on cpu-q with `-c 4`,
  32 GB, a 5,000 s solve limit and an 8 h checker limit.
- **(B)** For kissat-sc2024, kissat-4.0.4, ae-kissat-mab and lymphosat, rerun
  (solve + check) the rows whose checker timed out or ran out of memory, with an
  8 h checker limit and 64 GB.
- **(C)** After (A) finishes, rerun (A)'s Satsuma checker memory-outs with 64 GB.

The frozen campaign is not modified.

## 2. Runner

`tools/bench/baseline_rerun.py` (tests: `tools/bench/test_baseline_rerun.py`).

- **Commands.**
  - `--select` prints a selection and its counts and writes nothing.
  - `--prepare` builds a new campaign: a manifest, copied DSR pins and a frozen
    runner copy in `runner/`.
  - `--run-one`, `--aggregate` and `--submit` run, aggregate and submit it.
  - `--submit --governor-jobs` also registers the array with the quota governor.
- **Reuse of the frozen runner.** It imports `frozen_baseline.py` for exclusive
  claims, the pending journal and atomic promotion, verdict and model-protocol
  parsing, and checker acceptance strings.
- **Satsuma DSR route.**
  - Stages: `dsr-trim` (long64-fixed, `30207e6f…`) → `dsr-compress -d`
    (`8ddab830…`) → Trestle `srcheck` text mode (`19bf79f4…`).
  - Both CNF readers get `proof_oracle.canonical_checker_cnf`, and the stages come
    from `proof_oracle.reference_dsr_stages`.
  - The detached provenance object from `experimental_tierd.dsr_checker_provenance`
    is recorded in the manifest.
- **Other routes.** These stay exactly as in the frozen campaign: DRAT → drat-trim
  → cake_lpr, and PBP → VeriPB → CakePB, using the frozen pins in place,
  hash-bound.
- **Memory.**
  - Memory is enforced by the Slurm task cgroup (`--mem`).
  - When the solver limit is below the allocation, which is the (B)/(C) case
    (32 GiB solver, 64 GiB allocation), a process-group RSS watchdog stops the
    solver above 32 GiB. It samples every 2 s and sends SIGTERM, then SIGKILL.
  - No `RLIMIT_AS` is used, as `tests/test_slurm_memory_enforcement.py` requires.
  - The CakeML heap is the checker memory minus 4 GiB of stack and 4 GiB of
    slack. That is 24,576 MiB at 32 GiB (frozen amendment 2) and 57,344 MiB at
    64 GiB.
- **Storage.**
  - The scratch backend is pluggable: `--scratch-root`, default
    `{campaign}/scratch`, which is the user's choice.
  - Every child gets `TMPDIR` set to the task scratch directory.
  - Each checker stage's input is deleted after the stage, and everything is
    deleted before the result is published.
  - On SIGTERM (scancel or time limit) the runner kills the stage group, deletes
    the scratch and writes an `INTERRUPTED_SIGTERM` record.
  - Partial proofs of UNKNOWN runs are sized but not hashed.
- **Per-stage records.** Each stage records exit code, wall time, `/usr/bin/time`
  max RSS, the sampled peak RSS of the process group, the cgroup `oom_kill`
  delta, the terminating signal, and the pin hash.

## 3. Selection rule

`classify_check`, applied to the last checker stage of an UNSAT row:

| Class | Rule |
|---|---|
| timeout | Any stage killed by the pipeline deadline, or drat-trim printed its own `s TIMEOUT` |
| memout | Killed by the memory watchdog; or a cgroup `oom_kill` occurred; or exit 137 / SIGKILL; or the output names an allocation failure (`MEMOUT`, `out of memory`, `OutOfMemory`, `xrealloc`, `memory allocation of`, `heap space exhausted`, `bad_alloc`, `cannot allocate memory`) |
| crash | SIGSEGV or SIGBUS |
| rejected | `s INVALID`, `s NOT VERIFIED`, `Error`, or `failed` |

Rows that are UNKNOWN or SAT, and UNSAT rows without a proof, are never
selected. Applied to the frozen `results.json`, the rule reproduces a manual
audit of all 133 failed checks.

| Solver | memout | timeout | (B) rows |
|---|---:|---:|---:|
| kissat-4.0.4 | 7 | 5 | 12 |
| kissat-sc2024 | 7 | 4 | 11 |
| ae-kissat-mab | 3 | 8 | 11 |
| lymphosat | 24 | 7 | 31 |
| **total** | 41 | 24 | **65** |

Frozen Satsuma failures, all rerun by (A): 31 memout (30 `xrealloc` capacity
crashes and 1 srcheck OOM), 9 timeout, 10 dsr-trim SIGSEGV, 2 srcheck `s INVALID`.

Facts about the frozen run that matter for interpreting (B):

1. The frozen run enforced 32 GiB with **RLIMIT_AS** (runner commit `553d8fedf`;
   the manifest field is `process_memory_gib`; amendment 2 records a 32 GiB
   address space). Several frozen "memouts" are address-space failures, for
   example VeriPB `Os code 12` after 1 s at 5.8 MB RSS, and drat-trim's lrat
   table reallocation failing at 25–31 GB. A cgroup-only run may not reproduce
   them.
2. The frozen checker budget was **45,000 s**, and drat-trim's internal default
   timeout is 40,000 s. The 24 timeout rows already ran 40,000–45,000 s at
   1–14 GB RSS, so an 8 h rerun of them is expected to time out again.
3. Unchanged frozen checker false negatives, outside (B)'s scope and not rerun:
   - drat-trim's trivial-UNSAT exit 1 (6 rows on hwmcc17-6s16 and 6s29);
   - cake_lpr / CakePB reading SCPC-500-1's raw CNF (4 rows);
   - VeriPB "Assumption cannot be elaborated" on 5 rook rows.

## 4. Smoke tests (cpu-q)

| Campaign | Job | Rows | Result |
|---|---|---|---|
| `sc24-26-satsuma-fixed64-32g-8h-smoke` | 1757021 (agg 1757022) | Satsuma tasks 13, 33, 73, 133 | 3 UNSAT verified through dsr-trim→dsr-compress→srcheck on the canonical CNF; 1 SAT model valid |
| `sc24-26-recheck-64g-8h-smoke` | 1757026 (agg 1757027) | kissat-sc2024 11, 31, 71; lymphosat 99, 134, 144 | DRAT→drat-trim→cake_lpr (`CML_HEAP_SIZE=57344`) 2/2; PBP→VeriPB→CakePB 2/2; 2 SAT valid |

All 10 rows were valid, accounting shows COMPLETED 0:0, no pending journals
remain and every task scratch directory was deleted. The unit tests cover the
watchdog kill, the deadline, the SIGTERM cleanup path, pin tamper refusal and
once-only refusal.

## 5. Storage risk (decision needed)

The scratch lives in home (1 TB quota; 456–526 GB used on 2026-10-08).

- **Campaign (A).**
  - In the frozen run, Satsuma's timed-out (UNKNOWN) runs wrote partial proofs
    for the full 5,000 s: median 9.6 GB, p90 29.6 GB, maximum 253 GB.
  - Averaged over time, a running task holds about 4.9 GB.
  - A time-stepped simulation that includes the 750/700 governor peaks at about
    1.31 TB total usage with 100 concurrent tasks, and about 1.12 TB with 50.
- **Campaign (B).** The certificates total 884 GB. The first 24 array tasks hold
  307 GB of proofs before any LRAT or PB kernel is written (frozen ratios: LRAT
  up to 62× the proof, kernel up to 74.5 GB).
- **What the governor cannot do.** It stops new starts only. Running tasks keep
  writing, and exceeding the quota kills every hwu job.

## 6. Quota governor

`tools/slurm/quota_governor.py` (tests: `tools/slurm/test_quota_governor.py`).
Its frozen copy and state are in `benchmarks/baselines/sc24-26-rerun-quota-governor/`.

- **Usage reading.** Every 600 s it reads the hwu row of `/home/.zfs-quota`,
  falling back to `quota -s`.
- **Hold.** Above 750 G it runs `scontrol hold` on the PENDING tasks of the
  arrays listed in `jobs.json`, and keeps holding newly pending tasks of those
  arrays.
- **Release.** Below 700 G it runs `scontrol release` on exactly the tasks it
  held. It never releases another party's hold.
- **Never touched.** It never acts on running jobs or on jobs it does not list.
- **Logging and exit.** Every check goes to `governor.log.jsonl`. It exits once
  every listed job has left the queue and `expect_more` is false.

## 7. Commands

```bash
B=python3\ /home/hwu/SAT-solving/tools/bench/baseline_rerun.py
# aggregate a campaign (also runs automatically as the afterany job)
$B --campaign benchmarks/baselines/<campaign> --aggregate
# (C), after (A) is terminal:
$B --select --source benchmarks/baselines/sc24-26-satsuma-fixed64-32g-8h \
   --solvers satsuma-kissat --classes memout
$B --campaign benchmarks/baselines/sc24-26-satsuma-fixed64-64g-8h-memout --prepare \
   --source benchmarks/baselines/sc24-26-satsuma-fixed64-32g-8h --solvers satsuma-kissat \
   --classes memout --mem-gib 64 --solver-memory-gib 32 --checker-memory-gib 64 \
   --throttle <slots> --throttle-reason "user-requested cap of 125 concurrent jobs" \
   --reason "(C) Satsuma checker memory-outs of (A) with a 64 GiB checker" \
   --instruction "user 2026-10-08 (C)"
$B --campaign benchmarks/baselines/sc24-26-satsuma-fixed64-64g-8h-memout --submit \
   --governor-jobs benchmarks/baselines/sc24-26-rerun-quota-governor/jobs.json
# then set "expect_more": false in that jobs.json
```
