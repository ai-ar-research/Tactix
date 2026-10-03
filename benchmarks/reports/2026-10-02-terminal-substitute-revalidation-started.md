# Terminal Substitute repair: final-source revalidation started

The repair is applied and locally committed at `375b621ba6677600f3e77bf8383af25d256d7b67`. Fresh Debug/Release pins are built, hashed and frozen; required final-source validation is submitted. **Whole-goal acceptance remains pending.** The [application report](2026-10-02-terminal-substitute-repair-applied.md) owns prior Debug closeout and exact proposal application; [staged causal evidence](2026-10-02-terminal-substitute-feedback-repair.md) remains frozen.

## Candidate provenance and scope

Both builds use GCC 13.1.0 and event/rule-journal diagnostics off. Debug enables the proof-population shadow and contains 173 executables; Release disables the shadow and builds the solver only.

| Pin | Solver SHA256 |
|---|---|
| `build/pin-shared-policy-feedback-debug-375b621b` | `95197f8c9f468bd3b121b1cd66d65457b9d5baade62bffb27f95e82caff593c2` |
| `build/pin-shared-policy-feedback-release-375b621b` | `b766be0145f7dbc9fd50c03db45ee7e2e3855933faaa84c30865064acd6337ed` |

The [candidate audit](2026-10-02-terminal-substitute-revalidation-started/candidate-audit.json) verifies all current sources and binaries. Against `f6f01982`, 791 of 795 Debug source bindings and 238 of 240 Release source bindings are unchanged. Differences are exactly two production files and two regression files, matching the validated proposal. The twelve prior source/contract bindings remain unchanged and their reviewed anchors remain present. This is source-delta evidence, not universal runtime coverage.

The fresh full Debug registry has exactly the same 212 names as its predecessor. All 984 source/binary/support bindings verify before submission. Retain all 795 root source bindings unchanged through terminal postflight; immutable old pins remain untouched. A completed old-pin suite does not accept the new pin.

## Required campaigns and ownership

| Campaign | Handle | Contract |
|---|---|---|
| Full Debug | SLURM `1746924`, watcher `40103` | Complete unfiltered 212-test suite; exact JUnit/exit and 984 pre/post verification |
| B28+C5 | array `1746808`, watcher `2514` | 33 rows; 1,200-second solver cap; retained aggregate ceiling |
| Rank 1--50 | array `1746809`, watcher `41792` | 50 rows; 1,200-second solver cap; retained aggregate ceiling |
| WV100 | array `1746810`, watcher `69871` | 100 rows; accepted per-instance checkpoint and 1,200-second solver cap |
| WV2--89 | array `1746811`, watcher `61124` | 89 rows; accepted checkpoint/exceptions and 600-second solver cap |
| Paired feedback overhead | SLURM `1746812`, watcher `24271` | Same four synthetic inputs, both policies, warm-up plus seven measured pairs |
| Claude-owned fuzz preparation/reporting | CLI session `3818` | New owner on this Debug pin; both fresh >=100,000-seed legs, corpus, full R1--R13 matrix |

Parent arrays retain the accepted `cpu-dense-hwulab-q`, 12 logical CPUs and 32,768 MiB per task. All are submitted at full width without a node constraint or throttle. Each proof gets one 1,800-second whole-pipeline deadline. Comparable completed baseline time `t` permits candidate time at most `t + max(0.30*t, 20 seconds)`. Clean censoring remains incomplete verification; rejection/error and significant comparable slowdown fail the bounded gate. The frozen 272-row baseline is verified before preparation, with no reference-solver rerun.

The overhead protocol is unchanged: end-to-end frontend provenance/snapshot/serialization/write costs, strict enabled records, deterministic outcome/statistics/certificate equality, independently checked SAT models and each distinct UNSAT certificate. It does not claim isolated callback cost or large-instance competitiveness. Timing is evidence, not deterministic replay state.

Claude exclusively owns its new fuzz preparation and campaign. The parent supplied actual pin/commit/hash, action-space version, fixed allowances and R1--R13 minimums through a frozen request; no old owner was resumed. Fresh proposed bases 2040000000/2041000000 require owner freshness verification. The request separately requires the fixed corpus-009/2032000128 terminal Substitute receiver regression, genuine post-disconnection canonical/batch owned/borrowed controls, complete effective action/configuration reach, interruption, observation/feedback purity, replay and negative oracles. Fresh, fixed-prefix, corpus and targeted counts must reconcile separately. Parent access remains limited to process/scheduler metadata and delivered terminal reports or bounded failure handoffs; no private campaign contents are inspected.

## Status and next action

Submissions and live handles prove execution/queue ownership, not acceptance. Some regression tasks have completed; full gates, Debug, overhead and owner reporting are still pending. Debug and overhead were observed queued by priority; owner CLI was observed live. Preserve those handles, use maintained watchers, and inspect actual terminal artifacts rather than infer completion from quiet output or queue disappearance. The mutable [resume state](../runs/20261002-shared-policy-feedback-375b621b-preparation/resume-state.json) owns the latest observations.

The [interim goal audit](2026-10-02-terminal-substitute-revalidation-started/goal-audit.json) preserves all thirteen original requirement groups. Finish every final-source gate, analyze individual parent-owned regressions from saved counters/certificates/resource evidence, obtain passing owner acceptance, reconcile final docs/status, and produce the final Andrew report before whole-goal promotion. No scalar reward, learning, fallback, rollback, forced ResumeSearch or global action-loop budget is introduced. O6 and separate formal Tier-D/checker obligations remain deferred. No cleanup, unrelated cancellation or push occurred.

Raw generated CTest registry/JUnit artifacts in the applied-repair checkpoint contain trailing whitespace. Their exact bytes remain preserved; a narrowly scoped whitespace check excluding only those two raw artifacts passes. Production and prose have no whitespace exception. Documentation size checking reports zero errors and six existing reserve warnings; section references resolve 73/73 and live-queue PROGRESS checking passes without warning.
