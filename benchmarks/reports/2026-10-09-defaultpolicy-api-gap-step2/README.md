# DefaultPolicy public-API gap: Step 2 (public-side H publication parity)

Step 2 of the [gap plan](../2026-10-09-defaultpolicy-api-gap-plan/README.md) (findings 2 and 3),
on top of [Steps 0–1](../2026-10-09-defaultpolicy-api-gap-steps-0-1/README.md). The baseline is
c2.3 = `555ecb4b0` (Release `ae4a26e4…`, `build/pin-candidate-c2.3-rel`).

| | Commit | Release (identity pin) |
|---|---|---|
| Step 2 | Step 2 commit (validated at `5a23c64e5`) | `f6d83411…` (`build/pin-apigap-s2-rel`, deleted after the run; campaign copy `pins/tactix-new`) |

The validated commit differs from the Step 2 commit only in PROGRESS and this report.

## What changed

- **One publication executor.** `execution::execute_action(RuleApplicationRequest, apply,
  publisher)` (`src/strategy/publication_execution.hpp`) gains an `advice` switch and a
  `RootTrail` root origin (rescan the whole trail, for an application that backtracks below its
  entry trail). `NATIVE_RULE_PUBLICATION` names DefaultPolicy's dosed-candidate profile
  (advice, new roots, retirements).
- **Public profile (finding 2).** `tactix_apply`, `tactix_apply_instance`,
  `tactix_apply_condition`, `tactix_apply_semantic_instance` and `tactix_online_advance` publish
  through the session profile. The additive `tactix_set_guidance_publication(s,
  TACTIX_GUIDANCE_DISCARD | TACTIX_GUIDANCE_NATIVE)` selects it, and
  `tactix_config(s, TACTIX_CFG_GUIDANCE_PUBLICATION)` reads it back. DISCARD, the v1 default, is
  the old behavior expressed as `DiscardedRulePublisher`; NATIVE uses `HeuristicRulePublisher`
  with `NATIVE_RULE_PUBLICATION`. Online steps already published natively per application; the
  mode governs only events left after the step.
- **Controller (finding 3).** The random controller's ConflictAnalysis and Propagate search
  services publish through the same executor: Propagate with `{NewAssignments, no advice, no
  retirements}` (only its new roots, as before), ConflictAnalysis with `{RootTrail, advice,
  retirements}`. Analysis used to drain roots before advice; the executor drains advice first.
  The order is unobservable because controller root actions publish their advice inside the
  online executor, so a search service finds none; a Debug assertion states that invariant.
- **Trace replay** (`src/policies/trace.cpp`) publishes through the executor with the default
  profile (advice, retirements), the same calls in the same order.
- **Not in this step.** The controller's hand-written `++strategy_epoch` after Decide and its
  affine-session `disable` (finding 3) are not publication. Sweep's retirement hold until the
  follow-up Substitute is deferred to Step 5. DefaultPolicy's per-slot publication (Substitute
  rounds, Factor advice, forward closure) still calls the consumers directly until Step 5.

## Tests

- **New `tests/test_api_guidance_publication.c`.** It loads the same formula into three
  solvers and runs a BVE(5) / BVE(16, whose resolvent is a root unit) / Factor(seed 8, a 3×3
  grid) / Substitute (1↔2) sequence, each followed by Propagate. The solvers are:
  - *native:* `tactix_apply_instance` under NATIVE;
  - *reference:* the same applications as online steps with a Propagate follow-up, published by
    the shared executor with `HeuristicRulePublisher`, the publisher DefaultPolicy uses;
  - *discard:* the v1 default.

  Saved phases are seeded by two decisions and a restart. After every step all three have the
  same formula census. The native decision candidates equal the reference's in focused (queue)
  order and, after SwitchMode, in stable (score) order. The discarded solver differs in
  measurable ways: eliminated and substituted variables stay ranked in the queue instead of
  joining the unranked tail, and Factor's fresh variable is the most preferred candidate
  instead of the least preferred (score 0 in stable mode).
- **`tests/test_default_public_replay.cpp` now compares H.** It compares saved phases, scores,
  decision candidates, the queue-active set, target phases and mode with the reference
  DefaultPolicy run after every replayed record, once per publication mode.
- `test_api_c.c` pins the new readback id (first unknown id is now 28).
- `test_api_instances`, `test_no_heuristics`, `test_heuristics_reattach`,
  `test_random_controller_policy` and `test_default_action_map` pass unchanged.

## Replay and ratchet

No route changed. The ratchet is unchanged at 15 public and 14 internal entries (641 public and
6,062 internal records), so its expectations were not edited. The replay still replays 264 of
6,067 records, with the same blocking census as Step 1.

H comparison at the replay stop point:

| Mode | Fixtures with equal H at stop | First H divergence |
|---|---|---|
| DISCARD | 15/17 | none |
| NATIVE | 15/17 | none |

The remaining two fixtures (`online-config-3/4`) have empty streams, so they are unmeasured.
Over both modes the test made 588 comparisons covering 21,096 candidate literals.

The comparison has teeth: every compared record carries non-empty guidance. But on this prefix
it cannot tell the modes apart. The prefix is the startup sequence (load profile, root
Propagate, Lucky opportunities and, for the small SAT fixtures, a few decisions), and none of
it produces a root assignment, retirement or advice event after input completion. A
DefaultPolicy-level measurement of BVE/Factor/Substitute publication needs the replay to get
past the first `MaterializeColdState` (below). `test_api_guidance_publication` gives the
rule-level evidence in the meantime.

What follows the stop point, from record sequences of the same fixtures:

- **Default profile** (`profile-0-*`, the `tests/fixtures` CNFs): `MaterializeColdState` is
  followed directly by the structured-root online program, BVE and SubstitutionRedundancy as
  `structured-root-online` records. This is the first place where publication parity with
  DefaultPolicy is observable.
- **Search profiles** (`profile-1..5-pigeons-6`): `MaterializeColdState` is followed by
  `SparseCollect`, `CompactRandomOrder`, ten mid-stream `InstallRuleParameters`,
  `RootPreparation` and `InprocessPhaseEntry/Exit`. Then come one root Propagate, ResumeSearch
  and about ten search Propagate/Decide records before the first `ConflictAnalysis`, which is
  Step 6.

## Byte identity

**Full Debug.** The run used archived sources through `tools/bench/full_debug_archive_campaign.py`
with the source pin `benchmarks/runs/20261009-apigap-s2-source/pins/source-pin.json`
(`git archive` of `5a23c64e5`). It ran as `benchmarks/runs/20261009-apigap-s2-full-debug`,
job 1763096: **356/356 passed, 0 skipped**. That count is Step 1's 355 plus
`api_guidance_publication`. The run includes the controller's new Debug assertion that a
search service never finds pending advice.

**Controller.** `test_controller_execution_signature` (8 seeds × SAT/UNSAT, retained with
feedback) was compared against the same test built from c2.3. Stdout, DRAT bytes, models, CNFs,
deterministic transcripts, traces and feedback streams are identical across all 88 files; the
only differences are `elapsed-nanos` and other wall-time fields.
`test_random_controller_policy` stdout is byte-identical to c2.3.

**DefaultPolicy.** DefaultPolicy is untouched by this step. The three-arm SLURM identity check
cloned the Step 1 shape into `benchmarks/runs/20261009-apigap-s2-identity`, job 1763083. It
ran on `cpu-dense-hwulab-q` with 8 CPUs, 32 GiB and a 600 s solve limit through
`runtime_task.py`. The three arms (c2.3 `--auto-proof`, Step 2 `--auto-proof`, Step 2 with
`--default-graph-labeling=on`) ran sequentially in one allocation, and every array task
completed with exit 0. All 12 rows are identical in verdict, conflicts, decisions and
certificate SHA-256. Every certificate or model is valid, and certificates were deleted after
checking.

| Row | Verdict | Conflicts | Decisions | Certificate | Wall (s) |
|---|---|---|---|---|---|
| ramsey_4_4_19 | UNSAT | 0 | 0 | `d169d9686c94` | 0.2 / 0.1 / 0.1 |
| lightsout_24 | UNSAT | 1 | 0 | `6fbc0514e27a` | 0.7 / 0.6 / 0.6 |
| mp1-Nb7T42 | SAT | 638,214 | 15,456,796 | `3b6b38a3bfa0` | 56.8 / 56.6 / 56.6 |
| battleship-14-26 | SAT | 665,487 | 1,681,225 | `fbedb083879e` | 14.9 / 15.0 / 15.0 |
| oddball_53_5_tto | SAT | 0 | 0 | `ba97acaa4dc6` | 6.4 / 6.5 / 6.5 |
| abw-K-dwt__234 | SAT | 8,935 | 44,009 | `97eed2dcb01a` | 21.0 / 22.9 / 22.8 |
| cliquecolouring_n13_k8_c7 | UNSAT | 1 | 0 | `ed71148e8802` | 0.1 / 0.1 / 0.1 |
| 9vliw_bp_mc | UNSAT | 198,066 | 2,279,994 | `438781f64b25` | 10.9 / 10.7 / 10.7 |
| combined-crypto1 | SAT | 75,764 | 172,719 | `134984e88e13` | 2.2 / 2.2 / 2.2 |
| sted5_0x24204-50 | SAT | 2,333,179 | 5,655,352 | `83c48a7431f7` | 129.2 / 129.0 / 129.7 |
| noL-11-16 | SAT | 1,180,890 | 3,131,471 | `190aa7fb722c` | 31.0 / 31.1 / 30.8 |
| b18 | UNSAT | 3,331,178 | 49,260,596 | `df61ef101a9d` | 218.8 / 219.2 / 215.5 |

Every value equals the Step 1 table. Wall times are in the columns c2.3 / Step 2 / Step 2 with
graph labeling. abw-K-dwt shows the same 1.8–1.9 s layout offset as in Steps 0 and 1, whose
report attributes it to code layout.

**Doc checks.** All `tools/docs/check-*.py` pass.

## Recommendation for the next steps

- **Step 3 next, as planned.** Its additive `tactix_install_parameters(s, rule_id, params)`
  also unblocks the replay. Every search-profile stream hits ten mid-stream
  `InstallRuleParameters` records right after the startup support records. The reporter can
  apply them only at load today.
- **Pulling Step 4's cheap records forward alone is not worthwhile.**
  - `CloseStartupCertificate` is already a replay marker.
  - `MaterializeColdState` plus `SparseCollect` would move the stop point by one or two
    records in the search profiles, to `CompactRandomOrder` and then the mid-stream installs.
  - In the default profile, `MaterializeColdState` leads straight into the structured-root
    online program. The replay cannot issue that through `tactix.h` yet, because typed online
    parameters cannot be converted back to `tactix_online_step`.

  The pull becomes worthwhile when paired with online-invocation replay. It would then give the
  first DefaultPolicy-level H parity measurement on real BVE/SR publications.
- **Order to make the H comparison bite.**
  1. Step 3: mid-stream installs.
  2. Replay of `structured-root-online` records.
  3. Step 4 support vocabulary: `MaterializeColdState`, `SparseCollect`,
     `CompactRandomOrder`, `RootPreparation`, phase entry/exit.
  4. Step 6: ConflictAnalysis target, the dominant blocker with 851 records.
