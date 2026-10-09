# DefaultPolicy public-API gap: Step 4a (support vocabulary, structured-root replay)

Step 4 of the [gap plan](../2026-10-09-defaultpolicy-api-gap-plan/README.md) (A5, A11), on top
of [Steps 0–1](../2026-10-09-defaultpolicy-api-gap-steps-0-1/README.md),
[Step 2](../2026-10-09-defaultpolicy-api-gap-step2/README.md) and
[Step 3](../2026-10-09-defaultpolicy-api-gap-step3/README.md). The baseline is c2.3 = `555ecb4b0`
(Release `ae4a26e4…`, `build/pin-candidate-c2.3-rel`). Step 4 is split: this commit (4a) covers
every support transaction except the inprocessing phase bracket, which is 4b.

## What changed

- **Additive `tactix_support(s, TACTIX_SUPPORT_*, argument)`** runs the shared request DefaultPolicy
  uses: startup-certificate closure, preparation restart (keep level), root preparation (probing,
  target-capture, root-prefix flags), sparse collection, random-order and variable compaction, Reduce
  preparation, garbage collection, retired-redundant flush, a new BVE definition session, warm-up
  (H's decision order, heuristic or forced polarity), residual H reseeding, graph-labeling session
  reset, input-order release, root-prefix consumption, and entering/leaving an execution context
  (startup, inprocessing, speculation). Roots a support derives reach H through the session's
  publication profile. Contract: [controller interface](../../../docs/controller-interface.md);
  compositions: [calculus §2](../../../docs/calculus.md).
- **DefaultPolicy records** (feedback only; same transitions in the same order): support records
  carry the public argument as their instance; the per-solve resets Step 3 deferred
  (`ResetGraphLabelingSession`, `ReleaseInputBinaryOrder`, `ConsumeRootPrefix`) and the presearch
  and structured-root execution contexts (`Enter/LeaveExecutionContext`) are now records; a warm-up
  record names the polarity DefaultBranching's focused-phase pattern forces.
- **Declared non-actions:** `MaterializeColdState` (predicate-equivalent initial state; a C session
  never defers it) and `AttachHeuristics` (a C session owns H from `tactix_init`; reseeding is
  `RESEED_GUIDANCE`).
- **SR online conversion (N1 gap found by the replay).** The C conversion read only the selected SR
  producer's fields, but a GraphSymmetry step's structured-generator handoff runs the orbitopal
  producer with the step's orbitopal fields, and orbitopal refinement reads the graph limits. So
  DefaultPolicy's structured-root steps 84, 92 and 95 had no exact C form. The appended
  `complete_profile_mode` (ENABLE) makes the conversion read every field; zero-initialized callers
  are unchanged.

## Ratchet (`tests/test_default_action_map.cpp`)

| | Public entries | Internal entries | Declared | Public records | Internal records | Declared records |
|---|---|---|---|---|---|---|
| Step 3 | 18 | 12 | – | 1,421 | 5,514 | – |
| Step 4a | 34 | 14 | 2 | 1,633 | 5,341 | 9 |

The catch-all `support-transaction` (193 records) is split per transaction; an unmapped support
now fails the test. Internal support entries left: `support-phase-bracket` (20 records, step 4b),
`support-forward-closure` (0, step 5) and `support-warmup-installed-branching` (0; a caller-installed
`BranchingPolicy`, step 7). The 48 added records are the new reset and context records; every
other entry's count is unchanged.

## Replay (`tests/test_default_public_replay.cpp`)

Structured-root online steps are encoded as `tactix_online_step_v2` by
`tests/public_online_encoding.hpp`, configured as one-step programs (as the native sequence runs
each step with its own executor) and advanced once per record. Each encoding must convert back to
exactly the native request (asserted, 0 mismatches after the fix above).

| | Records | Replayed | Installs | Supports | Online steps | Strict stop |
|---|---|---|---|---|---|---|
| Step 3 | 6,294 | 699 | 435 | 0 | 0 | first `MaterializeColdState` |
| Step 4a | 6,344 | 1,049 | 505 | 74 | 198 | see below |

| Fixture class | Step 4a strict stop |
|---|---|
| default profile 0 (pigeons 4, 6), `sweep_interrupted_direction` | end of stream |
| search profiles 1–5 (pigeons 6), `sweep-retarget-lrat`, graph labeling | first `InprocessPhaseEntry` (#66 on the search profiles, formerly #50) |
| small SAT fixtures | end of stream |

The lookahead pass (assuming the 13 remaining internal support records) replays 1,129 records and
stops at the first `ConflictAnalysis` on the five search profiles and at the first slot
`Congruence` (step 5) on `sweep-retarget-lrat`.

**H at structured-root steps (first DefaultPolicy-level comparison on real BVE/SR publication).**
After every one of the 198 online steps, saved phases, scores, decision candidates, the active set,
target phases and mode equal the reference in both publication modes (DISCARD 198/198, NATIVE
198/198); 135 steps made rule progress and 23 grew the root. The modes agree because the online
executor publishes per application. Negative control (replay skipping the public execution-context
supports): 149/198, first divergence in saved phases at the first propagating SR step (97 on the
pigeonhole fixtures, 84 on `sweep_interrupted_direction`), so the comparison has teeth and the
inprocessing context is a required public transition. H is equal at every strict stop (15/17
fixtures; two have empty streams).

## Tests

- New `tests/test_api_support.cpp`: twin C sessions (one public under NATIVE, one running
  DefaultPolicy's composition of the same requests) stay identical in trail, clause census,
  lifecycle marker, contexts, random order, saved/target phases and candidates through root
  preparation, sparse collection, random-order compaction, warm-up (focused heuristic, forced
  positive, stable), Reduce preparation, preparation restart, variable compaction, collection and
  flush; lifecycle releases, context nesting, rejections without state change, residual reseeding,
  and `complete_profile_mode` conversion.
- `test_native_policy_feedback`: the structured-root stream is bracketed by one context pair.
- `tools/docs/check-config-wiring.py`: the `inprocessing` binding names the shared context
  functions.
- Local Release ctest 356/357; `gf3_gaussian_planner` fails as before (Release-only; full Debug
  runs it).
- Against c2.3 (scratch Release builds): `test_default_execution_signature`,
  `test_startup_support_signature` and `test_inprocess_execution_signature` retained DRAT, models
  and CNFs identical; stdout differs only in `state.strategy` and `rule_calls`, as in Steps 1–3.
  `test_controller_execution_signature` with feedback (88 files) identical apart from elapsed
  nanoseconds; `test_random_controller_policy` stdout identical.

## Byte identity and full Debug

Both campaigns use a `git archive` of the Step 4a commit `2c221ef9a`
(`benchmarks/runs/20261009-apigap-s4-source/pins/source-pin.json`). A first Release pin built from
the root checkout was discarded unused: another agent's edit was briefly applied to the shared tree
while it compiled (its binary hash differs from the archive build).

**Full Debug.** `tools/bench/full_debug_archive_campaign.py` with that source pin, as
`benchmarks/runs/20261009-apigap-s4-full-debug`, job 1763234: **357/357 passed, 0 skipped**
(Step 3's 356 plus `api_support`); the harness verifies the archive after the run. The Debug
build tree was deleted afterwards.

**DefaultPolicy.** The Step 3 three-arm campaign cloned as
`benchmarks/runs/20261009-apigap-s4-identity`, job 1763155 (`cpu-dense-hwulab-q`, 8 CPUs,
32 GiB, 600 s solve limit, `runtime_task.py`; c2.3 `--auto-proof`, Step 4a `--auto-proof`, Step 4a
with `--default-graph-labeling=on`, sequential in one allocation; Release `7b32db48…` built from
the archive, campaign copy `pins/tactix-new`). All 12 tasks completed with exit 0; all 12 rows are
identical in verdict, conflicts, decisions and certificate SHA-256; every certificate or model is
valid and certificates were deleted after checking.

| Row | Verdict | Conflicts | Decisions | Certificate | Wall (s) |
|---|---|---|---|---|---|
| ramsey_4_4_19 | UNSAT | 0 | 0 | `d169d9686c94` | 0.1 / 0.1 / 0.1 |
| lightsout_24 | UNSAT | 1 | 0 | `6fbc0514e27a` | 0.6 / 0.6 / 0.6 |
| mp1-Nb7T42 | SAT | 638,214 | 15,456,796 | `3b6b38a3bfa0` | 56.9 / 56.5 / 56.9 |
| battleship-14-26 | SAT | 665,487 | 1,681,225 | `fbedb083879e` | 15.0 / 14.9 / 14.9 |
| oddball_53_5_tto | SAT | 0 | 0 | `ba97acaa4dc6` | 5.6 / 5.6 / 5.5 |
| abw-K-dwt__234 | SAT | 8,935 | 44,009 | `97eed2dcb01a` | 21.0 / 23.8 / 23.1 |
| cliquecolouring_n13_k8_c7 | UNSAT | 1 | 0 | `ed71148e8802` | 0.1 / 0.1 / 0.1 |
| 9vliw_bp_mc | UNSAT | 198,066 | 2,279,994 | `438781f64b25` | 10.8 / 10.6 / 10.6 |
| combined-crypto1 | SAT | 75,764 | 172,719 | `134984e88e13` | 2.2 / 2.2 / 2.2 |
| sted5_0x24204-50 | SAT | 2,333,179 | 5,655,352 | `83c48a7431f7` | 127.8 / 128.5 / 128.1 |
| noL-11-16 | SAT | 1,180,890 | 3,131,471 | `190aa7fb722c` | 30.7 / 30.8 / 30.4 |
| b18 | UNSAT | 3,331,178 | 49,260,596 | `df61ef101a9d` | 216.5 / 216.2 / 216.2 |

Every value equals the Steps 1–3 tables; wall times are c2.3 / Step 4a / Step 4a with graph
labeling. abw-K-dwt keeps the 2–3 s layout offset Steps 0–3 attribute to code layout; the other
rows are within ±1%.

**Controller.** See Tests: identical to c2.3 apart from elapsed nanoseconds.

**Doc checks.** All `tools/docs/check-*.py` pass.

## What blocks next

- **Step 4b:** public phase entry/exit (inprocessing context, Eliminate watch disconnect/reconnect,
  occurrence session, DefaultFactorMatch selection, retired-redundant flush, sparse resume). Every
  search-profile stream stops there; with it they reach the first `ConflictAnalysis` (lookahead).
  The occurrence session is built after the phase-entry record without a record of its own, so 4b
  also needs that record.
- **Step 6:** `ConflictAnalysis` (851 records), the next blocker of every search profile.
- **Step 5:** registered slot rules under scoped slot parameters (`slot-registered` is not yet
  replayed), Sweep/Substitute/Vivify/fastel slots and the forward closure.
