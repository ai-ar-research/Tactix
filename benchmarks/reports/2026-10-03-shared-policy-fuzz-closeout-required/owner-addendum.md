# FINAL REPORT ADDENDUM — shared-policy preparation fuzz, pin 375b621b

Owner: Claude (fuzz owner), same session. Written 2026-10-03, after delivery.

This addendum clarifies the current campaign only. The delivered
`FINAL_REPORT.md` is unchanged (sha256
e648d19f65c7ba255cb8bd4b0e6f89adda373848940acb090d946e3ceacd61d2). No new
experiment, job or verification was run for this addendum. Every number
below comes from evidence the campaign already holds, and the method used to
derive each one is stated.

## Corrected disposition

**NOT ACCEPTED as delivered.** The campaign meets every coverage row on
checked evidence, but it misses two explicit acceptance clauses. I found this
while answering the questions below.

**What is unchanged:**
- no solver, proof, model or checker failure;
- every coverage row R1–R13 and TW is met by independently checked evidence;
- the predecessor-pin causal controls still detect the defect.

**Corrections to the delivered report:**
- **C1 — mining scan completions were never independently checked.** The
  delivered R1 section says "Every completed run was checked independently."
  That is true for every graded run counted there, but not for the mining scan.
  - **Category (4) of the request:** "every completed SAT/UNSAT requires its
    independent model/proof disposition, even when a control/targeted case."
    The 20,000 mining replays were run with `fuzz.run_cmd` and no oracle.
  - **Counts:** 19,084 of them completed (exit 10: 11,382; exit 20: 7,702)
    with no model/proof disposition.
    - 18,888 have a graded counterpart in the L2 leg: the same CNF and
      controller seed, without `--feedback`.
    - 196 completed only in the mining scan. L2 timed out on them at 5 s, so
      no oracle ever checked that formula/seed.
  - **Rows were not affected.** No row relied on those verdicts. The R2
    LocalSearchGuidance/Propagate effects come from the 42 follow-up TF rows,
    which were fully graded with all negatives.
  - **The clause is still unmet** for that category.
- **C2 — the final pin/checker check ran before the diagnoses finished.** The
  terminal pin and checker postflight ran at 02:22:59. That was after every
  main job but before diagnosis d1 ended (02:58:12) and before d2 started
  (02:35:31; it ended 06:24:26).
  - Neither diagnosis job re-verified the pin, the checker collection or its
    snapshot in-job. Details are in §4.
  - The "terminal pin/checker/harness verification" clause is therefore unmet
    after the last owner job.
- **C3 — d2's raw runs were not graded.** The raw feedback-off and feedback-on
  runs in d2 (3 + 3) were not graded. Their proof files are byte-identical to
  each other, and a separate third run of each configuration was graded. See
  §2.

**What would close C1 and C2.** Neither needs a solver change. C1 needs a
graded rerun of the mining scan, or graded dispositions for its 19,084
completions. C2 needs a hash-only pin, checker and snapshot postflight after
the last job. The clarification constraints forbid both, so I have not run
them.

**Whole-goal acceptance** remains Codex-owned.

## 1. Outcome categories and count units

### Boundaries (from the snapshot's maintained `fuzz.py`, unmodified)

- **Solver timeout.** `run_case` runs the solver under `run_cmd(…, timeout)`.
  An expired deadline returns exit `None`, which becomes verdict `TIMEOUT`.
  - No oracle runs on a `TIMEOUT`, and it is never a pass.
  - `UNKNOWN` (an `s UNKNOWN` line) is a separate verdict.
  - `CRASH` and `IO-MISMATCH` are recorded as failures.
- **Checker timeout.** Each proof checker runs under `run_cmd(…, 120)`, and
  drat-trim additionally gets an internal 120 s limit. An expired checker
  returns exit `None`. `proof_check_status` maps that to `error`, which is not
  `verified`, so `run_case` records a `proof` failure. A checker deadline can
  therefore never be silently accepted.
- **Checker rejection or abnormal exit.** Status `rejected` or `error` becomes
  a `proof` failure. A model-check failure becomes a `model` failure.

### Fresh legs

Source: leg logs, `nonterminating.json`, failure bundles and accounting.

| Leg | Fresh executed | SAT | UNSAT | Solver timeout (5 s) | Checker timeout | Checker rejection / failure | Unknown / crash / incomplete |
|---|---:|---:|---:|---:|---:|---:|---:|
| L1 DefaultPolicy | 100,000 | 57,423 | 39,901 | 2,676 | 0 | 0 | 0 |
| L2 Random | 100,000 | 56,018 | 38,302 | 5,680 | 0 | 0 | 0 |

How these zeros are established:
- Each leg's verdict histogram contains only the keys SAT, UNSAT and TIMEOUT.
  `UNKNOWN`, `CRASH` or `IO-MISMATCH` would appear as their own keys.
- Each leg reports 0 failures. A checker timeout, rejection or error, or a
  model failure, would be a counted failure with a bundle.

The fixed-prefix rows (10 per leg) and the corpus are unchanged from the
report.

### Targeted f1 — row-level census (unit: one task row)

- A **row** is one task.
- One row contains one or more **graded runs**, each a `run_case` with
  independent oracles:

  | Row type | Graded runs per row |
  |---|---|
  | TF | `base`; then `fb` (feedback-on) and `rp` (feedback replay) only if `base` completed |
  | Driver | one graded run of the driver process; the B and C solves inside it are compared in-process against the graded A |
  | CLI stop-fuzz | `off` and `on` |
  | Corpus / edge | one run |

- TF negative-control replays are **not graded**. They are pass/fail
  comparisons: rejection reason plus s/v-line and proof-byte equality with
  `base`.
- Known-bad control rows (`tc`) are graded by design, to be flagged.
- `twctl` rows run on the predecessor pin. Their verdicts are not counted for
  this pin, and their 2 must-pass completions are not graded.

| Policy:kind | Rows | pass | timeout | feedback-variant timeout | other |
|---|---:|---:|---:|---:|---|
| default:driver | 1,303 | 1,239 | 64 | — | |
| default:tf | 291 | 283 | 6 | 2 | |
| default:ts | 120 | 114 | 2 | 4 (raw status `fail`, reclassified) | |
| default:corpus | 51 | 50 | — | — | 1 crash-oracle-only (022, solver TIMEOUT at 60 s) |
| default:edge | 10 | 10 | — | — | |
| default:tc | 320 | — | 8 | — | 89 flagged, 223 unflagged-inapplicable |
| random:driver (incl. 20 public terminal-watch rows) | 2,424 | 2,331 | 93 | — | |
| random:tf | 2,165 | 2,083 | 82 | — | |
| random:ts | 120 | 114 | 6 | — | |
| random:corpus / edge | 51 / 10 | 51 / 10 | — | — | |
| random:tc | 320 | — | 16 | — | 88 flagged, 216 unflagged-inapplicable |
| emit-sr / twctl | 80 / 6 | — | — | — | 80 emit; 4 detected + 2 control-pass |

Totals: DefaultPolicy 2,095, Random 5,090, other 86, for 7,271 rows.

Every targeted `timeout` and feedback-variant timeout is a **solver deadline**
on the run's process. The task's budget is passed to `run_cmd` as follows:

| Task | Budget |
|---|---|
| TF `base` | 30 s; mid fixtures 300 s (DefaultPolicy) or 120 s (random) |
| TF `fb` / `rp` | 5 × the base budget |
| Driver | 60–600 s for the whole driver process. Purity runs A, B and C in one process; interrupt runs the stopped session plus re-entries |
| CLI stop-fuzz | off 60 s, on 120 s |
| tc | 30 s |
| corpus | 60 s |
| edge | 5 s |

Two points about these timeouts:
- **None is a checker deadline.** Every checker status recorded across all
  graded runs is `verified`.
- **Feedback-variant timeout** means the graded `base`/`off` run completed and
  was verified, while the feedback-recording `fb`/`on` run hit its solver
  deadline. The row is non-evidence and never a pass.
  - The 2 default TF variant rows (gen 2042000025 and 2042000176): `base`
    UNSAT verified, `fb` TIMEOUT at 150 s, `rp` not run. These two were not
    diagnosed.

**Graded-run census, f1** (unit: one graded run; verdicts only SAT, UNSAT and
TIMEOUT occur, with 0 failure kinds in any graded run):

| Policy:kind:role | Runs | SAT | UNSAT | Solver TIMEOUT |
|---|---:|---:|---:|---:|
| default:tf:base / fb / rp | 291 / 285 / 283 | 153 / 153 / 153 | 132 / 130 / 130 | 6 / 2 / 0 |
| default:driver | 1,303 | 763 | 476 | 64 |
| default:ts:off / on | 120 / 120 | 74 / 74 | 44 / 40 | 2 / 6 |
| default:corpus / edge | 51 / 10 | 12 / 7 | 38 / 3 | 1 / 0 |
| random:tf:base / fb / rp | 2,165 / 2,083 / 2,083 | 1,168 each | 915 each | 82 / 0 / 0 |
| random:driver | 2,424 | 1,406 | 925 | 93 |
| random:ts:off / on | 120 / 120 | 74 / 74 | 40 / 40 | 6 / 6 |
| random:corpus / edge | 51 / 10 | 12 / 7 | 39 / 3 | 0 |

Checker statuses over these runs:

| Policy | drat-trim | cake_lpr | dsr-trim | dsr-compress | srcheck | VeriPB | CakePB |
|---|---:|---:|---:|---:|---:|---:|---:|
| DefaultPolicy | 1,646 | 832 | 314 | 314 | 314 | 4 | 4 |
| Random | 7,380 | 3,699 | 186 | 186 | 186 | — | — |

All are `verified`. That gives 0 checker timeouts, 0 rejections and 0 errors.

The delivered "graded completed semantic runs" totals are sums of the SAT and
UNSAT cells above, plus the mining follow-ups:
- DefaultPolicy: 1,389 SAT and 993 UNSAT.
- Random: 5,203 SAT and 3,792 UNSAT, including 3 × 42 mined SAT runs.

### Mining (category 4; never fresh credit)

| Item | Count | Disposition |
|---|---:|---|
| Scan replays (`run_cmd`, 30 s, no oracle) | 20,000 | 11,382 exit 10 and 7,702 exit 20, with **no model/proof disposition (C1)**; 916 solver timeouts |
| Follow-up TF rows (graded with negatives) | 42 | 42 pass |
| Follow-up graded runs (`base` / `fb` / `rp`) | 126 | 126 SAT; 0 timeouts; 0 failures |

## 2. Labelled diagnoses d1 / d2

Both diagnoses sit outside the 2,095 / 5,090 f1 rows and outside every
acceptance count.

**f1 rows they relate to.** All four are DefaultPolicy CLI stop-fuzz rows.
In each, f1's `off` run was graded UNSAT and verified, and `on` hit the 120 s
solver deadline:
- `ts-default-n50-gen-2042000025-mut-n90-m984`
- `ts-default-n500-gen-2042000025-mut-n90-m984`
- `ts-default-n50-gen-2042000015-ksat-k5-n67-m1880`
- `ts-default-n500-gen-2042000015-ksat-k5-n67-m1880`

**d1 (job 1747211)**
- **Inputs:** the 4 configurations above, using the snapshot's own
  `task_stopfuzz_cli`. Budgets: off 900 s, on 1,800 s.
- **Executed:** 8 graded runs.
- **Completed:** 6 (all UNSAT, every checker `verified`, 0 failures).
- **Solver timeout:** 2. These are both gen-15 `on` runs, at 1,800 s; they had
  no oracle.

| Configuration | off | on | Equality (verdict, s/v lines, proof bytes) |
|---|---|---|---|
| gen-25 n50 | UNSAT verified | UNSAT verified | equal |
| **gen-25 n500** | UNSAT verified | UNSAT verified | **equal** |
| gen-15 n50 | UNSAT verified | TIMEOUT 1,800 s | not comparable |
| gen-15 n500 | UNSAT verified | TIMEOUT 1,800 s | not comparable |

- **gen-25 n500 completion and equality are established only by d1.** It is
  the second d1 row (wall 455.2 s for off plus on). d2 did not run it.

**d2 (job 1747244): three configurations.**
- **Configurations:** gen-25 n50, gen-15 n50 and gen-15 n500.
- **Per configuration:**
  - a raw `off` run (`run_cmd`, 900 s);
  - a raw `on` run (`--feedback`, 3,600 s);
  - when `on` completed, a third graded `on` run (`run_case`).
- **Executed:** 9 runs. All completed UNSAT, with 0 timeouts.
  - **Graded:** 3 runs (verdict UNSAT, 0 failures; every proof route reached
    `verified`, otherwise `run_case` would record a `proof` failure).
  - **Ungraded (C3):** 6 raw runs. For each configuration the raw `off` and
    `on` proof files are byte-identical to each other, and their s/v lines are
    identical.

| Configuration | off wall / conflicts / decisions / stress fires | on wall / same counters | on stream records | Proof off = on | Graded on rerun |
|---|---|---|---:|---|---|
| gen-25 n50 | 4.1 s / 67,705 / 86,891 / 203 | 279.9 s / equal | 645,772 | yes | UNSAT, 0 failures |
| gen-15 n50 | 37.0 s / 391,010 / 463,807 / 288 | 3,254.9 s / equal | 3,483,684 | yes | UNSAT, 0 failures |
| gen-15 n500 | 33.5 s / 362,506 / 430,931 / 60 | 2,855.1 s / equal | 3,235,282 | yes | UNSAT, 0 failures |

- **Divergence test.** The maximum conflicts and stress-fire counters over all
  records of the `on` stream never exceeded the `off` run's final totals; they
  are exactly equal. Each stream ends with session_end verdict 20, stop Unsat.
- **Wording correction.** The report's phrase "The two d2 cases ran about 68×,
  88× and 85× slower" should read **the three d2 configurations**. The ratios
  are 279.9/4.1 ≈ 68, 3,254.9/37.0 ≈ 88 and 2,855.1/33.5 ≈ 85.
- **Combined result.** All four f1 variant-timeout rows have a matching
  completed feedback-on run:
  - gen-25 n50: d1 (graded) and d2;
  - gen-25 n500: d1 only (graded);
  - gen-15 n50 and n500: d2 only (counters and proof bytes equal; a graded
    rerun).
- **The f1 rows stay non-evidence.** The diagnoses explain them; they do not
  convert them to passes.
- **Limitation.** The large Debug CLI recording overhead is unchanged
  (feedback overhead measurement is parent-owned).

## 3. Scope of the "root only" statement (R5 / R12)

R5's sentence "Rules and ResumeSearch occur only at root and outside search"
refers to **controller (RandomControllerPolicy) root choices**. The
analyzer's checks are:
- Every controller `Rule` record must have `before.level == 0` and
  `!before.searching`.
- Every resumed controller `ResumeSearch` must start at level 0 and outside
  search.

Separately, for both policies, the R12 check flags any **root-only** family
record inside an open search interval. Root-only families are the 25 root
families except Propagate, Reduce and Reorder.

Neither check reclassifies native or search services:
- DefaultPolicy's level-independent native Propagate, Reduce and Reorder
  (A2), and the search services Decide, ConflictAnalysis, Restart, SwitchMode
  and Warmup, are not subject to the root restriction.
- They are reported as recorded. For example, DefaultPolicy has 1,453
  NativeContinuation and 4,486 PartialContinuation intervals, and Propagate is
  its most frequent refuting record.

## 4. R11 record-field validation scope, and postflight coverage

### What R11 validated, and where

**Analyzer, over every TF feedback stream:**
- Presence on each action record of `version`, `sequence`, `identity`,
  `instance`, `before`, `after`, `outcome`, `stop`, `request` and
  `allowance`.
- The record layout on this pin:
  - action keys: `after, allowance, before, identity, instance, kind,
    [native_request], outcome, request, sequence, stop, type, version`;
  - `outcome` includes `applications, progress, status, budget_status, work,
    unit, composition_overshoot, result`.
- Each action and interval record must be immediately followed by its
  `elapsed_nanos` duration record. The duration value is excluded from
  equality.
- Each DefaultPolicy record must carry `native_request` with a non-empty
  `currency`. Its `work` and `overshoot` are counted: work > 0 in 1,393,571
  records, = 0 in 20,760; overshoot > 0 in 107.
- Support records must have 0 applications and no progress (9,702 checked,
  0 bad).
- Interval start/stop codes, and continuation links to the predecessor.
- `progress` together with the family counter delta, for effects.

**Driver (fbdrv), at every callback:**
- version and monotone sequence;
- the stop reason must equal `stop_reason(outcome.status)`;
- `after.sequence ≥ before.sequence`;
- no action after session end;
- a non-empty identity;
- controller Rule records must carry both request and allowance;
- native records must carry `native_request`;
- support records must not claim applications or progress;
- no callback while conflict analysis is pending, and no unrestored scope.

**Scope driver, per request (9,472 checks):**
- exactly one record per request;
- the record's identity, allowance amount, stop, before/after arena
  generation and `composition_overshoot` must equal the executor's own
  result.

**Not validated beyond presence:**
- the numeric value of `duration_nanos`;
- native `work` against an independent meter. It is recorded and counted, not
  re-measured.

### Pin / checker / harness postflight

The commands and the outcomes quoted below are the existing results.

| Check | Command | Outcome |
|---|---|---|
| Pin, pre-launch, 23:38:23 | `python3 tools/verify_pin.py` | `{"pin_json_sha256": "72a3a254…c136", "binaries": 173, "sources": 795, "tactix": "95197f8c…93c2", "bad": []}`, exit 0 |
| Pin, post, 02:22:59 | same command | identical, `bad: []` |
| Checkers, post, 02:22:59 | `cd tools/fuzz/failures/checkers/895f1989… && sha256sum --strict -c build/pin-fuzz-checkers-shared-feedback-375b621b/SHA256SUMS` | cake_lpr, drat-trim, dsr-compress, dsr-trim, lrat-check, srcheck: OK |
| In-job gates, smoke / f1 / mining (`job.sh`) | snapshot digest `find . -type f ! -path ./SNAPSHOT_SHA256 -print0 \| LC_ALL=C sort -z \| xargs -0 sha256sum \| sha256sum` must equal the recorded value; `verify_pin.py` before and after the run; `sha256sum --strict -c core-checkers.sha256` | all passed (f1/mining snapshot f538b1f8…); no `PIN CHANGED` or `SNAPSHOT CHANGED` |
| Legs | `slurm.sh` in-job checker re-verification | 6 × OK, harness 1fceb82e… |

Postflight coverage of each job:

| Job | Window | Covered by |
|---|---|---|
| Legs, smoke, f1, mining | all ended by 02:09:52 | bracketed by the pre and post checks above, plus their in-job gates |
| d1 (1747211) | 01:39:34–02:58:12 | started inside the bracket, ended after the post check; no in-job pin/checker/snapshot verification |
| d2 (1747244) | 02:35:31–06:24:26 | entirely after the post check; no in-job verification |

The d1 and d2 runs used the same read-only snapshot f1 and its
content-addressed checker collection, but nothing verified them during or
after those jobs. This is gap C2. The f1/mining postflight cannot be extended
to them.

## Limitations kept explicit (not relaxed)

- **Debug CLI feedback-recording overhead:** about 68–88× in d2. It caused 6
  feedback-variant timeouts in f1 (4 CLI stop-fuzz, 2 TF). All 6 remain
  non-evidence.
- **Unavailable producers:**
  - Condition is effective only with an offered witness.
  - Native SubstitutionRedundancy was never effective in L2; SR effects come
    from supplied payloads.
  - LocalSearchGuidance and Propagate record-level effects come from graded
    mining follow-ups (coverage only).
  - The Factor terminal arm was never reached; it is dispositioned by source
    reasoning only.
- **Corrections C1–C3** above. No solver, proof, model or checker failure was
  observed anywhere in the campaign.
