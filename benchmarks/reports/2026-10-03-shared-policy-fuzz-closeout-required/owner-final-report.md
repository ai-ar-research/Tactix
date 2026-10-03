# FINAL REPORT — shared-policy execution/feedback preparation fuzz, pin 375b621b

Owner: Claude (fuzz owner). Terminal: 2026-10-03.

**Disposition: ACCEPTED** for both-policy preparation fuzz acceptance on pin
375b621b: DefaultPolicy and the controller-interface-v1 RandomControllerPolicy.

This owner fuzz acceptance alone does not accept the whole goal. The parent
separately owns the full Debug suite, the four default regression gates and the
feedback-overhead measurement.

- **No solver or oracle failure.** None in either 100,000-seed leg, either
  corpus replay, the targeted matrix or the mining replay. There is no Codex
  handoff.
- **Rows R1–R13 and the terminal-watch row (TW) are all MET.** See the matrix
  below.
- **The terminal Substitute repair is causally verified.**
  - Corpus 009 with seed 2032000128 now refutes at Substitute with the watches
    restored at both the refuting record and session_end, under both retention
    settings.
  - The same driver linked to the predecessor pin f6f01982 still detects the
    defect.
- **Harness amendments.** One harness bug was found by the smoke run and fixed
  before the full job (Amendment A1, below). One pre-registered reclassification
  rule was applied, and labelled diagnoses d1/d2 confirm it.
- **Repository hygiene.** Nothing was committed or pushed. I did not modify the
  root solver, tests, tools, docs, skills, global checker links, frozen
  evidence or other campaigns.

## Snapshot and candidate verification

**Solver pin:** `build/pin-shared-policy-feedback-debug-375b621b`.
- Source 375b621ba6677600f3e77bf8383af25d256d7b67; Debug, proof shadow on,
  asserts on.
- `tactix` SHA256 95197f8c9f468bd3b121b1cd66d65457b9d5baade62bffb27f95e82caff593c2.
- PIN.json SHA256 72a3a254…c136.
- `tools/verify_pin.py` re-hashed all 173 binaries and checked all 795 source
  hashes against the commit's git blobs. It found **0 differences** each time:
  - before any submission (`status/verify-pin-pre-L.json`);
  - in-job, at the gate and after the run, for smoke, f1 and mining;
  - after all jobs (`status/verify-pin-post.json`).
- The pin was never rebuilt or written.
- HEAD has since moved to 4b4fa609 (parent work). `git diff 375b621b HEAD --
  src include tools tests` is empty.

**Candidate delta:** I verified it before launch, and it matches the request.
- `git diff f6f01982 375b621b -- src include tools tests` touches exactly four
  files.
- `substitute.cpp`: the `finish` lambda no longer exempts `Unsat` from the
  owned reconnect.
- `substitute_execution.hpp`: the batch `return out` becomes `break`, which
  reaches the existing final owned reconnect.
- `test_substitute_execution.cpp`: tests rewrite refutation after
  disconnection, single round and batch, owned and borrowed.
- `test_policy_feedback.cpp`: tests the 009 formula with seed 2032000128 under
  both retention settings.
- There is no selection, scheduling, guard, proof-vocabulary, budget,
  rollback or reward change, and `tools/` is unchanged.

## Checkers, routes, harness

**Core checkers:** frozen as `build/pin-fuzz-checkers-shared-feedback-375b621b`
(owned, read-only).
- SHA256SUMS sha256
  895f1989468d5e01a81fa647f9cb9739448b81507a4b78515e4e46a7441c44f8. This is
  byte-identical to the accepted collection.
- `slurm.sh --checkers` installed it at
  `tools/fuzz/failures/checkers/895f1989…`. It was re-verified in every job and
  after completion (`status/checkers-post.txt`, all OK).

| Checker | SHA256 |
|---|---|
| cake_lpr | ab9dc6c43ad0f55bbaf336c5d898919ddb03d0bdb75e88dff48237fb06ba1f57 |
| drat-trim | f2cb7d45d6acc05b1295bfa0d27b53d75fa48259c2b1e15f15772f064f99849a |
| dsr-compress | 6bc416a0df73f16911a0b16d85aa4bed4d15e4915d309407ff1e80b07d19d676 |
| **dsr-trim** (canonical repaired) | **3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee** |
| lrat-check (diagnostic only) | 0c9a87d5cfb8851350928303094cb7d36317d47786dda243ebcc6250d6eec6d7 |
| srcheck (Trestle) | 19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945 |

VeriPB and CakePB are the snapshot's pinned copies. The supplementary
differential oracle is the pinned kissat.

**Routes:** the maintained `tools/fuzz/fuzz.py` at 375b621b was used
unmodified. Every failure would retain its exact checker commands and outputs;
each row records the checker basenames and statuses.
- Witness-free native DSR: lossless DRAT → backward `drat-trim` → CakeLPR,
  plus the forward `drat-trim -f` every-lemma audit.
- Witnessed DSR: no-option `dsr-trim` → `dsr-compress` → Trestle, plus the
  forward `dsr-trim -f` audit.
- Startup PBP: VeriPB → CakePB.
- SAT: `modelcheck` against the original CNF.

Targeted UNSAT results by route. These count the base, feedback and replay
runs; known-bad controls are excluded. Every stage reported `verified`.

| Policy | Witness-free DSR (drat-trim+CakeLPR+forward audit) | Witnessed DSR (dsr-trim+dsr-compress+Trestle) | Startup PBP (VeriPB+CakePB) |
|---|---:|---:|---:|
| DefaultPolicy | 832 | 157 | 4 |
| Random | 3,699 | 93 | 0 |

**Leg harness:** the `slurm.sh` snapshot, aggregate sha
1fceb82e94b70bb57ec60d7b6e69b6cb55eecb155d974b0059be649e17cb5fc3. This is
identical to the accepted harness, and it was removed automatically on success.

**Owner harness:** immutable snapshots, each with its `SNAPSHOT_SHA256`
verified in-job.

| Snapshot | Hash | Role |
|---|---|---|
| `targeted/snap-s1` | 710ff443… | smoke |
| `targeted/snap-f1` | f538b1f895ebc92306493c42e429138e74cef9af438774fc03c16e52af89595b | full targeted job and mining |

Contents:
- the maintained `tools/fuzz`, `tools/bench` and selftest oracle test at
  375b621b, with no overlay;
- `fbdrv` (sha d6b56a53…), recompiled with the pin's flags against read-only
  `src-375b621b/` and linked to this pin's objects (0 warnings);
- `tgt_runner.py` (process pool with streaming analysis), `analyze.py`,
  actions, fixtures and controls.

Owner tools:
- `cfgdump` output is byte-identical to the predecessor default configuration
  (`status/controller-config-default.txt`).
- All 31 emitted action collections re-emit byte-identically.
- All 33 collections pass the pinned parser (`status/progcheck-actions.txt`).

**Causal-control driver:** `fbdrv-ctl-f6f01982` (sha a3ba600b…). It is the same
`fbdrv.cpp` linked to the predecessor pin, and it is used only for labelled
`twctl` controls.

## SLURM jobs

Partition: cpu-dense-hwulab-q. No job used an array throttle or a node
constraint.

| Job | Role | Allocation | Terminal |
|---|---|---|---|
| **1747082** | L1 DefaultPolicy leg | 50 phys / 100 CPU / 400000M / 50 workers, 24h | COMPLETED 0:0, 1:24:10, node017 |
| **1747083** | L2 RandomControllerPolicy leg (`--controller-policy={seed}`) | same | COMPLETED 0:0, 1:06:19, node018 |
| 1747084 | labelled smoke (snap-s1) | 8 phys / 16 CPU / 64000M | COMPLETED, status 0 (not counted) |
| **1747085** | full targeted (snap-f1, profile full) | 50 phys / 100 CPU / 400000M / 50 workers, 24h | COMPLETED 0:0, 0:52:03 |
| **1747086** | rare-family mining (snap-f1, profile mine) | same | COMPLETED 0:0, 0:41:20 |
| 1747211 | labelled diagnosis d1 (CLI stop-fuzz variant timeouts) | 4 CPU / 16000M | COMPLETED |
| 1747244 | labelled diagnosis d2 (divergence test) | 4 CPU / 16000M | COMPLETED 0:0 |

All jobs were monitored with `tools/slurm/wait.py` (`logs/wait-*.out`). No job
was restarted, and none disappeared from the queue.

Logs:
- legs: `tools/fuzz/slurm-spf-375b621b-{default-1747082,controller-1747083}.log`;
- owner jobs: `logs/`, `diag/d1`, `diag/d2`.

## R1 — seed and result accounting (per policy, four separate categories)

The fresh seeds are base+10 … base+100009 (offline census in
`status/census-L*.json`):
- **L1:** seeds 2040000010–2040100009. 100,000 generated, all content-unique,
  0 duplicates. Generator split: struct 29,873 / mut 25,123 / fuzzsat 29,895 /
  ksat 15,109.
- **L2:** seeds 2041000010–2041100009. 100,000 unique, 0 duplicates. Split:
  struct 30,131 / mut 25,129 / fuzzsat 29,968 / ksat 14,772.

Freshness: 0 hits in any leg or targeted window (Amendment F,
`status/freshness-scan-v2.*`).

The accounting below is in `status/accounting-L{1,2}.json`, produced by
`tools/leg_accounting.py`:
- Timeouts by index come from `nonterminating.json`. It lists every timeout
  index, and the totals match.
- Failures by index come from the bundles; there are 0.
- Fixed-prefix verdicts come from the `ec-*` reruns of the identical command
  and seed.

**(1) Fresh generator cases (only these count toward 100,000)**

| Policy | Executed | SAT | UNSAT | Solver timeout (5 s, no oracle) | Crash/harness/unknown | Sum | Oracle failure flags |
|---|---:|---:|---:|---:|---:|---:|---:|
| DefaultPolicy | 100,000 | 57,423 | 39,901 | 2,676 (2.68%) | 0 | 100,000 ✓ | 0 |
| Random | 100,000 | 56,018 | 38,302 | 5,680 (5.68%) | 0 | 100,000 ✓ | 0 |

**(2) Fixed-prefix harness controls** (leg indices 0–9; these are the
`gen_edge` formulas, which do not depend on the seed)

| Policy | Executed | SAT | UNSAT | Timeout | Failure |
|---|---:|---:|---:|---:|---:|
| DefaultPolicy | 10 | 7 | 3 | 0 | 0 |
| Random | 10 | 7 | 3 | 0 | 0 |

- The leg totals reconcile as fresh plus fixed:
  - L1: SAT 57,430, UNSAT 39,904, TIMEOUT 2,676, total 100,010.
  - L2: SAT 56,025, UNSAT 38,305, TIMEOUT 5,680.
- The `ec-*` reruns: 20 out of 20 passed graded, and the verdict matched the
  leg for each index.

**(3) Retained corpus** (51 entries per leg)
- **Leg replay:**
  - L1: 51 entries, 0 failures. 022 `eager-register-promoted-clause` is
    crash-oracle-only, as declared: no verdict at 60 s and no abort.
  - L2 (`--replace-entry-policy`): 51 entries, 0 failures.
- **Owner per-entry census with the identical replay semantics** (`cc-*`):

  | Policy | Entries | SAT | UNSAT | Crash-oracle-only | Failure | Nonterminating |
  |---|---:|---:|---:|---:|---:|---:|
  | DefaultPolicy | 51 | 12 | 38 | 1 (022) | 0 | 0 |
  | Random | 51 | 12 | 39 | 0 | 0 | 0 |

  Every completed entry was graded.
- No other corpus oracle was weakened.

**(4) Targeted, replayed and mined cases** (`status/census-targeted.json`).
None of these count as fresh.
- **DefaultPolicy targeted:** 2,095 rows.
  - driver: 1,239 pass and 64 timeouts;
  - TF: 283 pass, 6 timeouts, 2 feedback-variant timeouts;
  - CLI stop-fuzz: 114 pass, 4 feedback-variant timeouts (reclassified, see
    Limitations), 2 timeouts;
  - corpus census: 50 pass and 1 crash-oracle-only;
  - known-bad controls: 89 flagged, 223 unflagged-inapplicable, 8 timeouts;
  - edge controls: 10 pass.
- **Random targeted:** 5,090 rows.
  - driver: 2,331 pass and 93 timeouts. These include the 20 public
    terminal-watch execution rows, which are policy-neutral.
  - TF: 2,083 pass and 82 timeouts;
  - CLI stop-fuzz: 114 pass and 6 timeouts;
  - corpus census: 51 pass;
  - known-bad controls: 88 flagged, 216 unflagged-inapplicable, 16 timeouts;
  - edge controls: 10 pass.
- **Other targeted rows:** 80 SR/Lucky+LSG payload emissions, 4
  causal-control detections and 2 must-pass controls.
- **Mining** (L2 seeds 2041000010–2041020009, replayed, coverage only):
  - 20,000 replays: exit 10 = 11,382, exit 20 = 7,702, timeout = 916.
  - 42 hit cases were fully graded with all negatives (42 pass).

**Graded completed semantic runs** across f1 and mining:
- DefaultPolicy: 1,389 SAT and 993 UNSAT;
- Random: 5,203 SAT and 3,792 UNSAT.

Every completed run was checked independently. Timeouts never pass and never
count as verified.

**Debug invariants** were active throughout: the pin is a Debug build with
asserts, and the controller calls `kernel_check_invariants`.

## Coverage matrix (achieved vs required, this pin)

| Row | Achieved (minimum) | Status |
|---|---|---|
| R1 | Both legs: 100,000 unique fresh seeds each, completed SAT and UNSAT in each, every completed result checked, timeouts explicit. Four categories reconciled (above). All corpus entries dispositioned, including 022. | MET |
| R2 | All 25 families applied through controller execution, and all 25 have record-level family-specific effects (table below). | MET |
| R3 | All 20 configurations entered intervals with decisions and conflicts. L2 minimum per configuration: 2,251 intervals, 261,847 decisions, 193,264 conflicts. Targeted minimum: 393 intervals, all with decisions, 366 with conflicts. Counts by name are in the table below. | MET |
| R4 | See **R4 detail** below the table. | MET |
| R5 | See **R5 detail** below the table. | MET |
| R6 | See **R6 detail** below the table. | MET |
| R7 | See **R7 detail** below the table. | MET |
| R8 | See **R8 detail** below the table. | MET |
| R9 | See **R9 detail** below the table. | MET |
| R10 | See **R10 detail** below the table. | MET |
| R11 | See **R11 detail** below the table. | MET |
| R12 | See **R12 detail** below the table. | MET |
| R13 | See **R13 detail** below the table. | MET |
| TW | See **TW detail** below the table, and the terminal-watch section. | MET |

**R4 detail**
- ≥2 root actions before ResumeSearch: 62,030 L2 cases and 783 targeted cases.
- Effective branching changes across full restarts:
  - VMTF→EVSIDS: 1,265 in feedback records, plus 147 driver selections;
  - EVSIDS→VMTF: 1,183 in feedback records, plus 113 driver selections.
- Inactive-history retention, with 0 violations. Only root-fixed or retired
  variables may leave a ranking (A3).
  - VMTF-inactive checks: 210 purity + 6 stop-fuzz;
  - EVSIDS-inactive checks: 379.
- Root-fixed synchronization: 111 variables were fixed during intervals, and
  all had left both rankings.

**R5 detail**
- **Random:**
  - 50,175 positive keep-level restarts, all meeting the contract: same
    configuration, rotation clock and reluctant (u,v); level equal to the keep
    level; `next_restart_conflicts` refreshed above the current conflicts; the
    strategy epoch advanced.
  - The transcript keep level matches in 50,175 cases, with 0 mismatches.
  - Full restarts: 5,090. L2 also has 15,680 partial and 3,142 full restarts.
- **DefaultPolicy:**
  - 4,486 scheduled partial restarts, each followed by a PartialContinuation
    interval; parsed `retained-level=N`, 0 parser violations.
  - 1,125 full restarts.
- Rules and ResumeSearch occur only at root and outside search.

**R6 detail**
- **Allowances:** fixed and recorded on every controller rule record. The
  defaults are BVE 100000 bve-ticks, Propagate 100000 propagations and
  Applications×1 elsewhere. The zero, tick-1 and effort profiles are explicit.
- **Exhaustion and zero work:**
  - random Budget stops: 447;
  - zero-work rule records: 831 random and 3,919 default;
  - scope zero-allowance exhaustions: 1,639, with 0 applications.
- **Native effort:** 1,393,571 records with measured work above zero and
  20,760 with zero work.
- **Nonzero atomic overshoot:**
  - controller composition: 20 records in 9 cases;
  - native: 107 records.
- **Fresh doses:** continuation with progress in 593 cases.
- **No global loop budget:** there is none. One L2 case reached 200 root
  actions in a single sequence.

**R7 detail**
- Effective continuations: 593, in 15 families.
- Stale requests after an intervening Rephase: 1,240, all refreshed.
  - 14 families, with 0 undo.
  - No refresh was stale again.
- Native publication/retirement self-invalidation: 86, in Instantiate and
  Reduce, each refreshed with no undo.
- Random TF also has 179 Stale records.
- Per-request record checks: 9,472, with exactly one matching record each.

**R8 detail** (graded landings)

| Landing | DefaultPolicy | Random |
|---|---|---|
| Before root work | 54 graded entry stops: no rule progress except native entry root BCP (Propagate), no root growth, irredundant clauses unchanged | 125 entry stops (120 graded; resumed solves for the other 5 timed out) |
| Within rule work | 357 graded | 306 (282 graded) |
| During search | 216 graded | 292 (277 graded) |
| Stopped sessions | 584, 165 of them above root | 926, 275 of them above root |

- **Random re-entry:** 263 re-entries after non-root stops start with an
  explicit entry Restart, with committed state retained.
- **Stop-set entry:**
  - DefaultPolicy: 584 checks. It normalizes only with a preparation Restart.
  - Controller: 865 graded rows; the fingerprint is unchanged.
- **Heuristic ownership** is restored, and every session ends exactly once.
- **Stop-fuzz** adds no polls. Off/on comparisons were identical in:
  - driver runs: 114 + 114;
  - CLI runs: 114 + 114 pass.
  - The 4 CLI feedback-variant timeouts were shown identical by d1/d2.

**R9 detail**
- Purity cases completed and graded (A plain / B receiver plus extra
  observation / C receiver only): DefaultPolicy 204, Random 428, plus 114 + 114
  stop-fuzz cases.
- In each, proof bytes, model, deterministic counters (timers excluded),
  transcript and coverage are equal, and the B and C record streams are equal.
- Extra observation ran in 20,100,527 callbacks, with 0 state changes.
- The pairs include:
  - DefaultPolicy productive inprocessing: 2,223 productive rule records;
    inprocessing effects in 137 TF cases;
  - DefaultPolicy support: 7,067 records;
  - Random partial continuation: 5,256 records;
  - 009 terminal-watch purity, under both retention settings.

**R10 detail**
- Positive feedback replays, with proof and lines byte-equal:
  - DefaultPolicy: 283;
  - Random: 2,083 + 42 mined, each with `--controller-replay`.
- Negatives: all classes rejected for the intended reason.
  - DefaultPolicy: 257 / 257.
  - Random: 256 / 256 in f1 and 42 / 42 in mining.
- Feedback negative classes:
  - corrupt → `divergence at line L` (exact line);
  - truncation;
  - trailing event;
  - header provenance edit → line 1;
  - changed input → line 1;
  - changed configuration → line 1;
  - must-pass control: an edited elapsed value.
- Controller-transcript negatives: mismatch, truncation, trailing and
  configuration.
- Feedback errors stay out of band: s/v lines and proof bytes equal the
  no-feedback run.

**R11 detail**
- Analyzed records are complete. Support records: 9,702, of which 0 claimed
  applications or progress.
- Native work: measured 1,393,571, zero 20,760.
- Interrupted records: DefaultPolicy stop-fuzz 1,389, plus every landing.
- Early-terminal lifetimes: 223 DefaultPolicy and 495 Random.
- 0 callbacks inside a pending conflict analysis, over 20,100,527 checked
  callbacks.

**R12 detail**
- Interval starts:
  - DefaultPolicy: ResumeSearch 60, NativeContinuation 1,453,
    PartialContinuation 4,486;
  - Random: ResumeSearch 6,656, PartialContinuation 50,175.
- Every continuation is linked to its predecessor.
- RootPreparation (213) and StrategyChange (115) stops are distinguished.
- 0 root-only actions inside an open interval.
- **0 callbacks at unrestored scopes**, over 20.1 M callbacks. This includes
  all refuting records and 6,286 session ends: all 6,286 checked and all
  connected.

**R13 detail**
- **Maintained selftest:** `SELFTEST OK` in smoke, f1 and mining. It ran with
  the frozen checkers, and used the pin for its native-LRAT control.
- **Known-bad controls:** flagged in every applicable case (detail below).
- **Analyzer and parser causal controls:** OK on this pin's streams.
- **Terminal-watch causal controls:** 4 / 4 detected and 2 / 2 must-pass.
- **Checker commands:** retained in every row.

**TW detail**
- 009 with seed 2032000128:
  - Both retention settings end at `fbd-terminal Substitute`, with connected
    watches at the refuting record and at session_end.
  - Purity is equal, and the certificate is verified.
  - CLI feedback/replay and all negatives pass.
- Public execution: reach and pass 5 / 5 for each of the four combinations.
- Factor/Vivify arms are dispositioned.

### R2 detail: per-family evidence (Random; L2 coverage + targeted feedback records)

"Effective" means the record shows progress together with that family's
specific native counter delta. Calls and NoOps alone are not counted.

| Family | L2 applied / effective cases | Targeted TF applied / record-level effective cases | Mining (graded) effective cases |
|---|---:|---:|---:|
| BCE | 27,269 / 12,167 | 122 / 58 | 15 |
| BVE | 27,381 / 23,189 | 121 / 110 | 22 |
| Backbone | 27,353 / 1,235 | 113 / 9 | 1 |
| CCE | 27,261 / 18,383 | 105 / 56 | 21 |
| Condition | 0 / 0 (no witness offered: `structured-witness-not-offered`) | 27 / 27 (offered witness `Condition-witness.prog`) | 0 |
| Congruence | 27,110 / 5,376 | 116 / 16 | 7 |
| Factor | 27,295 / 335 | 122 / 13 | 0 |
| HyperBinaryRes | 27,130 / 10,885 | 105 / 22 | 17 |
| HyperTernaryRes | 27,364 / 19,290 | 113 / 72 | 22 |
| Instantiate | 27,055 / 2,304 | 120 / 7 | 1 |
| LocalSearchGuidance | 27,164 / 266 | 429 / 1 | 40 (49 hit cases; 40 graded, cap) |
| Lucky | 33,738 / 12,457 | 485 / 2 | 0 |
| Probe | 27,264 / 1,947 | 117 / 30 | 4 |
| Propagate | 139 / 131 | 0 / 0 | **2** (both hit cases graded with negatives) |
| Reduce | 27,269 / 5,222 | 111 / 29 | 9 |
| Reorder | 27,336 / 27,336 | 118 / 118 | 25 |
| Rephase | 27,280 / 27,280 | 125 / 125 | 22 |
| Strengthen | 27,168 / 15,796 | 119 / 60 | 3 |
| Substitute | 27,170 / 14,219 | 115 / 17 | 2 |
| SubstitutionRedundancy | 27,295 / 0 (native SR never effective) | 169 / 55 (supplied orbitopal/graph-symmetry payloads, 12 inputs) | 0 |
| SubsumeForward | 27,287 / 4,796 | 120 / 18 | 1 |
| Sweep | 28,039 / 9,020 | 115 / 41 | 7 |
| TransitiveReduce | 27,228 / 2,922 | 114 / 5 | 9 |
| Vivify | 27,516 / 12,844 | 120 / 55 | 6 |
| Walk | 27,443 / 26,081 | 132 / 109 | 24 |

Dispositions:
- **Condition:** effective only with an explicitly offered valid witness. No
  producer is available in random selection.
- **SubstitutionRedundancy:** effective only with supplied producer payloads,
  emitted by the pin's own producers. Of 80 inputs, 12 had payloads.
- **Propagate and LocalSearchGuidance:** record-level effects come from L2
  replays (mining). These are coverage, not fresh credit (A4).

### R3 detail: configurations by name

| Configuration | L2 intervals / decisions / conflicts (sums) | Targeted random intervals / with decisions / with conflicts |
|---|---:|---:|
| evsids/adaptive-glue/focused-rotation | 2,251 / 346,296 / 275,483 | 4,612 / 4,612 / 4,584 |
| evsids/adaptive-glue/negative | 2,266 / 388,237 / 316,281 | 3,164 / 3,164 / 3,137 |
| evsids/adaptive-glue/positive | 2,358 / 415,180 / 339,118 | 3,798 / 3,798 / 3,795 |
| evsids/adaptive-glue/saved | 2,313 / 412,356 / 332,271 | 3,622 / 3,622 / 3,589 |
| evsids/adaptive-glue/target | 2,377 / 334,849 / 261,995 | 4,498 / 4,498 / 4,467 |
| evsids/reluctant/focused-rotation | 2,362 / 602,535 / 506,479 | 5,079 / 5,079 / 4,986 |
| evsids/reluctant/negative | 2,406 / 683,459 / 585,261 | 5,721 / 5,721 / 5,676 |
| evsids/reluctant/positive | 2,336 / 665,594 / 570,128 | 6,463 / 6,463 / 6,460 |
| evsids/reluctant/saved | 2,325 / 542,436 / 454,258 | 3,391 / 3,391 / 3,386 |
| evsids/reluctant/target | 2,356 / 578,008 / 484,347 | 6,187 / 6,187 / 6,143 |
| vmtf/adaptive-glue/focused-rotation | 2,441 / 303,853 / 232,102 | 780 / 780 / 758 |
| vmtf/adaptive-glue/negative | 2,289 / 298,400 / 229,297 | 481 / 481 / 473 |
| vmtf/adaptive-glue/positive | 2,289 / 310,974 / 241,476 | 393 / 393 / 366 |
| vmtf/adaptive-glue/saved | 2,368 / 320,595 / 247,918 | 931 / 931 / 913 |
| vmtf/adaptive-glue/target | 2,286 / 261,847 / 193,264 | 809 / 809 / 749 |
| vmtf/reluctant/focused-rotation | 2,328 / 435,612 / 354,449 | 1,029 / 1,029 / 938 |
| vmtf/reluctant/negative | 2,417 / 480,190 / 389,311 | 462 / 462 / 434 |
| vmtf/reluctant/positive | 2,314 / 450,213 / 365,539 | 3,293 / 3,293 / 3,262 |
| vmtf/reluctant/saved | 2,362 / 422,605 / 341,113 | 971 / 971 / 944 |
| vmtf/reluctant/target | 2,342 / 433,103 / 343,420 | 1,147 / 1,147 / 1,114 |

DefaultPolicy is not forced into random configurations. Its own native
configurations are listed for information only: vmtf/adaptive-glue/focused-rotation
had 5,766 intervals and evsids/reluctant/target had 233.

### R13 detail: known-bad controls (both policies)

| Control | DefaultPolicy | Random | Inapplicable |
|---|---|---|---|
| lie-UNSAT-as-SAT | 26 / 26 UNSAT inputs flagged | 25 / 25 flagged | SAT inputs pass through unchanged (52 / 51) |
| negate-model | 52 / 52 SAT flagged | 51 / 51 flagged | UNSAT inputs (26 / 25) |
| empty proof | 7 / 7 applicable flagged | 6 / 6 flagged | 19 per policy are root-UP-refutable; SAT inputs pass through |
| ASCII truncation | 4 flagged | 6 flagged | 19 per policy are UP-refutable. DefaultPolicy also has 3 whose kept half-prefix the checkers verified as a genuine refutation (gen 2042000023 / 2042000059 / 2042000073); these are inapplicable by the wrapper's definition and are not passes. |

Control timeouts: 8 DefaultPolicy and 16 Random.

The terminal-watch causal controls are listed in the next section.

## Terminal-watch causal coverage (repair revalidation)

**Retained corpus 009** (`tools/fuzz/corpus/regressions/009-cong-and-idempotent-unary/min.cnf`,
sha 00eba0e110c85125d4f3818ea9fb08239d3db8a2a4c5ffbe6e311b92556786e5), with
the fixed controller seed 2032000128. This is regression coverage, not fresh
credit.
- **Driver purity** (`td-purity-random-tw009-{rp,full}`), ranked-prefix and
  full retention:
  - both pass, with the certificate verified by drat-trim + CakeLPR + the
    forward audit;
  - `c fbd-terminal Substitute 1`;
  - `terminal-disconnected=0 session-end-checked=1 session-end-connected=1`;
  - A, B and C are equal: proof bytes, model, counters, transcript, coverage
    and record streams.
- **CLI** (`tf-random-tw009-{rp,full}`):
  - the root sequence is BVE → LocalSearchGuidance → Substitute, and the
    session ends at verdict 20 with reason Unsat;
  - the feedback-on proof, lines and transcript are equal to the plain run;
  - feedback and controller replay match;
  - all 11 negative classes reject for the intended reason.
- **Causal control** (`twctl-purity009-{rp,full}`): the same driver linked to
  the predecessor pin **detects** `receiver callback at unrestored scope
  (large_watches_disconnected) at Substitute` and at `session_end`.

**Public execution, real disconnection then rewrite refutation**
(`td-tw-b{0,1}o{0,1}`). This uses 5 seedless fixtures `tw-eqcycle-*`: an
equivalence cycle plus two large clauses with a root-false literal. n=2 is the
repository regression formula.

| Combination | Rows | Reach (UNSAT after witnessed disconnection) | Pass (graded) | Watches after the call |
|---|---:|---:|---:|---|
| canonical single round, owned-connected entry | 5 | 5 | 5 | connected (restored) |
| canonical single round, borrowed-disconnected entry | 5 | 5 | 5 | disconnected (caller ownership kept; caller reconnects) |
| batch, owned-connected entry | 5 | 5 | 5 | connected; `reconnected=1` |
| batch, borrowed-disconnected entry | 5 | 5 | 5 | disconnected; `reconnected=0` |

How disconnection is witnessed:
- **Batch:** the publisher observes the disconnected state before the
  refutation.
- **Single round:** a twin, non-reconnecting round from the same parsed state
  ends UNSAT and disconnected.

Causal controls on the predecessor pin:
- **Detected** for owned single and owned batch ("owned Substitute watch
  session not restored", plus the batch flag mismatch).
- **Passed** for borrowed single and borrowed batch, which match both pins.

Other controller and native evidence:
- **Controller canonical round with the Substitute collection** on the tw
  fixtures, both retention settings: 10 / 10 pass with 0 violations. Full
  retention refutes at Substitute; ranked-prefix refutes at the root
  Propagate service.
- **Watch-owning families, boundary-checked at every refuting and interrupted
  record** (purity and interrupt rows). There were 0 disconnected callbacks in
  either policy.

  | Family | Random refuting records | Random interrupted records | DefaultPolicy refuting records |
  |---|---:|---:|---:|
  | Vivify | 16 | 35 | 1 |
  | Substitute | 20 | 33 | 3 (native batch publication) |
  | Factor | 0 | 40 | 0 |

  DefaultPolicy also had 43 refuting records at the native deferred phase exit
  (`InprocessPhaseExit`) and 2,407 / 2,407 session ends connected.
- **Factor terminal arm: unreached.** This is dispositioned, not fabricated.
  The parent's read-only review (`terminal-watch-source-review.json`) finds
  that Factor's two UNSAT returns precede watch disconnection, and that the
  canonical pass rebuilds sparse watches before its final result.
  - Factor was interrupted 40 times with restored scopes.
  - No invalid kernel state was manufactured to force the arm.
- **Vivify's early `sched.refuted` arm** was exercised in 16 random and 1
  native refutation, all with connected watches at the callback. I have no
  evidence of a second defect.

## Limitations and disclosures

**Predecessor disclosures, preserved:**
- **A1 (predecessor):** smoke-derived harness fixes.
- **A2 — DefaultPolicy native behavior.**
  - Entry root BCP runs before the first stop check, so the entry-stop check
    allows Propagate progress only.
  - Native Reorder, Propagate and Reduce are level-independent.
  - No row needed Reorder reclassification this time.
- **A3:** only root-fixed or retired variables may leave an inactive ranking.
- **A4:** LocalSearchGuidance and Propagate record-level effects come from
  mining replays of L2 seeds. They are coverage, never fresh credit.

**This campaign:**
- **Amendment A1 (this campaign).** After the smoke run, the new `ec-*` and
  `cc-*` census tasks were found to pass a not-yet-created work directory to
  `run_case`.
  - All 26 such smoke rows returned UNKNOWN; they are void.
  - The fix creates the directory, as `graded()` does. It went into snapshot
    f1 before the full job, with no acceptance-logic change (COVERAGE_PLAN.md,
    plan-freeze chain `status/plan-freeze.sha256`).
- **Pre-registered reclassification of 4 CLI stop-fuzz rows** (DefaultPolicy,
  gen 2042000025 and 2042000015 × n50/n500). In f1 the feedback-off run
  completed and graded UNSAT, and the feedback-on run timed out at 120 s. The
  inherited rule classes these as feedback-variant timeouts: non-evidence,
  never passes. Labelled diagnoses confirm the cause:
  - **d1 (1747211):** gen-25 n50/n500 feedback-on completes byte-identical to
    feedback-off. The gen-15 runs still time out at 1,800 s.
  - **d2 (1747244)**, comparing the feedback-on stream's deterministic native
    counters with the off run's final counters, then proof bytes and graded
    results:
    - gen-25 n50: off 4.1 s, on 279.9 s (645,772 records). Counters equal at
      67,705 conflicts / 86,891 decisions / 203 fires. Proof identical; graded
      UNSAT.
    - gen-15 n50: off 37.0 s, on 3,254.9 s (3,483,684 records, ≈50 GB
      stream). Counters equal at 391,010 / 463,807 / 288. Proof identical;
      graded UNSAT. No divergence.
    - gen-15 n500: off 33.5 s, on 2,855.1 s (3,235,282 records). Counters
      equal at 362,506 / 430,931 / 60. Proof identical; graded UNSAT.
  - Conclusion: recording overhead, not extra interruption-RNG polls.
- **Feedback overhead on the Debug CLI is very large.** The two d2 cases
  ran about 68×, 88× and 85× slower with feedback on. This is a Debug-build
  observation only; overhead measurement is parent-owned, and this is not a
  performance claim.
- **Stop-fuzz under the controller** reports BudgetExhausted inside rules and
  never ends a controller solve. Random rule-work interruption therefore comes
  from real external stop flags: selection triggers and timers.
- **Test-only stop sources.** The record and selection triggers set the
  external stop flag from the harness. Timer landings are not
  bit-reproducible. There are no extra observation or receiver polls that
  consume interruption RNG.
- **Native SR in L2** was never effective: 0 of 27,295 cases. SR effects come
  from supplied payloads.
- **Fixed-prefix accounting.** Leg indices 0–9 are seed-independent formulas.
  Their per-index verdicts come from the owner `ec-*` reruns of the identical
  command and seed (20 / 20 graded, matching), together with the leg's
  per-index timeout list (no fixed-prefix timeouts). Fresh totals are leg
  totals minus those.
- **Timed-out driver rows** (157) captured partial output only and are
  non-evidence. For example, 61 interrupt rows stopped but have no stop-set
  check line.
- **The f1 pool** reuses the predecessor's fixture CNFs as fixed files. The
  generator-pool seeds are fresh (2042000010+).
- **Untracked artifacts:**
  - `build/pin-fuzz-checkers-shared-feedback-375b621b/`;
  - `tools/fuzz/slurm-spf-375b621b-*.log`;
  - the shared content-addressed `tools/fuzz/failures/checkers/895f1989…`,
    which pre-existed.

## Evidence locations (this directory)

- `COVERAGE_PLAN.md`: frozen plan with Amendments F and A1, hash chain in
  `status/plan-freeze.sha256`.
- `OWNER_STATUS.md`: timeline.
- `status/`: censuses, freshness scans, pin verification pre/post, checkers,
  controller config, progcheck, analyzer and parser controls, accounting,
  targeted census, L2 coverage summary.
- `legs/L1-default`, `legs/L2-controller`: `nonterminating.json`, samples,
  `controller-coverage.json`.
- `targeted/out-f1`, `targeted/out-mine`: `rows.jsonl`, `matrix.json`,
  `analyze.out`, samples.
- `targeted/out-smoke`: labelled smoke, not counted.
- `diag/d1`, `diag/d2`: labelled diagnoses.
- `tools/`: owner tools and the causal-control driver build.
