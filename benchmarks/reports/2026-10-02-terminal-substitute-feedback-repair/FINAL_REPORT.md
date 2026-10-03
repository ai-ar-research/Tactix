# FINAL REPORT — shared-policy execution/feedback preparation fuzz, pin f6f01982

Owner: Claude (fuzz owner). Terminal: 2026-10-02.

**Disposition: NOT ACCEPTED.** This covers both-policy preparation fuzz
acceptance: DefaultPolicy and controller-interface-v1 RandomControllerPolicy on
pin f6f01982.

- **Reason:** a deterministic solver contract failure in the targeted
  RandomControllerPolicy driver checks (R12/R11). A FeedbackReceiver callback
  runs while large watches are disconnected, after a refuting Substitute round.
  - Per the request, the campaign stopped there. The reproducer is in
    `FAILURE_HANDOFF.md`.
  - The targeted and mining jobs were cancelled by explicit ID, so rows R2–R13
    are not established on this pin.
- **Not affected:** no proof, model or verdict failure occurred anywhere.
- **Both 100k legs passed on their own terms (R1).** They are not acceptance,
  because the gate needs all rows.
- **Repository hygiene:** nothing was committed or pushed. No root solver,
  test, tool, doc, skill, global checker link, frozen evidence or other
  campaign was modified.

## Snapshot, checkers, harness

**Solver pin:** `build/pin-shared-policy-feedback-debug-f6f01982`.
- Source f6f01982e08b…; Debug, proof shadow on, asserts on.
- `tactix` sha256 7dcbbec3d13c9b3e06b9cd2a5ff82d47ccc96561a95aa3547de4ae2aae3bfc08.
- PIN.json sha256 9dfa5398…fefe.
- `tools/verify_pin.py` checked 173 binaries and 795 sources against git blobs.
  It found 0 differences before submission, inside the smoke job (gate and
  post-run), at the f1 gate, and after all jobs (`status/verify-pin-*.json`).
- The pin was never rebuilt or written.

**Core checkers:** owned frozen copy at
`build/pin-fuzz-checkers-shared-feedback-f6f01982`.
- SHA256SUMS sha256 895f1989468d5e01a81fa647f9cb9739448b81507a4b78515e4e46a7441c44f8.
- Byte-identical to the accepted collection; installed by `slurm.sh --checkers`
  and re-verified in-job.

| Checker | SHA256 |
|---|---|
| cake_lpr | ab9dc6c43ad0f55bbaf336c5d898919ddb03d0bdb75e88dff48237fb06ba1f57 |
| drat-trim | f2cb7d45d6acc05b1295bfa0d27b53d75fa48259c2b1e15f15772f064f99849a |
| dsr-compress | 6bc416a0df73f16911a0b16d85aa4bed4d15e4915d309407ff1e80b07d19d676 |
| **dsr-trim** (canonical repaired) | **3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee** |
| lrat-check (diagnostic) | 0c9a87d5cfb8851350928303094cb7d36317d47786dda243ebcc6250d6eec6d7 |
| srcheck (Trestle) | 19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945 |

VeriPB/CakePB are the harness snapshot's pinned copies. Kissat is the
supplementary differential oracle.

**Routes:** the maintained `fuzz.py` was used unmodified. `tools/` is unchanged
since 0531c2f1, so the leg harness snapshot sha 1fceb82e… is identical to the
accepted one.
- Witness-free native DSR: DRAT conversion → backward drat-trim → CakeLPR,
  plus the forward `drat-trim -f` every-lemma audit.
- Witnessed DSR: no-option dsr-trim → dsr-compress → Trestle, plus the forward
  `dsr-trim -f` audit.
- Startup PBP: VeriPB → CakePB.
- SAT: `modelcheck` against the original CNF.

**Owner harness:** the predecessor's harness, frozen in this namespace with
paths, pin and seeds changed (COVERAGE_PLAN.md).
- `fbdrv`, `cfgdump` and `progcheck` were recompiled against
  `src-f6f01982/` with the pin's flags and linked to this pin's objects. No old
  binary was executed.
- The process-pool, streaming runner was used. No threaded analysis ran.
- Snapshots:
  - `snap-s1` (2306b942…): smoke;
  - `snap-f1` (dd3f36eb…): full targeted and mining.
- `cfgdump` output is byte-identical to the predecessor's default
  configuration. All action collections parse with the pinned parser.

**Description-label adjustment (recorded before any job):**
- The only parser is the DefaultPolicy scheduled-Restart keep level. It now
  matches `retained-level=N;purpose=scheduled`.
- Any `purpose=scheduled` description that does not parse is now a violation.
- No Reduce description is parsed.
- Causal controls pass on the predecessor's old-label stream and on this pin's
  stream (`status/parser-controls/`):
  - the old label is flagged;
  - relabelled streams count 82 and 23 positive levels respectively;
  - a malformed label is flagged.
- On this pin, 2,205 DefaultPolicy positive retained-level restarts were
  parsed with 0 violations.

**Seeds:** fresh, and checked against all retained schedules, including
targeted and other campaigns (COVERAGE_PLAN.md and
`status/freshness-window-scan.txt`).
- Legs use 2020000000 and 2021000000.
- Targeted windows use 2022–2032 ×10⁶. The driver windows were moved off 2026
  because of the 12e/eba campaign seeds 2026100101+.
- The only in-window hits are non-seed statistics, one trace value and the
  request files.

## SLURM jobs

Partition: cpu-dense-hwulab-q. No job used an array throttle or a node
constraint.

| Job | Role | Allocation | Terminal |
|---|---|---|---|
| **1746775** | L1 DefaultPolicy leg | 50 phys/100 CPU/400000M/50 workers, 24h | COMPLETED 0:0, 1:26:32, node017 |
| **1746776** | L2 RandomControllerPolicy leg (`--controller-policy={seed}`) | same | COMPLETED 0:0, 1:08:42, node018 |
| 1746774 | labelled smoke (snap-s1) | 4 phys/8 CPU/30000M | COMPLETED 0:0; SELFTEST OK; 406 rows, 0 fail (not counted) |
| **1746777** | full targeted f1 (snap-f1) | 50 phys/100 CPU/400000M | CANCELLED by owner (explicit ID) after the failure; 3,933 rows retained |
| **1746778** | rare-family mining (L2 seed replay) | same | CANCELLED by owner (explicit ID); 262 rows, coverage only |
| 1746782 | labelled diagnosis d1 (failure determinism) | 2 CPU/8000M | COMPLETED; failure reproduced 3/3 |

Logs:
- legs: `tools/fuzz/slurm-spf-f6f01982-{default-1746775,controller-1746776}.log`;
- owner jobs: `logs/`, `diag/d1/`.

The leg harness pins were removed automatically on success.

## Policy legs (R1 component)

| Policy | Fresh seeds (census: unique, 0 duplicates) | Corpus | SAT | UNSAT | Timeouts (5 s, no oracle) | Failures |
|---|---|---|---:|---:|---:|---:|
| DefaultPolicy (`tactix`) | 2020000010–2020100009 (100,000); struct 30104 / mut 24820 / fuzzsat 30114 / ksat 14962 | 51 entries, 0 failures; 022 crash-oracle-only pass (no verdict at 60 s, no abort) | 57,091 | 40,222 | 2,697 (2.7%) | **0** |
| RandomControllerPolicy | 2021000010–2021100009 (100,000); struct 29944 / mut 25171 / fuzzsat 29954 / ksat 14931 | 51 entries, 0 failures (`--replace-entry-policy`) | 56,193 | 38,166 | 5,651 (5.7%) | **0** |

- Every completed result passed the independent model/proof checks and the
  differential oracle. There were no crash, sanitizer or `c ERROR` reports.
- Timeouts are not passes; 25 per leg are kept under `legs/*/nonterminating/`.
- L2 controller coverage (`legs/L2-controller/controller-coverage.json`):
  - **Configurations:** all 20 had intervals with decisions and conflicts. The
    minimums were 2,240 intervals, 252,884 decisions and 185,043 conflicts.
  - **Families:** 24 of 25 were applied; Condition was not offered, as
    expected. Every applied family was effective except SubstitutionRedundancy,
    whose native SR was never effective. Propagate was applied in 146 cases
    and effective in 133.
  - **Restarts:** 3,087 full and 15,021 partial; 2,628 configuration changes.
  - **Root sequences:** ≥2 root actions in 62,203 cases.

## Targeted evidence before the stop (snap-f1; rows written before cancellation only)

The analysis covers the 3,933 rows in `targeted/out-f1-at-cancel/`.
- Statuses: 3,807 pass, 48 timeouts, 2 variant timeouts, 75 SR-payload emit
  and **1 fail**.
- 26 rows were appended during cancellation and are excluded: 17 harness
  errors, 4 "fail", 4 pass and 1 variant timeout. They are artifacts of the job
  trap deleting scratch on SIGTERM; in the 4 "fail" rows the base runs graded
  cleanly.

Observed so far:
- **TF feedback/replay:**
  - DefaultPolicy: 271/271 pass and 271 positive replays. Negatives were 245/245
    for every class, plus the elapsed must-pass control.
  - Random: 119/119 pass and 119 positive feedback+controller replays.
    Negatives were 94/94 for every class, including controller mismatch,
    truncation, trailing and configuration.
- **Driver purity:**
  - DefaultPolicy: 199 completed, 0 scope or pending violations over 834,208
    callbacks.
  - Random: 199 completed and **1 failure** (2 scope violations).
- **Partial restarts:**
  - Random partial-restart contract: 2,987 OK, 0 bad. The transcript keep level
    matches in all 2,987.
  - DefaultPolicy: 2,205 partial and 914 full scheduled restarts.
- **Scope:** 586 continuations, 1,215 stale with 1,215 refreshed, and 86
  self-stale.
- **Interrupt landings:** all graded, except random search 335/337 and
  rule-work 204/208, which are timeouts.
- **Re-entry:** random 313 non-root stops, each with an explicit entry Restart.
- **CLI stop-fuzz off/on:** 120 + 120 identical.
- **Analyzer causal controls:** OK on this pin's streams
  (`status/analyzer-controls/this-pin.txt`).
- **Not reached before the stop:**
  - random per-family, Condition-witness and SR/LSG payload TF tasks;
  - scope-zero/overshoot cases;
  - known-bad TC controls (0 rows);
  - phase-2 payload follow-ups;
  - mining hits.

## Coverage matrix (achieved vs required, this pin)

| Row | Status |
|---|---|
| R1 | **Legs met:** 100,000 fresh seeds each, completed SAT/UNSAT independently checked, timeouts explicit, corpus 51/51 dispositioned including 022. The row is otherwise not closed, because acceptance stopped. |
| R2 | **NOT MET.** L2 applies 24/25 families. Condition needs an offered witness, and SR effects need offered payloads; neither targeted task ran. The record-level LSG/Propagate mining was cancelled. |
| R3 | Achieved by L2: all 20 configurations, minimum 2,240 intervals with decisions and conflicts. |
| R4 | Partial. L2 has 62,203 cases with ≥2 root actions. Driver purity shows VMTF→EVSIDS 102 and EVSIDS→VMTF 77, with retention checks clean. The row is not closed. |
| R5 | Partial. Random: 2,987 partial OK and 214 full. DefaultPolicy: 2,205 partial and 914 full. Not closed. |
| R6 | **NOT MET.** The overshoot and tick-1 collections did not run before the stop (overshoot-cases 0). |
| R7 | Partial. 586 continuations, 1,215 stale/refreshed, 86 self-stale. Not closed. |
| R8 | Partial. Random before-root-work 130 graded. A DefaultPolicy entry-landing classification was not separately confirmed before the stop. Not closed. |
| R9 | Partial. Purity cases: 199 DefaultPolicy and 199 Random completed; 1 Random case failed. Not closed. |
| R10 | Partial. 271 DefaultPolicy and 119 Random positive replays; all negatives rejected for the intended reason. Not closed. |
| R11 | **FAILED** (receiver lifetime at an unrestored scope, Random). |
| R12 | **FAILED.** A receiver observed an unrestored scope (`large_watches_disconnected`) at the Substitute record and at session_end. |
| R13 | Partial. Selftest OK (smoke and f1 gate). Analyzer and parser controls OK. Known-bad TC controls did not run. |

## Limitations and disclosures

- **Predecessor disclosures preserved** (they guided harness setup only):
  - A1: smoke-derived harness fixes.
  - A2: DefaultPolicy entry root BCP and native level-independent Reorder.
  - A3: only root-fixed or retired variables may leave an inactive ranking.
  - A4: LSG/Propagate record-level effects come only from L2 replays, as
    coverage, not fresh credit.
- **Owner amendment A1 here:** the full job retains mid-fixture samples so the
  analyzer controls run on this pin's streams.
- **Test-only stop sources:** record and selection triggers set the external
  stop flag from the harness, and timers are not bit-reproducible. Random
  rule stop-fuzz reports BudgetExhausted and does not end a solve. True
  interruption uses external flags and timers.
- **Cancellation:** both targeted jobs were cancelled deliberately because of
  the solver failure, not because of an observation timeout or a queue
  disappearance.
- **Untracked artifacts:**
  - `build/pin-fuzz-checkers-shared-feedback-f6f01982/`;
  - `tools/fuzz/slurm-spf-f6f01982-*.log`;
  - the shared content-addressed `tools/fuzz/failures/checkers/895f1989…` and
    `failures/harnesses/1fceb82e…`, which pre-existed.

## Next step

Codex repairs the failure in FAILURE_HANDOFF.md and adds a causal regression.
A fresh owner run on the repaired pin then reruns:
- both ≥100k legs;
- the full targeted matrix (snap-f1 adapted);
- the mining replay.
