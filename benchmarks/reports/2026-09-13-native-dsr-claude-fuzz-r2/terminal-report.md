# Native-DSR replacement fuzz checkpoint — terminal audit of job 1688573

**Verdict: PASS.** At the same `8ad66789` Release and Debug binaries as failed
job 1688465, with only `dsr-trim` replaced by the patched build and the harness
repaired, every leg completed with 0 failures. That covers the canonical DSR
path (DSR → `dsr-trim` → LSR → Trestle `srcheck`), DSRA, DRAT, LRAT, models,
the Kissat differential, replay, interruption, negative controls and
sanitizers. Every required reachability key in all five audit modes was
reached. The remaining weaknesses below are about coverage; none is a failure.

Scope: a read-only audit of `campaign/` (frozen harness, `out-1688573/`,
`slurm-1688573.log`) and of the identity files and manifests of both pins. No
job was submitted, no solver or checker was run, and nothing was changed in
the pin or the campaign. **Observed** means read directly from an artifact.
**Inferred** means reasoned from artifacts or source but not directly shown by
them. Predecessor reports: [first audit](../2026-09-13-native-dsr-claude-fuzz/terminal-report.md)
and [checker repair](../2026-09-13-dsr-trim-tautology-hash-fix.md).

## Identity (observed)

| Item | Value |
|---|---|
| Job | 1688573, `fuzz-nativedsr-8ad66789-r2`, one non-array allocation on node017: 100 CPUs, 400000 MiB, 1:30:00, `--signal=B:USR1@240`. Same shape as 1688465 |
| Terminal state | `work exit=0 elapsed=899s`. No `signal.txt`, so no USR1/TERM path ran. `work.log` ends `== work done ==` |
| Pin | `build/pin-dsrfuzz-8ad66789-r2`. `MANIFEST.sha256` = `49bc8bf7…a0c5d` (4527 entries). `sha256sum -c` passed at job start (`manifest-check.txt`) |
| Source | commit `8ad667892abe…` and tree `27dfc883…`, both the same as the first pin |
| Solver binaries | Release `tactix` `eee6fc0d…` and Debug `tactix` `5a7a2710…`. Byte-identical to the first pin's manifest entries |
| Checkers | `dsr-trim` changed from `114b3662…` to `f9f68939…`, matching the repair report. `dsr-compress`, `srcheck` (Trestle 03ac9418), `drat-trim`, `lrat-check`, `cake_lpr` and `kissat` are unchanged |
| Manifest diff vs first pin | No entry removed. Changed hashes only in `PIN.txt`, `harness/{audit.py,batch.sh,capwrap.sh,selftest_harness.py,work.sh}` and `root/tools/checkers/bin/dsr-trim`. Added entries: 142+142 `{rel,dbg}/CMakeFiles`, two build logs, `harness/d000848.cnf`, and the `root/build/debug/tactix` link |
| Harness | All 10 campaign files hash-match the manifest's `harness/` entries. `capidriver.c`, `pargen.py`, `runtest.sh` and `tracecheck.cpp` are byte-identical to the first campaign |

*Inferred:* the added manifest entries are build metadata and one input file,
none of which executes. The r2 pin is a hardlink copy (`cp -al`) of the first
pin. The start-of-job manifest check rules out drift at that moment.

## Preflight and tests (observed; treated as mandatory)

- **`d000848` checker preflight** (`preflight.log`): empty DSR proof on the
  retained CNF.
  - Old `dsr-trim` `114b3662…`: exit 1, `Fatal error: Clause 3 was already
    deleted.`, `CONTROL OK`.
  - Patched `f9f68939…`: exit 0, `s VERIFIED UNSAT`, `PREFLIGHT OK`.
  - Covers the `dsr-trim` stage only; this preflight did not run Trestle.
- **Focused tests** (`tests.log`): 10/10 `TEST` lines exit 0, 0 `FAIL` lines,
  0 sanitizer lines.
  - Release and Debug each ran `test_proof`, `test_api_c`,
    `test_random_online`, `trace_replay` and `micro_proofs`.
  - Each tail shows its OK line; both `micro_proofs` logs end
    `micro_proofs OK`.
- **Micro proofs: direct non-RAT SR and its negative controls.**
  - *Observed:* `micro_proofs.sh` runs under `set -euo pipefail`. It prints
    `micro_proofs OK` only after all of these pass:
    - `check_dsr direct-sr.cnf direct-sr.dsr forward`: a two-row permutation
      witness through `dsr-trim -f`, `dsr-compress` and Trestle, each required
      to print `s VERIFIED UNSAT`.
    - The half-witness control: `dsr-trim` must fail with `No UP
      contradiction`.
    - The incomplete-hint LSR control: Trestle must print `s INVALID` and must
      not print VERIFIED.
  - The exit-77 skip path did not run (exit 0).
  - *Limitation:* each step's checker stdout was not promoted. Only the final
    OK line proves they passed.
- **Harness self-test**: `HARNESS SELFTEST OK (584 checks)`, exit 0. Every
  `ok()` failure raises `SystemExit`, so every check passed, including:
  - the r2 capwrap controls: exec in place, early SIGTERM to the real pid,
    SIGKILL of a process that ignores SIGTERM, and the lowered-cap RSS kill
    with its `CAPKILL` log line;
  - the stale-interrupt-key guard.

  The 8 `FAIL` lines are the expected planted-defect detections.
- **Stock `selftest.sh`**: `SELFTEST OK`, exit 0.
  - Kissat 150 and CaDiCaL 100 cases, 0 failures.
  - Planted defects all flagged: lie-UNSAT-as-SAT, negated model, truncated
    proof.
  - ddmin passed, and so did the duplicate-literal native-LRAT control.
- **`test_modelcheck.py`**: 11 tests OK, exit 0.

## Audit legs (observed; all `AUDIT-* exit=0`, 0 failing, 0 checker-timeout-only, `missing: []`)

| Mode | Cases | Proofs verified: DSR / DSRA / DRAT / LRAT | Models, differential, replay | Required keys |
|---|---|---|---|---|
| regress | 8 (134 programs) | 2 / — / 2 / 2 | `model_ok` 11. Kissat 8/8. Replays Debug 5/5 and Release 5/5 identical, transitions 5/5 | 15/15 |
| interrupt | 79 | — | see Interruption | 12/12 |
| random | 4000 (166634 programs) | 924 / 325 / 480 / 484 | `model_ok` 6620. Kissat 3679/3679. Replays Debug and Release 3679/3679 identical, transitions identical 3679. 3076 cases with an effective transition | 82/82 |
| default | 3000 | 603 / 190 / 378 / 384 | `model_ok` 5025. Kissat 2990/2990. Debug = Release 2990/2990 | 44/44 |
| progs | 1000 | 60 / 22 / 28 / 30 | `model_ok` 884. Kissat 512/512. Determinism and Debug = Release 1000/1000 | 20/20 |

- **Consistency with 1688465 (observed; causes inferred).**
  - Default mode: first-run verified + `error` counts were DSR 599+4 and DSRA
    188+2, exactly r2's 603 and 190.
  - Random mode: r1 had DSR 915+10 and DSRA 326; r2 has 924 and 325. Timeouts
    went from 317 to 321, and differential comparisons from 3682 to 3679.
  - *Inferred:* the random-mode shift comes from wall-clock timeouts under
    load, not from a solver change, since the binaries are identical.
- **Negative controls, random mode.** All rejected:
  - DSR empty-only 234 and constructed/truncated no-conflict 234/223;
  - LSR hints-removed 234;
  - DRAT 119/115;
  - LRAT 241/236;
  - corrupted models 2720;
  - corrupted replay verdicts 198/198.
- **Negative controls, default and progs.** Default mode has the same classes
  (193/193/172/193/96/85/176/173 and models 2270). `progs` rejected malformed
  one-hot tuples, invalid one-hot parameters and wrong-rule parameters, each
  125/125.
- **Inapplicable controls** are counted under `*_inapplicable_*` and not
  credited.
- **Interrupt key repair.** The requirement is now
  `sweep_{unlimited,finite}_interrupted_any` plus the per-format keys. Reached:
  `_any` 18/18, DSR/DRAT/LRAT 6 each, Debug 6.
- **Interruption.**
  - Interrupt mode: sweep Release 48/48 and Debug 6/6 clean UNKNOWN. 24 traces
    ran out before the SIGTERM. F2 fixed plus the three sweep fractions: 4/4
    clean.
  - Random mode: SIGTERM Release 91 = 91 clean, Debug 55 = 55 clean. Timeout
    arms: Release 200/200, Debug 321/321. Default mode: Release timeout 10/10.
  - Every SIGTERM ended in a clean UNKNOWN or a finished verdict.
- **Sanitizers.**
  - The ASan/UBSan Debug build ran in every audit mode, the Debug corpus leg
    and the Debug focused tests.
  - 0 crash-class failures and 0 sanitizer lines in the tests.
  - *Limitation:* per-case stderr is visible only through failure summaries.
- **Memory cap.**
  - `capwrap.log`: 274 `HWM` lines (the maximum is 3,594,648 kB), 0 `CAPKILL`
    lines, no other lines.
  - Audit peak RSS maximum 873,232 kB (random, Debug).
  - *Observed:* the wrapper was active in the campaign.
  - *Not observed:* which mode each `HWM` line came from; `pid` is the only
    attribution.

## Stock `fuzz.py` legs (observed; every corpus and fuzz exit 0)

| Leg | Corpus | Fuzz | Reached a verdict (UNSAT) | Failures |
|---|---|---|---|---|
| random-dsr | 30/30 | n=1500, `--require-reached ParityModel` (137) | 1339 (554) | 0 (r1: 9 proof) |
| default-dsr (CLI default) | 30/30 | n=1200 | 1185 (507) | 0 (r1: 7 proof) |
| random-drat | 30/30 | n=600 | 531 (215) | 0 |
| random-lrat | 30/30 | n=600 | 531 (224) | 0 |
| corpus-dbg-dsr (ASan/UBSan) | 30 entries, 0 failures | — | — | 0 |

- The DRAT and LRAT verdict lines match 1688465 exactly.
- In default-dsr, one case went from UNSAT to TIMEOUT (508 → 507).

## Cleanup (observed)

- `residue.txt`: `scratch_exists=no`, `new_entries_vs_job_start=0`,
  `user_entries_newer_than_start=0` on node017.
- `bundles.txt`: `bundles_available=0 fuzzpy_bundles=0`, and no `reproducer/`
  was promoted, consistent with 0 failures.
- Promoted output is 186 KB.

## Remaining weaknesses (none blocking)

1. **No solver-emitted SR witness exists yet.**
   - *Observed:* `dsr_proofs_with_sr_witness=0` and `dsr_sr_witness_lines=0`
     in every mode, with 0 size-skipped scans. In the pinned source,
     `add_sr` is called only from `tests/test_proof.cpp`.
   - *Inferred:* no rule at `8ad66789` can emit a non-RAT SR step. Non-RAT SR
     acceptance is therefore shown only by the fixed micro proof, the
     serializer test and the harness self-test — never by fuzzing.
   - This gate must be repeated once a rule emits SR (the
     `substitution_redundancy` work).
2. **026 stays unchecked under Debug.** Corpus entry
   `026-sweep-drat-admission-gate` still times out at 60 s, so its proof oracle
   never ran. Corpus entries 011 and 022 pass only by their crash-oracle
   condition.
3. **Timeouts shrink oracle coverage.**
   - `fuzz.py` legs: random 10.7–11.5% and default 1.2% of cases got no oracle.
   - Audit random mode: 321/4000 Debug timeouts.
4. **Termination latency near the SIGKILL grace.**
   - Random-mode Debug timeout-arm maximum was 9339 ms against `GRACE=10 s`;
     r1 was 1955 ms. It still counted as clean, 321/321.
   - *Inferred:* the cause is load on 100 busy CPUs under ASan.
   - A slightly slower case would be SIGKILLed and flagged, so the margin is
     thin.
5. **The cap lane is armed but never fired.**
   - The kill path is shown only by the self-test's lowered-cap control;
     campaign `CAPKILL` = 0.
   - Stock `fuzz.py` legs and `selftest.sh` are not wrapped. *Inferred from
     grep:* `fuzz.py` sets only `RLIMIT_CPU`.
   - `capwrap.log` does not attribute lines to modes.
6. **Rule reach gaps in stock legs.**
   - Random legs: `VariableCompaction=0`, and `BveBackwardUnit` was 0 or 1.
   - `default-dsr` never fired Probe, HyperBinaryRes, BCE, CCE, Instantiate,
     HyperTernaryRes, SubsumeForward, BveBackward or VivifyFlush.
   - *Inferred:* these are small instances under DefaultPolicy's schedule.
7. **Evidence limits.**
   - Checker stdout is kept only for the preflight.
   - Micro-proof steps are proven by the fail-fast OK line, not per-step output.
   - The pin is a hardlink copy of the old pin; the start-of-job manifest check
     is the immutability evidence.

## Terminal verdict

**PASS** for job 1688573 at source `8ad66789`, build manifest `49bc8bf7…`, and
patched `dsr-trim` `f9f68939…`. The 1688465 failure is resolved by the checker
patch alone: identical solver binaries now give 0 DSR/DSRA failures, where r1
had 23 cases. Every required key and preflight, test, control and cleanup
check is satisfied. Weakness 1 limits the claim: this checkpoint shows
native-DSR acceptance for the RAT-expressible steps Tactix emits today, not for
solver-emitted substitution witnesses.
