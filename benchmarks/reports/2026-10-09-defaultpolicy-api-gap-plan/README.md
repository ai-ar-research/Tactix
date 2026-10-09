# DefaultPolicy as a client of the public action surface: gap inventory and refactor plan

Read-only analysis of the working tree at `99ac421d4` on 2026-10-09. Another agent is editing
`src/` concurrently, so line numbers in `src/policies/default_policy.cpp` (abbreviated `dp.cpp`)
may drift by about ten lines. Nothing was built or run. Every claim below comes from reading the code.
Anything that still needs measurement is marked "verify".

## 0. Findings in brief

1. **Most of DefaultPolicy already runs on shared rule code, but not on public requests.**
   `dp.cpp` issues 41 distinct typed executor requests, `execution::execute_action(k, XRequest)`
   (`src/strategy/*_execution.hpp`). The random controller issues one directly (`DecisionRequest`), and
   `online_inprocess.cpp` issues three. Each request reaches the same rule function that the registry
   reaches. Examples: Lucky's registry `lucky_apply_instance` (`rules/lucky/lucky.cpp:325-433`) and
   DefaultPolicy's `default_lucky_opportunity` (`dp.cpp:541`) both call
   `execute_action(LuckyRequest<X>)`. `Decide` and DefaultPolicy both call `decide_apply`
   (`rules/decide/decide.cpp:17`). The typed executor is internal C++, though, and not a public request
   surface. The public routes are:
   - registry dispatch `apply_rule*` → `dispatch_rule` (`rules/registry.cpp`, around 330-400), which
     closes the startup certificate, increments `strategy_epoch`, brackets semantic-variable
     candidates and charges `rule_nanos`/`rule_calls`;
   - `tactix_online_*` steps with typed parameters;
   - controller `ActionRequest`.

   This document uses three tiers:
   - **P (public):** reachable as a registry id plus an instance or semantic instance, as an online
     invocation with typed parameters, or as a controller `ActionRequest`.
   - **S (shared-typed only):** a typed `execution::*Request` that only native policies can issue. Many
     of these appear in feedback as `FeedbackKind::Support` records, but no client can request them.
   - **K (raw kernel):** direct `Kernel&` field writes, calls into rule-internal functions, kernel
     hooks/callbacks, and reads outside the `Observation` contract.
2. **The public API breaks correspondence on its own side.** The following functions all call
   `heuristics_discard_rule_events` (`api.cpp:2481, 2525, 2581, 2628, 3051`), which throws away
   fresh-variable demotions, substitution demotions and retirements, and never consumes new root
   assignments into H:
   - `tactix_apply`
   - `tactix_apply_instance`
   - `tactix_apply_condition`
   - `tactix_apply_semantic_instance`
   - `tactix_online_advance`

   DefaultPolicy consumes those same events at precise boundaries (`heuristics_consume_*`,
   `policies/rule_publication.hpp`). As a result, σ without H matches across routes, but H does not, and
   `tactix_decide_candidates` diverges after the first such event.
3. **The controller is not a pure client either.**
   - `random_controller_policy.cpp:2964-2967, 2989, 3040` calls `heuristics_consume_*` around
     `apply_rule`.
   - `:3020` does `++k.strategy_epoch` by hand after Decide.
   - `:3054` calls `k.affine_matrix_search_session->disable(k)`.
   - `RestartController` attaches H directly.

   The controller therefore needs the same publication contract.
4. **Kernel callbacks inject policy code into atomic rules.** DefaultPolicy installs code that runs
   inside rules:
   - `analysis_conflict_hook` retiers during OTFS analysis (`dp.cpp:4450`, `analyze.cpp:1467`);
   - `bve_score_update_hook` maintains the Kissat BVE heap (`dp.cpp:5936`, `kernel.hpp:2435`);
   - a `factor_selection` pointer (`guidance_execution.hpp:52`);
   - an `arena_ordering` pointer (`dp.cpp:4464`, off by default).

   Plan §5.1 says v1 has no callback injection, so no client can reproduce these.
5. **One hot-path transition cannot be expressed publicly.** Native ConflictAnalysis chooses its
   backjump target *after* learning: chrono if `back - jump > chrono_levels` (`dp.cpp:5208-5236`;
   `analyze.cpp:1189` keeps a separate `PublicTarget=false` specialization). The public instance fixes
   the target *before* analysis and clamps it (`analyze.cpp:1673-1696`). That is an N1 expressibility
   gap even though the step legitimately lives inside the search loop.
6. **The ≤5% hardcoded-twin gate has no harness.** I found no twin harness for plan §8.4's
   "dynamic-macro/static-micro overhead ≤5%" in `tools/` or `benchmarks/reports/`.

## 1. Inventory

Frequency classes:
- **H:** per propagation call or per decision.
- **C:** per conflict.
- **R:** per restart or reduce.
- **P:** per inprocessing phase, round or candidate.
- **S:** once per solve.

Boundary column:
- **in** = legitimately inside the native search interval (see §2).
- **x** = crosses an action boundary and should be closed.

### (a) Action execution outside the registered rule/action interface

| ID | Site | What it does | Freq | Public equivalent | Bnd |
|---|---|---|---|---|---|
| A1 | `default_propagate` dp.cpp:446-467 | `propagate_apply(k)` through `default_direct_rule` (408-428; journal-only adapter, no registry dispatch), then `heuristics_consume_root_assignments` | H (search loop 5134); P (6096, 6632); S (startup 4650, root collect, post-preprocess) | P: `Propagate` registry entry, same `propagate_apply` (propagate.cpp:78-82). Missing: H root consumption (finding 2), epoch/timing | in (search); x (root/inprocess uses) |
| A2 | Decide, dp.cpp:5583-5606 | `bp.pick(k)`, then `DecisionRequest{lit}` → `decide_apply(k,l,false)`, then `heuristics_note_target_decision` | H | P: `tactix_decide`/Decide instance → `decide_apply(...,true)`. Differences: `stats.api_decisions`, registry epoch/timing, and no target-decision note (which writes Observation fields `local_search_last_target_decision_*`) | in |
| A3 | ConflictAnalysis, dp.cpp:5150-5246 | `NativeConflictAnalysisRequest` → `analyze_apply` (`PublicTarget=false`); policy closure computes target after learning (chrono 5214-5216; `reuse_trail_level` 2078 under the off-by-default `chrono_reuse_trail_`), then `backjump_apply_to` | C | **None** for the learned-clause path. Public instance = pre-chosen target clamped to [jump, level-1]; canonical = jump with no chrono (analyze.cpp:1673-1696). Reuse paths already use `k.conflict_reuse_chrono_levels` (analyze.cpp:891-899) | in (inexpressible) |
| A4 | Scheduled Restart, dp.cpp:5351-5446 | keep = `bp.pick_for_restart_reuse` + `heuristics_restart_reuse_level_for` (5380-5381); `heuristics_update_target_phases` (5390); `RestartRequest{keep}` | R | P: Restart instance keep-level (restart.cpp:76-82), same `restart_apply_to`. Target-phase capture is not part of the public Restart | in (keep>0); x (keep=0 with affine/graph services) |
| A5 | Preparation restarts: solve entry 4470-4478; run_phase 6893-6909; `prepare_rephase_root` 1950-1969 | `RestartRequest{0, Preparation}` → `restart_backtrack_to` (no `stats.restarts++`) | R/P | Partial: only Restart(0), which counts as a scheduled restart | x |
| A6 | Lucky opportunities, dp.cpp:541-592, startup 4600-4800, early/late 4895-4910 and 5047-5061 | `LuckyRequest<X>` called directly behind policy guards (e.g. `config_.exact_cover_model.enabled && exact_cover_model_guard(k)`), with no `dispatch_rule` | S | P: Lucky instances 6-24 dispatch the same request. Caveat: instances 15/16/23/24 first call `inprocess_reach_root` (lucky.cpp:417); Default calls ParityProof (23) and ExactOneProof (24) without it (verify the root is already at a fixpoint there) | x |
| A7 | Post-structured BCE, dp.cpp:4855-4873 | `inprocess_reach_root(k)`, then `bce_apply(k)` directly | S | P: BCE canonical (bce.cpp:484) | x |
| A8 | Affine root closure 469-539; persistent search 3886-4230 | `affine_matrix_search_initialize/step` called directly; `session->disable()` (535, 4182) | S (closure on by default); R (search off by default) | Partial: `AffineMatrixInitialize` op with Operation ∈ {Initialize, Refresh, RootMaintenance, SearchStep} (affine_matrix_search.cpp:2048). No public disable | x |
| A9 | Graph labeling, dp.cpp:4293-4385 | `GraphLabelingActionRequest` (= Lucky 26/27); `local_search_guidance_apply_candidate` called directly | R | P: Lucky 26/27; LocalSearchGuidance instance = hint id (local_search.cpp:116-123). Only the registry wrapper is missing | x |
| A10 | Inprocessing slot runner: run_phase 6859-7312, run_batch_entry 6431-6638, run_budgeted_batch 6299-6429, run_dosed_entry 5783-6150, run_vivify_dose 6197-6297, fastel 5688-5772 | Mixed. Already P-routed: `RegisteredRuleRequest{r}` (6405, 6555), `apply_rule_instance` (6053, 6061). Typed only: `BveInvocationSession::apply_one` (6055); `SweepRequest` (6352); `SubstituteBatchRequest` with a policy round-continuation closure (6373-6400, 2044-2059); `VivifyBandRequest` with a cumulative ledger (6240). Direct calls into rule internals: `strengthen_reconnect_and_flush` (6620), `bve_flush_units_while_connected` (6104), `subsume_forward_try_fastel` inside `apply_rule_structured` (5746-5751) | P | Partial. Per-instance BVE, Sweep limits, Vivify scope and Substitute demotion are public typed parameters. Not public: Substitute max-rounds and per-round effort break, Vivify band ledger, Strengthen forward closure, fastel occurrence flush, BVE unit flush | x |
| A11 | Support transactions (S tier) | RootPreparation (6894, 1960); Disconnect/ReconnectLargeWatches (6945, 7249); OccurrenceSession (6978); FlushRetiredRedundant (7234); CollectGarbage (6137); SparseResume (594, 7262); SparseCollect (4988); CompactVariables (5308); CompactRandomOrder (5012); MaterializeColdState (4814, 4922); ResidualActivation/Import (4946, 4974); AttachHeuristics (4962); **Warmup** (5492, `guidance_execution.hpp:91-118`: `kernel_assign_decision` plus `kernel_propagate_beyond_conflicts`); ReductionPreparation (608, 5254); CloseStartupCertificate (4643); BeginBveDefinitionSession (6874); FactorSelectionInstallation (6951); ArenaOrderingInstallation (4464); Work/MaterialSettings scopes | S/P | **None requestable.** Feedback records them by name. Warmup, CompactRandomOrder and residual seeding are H-only guidance, which plan §4 requires to be API-exposed under N1 | x |
| A12 | Reduce, dp.cpp:5245-5323 | ReductionPreparation; `retier_current_mode` (5263 → `reduce_retier`); `pose_reduce_tiers` (5264); `reduce_candidates(measure=false)` (5272) with `ReductionPolicy::victims`; `ReductionRequest{span, compact_due}` (5292) where `compact_due` comes from `kernel_should_compact_variables` (5270); then CompactVariables | R | Partial. Reduce instance = victim count (reduce.cpp:363-389) runs `reduce_prepare` + `reduce_candidates(measure=true)` with the *installed* tiers and `k.reduce_compact_collection`. It has no per-call compaction, no retier/pose and no CompactVariables. The default `GlueTieredReduction` without ramp equals the canonical fractional count | x |
| A13 | SwitchMode 5327-5350, Reorder 5449-5465, Rephase/Walk 5467-5562 | `SwitchModeRequest`; `ReorderRequest{target}`; `RephaseRequest{type}`; `WalkRequest{allowance, seed, candidate}`. The seed is DefaultBranching's `randec_rng` (5515); the candidate comes from `local_search_guidance_select_next` (5507), which advances a cursor | rare | P-equivalent: registry ops with instances, and online Walk parameters (step budget, seed, seed_candidate). Only the dispatch route differs | x |

### (b) State reads outside the read-only observation contract

The contract is `Observation`/`observe_kernel` (`strategy/controller_observation.hpp:13-72`):
counts, the full `Stats`, candidates, and the policy's own `native_policy_state` string. Reads of
`k.stats.*` (restart, reduce, mode, rephase and reorder clocks in `default_restart.hpp` and
`default_periodic.hpp`) are covered by `Observation.cumulative`, so they are **not** gaps.

| ID | Site | Reads | Freq | Note |
|---|---|---|---|---|
| B1 | `DefaultBranching::pick_impl/decide_lit` 3195-3256; `start_random_sequence` 3258 | `k.h->stable/target/saved`; `heuristics_next_active_decision_var` (lazy-pops the heap, moves the queue cursor); `k.compact_import_order`, `k.values`, `kernel_var_retired` | H | H is policy-owned guidance hosted in the kernel. The public `tactix_decide_candidates` (heuristics.cpp:479-509) is const and stable-sorts by score, so its ties differ from heap-pop order, and it has no randec. Reading H is legitimate for its owner; the divergence comes from H *publication* (finding 2) |
| B2 | `reuse_trail_level` 2078-2121; `observe_affine_consumption` 4387-4427; root-consumption trail scans | `k.frames`, `k.trail`, `k.assigned[].reason`, `k.unit_ids`, arena clause ids, H scores | C/H (affine: after every search propagation, but AffineSearch is off by default) | Not observable |
| B3 | Analysis tail 5170-5236; `update_tiers_after_analysis` 3101-3130 | `k.pending.{valid,lits,glue,jump_level}`, `k.analysis_conflict_checkpoints` | C | Not observable. The completed transaction could return these as an action result |
| B4 | Classification 3153-3161; slot marks 6455-6458, 6420-6424; phase clock 6865; `factor_entry_declines` 6152-6191; vivify 6248; affine census 3899-3906 | `k.watches.binary_clauses()`, `k.live_irredundant_large`, `k.binary_ids.size()`, `k.arena.size()`, `kernel_logical_root_fixed`, `kernel_active_variables`, `k.factor_literals_marked`, `k.subsume_material_count`, `k.tier1==k.tier2`, `arena_generation`, `touch_epoch`, proof epochs | P/R | Scalar census. Cheap to add to `Observation` |
| B5 | `BveCandidateHeap::initialize` 2603-2640; `rank_bve_reference` 2882-2910; `rank_transitive_reference` 2538; fastel traversal 5688-5772 | `k.dense->large_size`, physical watches, `bve_build_occurrences`, H scores/stamps, `k.dense->ordered` | P | Physical representation. Belongs inside the rule, not in observation |

### (c) Scheduling-state writes into kernel fields

| ID | Site | Write | Freq | Typed/public route that exists |
|---|---|---|---|---|
| C1 | dp.cpp:5154-5156, reset 5173-5174 | `k.bump_reason_limit`, `k.bump_reason_depth` | C | `AnalyzeParameters.bump_reason_*`; C `tactix_online_analyze_parameters` (tactix.h:505-515), step-scoped only |
| C2 | 3167 | `k.jump_reasons_bigbig` (formula classification) | R/P | `AnalyzeParameters.jump_reasons_bigbig` / `enable_jump_reasons_bigbig` |
| C3 | 669-670, 3120-3121, 4551-4552; `reduce_retier` 687-694; `reduce_retier_from_histogram` 3114 | `k.tier1`, `k.tier2`, `k.tier{1,2}_by_mode` | R/P/C (OTFS) | `tactix_online_reduce_parameters.tier1/tier2` (scoped); no retier operation |
| C4 | 4450 (`analysis_conflict_hook`); 2618, 5936-5937, 6113-6116 (`bve_score_update_hook/state`, `bve_occurrence_count_image`); 6951 (`k.factor_selection`); 4464 (`k.arena_ordering`) | Kernel callbacks and pointers into policy objects | C (OTFS) / P | **None.** Plan §5.1: no callback injection |
| C5 | 5808 `k.bve_bound`; 7294 `k.bve_retry` fill; 6194/6412 `k.factor_composite`; 7009-7010 `k.subsume_complete/strengthen_complete`; 5081 `k.unflushed_root=0`; 4889-4893 `k.input_binary_order` clear; 4496 `k.graph_labeling_session.clear()`; 5764 `k.dense->ordered.compact`; 3128 checkpoint clear | Rule-state and lifecycle flags | P/S | Partial: BVE bound (legacy `tactix_set_bve_bound`, BveParameters), Factor composite (online factor parameters). The rest have none |
| C6 | Solve entry 4502-4547 (15× `ParameterInstallationRequest`, `WalkGuidanceInstallationRequest`); per-phase `install_parameters` 3601 | Permanent parameter installs | S/P | Partial: legacy `tactix_set_*`. Online parameters are step-scoped, not permanent |
| C7 | 42 policy-side `k.stats.*` writes (e.g. `chronological` 5216/5222, `random_decisions`, `lucky_*_calls`, `fastel_*`, `inprocess_*` timers, `rule_nanos/calls` 6352-6355) | Counters | various | Not trajectory, but `Observation.cumulative` exports them. Declare them policy-owned counters or move them into policy diagnostics |

### (d) Hot-loop CDCL internals inside search

D1 is the loop at `dp.cpp:5115-5606`: Propagate → (Conflict → Analysis) → {Reduce | SwitchMode | Restart |
Reorder | Rephase | Probe | Eliminate} → Decide. All of it uses static dispatch with no registry
timing or epoch. A1-A4, B1-B3, C1 and the C4 analysis hook live here.

## 2. Hot path versus boundary

The declared boundary for DefaultPolicy's search is one **native search interval**, the analogue of
`ResumeSearch(configuration)`. DefaultPolicy already brackets these intervals:
- it opens one with `feedback.resume(IntervalStart::ResumeSearch, …)` (5120) and with
  `resume(NativeContinuation …)`;
- it closes one with `feedback.end_interval(StopReason::…)` (5249 Reduce, 5329 SwitchMode,
  5402 Restart, 5473 Rephase, 5562/5572 Probe/Eliminate).

**Legitimately inside the interval** (plan §3 static micro dispatch; keep it static). A2 Decide (with
randec, focused-phase pattern, target note), A1 search Propagate (with H root consumption), A3
ConflictAnalysis (with glue/decision EMA, reluctant tick, bump-reason gating, OTFS retier, backjump
target), A4 scheduled partial restart (with reuse pick and target capture), plus the B1-B3 reads and C1
writes that serve them. The only obligation is that each internal step equal a public action with a
*declared instance*, so that N1 holds and a public replay reproduces it. Three internal items fail that
today and should still be fixed, because they are expressibility gaps rather than dispatch-route gaps:
- the native analysis target (A3);
- the per-conflict bump payload (C1 as a parameter, not a field poke);
- the mid-analysis retier hook (C4).

**Crossing a boundary** (the closure target):
- everything before 5120: startup Lucky, structured root, BCE, affine closure, sparse collect,
  heuristics attach, Probe/FastEliminate, late Lucky, graph labeling;
- every macro event that ends an interval: Reduce, SwitchMode, Rephase/Warmup/Walk, Reorder,
  Probe/Eliminate phases, restarts with root services;
- every inprocessing slot and candidate (A10);
- every support transaction (A11);
- the boundary writes C2, C3 (at reduce/vivify), C5 and C6.

All of these are low-frequency, so plan §3's "macro rules use registry-driven dynamic dispatch" already
allows routing them dynamically. Note that Reduce runs at level > 0 inside DefaultPolicy's search, while
the controller treats Reduce as a root action. The public `reduce_guard` is only "consistent"
(reduce.cpp:78-81), so a mid-search public Reduce is legal.

## 3. Plan

Equality criterion for "byte-identical" in every step: the same verdict, model, certificate bytes
(`--auto-proof --default-graph-labeling=on`, which yields DSR or startup PBP), conflicts, decisions,
learned clauses and propagations. Allowed differences must be declared counters:
`rule_nanos`, `rule_calls`, `api_decisions`, `reduce_scans` (the `measure` flag), wall timers and the
numeric `strategy_epoch`. **Risk to verify in every step:** registry dispatch increments
`strategy_epoch`, which is part of `StateVersion` and feeds:
- semantic-advice staleness (`semantic_variable_advice.cpp:16-28`);
- online stale checks (`online_inprocess.cpp:1975`);
- provenance JSON (ramsey_startup.cpp:228, parity_consequence.cpp:25);
- feedback streams.

Certificate bytes should not depend on it unless a stale check flips. Prove that with the identity test
below.

### Step 0: measurement and ratchet (tests and feedback only; no solver change)
- **Action map plus ratchet test** `tests/test_default_action_map.cpp`. Run DefaultPolicy with a
  `FeedbackReceiver` on the fixtures of `test_default_execution_signature.cpp` (profiles 0-6, pigeonhole
  4/6) and `tests/fixtures/*.cnf`. Classify every record (kind, name, instance) as P, S or K through a
  checked-in map from feedback name to public route (registry id, instance encoding, online parameter
  kind). Assert that no record is unclassified and that the S/K set equals the expected set for the
  current step. Each later step shrinks the expected set. This implements §5.6's "map every default
  operation".
- **Record the realized ConflictAnalysis target** in the feedback description. Today it is
  `INSTANCE_CANONICAL` / "native-target-after-learning" (dp.cpp:5163-5169). This changes the feedback
  stream only; re-baseline `test_native_policy_feedback.cpp` and `test_inspect_policy_feedback.py`.
- **Public replay reporter** `tests/test_default_public_replay.cpp`, using tactix.h only after load.
  Replay a DefaultPolicy feedback stream through `tactix_apply_instance`, `tactix_decide` and
  `tactix_online_*`, and stop at the first S/K record with a reason. It is report-only at this step and
  becomes a passing gate at step 7.
- N1/N2: none. Performance: none.

### Step 1: macro actions through registry dispatch (A6, A7, A9, A13, A4 keep=0, A12 core)
- Replace `execute_action(LuckyRequest<X>)` with `apply_rule_instance(Lucky, k, id)` using the
  instance map Cascade→canonical, Parity 6, Onehot 7, Affine 8, DirectCsp 9, BalancedSignature 10,
  Fermat 11, Cerny 12, LookupCircuit 13, Simon 14, Stconn 15, SignedCardinality 16, ExactCover 17,
  IndependentSet 18, StartupParity 19, StartupCount 20, GraphLabeling 21, Ramsey 22, ParityProof 23,
  ExactOneProof 24. Do the same for BCE, SwitchMode, Reorder(target), Rephase(type), Walk (scoped
  `WalkParameters`), LocalSearchGuidance(hint id), graph labeling (Lucky 26/27) and scheduled Restart
  (keep).
- Reduce becomes Reduce canonical under scoped `ReduceParameters{tier1, tier2, victim fraction,
  compact_collection = compact_due}`. All of these fields exist in `tactix_online_reduce_parameters`.
  The `measure` flag affects only counters.
- Keep policy choice logic, feedback wrappers and H publication exactly where they are.
- N1: no new surface; this proves the existing surface suffices. N2: the same proof calls.
- Tests:
  - `test_default_execution_signature` (DRAT bytes per profile, retained via argv dir), compared before
    and after;
  - `test_startup_support_signature`, `test_lucky*`, `test_graph_labeling_actions`,
    `test_restart_reduce`, `test_reduce_root_*`, `test_rephase`, `test_walk`, `test_reorder`,
    `test_switchmode`, `test_ramsey_policy_paths`;
  - the step-0 ratchet;
  - then the §8.4 four-set default gates.
- Performance: per macro event, about two `now_nanos` calls, an epoch increment and an indirect call.
  That is negligible next to Reduce/Rephase work. Restarts number in the thousands per run.

### Step 2: public-side H publication parity (finding 2, finding 3)
- Add a typed publication profile for public dispatch, reusing `RuleApplicationRequest{roots,
  retirements}` and `HeuristicRulePublisher` (`publication_execution.hpp:9-41`). The additive C API is
  `tactix_set_guidance_publication(s, TACTIX_GUIDANCE_DISCARD | TACTIX_GUIDANCE_NATIVE)`. The v1 default
  stays DISCARD, so old replays do not change.
- Route the controller's ad hoc `heuristics_consume_*` calls through the same publisher.
- Sweep's retirement hold until the follow-up Substitute (dp.cpp:7160-7190) is a two-action composite.
  Handle it in step 5.
- DefaultPolicy is unchanged, so its bytes are trivially identical. The controller must stay
  byte-identical too.
- Tests: `test_api_instances.c`, `test_no_heuristics.cpp`, `test_heuristics_reattach.cpp`,
  `test_controller_execution_signature.cpp`, `test_random_controller_policy.cpp`, and a new
  `test_api_guidance_publication.c` (NATIVE mode reproduces Default's H after a BVE/Factor/Substitute
  sequence).
- Performance: public path only.

### Step 3: boundary kernel-field writes become typed parameters (C2, C3 at reduce/vivify, C5, C6)
- Mappings:
  - tiers → per-call `ReduceParameters` / Vivify tier mode;
  - `bve_bound` → `BveParameters` per pass;
  - the `bve_retry` fill → a BVE invocation flag;
  - `factor_composite` → Factor parameters;
  - `subsume/strengthen_complete` → a round-start flag in their invocation;
  - `jump_reasons_bigbig` → `AnalyzeParameters` install;
  - `unflushed_root` and `input_binary_order` → a declared StartSearch/CloseStartup support (step 4);
  - `graph_labeling_session.clear()` → a Lucky 26 parameter.
- Add an additive `tactix_install_parameters(s, rule_id, const tactix_online_parameters*)` for the
  permanent installs at solve entry.
- Keep the same writes in the same order.
- Tests: `test_strategy_config`, `test_slot_pose`, `test_reduce_*`, `test_vivify_*`,
  `test_factor*`, `test_eliminate_limit`, the signature tests, and the ratchet.
- Performance: per reduce/phase/dose, negligible.

### Step 4: support-action vocabulary and guidance registration (A5, A11)
- Add a versioned `ActionType::Support` vocabulary to the controller interface, with an online/C
  equivalent. Members: RootPreparation{probing}, Restart{purpose=Preparation}, SparseCollect,
  CompactVariables{fold}, CollectGarbage, FlushRetiredRedundant, phase entry/exit (folding
  Disconnect/Reconnect watches and OccurrenceSession), SparseResume, MaterializeColdState,
  CloseStartupCertificate, AttachHeuristics/Residual seeding, CompactRandomOrder, ReductionPreparation.
- Register **Warmup** as an H-only PolicyOp (or a Walk parameter). Its `pick` closure becomes "the
  installed branching configuration", which the step-7 native profile supplies.
- N2: SparseCollect, RootPreparation and SparseResume can derive root units or a refutation
  (`RootCollectOutcome::Unit/Refuted`, dp.cpp:4990-4996). Document them as compositions of M1 Propagate
  with formula-neutral collection, not as new proof rules.
- Tests: `test_startup_support_signature`, `test_preparation_execution`, `test_execution_support`,
  `test_collect_*`, `test_root_*`, `test_guidance_execution`, and the ratchet.
- Performance: per phase/solve, none.

### Step 5: inprocessing slots as online invocations; BVE/Factor hooks into typed invocations (A10, C4 BVE/Factor)
- Express each slot occurrence as an `OnlineInvocation` (rule, typed parameters, granularity
  CompleteCurrentPass or OneInstance, `rule_work_budget` = ledger remaining, post_application=Propagate).
  `run_structured_root` already does this (6640-6835). Scheduling logic (limits, delays, followups,
  meters) stays in policy.
- Add the missing typed surface:
  - Substitute {max_rounds, per-round effort ceiling, complete};
  - Vivify band {scope, cumulative, offset};
  - a Strengthen forward-closure post-application;
  - BVE {fastel occurrence-flush profile moved into the rule; unit flush while connected};
  - the Sweep plus Substitute publication composite.
- Move `BveCandidateHeap` and its occurrence image into `BveInvocationSession` as an ordering profile.
- Make DefaultFactorMatch a typed Factor selection profile. Keep FactorMatchPolicy and ArenaOrdering
  injection as declared, excluded diagnostic arms.
- Tests: `test_inprocess_execution_signature`, `test_sequence_followups`,
  `test_substitute_execution`, `test_vivify_*`, `test_bve*`, `test_bve_active_pop`,
  `test_eliminate_limit`, `test_material_schedule`, `test_factor*`, `test_inprocess_program`, and the
  ratchet. Then the §8.4 gates and a same-window A/B with per-pass profiles (profile-every-slowdown
  memory).
- Performance: **medium.** Per BVE candidate there is now parameter validation and scope save/restore.
  Mitigate by validating once per pass, as `BveInvocationSession` already does (5810-5826), and by using
  header-only template execution. Moving the hook into the session replaces a function pointer with a
  direct call.

### Step 6: search-internal expressibility (A3, C1, C4 analysis hook)
- `NativeConflictAnalysisRequest` carries `{bump_reason_limit, bump_reason_depth, target_mode,
  retier_schedule}` by value. The executor performs today's field set/reset (5154-5156, 5173-5174).
- Add an additive ConflictAnalysis semantic instance, `ANALYZE_TARGET_CHRONO` (high-bit tag like
  `REDUCE_CLAUSE_INSTANCE`). It applies the existing `conflict_reuse_target` (analyze.cpp:891-899) on
  the learned-clause path too.
- At the OTFS checkpoint, the analysis applies `reduce_retier_from_histogram` from the request's
  schedule (analyze.cpp:1453-1468) and returns the updated schedule. This replaces the hook.
- DefaultPolicy's advise closure keeps only observation: glue, reluctant, bump delay.
- The `chrono_reuse_trail_` arm (off by default; H-ranked target) stays a declared diagnostic outside
  the correspondence claim.
- Byte identity: `stats.chronological` is incremented on both sides today (dp.cpp:5216/5222 versus
  analyze.cpp:895). Keep exactly one increment per event.
- N1: widens the instance surface. N2: identical learned clauses and proof lines.
- Tests:
  - `test_conflict_reuse_targets`, `test_root_conflict_reuse`, `test_analyze`,
    `test_analyze_parameters_api`, the signature tests;
  - a new `test_analysis_native_target.cpp` (random CNFs: native vs public CHRONO instance give the
    same trail, learned clauses and proof);
  - the §8.4 gates.
- Performance: **hot (per conflict).** The payload goes by value into header-only templates
  (`conflict_analysis_execution.hpp`). Verify instructions and cycles on one high-conflict instance with
  a same-window A/B via an env hatch, and require `.text` identity or a perf delta within noise.

### Step 7: native ResumeSearch profile and ActionPort twin (D1; final proof)
- Add a versioned native SearchConfiguration to `controller-interface.md`: Kissat profile with
  tick-driven mode switching, randec, focused-phase pattern, chrono, bump reasons, reluctant/adaptive
  restarts. DefaultPolicy's interval becomes `ResumeSearch(native)`, and macro events leave it through
  requests (steps 1-5).
- Template DefaultPolicy over an `ActionPort`:
  - `StaticPort` (production) inlines the same registered functions;
  - `PublicPort` goes through tactix.h, online steps and `observe()` only.
- The twin test `tests/test_default_public_correspondence.cpp` runs both ports on the fixtures and
  compares certificate bytes, verdict, model and trajectory counters. Its pass is the evidence that
  "DefaultPolicy is one client". The step-0 replay reporter becomes a gate.
- The same harness provides the §8.4 overhead measurement: StaticPort versus a hardcoded loop.
- Performance: StaticPort must compile to today's code. Check `.text` identity on a `-g` pin, or equal
  perf counters, and the §8.4 ≤5% bound.

### Documents to update
- **DEVELOPMENT_PLAN.md** (revise-plan):
  - §3: define static micro dispatch as a static instantiation of the same registered functions, and
    list the native-interval internals;
  - §5 strategy composition: support scopes become declared actions;
  - §5.1: additive API — guidance publication, parameter install, support actions, analysis target
    instance; v1 frozen;
  - §5.6: DefaultPolicy is `ResumeSearch(native)` plus requests; acceptance = twin test;
  - §8.4: correspondence gate and a real twin harness;
  - §9.4: staging under "shared execution and feedback preparation".
- **docs/calculus.md** (calculus):
  - M3 ConflictAnalysis instance surface (chrono target);
  - Reduce per-call tier/compaction parameters;
  - Restart preparation purpose;
  - the Decide target-note H effect;
  - Warmup as a guidance op;
  - support actions as compositions;
  - BVE ordering and Factor selection profiles.
- **docs/implementation-notes.md:**
  - the public discard of H events and its removal;
  - the `PublicTarget` specialization;
  - excluded diagnostic arms (`chrono_reuse_trail_`, FactorMatchPolicy, ArenaOrdering).
- **docs/controller-interface.md:** new version with the support vocabulary, native search
  configuration, publication profile and the feedback target field.
- **docs/scheduling-parity-ledger.md:** no change expected. Confirm per step.

## 4. Effort and recommendation

These are rough engineering estimates, excluding cluster time:

| Step | Estimate |
|---|---|
| 0 | 3-4 days |
| 1 | 3-5 days |
| 2 | 2 days |
| 3 | about 1 week |
| 4 | 1.5-2 weeks |
| 5 | 3-4 weeks (highest uncertainty) |
| 6 | 1 week |
| 7 | 2-3 weeks |

Total: about 11-15 engineer-weeks. Gates:
- every step: full Debug, plus the Claude-owned fuzz legs (§8.2);
- steps 1, 5, 6 and 7, which touch the default path: the §8.4 four-set default regression gates;
- each gate cycle costs days of cluster time.

**Recommended first two steps:**
- **Step 0** (ratchet, action map, realized analysis target, replay reporter) makes the gap measurable
  and keeps it from regrowing, with zero solver risk.
- **Step 1** (macro actions through registry dispatch) closes the largest boundary-crossing class. Its
  public equivalents already exist; it is low-frequency, so it has no hot-path risk; and it is checkable
  by byte identity plus the ratchet.

Step 2 is a cheap follow-up that needs no default gate.
