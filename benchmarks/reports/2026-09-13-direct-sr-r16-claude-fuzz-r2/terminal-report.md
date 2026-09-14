# Direct-SR R16 fuzz checkpoint r2: terminal audit of job 1689057

**Verdict: PASS.** Every one of the 20 expected legs recorded exit 0. Every audit
and stock-fuzz leg reports 0 failing cases, and no required reachability key is
missing. The forward DSR → `dsr-trim -f` → decompressed LSR → pinned Trestle chain
accepted every solver-emitted DSR/DSRA certificate, including all SR-witness
certificates. Identity, immutability and cleanup checks all pass.

This closes the R16 Claude fuzz gate for source `298c3467` checked with the
repaired checker pin (dsr-trim `0b8cc0ef` + patch `a5653c3a`). It does not land
the solver increment.

Audit type: read-only. The auditor ran no solver, checker, test, fuzz or benchmark.
The only things executed were `sacct`, `sha256sum -c` on the pin manifest, `cmp`/`diff`
of the harness against `campaign/`, and small Python readers over the promoted JSON
and log artifacts. Auditor date: 2026-09-14. The submission record is
[SUBMISSION.md](SUBMISSION.md).

## 1. Scheduler, pin and identity (observed)

| Item | Evidence | Result |
|---|---|---|
| Job | `job-id.txt` = 1689057. `sacct`: COMPLETED 0:0 (batch and extern), node017, 100 CPUs, 400000M, 00:03:15–00:23:23 (20:08 of 1:30:00), batch MaxRSS 9,636,656K. No `signal.txt`, so the job ended normally through the EXIT trap | OK, but not used as the pass criterion |
| Workload aggregate | `work-exit.txt`: `work exit=0 elapsed=1206s`. `work.log`: `WORK-VERDICT PASS bad=0`, `aggregate exit=0` | OK |
| Aggregator completeness | `status/expected` lists 20 legs, and there are exactly 20 `*.exit` files, all `0`, with no unexpected names. `work-status.txt` has 20 `LEG … exit=0` rows, 20 `TEST` rows (10 tests × Release/Debug, each `exit=0 fail_lines=0 sanitizer_lines=0`), no `BAD` line, and `WORK-VERDICT PASS legs=20 bad=0`. `tests.log` has exactly 20 `TEST` lines, matching `tests/list` | OK |
| Source | `PIN.txt`: `298c3467d11bb215237c2ccaaac62e63a949d0c1`. Candidate pin manifest `8f8b32ba…0359` | OK |
| Checkpoint pin | `build/pin-srfuzz-direct-r16-298c3467-r2`. `sha256(MANIFEST.sha256)` = `cb302bda…627e` pre-submit, in job, and re-verified now: 168 entries, all OK. 0 writable entries. 0 files newer than the manifest | OK |
| Exact identities | 13 hashes are checked three times (`pre-submit-manifest-check.txt`, `identity-precheck.txt`, in-job `identity.log`): Release `4d0c3668…`, Debug `ae700889…`, srdriver-rel `0a4cc0fe…`, srdriver-dbg `7c18c160…`, tracecheck `232af674…`, dsr-trim `a1d7251c…`, dsr-compress `89aaab76…`, srcheck `f0eef5c4…`, drat-trim `f2cb7d45…`, lrat-check `0c9a87d5…`, cake_lpr `ab9dc6c4…`, kissat `3aa8e41f…`, cadical `ecf73365…`. dsr-trim and srcheck equal the [forward-elaboration repair](../2026-09-13-dsr-trim-forward-elaboration-fix.md) | OK |
| Harness | All 19 entries of `pin/harness/` are byte-identical to `campaign/`, including `fixtures/`. The pinned `root/tools/fuzz/fuzz.py` calls `dsr-trim -q -f -c`. In the pinned `micro_proofs.sh`, `check_dsr` defaults to forward | OK |
| Legs, seeds, sizes | `work.sh` matches SUBMISSION.md and job 1689026 | OK |

## 2. Self-tests and focused tests (observed)

- **Harness self-test:** `HARNESS SELFTEST OK (4108 checks)`, exit 0. This is the
  same count as the staged pre-submit run. The `FAIL …` lines before it are
  diagnostics from planted controls (fake drivers, mutated attach/replay cases);
  the self-test exits nonzero if any expectation is unmet.
- **Stock `selftest.sh`:** `SELFTEST OK`, exit 0.
  - The kissat control (150 cases) and cadical control (100 cases) had 0 failures.
  - The planted lie-unsat-as-sat and negate-model faults were flagged as `model`,
    and truncate-proof as `proof`.
  - The ddmin and duplicate-literal controls pass.
- **`test_modelcheck.py`:** exit 0.
- **Focused tests:** 20/20 exit 0, with 0 FAIL lines and 0 sanitizer lines in both
  Release and ASan/UBSan Debug.
  - Tests: test_proof, test_api_c, test_api_instances, test_random_online,
    test_online_inprocess (reconfigure), test_online_api (revision/transaction),
    test_online_all_actions (27 actions), test_inprocess_program, trace_replay and
    micro_proofs (forward DSR).
  - `test_inprocess_program` prints nothing, so it passes on exit status alone.

## 3. Leg results (observed)

Failing counts come from `summary.failures`, reachability from `*-reachability.json`,
and exits from `status/*.exit` and the `AUDIT-*` log lines.

| Leg | Cases | Failing | Checker-timeout-only | Exit | Required keys (missing) | Differential agree/compared |
|---|---|---|---|---|---|---|
| srdefault | 2000 | 0 | 0 | 0 | 22 (none) | 2000/2000 |
| srrandom | 2000 | 0 | 0 | 0 | 12 (none) | 1840/1840 |
| srprogs | 1200 | 0 | 0 | 0 | 16 (none) | 178/178 |
| srcapi | 1800 | 0 | 0 | 0 | 32 (none) | 1620/1620 |
| srinterrupt | 21 | 0 | 0 | 0 | 5 (none) | — |
| regress | 8 | 0 | 0 | 0 | 15 (none) | 8/8 |
| interrupt | 79 | 0 | 0 | 0 | 12 (none) | — |
| random | 4000 | 0 | 0 | 0 | 81 (none) | 3675/3675 |
| default | 3000 | 0 | 0 | 0 | 44 (none) | 2990/2990 |
| progs | 1000 | 0 | 0 | 0 | 20 (none) | 512/512 |
| stock random-dsr | 30 corpus + 1200 | 0 | — | 0 | `--require-reached ParityModel` (87) | 1061 effective |
| stock default-dsr | 30 + 1000 | 0 | — | 0 | — | 983 effective |
| stock random-drat | 30 + 500 | 0 | — | 0 | — | 440 effective |
| stock random-lrat | 30 + 500 | 0 | — | 0 | — | 454 effective |
| corpus-dbg-dsr | 30 | 0 | — | 0 | — | — |

In every mode, `done == cases`, `skipped_deadline=0`, and
`root_only_rules_at_search_level=[]`.

### Oracle families (observed)

- **DSR → LSR → Trestle.**
  - Verified DSR/DSRA certificates: srdefault 1350/276, srrandom 1156/244,
    srprogs 136/26, srcapi 638/113, srinterrupt 24/18.
  - SR-witness certificates verified forward: srdefault 622, srcapi 621,
    srprogs 82, srinterrupt 42, srrandom 33.
  - No `*_proof_rejected`, `*_proof_error` or `sr_forward_rejected` counter is
    nonzero in any mode, and there were no checker timeouts.
  - This is the exact class that failed in job 1689026 (64 cases).
- **Direct-SR effective transitions** (required-key cases):
  - srdefault: `applied_dsr` 248, `applied_dsra` 48, `applied_UNSAT` 283,
    arbiter-confirmed 283.
  - srprogs: `sr_one/batch/orbitopal/general/multi_applied` 144/139/71/68/132.
  - srcapi: `sr_progress_one/batch/subset/orbitopal/general/multi` 140/125/136/73/117/140;
    `sr_witness_forward_verified_{one,batch,orbitopal,general}` 78/68/44/80;
    `sat_model_after_sr_progress` 330.
  - srrandom: `producer_orbitopal` 580, `explicit_instances` 6, `applied_dsr` 18.
  - srinterrupt: `sr_probe_applied` 10.
  - stock random-dsr: `SubstitutionRedundancy` fired in 9 cases.
- **DRAT/LRAT compatibility.**
  - SR is declined for DRAT/LRAT in every SR mode: srdefault 61/57, srprogs
    102/111, srcapi 183/183, srrandom 2 (DRAT).
  - DRAT and LRAT certificates verified in every mode.
  - The 12-field compatibility steps were identical in 600/600 cases.
- **Models.** `model_ok` is nonzero in every mode that yields SAT, with no model
  failure. The falsified-model negative control was rejected every time it ran
  (for example srdefault 981, random 2719).
- **Replay and determinism.**
  - `replay_transitions_identical` 1840/1840 in srrandom and 3675/3675 in random.
  - srprogs determinism and debug=release 1200/1200. srcapi cross-build identical
    600/600.
  - Replay-corruption negatives rejected: srrandom 92/92, random 199/199,
    progs 66/66, regress 5/5.
  - `trace_replay` passes in both builds.
- **Reconfiguration.** `progs:multi_revision_resumed` 274. The srcapi C-API
  progress/proof-attach keys were all reached (`proof_after_add_accepted` 30,
  `proof_late_rejected` 30, `proof_duplicate_rejected` 30). test_online_inprocess
  and test_online_api pass in both builds.
- **Interruption.**
  - SIGTERM runs balance exactly against clean-UNKNOWN runs plus verdict-before-signal
    runs: srinterrupt default 20=20, probe 11=11, random release 12=12,
    random debug 2=2; srdefault 8 = 7 + 1; srrandom Debug timeout 160 = 158 + 2.
  - The interrupt mode's 12 required keys were all reached.
- **Negative controls.** Every `negative:*` required key was reached, and no
  `neg_*_accepted` counter exists in any mode.
- **Sanitizers and crashes.** A grep of all 91 promoted files for AddressSanitizer,
  LeakSanitizer, UndefinedBehaviorSanitizer, runtime error, Segmentation, core dumped,
  Traceback, Killed, OOM, Assertion, terminate called, harness-exception and CAPKILL
  matches nothing.
- **Memory.** `capwrap.log` has 0 CAPKILL lines, and the maximum per-process HWM is
  3,033,936 kB. Summary peak RSS is at most 626,872 kB in srdefault.

### Explained observations (not failures)

1. **Negative control errors instead of rejections.**
   `neg_dsr_constructed_no_conflict_error_at_dsr-trim` appears 1 time in srdefault,
   2 in srrandom and 1 in srcapi.
   - The control gives dsr-trim a one-line proof restating the first input clause. It
     fails only if that proof is `verified`, and none was.
   - These counts are identical to job 1689026, which used the backward checker on
     the same seeds. That fits a deterministic per-input checker refusal.
   - The exact dsr-trim message was not retained, because this control does not
     retain checker output.
2. **Arbiter split between database views.** `sr_arbiter_original_valid_invalid`
   appears 10 times in srrandom (same as job 1689026) and 3 times in random.
   - In these cases the first SR line is valid against one of the arbiter's two
     database views (with or without prior deletions) but not the other.
   - That still counts as calibrated, and the pinned chain verified those
     certificates.
3. **Differential-only UNSAT checks.** `srcapi_late_UNSAT_differential_only` = 17.
   - A late proof attachment is rejected by contract, so no certificate exists and
     the UNSAT verdict is checked against kissat only.
   - `capi:proof_late_solved_checked` = 30 was reached.
4. **Non-terminating stock cases.** Stock legs had cases that hit the 10 s limit:
   random-dsr 139, default-dsr 17, random-drat 60, random-lrat 46. No oracle ran on
   those cases, so the effective counts above exclude them.
   - Corpus entries 011 and 022 passed on their recorded crash-oracle-only condition.
5. **Stock legs where SR never fires.** `SubstitutionRedundancy=0!` is expected in
   stock default-dsr, random-drat and random-lrat. SR is declined for DRAT/LRAT, and
   the default policy does not produce SR on the stock families. SR reachability is
   owned by the SR audit modes above.

## 4. Failures and reproducers (observed)

- `failure-index.json` shows `bundles_total=0` and `observed={}`.
- `bundles.txt` shows `bundles_available=0 classes=0 promoted=0 uncovered_classes=0`,
  with no `promote.py exit` line.
- No failure class was observed, so there is no reproducer to hand off. Limitation
  (SUBMISSION.md): `--max-failures 10` did not trigger, because no leg recorded a
  failure.

## 5. Cleanup (observed)

- `residue.txt`: `scratch_exists=no`, `new_entries_vs_job_start=0`,
  `user_entries_newer_than_start=0`, `pin_writes_after_start=0`.
- Scratch `/tmp/tmp.HCNt6lmd0I` was 608 kB before removal. The EXIT trap ran; no signal
  path was taken.
- Re-verified now: the pin manifest is unchanged, and there are 0 writable entries
  and 0 files newer than the manifest.
- **Owner action:** per §8.2, a passing checkpoint's harness snapshot is removable.
  `build/pin-srfuzz-direct-r16-298c3467-r2` is still present; this audit did not
  delete it. Its base pin `build/pin-srfuzz-direct-r16-298c3467` (the job-1689026
  failure handoff) is also still present.

**Cleanup: PASS.**

## 6. Terminal statement

**PASS.**

- All 20 legs exited 0, the aggregator reports `WORK-VERDICT PASS legs=20 bad=0`,
  there are 0 failing cases, and 0 required keys are missing.
- There were 0 DSR→LSR→Trestle rejections, 0 model or differential failures, and
  0 sanitizer or crash indicators. Negative controls behaved as expected.
- Identities are exact and cleanup passed.
- This closes the R16 Claude fuzz gate for source `298c3467` with the forward-repaired
  checker pin. Landing the R16 direct-SR increment remains Codex's action, separately
  from the newer uncommitted producer work.
