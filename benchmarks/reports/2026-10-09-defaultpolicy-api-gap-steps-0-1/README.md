# DefaultPolicy public-API gap: Steps 0 and 1

Steps 0 and 1 of the [gap plan](../2026-10-09-defaultpolicy-api-gap-plan/README.md).
The baseline is c2.3 = `555ecb4b0` (Release `ae4a26e4…`, `build/pin-candidate-c2.3-rel`).

| Step | Commit | Release (identity pin) |
|---|---|---|
| 0: route ratchet, replay reporter, realized analysis target | `0e4a27760` | `10ff3abf…` |
| 1: macro actions through registry dispatch | Step 1 commit (validated at `bf56595d6`) | `44db8f24…` |

## Route ratchet (`tests/test_default_action_map.cpp`)

The map in `tests/default_action_map.hpp` classifies every DefaultPolicy feedback record
on 19 fixtures. These are the six execution-signature profiles × pigeonhole 4 and 6, the
four `tests/fixtures` CNFs, a graph-labeling permutation, and a post-structured-BCE
fixture. Each record maps to one route: **Public** (registry dispatch or an online
invocation), **Shared** (a typed request with a declared public equivalent) or **Kernel**
(no public request yet). The test checks dispatch empirically: a Public record must
advance `strategy_epoch` whenever it applies something, and an internal record that
names a registry rule must not.

| | Public entries | Internal entries | Public records | Internal records |
|---|---|---|---|---|
| Step 0 | 5 | 24 | 223 | 6,480 |
| Step 1 | 15 | 14 | 641 | 6,062 |

The ten entries that moved are `restart-scheduled`, `reduce`, `switch-mode`, `reorder`,
`rephase`, `walk`, `lucky-opportunity`, `lucky-graph-session`, `local-search-guidance`
and `bce-post-structured`.

The remaining internal entries are:

- **Search interval:** `propagate-search`, `decide` and `conflict-analysis`.
- **Root uses:** `propagate-root`.
- **Affine:** `affine-matrix-initialize` and `affine-matrix-search`.
- **Slot exceptions:** `slot-sweep-batch`, `slot-substitute-rounds`, `slot-vivify-band`
  and `slot-fastel-occurrence`.
- **Installs:** `install-parameters`, `install-walk-guidance` and
  `install-material-settings`.
- **Support:** `support-transaction`.

Two corrections to the plan inventory (A10):

- Dosed BVE candidates already dispatch through the registry. They go via the shared
  `BveInvocationSession::apply_one` → `apply_rule_instance`.
- Registered slot rules already dispatch through the registry. They go via
  `RegisteredRuleRequest`.

## Replay reporter (`tests/test_default_public_replay.cpp`)

The reporter replays a reference DefaultPolicy stream on a second C-API solver. It uses
`tactix_apply_instance` and `tactix_decide`, and applies the leading `InstallRuleParameters`
profile at load. It is report-only.

At Step 1 it replays 264 of 6,067 records. Every non-trivial stream stops at the first
`MaterializeColdState` support record, which belongs to Step 4. Small SAT fixtures
replay to the end of the stream.

Blocking entries over all streams:

| Entry | Records | Plan step |
|---|---|---|
| `conflict-analysis` | 851 | 6 |
| `support-transaction` | 187 | 4 |
| `structured-root-online` | 184 | replay of online invocations not yet implemented |
| `slot-fastel-occurrence` | 128 | 5 |
| `walk` | 26 | scoped Walk parameters are not replayable through `tactix_apply_instance` |

ConflictAnalysis feedback now carries the realized target:

- `;target=T;jump=J` when the analysis backjumps;
- `;target=T;no-backjump=1` for conflict reuse and level-one unit batches.

## Byte identity

Full Debug ran on archived sources through `tools/bench/full_debug_archive_campaign.py`:

| Step | Campaign | Job | Result |
|---|---|---|---|
| 0 | `benchmarks/runs/20261009-apigap-s0-full-debug` | 1762789 | 355/355, 0 skipped |
| 1 | `benchmarks/runs/20261009-apigap-s1-full-debug` | 1762790 | 355/355, 0 skipped |

Execution-signature tests (`test_default_execution_signature`,
`test_startup_support_signature`, `test_inprocess_execution_signature` and
`test_controller_execution_signature`) were compared with c2.3. Verdicts, DRAT bytes,
models, search counters, trails, phases and scores are identical. The only differences are
the serialized `strategy` epoch and `rule_calls` fields.

The three-arm SLURM identity check ran on `cpu-dense-hwulab-q` with 8 CPUs, 32 GiB and a
600 s solve limit through `runtime_task.py`. The arms are c2.3 `--auto-proof`, the step's
Release `--auto-proof`, and the step's Release with `--auto-proof --default-graph-labeling=on`.
They ran sequentially in one allocation:

- Step 0: `benchmarks/runs/20261009-apigap-s0-identity`, job 1761709;
- Step 1: `benchmarks/runs/20261009-apigap-s1-identity`, job 1762792.

In both campaigns all 12 rows are identical in verdict, conflicts, decisions and certificate
SHA-256, and every certificate or model is valid. Wall times (c2.3 / Step 1 / Step 1 graph
labeling on) are in seconds.

| Row | Verdict | Conflicts | Decisions | Certificate | Wall (s) |
|---|---|---|---|---|---|
| ramsey_4_4_19 | UNSAT | 0 | 0 | `d169d9686c94` | 0.1 / 0.1 / 0.1 |
| lightsout_24 | UNSAT | 1 | 0 | `6fbc0514e27a` | 0.7 / 0.6 / 0.6 |
| mp1-Nb7T42 | SAT | 638,214 | 15,456,796 | `3b6b38a3bfa0` | 58.6 / 57.8 / 60.2 |
| battleship-14-26 | SAT | 665,487 | 1,681,225 | `fbedb083879e` | 15.0 / 15.0 / 14.9 |
| oddball_53_5_tto | SAT | 0 | 0 | `ba97acaa4dc6` | 6.1 / 6.2 / 6.1 |
| abw-K-dwt__234 | SAT | 8,935 | 44,009 | `97eed2dcb01a` | 21.0 / 23.5 / 23.1 |
| cliquecolouring_n13_k8_c7 | UNSAT | 1 | 0 | `ed71148e8802` | 0.1 / 0.1 / 0.1 |
| 9vliw_bp_mc | UNSAT | 198,066 | 2,279,994 | `438781f64b25` | 10.9 / 10.7 / 10.8 |
| combined-crypto1 | SAT | 75,764 | 172,719 | `134984e88e13` | 2.2 / 2.2 / 2.2 |
| sted5_0x24204-50 | SAT | 2,333,179 | 5,655,352 | `83c48a7431f7` | 129.8 / 131.4 / 130.5 |
| noL-11-16 | SAT | 1,180,890 | 3,131,471 | `190aa7fb722c` | 30.8 / 30.9 / 31.5 |
| b18 | UNSAT | 3,331,178 | 49,260,596 | `df61ef101a9d` | 221.6 / 216.0 / 217.2 |

The three long rows exercise many macro actions:

| Row | Restarts | Reductions | Mode switches | Rephases | Walks | Reorders |
|---|---|---|---|---|---|---|
| sted5_0x24204-50 | 79,658 | 230 | 49 | 31 | 10 | 20 |
| noL-11-16 | 37,264 | 146 | 37 | 26 | 9 | 15 |
| b18 | 98,277 | 292 | 49 | 39 | 13 | 25 |

All of these counters are identical across the arms.

**Timing.** The same-node pairs differ by noise-level amounts: sted5 +1.2%, b18 −2.5%,
noL-11-16 +0.4% and mp1 −1.5%.

abw-K-dwt is 2.3–2.5 s slower in both Step 0 and Step 1, which rules out the Step 1
dispatch as the cause. The slowdown lies entirely in graph-labeling continuation slices,
which this work does not change, and every non-timing counter is identical. Earlier
identity campaigns show the same row between 20.9 and 24.4 s across behavior-identical
binaries (`benchmarks/runs/20261009-cleanup-identity-i{1,2,3}`), and c2.3 is the fast
extreme. We attribute it to code layout, not mechanism; it is not further profiled here.

## Telemetry changes (allowed counters only)

- **`strategy_epoch`.** It now advances once per macro action. This changes every later
  feedback and observation `state.strategy`, `last_action` state versions, and serialized
  observations in the signature tests. No certificate, model or search counter changed.
- **`rule_calls` and `rule_nanos`.** Lucky, Restart, Reduce, SwitchMode, Reorder, Rephase,
  Walk, BCE and LocalSearchGuidance are now charged. The `c timing` table therefore
  attributes graph-labeling time to Lucky: on abw-K-dwt, 22.2 s in 76 applications that
  were previously unattributed.
- **`reduce_scans`, `reduce_scan_words` and `reduce_skippable_words` (`c redscan`).** The
  rule now ranks candidates with `measure=true` on every reduction. On sted5 the count
  goes from 0 to 230 scans.
- **Reduce feedback record.** The default reduction is ramped (`ramp_fraction = true`), so
  the policy's count is the Reduce victim-count instance. This changes:
  - the record instance, from canonical to the count;
  - its invocation, which now carries the scoped `ReduceParameters` and the instance;
  - `native_policy_state` `last-native`.

  The ramped path re-ranks candidates inside the rule after the policy ranks them to
  choose the count, at the cost of one extra candidate scan per reduction.
- **ConflictAnalysis feedback description.** It now carries the realized target (Step 0).
- **Rule-journal builds.** Lucky, Restart, Reduce and BCE records now come from registry
  dispatch with the instance as the candidate string. `default_policy_rule_journal`
  passes.

  `test_rule_journal` fails in a journal build (`proof_rules == 24`). This is a stale
  registry count; neither step touches the registry, so the failure predates them. The
  journal configuration is outside full Debug.
