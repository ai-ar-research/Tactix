# DefaultPolicy public-API gap: Step 5 (inprocessing slots as online invocations)

Step 5 of the [gap plan](../2026-10-09-defaultpolicy-api-gap-plan/README.md) (A10, C4 BVE/Factor),
on top of [Step 4b](../2026-10-10-defaultpolicy-api-gap-step4b/README.md) and
[Step 6](../2026-10-10-defaultpolicy-api-gap-step6/README.md). The
identity baseline is c2.4 = `3361d3b51` (Release `57bdd51c…`, `build/pin-restart-c4-rel`).

| | Commit | Release (identity pin) |
|---|---|---|
| Step 5 code, tests, docs | `8ddd44960` (validation tree) | `034dfcc9…` (`build/pin-apigap-s5-rel`, built from the archive; deleted after copying; campaign copy `pins/tactix-new`) |
| Step 5 test follow-up | `123cf44fb` (tests only: the direct Vivify fixture uses Step 6's `RetierSchedule`) | same binary inputs |
| Report and PROGRESS | the commit adding this file | — |

## What changed

- **Slot occurrences are typed online invocations.** Scheduling (limits, delays, follow-ups,
  meters, effort) stays in DefaultPolicy; each occurrence it admits runs the registered rule
  through registry dispatch with typed parameters and the ledger remainder as rule work budget:
  - **Sweep** under its `SweepParameters` (`apply_rule_structured`; previously a typed request
    without dispatch).
  - **Substitute** under `SubstituteParameters{max_rounds, round_effort_ceiling}`: a round batch
    in one watch session; a further round runs only after a round that retired a variable and,
    with the ceiling, only while that round's own `substitute_ticks` stayed within the rule work
    budget (`execution::execute_substitute_rounds`). This replaces the policy's round-continuation
    closure and the unbudgeted complete helper.
  - **Vivify bands** under `VivifyParameters{scope, band, band_budget, band_offset}`: one share
    of the cumulative dose ledger (`VivifyBandRequest` now carries the typed parameters).
  - **Strengthen** in the Eliminate pipeline with `PostApplication::ForwardClosure`
    (`execution::ForwardClosureRequest`: watch reconnection plus root propagation from the
    application's entry propagation head).
- **Public surface (additive).** C: appended `max_rounds`/`round_effort_ceiling` to
  `tactix_online_substitute_parameters`, `band`/`band_budget`/`band_offset` to
  `tactix_online_vivify_parameters`, `TACTIX_ONLINE_POST_FORWARD_CLOSURE` (Strengthen only), and
  supports `TACTIX_SUPPORT_BEGIN_FORWARD_ROUND` / `TACTIX_SUPPORT_RESCHEDULE_BVE`. Zero-initialized
  callers are unchanged; permanent installs reject round batches and band ledgers. The online
  executor runs a Substitute round batch as one dispatched application and poses a band's ledger
  for that application only. Program serialization appends the new fields only when set, so
  existing transcripts keep their bytes.
- **Step 3 deferrals as typed rule state.** The per-round SubsumeForward/Strengthen completeness
  reset and the BVE retry refill after a growth-bound increase are recorded supports
  (`BeginForwardRound`, `RescheduleBveCandidates`) running shared requests
  (`src/strategy/slot_execution.hpp`), at the same points.
- **C4 BVE: ordering profile in the session.** `BveCandidateHeap` (Kissat's mutable elimination
  heap) and `kissat_bve_variable_score` move to `src/strategy/bve_schedule.hpp`.
  `BveInvocationSession` owns the score-heap ordering profile, its occurrence-count image and the
  kernel score hook for the pass (`order_by_score_heap` / `next_ordered` / `finish_ordering`;
  released with the session), and the connected unit flush after a candidate's propagation.
  DefaultPolicy no longer installs a kernel callback into a policy object.
- **C4 Factor.** Step 4b already made DefaultFactorMatch the public phase-entry Factor bound; the
  injected `FactorMatchPolicy` arm is now a declared, excluded diagnostic arm in the map.

## Ratchet (`tests/test_default_action_map.cpp`)

| | Public entries | Internal entries | Declared | Public records | Internal records | Declared records |
|---|---|---|---|---|---|---|
| Step 6 | 37 | 15 | 2 | 1,668 | 5,321 | 9 |
| Step 5 | 43 | 10 | 3 | 1,751 | 5,322 | 9 |

Moved to Public: `slot-sweep-batch` (2 records), `slot-substitute-rounds` (2), `slot-vivify-band`
(3) and `support-forward-closure` (1; a marker for the preceding Strengthen step's
post-application). New public entries `support-begin-forward-round` (14) and
`support-reschedule-bve` (1). `support-phase-entry-injected-factor` (0) becomes Declared (excluded
diagnostic arm). `slot-registered` and `slot-bve-candidate` now replay as online steps. Every slot
entry shows registry dispatch on every applied record.

The solve fixtures never schedule a search-time Vivify dose, an Eliminate-pipeline Strengthen pass
or a BVE growth-bound increase, so the test adds two direct-phase fixtures (one native phase with
only the named slots enabled, as `test_native_policy_feedback` runs it): `phase-vivify` (Vivify) and
`phase-eliminate` (BVE + Strengthen on PHP(4,5)). They contribute the Vivify, forward-closure and
reschedule records, 27 BVE candidates, one Strengthen slot and one internal record; all of them are
required to be observed.

Remaining internal entries: `propagate-search`, `decide`, `conflict-analysis`,
`conflict-analysis-ranked-trail`, `propagate-root` (search internals, Step 7, and H root
consumption), `affine-matrix-initialize`,
`affine-matrix-search`, `install-affine-matrix-parameters` (A8), `support-warmup-installed-branching`
(step 7) and `slot-fastel-occurrence` (374 records; see below).

## Replay (`tests/test_default_public_replay.cpp`)

Slot records replay as one-step online programs (`Replay::Online`, each occurrence configured
afresh); the test encoder gains Sweep, SubsumeForward and Strengthen profiles, the new Substitute
and Vivify fields and the forward-closure post-application. A public install is now compared with
the record's own typed profile: the reference H/profile capture is delivered at the end of a native
publication scope (a whole inprocessing phase), so it is not current inside a phase.

With Step 4b's public phase bracket the strict replay reaches into the inprocessing phases: `sweep-retarget-lrat`
now replays five slot occurrences (Congruence first) and stops at #108, the first startup
fast-elimination occurrence flush (`slot-fastel-occurrence`). All encoded slot steps convert back to
the native request (0 round-trip mismatches) and every install reproduces its profile. H after a
slot step equals the reference capture on 2 of 5 (first difference after the Congruence slot:
candidates and active set). That comparison is not conclusive: the reference capture of an in-phase
record is taken when the phase's publication scope delivers it, not after the record; DefaultPolicy
also publishes a slot's advice, roots and retirements at the slot boundary (and holds Sweep's
retirements until the follow-up Substitute) while the online executor publishes per application.
Totals on the final tree: 3,030 of 6,366 records replayed (606 installs, 162 supports, 203 online
steps of which 5 slots; Step 6 alone: 2,999); blocking: `conflict-analysis-ranked-trail` 156,
`slot-fastel-occurrence` 128, `walk` 26. Profiles 2 and 5 still stop at Step 6's mid-search Reduce
divergence (#760, #1273), which Step 5 does not touch.

## Tests

- New `tests/test_slot_invocations.cpp`: C conversion of the Substitute round batch, Vivify band
  and forward closure (and their rejections: ceiling without rounds, ledger without band, forward
  closure on another rule, permanent installs); a public Substitute round batch equals
  DefaultPolicy's composition on a twin C session (with and without the ceiling: 1 vs 2 rounds);
  the two rule-state supports; a public Vivify band equals the native band request on twin kernels
  for three ledgers; the BVE session's score-heap lifecycle (hook and image installed, popped,
  released by `finish_ordering` or by the session); serialization round trip with unchanged text
  for default fields.
- Updated: `test_default_action_map` (above), `test_default_public_replay`, `test_api_support`
  (first unknown support 23), `test_work_execution` (typed band),
  `test_kissat_journal_overlay_static` (heap trace site moved), `tools/docs/check-config-wiring.py`
  (band ledger binding).
- Debug ASan/UBSan (SLURM, targeted): `test_slot_invocations`, `test_default_action_map`,
  `test_default_public_replay`, `test_native_policy_feedback`, `test_work_execution`,
  `test_api_support`, `test_bve_active_pop`, `test_substitute_execution`, `test_online_api`,
  `test_inprocess_execution_signature` pass.
- Release (SLURM, full ctest from a source tarball): every failure is environmental (checker
  binaries, reference clones and benchmark caches absent from the tarball) or the known
  Release-only `gf3_gaussian_planner`; `test_vivify*`, `test_bve*`, `test_eliminate_limit`,
  `test_material_schedule`, `test_factor*`, `test_inprocess_program`, `test_sequence_followups`,
  `test_substitute*`, `test_slot_pose`, `test_scheduling` pass.
- Signature tests against the pre-step tree (fc128fae5, scratch Release builds, before rebasing on
  4b): `test_default_execution_signature`, `test_startup_support_signature` and
  `test_inprocess_execution_signature` retained DRAT/DSR, models and CNFs identical;
  `test_controller_execution_signature` with feedback (88 files) identical apart from elapsed
  nanoseconds; `test_random_controller_policy` stdout identical. `test_inprocess_execution_signature`
  stdout differs only in `state.strategy` and, in its post-phase solve lines for the Sweep and
  Substitute families, `rule_calls[Lucky]` (−3): the slot is now an ordinary registered action and
  closes the original-input certificate window like every other one, so the fixture's following
  solve skips startup Lucky opportunities. DefaultPolicy's solve closes that window before any
  inprocessing, so its trajectory is unaffected.

## Byte identity, performance and full Debug

**DefaultPolicy.** The c2.4 three-arm campaign cloned as `benchmarks/runs/20261010-apigap-s5-identity`,
job 1763920 (`cpu-dense-hwulab-q`, 8 CPUs, 32 GiB, 600 s solve limit, `runtime_task.py`; c2.4
`--auto-proof`, Step 5 `--auto-proof`, Step 5 with `--default-affine-search=on`, sequential in one
allocation; Release `034dfcc9…` from the `8ddd44960` archive, which shares every non-test input
with `123cf44fb`). All 36 runs exited normally, all 12 rows are identical in verdict, conflicts,
decisions and certificate SHA-256, every certificate or model is valid, and certificates were
deleted after checking. An interim campaign of the pre-rebase tree (`…-identity-interim1`) was
also identical on all 12 rows.

| Row | Verdict | Conflicts | Decisions | Certificate | Wall (s) |
|---|---|---|---|---|---|
| ramsey_4_4_19.normalised.cnf | UNSAT | 0 | 0 | `d169d9686c94` | 0.1 / 0.1 / 0.1 |
| lightsout_sat_24_unbounded_t | UNSAT | 1 | 0 | `6fbc0514e27a` | 0.6 / 0.6 / 0.6 |
| mp1-Nb7T42.cnf.xz | SAT | 638214 | 15456796 | `3b6b38a3bfa0` | 56.8 / 57.5 / 59.2 |
| battleship-14-26-sat.cnf.xz | SAT | 665487 | 1681225 | `fbedb083879e` | 15.2 / 15.3 / 15.4 |
| oddball_53_5_tto_zp.normalis | SAT | 0 | 0 | `ba97acaa4dc6` | 5.8 / 5.5 / 5.5 |
| abw-K-dwt__234.mtx-w50.cnf.x | SAT | 0 | 0 | `1026936d5617` | 20.8 / 19.4 / 19.4 |
| cliquecolouring_n13_k8_c7.sa | UNSAT | 1 | 0 | `ed71148e8802` | 0.1 / 0.1 / 0.1 |
| 9vliw_bp_mc.shuffled.cnf.xz | UNSAT | 198066 | 2279994 | `438781f64b25` | 10.7 / 10.6 / 10.7 |
| combined-crypto1-wff-seed-1- | SAT | 75764 | 172719 | `134984e88e13` | 2.2 / 2.2 / 2.2 |
| sted5_0x24204-50.cnf.xz | SAT | 2333179 | 5655352 | `83c48a7431f7` | 128.3 / 128.8 / 127.9 |
| noL-11-16.sanitized.cnf.xz | SAT | 1180890 | 3131471 | `190aa7fb722c` | 30.8 / 30.9 / 30.9 |
| b18.cnf.xz | UNSAT | 3331178 | 49260596 | `df61ef101a9d` | 220.1 / 219.8 / 219.4 |

Wall times are c2.4 / Step 5 / Step 5 with affine search.

**Performance (same CPU, sequential arms, `perf stat` user counters).**
`benchmarks/runs/20261010-apigap-s5-perf` (jobs 1763933/1763934), c2.4 against the final pin
(which also contains Steps 4b and 6), `--auto-proof`, on two BVE/Factor-heavy rows:

| Row | Instructions | Cycles | Solve (s) | BVE | Factor | Sweep | Substitute | Vivify | Strengthen |
|---|---|---|---|---|---|---|---|---|---|
| oisc-subrv-sll-nested-8 (Rank 22) | −0.017% | −3.0% | 240.6 → 233.4 | 8.91 → 8.71 | 18.63 → 17.69 | 42.83 → 42.18 | 10.20 → 10.03 | 18.51 → 18.01 | 9.14 → 7.14 |
| oski15a01b15s_opt (Rank 33) | −0.051% | +0.55% | 460.9 → 463.5 | 28.00 → 28.13 | 16.38 → 16.43 | 29.67 → 29.76 | 15.66 → 15.50 | 44.87 → 45.55 | 8.69 → 8.58 |

Per-pass columns are seconds from the solver's `c timing` lines; conflicts are identical. Both rows
execute fewer instructions, and no inprocessing pass runs longer by more than 1.5%. The interim
pre-rebase A/B (Step 5 alone on c2.4's base, `…-perf-interim1`) had instructions within +0.0008%
and cycles +0.6%/+1.8%, concentrated in search functions Step 5 does not touch (`shrink_block`
2.1% → 3.3% of samples): the hot search functions moved by 0x160 bytes (`kernel_propagate`,
`kernel_backtrack`, `shrink_block` changed 64-byte alignment), i.e. code layout. The per-candidate
BVE path validates once per pass as before and calls the session-owned heap directly.

**Full Debug.** Archived sources of `123cf44fb` through
`tools/bench/full_debug_archive_campaign.py` with the source pin
`benchmarks/runs/20261010-apigap-s5-final-source/pins/source-pin.json`, as
`benchmarks/runs/20261010-apigap-s5-full-debug`, job 1763946: **360/360 passed, 0 skipped**
(Step 6's 359 plus `slot_invocations`); the harness verified the archive after the run, and the
Debug build tree was deleted afterwards.

**Controller.** See Tests: identical to the pre-step tree apart from elapsed nanoseconds.

**Doc checks.** All `tools/docs/check-*.py` pass on the committed tree.

## Not done in Step 5

- **Startup fast-elimination occurrence flush** (`slot-fastel-occurrence`, 374 records): a policy
  traversal of the ordered occurrence image that drives SubsumeForward per clause (with an arena
  reference as instance) and compacts the image in place. It needs a rule-owned flush with a public
  BVE profile; not attempted here (the Step 7 work takes it).
- **Score-heap ordering for a public BVE pass:** the profile lives in `BveInvocationSession` and is
  selectable by shared-layer clients, but the online executor orders a CompleteCurrentPass by its
  selection snapshot, so `tactix.h` cannot select it.
- **Sweep + Substitute publication composite:** the retirement hold (and per-round Substitute,
  slot-boundary and forward-closure root publication) remains DefaultPolicy publication; it changes
  only H, and a per-record reference capture inside a phase is needed before replayed slot steps can be compared in H.
- **Gates:** the §8.4 default gates and the §8.2 fuzz legs were not run for Step 5.
