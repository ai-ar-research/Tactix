# FINAL REPORT CLOSEOUT — shared-policy preparation fuzz, pin 375b621b

Owner: Claude (fuzz owner), same session. Terminal: 2026-10-03T08:42-04:00.

## Disposition

**ACCEPTED.** This is both-policy preparation fuzz acceptance for DefaultPolicy
and the controller-interface-v1 RandomControllerPolicy on pin 375b621b. It
holds only within the replay scope stated in §1 and §2.

The delivered `FINAL_REPORT_ADDENDUM.md` disposition NOT ACCEPTED stays on
record; it was correct when issued. This closeout replaces it only because
C1–C3 are now closed by the preregistered work below:
- **No solver change.** The candidate pin and checkers were not modified.
- **No failure.** 0 failures occurred in any of the 40,062 new graded runs.

Both earlier reports are preserved byte-for-byte; the C2 postflight confirmed
these hashes:
- `FINAL_REPORT.md`: e648d19f65c7ba255cb8bd4b0e6f89adda373848940acb090d946e3ceacd61d2
- `FINAL_REPORT_ADDENDUM.md`: 8a721895dfbb419ecb27dad91b2d8c4ca5a21ba2117cb63d9406a784b9dd64f4

Whole-goal acceptance, docs and the Andrew handoff remain Codex-owned. Nothing
was committed or pushed. I did not edit root source, tests, tools, docs,
skills or global checker links, and did not touch any parent campaign.

## Preregistration and provenance

- **Plan:** `CLOSEOUT_PLAN.md`, sha256
  9f4da453552f9bb225445d4660d8d133d37211c4e663c4cd024ceaa1a58fe3a8. It was
  written and hash-frozen (`closeout/plan-freeze.sha256`) before any closeout
  submission.
  - It records the artifact search first. No original mining or d2 raw
    certificate or model was retained:
    - mining kept only per-seed exit codes and LocalSearchGuidance/Propagate
      hit counts;
    - d2 kept only counters, s-lines, stream shape and equality flags.
- **Runner:** `closeout/closeout.py` sha256 26cf9879…bb8, `job.sh` 01314eac…707,
  and `make_snapshot.sh` 8f7fa191…c49. All ran from the immutable snapshot
  **c1**, `047864a8818335eebd6a3c1779c64069c5e125badb80b64a05d4a24447597a20`.
  - c1 is an exact copy of targeted snapshot f1 (f538b1f8…): the maintained
    `fuzz.py`, `generators.py` and proof oracles at 375b621b, the controls,
    and `verify_pin.py`.
  - Added to that copy: the closeout runner, the original mining and d2 rows
    (read-only), and the two d2 input CNFs.
- **Solver:** the same immutable Debug pin
  `build/pin-shared-policy-feedback-debug-375b621b`, tactix
  95197f8c9f468bd3b121b1cd66d65457b9d5baade62bffb27f95e82caff593c2.
- **Checkers:** the same frozen collection, SHA256SUMS
  895f1989468d5e01a81fa647f9cb9739448b81507a4b78515e4e46a7441c44f8, including
  dsr-trim 3b5c7737…dbee.
- **Oracles:** the maintained `fuzz.run_case`.
  - SAT: `modelcheck` against the original CNF.
  - UNSAT: native DSR. Witness-free proofs go through lossless DRAT → backward
    drat-trim → CakeLPR, plus the forward `drat-trim -f` every-lemma audit.
    Witnessed proofs and PBP would take their own routes, but none occurred
    here.
  - Supplementary differential verdict against pinned kissat.
- **In-job gates on every closeout job:**
  - snapshot digest equal to its recorded value, before and after the run;
  - `verify_pin.py` before and after (all 8 outputs: `bad: []`, 173 binaries,
    795 sources);
  - `sha256sum --strict -c core-checkers.sha256` before and after (6 × OK);
  - the maintained selftest (`SELFTEST OK`) for the remine jobs.

### Closeout jobs

All are COMPLETED 0:0 with job status 0, on partition cpu-dense-hwulab-q.
None used an array throttle or a node constraint, and none was restarted.

| Job | Role | Allocation | Window | Node |
|---|---|---|---|---|
| 1747298 | C1 labelled smoke (`remine-smoke`, i = 10..33; not counted) | 8 phys / 16 CPU / 64000M | 06:37:33–06:39:28 | node018 |
| 1747299 | C3 d2 graded reruns (`d2re`) | 4 CPU / 16000M, 12h | 06:37:34–08:41:44 | node017 |
| 1747300 | C1 full graded replay (`remine`, 20,000) | 50 phys / 100 CPU / 400000M / 50 workers, 24h | 06:39:47–07:50:28 | node018 |
| 1747314 | C1 stage 2 (`remine-followup`, 4 cases, 600 s) | 4 phys / 8 CPU / 32000M | 07:50:53–07:53:13 | node018 |

Resource differences from the original protocol:
- **d2re:** 4 CPUs and one sequential process, preregistered. Each
  feedback-on run writes about 47–50 GB to node-local `/tmp`, so the three
  must not run concurrently.
- **Stage 2:** 8 CPUs, 4 workers. It has only 4 tasks. Its exact allocation
  was not fixed in the plan; it is recorded here.
- **remine:** the original mining allocation.

## 1. C1 — mining scan: original vs graded replay

**Original scan** (job 1747086; unchanged and still unchecked as executed)
- **Schedule:** 20,000 cases. L2 seeds 2041000010–2041020009, i = 10..20009.
- **Invocation:** `tactix --controller-policy=SEED --feedback=F CNF PROOF`,
  with a 30 s deadline.
- **Original outcomes:** 19,084 completed (exit 10: 11,382; exit 20: 7,702),
  916 solver timeouts and 0 other results.
- **Original hit rows:** 51, with LocalSearchGuidance/Propagate hits.

**Graded replay** (identical CNF from `generate(seed, i)`, identical
invocation)
- **Per case:** two independent executions, A and B, each fully graded.
- **Twin equality:** when both complete, A and B must be equal on verdict, s/v
  lines, proof bytes and the feedback stream without its `elapsed_nanos`
  records.
- **Correspondence:** the original exit code against A's verdict, and the
  original hit counts against A's hits.

| Original category | Count | Stage 1 (30 s) result | Stage 2 (600 s) | Final |
|---|---:|---|---|---|
| Original completions | 19,084 | 19,080 resolved; 4 unresolved | 4 / 4 resolved | **19,084 / 19,084 resolved; 0 unresolved** |
| Original timeouts | 916 | 897 both twins timed out; 16 both completed (graded, twin-equal); 3 one twin completed (graded) | not applicable | reported as is; no original verdict exists to correspond to |
| Total | 20,000 | | | |

The 19,080 resolved at stage 1 split as 11,380 SAT and 7,700 UNSAT. For each:
- A and B both completed and were graded;
- the verdict equals the original;
- A and B are byte-equal (twin);
- the hits equal the original.

The 4 unresolved at stage 1:

| Index | Original | A | B |
|---|---|---|---|
| 3913 | SAT | TIMEOUT | TIMEOUT |
| 17170 | UNSAT | TIMEOUT | TIMEOUT |
| 17515 | UNSAT | TIMEOUT | TIMEOUT |
| 19436 | SAT | TIMEOUT | SAT, graded |

At stage 2 all four completed in both twins (2 SAT, 2 UNSAT), graded,
twin-equal, verdict-equal and hit-equal.

The 3 original-timeout cases where only one twin completed:
- A TIMEOUT, B SAT;
- A TIMEOUT, B UNSAT;
- A UNSAT, B TIMEOUT.

Every completed twin among them was graded.

**Hit correspondence:** 51 / 51 original hit rows match, and no row has a hit
mismatch.

**Graded-run census, new runs** (unit: one execution graded by `run_case`)

| Set | Runs | SAT | UNSAT | Solver timeout | Checker timeout | Checker rejection / error | Model / differential / crash failures |
|---|---:|---:|---:|---:|---:|---:|---:|
| Stage 1 (A + B) | 40,000 | 22,778 | 15,418 | 1,804 | 0 | 0 | 0 |
| Stage 2 (A + B) | 8 | 4 | 4 | 0 | 0 | 0 | 0 |
| Labelled smoke (A + B, not counted) | 48 | 24 | 24 | 0 | 0 | 0 | 0 |

- **Stage 1 timeouts:** the 1,804 = 1,797 runs in original-timeout cases + 7
  in original-completion cases. The latter are the 4 cases above,
  re-resolved at stage 2.
- **Checker statuses:** every one is `verified`.
  - Stage 1: drat-trim 30,836 and CakeLPR 15,418.
  - Stage 2: 8 and 4.
  - Smoke: 48 and 24.
  - Every UNSAT took the witness-free native-DSR route.
- **SAT runs:** every model check passed. A failure would be recorded as a
  `model` failure; there were 0.

**Verification scope and limitation (exact).** The original 19,084 scan
completions produced certificates and models that were never retained, and
they are not checked byte-for-byte. Each one is dispositioned by four facts:
- A and B are two graded executions of the identical invocation on the
  identical pin, CNF and seed.
- A and B are byte-identical in proof, s/v lines and deterministic feedback
  stream. This demonstrates per-case determinism of the invocation.
- Their verdict equals the original exit code.
- Their LocalSearchGuidance/Propagate hit counts equal the original record.

On a deterministic invocation this establishes the certificate and model that
the original scan would have emitted. It is not a re-check of the lost bytes.

I did not use subtraction of leg counterparts, a check of only the 196
leg-timeout pairs, or verdict matching alone. The 916 original timeouts had no
original verdict. Their reruns are reported separately, with every completed
rerun graded.

## 2. C3 — d2 raw runs: graded reruns with correspondence

The d2 raw run census is unchanged: 3 configurations, 6 raw runs, all UNSAT,
ungraded at the time, with off/on proof bytes equal. d1 is unchanged: 8 graded
runs, 6 UNSAT verified and 2 solver TIMEOUT at 1,800 s.

The four f1 CLI feedback-variant rows remain excluded non-evidence. So do the
two undiagnosed TF recording-variant timeouts.

**Closeout job 1747299** reran each configuration with the identical wrapper
and arguments and the original budgets: off 900 s and on 3,600 s, both graded
by `run_case`.

| Configuration | Graded off | Graded on | Proof bytes (off = on) | Counters vs original raw off / on (s, conflicts, decisions, stress fires) | On stream vs original scan (max counters, lines, records, end) |
|---|---|---|---|---|---|
| gen-2042000025 n50 | UNSAT, drat-trim + CakeLPR + forward audit verified, 8.7 s | UNSAT verified, 285.4 s | 2,173,327 = 2,173,327 | equal / equal (67,705 / 86,891 / 203) | equal (645,773 lines, 645,772 records, end [20, Unsat]) |
| gen-2042000015 n50 | UNSAT verified, 80.7 s | UNSAT verified, 3,302.8 s | 11,866,676 = 11,866,676 | equal / equal (391,010 / 463,807 / 288) | equal (3,483,685 / 3,483,684 / [20, Unsat]) |
| gen-2042000015 n500 | UNSAT verified, 73.5 s | UNSAT verified, 2,891.4 s | 11,133,825 = 11,133,825 | equal / equal (362,506 / 430,931 / 60) | equal (3,235,283 / 3,235,282 / [20, Unsat]) |

- **Result:** 6 / 6 graded runs completed and verified, with 0 timeouts and 0
  failures.
- **Correspondence argument.** Each original raw run is dispositioned by an
  exact graded rerun of its invocation, which matches it on:
  - the s-line;
  - final conflicts, decisions and stress fires;
  - the on stream's maximum counters, exact line and record counts, and
    session end.

  The original raw off = on proof equality is reproduced as rerun off = on
  proof equality.
- **Supporting note only.** During the original d2 job I listed its temporary
  directory, and the raw proof sizes for the two gen-15 configurations were
  11,866,676 and 11,133,825 bytes. Those equal the rerun sizes. That listing
  is not a retained artifact, so it is supporting only.
- **Scope and limitation:** the lost raw bytes themselves are not re-checked.

## 3. C2 — terminal postflight after all owner jobs

`closeout/postflight.sh` is hash-only and runs no solver. It ran at
2026-10-03T08:42:32-04:00, after `squeue` showed 0 owner jobs. Its outputs are
in `closeout/postflight/`, and it returned **status 0**.

| Check | Command / binding | Result |
|---|---|---|
| Solver pin + source | `python3 tools/verify_pin.py` (PIN.json sha 72a3a25461ab2a1974b6c16e723e54e87734fdcd14e5bfbddf1676f96cf3c136; 173 binaries re-hashed; 795 source hashes against the git blobs of 375b621ba6677600f3e77bf8383af25d256d7b67) | `"bad": []`, rc 0; tactix 95197f8c…93c2 |
| Source drift | `git diff --stat 375b621b HEAD -- src include tools tests` (HEAD 335c511a0efe5ab00cb0e2a4543673dc5d3747a6) | empty |
| Core checkers, owned pin | `cd build/pin-fuzz-checkers-shared-feedback-375b621b && sha256sum --strict -c SHA256SUMS` | 6 / 6 OK |
| Core checkers, installed | `cd tools/fuzz/failures/checkers/895f1989… && sha256sum --strict -c <owned SHA256SUMS>` | 6 / 6 OK; both manifests sha256 895f1989468d5e01a81fa647f9cb9739448b81507a4b78515e4e46a7441c44f8 |
| Owner snapshots | `find . -type f ! -path ./SNAPSHOT_SHA256 -print0 \| LC_ALL=C sort -z \| xargs -0 sha256sum \| sha256sum` | s1 710ff4438b3f581d3a8b582fe7e8c01cdf0da8753fefce4bc37ff8806c75db1e, f1 f538b1f895ebc92306493c42e429138e74cef9af438774fc03c16e52af89595b, c1 047864a8818335eebd6a3c1779c64069c5e125badb80b64a05d4a24447597a20: all MATCH their recorded values |
| Leg harness | `slurm.sh`'s formula over `tools/fuzz/failures/harnesses/1fceb82e…/fuzz` | 1fceb82e94b70bb57ec60d7b6e69b6cb55eecb155d974b0059be649e17cb5fc3, MATCH |
| Diagnosis scripts | `sha256sum --strict -c` of `diag/d1/SHA256SUMS` and `diag/d2/SHA256SUMS` | 4 / 4 OK |
| Closeout freeze | `sha256sum --strict -c closeout/plan-freeze.sha256` | 4 / 4 OK |

Owner tool hashes (unchanged):

| File | sha256 |
|---|---|
| fbdrv (= `snap-f1/fbdrv`) | d6b56a53…994b |
| fbdrv.cpp | c1dd1048…60ed |
| fbdrv-ctl-f6f01982 | a3ba600b…760d |
| cfgdump | a2c0e2e9…52de |
| progcheck | 9b2de8f9…87e6 |

All frozen pins are preserved.

**Terminal job accounting.** All 11 owner jobs are COMPLETED 0:0; full
`sacct` output is in `postflight/sacct.txt`.

| Job | Role | Window |
|---|---|---|
| 1747082 | L1 leg | 00:04–01:28 |
| 1747083 | L2 leg | 00:04–01:10 |
| 1747084 | smoke | 00:04–00:15 |
| 1747085 | f1 targeted | 01:10–02:02 |
| 1747086 | mining | 01:28–02:09 |
| 1747211 | d1 | 01:39–02:58 |
| 1747244 | d2 | 02:35–06:24 |
| 1747298 | closeout smoke | 06:37–06:39 |
| 1747299 | d2re | 06:37–08:41 |
| 1747300 | remine | 06:39–07:50 |
| 1747314 | stage 2 | 07:50–07:53 |

**Limitation kept.** d1 (1747211) and d2 (1747244) still have no in-job pin,
checker or snapshot verification, and the earlier postflight (02:22:59)
preceded their end. This terminal check shows the pin, checkers and snapshot
f1 are unchanged after them. It does not create historical in-job checks.

## 4. Reconciled categories (old and new)

| Category | Executed | Completed and independently checked | Solver timeout | Checker timeout / rejection / error | Unknown / crash | Fresh credit |
|---|---|---|---|---|---|---|
| Fresh L1 DefaultPolicy (unchanged) | 100,000 | 57,423 SAT + 39,901 UNSAT | 2,676 | 0 | 0 | yes |
| Fresh L2 Random (unchanged) | 100,000 | 56,018 SAT + 38,302 UNSAT | 5,680 | 0 | 0 | yes |
| Fixed-prefix controls (unchanged) | 10 + 10 | 7 SAT + 3 UNSAT each | 0 | 0 | 0 | no |
| Corpus, per leg (unchanged) | 51 + 51 | DefaultPolicy 12 SAT + 38 UNSAT, plus 022 crash-oracle-only; Random 12 SAT + 39 UNSAT | 022 (DefaultPolicy, 60 s) | 0 | 0 | no |
| f1 targeted graded runs (unchanged; addendum §1) | DefaultPolicy 2,095 rows; Random 5,090 rows | all completed graded runs verified | per addendum | 0 | 0 | no |
| Mining original scan (unchanged, unchecked as run) | 20,000 | 19,084 completions, now **dispositioned via C1 replay** | 916 | not applicable | 0 | no |
| Mining follow-up TF (unchanged) | 42 rows / 126 runs | 126 SAT verified | 0 | 0 | 0 | no |
| C1 replay stage 1 + stage 2 | 40,008 runs | 22,782 SAT + 15,422 UNSAT | 1,804 | 0 | 0 | no |
| C1 labelled smoke | 48 runs | 24 SAT + 24 UNSAT | 0 | 0 | 0 | no |
| d1 (unchanged) | 8 runs | 6 UNSAT | 2 | 0 | 0 | no |
| d2 raw (unchanged) | 6 runs | ungraded then; **dispositioned via C3 rerun** | 0 | not applicable | 0 | no |
| C3 d2 graded reruns | 6 runs | 6 UNSAT | 0 | 0 | 0 | no |

The f1 targeted graded-run detail is in the addendum: DefaultPolicy has 1,389
SAT and 993 UNSAT; Random, including the mining follow-ups, has 5,203 SAT and
3,792 UNSAT.

New graded runs total 40,062: 40,008 replay + 48 smoke + 6 C3.

**Completions without their own oracle, explicitly scoped:**
- **Byte-identity, as before:** TF negative-control replays and driver B/C
  solves are dispositioned by byte identity to a graded run of the same
  invocation.
- **Causal controls on the predecessor pin:** the six `twctl` runs used the
  deliberately defective driver on f6f01982. Two must-pass runs exited 20
  ungraded. The four detection controls exited 3, a failed-check exit, not a
  completed verdict. None is a result of the candidate solver. The
  corresponding candidate-pin terminal-watch runs are all graded (20 / 20).

## 5. Prior evidence and remaining limitations

**Prior evidence still applies.** R1–R13 and TW in `FINAL_REPORT.md` were
established on this same pin, with this checker collection and the f1
snapshot. The C2 postflight shows all three are unchanged. The closeout
changes no row's evidence: it adds checked dispositions for previously
unchecked completions.

**Limitations, explicit and not relaxed:**
- **C1/C3 replay scope:** the lost original bytes are not re-checked; see the
  exact scope in §1 and §2.
- **d1/d2 in-job verification:** they have none (§3).
- **Debug CLI feedback-recording overhead:** 68–88× (it reproduced in d2re at
  285 / 3,303 / 2,891 s). It accounts for the 6 non-evidence
  feedback-variant timeouts.
- **Unavailable native producers:**
  - Condition is effective only with an offered witness.
  - Native SubstitutionRedundancy was never effective; SR effects come only
    from supplied payloads.
  - LocalSearchGuidance and Propagate record-level effects come only from the
    mining follow-ups.
  - The Factor terminal arm is unreached; it is dispositioned by source
    reasoning.
- **Fresh credit:** no mining, replay or diagnosis case is counted as fresh.
  The fresh seeds remain the separate 100,000 per policy.
