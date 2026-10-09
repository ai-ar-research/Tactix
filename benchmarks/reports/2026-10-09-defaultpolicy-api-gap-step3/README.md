# DefaultPolicy public-API gap: Step 3 (boundary profile writes as typed installs)

Step 3 of the [gap plan](../2026-10-09-defaultpolicy-api-gap-plan/README.md) (C2, C3 at
Reduce/Vivify, C5, C6), on top of [Steps 0–1](../2026-10-09-defaultpolicy-api-gap-steps-0-1/README.md)
and [Step 2](../2026-10-09-defaultpolicy-api-gap-step2/README.md). The baseline is c2.3 =
`555ecb4b0` (Release `ae4a26e4…`, `build/pin-candidate-c2.3-rel`).

| | Commit | Release (identity pin) |
|---|---|---|
| Step 3 | Step 3 commit (validated at detached `d32377624`, identical code/test/doc tree) | `0219ef1e…` (`build/pin-apigap-s3-rel`, deleted after copying; campaign copy `pins/tactix-new`) |

The validated tree differs from the Step 3 commit only in PROGRESS and this report.

## What changed

- **Public permanent installs (C6).** Additive `tactix_install_parameters(s, rule_id,
  const tactix_online_parameters *)` converts the profile exactly as an online step does, requires
  the online step's rule/family match and value validity
  (`strategy::validate_parameter_installation`), and installs it with the family's native setter
  (`strategy::install_permanent_parameters`, the setter `ParameterInstallationRequest` and scoped
  execution use). A Walk profile is its scoring guidance; its step budget, seed and seed candidate
  must be zero. Additive `tactix_install_material_settings(s, touch, likely_kept)` takes
  `TACTIX_ONLINE_SETTING_*`; readback ids `TACTIX_CFG_TOUCH_FILTER` (28) and
  `TACTIX_CFG_LIKELY_KEPT_REDUNDANT` (29). Neither call finalizes input or changes σ or H.
- **DefaultPolicy boundary writes become typed installs** (same values, same order; each is now an
  `InstallRuleParameters` support record):
  - C2: formula classification installs the ConflictAnalysis profile with `jump_reasons_bigbig`.
  - C3: every Reduce and Vivify tier pose (solve entry, before each Reduce, after each Reduce, each
    Vivify dose) installs ReduceParameters `tier1/tier2`, the shared boundaries that Reduce
    ranking, Vivify's bands and the likely-kept filter read.
  - C5: each BVE ramp pass installs its growth bound; a Factor dose installs composite matching, and
    the end of a budgeted dose clears it (an install only when it is set, which leaves the same state
    as the former unconditional clear).
- **Walk guidance record.** `InstallWalkGuidance` now records the profile a client installs: the
  guidance with zero budget, seed and candidate (it previously copied the inherited kernel values,
  which is why Step 1's replay could not apply it).
- **Not typed in this step** (rule-state resets, not profiles; a parameter side effect would also
  fire on every scoped online install of the family):
  - the BVE retry refill after a bound increase → Step 5 (BVE ordering profile);
  - the per-round SubsumeForward/Strengthen completeness reset → Step 5 (slot invocations);
  - the per-solve graph-labeling session clear → Step 4, with `unflushed_root` and the input-binary
    order release that the plan already assigned there;
  - the per-mode retier schedule (`tier{1,2}_by_mode`, written by the Reduce rule's retier
    function) and the OTFS retier pose → Step 6 (`retier_schedule`).

## Ratchet (`tests/test_default_action_map.cpp`)

| | Public entries | Internal entries | Public records | Internal records |
|---|---|---|---|---|
| Step 2 | 15 | 14 | 641 | 6,062 |
| Step 3 | 18 | 12 | 1,421 | 5,514 |

`install-parameters`, `install-walk-guidance` and `install-material-settings` move to Public
(`Replay::Install`). The new internal entry `install-affine-matrix-parameters` (0 records on the
fixtures) splits off the persistent AffineMatrixInitialize install, which has no
`tactix_online_parameters` form and belongs to A8. Install records grow from 516 to 748 (the C2/C3/C5
installs); every other entry's record count is unchanged, so the fixture trajectories are the same.
The map now classifies a structured-stage Lucky step as `structured-root-online` rather than
`lucky-opportunity` (8 replay records; no ratchet fixture runs the structured root with Lucky).

## Replay (`tests/test_default_public_replay.cpp`)

Every install record, leading or mid-stream, now replays through `tactix_install_parameters` /
`tactix_install_material_settings`, with the typed profile encoded by
`tests/public_parameter_encoding.hpp` (the inverse of the C conversion). The reference captures an
installed-profile image (the 26 rule families DefaultPolicy installs, Walk guidance and the material settings)
after every record. After each public install the replayed family must equal the reference's
(asserted: 870 installs over both publication modes, 0 mismatches), and the first divergence of the
whole image is reported: none on any fixture.

| | Records | Replayed | of which installs | Strict stop |
|---|---|---|---|---|
| Step 2 | 6,067 | 264 (+ load-time profile) | applied at load only | first `MaterializeColdState` |
| Step 3 | 6,294 | 699 | 435 | first `MaterializeColdState` (unchanged) |

The strict prefix does not advance: on every non-trivial stream `MaterializeColdState` (then
`SparseCollect`, `CompactRandomOrder`) precedes the mid-stream installs. H is equal at every stop
point in both modes (15/17; the other two streams are empty).

A new lookahead pass assumes the support transactions that have no public request yet (40 records)
and shows what blocks next:

| Fixture class | Lookahead stop |
|---|---|
| search profiles 1–5, pigeons 6 | `#79 ConflictAnalysis` (Step 6) after the ten mid-stream installs, RootPreparation, phase entry/exit, post-preprocess Propagate, both classification installs, ResumeSearch and the first search Propagate/Decide records |
| default profile 0, `sweep_*` CNFs | the first structured-root online step (`Lucky 8`, then BVE/SR) right after `MaterializeColdState` |
| small SAT fixtures, graph labeling | end of stream |

Lookahead replays 827 records in all.

## Tests

- New `test_api_c` case `test_permanent_installs`: rule/family mismatch, retired and unknown ids,
  NULL, INHERIT and invalid profiles are rejected with no change; BVE, Factor, Vivify and Lucky
  (Simon) installs read back; a Walk install with a step budget is rejected and a guidance install
  leaves the work budget unchanged; material settings honour INHERIT; installs do not finalize input.
  The first unknown config id is now 30.
- `test_native_policy_feedback`: a Vivify dose's tier pose adds one `Reduce` install record.
- `test_preparation_execution`: the Walk guidance record carries zero budget and seed.
- Signature tests against the pre-step tree and c2.3 (scratch Release builds): DefaultPolicy DRAT
  bytes, models, CNFs and stdout (`test_default_execution_signature`, `test_startup_support_signature`,
  `test_inprocess_execution_signature`) are identical; the default feedback streams differ only by the
  added `InstallRuleParameters` records (1–48 per stream) and their `elapsed_nanos`.
- Controller: `test_controller_execution_signature` with feedback retained, 88 files, identical to
  c2.3 except `elapsed-nanos`/`elapsed_nanos`; `test_random_controller_policy` stdout identical.
- Local Release ctest: 353/356 before the two expectation updates above; the third failure,
  `gf3_gaussian_planner` (`map::at`), fails identically on the pre-step Release build and is
  NDEBUG-specific (full Debug runs it).

## Byte identity and full Debug

**Full Debug.** Archived sources of `d32377624` through
`tools/bench/full_debug_archive_campaign.py` with the source pin
`benchmarks/runs/20261009-apigap-s3-source/pins/source-pin.json`, as
`benchmarks/runs/20261009-apigap-s3-full-debug`, job 1763144: **356/356 passed, 0 skipped**
(the Debug build tree was deleted after the run).

**DefaultPolicy.** The three-arm SLURM identity check cloned the Step 2 campaign into
`benchmarks/runs/20261009-apigap-s3-identity`, job 1763132 (`cpu-dense-hwulab-q`, 8 CPUs, 32 GiB,
600 s solve limit, `runtime_task.py`; c2.3 `--auto-proof`, Step 3 `--auto-proof`, Step 3 with
`--default-graph-labeling=on`, sequential in one allocation). All 12 tasks completed with exit 0.
All 12 rows are identical in verdict, conflicts, decisions and certificate SHA-256, every
certificate or model is valid, and certificates were deleted after checking.

| Row | Verdict | Conflicts | Decisions | Certificate | Wall (s) |
|---|---|---|---|---|---|
| ramsey_4_4_19 | UNSAT | 0 | 0 | `d169d9686c94` | 0.1 / 0.1 / 0.1 |
| lightsout_24 | UNSAT | 1 | 0 | `6fbc0514e27a` | 0.6 / 0.6 / 0.7 |
| mp1-Nb7T42 | SAT | 638,214 | 15,456,796 | `3b6b38a3bfa0` | 56.7 / 56.2 / 56.2 |
| battleship-14-26 | SAT | 665,487 | 1,681,225 | `fbedb083879e` | 14.9 / 14.9 / 14.9 |
| oddball_53_5_tto | SAT | 0 | 0 | `ba97acaa4dc6` | 5.6 / 5.4 / 5.4 |
| abw-K-dwt__234 | SAT | 8,935 | 44,009 | `97eed2dcb01a` | 21.0 / 23.6 / 23.7 |
| cliquecolouring_n13_k8_c7 | UNSAT | 1 | 0 | `ed71148e8802` | 0.1 / 0.1 / 0.1 |
| 9vliw_bp_mc | UNSAT | 198,066 | 2,279,994 | `438781f64b25` | 10.8 / 10.6 / 10.6 |
| combined-crypto1 | SAT | 75,764 | 172,719 | `134984e88e13` | 2.2 / 2.2 / 2.2 |
| sted5_0x24204-50 | SAT | 2,333,179 | 5,655,352 | `83c48a7431f7` | 128.4 / 129.0 / 129.1 |
| noL-11-16 | SAT | 1,180,890 | 3,131,471 | `190aa7fb722c` | 30.9 / 30.7 / 30.8 |
| b18 | UNSAT | 3,331,178 | 49,260,596 | `df61ef101a9d` | 217.1 / 216.9 / 216.5 |

Every value equals the Step 1 and Step 2 tables; wall times are c2.3 / Step 3 / Step 3 with graph
labeling. abw-K-dwt shows the same 2.6 s layout offset that Steps 0–2 attribute to code layout; the
other rows are within ±1%.

**Controller.** See Tests: identical to c2.3 apart from wall-time fields.

**Doc checks.** All `tools/docs/check-*.py` pass.

## Recommendation for the next steps

- **Step 4's support vocabulary next, together with replay of the structured-root online steps.**
  The strict replay is still pinned at the first `MaterializeColdState`; the lookahead shows that
  once `MaterializeColdState`, `SparseCollect`, `CompactRandomOrder`, `RootPreparation` and phase
  entry/exit are public, the search profiles replay through the mid-stream installs (now public) to
  the first `ConflictAnalysis`, and the default profile reaches its first structured-root online
  step. Pairing Step 4 with a typed-online-step encoder (the online analogue of
  `public_parameter_encoding.hpp`) gives the first DefaultPolicy-level H comparison on real BVE/SR
  publication. Fold the three per-solve resets deferred here (graph-labeling session clear,
  `unflushed_root`, input-binary-order release) into Step 4's StartSearch/CloseStartup support.
- **Then Step 6** (ConflictAnalysis target, 851 blocking records), which is the next blocker of every
  search profile, including the OTFS retier pose and the per-mode retier schedule left here.
- **Then the rest of Step 5**, including the BVE retry refill and the per-round completeness reset
  deferred here.
- The §8.4 default gates and §8.2 fuzz legs for Steps 1–3 remain pending before push; Step 3 adds
  no hot-path work (installs run per reduce, phase, pass or dose), so it needs no separate
  performance gate beyond them.
