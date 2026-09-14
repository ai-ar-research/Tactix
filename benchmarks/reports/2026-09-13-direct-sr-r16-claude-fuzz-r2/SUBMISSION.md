# Direct-SR R16 fuzz checkpoint r2: submission record

This is the submission record only. The terminal report is written after job
1689057 reaches a terminal state.

| Item | Value |
|---|---|
| Candidate | Source `298c3467d11bb215237c2ccaaac62e63a949d0c1`, pin `build/pin-direct-sr-r16-canonical-proof`. The manifest `8f8b32ba…` was verified and `tactix` is `4d0c3668…`. |
| Base pin | `build/pin-srfuzz-direct-r16-298c3467`, job 1689026, manifest `643417bf…`, verified. `rel/`, `dbg/`, `extra/`, `src/` and `root/` were copied byte-for-byte; nothing was rebuilt. |
| Checkpoint pin | `build/pin-srfuzz-direct-r16-298c3467-r2`. `MANIFEST.sha256` = `cb302bdaa48a00c0ad95938f30150a490708afe48f95ab443c783d0ef575627e` (168 files). No entry is writable. The manifest and the exact hashes were verified before sbatch (`campaign/pre-submit-manifest-check.txt`). |
| Checkers | dsr-trim `a1d7251c…` and dsr-compress `89aaab76…`, built from `dsr-trim-sr` at `0b8cc0ef` plus patch `a5653c3a…`. Trestle srcheck `f0eef5c4…`. drat-trim `f2cb7d45…`, lrat-check `0c9a87d5…`, cake_lpr `ab9dc6c4…`, kissat `3aa8e41f…` and cadical `ecf73365…` are unchanged. |
| Binaries | Release `4d0c3668…`, Debug (ASan/UBSan, proof shadow) `ae700889…`, srdriver-rel `0a4cc0fe…`, srdriver-dbg `7c18c160…`, tracecheck `232af674…`. These are in `campaign/expected-hashes.sha256`, which is checked three times: at pin assembly, in `batch.sh` before the workload, and by the in-job `identity` leg. |
| Job | **1689057**. One non-array job on `cpu-dense-hwulab-q`: 100 CPUs, 400000M, 1:30:00, `--signal=B:USR1@240`. No throttle and no node constraint. Resources match job 1689026. |
| Legs | Same legs, seeds and sizes as job 1689026. The legs are: identity, harness self-test, focused tests (Release and Debug, including micro_proofs and trace_replay), stock `selftest.sh`, `test_modelcheck`, the SR audit modes `srdefault`/`srrandom`/`srprogs`/`srcapi`/`srinterrupt`, the prior modes `regress`/`interrupt`/`random`/`default`/`progs`, and the stock fuzz.py legs `random-dsr`/`default-dsr`/`random-drat`/`random-lrat`/`corpus-dbg-dsr`. |

## Harness repairs (harness only; solver source untouched)

- **F: forward-only DSR acceptance.**
  - `audit.check_dsr` now defaults to forward mode. `check_proof` always calls it forward.
  - The old backward acceptance followed by a forward SR re-check is now a single forward check.
  - The pinned copies of `root/tools/fuzz/fuzz.py` and `src/tests/micro_proofs.sh` are patched at assembly to use `dsr-trim -f`. The repository files are unchanged.
  - Forward dsr-trim refuses a certificate in two ways that the pinned status parser calls `error`. The audit classifies both as `rejected`:
    - `Fatal error: Didn't UP refute the empty clause.` (exit 1);
    - `s VALID` with no UNSAT line (exit 0).
    - Any other abnormal output stays `error`.
- **H-A: leg-status aggregation.**
  - Each leg writes its own exit status through `legs.sh`.
  - `legstatus.py` fails the workload on any of these:
    - a nonzero, missing or unexpected leg status;
    - a focused test with a missing or duplicate `TEST` line, a nonzero exit, `FAIL` lines, or sanitizer lines.
  - `runtest.sh` exits with the test's own status.
  - `work.sh` exits with the aggregate, and `batch.sh` exits with the workload status. A failing leg therefore makes the Slurm job FAILED.
  - The status files and `work-status.txt` are promoted into `out-JOB/`.
- **H-B: failure subtypes.**
  - `failclass.failure_class` names a class `KIND[SUBTYPE]`.
  - A proof failure's subtype is its format, status, checker stage and SR-witness presence.
  - For other failures, the subtype is the leading detail text with digit runs folded.
  - `keep_failure` and `promote.py` (bundles, summaries, fuzz.py bundles and leg logs) all key on this class. A new subtype is always retained; the cap is 64 first-of-class bundles.
- **Checker controls, run against the real pinned binaries.** These run inside the harness self-test:
  - tautology and duplicate input clauses during an SR check, in ASCII and binary. The LSR must delete the parser-removed clauses first;
  - an SR addition that derives the empty clause before another DSR line, in ASCII, binary, and with a trailing line. The LSR must contain an explicit hinted empty clause directly after the SR line;
  - invalid DSR for both fixtures, which dsr-trim must reject with the right subtype;
  - invalid LSR: an empty clause without hints, a missing empty clause, and an unjustified empty clause. Trestle must reject each one;
  - the four retained job-1689026 reproducer proofs, which must be verified through the forward chain. Their stale elaborated LSRs must stay Trestle-rejected;
  - a recording `dsr-trim` stand-in that refuses any call without `-f`. It is driven through the audit oracle, `check_proof`, the negative controls, and the pinned `fuzz.py run_case` with a stand-in `tactix`. A planted backward call must be caught.

## Pre-submit self-test evidence

- **Staged run.** `selftest_harness.py` ran from the staged pin
  (`build/stage-srfuzz-r16r2-selftest`, since deleted), using local `/tmp` scratch. No solver was executed.
  - Result: `HARNESS SELFTEST OK (4108 checks)`, exit 0.
  - The staged manifest was unchanged afterwards (`build-logs/stage-selftest_harness.log`).
  - The harness files are byte-identical to the frozen pin's `harness/`.
- **Mutation proof.** Seven reverted copies each fail the self-test for the intended reason (`build-logs/mutant-selftests.log`):
  1. the job-1689026 `work.sh`;
  2. backward acceptance;
  3. bare-kind classes;
  4. `legstatus.py` ignoring nonzero exits;
  5. `runtest.sh` without an exit;
  6. the pre-repair dsr-trim/dsr-compress;
  7. the unpatched backward `fuzz.py`.

## Cleanup contract

This is unchanged from job 1689026:

- The job uses one anonymous node-local `mktemp -d /tmp` directory as `TMPDIR`.
- The EXIT/USR1/TERM/INT/HUP traps stop the workload's process group, promote capped logs, summaries, statuses and one reproducer per failure class, remove the scratch directory, and audit residue and pin writes.

## Limitations

- `fuzz.py --max-failures 10` stops a leg after 10 failures, so failure classes that appear later in that leg are not observed.
- A stock corpus failure has no scratch bundle. Its reproducer is the pinned corpus entry.
- A node failure, or SIGKILL after KillWait, can leave scratch behind.
