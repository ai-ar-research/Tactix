# DefaultPolicy public-API gap: Step 4b (public inprocessing phase bracket)

Step 4b of the [gap plan](../2026-10-09-defaultpolicy-api-gap-plan/README.md) (A11), on top of
[Step 4a](../2026-10-09-defaultpolicy-api-gap-step4/README.md). The baseline is c2.4 = `3361d3b51`
(Release `57bdd51c…`, `build/pin-restart-c4-rel`); main's source at `fc128fae5` equals it.

| | Commit | Release (identity pin) |
|---|---|---|
| Step 4b | Step 4b commit (validated at `1caa7af83`, identical code/test/doc tree) | `21f203b6…` (`build/pin-apigap-s4b-rel`, deleted after copying; campaign copy `pins/tactix-new`) |

The validated tree differs from the Step 4b commit only in PROGRESS and this report.

## What changed

- **Shared bracket `execution::InprocessPhase`** (`src/strategy/phase_execution.hpp`). Entry enters
  the inprocessing context, records the sparse-resume cursor, disconnects large watches for
  Eliminate and installs the Factor selection with its bound. `open_occurrences` builds the phase's
  occurrence index (ordered for Eliminate). Exit flushes retired redundant clauses outside
  Eliminate unless refuted, publishes retirements through the caller's publisher, closes the session,
  reconnects watches (Eliminate, or when a rule left them disconnected), resumes sparse Propagate
  after Eliminate, clears the selection and leaves the context. `exit_terminal` is the
  structured-startup SAT exit (no session). DefaultPolicy's `run_phase` now runs exactly these
  transitions in the same order; the context is left at the end of the exit transaction instead of at
  function return, and nothing between the two reads or writes the context fields.
- **DefaultPolicy records** (feedback only): the phase-entry record carries the public argument as
  its instance (Eliminate flag | default Factor bound << 32); the occurrence session, which was built
  after the entry record without a record of its own, is now an `OpenOccurrenceSession` support
  record. An entry with an injected `FactorMatchPolicy` (diagnostic arm) keeps a canonical instance.
  `DefaultFactorMatch::set_bound` now sets its bound, so the default selection receives its bound
  through the shared request (DefaultPolicy wrote the field directly before).
- **Public form.** Additive `tactix_support` ids `ENTER_INPROCESS_PHASE` (18; argument
  `TACTIX_PHASE_ELIMINATE | bound << TACTIX_PHASE_FACTOR_BOUND_SHIFT`), `OPEN_OCCURRENCE_SESSION`
  (19) and `LEAVE_INPROCESS_PHASE` (20). The session holds one bracket with its own default selection
  and occurrence index; contexts nest strictly inside it; root-only supports decline while its
  session is open; an Eliminate bracket cannot close without its session; `tactix_solve` leaves an
  open bracket first. Retirements and sparse-resume roots reach H through the session's guidance
  publication profile. Contract: [controller interface](../../../docs/controller-interface.md);
  composition: [calculus §2](../../../docs/calculus.md).
- **Replay harness.** Records of a native composite are delivered when the phase finishes, so the
  reference H captured at delivery is current only if the state has not moved since the record.
  The replay now compares H only where the capture is current (state version and observation
  unchanged) and reports the others as deferred.

## Ratchet (`tests/test_default_action_map.cpp`)

| | Public entries | Internal entries | Declared | Public records | Internal records | Declared records |
|---|---|---|---|---|---|---|
| c2.4 (Step 4a) | 34 | 14 | 2 | 1,638 | 5,341 | 9 |
| Step 4b | 37 | 14 | 2 | 1,668 | 5,321 | 9 |

`support-phase-bracket` (20 records) is replaced by public `support-phase-entry` (10),
`support-occurrence-session` (10, new records) and `support-phase-exit` (10). The internal entry
`support-phase-entry-injected-factor` (0 records; the injected-matcher diagnostic arm, step 5's
Factor selection profile) takes its place in the list. Every other entry's count is unchanged.

Remaining internal entries (records on the fixtures):

| Entry | Records | Route | Plan |
|---|---|---|---|
| propagate-search | 2,518 | S | A1 |
| decide | 1,400 | S | A2 |
| conflict-analysis | 962 | K | A3, C1, C4 |
| propagate-root | 63 | S | A1 |
| slot-fastel-occurrence | 374 | K | A10 |
| slot-sweep-batch | 2 | S | A10 |
| slot-substitute-rounds | 2 | K | A10 |
| slot-vivify-band | 0 | K | A10 |
| support-forward-closure | 0 | K | A10 |
| support-phase-entry-injected-factor | 0 | K | A11, C4 |
| affine-matrix-initialize | 0 | K | A8 |
| affine-matrix-search | 0 | K | A8 |
| install-affine-matrix-parameters | 0 | K | A8 |
| support-warmup-installed-branching | 0 | K | A11 |

## Replay (`tests/test_default_public_replay.cpp`)

| | Records | Replayed | Supports | Lookahead replayed | Lookahead assumed |
|---|---|---|---|---|---|
| c2.4 (Step 4a) | 6,349 | 1,049 | 74 | 1,134 | 13 |
| Step 4b | 6,357 | 1,154 | 106 | 1,154 | 0 |

| Fixture class | Step 4a strict stop | Step 4b strict stop | H at stop (discard / native) |
|---|---|---|---|
| search profiles 1–5, pigeons 6 | #66 `InprocessPhaseEntry` | **#84 first `ConflictAnalysis`** (Step 6) | equal / equal |
| `graph-labeling-permutation` | #53 `InprocessPhaseEntry` | **end of stream** (70 of 72 replayed; the other two are markers) | equal / equal |
| `sweep-retarget-lrat` | #81 `InprocessPhaseEntry` | #83 first slot `Congruence` (Step 5) | deferred (inside the phase) |
| default profile 0, pigeons 4 profiles, `sweep_interrupted_direction` | end of stream | end of stream | equal / equal |

The strict replay now equals the lookahead: no support record without a public request occurs on
the fixtures. H equals the reference at 14 of 17 stop points in both publication modes; one stop
(`sweep-retarget-lrat`) lies inside a phase whose reference capture is not current, and two streams
are empty. H after all 198 structured-root online steps is still equal in both modes.

## Tests

- `tests/test_api_support.cpp` `test_phase_bracket`: a public session and DefaultPolicy's shared
  bracket (default selection, H retirement publication, native root Propagate) stay identical in
  trail, clause census, contexts, saved/target phases and candidates through an Eliminate bracket
  with a BVE application and a Probe bracket; watch disconnection, the Factor bound, the occurrence
  session and the restored state after exit are checked; rejections (second entry, Eliminate exit
  without a session, invalid flags, root-only supports during the session, mis-nested contexts);
  the session-free Probe exit; `tactix_solve` leaving an open bracket.
- `test_native_policy_feedback`: a phase records one entry, one occurrence session and one exit.
- `test_default_action_map`, `test_default_public_replay`: above.
- Against c2.4 (scratch Release build of `fc128fae5`'s source, `build/scratch-s4b-base`, since deleted): retained
  DRAT, models and CNFs of `test_default_execution_signature`, `test_startup_support_signature`
  and `test_inprocess_execution_signature` identical; their 42 feedback streams identical in every
  action's kind, identity, native description and outcome once the 28 added `OpenOccurrenceSession`
  records are dropped and the entry instance is ignored. `test_controller_execution_signature`
  (72 retained files) identical apart from `elapsed-nanos`; `test_random_controller_policy` stdout
  identical. `test_preparation_execution`, `test_api_c`, `test_scheduling`,
  `test_inspect_policy_feedback.py` pass.

## Byte identity and full Debug

Both campaigns use a `git archive` of the validation commit `1caa7af83`
(`benchmarks/runs/20261010-apigap-s4b-source/pins/source-pin.json`, archive `117844c8…`).

**Full Debug.** `tools/bench/full_debug_archive_campaign.py` with that source pin, as
`benchmarks/runs/20261010-apigap-s4b-full-debug`, job 1763856: **358/358 passed, 0 skipped**; the harness verifies the archive after the run
(358 tests, 288 executables, 772 bindings). The Debug build tree was deleted afterwards.

**DefaultPolicy.** The c2.4 three-arm campaign cloned as
`benchmarks/runs/20261010-apigap-s4b-identity`, job 1763729 (`cpu-dense-hwulab-q`, 8 CPUs,
32 GiB, 600 s solve limit, `runtime_task.py`; c2.4 `--auto-proof` (`pins/tactix-c24`, `57bdd51c…`),
Step 4b `--auto-proof`, Step 4b `--auto-proof --default-affine-search=on`, sequential in one
allocation; Release `21f203b6…` built from the archive by job 1763701 into
`build/pin-apigap-s4b-rel`, since deleted). All 12 tasks completed with exit 0; all 36 runs are valid with the
expected verdict, and all 12 rows are identical in verdict, conflicts, decisions and certificate
SHA-256. Certificates were deleted after checking.

| Row | Verdict | Conflicts | Decisions | Certificate | Wall (s) |
|---|---|---|---|---|---|
| ramsey_4_4_19 | UNSAT | 0 | 0 | `d169d9686c94` | 0.1 / 0.1 / 0.1 |
| lightsout_24 | UNSAT | 1 | 0 | `6fbc0514e27a` | 0.6 / 0.6 / 0.6 |
| mp1-Nb7T42 | SAT | 638,214 | 15,456,796 | `3b6b38a3bfa0` | 57.8 / 57.8 / 58.1 |
| battleship-14-26 | SAT | 665,487 | 1,681,225 | `fbedb083879e` | 14.9 / 14.9 / 15.0 |
| oddball_53_5_tto | SAT | 0 | 0 | `ba97acaa4dc6` | 5.7 / 5.7 / 5.7 |
| abw-K-dwt__234 | SAT | 0 | 0 | `1026936d5617` | 22.3 / 21.0 / 21.2 |
| cliquecolouring_n13_k8_c7 | UNSAT | 1 | 0 | `ed71148e8802` | 0.1 / 0.1 / 0.1 |
| 9vliw_bp_mc | UNSAT | 198,066 | 2,279,994 | `438781f64b25` | 10.7 / 10.7 / 10.7 |
| combined-crypto1 | SAT | 75,764 | 172,719 | `134984e88e13` | 2.2 / 2.2 / 2.2 |
| sted5_0x24204-50 | SAT | 2,333,179 | 5,655,352 | `83c48a7431f7` | 128.1 / 128.4 / 126.8 |
| noL-11-16 | SAT | 1,180,890 | 3,131,471 | `190aa7fb722c` | 31.0 / 31.1 / 31.1 |
| b18 | UNSAT | 3,331,178 | 49,260,596 | `df61ef101a9d` | 221.2 / 216.1 / 216.5 |

Every value equals the c2.4 identity campaign (`20261009-restart-rework-c4-identity`); wall times
are c2.4 / Step 4b / Step 4b with affine search. abw-K-dwt is solved by graph labeling at c2.4
(0 conflicts). Wall times are within ±1.3% except b18 and abw-K-dwt, where the c2.4 arm is
2.4% and 6% slower than both Step 4b arms (identical work in all three arms).

**Controller.** See Tests: identical to c2.4 apart from elapsed nanoseconds.

**Doc checks.** All `tools/docs/check-*.py` pass.

## What blocks next

- **Step 6** (`ConflictAnalysis`, 851 blocking records) is now the strict stop of every search
  profile.
- **Step 5**: registered slot rules under scoped slot parameters (`slot-registered` is not yet
  replayed; first stop of `sweep-retarget-lrat`), the Sweep/Substitute/Vivify/fastel slots, the
  forward closure and the Factor selection profile that retires the injected-matcher entry.
