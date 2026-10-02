# FINAL REPORT — shared-policy execution/feedback preparation fuzz, pin 0531c2f1

Owner: Claude (fuzz owner). Terminal: 2026-10-02.

**Disposition: ACCEPTED — both-policy preparation fuzz acceptance (DefaultPolicy
and controller-interface-v1 RandomControllerPolicy) on pin 0531c2f1.**

- No solver or oracle failure in either 100,010-case leg, in either corpus
  replay, or in any targeted job. There is no Codex handoff.
- Rows R1–R13 are all MET (matrix below).
- Four harness amendments (A1–A4) refined only the owner's own checks. Each is
  recorded in COVERAGE_PLAN.md before the job it affected, and each is
  disclosed under Limitations.
- Nothing was committed or pushed. No root solver, test, doc, skill, global
  checker link, frozen evidence or other campaign was modified.

## Snapshot, checkers, harness

**Solver pin:** `build/pin-shared-policy-feedback-debug-0531c2f1`.
- Source 0531c2f1fb153f0d6be240041eda41fa835427a8; Debug, proof shadow on,
  asserts on.
- `tactix` SHA256 9998afedeaf9836489f65a97a805a24feeaadf12b21a447c32b93ec3afd8ad99.
- PIN.json SHA256 22dba2a2…c42f.
- `tools/verify_pin.py` re-hashed all 173 binaries and checked all 795 source
  hashes against the commit's git blobs. It found 0 differences each time it
  ran: before the legs, after the legs, inside every targeted job (gate and
  post-run), after the targeted jobs, and after f2. The pin was never rebuilt
  or written.
- HEAD has since moved to 9fa9ea4b (parent work). `git diff 0531c2f1 HEAD --
  src include tools/fuzz tools/bench` is empty.

**Core checkers:** frozen via `slurm.sh --checkers
build/pin-fuzz-checkers-shared-feedback-0531c2f1`.
- Installed read-only at `tools/fuzz/failures/checkers/895f1989…c44f8`
  (SHA256SUMS digest). The jobs re-verified it.
- It is content-identical to the accepted collection.

| Checker | SHA256 |
|---|---|
| cake_lpr | ab9dc6c43ad0f55bbaf336c5d898919ddb03d0bdb75e88dff48237fb06ba1f57 |
| drat-trim | f2cb7d45d6acc05b1295bfa0d27b53d75fa48259c2b1e15f15772f064f99849a |
| dsr-compress | 6bc416a0df73f16911a0b16d85aa4bed4d15e4915d309407ff1e80b07d19d676 |
| **dsr-trim** (canonical repaired) | **3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee** |
| lrat-check (diagnostic only) | 0c9a87d5cfb8851350928303094cb7d36317d47786dda243ebcc6250d6eec6d7 |
| srcheck (Trestle) | 19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945 |

VeriPB and CakePB are the harness snapshot's pinned copies. The differential
oracle is pinned kissat (`tools/solvers/bin`).

**Routes:** the maintained `fuzz.py` (sha 125a71bd…) was used unmodified in
every job.
- Witness-free native DSR: lossless DRAT conversion → backward drat-trim → LRAT
  → CakeLPR, plus the forward `drat-trim -f` every-lemma audit.
- Witnessed DSR: no-option dsr-trim → dsr-compress → Trestle, plus the forward
  `dsr-trim -f` every-lemma audit.
- Startup PBP: VeriPB → CakePB.
- SAT: independent `modelcheck` against the original CNF.
- Every failure retains the exact checker commands and their stdout/stderr.

Targeted UNSAT results by route (base, feedback and replay runs; known-bad
controls excluded). Every route stage verified:

| Policy | Witness-free DSR | Witnessed DSR | PBP |
|---|---:|---:|---:|
| DefaultPolicy | 908 | 110 | 4 |
| Random | 3,339 | 99 | 0 |

**Leg harness:** the `slurm.sh` snapshot sha 1fceb82e… (L1/L2). The snapshot
was removed on success; the replay copy is kept under
`failures/harnesses/1fceb82e…`.

**Owner harness:** immutable snapshots under `targeted/snap-*`, each with its
`SNAPSHOT_SHA256` verified in-job. Contents:
- maintained tools/fuzz, tools/bench and the selftest oracle test at
  0531c2f1, with no overlay;
- `fbdrv`, a C++ driver linked to the pin's own objects and compiled against
  the read-only `src-0531c2f1/` archive with the pin's flags;
- `tgt_runner.py`, actions, fixtures and controls.

| Snapshot | Hash prefix | Role |
|---|---|---|
| f1 | bfe5ee37 | main targeted job |
| f1b | 43cdc749 | follow-up (A2) |
| f1c | 7f36c099 | follow-up (A3, process pool) |
| f2 | 8e77842b | rare-family mining (A4) |
| s1, s2 | — | smoke checkpoints |

The pinned owner tools under `tools/` are:
- `cfgdump`, which printed the default configuration;
- `progcheck`, which validated every action collection with the pinned
  parser;
- `seed_census.py` and `verify_pin.py`.

## SLURM jobs

Partition: cpu-dense-hwulab-q. No job used an array throttle or a node
constraint.

| Job | Role | Allocation | Terminal |
|---|---|---|---|
| **1746357** | L1 DefaultPolicy leg | 50 phys/100 CPU/400000M/50 workers | COMPLETED 0:0, 1:30:08, node018 |
| **1746358** | L2 RandomControllerPolicy leg | same | COMPLETED 0:0, 1:11:58, node017 |
| 1746413 | smoke | 8 phys | cancelled while pending (could not fit idle CPUs); never ran |
| 1746422 / 1746452 | labelled smoke checkpoints s1/s2 | 4 phys/8 CPU/30000M | COMPLETED 0:0 (not counted) |
| **1746454** | targeted f1 | 50 phys/100 CPU/400000M | CANCELLED by owner after 2:52:43; 3,618 complete rows retained (see Limitations) |
| **1746462** | follow-up f1b (A2) | 50 phys | COMPLETED 0:0 |
| 1746470 / 1746471 | labelled diagnosis d1/d2 (one deterministic case) | 2 CPU | COMPLETED |
| **1746474** | follow-up f1c (remaining f1 tasks + A3 rerun) | 50 phys | COMPLETED 0:0 |
| **1746476** | f2 rare-family mining (A4) | 50 phys | COMPLETED 0:0 |

Logs:
- legs: `tools/fuzz/slurm-spf-0531c2f1-{default-1746357,controller-1746358}.log`;
- owner jobs: `logs/`.

## Policy legs (R1)

Seed census (`status/census-L*.json`):
- Each leg has 100,000 fresh generated cases plus the 10 fixed degenerate
  edges, which are not counted.
- All 100,000 cases in each leg are content-unique.
- The generator split is struct/mut/fuzzsat/ksat, 30/25/30/15.
- No retained schedule used any seed in [1.9e9, 2.026e9). The only textual
  matches for the bases are configuration constants, not seeds.

| Policy | Fresh seeds | Corpus | SAT | UNSAT | Timeouts (5 s, no oracle) | Failures |
|---|---|---|---:|---:|---:|---:|
| DefaultPolicy (`tactix`) | 2000000010–2000100009 | 51 entries, 0 failures | 57,363 | 40,066 | 2,581 (2.6%) | **0** |
| Random (`tactix --controller-policy={seed}`) | 2001000010–2001100009 | 51 entries, 0 failures (`--replace-entry-policy`) | 55,863 | 38,419 | 5,728 (5.7%) | **0** |

- **Completed results:** every completed result passed the independent
  model/proof checks and the differential oracle. There were 0 crashes,
  sanitizer or `c ERROR` reports.
- **Corpus entry 022 (`eager-register-promoted-clause`):**
  - DefaultPolicy: it is the crash-only exception. There was no verdict at
    60 s and no abort, which is its recorded pass condition.
  - Random: it reached a graded verdict.
- **Timeouts:** never counted as passes. 25 are kept per leg under
  `legs/L*/nonterminating/`.
- **Corpus size:** the corpus holds 51 entries. `README.md` is not an entry;
  the plan's figure of 52 is corrected here.
- Debug invariants were active throughout. The pin is Debug, and the
  controller calls `kernel_check_invariants` every 256 actions.

L2 controller coverage (`legs/L2-controller/controller-coverage.json`):
- **Families:** every family except Condition was applied. Condition had 0
  requests because no witness was offered; its availability reason is
  `structured-witness-not-offered`.
- **Configurations:** all 20 had intervals and decisions. The minimums were
  2,222 intervals, 247,140 decisions and 176,620 conflicts.
- **Restarts:** 3,213 full and 15,734 partial; 2,791 configuration changes.
- **Root sequences:** a sequence of ≥2 root actions occurred in 61,782 cases.

## Targeted coverage

The merged rows are 6,433 targeted tasks plus 20,000 mining replays
(`targeted/out-f1/matrix.json`). Later reruns supersede earlier rows; there
are 67 superseded rows, all listed in the matrix.

**Results**
- 0 failures.
- 377 timeouts and 12 feedback-variant timeouts. These are non-evidence and
  are never counted.
- Graded completed semantic runs:
  - DefaultPolicy: 1,249 SAT and 1,022 UNSAT;
  - Random: 4,250 SAT and 3,438 UNSAT.
- The maintained selftest returned `SELFTEST OK` in all six owner jobs that
  ran it.

| Row | Achieved (required) | Status |
|---|---|---|
| R1 | Both legs above. | MET |
| R2 | See the **R2 detail** block after this table. | MET |
| R3 | All 20 configurations have intervals with both decisions and conflicts. Sources: L2 (min 2,222 intervals per configuration) and targeted random feedback records (min 471 intervals with conflicts per configuration). | MET |
| R4 | Sequences of ≥2 root actions: L2 61,782 cases; targeted 760. Effective branching changes across full restarts: VMTF→EVSIDS 1,774 and EVSIDS→VMTF 1,597 in feedback records, plus 66/32 in driver selection checks. Inactive-history retention with 0 violations: VMTF-inactive 114 checks, EVSIDS-inactive 112. Root-fixed synchronization: 55 variables fixed during intervals had left both rankings. | MET |
| R5 | See the **R5 detail** block after this table. | MET |
| R6 | See the **R6 detail** block after this table. | MET |
| R7 | Effective continuation: 557 cases, in 14 families. Stale-scope refreshes after an intervening Rephase: 1,177, in 15 families, with 0 undo, and no refresh was stale again. Self-invalidation by publication/retirement: 77 (Instantiate, Reduce), each refreshed with no undo. Random TF also recorded 47 Stale records. 8,947 per-request record checks: exactly one record, with request/allowance/stop matching. | MET |
| R8 | See the **R8 detail** block after this table. | MET |
| R9 | See the **R9 detail** block after this table. | MET |
| R10 | Positive feedback replays (all passed): DefaultPolicy 273; Random 1,884, each with simultaneous `--controller-replay`. Negatives: every class rejected for its intended reason in all 247 DefaultPolicy and 289 Random cases, with s/v lines and proof bytes equal to the no-feedback run. Classes: event corruption → `divergence at line L` (exact line); truncation; trailing event; header provenance edit and changed input bytes → line 1; changed configuration (seed or `--bve-gates`) → line 1. Must-pass control: an edited elapsed value. Controller-transcript negatives (all 289): mismatch / truncation / trailing / configuration. There is no DefaultPolicy controller-transcript interface, and none was demanded. | MET |
| R11 | All analyzed records carry version, sequence, identity, instance, request/allowance (controller rules), native_request (native), before/after observations, outcome (progress, work, unit, overshoot), stop and a duration record. 9,278 support records, 0 of which claimed applications or progress. Native records: 1,250,957 with work above zero and 18,718 with zero work. Interruption records: 2,906 from DefaultPolicy stop-fuzz, plus every landing record. Early-terminal lifetimes: 210 / 512. 0 callbacks inside a pending conflict analysis over all driver callbacks (≈1.6 M). | MET |
| R12 | Interval starts: DefaultPolicy ResumeSearch 63 / NativeContinuation 1,389 / PartialContinuation 3,454; Random ResumeSearch 8,112 / PartialContinuation 72,258. Every continuation is linked to its predecessor. RootPreparation (194) and StrategyChange (111) stops are distinguished. 0 root-only actions inside an open interval (3 native Reorder rows reclassified under A2). 0 receiver callbacks at unrestored scopes. | MET |
| R13 | See the **R13 detail** block after this table. | MET |

**R2 detail**
- **Applications:** all 25 families were applied through controller
  execution: L2, plus feedback records from the targeted random runs.
- **Record-level effects:** every family has record-level, family-specific
  effects, meaning progress together with that family's Stats delta. Case
  counts: BCE 30, BVE 94, Backbone 9, CCE 58, Condition 31, Congruence 5,
  Factor 11, HyperBinaryRes 16, HyperTernaryRes 66, Instantiate 8, Lucky 3,
  Probe 15, Reduce 29, Reorder 99, Rephase 85, Strengthen 39, Substitute 8,
  SubstitutionRedundancy 50, SubsumeForward 15, Sweep 26, TransitiveReduce 3,
  Vivify 48, Walk 103.
- **LocalSearchGuidance (40 cases) and Propagate (3):** these effects come
  from f2, the mining replay of L2 cases (A4). All are fully graded with
  negatives.
- **Condition:** effective only with the explicitly offered witness
  (`Condition-witness.prog`).
- **SubstitutionRedundancy:** effective only with offered explicit payloads,
  produced by the pin's own orbitopal and graph-symmetry producers on 9
  inputs.

**R5 detail**
- **Random:**
  - Positive keep-level restarts: 72,258, all meeting the contract. Each
    keeps the same configuration, rotation clock and reluctant (u,v), has
    level equal to the keep level, refreshes `next_restart_conflicts` above
    the current conflict count, and advances the strategy epoch.
  - The transcript `service Restart K … level` matches K in 72,258 cases with
    0 mismatches.
  - Full restarts: 6,801.
  - Rules and ResumeSearch occur only at root and outside search.
- **DefaultPolicy:**
  - Scheduled partial restarts: 3,454, each followed by a
    PartialContinuation interval.
  - Full (keep-level 0) restarts: 1,084.

**R6 detail**
- **Allowances:** fixed per collection and recorded on every controller rule
  record. Defaults are Applications×1, BVE 100000 bve-ticks and Propagate
  100000 propagations; the zero, tick-1 and native-effort profiles are
  explicit.
- **Exhaustion and zero work:**
  - random Budget stops: 412;
  - zero-work rule records: 622;
  - scope zero-allowance exhaustions: 1,545, with 0 applications.
- **Native effort:** recorded in its own currency.
- **Atomic overshoot above zero:**
  - controller composition: 43 records in 14 cases;
  - native: 89 records.
- **Fresh doses:** continuation with progress in 557 cases.
- **No global loop budget:** there is none. One L2 root sequence reached 177
  actions.

**R8 detail**

| Landing | DefaultPolicy | Random |
|---|---|---|
| Before root work | 62 graded (entry stop: no rule progress other than native root BCP; irredundant clauses unchanged) | 139 graded |
| Within rule work | 338 graded (Lucky, timers) | 196 graded (selection-triggered stop; timers) |
| During search | 254 graded | 341 graded |
| At a root boundary | 17 | 143 |
| Stopped above root | 202 | 348 |

- **Every stopped session:**
  - ends its feedback session exactly once, with reason Interrupted and any
    open interval closed;
  - restores the heuristic owner, saved-phase pointer and stop pointer;
  - passes `kernel_check_invariants`.
- **Stop-set entry:**
  - the controller leaves the fingerprint untouched;
  - DefaultPolicy normalizes a non-root trail with a `purpose=preparation`
    Restart while keeping committed state.
- **Random re-entry:** 319 re-entries after non-root stops start with an
  explicit Restart, from level>0 to 0, with irredundant, learned, arena and
  root state retained.
- **Graded results:** every completed resumed result was graded.
- **Stop-fuzz:** stop-fuzz with feedback off and on gave identical outputs.
  Driver runs: 108 + 108; CLI runs: 113 + 112. The DefaultPolicy runs carried
  2,906 interrupted rule records.

**R9 detail**
- Driver purity cases: DefaultPolicy 198 and Random 197, completed and
  graded.
- Each compares three runs: A plain, B with a receiver plus extra
  observation, C with a receiver only. Proof bytes, model, deterministic
  counters (timers excluded), transcript and coverage are equal, and the B and
  C record streams are equal.
- Extra observation ran in 1.6 M callbacks plus 8,377 selection-wrapper
  checks, with 0 state changes. The fingerprint covers raw Stats,
  stop-fuzz RNG state and H.
- The pairs include:
  - DefaultPolicy productive inprocessing: 1,912 productive rule records;
    125 TF cases;
  - DefaultPolicy support: 6,999 records;
  - Random partial continuation: 3,168 partial-restart records.
- CLI off/on pairs are proof- and line-identical: DefaultPolicy 273, Random
  1,884.

**R13 detail**
- **Maintained selftest:** OK in jobs 1746422, 1746452, 1746454, 1746462,
  1746474 and 1746476. It covers the kissat/cadical clean runs, the broken
  wrappers, every-lemma causal controls and checker-collection controls, and
  used the pin for its native-LRAT control.
- **Known-bad controls on both policies:**

  | Control | DefaultPolicy | Random | Inapplicable cases |
  |---|---|---|---|
  | lie-UNSAT-as-SAT | 31/31 flagged | 27/27 flagged | — |
  | negate-model | 47/47 flagged | 46/46 flagged | — |
  | empty proof | 12/12 applicable flagged | 8/8 applicable flagged | 19 per policy are root-UP-refutable |
  | ASCII truncation | 11 flagged | 8 flagged | 19 per policy are UP-refutable; DefaultPolicy also has 1 whose kept prefix verified as a genuine refutation |

- **Feedback and controller replay negatives:** as in R10.
- **Analyzer causal controls:** OK (`targeted/analyzer-controls/`). They
  detect a root rule inside an interval, a missing duration record, a missing
  session end and a broken continuation link.
- **Checker commands:** retained in every row (`checkers`), with hashes as
  above.

## Limitations and disclosures

**A1 — smoke-derived harness fixes**, made before the main job:
- interval-span reconstruction for native `resume()`;
- negatives on streams without requests or actions;
- added stop sources (Decide triggers, selection-triggered stop, longer
  timers);
- tick-1 overshoot collections;
- 5× variant timeouts.

**A2 — two owner expectations were wrong for native DefaultPolicy.** Neither
was a solver defect.
- **Entry stop.** DefaultPolicy closes input units under root BCP before its
  first stop check.
  - The 51 affected rows exited 3 and were not graded.
  - They were re-run in f1b (same ids, seeds and inputs) with the refined
    check: rule progress by Propagate only, root growth only with it, and
    irredundant clauses unchanged.
  - Result: 62 graded, 4 timeouts, and BCP-only in 52.
- **Native Reorder** is level-independent (Kissat's search-loop reorder). In
  three mid-size DefaultPolicy feedback rows its only violation was "Reorder
  inside an interval". Those rows were reclassified. Their base, feedback and
  replay runs were graded and their checks passed.

**A3 — the retention check was too strict.**
- Case: corpus 032, seed 2006000151.
- Diagnosis (labelled jobs d1/d2): the inactive VMTF lost variable 3. That
  variable was root-fixed before the interval but still queued, and was
  drained by ConflictAnalysis.
- This is root-fixed eligibility synchronization, the class the accepted
  predecessor admitted.
- The refined rule allows only root-fixed or retired variables to leave a
  ranking. All other order, stamp and score checks are unchanged.
- The row was re-run in f1c and passed.
- Related diagnostic, not a criterion: root-fixed variables can still be
  queued at a controller activation (210 such checks). Decisions skip
  assigned variables, and synchronization occurs on the next drain.

**A4 — LocalSearchGuidance and root Propagate effects are rare.** At the
record level they appear only in replays of L2 cases (f2: 20,000 L2 seeds
replayed with `--feedback`; 55 LSG and 3 Propagate hits; 43 hits fully
graded). These replays are coverage evidence, not extra fresh seeds.

**Job 1746454 was cancelled by the owner, by explicit job id.**
- Cause: its threaded runner was GIL-bound on JSON feedback analysis (one CPU
  busy, 49 GB RSS). This was not an observation timeout.
- Its 3,618 complete rows are retained (`rows-1746454.jsonl`).
- The remaining 2,144 tasks plus 672 phase-2 tasks ran in f1c. f1c used a
  process pool and streaming analysis that is output-identical on the
  recorded samples. Task definitions and seeds were unchanged.

**Other limitations**
- **Stop-fuzz under the controller:** stop-fuzz fires inside controller rules
  are reported as BudgetExhausted, not Interrupted, so they never end a
  controller solve. Random rule-work interruption evidence therefore comes
  from real stop flags: the selection trigger and timers. DefaultPolicy
  classifies stop-fuzz fires as Interrupted records.
- **Test-only stop sources:** the record- and selection-triggered stop
  sources set the *external* stop flag from the harness. They emulate an
  asynchronous signal at that boundary. Production receivers do not steer.
  Timer landings are not bit-reproducible.
- **Native SR in L2:** native SR with the reference profile was never
  effective in L2 (0 of 45,900 applications). SR effects come from offered
  producer payloads.
- **Startup PBP:** observed 4 times in targeted DefaultPolicy cases. The legs
  do not print route counts. The route stays active in the harness.
- **Cosmetic:** `run.sh` prints "10000 cases" in its header, while `--n
  100010` governs; progress and summary lines show 100,010.
- **Untracked artifacts:**
  - `build/pin-fuzz-checkers-shared-feedback-0531c2f1/`;
  - `tools/fuzz/slurm-spf-0531c2f1-*.log`;
  - `tools/fuzz/failures/harnesses/1fceb82e…/`.

  The leg harness pins were removed automatically on success.

## Evidence locations (this directory)

- `COVERAGE_PLAN.md`: frozen plan and amendments A1–A4, with the hash chain in
  `status/plan-freeze.sha256`.
- `OWNER_STATUS.md`: timeline.
- `legs/L1-default`, `legs/L2-controller`: nonterminating samples and L2
  coverage.
- `targeted/out-f1/rows.jsonl`: rows from 1746454 and 1746474.
- `targeted/out-f1b/`, `targeted/out-f2/`.
- `targeted/out-f1/matrix.json`: merged matrix, including superseded and
  reclassified rows.
- `targeted/out-f1/samples/`: feedback streams.
- `diag/d1`, `diag/d2`.
- `tools/`: owner tools.
- `src-0531c2f1/`: read-only headers used by the drivers.
