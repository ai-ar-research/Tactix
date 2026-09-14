# Direct-SR R16 fuzz checkpoint: terminal audit of job 1689026

**Verdict: FAIL.** The failure class is `proof`: the DSR→LSR→Trestle checking path
(dsr-trim, then srcheck) rejected solver-emitted SR/DSR/DSRA proofs.

- **Scale:** 64 failing cases across three SR audit legs.
  - srdefault: 24 of 2000 cases
  - srprogs: 4 of 1200 cases
  - srcapi: 36 of 1800 cases
- **Other checks:** every other oracle, leg, required reachability key, and the
  cleanup and identity checks passed.
- **Root cause is not settled** (see §5). Independent static analysis suggests the
  checker pipeline is at fault, not unsound solver reasoning. That does not change
  the verdict: this gate requires proofs to be accepted by the pinned DSR→LSR→Trestle
  path, and 64 cases were not.

Audit type: read-only. No solver, checker, test, fuzz or benchmark was run. The only
things executed were `sha256sum -c` on the two pin manifests, `sacct`, and small
ad-hoc Python scripts. Those scripts decode proofs and do unit propagation over the
retained reproducer files (§5). They are *not* the pinned checkers, and their results
are marked as inference.

Auditor date: 2026-09-13.

---

## 1. Job and pin identity (observed)

| Item | Evidence | Result |
|---|---|---|
| Job | `campaign/job-id.txt` = 1689026. `sacct`: COMPLETED 0:0, node017, 100 CPUs, 400000M, 22:31:48–22:51:48 (00:20:00), limit 90 min, batch MaxRSS 11305132K | OK |
| Source commit | `pin-srfuzz…/PIN.txt` and `pin-direct-sr-r16-canonical-proof/COMMIT` = `298c3467d11bb215237c2ccaaac62e63a949d0c1` | OK |
| Checkpoint pin manifest | `sha256(MANIFEST.sha256)` = `643417bfa5512e347e906b60d13e57452864e7f033abf23ddb4f9836d88ab53f`. Checked three times: pre-submit (`pre-submit-manifest-check.txt`), in-job (`manifest-check.txt`), and re-verified now with `sha256sum -c` (152 entries, all OK). No file newer than the manifest | OK |
| Candidate pin manifest | `8f8b32babd841d4f33beee26e415be1f8cadd758b7623432eae49e1f26000359`, 20 entries, re-verified now. Matches `candidate_pin` in PIN.txt | OK |
| Release binary | `rel/tactix` = `4d0c3668…4baccd`, the same as the candidate pin's `tactix`, both in the manifest and in-job (`identity.log`) | OK |
| Debug binary | `dbg/tactix` = `ae700889…9f023f8` (ASan/UBSan, proof shadow ON) | OK |
| Checkers | dsr-trim `f9f68939`, dsr-compress `34022e18`, srcheck `f0eef5c4`, drat-trim `f2cb7d45`, lrat-check `0c9a87d5`, cake_lpr `ab9dc6c4`, kissat `3aa8e41f`. In-job hashes equal the candidate-pin manifest hashes | OK |
| Harness immutability | Every file in `pin…/harness/` is byte-identical to `campaign/`. `residue.txt` reports `pin_writes_after_start=0` | OK |
| Legs and seeds | `work.sh` matches SUBMISSION.md: 5 SR audit modes, 5 prior modes, 5 stock fuzz.py/corpus legs, tests, and self-tests | OK |

## 2. Preflight and self-tests (observed)

- **Harness self-test** (`selftest_harness.log`): `HARNESS SELFTEST OK (4046 checks)`,
  exit 0.
  - The `FAIL …` lines before the OK line are diagnostics printed by injected controls:
    fake C-API drivers and mutated attach/replay cases. `selftest_harness.py` raises
    `SystemExit` if any expectation fails.
  - The frozen pre-submit staging run ends with the same OK line and the same 4046
    count.
- **Stock `selftest.sh`**: `SELFTEST OK`, exit 0.
  - kissat control: 150 cases, 0 failures. cadical control: 100 cases, 0 failures.
  - Planted faults were all flagged: lie-unsat-as-sat and negate-model as `model`,
    truncate-proof as `proof`.
  - ddmin shrink works. The duplicate-literal conversion control works.
- **`test_modelcheck.py`**: exit 0.
- **Focused tests** (`tests.log`, 20 runs across Release and Debug): all exit 0, with 0
  fail lines and 0 sanitizer lines.
  - Coverage: test_api_c, test_proof, test_api_instances, test_online_api,
    test_online_all_actions, test_online_inprocess, test_random_online, trace_replay,
    micro_proofs.
  - Note: `test_inprocess_program` prints nothing in either build, so it passes on exit
    code alone.

## 3. Leg-by-leg results

"Failing" counts come from `summary.failures` in each `*-summary.json`. The
AUDIT exit codes come from the leg logs.

| Leg | Cases | Failing | AUDIT exit | Missing required keys | Notes |
|---|---|---|---|---|---|
| srdefault | 2000 | **24** | **1** | none (22 required) | 51 `sr_forward_rejected` check instances (release, debug and -S variants). diff 2000/2000 agree. debug=release 2000/2000 |
| srrandom | 2000 | 0 | 0 | none (12) | replay transitions identical 1839/1839. D/P/X identical. 92/92 replay-corruption negatives rejected |
| srprogs | 1200 | **4** | **1** | none (16) | `dsr_proof_rejected_at_srcheck` 6 plus `dsra…` 2, i.e. r1 and dbg for 4 cases. Determinism and debug=release 1200/1200 |
| srcapi | 1800 | **36** | **1** | none (32) | 34 `sr_forward_rejected` plus 2 default-mode `dsr_proof_rejected_at_srcheck` (sc000518, sc001365). Cross-build identical 600/600 |
| srinterrupt | 21 | 0 | 0 | none (5) | 34 SIGTERM runs consistent. Partial-SR proofs verified. Max latency 969 ms |
| regress | 8 | 0 | 0 | none (15) | |
| interrupt | 79 | 0 | 0 | none (12) | |
| random | 4000 | 0 | 0 | none (81) | Debug SIGTERM latency max 9729 ms (Debug build, no failure raised) |
| default | 3000 | 0 | 0 | none (44) | |
| progs | 1000 | 0 | 0 | none (20) | |
| stock random-dsr | 1200 fuzz + 30 corpus | 0 | fuzz 0 / corpus 0 | `--require-reached ParityModel` satisfied (88) | 138 non-terminating (11.5%): 1062 effective cases |
| stock default-dsr | 1000 + 30 | 0 | 0 | — | 17 non-terminating |
| stock random-drat | 500 + 30 | 0 | 0 | — | 60 non-terminating: 440 effective |
| stock random-lrat | 500 + 30 | 0 | 0 | — | 47 non-terminating: 453 effective |
| corpus-dbg-dsr | 30 | 0 | 0 | — | Entry 022 passes on the recorded crash-oracle-only condition |

**Workload exit 0 does not mean pass (harness defect, observed).** `work.sh` ends with
`wait $PIDS` and then an `echo`, so it never collects the leg exit statuses. As a
result `work exit=0` and Slurm `COMPLETED 0:0` were reported even though three legs
logged `AUDIT-… exit=1`. The verdict above is based on the leg logs and summaries,
not on the job exit code.

### Required reachability keys (observed)

- `missing` is `[]` in all 10 `*-reachability.json` files.
- The new srcapi keys that SUBMISSION.md said were only statically plausible were all
  reached:

| Key | Cases |
|---|---|
| `capi:proof_after_add_accepted` | 30 |
| `capi:proof_after_add_unsat_verified` | 21 |
| `capi:proof_late_rejected` | 30 |
| `capi:proof_late_solved_checked` | 30 |
| `capi:proof_duplicate_rejected` | 30 |
| `capi:proof_duplicate_unsat_verified` | 19 |
| `capi:sr_witness_forward_verified_general` | 80 |

- There is no harness gap on required keys.

### Oracle families

- **SR witnesses** (observed): solver-emitted witnesses were present and
  checked:

  | Leg | Proofs with an SR witness | Forward-verified | Forward-rejected |
  |---|---|---|---|
  | srdefault | 622 | 571 | 51 |
  | srcapi | 619 | 585 | 34 |
  | srinterrupt | 42 | 42 | 0 |

  The srprogs `sr_forward_verified` count is 74. The arbiter confirmed the original
  witness in every arbitrated case in srdefault, srprogs and srcapi. srrandom has 10
  `original_valid_invalid` arbiter outcomes, but the harness does not count them as
  failures.
- **DSR→LSR→Trestle** (observed): **FAILED**. See §4.
- **DRAT/LRAT compatibility controls** (observed):
  - SR declines are recorded in every SR leg: `sr:declined_drat` and
    `sr:declined_lrat` in srdefault (61/57), srprogs (102/111), srcapi (183/183) and
    srrandom (2/2).
  - DRAT and LRAT proofs verified in every leg, and the stock DRAT/LRAT legs had 0
    failures.
  - The 12-field compatibility steps were identical in 600/600 cases.
- **SAT models** (observed): `model_ok` counts are non-zero. No `model` failures in
  any leg, and `neg_model_rejected` fired in every relevant leg.
- **Differential against kissat** (observed): `diff_agree == diff_compared` in every
  leg (srdefault 2000, srrandom 1839, srprogs 178, srcapi 1620), and 0 differential
  failures in the stock legs.
- **Replay** (observed):
  - srrandom: `replay_transitions_identical` 1839/1839.
  - `trace_replay` passed in both builds.
  - The negative replay-corruption controls were all rejected (92/92).
- **Online reconfiguration** (observed):
  - `test_online_inprocess` (reconfigure) and `test_online_api` pass in both builds.
  - srrandom/random `program_coverage` shows about 800k 13-field steps.
  - srcapi `capi:*` progress keys were reached.
- **Interruptions** (observed):
  - srinterrupt: 0 failures, `sr_term_accounting_consistent` 34.
  - interrupt: 0 failures.
  - Sampled SIGTERM runs ended with a clean UNKNOWN.
- **Negative controls** (observed):
  - Every `negative:*` required key was reached, and no negative-control failures
    occurred.
  - `neg_dsr_constructed_no_conflict_error_at_dsr-trim` appears 1–2 times per leg.
    These are counted as controls, not failures.
- **Sanitizers, crashes and API errors** (observed):
  - A grep of all logs for AddressSanitizer, runtime error, LeakSanitizer,
    Segmentation, core dumped, Traceback, Killed and oom returns nothing.
  - No `crash` or `capi` failure kinds appear in any summary. The API contract checks
    were reached.
- **Memory cap** (observed):
  - `capwrap.log` has 0 `CAPKILL` lines. The highest per-process HWM is 3,033,936 kB,
    under the 16 GiB cap.
  - Summary peak RSS is at most 1,048,216 kB (srcapi Release).
  - No cgroup OOM evidence was found.

## 4. Failure detail (observed)

All 64 failing cases are `real=True` and have the kind `proof`, `proof-debug`,
`proof-S`, `proof-r1` or `proof-dbg`. Each message has the form
`format={dsr|dsra} [SR forward] status=rejected stage=srcheck`.

- In every retained log, srcheck prints `s INVALID` and exits 0.
- There are no checker timeouts, no `skipped_deadline`, and no differential or model
  failures.

| Leg | Failing tags |
|---|---|
| srdefault (SR forward; 2 dsra) | sd000031 48 280 393 543 663 752 769 777 827 946(dsra) 997 1106(dsra) 1152 1155 1177 1277 1357 1431 1501 1827 1845 1917 1968 |
| srprogs (default-mode check) | sp000366 438 859 1165(dsra) |
| srcapi (SR forward unless noted) | sc000046 51 74 80 234(dsra) 237(dsra) 246 295 298 338 362 423 518(default mode) 530(dsra) 673(dsra) 808 838 844 946 1005 1235 1306 1312 1341 1365(default mode) 1387(dsra) 1402 1416 1421 1460(dsra) 1536(dsra) 1558 1580 1630 1636 1761 |

**Retained reproducers** (under
`benchmarks/reports/2026-09-13-direct-sr-r16-claude-fuzz/campaign/out-1689026/reproducers/`):

- `00-srcapi_proof/` covers class `srcapi:proof` (sc000234, dsra, batch scenario;
  `release.spec` is the driver spec).
- `01-srdefault_proof/` covers `srdefault:proof` and `srdefault:proof-debug`
  (sd000031, dsr). Command: `rel/tactix case.cnf on.proof`.
- `02-srdefault_proof-S/` covers `srdefault:proof-S` (sd000280, dsr, SIGTERM at 0.2 s).
- `03-srprogs_proof-dbg/` covers `srprogs:proof-dbg` and `srprogs:proof-r1`
  (sp000859, dsr). Command: `rel/tactix --replay=trace case.cnf r1.proof`. This is the
  smallest reproducer: 12 variables, 26 clauses, 13 proof steps.

Promotion summary: `bundles.txt` reports 6 classes and `uncovered_classes=0`.

**Coverage caveat:** promotion works per kind, so the srcapi *default-mode* rejection
subtype (sc000518, sc001365) has no bundle of its own. Its scratch was deleted,
consistent with `failure_bundles_dropped=32`. Seeds are recorded in
`srcapi-summary.json`. The srprogs bundle `03` is the retained example of a
default-mode rejection.

## 5. Root-cause evidence (inference, not a verdict change)

I used ad-hoc Python checks (unit propagation plus SR/RAT semantics) on the retained
files only. They are not the pinned checkers and may differ from dsr-trim or srcheck
in edge cases.

1. **Solver proofs look valid.** I decoded the solver-emitted binary and text
   DSR/DSRA proofs in all four bundles (`00/release.proof`, `01/on.proof` and
   `dbg.proof`, `02/on.proof` and `t.proof`, `03/r1.proof`). Each is a valid
   refutation against its `case.cnf` under standard SR semantics:
   `C|σ` is satisfied, and for every `D` in `F`, `F ∧ ¬C ⊢₁ D|σ`. The check treats
   extension-variable lines as RAT. Release and Debug proofs are byte-identical where
   both exist.
2. **The formulas share unusual features.** All failing reproducer CNFs contain
   duplicate clauses, and `00`, `01` and `02` each contain one tautological input
   clause. `gen_sr.py:19` generates both on purpose.
   - In `01` and `02`, the backward-mode LSR (`*.verified.lsr`, accepted) deletes the
     tautology in its first deletion line.
   - The forward-mode LSR (`*.fail-forward-srcheck.lsr`, rejected) never deletes it.
   - A strict hint-order replay of those forward LSRs accepts them only if a
     substituted tautology counts as satisfied. This suggests srcheck, or the hints
     dsr-trim emits, do not treat a live tautological clause as satisfied under the
     witness.
3. **In `03`, dsr-trim's own LSR drops clauses its witness depends on.** No
   tautology is involved here, and the default-mode check rejected.
   - dsr-trim minimized the witness `{¬4, ¬8, 7↔12, 5↔2, 1↔¬3}` into assignments that
     rely on units propagated from clauses 16, 20 and 24. Clauses 16 and 24 are the
     duplicated `(-7 -4)`.
   - But line 26 of the emitted LSR deletes those same clauses before SR line 27.
   - The emitted SR step is therefore not valid for the post-deletion formula (the
     reduct of clause 3 `(8 10)` is `(10)`, which is not implied). The same witness is
     valid on the formula without those deletions.

**Inference:** the most likely defect is in the dsr-trim/srcheck toolchain's handling
of tautological and duplicate input clauses around SR lines, not an unsound solver
step. This is unconfirmed: I did not run the pinned checkers, and the CNFs of the
other 60 failing cases were not retained.

- Before any gate re-run, the owner must confirm the diagnosis on these reproducers
  with the pinned binaries.
- The self-test should gain a control for tautological and duplicate input clauses on
  the forward SR path.

## 6. Cleanup (observed)

- `residue.txt`: `scratch_exists=no`, `new_entries_vs_job_start=0`,
  `user_entries_newer_than_start=0`, `pin_writes_after_start=0`.
- Scratch `/tmp/tmp.H1VTJ9wrBQ` was 1776 (as reported by `du`) before removal.
- The job ended normally through the EXIT trap. There was no USR1 or TERM signal path
  (elapsed 1200 s, well under the limit).

**Cleanup: PASS.**

## 7. Harness findings (not the cause of FAIL)

- **H-A:** `work.sh` never collects per-leg exit statuses (§3), so a job exit of 0 hides
  AUDIT failures. Fix this before the next campaign.
- **H-B:** promotion per `MODE:KIND` hides subtypes that share a kind (srcapi
  default-mode versus forward-mode rejections).
- **H-C:** no self-test control covers tautological or duplicate input clauses on the
  forward DSR/DSRA SR path.

## 8. Terminal statement

**FAIL.**

- Failure class: `proof` (srcheck rejection on the DSR→LSR→Trestle path).
- 64 cases: srdefault 24, srprogs 4, srcapi 36.
- 0 missing required keys. Cleanup passed. Identity and immutability verified.
- Retained reproducers are under `campaign/out-1689026/reproducers/` (00–03).
- The cause is most likely in the checker pipeline rather than the solver (inference,
  §5), but this fuzz gate does not pass.
