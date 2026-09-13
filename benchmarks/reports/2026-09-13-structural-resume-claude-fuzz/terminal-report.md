# Structural-resume fuzz checkpoint: candidate `a08f9f6a` (job 1687182)

**Verdict: PASS.** No leg found a solver failure, and no required reachability
feature is missing. The three defect classes from job 1687094 are closed:

- **S1 (invalid one-hot SAT model):** closed.
- **S2 (`falsified large clause escaped propagation`):** closed.
- **S3 (`assigned_count + k.unassigned == k.nvars`):** closed.

This time the Debug build ran with ASan and UBSan. Everything that was already
closed in 1687094 stays closed: F1, F2, H4, GF(2)/GF(3), the typed API, replay,
the stock fuzz legs, native model/DRAT/LRAT checks and the negative controls.

Previous checkpoint: `../2026-09-13-final-refactor-claude-fuzz/terminal-report.md`.

## Identity

| Item | Value |
|---|---|
| Candidate | `a08f9f6a841c22f006af872f6f162ca606cef3bd`, tree `5c2931d460628984d86d7f359b239a7215457eeb` |
| Pin | `build/pin-structfuzz-a08f9f6a` (read-only). Source is `git archive` of the exact commit. Compiler GCC 13.1.0 |
| Release `rel/tactix` (`-O3`) | `b63f30031229a36b9fea46e37590e892ab4a178a1239357ff3f80557548e1413`. Byte-identical to the supplied `build/release/tactix` |
| Debug `dbg/tactix` | `2b307b56759c40d1c9b4682bce723dc54650c2aab061c4a9ec31f2de1ee45287`. Debug preset `-O1 -g -fsanitize=address,undefined`, `TACTIX_ENABLE_PROOF_SHADOW=ON` |
| C-API drivers | `capidriver-rel` `4cfc9952…`, `capidriver-dbg` (ASan/UBSan) `564d3c9f…` |
| `tracecheck` | `a21859e8…` |
| Manifest | `MANIFEST.sha256` has 3742 entries, sha256 `27236a0815082d716328b42de91cc3103a00b67c3256c010bf19cc81604574d8`. The job verified it before starting |
| Tool root | Same file set and bytes as the 1687094 pin root: kissat, cadical, drat-trim, cake_lpr, lrat-check, tools/fuzz, corpus |

**Harness** (frozen under `pin/harness`; editable copy in `campaign/`):

| File | sha256 |
|---|---|
| `audit.py` | `31245d31…` |
| `capidriver.c` | `1f53ecda…` |
| `selftest_harness.py` | `83692c57…` |
| `work.sh` | `d3a287c1…` |
| `batch.sh` | `c633b3aa…` |
| `runtest.sh` | `24c71b7c…` |
| `pargen.py` | `dc30cf84…` (unchanged) |
| `tracecheck.cpp` | `365e60c6…` (unchanged) |
| `capwrap.sh` | `a3cf5258…` (unchanged) |

## Job

| | |
|---|---|
| Job | 1687182, a single non-array job on `cpu-dense-hwulab-q`, node018 |
| Resources | 100 logical CPUs, 400000M, 1 h limit, `--signal=B:USR1@240`, no throttle, no node constraint |
| Outcome | COMPLETED 0:0 in 16:12; `work exit=0` after 971 s; batch MaxRSS 6.8 GiB |
| Memory | no RSS-cap kills; largest per-process high-water mark 1.2 GiB |
| Legs | every leg has `skipped_deadline=0`; every audit leg exited 0 with `missing=[]` |
| No follow-up | no other job was submitted |

Leg sizes and seeds are identical to 1687094:

| Leg | Size and seeds |
|---|---|
| regress | 4 F1 replays, plus the 3 new cases below |
| interrupt | 53 tasks, base 913900000 |
| random | n=5000, seeds 913500000+i |
| default | n=4000, seeds 913600000+i |
| capi | n=4000, seeds 913700000+i |
| progs | n=3000, seeds 913750000+i |
| stock `fuzz.py` | random-drat/lrat n=1500 (913800000 / 913810000); default-drat/lrat n=1200 (913820000 / 913830000); each also runs the 30-entry corpus |

## Harness changes from 1687094 (disclosed)

- **New regression cases in `regress`:**
  - **S1:** d000437 (seed 913600437, oh_excluded_unsat), in both DRAT and LRAT,
    through the DefaultPolicy path. Release and Debug both run, then the model,
    proof and Kissat oracles.
  - **S3:** c000027 (seed 913700027, xor_affine_hidden, mode 5), through both
    C-API drivers.
- **Watch list:** every S1, S2 and S3 case id from 1687094 is on a watch list.
  Each one's per-case outcome is recorded in `summary.json` under `watch`.
- **New C-API driver checks.** The driver makes no new RNG draws, so every seed
  keeps its 1687094 scenario.
  - After any online SAT, `tactix_status` must return 10.
  - Two external model exports, separated by a status query, must be
    byte-identical. The first export is the model that gets printed and
    checked.
  - A post-export probe configures a Lucky 0 OneInstance program and advances
    once. This makes Debug run `kernel_check_invariants` on the state that the
    non-materializing export left behind.
  - For mode 5, the driver reports `warm_status` and `nonterminal_resumed`.
- **New required features:**
  - `capi:online_sat_status_reflected`
  - `capi:online_export_stable`
  - `capi:post_export_probe_ran`
  - `capi:reconfigure_nonterminal_resumed` and its cross-build-identical variant
  - mode-5 resumption for Lucky 6 and for Lucky 7
  - the `regress:s1_*` and `regress:s3_*` keys
- **Fixes for the harness findings reported in 1687094:**
  - **H-a:** the authored-program admission bound is now the largest
    `maximum_variables` in the program.
  - **H-b:** Lucky `Sat`/`Unsat` status lines are classified as terminal, not as
    search-level work.
  - **H-c:** the Debug replay limit rose from 80 s to 300 s.
  - **H-d:** F2 gets in-Sweep credit when the SIGTERM time falls inside the
    probe's longest Sweep interval, and the last online line is that Sweep.
- **Debug build:** Debug is now the sanitizer preset. In 1687094 it was plain
  `-g` with assertions, which was weaker than required. `runtest.sh` gives Debug
  tests ASan's `hard_rss_limit_mb` instead of `ulimit -v`.
- **Self-test:** the harness self-test now has 560 checks, 4 more than in
  1687094. The new checks prove that an unreflected SAT and an unstable export
  are flagged. They also prove that mode-5 and S3 credit flows through the
  driver pair, and that the H-a bound is applied. The self-test passed in the
  session before submission and again inside the job.

## Explicit regressions

### S1: d000437 / 913600437 / oh_excluded_unsat — PASS

| Where | Result |
|---|---|
| `regress`, DRAT | UNSAT; drat-trim verified the Release and Debug proofs; Kissat agrees; Debug = Release |
| `regress`, LRAT | UNSAT; cake_lpr verified both proofs; Kissat agrees; Debug = Release |
| `default` leg, watch d000437 (DRAT) | UNSAT, proof verified, Kissat agrees |

No invalid SAT model was returned.

**All S1/S2 one-hot cases from 1687094:**

**DefaultPolicy S1 cases:**

| Case | Family | Verdict | Oracle | Kissat |
|---|---|---|---|---|
| d001121 | oh_excluded_unsat | UNSAT | proof verified | agrees |
| d001383 | oh_excluded_unsat | UNSAT | proof verified | agrees |
| d001803 | oh_excluded_unsat | UNSAT | proof verified | agrees |
| d002591 | oh_excluded_unsat | UNSAT | proof verified | agrees |
| d001246 | oh_sat_units | SAT | model valid; one-hot SAT after rejected candidates | agrees |
| d001587 | oh_sat_units | SAT | model valid; one-hot SAT after rejected candidates | agrees |
| d002615 | oh_hidden | SAT | model valid | agrees |

- In all 7 cases Debug output equals Release output.
- Kissat's verdict equals the tactix verdict in every row.
- The 1687094 S1 signature was UNSAT formulas reported SAT. Here those formulas
  are UNSAT with verified proofs, and the SAT formulas have valid models.

**C-API S1/S2 cases.** Every case ran in both Debug (ASan) and Release; Debug
and Release output are identical and neither run tripped an invariant.

| Case | Result |
|---|---|
| c000197 | SAT, model valid, status reflected, export stable, probe ran |
| c001062 | SAT, model valid, status reflected, export stable, probe ran |
| c002882 | SAT, model valid, status reflected, export stable, probe ran |
| c002928 | SAT, model valid, status reflected, export stable, probe ran |
| c001112 | UNKNOWN. A Lucky-only program has no model to report; this is legal |
| c003515 | UNKNOWN (legal) |
| c003703 | UNKNOWN; mode-5 nonterminal resume |
| c001365 | UNKNOWN; mode-5 nonterminal resume |

**Authored-replay S2 cases:**

- **Cases:** p000456, p000603, p001004, p001082, p001193, p001361, p001443,
  p001779, p001926, p002913.
- **Result:** no Debug abort. Release is deterministic, and Debug equals Release.
- **Verdicts:** the 4 SAT verdicts have valid models and Kissat agrees. The
  other 6 exhausted their programs with UNKNOWN.

**Lane totals:**

| Lane | Invalid models | Differential mismatches | Invariant or sanitizer alarms |
|---|---|---|---|
| default | 0 | 0 of 3988 | 0 |
| capi | 0 (2114 models valid) | 0 (UNSAT-vs-Kissat-SAT check) | 0 |
| progs | 0 | 0 of 1415 | 0 |

### S3: seed 913700027 and the mode-5 population — PASS

- **c000027:** the warm Lucky batch returned SAT, and `tactix_status` returned
  10.
  - This is exactly the 1687094 path. There, status stayed 0, reconfiguration
    ran, and Debug asserted.
  - Now no reconfiguration follows. The model is valid and the export is stable.
  - The post-export probe advanced with result Completed and status 10, and the
    ASan Debug build raised no invariant failure.
  - Debug and Release are identical. The regress copy gives the same result.
- **Full mode-5 population:** 646 cases, each run in Debug and in Release.

| Warm step result | Cases | What followed |
|---|---|---|
| nonterminal (status 0) | 383 | reconfigured and resumed; Debug = Release in all 383 (Lucky 6: 197, Lucky 7: 186) |
| terminal SAT (status 10) | 202 | status reflected, no reconfiguration |
| UNSAT (status 20) | 61 | — |

There were zero `assigned_count`/`unassigned` invariant failures and zero driver
aborts.

### Non-materializing online model export — PASS

- **Scope:** 866 C-API cases returned an online SAT.
- **Checks:** in every one, `tactix_status` returned 10, the two exports were
  byte-identical, and the printed model passed the original-CNF check.
- **Post-export probe (866 cases, Debug and Release):**

| Probe result | Cases |
|---|---|
| Inapplicable | 816 |
| Completed | 40 |
| Sat | 10 |

- **Solver state after the probe:** `tactix_status` stayed 10 in every case.
  ASan Debug raised no invariant failure, and Debug output equals Release in
  all 4000 C-API cases.
- **Unit test:** the candidate's `test_kernel`, including
  `test_resumable_model_export_preserves_solver_state`, passed in Release and
  in Debug.

## Previously closed lanes

### F1 exact replays — PASS

| Seed and format | Recorded run | Replays |
|---|---|---|
| 913501804 DRAT | SAT | Debug and Release replay identical |
| 913503173 LRAT | SAT | Debug and Release replay identical |
| 913501804 LRAT (swapped format) | SAT | Debug and Release replay identical |
| 913503173 DRAT (swapped format) | SAT | Debug and Release replay identical |

- Effective transitions are identical in all 4.
- The CNF sha matches job 1686850.
- Release is deterministic and Debug equals Release, 4/4.
- A corrupted recorded verdict was rejected, 4/4.

### F2 interruption — PASS (now credited by the harness rule)

**Seed 913500739** (`ksat-k4-n69-m652`, sha `0f132362…`):

- The probe finished SAT in 26.9 s, and its model is valid.
- The long Sweep application runs from 0.09 s to 26.67 s (26.58 s).

| SIGTERM at | Result | Latency | `c interrupt` | Last Sweep line status |
|---|---|---|---|---|
| 10.0 s | clean UNKNOWN | 1502 ms | 3 | NoChange |
| 6.74 s | clean UNKNOWN | 639 ms | 2 | NoChange |
| 13.38 s | clean UNKNOWN | 1594 ms | 1 | Completed |
| 20.07 s | clean UNKNOWN | 1908 ms | 1 | Completed |

- No run needed SIGKILL.
- **Observation, carried over from 1687094:** an interrupted Sweep application
  reports NoChange or Completed, not Interrupted.

**Sweep-only runs:**

- 36 runs stopped with a clean UNKNOWN: 32 Release (max latency 66 ms) and 4
  Debug (max 204 ms).
- Each of these lanes was credited at least 6 times:
  - unlimited DRAT and unlimited LRAT
  - finite DRAT and finite LRAT
- The Debug lane was credited 4 times.

**Other interruption results:**

| Leg | Clean UNKNOWN / SIGTERMs | Max latency |
|---|---|---|
| random, Release scheduled | 95/97, plus 2 checked verdicts | 201 ms |
| random, Debug scheduled | 53/56, plus 3 checked verdicts | 1593 ms |
| random, Debug timeout | 388/388 | 6998 ms (ASan) |
| random, Release timeout | 248/248 | 1433 ms |
| default | 12/12 | 54 ms |

Across all of these there were 0 interruption misses.

### H4 admission, GF(2)/GF(3), typed API, replay — PASS

**Admission:**

| Feature | default | random |
|---|---|---|
| `default:admission_bound_noop` | 1 | 4 |
| onehot admission bound noop | 1 | 7 |

- In the default leg, every one of the 21 families was checked.

**Structural models (SAT cases whose model was accepted):**

| Leg | Parity | One-hot |
|---|---|---|
| default | 43 (13 after rejected candidates) | 101 (66 after rejected candidates) |
| random | 19 | 10 |

- In the random leg, Debug and Release replay were identical for every
  parity and one-hot SAT.

**C-API features:** direct, OneInstance, SelectedBatch, CompleteCurrentPass,
two-step reordered, Count/Fixpoint, predecide, inconsistent-ingest and NoOp were
all reached. All 12 invalid-parameter variants were rejected transactionally.

**Authored programs:**

- **Templates:** One, Batch, Pass and two-step for both parity and one-hot.
- **Shapes:** repeated, bounded, fixpoint and multi-revision resumption.
- **Controls:** malformed tuples, invalid parameters and wrong-rule parameters
  were rejected 375 times each. Corrupted verdicts were rejected 166 times.

**Random leg:**

| Check | Result |
|---|---|
| replay identical, Debug and Release | 4612/4612 |
| effective transitions identical | 4612/4612 |
| Release determinism | 4752/4752 |
| cross-build identical | 4612/4612 |

- `root_only_rules_at_search_level` is empty in every leg (H-b fixed).

### Oracles

| Check | random | default | progs | capi | regress |
|---|---|---|---|---|---|
| Models valid | 8303 | 6741 | 2418 | 2114 | 9 |
| DRAT proofs verified | 1572 | 1045 | 214 | — | 2 |
| LRAT proofs verified | 1216 | 990 | 198 | — | 2 |
| Kissat agrees | 4612/4612 | 3988/3988 | 1415/1415 | — | 6/6 |
| Negative model controls rejected | 3401 | 3047 | 1209 | 1057 | 5 |

- No proof was rejected and no checker timed out. The longest check took 19 s,
  and the largest proof was 91 MiB.
- Negative DRAT and LRAT controls (no-conflict, truncated, hints removed) were
  rejected wherever they applied.
- **Stock `fuzz.py` legs:** 0 failures, and each 30-entry corpus had 0 failures.

| Leg | Cases with a verdict | ParityModel reached |
|---|---|---|
| random-drat | 1337/1500 | 127 |
| random-lrat | 1343/1500 | 121 |
| default-drat | 1182/1200 | 295 |
| default-lrat | 1180/1200 | 298 |

- **Focused tests:** 18 runs, Release and ASan/UBSan Debug, all exit 0 with 0
  sanitizer lines:
  - test_parity_model, test_onehot_model, test_lucky
  - test_online_all_actions, test_random_online
  - test_sweep, test_probe, test_kernel
  - trace_replay.sh
- **Self-tests:**
  - `selftest_harness.py` passed 560 checks.
  - `tools/fuzz/selftest.sh` reported SELFTEST OK against the pinned sanitizer
    Debug build.
  - `test_modelcheck.py` exited 0.

## Cleanup audit

- **Node-local scratch:** every CNF, model, proof and trace lived in node-local
  `/tmp/tmp.xH8KjfsYIw` on node018. The EXIT trap removed it
  (`scratch_exists=no`).
- **node018 residue:** `/tmp`, `/var/tmp` and `/dev/shm` had 0 new entries
  compared with the job-start snapshot. There were 0 user entries newer than
  the start.
- **Failure bundles:** none were produced (`bundles_available=0`,
  `fuzzpy_bundles=0`), so no reproducer was retained.
- **Session shell:** there were no campaign entries in `/tmp`, `/var/tmp` or
  `/dev/shm`. The one pre-submission self-test temp directory was empty and was
  removed.
- **Repository output:** this report directory totals 308 KiB. It holds scripts,
  capped logs and JSON summaries only.
- **Pin:** `build/pin-structfuzz-a08f9f6a` stays read-only. It is retained as the
  evidence pin.
- **Untouched:** no solver source, tests, PROGRESS, plan, Codex reports, other
  pins or other jobs. Nothing was committed. The shared working tree has
  unrelated staged changes that belong to another owner; they were not touched.

## Limitations

- **SIGTERM before the handler is installed:** a signal in the first ~10 ms,
  before `main` installs its handler, is still not tested. Same as 1687094.
- **ON/OFF arm:** the parity/one-hot ON/OFF arm was not rebuilt.
- **Post-export probe:** the probe checks invariants and cross-build identity.
  It does not assert a particular probe status, because advancing after a
  terminal SAT has no specified result. All 866 probes nonetheless kept
  `tactix_status` at 10.
- **Sanitizer Debug is a change from 1687094:** Debug timing differs from that
  campaign. No lane lost coverage: every deadline skip is 0 and nothing required
  is missing.
