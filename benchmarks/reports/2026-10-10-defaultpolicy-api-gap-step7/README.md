# DefaultPolicy public-API gap: Step 7 (action port, public twin, §8.4 overhead)

This is Step 7 of the [gap plan](../2026-10-09-defaultpolicy-api-gap-plan/README.md), covering
D1 and finding 6. It builds on Steps [0–1](../2026-10-09-defaultpolicy-api-gap-steps-0-1/README.md),
[2](../2026-10-09-defaultpolicy-api-gap-step2/README.md),
[3](../2026-10-09-defaultpolicy-api-gap-step3/README.md),
[4a](../2026-10-09-defaultpolicy-api-gap-step4/README.md), 4b (`daffc2623`), 6 (`d1cf91ecd`)
and 5 (`123cf44fb`). The identity baseline is c2.4 = `3361d3b51` (Release `57bdd51c…`).

| | Commit | Notes |
|---|---|---|
| Step 7 code | `d56c7f9bc` | Release `4ee060d8…` (`build/pin-apigap-s7-rel`, from the archive) |
| Evidence | `1c677c97d` | identity, full Debug and overhead on `d56c7f9bc` |
| Scope closure | `50c619192` | Release `d540ef32…` (`build/pin-apigap-s7-scopes2-rel`, from the archive) |
| Scope evidence | this record | object identity vs `4ee060d8…`, identity and full Debug on `50c619192` |

Development ran in extracted copies (`benchmarks/runs/20261010-apigap-s7-source`): an overlay of
new files plus scripted edits (`apply_step7.sh`) rebased onto `1caa7af83`/`daffc2623` (4b),
`673e85a0b`/`d1cf91ecd` (6), `123cf44fb`/`a5fbb4125` (5) and `ce705cddb` (6 follow-up), with
SLURM builds and tests at each base.

## What changed

**One action port** (`src/strategy/action_port.hpp`). `default_policy.cpp` leaves its translation
unit only through `port::` funnels: `execute_action` (typed executor), `complete_action`
(feedback wrapper), `apply_rule*` (registry dispatch), `propagate_apply` (M1), `InprocessPhase`
(the 4b bracket), `affine_matrix_search_{initialize,refresh,step,release}`,
`subsume_forward_try_fastel`, `bve_apply_one` and `execute_substitute_rounds`. The rename is
mechanical (`port_sed.sh` in the campaign source directory) plus one `#include` on a blank line;
two policy edits replace inline state changes with shared requests (the fastel prefix compaction
and the slot-boundary retirement publication).

- **StaticPort** (production) binds the funnels to the registered functions with using-declarations
  and trivial inline forwarders. The production `.text` built from the same tree with and without
  the port is identical (on 4b, `tactix` itself was byte-identical, `21f203b6…`; on Step 5, against
`123cf44fb` with the same additive API edits but no port, `.text` is identical and only
`default_policy.cpp.o` differs outside `.text`).
- **PublicPort** (`tests/default_public_port.hpp`) runs DefaultPolicy on a bound C session under
  NATIVE guidance publication and issues every action through `tactix.h`: registry dispatch as
  `tactix_apply*` (including Propagate, the score-heap BVE candidates in the session's heap order,
  Lucky 26/27 and the fastel instance), Decide as `tactix_decide`, installs, supports (including
  the phase bracket, root preparation and warm-up, whose arguments come from the enclosing
  record's instance), the affine profile and operations, and Walk, Sweep and Substitute as one
  online step each. Anything else runs through StaticPort and is counted (ratcheted); a refused
  public request (−1, no state change) is counted as declined.
- **HardcodedPort** calls the registered rule functions without dispatch accounting (same
  startup-boundary closure, semantic-variable bracket and strategy epoch; no trace scope, rule timer
  or `rule_calls`): the §8.4 hardcoded twin, built as `tactix-hardcoded` from the same objects.

**Additive public surface** ([controller interface](../../../docs/controller-interface.md)):

- **Affine services** (the startup root closure is on by default; the persistent search was kept by
  user decision): the `TACTIX_AFFINE_MATRIX_COMPLETE_SNAPSHOT` admission (the root closure's
  non-persistent INITIALIZE/SEARCH_STEP, zero persistence), `tactix_install_affine_matrix_profile()`,
  `tactix_read_affine_matrix_result()` (the last public application's typed result; the full result
  is session observation) and `TACTIX_SUPPORT_RELEASE_AFFINE_MATRIX` (23).
- **Fastel:** SubsumeForward instance `TACTIX_SUBSUME_FORWARD_FASTEL` (clause start, occurrence
  limit) inside the Eliminate occurrence session, and `TACTIX_SUPPORT_COMPACT_FASTEL_PREFIX` (24),
  the scan's occurrence-image compaction.
- **Retirement hold:** `TACTIX_SUPPORT_HOLD_RETIREMENTS` (25; HOLD, RELEASE, RELEASE_ONLY), Kissat's
  post-Sweep union published after the follow-up Substitute.
- **Substitute rounds:** the online Substitute round batch reports a round-effort break as an
  incomplete selection.
- **Typed results as observation:** the graph-labeling session keeps the last public Lucky 26/27
  result; `BveInvocationSession::apply_one_with` lets a port dispatch the candidate.
- **`COMPACT_VARIABLES` at any consistent state:** DefaultPolicy compacts during Reduce above the
  root (twin and replay both found the refusal).

**Twin test** `tests/test_default_public_correspondence.cpp` (ctest `default_public_correspondence`).
One source is built three times, once per port.

- The public executable runs the static and hardcoded executables and its own arm on 19
  fixtures:
  - the 17 replayable action-map fixtures: profiles 0–5 × pigeonhole 4/6, the four
    `tests/fixtures` CNFs and the graph-labeling permutation;
  - an odd K4 Tseitin system, which the startup affine root closure refutes;
  - the mixed pigeonhole/XOR fixture of `test_default_affine_search` with affine search on.
- It requires equal values for: verdict, certificate bytes (DRAT; DSR for the affine fixtures),
  model, conflicts, decisions, propagations, ticks, learned clauses and literals, restarts,
  reductions, mode switches, walks, roots, eliminations, affine initializations and services,
  final level, trail, and H's saved phases and scores.
- It then reports the ratchet's internal entries the fixtures reach and the PublicPort fallbacks.
  Fallbacks are ratcheted both ways: a new kind fails, and a kind that disappears must be removed
  from the list. The complete correspondence is declared once neither remains.

**Replay gate.** `test_default_public_replay` is now a gate.

- Gate conditions:
  - no public record may diverge or be refused;
  - H must match at every measured stop under NATIVE publication;
  - every public install must reproduce its profile;
  - with no blocking entry left, every stream must replay to its end.
- Fixes to the replay itself, needed once it ran past Step 6's analyses and Step 5's slots:
  - a Reduce record replays under the scoped ReduceParameters it carries;
  - a fastel record replays as the public fastel instance;
  - an online step with an unlimited budget takes its rule's work unit, since the record keeps the
    feedback default.

**Ratchet.**

- Now Public, because the twin proves the public request:
  - the micro routes propagate-search, propagate-root and decide;
  - the three affine entries;
  - slot-fastel-occurrence.
- New route `Diagnostic` for off-by-default arms outside the claim: the installed-branching
  warm-up and the ranked-trail analysis target. Step 5 had already declared the injected
  FactorMatchPolicy arm.

**§8.4 overhead harness.** `tools/bench/port_overhead.py` prepares a `runtime_task.py` campaign
with perf counters (`collect_runtime_context`) and checked certificates.

- Each array task runs both arms of one instance in one allocation on the same CPU, alternating
  the order by instance parity.
- `summarize` reports per-instance and aggregate static/hardcoded ratios of instructions, cycles
  and solve wall, plus certificate identity between the arms.

## Twin test

Final tree (`ce705cddb` + Step 7), SLURM Release (job 1764068) and Debug ASan/UBSan:

| | Count |
|---|---|
| Fixtures | 19 (17 action-map + 2 affine) |
| Static vs public mismatches | 0 |
| Static vs hardcoded mismatches | 0 |
| Public requests through `tactix.h` | 6,934 |
| Static fallbacks | 0 |
| Declined public requests | 0 |
| Ratchet internal entries reached | 0 |
| Verdict | `correspondence: COMPLETE` |

Public requests by entry: `tactix_apply` 2,480 (Propagate and registry rules), Propagate refused
at a fixpoint 224 (a no-op on both routes), `tactix_decide` 1,453,
`tactix_apply_semantic_instance` ConflictAnalysis chrono 845, `tactix_install_parameters` 816,
`tactix_support` 520 (supports, phase bracket, root preparation, warm-up, fastel compaction,
retirement hold), fastel instances 374, `tactix_apply_instance` 311, BVE candidates 61, Walk
online steps 26, Lucky 26/27 23, material settings 17, AffineMatrixInitialize 4, Sweep and
Substitute online steps 2 each. Run statically by declaration: the ranked-trail ConflictAnalysis
arm (156, profile 1), `MaterializeColdState` (9, declared non-action) and the publication wrapper
around composed applications (61). The affine fixtures run the startup root closure (odd K4
Tseitin: one initialization, one refuting service step) and the persistent search (mixed
pigeonhole/XOR: initialization and startup service). `sweep-retarget-lrat` runs a productive Sweep
(17 equivalences) and its follow-up Substitute (9 variables substituted) under the public
retirement hold, with H's exact layout identical.

## Replay gate

Final tree: 5,462 of 6,366 records replayed (Step 6 alone: 3,076; Step 4b: 1,154). The gate passes:
every strict stop is at a record without a public request. The only such stop is the declared
ranked-trail arm (profile 1); profiles 0 and 2–5, the fixture CNFs and the graph-labeling
permutation replay to the end. Records replayed: 691 installs, 245 supports, 255 online steps
(31 inprocessing slots), and 695 chrono ConflictAnalysis records (11 with an OTFS retier). H at
the stop matches under NATIVE publication on all 15 nonempty streams.

## §8.4 overhead: StaticPort vs HardcodedPort

`tactix` (StaticPort) against `tactix-hardcoded` (HardcodedPort), 67 instances (Rank1–50 and
B28+C5 rows the c2.4 gate campaigns solved within 300 s), both arms same allocation and CPU, perf
counters and checked certificates through `runtime_task.py`. Ratio = static / hardcoded.

| Build | Instances (identical certificates) | Instructions aggregate / geomean / max | Cycles aggregate / geomean | Wall aggregate |
|---|---|---|---|---|
| 4b base (`20261010-apigap-s7-overhead-1caa`) | 67 (67) | 1.0001 / 1.0005 / 1.0093 | 0.9907 / 0.9944 | 0.9908 |
| Step 5 base (`20261010-apigap-s7-overhead-a5fb`) | 66 (66) | 1.0021 / 1.0017 / 1.0360 | 1.0058 / 1.0094 | 1.0056 |
| **Step 7 commit** (`20261010-apigap-s7-overhead-final`, pins `4ee060d8…` / `95d3ebc6…`) | 67 (67) | 0.9985 / 1.0008 / 1.0610 | 0.9842 / 1.0040 | 0.9828 |

The overhead is within the ≤5% bound. On the commit every one of the 67 instances has identical
certificates between the arms. One row stands out: `hwmcc17miters-xits-iso-oski15a08b00s`, a 4–5 s
row, is +6.1% instructions on its own (ratio 1.0610), while the aggregate over all 67 rows is
−0.15% (0.9985). The Step 5 rise is BVE candidates and fastel
instances, which now go through registry dispatch accounting.

## Byte identity (12 rows × 3 arms)

Campaign `benchmarks/runs/20261010-apigap-s7-identity`, job 1764072, cloned from the Step 6 campaign: `cpu-dense-hwulab-q`, 8 CPUs, 32 GiB, 600 s solve limit, `runtime_task.py`. The three arms run sequentially in one allocation: c2.4 `--auto-proof` (`57bdd51c…`), Step 7 `--auto-proof` (`4ee060d8…`, `build/pin-apigap-s7-rel` from the archive of `d56c7f9bc`), and Step 7 with `--default-affine-search=on`. All 12 rows agree in verdict, conflicts, decisions and certificate SHA-256 across the three arms, and every certificate or model is valid.

| Row | Verdict | Conflicts | Decisions | Certificate | Wall (s): c2.4 / affine on / Step 7 |
|---|---|---|---|---|---|
| ramsey_4_4_19 | UNSAT | 0 | 0 | `d169d9686c94` | 0.167 / 0.167 / 0.167 |
| lightsout_24 | UNSAT | 1 | 0 | `6fbc0514e27a` | 0.768 / 0.718 / 0.818 |
| mp1-Nb7T42 | SAT | 638,214 | 15,456,796 | `3b6b38a3bfa0` | 65.325 / 65.683 / 65.3 |
| battleship-14-26 | SAT | 665,487 | 1,681,225 | `fbedb083879e` | 15.243 / 15.096 / 15.041 |
| oddball_53_5_tto | SAT | 0 | 0 | `ba97acaa4dc6` | 5.676 / 6.026 / 5.678 |
| abw-K-dwt__234 | SAT | 0 | 0 | `1026936d5617` | 22.355 / 21.955 / 21.007 |
| cliquecolouring_n13_k8_c7 | UNSAT | 1 | 0 | `ed71148e8802` | 0.067 / 0.067 / 0.067 |
| 9vliw_bp_mc | UNSAT | 198,066 | 2,279,994 | `438781f64b25` | 11.035 / 10.785 / 10.757 |
| combined-crypto1 | SAT | 75,764 | 172,719 | `134984e88e13` | 2.17 / 2.17 / 2.17 |
| sted5_0x24204-50 | SAT | 2,333,179 | 5,655,352 | `83c48a7431f7` | 130.847 / 129.025 / 128.591 |
| noL-11-16 | SAT | 1,180,890 | 3,131,471 | `190aa7fb722c` | 30.769 / 30.805 / 31.269 |
| b18 | UNSAT | 3,331,178 | 49,260,596 | `df61ef101a9d` | 230.524 / 219.344 / 220.545 |

abw-K-dwt and oddball report 0 conflicts because startup routines solve them. Wall times are within noise. The single outlier is c2.4's own b18 arm at 230.5 s, against 219–221 s for the Step 7 arms, which is a per-allocation effect: every counter is identical.

## Full Debug

`tools/bench/full_debug_archive_campaign.py` ran on the archived source of `d56c7f9bc`
(`benchmarks/runs/20261010-apigap-s7-final-source/pins/source-pin.json`) as
`benchmarks/runs/20261010-apigap-s7-full-debug`, job 1764252: **361/361 passed**. That is Step 5's
360 plus `default_public_correspondence`, and the harness verified the archive after the run. The
Debug build tree was deleted afterwards. Port-relevant Debug tests also passed at every
intermediate base.

## Ratchet

| | Public | Internal | Declared/diagnostic | Public records | Internal records | Declared records |
|---|---|---|---|---|---|---|
| Step 7 | 51 | 0 | 5 | 6,917 | 0 | 165 |

The base `ce705cddb` listed 9 internal entries: `propagate-search`, `decide`, `propagate-root`,
the three affine entries, `slot-fastel-occurrence`, and the ranked-trail and installed-branching
warm-up arms.

Reclassified Public by this step:
- `propagate-search`, `propagate-root` and `decide`: static instantiations of the registered
  functions, proven by the twin;
- the three affine entries;
- `slot-fastel-occurrence`;
- `walk`, which now replays as an online step.

Declared: `materialize-cold-state` and `attach-heuristics` (non-actions); the injected
FactorMatchPolicy, installed-branching warm-up and ranked-trail arms (diagnostics).

## Scope closure (`50c619192`)

DefaultPolicy holds a scope around some of its public requests. Before this commit those scopes wrote
kernel state directly in every binary, PublicPort's included. They are now port types
(`port::ScopedReduceParameters`, `BveInvocationSession`, `WorkSettingsScope`,
`ScopedRuleWorkBudget`, `VivifyDoseScope` and `TickSinkScope`). `default_policy.cpp` changes
names only, on the same lines, and production binds the same types with using-declarations.
PublicPort establishes each scope through `tactix.h` alone.

| Scope | Encloses | PublicPort's public form |
|---|---|---|
| Reduce tier, victim and compact profile | Reduce dispatch | `tactix_install_parameters` (Reduce), restored the same way |
| BVE pass profile (`BveInvocationSession`) | score-heap BVE candidates | `tactix_install_parameters` (BVE) for the session's lifetime; the new `CallerPosed` constructor validates without posing |
| Work settings: budgeted batch (Sweep, Substitute, SubsumeForward midround, …), BVE candidate residual | the batch or candidate dispatch | `tactix_install_work_settings`, restore value from `tactix_read_work_settings` |
| Rule work budget (graph-labeling slice) | Lucky 27 | the same calls, with the online step's rule-budget wires |
| Vivify dose and bands | Vivify bands | the same calls plus `tactix_set_vivify_scope` |
| Tick sink (Probe, HBR, Backbone, Vivify, Instantiate currencies) | the metered dispatch | `tactix_install_tick_attribution` |

The work-settings and tick-attribution entries are additive. They live in a translation unit of
their own (`src/api/api_work.cpp`), and neither entry bumps the strategy epoch, as with
`tactix_install_material_settings`. A typed profile's restore value is read from the session's
kernel, because no typed readback exists; every write goes through a public call. The Vivify band
form is compiled and type-checked, but no fixture reaches a Vivify band.

**Twin and tests on `50c619192`.** The twin is `correspondence: COMPLETE` on 19 fixtures with 0
mismatches, 0 fallbacks and 0 declined, over 7,248 public requests. The 314 added since
`d56c7f9bc` are:
- 142 tick-attribution installs;
- 18 work-settings installs and 14 reads;
- 140 typed installs for the Reduce and BVE scopes.

Replay (5,462 of 6,366 records), `test_api_support` (with a work-settings and tick-attribution
case) and the ratchet (51/0/5) pass.

**Object identity against the `4ee060d8…` build** (`build/pin-apigap-s7-rel`, read-only), both
built from archives with the same flags:

- All 152 existing production objects are byte-identical, including `default_policy.cpp.o` and
  `api.cpp.o`: 151 `tactix_lib` objects plus `main.cpp.o`. The single new object is `api_work.cpp.o`.
- The binary's hash changes (`d540ef32…`; hardcoded twin `2c079d6a…`), because the new object's
  code is inserted into `.text` and shifts the layout behind it by 0x200 bytes.
- Every pre-existing symbol keeps its size. Disassembly of `.text` with addresses and RIP
  displacements normalized differs only by the three new functions (148 lines, one block).
- So the §8.4 gates measured on `4ee060d8…` run the same machine code as the final binary.
- A first attempt defined the entries in `api.cpp`. That changed `api.cpp.o`'s code generation,
  including the weak `AffineMatrixSearchSession::disable` copy the linker kept, which DefaultPolicy
  reaches. Moving the entries out removed the change.

**Identity and full Debug on `50c619192`:** campaign
`benchmarks/runs/20261010-apigap-s7-scopes2-identity` (job 1764443) was cloned from the Step 7
identity campaign onto `d540ef32…`. All 12 rows × 3 arms (c2.4, the closure, the closure with
affine search on) are ALL-IDENTICAL in verdict, conflicts, decisions and certificate SHA-256 (the
same values as the table above), and every certificate or model is valid. Full Debug
(`tools/bench/full_debug_archive_campaign.py` on the archived source of `50c619192`,
`benchmarks/runs/20261010-apigap-s7-scopes2-full-debug`, job 1764455): **361/361 passed**; the
archive was verified after the run and the Debug build tree deleted.

## What remains

Nothing blocks the correspondence on the fixtures. Two items remain outside the claim:

- the declared diagnostic arms;
- the H comparison's sensitivity to the retirement hold. On these fixtures it is not shown to be
  necessary: the twin matched before the hold existed.
