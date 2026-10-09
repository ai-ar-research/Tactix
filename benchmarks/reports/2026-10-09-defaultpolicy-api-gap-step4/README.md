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

Pending at commit time; recorded in a follow-up to this report.

## What blocks next

- **Step 4b:** public phase entry/exit (inprocessing context, Eliminate watch disconnect/reconnect,
  occurrence session, DefaultFactorMatch selection, retired-redundant flush, sparse resume). Every
  search-profile stream stops there; with it they reach the first `ConflictAnalysis` (lookahead).
  The occurrence session is built after the phase-entry record without a record of its own, so 4b
  also needs that record.
- **Step 6:** `ConflictAnalysis` (851 records), the next blocker of every search profile.
- **Step 5:** registered slot rules under scoped slot parameters (`slot-registered` is not yet
  replayed), Sweep/Substitute/Vivify/fastel slots and the forward closure.
