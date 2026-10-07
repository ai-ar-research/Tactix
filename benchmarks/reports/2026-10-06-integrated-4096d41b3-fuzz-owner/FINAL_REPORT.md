# FINAL REPORT — integrated cumulative fuzz, candidate 4096d41b3

Owner: Claude cumulative fuzz owner. Terminal: 2026-10-06/07.
Request: `benchmarks/reports/2026-10-06-integrated-4096d41b3-fuzz-owner/request.txt`.

## Disposition: **FAILED — NOT ACCEPTED**

One deterministic source failure was found in the cumulative candidate's
public theory controller surface. A random-controller **native X18
`NaryCspEquality` offer** dispatched after search changed the root formula
returns `InvalidRequest` (status 9). The controller treats this as terminal:
`c ERROR: controller execution status=9 action=NaryCspEquality` and `s UNKNOWN`.
The failure is independent of the X20 producer. DefaultPolicy answers SAT on
the same CNF. Bounded handoff: [`handoff/HANDOFF.md`](FAILURE_HANDOFF.md) (campaign copy: `benchmarks/runs/20261006-integrated-4096d41b3-fuzz-owner/handoff/`). It
gives a 34-clause reproducer, the exact command, both pin bindings and the
transcript/feedback evidence. No model, proof, differential, sanitizer or
assertion oracle rejected anything.

Every other declared requirement is met. That includes both broad policy legs
and all coverage owned by the three changed surfaces: predecessor restore,
X20 status/retry, and the affine A/B/C repair. Acceptance is blocked only by the
handoff. After a repair, rerun the `x20-*`/`drv` targeted kinds and the three
reproducers. Rerun L2 as well if controller settlement changes. The earlier
95b0fad13 owner verdict is not cumulative acceptance.

## Bindings

| Item | Value |
|---|---|
| Candidate | `4096d41b35362e244e165ec1ac704728fdb55f4e`; `git archive` sha256 `2e4f9859694517e86a8def2173d8c0f961f9ecb9cb68a73a8bb44d0a27f789b8` → read-only `src-4096d41b3/` |
| ASan pin (legs, targeted, witness) | `build/pin-integrated-4096d41b3-asan` Debug+asserts+PROOF_SHADOW+ASan/UBSan, `tactix` only; PIN.json `e094bd3d…ef90c`; tactix `626bda31721dd6805ce26916b90ef02e02428b6993fd9a5fe9759f3853885dfe` |
| Release pin (census, repro) | `build/pin-integrated-4096d41b3-rel`; PIN.json `55510018…c330`; tactix `506ada0fa32805bf2ad022be1d8c9dc97286b8a183d133982a91f757f084c367` |
| Matrix fixtures | ASan pin `fixtures/matrix_diff` `feea40f3…6b326` (owner `tools/matrix_diff.cpp`), candidate `test_affine_matrix` `e6962a01…31ae6` |
| X20 driver | `build/pin-integrated-4096d41b3-fixtures-v3/x20drv` `8c7480c5d3544b20649c294535147a526987cdb7d5b255d4eff9e8b5948ff70a` (`tools/x20drv.cpp` `181cfedc…d8f0`, linked from the ASan pin's objects). v1/v2 were calibration-only (A5) |
| Core checkers | `build/pin-fuzz-checkers-r263aff-3676cff1c` SHA256SUMS `895f1989…44f8` (cake_lpr, drat-trim, dsr-compress, dsr-trim 3b5c, lrat-check diagnostic, srcheck/Trestle). Verified pre-launch and in every job |
| Broad harness | `tools/fuzz` snapshot `28ae90a22ecaa83134df39e5b695ae43d1b188f1f5bca45ef3500f6c13012eec` (identical to candidate) |
| Targeted snapshots | full `f1` `ccccf177…0b68`; continuation `g2` `b9e10164…2e01`; calibrations `c1` `25ad217b…4894`, `c2` `3b0f97a8…84d0` |
| Storage guard | 220,344,085,504 B < 700 GB before building |

Every pin was re-verified after each job (`verify-pin-post.json`: 0 differences).

## Jobs (cpu-dense-hwulab-q; full width, no throttle, no node constraint)

| Job | Role | Result |
|---|---|---|
| 1751462 | pin build + matrix fixtures | COMPLETED |
| 1751467 | **L1 DefaultPolicy** 100 CPU/400000M/50 workers | exit 1 = corpus-026 budget only → **PASS** (A2/A3) |
| 1751468 | **L2 random controller** same resources | COMPLETED → **PASS** |
| 1751469 / 1751470 | Release reach census L1 / L2 | COMPLETED (reach only) |
| 1751471 | matrix differential, 32,000 seeds | COMPLETED, 0 failures |
| 1751474 | graded rerun corpus 026 at 3,000 s | UNSAT, verified |
| 1751466, 1751473 | targeted calibrations (labelled, uncounted); 1751472 cancelled while pending (A1) | — |
| **1751475** | **full targeted matrix**, 13,155 tasks | COMPLETED 3:55:40 |
| **1751480** | P1 witness continuation, 600 tasks (A6) | COMPLETED |
| 1751478 / 1751479 / min job | reproducer confirmation, ddmin, minimal evidence | COMPLETED |

Amendments A1–A6 are in `status/amendments.md`. None changes a leg's seeds,
oracles or acceptance criteria.

## Broad legs (fresh seeds + 52-entry retained corpus)

| Leg | Fresh seeds | SAT model-checked | UNSAT proof-checked | Timeout 5 s (no oracle) | UNKNOWN | Failures |
|---|---|---:|---:|---:|---:|---:|
| L1 DefaultPolicy | 2099000010–2099100009 | 56,844 | 39,417 | 3,749 | 0 | 0 |
| L2 random controller `--controller-policy={seed}` | 2099500010–2099600009 | 55,679 | 37,110 | 7,221 | 0 | 0 |

- Every completed SAT result was checked against the original CNF. Every
  completed UNSAT result went through its native route (witness-free DSR →
  drat-trim + CakeLPR + forward audit; witnessed DSR → dsr-trim → Trestle +
  forward audit; startup PBP → VeriPB → CakePB), plus a kissat differential.
- L1 corpus: 51 graded entries, 0 failures. Entries 006 and 022 are
  crash-oracle-only passes. `026-sweep-drat-admission-gate` exceeded the 60 s
  ASan budget; its graded rerun at 3,000 s (1751474) was UNSAT and verified,
  66 s wall. This is ASan budget censoring, not a solver failure.
- L2 corpus: 51 entries, 0 failures, including entry 038, which ended UNKNOWN
  on 3676cff1c. All 7,221 non-terminating cases have verdict TIMEOUT; there is
  no UNKNOWN. The r263 campaign had 3,011 class-C UNKNOWN.
- L2 loop: `AffineMatrixInitialize` was requested in 26,925 cases and effective
  in 11,904. There were 1,195 full-restart and 741 partial-restart cases, and
  914 configuration-change cases.
- Census (reach only). L1: affine init was attempted in 74,859 cases with 0
  admitted (32,044 IncompleteImage) and 0 services. Random CNFs give zero
  DefaultPolicy affine reach. L2: 48 Applied, 1,980 Stale and 1,017
  Inapplicable services, all non-terminal. X20 reach is 0 in both legs:
  native producers are off by default, and DefaultPolicy has X20 disabled.
  That disabled X20 is a compatibility control, not X20 coverage.

## Targeted matrix (1751475 + 1751480): outcome classes

| Class | Rows | Meaning |
|---|---:|---|
| PASS (graded) | 11,937 + 600 | every completed run graded; replay/purity identical |
| SOURCE FAIL | 3 | `x20-random` X18 InvalidRequest → handoff |
| HARNESS FAIL | 12 | `drv-missing` on two-owner **batch** seeds: my driver expected a retry after `MissingEquality` on batch member A, but the batch correctly advances to B. Single-owner missing is covered (66 PASS) |
| TIMEOUT (120 s, no oracle) | 118 | dpg 26, pc 18, rp 53, rpp 17, rps 4 |
| Deliberate stop (UNKNOWN) | 54 | `rps` with `TACTIX_STOP_FUZZ=40` |
| Status controls (UNKNOWN by design) | 163 | x20drv `stale` 95 (forced arena premise loss) and `unsupported` 68 (LRAT): all status checks passed; no proof/model is produced |
| SKIP (unreached) | 162 | x20drv fixtures outside X17/equality admission (e.g. domain size 2) |
| PREFIX-UNAVAILABLE (unreached) | 706 | required native startup step not offered by the generated formula; the controller stops with `c ERROR` by design. Never counted as pass |

Graded runs: 26,650 + 1,292 SAT model checks and 16,203 + 1,108 UNSAT proof
checks, with 0 model rejections and 0 proof rejections. Known-bad controls were
all rejected: falsified model 387, tampered transcript 359, tampered feedback
576 (rejected at the exact line), tampered affine event 310, truncated proof 63.
No truncation control was vacuous.

## Coverage required vs achieved (`COVERAGE_PLAN.md`)

| Id | Required | Achieved | Status |
|---|---|---|---|
| L1 / L2 | 100,000 fresh each, 0 failures | as above | MET |
| M1 | ≥1M calls, ≥50k restores, ≥1k each class, 0 failures | 1,139,024 calls; 134,289 restores (all equal to the predecessor copy's exact rows/sources/event, charged 1+|trail|); cap declines 80,854; deeper backtrack after restore 24,931; reassignment 79,194; prior partial 11,220; interrupted 51,035; exhausted 29,918; nonmatching same-length candidates 14,576 never restored | MET |
| P1 | ≥50 checked runs with a witnessed CDCL restore, incl. restart+retention | 69 runs / 100 restores (21 + 48; 57 under restart period 1 + full trail retention); 35 SAT, 34 UNSAT checked | MET (A6) |
| P2 | rebuild/extension/backtrack/reassignment/partial counters equal the witness | 840 witnessed runs, 0 mismatches; restores never counted as rebuilds | MET |
| P3 | ≥100 SAT, ≥50 UNSAT after init + service; interruption | pr: 479 SAT / 240 UNSAT checked; stop 40/400: 360 PASS | MET |
| A1 | root unit published, both policies | DefaultPolicy 315 (SAT); controller 387 | MET |
| A2 | empty-reason root conflict → UNSAT, both policies | DefaultPolicy 318; controller 139 | MET |
| A3 | Stale and Inapplicable continue to a checked verdict | controller Stale 80, Inapplicable 53; DefaultPolicy Inapplicable 31; reproducers A/B/C1/C2 360/360 PASS | MET |
| A4 | no-mutation guards; LRAT never applies affine | 0 violations; service `Invalid` 0 | MET |
| X1 | X20 valid application with checked SAT/UNSAT, bin + ASCII | prefix/all/random/rnat/stale: 736 checked rows with an X20 application (bin and ASCII, SAT and UNSAT) | MET |
| X2 | public statuses exact | ResourceExhausted (proof 170, recognition 131), Interrupted 182, StaleRequest 95 driver + 134 natural CLI, InvalidRequest 123, Inapplicable (LRAT) 68 | MET |
| X3 | predispatch zero attempts | InvalidRequest and LRAT: applications = attempts = 0 in all 191 | MET |
| X4 | retention + fresh-allowance retry, then checked result | 483 retries; 75 two-owner selections committed the retained first owner, then the second | MET |
| X5 | duplicate / missing definitions | Duplicate 90 (NoChange, no mutation); missing 66 (NoChange, then completion applies) | MET |
| X6 | native random collection reach | random-selected X20 applications checked: x20-random 21, random-all 11, rnat 34 | MET |
| R1 | purity / seeded replay / tamper | all graded rows identical (plain, replay, seed2, transcript determinism); every tamper rejected | MET |
| D0 | DefaultPolicy X20 attempts = 0 | 0 (dpg 400, L1) | MET (control only) |

## Scope limitations

- PriorPartial rebuild is not reached through the CLI (0 witnessed). It is
  covered by the matrix differential only (11,220 calls).
- In 76 of 1.14M matrix calls the event kind differed from a fresh reduction.
  The bases are equivalent and every reason is brute-force entailed and
  authenticated, so this is representation-dependent event choice that
  predates this change. It is recorded, not failed.
- CLI X20 typed payloads, missing definitions, interruption and LRAT startup
  programs are inexpressible while native producers are on. Calibration c1
  showed that a typed startup step requires a static offer. Those statuses are
  covered through the public `RestartController` path by `x20drv`, with
  DefaultPolicy completion graded by the same oracles.
- The driver's stale cause uses the candidate test's forced arena premise loss
  and produces no proof/model. Natural staleness is covered by the graded CLI
  `x20-stale` rows.
- The broad legs give zero X17/X18/X20 reach and zero DefaultPolicy affine
  reach. Targeted rows own that coverage.
- `slurm.sh` created harness snapshots outside this campaign's pin prefix, as
  the standard runner does. `build/pin-fuzz-harness-weP51R` is retained after
  L1's nonzero exit.
- The concurrent 95b0fad13 campaign was neither inspected nor reinterpreted.

## Artifacts

`status/` (accounting JSON, amendments, pin verification, census jobs),
`targeted/out-full`, `targeted/out-gdb2` (rows.jsonl, samples, failures),
`matrix/`, `census/`, `legs/`, `logs/`, `diag/c026/`, `handoff/` (SHA256SUMS).
Nothing was committed or pushed. Solver source, tests, docs, PROGRESS and the
tracked corpus are unchanged.
