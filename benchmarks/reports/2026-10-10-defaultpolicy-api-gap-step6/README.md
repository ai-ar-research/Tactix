# DefaultPolicy public-API gap: Step 6 (ConflictAnalysis payload, chrono instance, retier schedule)

Step 6 of the [gap plan](../2026-10-09-defaultpolicy-api-gap-plan/README.md) (A3, C1, C4 analysis
hook, plus the C3 items [Step 3](../2026-10-09-defaultpolicy-api-gap-step3/README.md) deferred), on top
of [Step 4b](../2026-10-10-defaultpolicy-api-gap-step4b/README.md). Baseline: c2.4 = `3361d3b51`
(Release `57bdd51c…`, `build/pin-restart-c4-rel`); main's source at `fc128fae5` equals it.

| | Commit | Release |
|---|---|---|
| Validation tree | detached `673e85a0b` = `daffc2623` + the Step 6 code/test/doc tree | `454c2ebd…` (campaign copies `pins/tactix-new`) |
| Step 6 | this commit = the validation tree plus the two fixes its full Debug found (below), PROGRESS and this report | rerun on this commit: see Full Debug |

## What changed

- **Per-conflict payload by value (C1).** `execution::NativeConflictAnalysisRequest` carries
  `{bump_reason_limit, bump_reason_depth, target_mode, retier_schedule}`. The executor installs the
  bump payload for that analysis only and resets it to 0/1 (the former DefaultPolicy field
  set/reset), applies the schedule, and returns the updated schedule in
  `ConflictAnalysisExecutionResult`. DefaultPolicy's advise closure keeps only observation (bump
  delay gate, glue EMA, reluctant tick, trace).
- **Public chrono instance (A3).** Additive ConflictAnalysis semantic payload
  `ANALYZE_TARGET_CHRONO` = `TACTIX_ANALYZE_TARGET_CHRONO` (bit 63, like `REDUCE_CLAUSE_INSTANCE`):
  the target is chosen after learning by `conflict_reuse_target` on the learned-clause path as on both
  reuse paths. DefaultPolicy's native request uses this mode, so `stats.chronological` is incremented
  exactly once per chronological target, inside the analysis (DefaultPolicy no longer counts it). No
  semantic enumerator is registered: semantic enumeration keeps the v1 level list, so the random
  controller's draws are unchanged. Semantics: [calculus M3](../../../docs/calculus.md); modes:
  [implementation notes §4](../../../docs/implementation-notes.md).
- **Retier schedule replaces the hook (C4, C3).** `analysis_conflict_hook`,
  `KernelAnalysisConflictPublisher` and the checkpoint vector are gone. `RetierSchedule` (deadline,
  geometric interval, quantiles, focused/stable tier snapshots) is caller state: at every conflict an
  OTFS re-analysis counts, the analysis runs `reduce_retier_at_conflict` on the request's schedule,
  which retiers the current mode, poses its pair as the Reduce `tier1/tier2` and advances the
  deadline. The per-mode snapshots (`tier{1,2}_by_mode`, formerly kernel fields written by the policy
  at solve entry and by Reduce's retier) move into DefaultPolicy's schedule; Reduce's
  current-mode retier and every tier pose read and write that schedule. A public session installs a
  schedule with additive `tactix_install_retier_schedule` (read back with
  `tactix_read_retier_schedule`); registry ConflictAnalysis applies it when enabled (default
  disabled, so existing clients are unchanged).
- **Ranked-trail arm.** `analysis.reuse_ranked_trail_suffix` (off by default; signature profile 1)
  keeps the Advised mode (its H-ranked target after learning) and records the canonical instance; it
  is the declared diagnostic outside the correspondence claim.
- **Hot path.** The first build kept three instantiations of `analyze_apply_internal`; GCC then
  stopped inlining the analysis' scratch `std::vector::push_back` (perf record on mp1: out-of-line
  `push_back` 0.71% → 2.47% of instructions), +1.36% user instructions on mp1. Chrono and Advised now
  share one instantiation with a runtime `chosen` pointer, which removes the regression (below).

## Ratchet (`tests/test_default_action_map.cpp`)

| | Public entries | Internal entries | Declared | Public records | Internal records | Declared records |
|---|---|---|---|---|---|---|
| Step 4b | 37 | 14 | 2 | 1,668 | 5,321 | 9 |
| Step 6 | 37 | 15 | 2 | 1,668 | 5,321 | 9 |

`conflict-analysis` moves from Kernel/no replay to **Shared** with replay through the public chrono
instance (806 records): like `propagate-search` and `decide`, it is a static in-interval step
whose transition a declared public request expresses; it cannot be Public under the ratchet's
definition, which requires registry dispatch (strategy-epoch advance) for each record. The new
Kernel entry `conflict-analysis-ranked-trail` (156 records, all from signature profile 1) is the
declared diagnostic arm. Record counts per entry are otherwise unchanged.

## Replay (`tests/test_default_public_replay.cpp`)

Each chrono ConflictAnalysis record carries its whole payload: the analysis profile with the bump
fields, and `;retier=`/`;retier-after=` (the schedule carried and returned). The replay installs the
profile (`tactix_install_parameters`) and the schedule (`tactix_install_retier_schedule`), applies
`tactix_apply_semantic_instance(ConflictAnalysis, TACTIX_ANALYZE_TARGET_CHRONO)`, checks the result
code and state, asserts that `tactix_read_retier_schedule` equals the native returned schedule, then
restores the neutral bump fields and the idle schedule.

| | Records | Replayed | Public chrono analyses | Blocking entries |
|---|---|---|---|---|
| Step 4b | 6,357 | 1,154 | 0 | `conflict-analysis` 851, slots 159, `walk` 26 |
| Step 6 | 6,357 | 2,999 | 330 (all with a bump payload, 4 with an OTFS retier; 0 schedule mismatches) | `conflict-analysis-ranked-trail` 156, slots 159, `walk` 26 |

Search-profile strict stops (pigeons-6; Step 4b stopped all of them at their first ConflictAnalysis,
#84):

| Profile | Records | Replayed | Stop | H at stop (NATIVE) |
|---|---|---|---|---|
| 1 (ranked-trail arm) | 856 | 82 | #84 `conflict-analysis-ranked-trail` (declared diagnostic) | same |
| 2 (chrono levels 0) | 813 | 746 | #759 Reduce: public result NoOp, native Ok | same |
| 3 | 1,000 | 92 | #94 Walk (step 5: online Walk parameters) | same |
| 4 | 1,433 | 92 | #95 Walk | same |
| 5 | 1,298 | 1,243 | #1272 Reduce: irredundant 60 vs 61 after the record | same |

Under NATIVE publication H equals the reference at every stop; under DISCARD the two Reduce stops
differ in the active set (v1 DISCARD drops root publication, as in Step 2). The two mid-search
Reduce divergences are the next blockers on the chrono path: the public Reduce instance has no
per-call compaction / CompactVariables (plan inventory A12), not an analysis difference (the
preceding 330 analyses and their schedules match).

## Tests

- New `tests/test_analysis_native_target.cpp`: 40 random 3/4-CNFs around and above the threshold, in
  DRAT and LRAT; twin kernels make the same decisions; at each conflict one runs the native request
  (random bump payload, chrono target, carried schedule), the other installs the payload and applies
  the public chrono instance through registry dispatch. Trail, assignments and reasons, clause arena,
  analysis counters, posed tiers, H scores, schedule and proof bytes are identical; 10,858 conflicts,
  1,764 chronological targets, 602 reuses, 200 OTFS retiers, 446 OTFS strengthenings, 4,762
  reason-bump rounds, 6 refutations (all asserted nonzero).
- `test_scheduling`: the retier cadence test exercises `reduce_retier_at_conflict` (deadline,
  growth, stable pose, disabled schedule); Vivify tier tests set the snapshots on the scheduler's
  schedule. `test_restart_reduce`, `test_event_trace`, `test_native_policy_feedback`: snapshot moved
  to `RetierSchedule`.
- Unchanged and passing: `test_analyze`, `test_conflict_reuse_targets`, `test_root_conflict_reuse`,
  `test_shared_policy_transactions`, `test_api_support`, `test_strategy_config`.
- Against a Release build of `fc128fae5` (= c2.4 source): `test_default_execution_signature`,
  `test_startup_support_signature`, `test_inprocess_execution_signature` retained DRAT, models, CNFs
  and stdout identical; `test_controller_execution_signature` with feedback (88 files) identical apart
  from elapsed nanoseconds; `test_random_controller_policy` stdout identical.

## Byte identity, performance and full Debug

**DefaultPolicy identity.** The Step 4 three-arm campaign cloned as
`benchmarks/runs/20261010-apigap-s6-identity-673e`, job 1763880 (`cpu-dense-hwulab-q`, 8 CPUs,
32 GiB, 600 s, `runtime_task.py` with runtime context and perf counters; c2.4 `--auto-proof`, Step 6
`--auto-proof`, Step 6 `--default-affine-search=on`, sequential in one allocation). All 36 tasks
exit 0; all 12 rows are identical in verdict, conflicts, decisions and certificate SHA-256, every
certificate or model is valid, certificates deleted after checking. An earlier campaign on the
three-instantiation build (`20261010-apigap-s6-identity`, job 1763717) was also 12/12 identical.

| Row | Verdict | Conflicts | Decisions | Certificate | Wall (s) | Instructions c2.4 / Step 6 |
|---|---|---|---|---|---|---|
| ramsey_4_4_19 | UNSAT | 0 | 0 | `d169d9686c94` | 0.2 / 0.2 / 0.2 | 1.203e9 / 1.202e9 |
| lightsout_24 | UNSAT | 1 | 0 | `6fbc0514e27a` | 0.7 / 0.7 / 0.7 | 4.719e9 / 4.715e9 |
| mp1-Nb7T42 | SAT | 638,214 | 15,456,796 | `3b6b38a3bfa0` | 57.1 / 56.6 / 57.6 | 4.247e11 / 4.243e11 |
| battleship-14-26 | SAT | 665,487 | 1,681,225 | `fbedb083879e` | 14.9 / 14.9 / 15.0 | 1.348e11 / 1.347e11 |
| oddball_53_5_tto | SAT | 0 | 0 | `ba97acaa4dc6` | 5.8 / 5.8 / 5.8 | 6.142e10 / 6.141e10 |
| abw-K-dwt__234 | SAT | 0 | 0 | `1026936d5617` | 21.1 / 22.1 / 22.2 | 3.419e11 / 3.419e11 |
| cliquecolouring_n13_k8_c7 | UNSAT | 1 | 0 | `ed71148e8802` | 0.1 / 0.1 / 0.1 | 3.113e8 / 3.114e8 |
| 9vliw_bp_mc | UNSAT | 198,066 | 2,279,994 | `438781f64b25` | 10.7 / 10.7 / 10.7 | 9.106e10 / 9.101e10 |
| combined-crypto1 | SAT | 75,764 | 172,719 | `134984e88e13` | 2.2 / 2.2 / 2.2 | 1.827e10 / 1.826e10 |
| sted5_0x24204-50 | SAT | 2,333,179 | 5,655,352 | `83c48a7431f7` | 128.4 / 127.9 / 127.0 | 8.705e11 / 8.696e11 |
| noL-11-16 | SAT | 1,180,890 | 3,131,471 | `190aa7fb722c` | 30.6 / 31.5 / 30.6 | 2.295e11 / 2.294e11 |
| b18 | UNSAT | 3,331,178 | 49,260,596 | `df61ef101a9d` | 224.6 / 215.8 / 220.6 | 1.589e12 / 1.587e12 |

Wall times are c2.4 / Step 6 / Step 6 with affine search. abw-K-dwt's certificate differs from the
Step 1–4a tables because c2.4's graph-labeling change altered it; all three arms agree.

**Same-CPU A/B (proofless).** `benchmarks/runs/20261010-apigap-s6-perf2`, job 1763893: per instance
one allocation runs c2.4, the Step 4b base (`1caa7af83`, Release `21f203b6…`) and Step 6 in turn
pinned to one CPU, `perf stat -e instructions:u,cycles:u`.

| Instance | Instructions c2.4 / 4b / Step 6 | Δ vs c2.4 | Cycles c2.4 / 4b / Step 6 | Δ vs c2.4 |
|---|---|---|---|---|
| b18 | 1.54967e12 / 1.54967e12 / 1.54878e12 | −0.06% | 8.204e11 / 8.171e11 / 8.018e11 | −2.3% |
| mp1-Nb7T42 | 4.06161e11 / 4.06161e11 / 4.05714e11 | −0.11% | 2.109e11 / 2.092e11 / 2.120e11 | +0.5% |
| sted5_0x24204-50 | 8.52509e11 / 8.52509e11 / 8.51716e11 | −0.09% | 4.825e11 / 4.769e11 / 4.791e11 | −0.7% |
| noL-11-16 | 2.23547e11 / 2.23547e11 / 2.23388e11 | −0.07% | 1.163e11 / 1.162e11 / 1.160e11 | −0.2% |

User instructions are deterministic (repeat runs agree to 1e-6) and now slightly lower than c2.4
(the checkpoint vector and hook call are gone); cycle differences are within the ±1% spread of the
two identical-code arms (c2.4 vs 4b). The rejected three-instantiation build measured
4.0616e11 → 4.1170e11 user instructions on mp1 (+1.36%; `benchmarks/runs/20261010-apigap-s6-perf`).

**Full Debug.** `tools/bench/full_debug_archive_campaign.py` with the source pin
`benchmarks/runs/20261010-apigap-s6-source/pins/source-pin.json` (archive of `673e85a0b`), as
`benchmarks/runs/20261010-apigap-s6-full-debug`, job 1763900: 357/359 passed. The two failures
were documentation checks: `rule_policy_boundary` (the first `reduce_retier_at_conflict` read the
search mode from H inside `rules/reduce`; the caller, ConflictAnalysis, now passes the mode) and
`comment_hygiene` (a 13-line replay header comment). This commit contains both fixes; the full
Debug and identity reruns on this commit are recorded in the follow-up commit.

**Doc checks.** All `tools/docs/check-*.py` pass.

## What blocks next

- **Mid-search Reduce** (profiles 2 and 5): the public Reduce instance lacks DefaultPolicy's per-call
  compaction and CompactVariables (A12); both chrono-profile streams stop there after replaying
  every preceding analysis.
- **Walk** (profiles 3, 4) and the slot rules (`slot-registered` Congruence, BVE candidates, fastel,
  Substitute rounds, Sweep batch): Step 5.
- **Ranked-trail arm**: remains a declared diagnostic (Kernel entry), by plan.
