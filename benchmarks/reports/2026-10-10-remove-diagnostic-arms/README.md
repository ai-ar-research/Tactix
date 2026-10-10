# Remove DefaultPolicy's diagnostic arms

User decision, 2026-10-10: remove the three off-by-default diagnostic arms of DefaultPolicy's
action-route ratchet (`tests/default_action_map.hpp`). Remove the two declared non-actions as well,
if that leaves the default solver unchanged. Builds on the
[Step 7 record](../2026-10-10-defaultpolicy-api-gap-step7/README.md). The identity baseline is c2.4
(`build/pin-restart-c4-rel`, Release `57bdd51c…`).

## Result

| | Before (`668797c59`) | After |
|---|---|---|
| Ratchet entries public / internal / declared | 51 / 0 / 5 | 51 / 0 / 2 |
| Declared entries | ranked-trail analysis, installed-branching warm-up, injected Factor matcher, `MaterializeColdState`, `AttachHeuristics` | `MaterializeColdState`, `AttachHeuristics` |
| Twin `default_public_correspondence` | COMPLETE | COMPLETE: 19 fixtures, 0 mismatches, 0 fallbacks, 0 declined, 7,402 public requests |
| Replay gate | 5,462 of 6,366 records; profile 1 stopped at the ranked-trail arm | 6,219 of 6,363 records; every stream reaches its end |
| Release `tactix` | `d540ef32…` | `9e1c52b4…` |

The ratchet now asserts its declared set (`EXPECTED_DECLARED`) both ways, as it already did for the internal set.

## Part A: the three arms, removed

1. **Ranked-trail ConflictAnalysis target.** Removed: `DefaultStrategyConfig::analysis.reuse_ranked_trail_suffix`,
   `DefaultPolicy::chrono_reuse_trail_`, `TACTIX_CHRONO_REUSE`, `reuse_trail_level` and the Advised advice branch.
   DefaultPolicy now issues only the public `ANALYZE_TARGET_CHRONO` instance. The shared executor's Advised mode stays,
   because it is the rule's caller-target mode. Signature profile 1 is still forced focused mode, now with the shipped
   chrono target. Parity-ledger `SP-CHRONO-1` records the removal.
2. **Warm-up with a caller-installed BranchingPolicy.** The `DefaultPolicy::branching` injection is not the vehicle
   for §8.4's "external branching without kernel edits": that is the public `tactix_decide` path
   (`tests/test_api_c.c` `test_external_branching`, `examples/`). The injection was therefore deleted. Every pick,
   restart-reuse query, conflict hook and warm-up now uses `DefaultBranching`, and the observation's
   `external_branching` field is gone. Two tests used an installed BranchingPolicy as a stop seam:
   - `test_graph_labeling_model` now stops at the first `Decide` feedback record and runs the same assertions there;
   - the action-map graph-labeling fixture now solves to completion (SAT), so the twin needs no stop seam.
3. **Injected FactorMatchPolicy.** Removed the `factor_match` pointer and the `FactorMatchPolicy` interface.
   `DefaultFactorMatch` now implements the rule's `FactorSelection` directly, and the phase entry always carries
   the public bound. `test_bve_bound_cut` and `test_slot_pose` read the bound Factor receives from the
   `InprocessPhaseEntry` record's public argument (its high 32 bits). Their expected ladders are unchanged.

Diff against `668797c59`: +159 / −303 lines in 20 files. `src/` is +43 / −174: −86 net in
`default_policy.cpp`, and the 37-line `FactorMatchPolicy` interface deleted from `subpolicy.hpp`.

## Part B: both non-actions stay declared

Campaign `benchmarks/runs/20261010-rmdiag-identity` (job 1766705; `cpu-dense-hwulab-q`, 8 CPUs, 32 GiB, 600 s,
`runtime_task.py`). Each row ran six arms sequentially in one allocation:

- c2.4;
- Part A (`--auto-proof`, and with `--default-affine-search=on`);
- the candidate "A + cold-state deferral removed" (both modes);
- a probe: the candidate plus a default frontend that attaches H at init, as a C session does.

The rows are the 12 identity rows plus the 58,633,530-variable Tier D row `1c21a43a…-9.normalised`. That is
the Rank-5 input whose memory motivated the deferral (`35aca7326`). Sources: `refs/wip`-archived commits under
`benchmarks/runs/20261010-rmdiag-pins`; pins `build/pin-rmdiag-{a,cand,probe}-rel`.

**`MaterializeColdState`: kept.** Removing the deferral is search-identical: the candidate matches c2.4 in verdict,
conflicts, decisions and certificate on all 13 rows. It does regress peak RSS on
the large-variable row. The increase is deterministic: three binaries of the deferring code all give exactly
15,325 MiB.

| Row | c2.4 | Part A (auto / affine) | Candidate, deferral removed (auto / affine) |
|---|---|---|---|
| `1c21a43a…-9.normalised` (58.6M vars) peak RSS MiB | 15,325 | 15,325 / 15,325 | **15,538 / 15,538 (+213, +1.4%)** |
| same, wall s | 60.47 | 59.53 / 59.66 | 60.13 / 62.57 |

On the other rows the candidate is within −29…+7 MiB, and its wall times are within allocation noise.

**`AttachHeuristics`: kept.** Owning H from init is not byte-identical, and it costs memory:

- `combined-crypto1` has the same verdict, conflicts (75,764) and decisions, but a different certificate
  (`3eaee0f2ba5a` vs `134984e88e13`).
- The large row peaks at 17,945 MiB (+2,620 MiB, +17%).

Two mechanisms make an attached H observable before the shipped attach point:

- Startup propagation and early Lucky write assignment-time saved phases into it and deactivate root
  assignments in trail order.
- DefaultPolicy's residual activation handoff seeds only an unattached H. `test_startup_support_signature`
  scenarios 3 and 4 show the handoff running without a borrowed H and skipped with one.

## Part B mechanics

| Row | Verdict | Conflicts | Certificate | c2.4 | A auto | A affine | cand auto | probe auto |
|---|---|---|---|---|---|---|---|---|
| ramsey_4_4_19 | UNSAT | 0 | `d169d9686c94` | 0.17/12 | 0.17/12 | 0.17/12 | 0.17/10 | 0.17/10 |
| lightsout_24 | UNSAT | 1 | `6fbc0514e27a` | 0.67/182 | 0.62/182 | 0.62/182 | 0.62/181 | 0.62/182 |
| mp1-Nb7T42 | SAT | 638,214 | `3b6b38a3bfa0` | 56.9/238 | 56.4/231 | 56.9/238 | 56.3/217 | 56.2/225 |
| battleship-14-26 | SAT | 665,487 | `fbedb083879e` | 15.1/91 | 15.0/91 | 15.1/80 | 14.5/91 | 15.1/80 |
| oddball_53_5_tto | SAT | 0 | `ba97acaa4dc6` | 5.78/896 | 5.68/896 | 5.63/896 | 5.68/867 | 5.58/868 |
| abw-K-dwt__234 | SAT | 0 | `1026936d5617` | 20.6/72 | 21.5/72 | 21.1/93 | 21.5/74 | 22.4/75 |
| cliquecolouring_n13 | UNSAT | 1 | `ed71148e8802` | 0.07/13 | 0.07/13 | 0.07/13 | 0.07/13 | 0.07/13 |
| 9vliw_bp_mc | UNSAT | 198,066 | `438781f64b25` | 10.7/81 | 10.7/81 | 10.6/81 | 10.6/77 | 10.7/78 |
| combined-crypto1 | SAT | 75,764 | `134984e88e13` | 2.17/36 | 2.17/36 | 2.17/34 | 2.17/30 | 2.17/30 (cert `3eaee0f2ba5a`) |
| sted5_0x24204-50 | SAT | 2,333,179 | `83c48a7431f7` | 128.9/46 | 128.9/46 | 128.0/55 | 127.4/53 | 127.2/46 |
| noL-11-16 | SAT | 1,180,890 | `190aa7fb722c` | 30.8/40 | 31.2/41 | 30.8/44 | 30.2/40 | 31.1/41 |
| b18 | UNSAT | 3,331,178 | `df61ef101a9d` | 217.9/265 | 215.8/263 | 218.5/263 | 214.7/266 | 217.8/272 |
| 1c21a43a…-9.normalised | SAT | 0 | `d2f314b1e7e0` | 60.5/15,325 | 59.5/15,325 | 59.7/15,325 | 60.1/15,538 | 62.5/17,945 |

Cells are wall s / peak RSS MiB. Every result is valid: UNSAT certificates verified, SAT models checked.

## Identity of the final code

The final source differs from the tested Part A source (`refs/wip/rmdiag-a`, `3fea35ab`) only in two comment lines,
with the same line counts. Its Release `tactix`, built from the archive of validation tree `7fd03d35`
(`benchmarks/runs/20261010-rmdiag-final-source`, job 1767538), is byte-identical to the measured pin `9e1c52b4…`.

That binary is ALL-IDENTICAL to c2.4 on the 12 identity rows plus the large row: same verdict, conflicts, decisions
and certificate SHA-256 in both `--auto-proof` and `--default-affine-search=on`.

## Signatures (`benchmarks/runs/20261010-rmdiag-signatures`, job 1766701)

Release test binaries were built from the archives of `668797c59` and the candidate tree. The candidate has the
final tree's signature tests, and its fixtures (which never defer) run the final tree's search. Results:

- **`test_default_execution_signature`.** Profiles 0 and 2–5 have byte-identical retained artifacts (CNF, DRAT,
  model) and identical stdout signatures: proof hash, work counters, trail, H phases and scores. Profile 1 changes,
  as intended: the ranked-trail target is gone.
- **Feedback mode.** Streams for profiles 0 and 2–5 are equal after dropping timings, sequence numbers, the header
  config snapshot, the observation's `external_branching` field and the candidate-only removal of the
  `MaterializeColdState` record, which the final tree keeps (`compare_feedback.py`, `compare_feedback.txt`).
- **`test_controller_execution_signature`.** 56 deterministic artifacts and stdout are byte-identical; the `.trace`
  files differ only in `elapsed-nanos`.
- **`test_random_controller_policy`.** Stdout is identical.
- **`test_inprocess_execution_signature` and `test_startup_support_signature`.** Artifacts and stdout are identical.

## Full Debug

`tools/bench/full_debug_archive_campaign.py` ran on the archived source of validation tree `7fd03d35`
(`benchmarks/runs/20261010-rmdiag-final-source/pins/source-pin.json`) as `benchmarks/runs/20261010-rmdiag-full-debug`,
job 1767537: **361/361 passed**, and the harness verified the archive after the run. The Debug build tree was deleted.
The doc checks (`tools/docs/check-*.py`) pass.
