# Development Plan: A Competitive, Proof-Rule-Driven SAT Solver

Working name: **Tactix**. Reference: Kissat 4.0.4. Current status: [PROGRESS.md](PROGRESS.md).

This document owns stable scope, architecture, roadmap, gates, and deliberately open decisions. Status belongs in PROGRESS; rule semantics in `docs/calculus.md`; guards and deviations in `docs/implementation-notes.md`; evidence in frozen reports and runs. The solver is sequential and deterministic.

### Engineering order

Preserve native DSR, the typed direct-SR API, checked models/proofs and regression evidence. The winner-integration sequence and its preservation gates are owned by §8.3; resources and checker protocols by §8.1. Controller development is staged separately in §9.4. Current authorizations, completed campaigns and the next action belong in PROGRESS.

Historical checker evidence: [SC 2026 audit](benchmarks/reports/2026-09-15-sc2026-sr-checking-audit.md). Routine 57-row evaluations use a separate 3,600-second complete checker pipeline allowance, including after 300-second solver runs; checkers may finish independently. A proof that only times out is rechecked with a 45,000-second pipeline ceiling before milestone acceptance. This is a local extended-check policy, consistent with the documented SC 2025 checker limit; the numerical SC 2026 checker limit remains unverified. This corrects the SC 2026 attribution in the historical audit above. Final acceptance requires the complete pipeline selected by the certificate under §5; checker success alone is never inferred.

## 1. Goal and requirements

- **R1:** emit one certificate for every UNSAT execution (startup PBP or DSR)
  and check its complete pinned pipeline; reconstruct and validate models
  for SAT. Retain DRAT and native LRAT as explicit compatibility outputs.
- **R2:** implement competition DIMACS I/O, result lines, exit codes, proof-file
  handling, resource limits, and signal behavior.
- **R3:** expose every applicable proof rule and legal instance through a public
  API; rule order is not hardcoded into the kernel.
- **R4:** ship an automatic DefaultPolicy with state-of-the-art scheduling.
- **R5:** beat every frozen competition winner by engineering before learned
  scheduling; learning is additional upside, not the first route to parity.
- **R6:** match the practical proof-checking behavior of the SC 2026 SR path;
  a larger checker ceiling makes validation conclusive but does not excuse
  avoidably large or slow Tactix certificates on easy symmetry instances.

### 1.1 Non-negotiables

- **N1 — Universal applicability.** At every declared action boundary, the API
  exposes every rule whose pure guard holds and every legal instance, including
  branch variable and phase. Search actions are available during search;
  inprocessing actions and root-phase Lucky are defined on the level-zero root
  formula `F|U` after an explicit Restart-to-root transition. DefaultPolicy may
  decline an offered action; it may not narrow the legal API surface.
- **N2 — Proof/model correctness.** Every UNSAT policy/rule path produces a
  valid machine-checkable proof, and every SAT path produces a valid
  reconstructed model. Proof production is mandatory; the format is otherwise
  negotiable. DSR is canonical for search; a complete startup refutation may
  use PBP checked by VeriPB then CakePB (§5). New Satsuma-derived rules emit
  direct DSR witnesses and do not owe a DRAT/LRAT expansion.
- **N3 — Competitive default.** A shipped DefaultPolicy reaches at least Kissat
  parity on Tier D. Flexibility never excuses a slow default.
- **N4 — Scheduling floor.** For every integrated technique, DefaultPolicy scheduling—order, effort, delays, watermarks, quality bounds, ramps, feedback,
  and couplings—is at least as sophisticated as the state-of-the-art reference
  that owns that technique, including Kissat, Satsuma, and LymphoSAT in the
  frozen landscape. Every gap has a closure or an accepted deviation with
  evidence and a reopening path in the scheduling-parity ledger.

N1–N4 are immutable requirements, not work items that can be opened, closed, or preserved. Their verification is `pending` or `complete`. The scheduling-parity ledger contains open or dispositioned findings used to verify
N4; new evidence may add a finding and return verification to pending without
changing N4 itself.

Priority is N1/N2 soundness, N3 performance, N4 mechanism quality, then other
goals. A soundness regression is never staged.

## 2. Landscape and reference policy

The frozen SC24–26 performance set is Kissat 4.0.4, kissat-sc2024,
AE-Kissat-MAB, Satsuma+Kissat and LymphoSAT (checked SAT and UNSAT).
CaDiCaL, Glucose and CryptoMiniSat are mechanism references, not Tier-D arms.
Transliterate mechanisms/effects, never source code. Read Kissat and CaDiCaL,
choose the stronger mechanism and break ties with Kissat. Preserve a faithful
baseline beside stronger work; derive or remove unbacked constants and measure
with proofs enabled. Investigate implementation and coupled mechanisms before
changing reference behavior.

Port complete semantic units: result, retained trail, candidate class,
predecessors/follow-ups and proof/model transition. Incomplete units stay open.
Repair a mismatch or prove a stronger effect under the same correctness contract;
ask the owner when source, traces and semantics establish neither. Compare the
earliest common state, including order/layout, propagation, heuristic updates,
queue topology and charged clocks; equal sets or outcomes are insufficient.
Retain sound general repairs unless evidence disproves fidelity, correctness,
generality or an N1–N4 obligation, with a regression for the first wrong transition.

Each bounded diagnostic records its gate, hypothesis, fixture, trace anchor and
removal condition. Prefer trace-only instrumentation and matched feature-off
projections; one-sided disables may isolate a narrow cause. Remove completed,
neutral or superseded arms. Permanent knobs need continuing use; diagnostics
never justify disabling required shipped mechanisms or replacing enabled gates.

DefaultPolicy contains no magic strategy numbers. Centralize every strategy
number affecting choice, order, admission, timing,
effort, scope or retention in typed validated settings with descriptive names
and a source or derivation. Test wiring/defaults; narrowly annotate exceptional
literals. Rules retain only semantic/representation facts, enums and sentinels,
not heuristic cutoffs or search limits. Caller budgets never narrow legal public
actions or become soundness preconditions; label diagnostic-only limits clearly.

## 3. Core design: flexibility without hot-loop overhead

Use two dispatch tiers backed by the same rule implementations:

- **Micro rules** (`Propagate`, `Decide`, `ConflictAnalysis`) use static dispatch
  in DefaultPolicy and dynamic stepping in instrumented policies.
- **Macro rules and policy operations** use registry-driven dynamic dispatch;
  their lower frequency makes runtime flexibility affordable.

The competition configuration must remain within 5% of an equivalent hardcoded
loop (§8.4). Kernel state and proof machinery never depend on a policy.

## 4. Formal model

Solver state is `σ = <F,R,τ,ρ,H,P>`: irredundant and redundant clauses, trail,
model-reconstruction stack, heuristic/policy state, and proof state. A proof
rule has a pure applicability predicate, lazy legal instances, a state effect,
and a proof delta. Abstract definitions and soundness are authoritative in
`docs/calculus.md`; stronger implemented guards and their removal paths are in
`docs/implementation-notes.md`.

Rules compose `add-RUP`, `add-RAT(pivot)`, `add-SR(substitution)`, `delete`,
`restore`, `push-witness`, and terminal original-input PB refutation.
Guidance-only operations change `H`, not the
formula, but remain API-exposed under N1. Terminal UNSAT requires a checkable
contradiction derivation; terminal SAT requires an original-formula model after
reconstruction.

## 5. Architecture

Layers are frontends → policies → registered rules/policy operations → kernel →
proof backend. The kernel owns arenas, watches, trail/reasons, reconstruction,
limits, and statistics; it has no policy knowledge. It may hold an opaque,
non-owning pointer to policy-owned heuristic state, but correctness never depends
on its presence: null-heuristic execution remains sound. DefaultPolicy uses a static
micro loop and reference-strength macro scheduler. Scripted, replay, random, and
learned policies drive the same public surfaces.

The proof backend streams typed events. Automatic mode selects PBP only for a complete original-input
startup refutation before any proof event is published; otherwise it commits binary DSR before search.
DSR carries witnessless RUP/RAT, deletions and finite SR witnesses. Explicit DSR/DRAT/LRAT keep their meanings; unsupported actions decline
before mutation. No-proof runs choose the same logical actions. Startup actions are shared, typed,
replayable and API-selectable; discovery is formula-neutral, bounded and interruptible. Preserve
original CNF identities across input normalization; no mixed streams, format switching after publication,
or hidden whole-run proof ledger. Interrupted/failed publication cannot claim UNSAT.
The solver declares its UNSAT route; the runner rejects a certificate mismatch. PBP takes pinned VeriPB 3.0.2 then CakePB `6edb2f35` against original CNF (24,576 MiB heap).
Native DSR takes one content-declared hybrid route: a witness-free stream is validated and converted to DRAT, then drat-trim elaborates LRAT for pinned CakeLPR with original-CNF duplicate/empty-input bridges; a witnessed stream takes no-option `dsr-trim` to LSR, decoding binary LSR as needed, then Trestle. The route is fixed before checking, never retried after rejection, and both branches check against the original CNF.
Explicit DRAT/LRAT retain their routes. Full independent verification requires every stage to verify within its allowance; §8.4's bounded regression checks report incomplete checking separately. Fuzz audits every lemma and rejects corrupted certificates.

Keep kernel and checker-visible premises synchronized. Publish root facts needed
by later rules explicitly (DSR RUP units or LRAT reason chains); DRAT's established
implicit units remain valid only where no later direct-SR step consumes them.
DSR/DRAT literal-multiset deletions must match exact live clause multiplicity.
Retire proof-only transient lemmas before SR validation. Interrupted multi-step
publication deletes emitted helper prefixes if the final consequence never
enters the kernel. The calculus owns these proof-state and transaction invariants.

The C API attaches at most one proof writer, before the first operation that
settles the input/proof-id boundary. A duplicate attachment or attachment after
`status`, solving, enumeration, or another reasoning entry point returns `-1`;
otherwise the earlier unrecorded transitions would make a whole-solver
certificate incomplete.

DIMACS, proof, model, reconstruction, and public-API literals have permanent
external identities. Search state uses dense internal indices behind
kernel-owned bidirectional maps. Formula-neutral variable compaction is a
kernel support service; policy decides when to invoke it, and no proof rule or
guard depends on that scheduling decision.

**Strategy composition.** Decompose DefaultPolicy into branching/analysis,
restart/mode, database-maintenance, phase-guidance and inprocessing sub-strategies.
Keep cohesive configuration/runtime ownership and static hot dispatch. Static
and dynamic modes share one rule implementation; separate pure guards from policy advice.

All strategies use typed, validated per-call parameters, explicit instances,
granularity and budgets. Support single legal instances, caller-selected batches,
rounds and closures; arbitrary reordered/repeated rules or sequences; scoped
fixpoint repetition or explicit budget/stop. Shared execution preserves state,
proof and reconstruction invariants without hiding strategy choices.

Inprocessing programs, Lucky, Walk, and LocalSearchGuidance execute at level zero. A caller that is
currently searching composes an explicit `Restart` before those operations; the
executor does not suspend, rewrite or replay a non-root decision trail.

Distinguish local exhaustion, scoped fixpoint, budget exhaustion, interruption
and terminal results. Online updates take effect at safe boundaries without
restarting the solver; version settings and validate/invalidate resumption state.
Serialize configurations and actions for replay. Default, scripted, broad random
and future learned strategies share the same surface. Preserve existing default
behavior and report any conflicting N1/soundness repair under §8.4.

### 5.1 Public API (v1, frozen)

```c
tactix* tactix_init(void);
void tactix_release(tactix*);
void tactix_add(tactix*, int lit);
int tactix_rule_count(void);
const char* tactix_rule_name(int rule_id);
int tactix_rules_applicable(tactix*, int* out, int cap);
int tactix_rule_instances(tactix*, int rule_id, uint64_t* out, int cap);
int tactix_apply(tactix*, int rule_id);
int tactix_apply_instance(tactix*, int rule_id, uint64_t instance);
int tactix_decide_candidates(tactix*, int* literals, int cap);
int tactix_decide(tactix*, int literal);
int tactix_solve(tactix*);
int tactix_status(tactix*);
int tactix_proof(tactix*, const char* path, int format);
int tactix_value(tactix*, int literal);
uint64_t tactix_stat(tactix*, int stat_id);
```

Rule ids are stable name-sorted integers. Instances are unsigned 64-bit
snapshots valid until mutation; literal instances zero-extend the DIMACS
literal's 32-bit pattern and cannot alias `TACTIX_INSTANCE_CANONICAL`.
Enumeration is lazy and caller-buffered. Errors are integer returns. External
policies pull; v1 has no callback injection. Configuration may narrow or order
DefaultPolicy choices but never the N1 instance surface. IPASIR-style
incremental solving remains O7.

### 5.2 Rule catalog

The catalog is the scope index. Semantics and implementation status live in
their owning documents.

| # | Family | Proof delta | Phase |
|---|---|---|---|
| M1 | `Propagate` | none | 1 |
| M2 | `Decide` | none (`H`) | 1 |
| M3 | `ConflictAnalysis` (learn + backjump) | add-RUP | 1 |
| M4 | folded into M3 | — | — |
| S1 | `Restart` | none; empty if the retained root refutes | 1 |
| S2 | `Reduce` | delete | 1 |
| S3 | `Rephase` policy operation | none (`H`) | 2 |
| S4 | `SwitchMode` policy operation | none (`H`) | 1 |
| S5 | `Lucky` | none | 2 |
| S6 | `Walk` policy operation | none (`H`) | 3 |
| S7 | `Reorder` policy operation | none (`H`) | 3 |
| S8 | `LocalSearchGuidance` policy operation | none (`H`) | 3 |
| S9 | `SemanticVariableAdvice` policy operation | none (`H`) | winner integration |
| C1 | `SubsumeForward` | delete | 3 |
| C2 | `SubsumeBackward` / strengthen | add-RUP + delete | 3 |
| C3 | `Vivify` | add-RUP + delete | 3 |
| V1 | `Probe` | add-RUP units | 3 |
| V2 | `Backbone` | add-RUP units | 3 |
| V3 | `Substitute` | add-RUP + delete + push-witness | 3 |
| V4 | `TransitiveReduce` | delete | 3 |
| V5 | `Sweep` | add-RUP + delete | 3 |
| V6 | `Congruence` | add-RUP + delete | 3 |
| E1 | `BVE` | add-RUP + delete + push-witness | 3 |
| E2 | `Factor` / BVA | add-RAT + delete + push-witness | 3 |
| X1 | `BCE` | RAT-justified delete + push-witness | 3 |
| X2 | `HyperTernaryRes` | add-RUP | 3 |
| X3 | `CCE` | delete + push-witness | 3 |
| X4 | `Instantiate` | add-RUP + delete | 3 |
| X5 | `Condition` | delete + push-witness | 3 |
| X6 | `HyperBinaryRes` | add-RUP + delete | 3 |
| X7 | `SubstitutionRedundancy` | direct add-SR | 3 |
| X8 | `ParityConsequence` | certified XOR sum: add-RAT + add-RUP + helper deletion | winner integration |
| X9 | `AffineConsequence` | certified affine basis/consequence: add-RAT + add-RUP + helper deletion | winner integration |
| X10 | `ExactCoverExtension` | conservative definitions: add-RAT + model projection | winner integration |
| X11 | `IndependentSetCover` | certified theory consequence: add-RUP + helper deletion | winner integration |
| X12 | `Gf3AffineConsequence` | certified GF(3) resultant CNF: add-RUP | winner integration |
| X13 | `DirectCspSupport` | certified support or guarded-cover tuple clause; optional exact-OR definition: add-RUP/add-RAT | winner integration |
| X14 | `Gf3GaussianConsequence` | authenticated Gaussian unit/conflict batch: add-RUP | winner integration |
| X15 | `DirectPhpRefutation` | terminal direct-PHP certificate: add-RAT/add-RUP/delete | winner integration |
| X16 | `TseitinCycleBreaking` | authenticated XOR-component cycle unit: direct add-SR/RUP helper route | winner integration |
| X17 | retired (`NaryCspPrefix`) | no schedulable action; retain only its numeric tombstones while later public IDs remain stable | removed after negative Rank 401 evidence |
| X18 | `GraphIntervalEquivalence` | authenticated duplicate interval equivalence over existing variables: add-RUP | winner integration |
| X19 | `Gf3GeneralRowConsequence` | authenticated forbidden tuple of one non-unit Gaussian row: add-RUP | winner integration |
| X20 | `NaryCspEqualityAmo` | authenticated pairwise AMO over three aligned-domain equality variables: add-RUP + helper deletion | winner integration |
| X21 | `NaryCspEqualityTransitivity` | authenticated cross-owner equality closure: guarded RUP bridges, three ternaries and helper deletion | winner integration |
Support services—classification, reconstruction, internal checking, occurrence indexes—are not schedulable rules. Phase 3 requires every catalog family to be implemented and reached, or to have a reasoned no-implement disposition with evidence and a reopening path.
### 5.3 Extensibility contract

A new proof rule adds one `src/rules/<name>/` module and one `TACTIX_REGISTER_RULE` line. A guidance operation similarly adds one
`src/policies/ops/<name>/` module and one registration line. Registries drive
discovery, API exposure, random-policy fuzzing, and default scheduling.

Rules own semantics, statistics, guards, lazy instances and proof tests;
strategies own heuristic parameters and execution budgets. CCE tests phase-5
extensibility: unrelated kernel/policy/rule/proof edits require architecture repair.

### 5.4 Revamp discipline
Revamps within this project need no separate user approval: communicate the
design, then proceed through `revamp` with measurable success and rollback points.
Preserve N1–N4 and revalidate default, scripted and broad random policies.

### 5.5 Codebase hygiene

At gates and after major features, use `revisit-codebase` to remove dead or
duplicated machinery and route structural issues to §5.4. Never trade N1
capability or N1–N4 for simplicity.

### 5.6 Learned proof-rule scheduling

Build a controller that owns strategy objects and accepts policy action requests; first use a random policy, then a feedback-informed learner. Both drive the same execution interface within one solve. DefaultPolicy remains the comparison baseline, not a mandatory learner arm or fallback. Gates are in §8.5; development stages are in §9.4.

**Shared execution.** DefaultPolicy and random/learned selection are concrete, stateful policies over shared typed rule and support execution. Policies choose order, timing, instances, parameters, effort and scheduling-state updates; the executor owns mechanical preparation, application, publication, continuation and accounting. Preserve DefaultPolicy's native scheduling, adaptive doses, startup certificate boundaries and static hot dispatch; random selection retains fixed configured allowances. Shared machinery need not impose identical budgets or choices. Map every default operation and verify deterministic action/state correspondence, including support scopes, rather than infer architectural equivalence from regression passes.

**Restart loop.** Learner strategy changes and all theory reasoning occur only after explicit full Restart to decision level zero. Theory discovery, retained-state maintenance, bounded continuation and proof publication are root actions inside that restart window; no theory service runs at an ordinary search fixpoint or partial restart. Ordinary restarts continuing with the installed search configuration may retain a valid trail prefix through the shared parameterized Restart mechanism; preserve DefaultPolicy's native reuse. Root-preparation backtracks and scheduled restarts retain distinct counter semantics. Retain learned clauses, formula transformations, authenticated private theory state and root assignments. At root: observe, request an eligible action, apply it, observe its result, recompute eligibility, and repeat until explicit `ResumeSearch` or a terminal result. Multiple inprocessing actions may precede search. Search configuration selection belongs to `ResumeSearch(configuration)` and takes effect immediately before search begins, not as a separate repeated root action; partial continuation does not reinstall it.

**Search configurations.** Decouple branching, restart scheduling and phase guidance. The versioned interface below owns the complete named collection; initially include every legal combination, with later manual pruning allowed. Target selection uses saved phases when a target is absent. Preserve focused phase rotation and target collection independently of mode coupling. Rephasing remains an immediate, separate root action: Best, Original (all positive), Inverted (all negative), or Walk guidance. Atomic bundles may be added later.

**Strategy state.** Update only the active brancher's conflict history; retain inactive VMTF/EVSIDS history and synchronize live-variable eligibility on activation. Adaptive restart averages persist and update only while selected; start a fresh minimum conflict interval when search resumes. Reluctant scheduling continues its sequence over consecutive selected restarts, but entering from another scheduler starts a fresh sequence. Changing branching or phase selection alone does not reset restart history. Search-interval accounting starts at `ResumeSearch`, excluding root-action work. Preserve shared saved/target/best phase information; the versioned interface below owns the concrete phase-state and rotation-clock lifecycle.

**Actions and effort.** Expose eligible root proof-rule instances and guidance operations through the shared typed surface; this controller's root action set does not narrow N1's public search actions. Each work action has a fixed configurable effort allowance; the policy initially chooses the action, not its allowance. Record actual work, any overshoot, completion/progress and stop reason at supported atomic boundaries. There is no global root-loop effort budget, forced search resumption, solver-state rollback, or automatic fallback to another policy. Ordinary interruption/resource handling remains explicit. Sampling separates action-versus-`ResumeSearch` choice from configuration selection; initial policy is random, with probability tuning deferred to O19.

The concrete versioned action-space specification is owned by [docs/controller-interface.md](docs/controller-interface.md) and its matching typed interface. It specifies stable identities, root eligibility/instance selection, parameter domains, allowances, continuation and seeded sampling/replay. Root choices are eligible inprocessing instances, rephasing operations and `ResumeSearch`; after resumption the selected configuration drives ordinary search until the next full restart or terminal result. Account explicitly for excluded or unavailable choices without narrowing N1. This specification, rather than the older unrestricted public-action random harness, defines the new random policy's scope.

**Unfinished work.** Keep completed changes and each rule's existing continuation behavior; do not promise that every action can resume at an exact cursor. Preserve valid continuation state, validate it against intervening changes, and refresh scopes when the executor reports stale requests. Distinguish exhaustion, no change, fixpoint, inapplicability, interruption, resource/error and terminal results; none implies rollback or fallback. Re-selecting an action supplies its configured per-call allowance while preserving relevant native progress.

**Observation and feedback.** `observe()` is read-only and callable at any safe single-threaded action/search boundary, including after every application. Expose current formula/root state, cumulative work and fixed-variable counters, search statistics (including conflicts and learned-clause LBD), selected strategies, eligibility and action execution results. Repeated reads do not consume feedback or reset solver, accounting or reward history. Versioned after-action feedback binds before/after observations, identity, actual native effort/overshoot, duration, progress and stop status; record search intervals through restart, termination or interruption separately. Selection and feedback are replaceable interfaces; collecting feedback must not change state, accounting or random choices. Replay checks deterministic fields, not timing. The reward direction is immediate result divided by actual effort after an action, without retrospective credit assignment. O6 still owns the numerator, common effort currency, zero/terminal treatment, ResumeSearch reward boundary and algorithm; raw measurements precede those decisions.

## 6. Competition compliance

Accept DIMACS CNF robustly; emit standard `s`/`v` lines and exit codes 10/20;
write the requested proof; handle SIGINT/SIGTERM; enforce memory limits; package
licenses, attribution, build/run scripts, and a solver description. Automate the
contract in `tools/compete/check.sh` through the `compete` skill.

## 7. Performance strategy

Profile before structural changes; preserve reference data-structure discipline
and scheduling before tuning. Optimize without weakening the API, measuring
search, inprocessing, proof, and memory costs separately. When a more complex
candidate has mixed or comparable results, retain the simpler design. General novel engineering starts in Phase 4.5; §9.5 authorizes the bounded
winner-integration workstream before formal T4-VBS without closing that gate.

## 8. Evaluation plan

### 8.1 Benchmark tiers

Solver runs use SLURM with one single-threaded solver per task. Each coherent Satsuma candidate runs on all 57 rows at c8/32 GiB/300 seconds. Optional
10-second screens measure fast transitions, not final coverage. WV100 development
and final coverage may use `cpu-q`; development uses the same c8/32 GiB/300s contract. B28+C5 and Rank 1--50 use `cpu-dense-hwulab-q` with 12 logical
CPUs, 32 GiB and 1,200 solver seconds. Canonical Tier D uses a 32-GiB task-cgroup
limit and 5,000-second solve limit. Record partition and placement for every
WV100 run; its final coverage limit remains 1,200 seconds and 32 GiB.

- **Storage gate:** check allocated repository usage before creating a build or submitting jobs and at session wrap-up. Above **700 GB (700,000,000,000 bytes)**, clean up aggressively until usage is below the threshold: remove obsolete generated builds, duplicate test executables, unused campaign scratch and reconstructible source copies. Preserve active/pending-job dependencies, irreproducible source variants, checker authorities and evidence required to interpret or reproduce retained results. Record removed paths/bytes and recheck usage before launching more work. Build only the executables needed by the run.
- Every arm owns an immutable pin and manifest. One task runs one solver;
  comparative gate arms run as separate jobs. Submit full-width arrays without
  throttles or node constraints unless the user authorizes an exception or a
  documented correctness/resource constraint requires it. Record that reason;
  never mutate a queued/running pin.
- Independently check every verdict and record solve/check time, peak memory,
  certificate size/hash, checker provenance, placement, and cleanup. Formula,
  proof, model, and checker scratch must be anonymous node-local files whose
  storage is released on normal exit, timeout, crash, OOM, or cancellation.
  Residue or a missing final result makes the task incomplete and unscorable.
- Pending checking permits independent work but cannot close a gate or authorize
  promotion. Preserve and classify failures; a crash, sanitizer finding,
  rejected proof/model, or unexplained exit enters the §8.2 repair workflow.

| Tier | Purpose | Set and limit | Cadence |
|---|---|---|---|
| A | correctness/smoke | generated + crafted, 30 s, 32 GB | every commit |
| B | daily signal | 28 frozen SC instances, 300 s, 32 GB | daily/change |
| C | scheduling signal | 30 frozen instances, 1200 s, 32 GB | weekly/major change |
| D | gate scale | SC24+SC25+SC26 union, 1166 unique, 5000 s, 32 GB | frozen reference + phase gates |

The annual memberships are scoring views over 1166 unique runs. Per-instance
results are authoritative. Frozen reference data lives under
`benchmarks/baselines/`; never rerun a valid reference merely to fill a metric
or repair a frozen result. Lower time caps use exact censoring; lower-memory
views are labeled peak-RSS simulations and do not prove OOM.

Memory is a separate gate: compare matched inputs, proof modes, and resources;
attribute solver and checker memory separately. Diagnostic subsets and
projections help causal analysis but never replace a complete tier gate. Every
frozen instance remains scored, and identities or measured deadlines never
parameterize a solver repair.

### 8.2 Correctness

For every change, cover affected behavior and boundaries with focused tests or a
recorded reason existing tests suffice. Check SAT models against the original
CNF, UNSAT proofs independently, differential verdicts, and internal invariants.
Tier D follows the SC2026 Main-track proof contract; checker failure or timeout
is unsolved. Fuzz recorded uniform, structured, mutated, circuit-derived, and
degenerate inputs under legal random/stress schedules and known-bad controls.
Shrink failures and retain minimal regressions. Operational commands belong in
`fuzz`, `proof-check`, and `stress-policy`.

**Claude owns fuzzing.** Codex supplies each meaningful solver-change snapshot,
scope, resources and acceptance criteria. Claude owns preparation through the
terminal report. Codex reads owner-published status, dispositions and bounded
failure handoffs, but does not inspect raw campaigns or regrade their results.
Use an immutable hashed pin and normally the 50-physical-core harness;
record any smaller bounded checkpoint. Do not repeat a passing unchanged run.
Pending fuzz never authorizes promotion, and a failure hands Codex one bounded
reproducer, command and failure class.

**Codex owns debugging.** Preserve and repair the original implementation,
performance, crash, sanitizer, proof/model or soundness failure, add a causal
regression, and rerun affected independent oracles before dependent work resumes.
Claude receives debugging or repair review only by explicit user request; a
repair alone does not close its fuzz gate.

**Controller-foundation acceptance.** Fuzz both DefaultPolicy and the new controller-driven random policy on the candidate snapshot; old-policy regressions alone do not replace its fuzz leg. Use §5.6's action-space specification to require demonstrated execution of every declared root action family and named search configuration, repeated root actions, strategy changes across restarts, state retention, allowance exhaustion, continuation/stale scopes and interruption. Check read-only observation and seeded deterministic replay. Record coverage targets before launch and prove reach with counters/traces, including effective transitions rather than mostly inapplicable calls or NoOps. Independently check UNSAT proofs and original-CNF SAT models for both policies. Fix candidate-related failures and obtain a passing terminal Claude-owned report covering both legs before declaring stage 7a complete; disclose unmet coverage rather than infer acceptance from zero observed failures. Operational reporting belongs in the fuzz skill.

### 8.3 Performance gates

- **T1:** all Tier A; at least MiniSat on Tier B.
- **T2:** at least Glucose on Tier B; propagation within 20% of Kissat.
- **T3:** on both Tier B and Tier C, solve every instance solved by original
  Kissat and achieve at most 1.10× its PAR-2 under the matched allocation
  contract. A rule-neutral view may diagnose a mechanism but cannot replace
  either full-tier requirement. Gates phase 4.
- **T4-K:** on each annual Tier-D view and their union, cover every Kissat 4.0.4
  solve and reach at most 1.10× its PAR-2.
- **T4-WV100:** solve all 100 fixed WV100 instances within 1,200 seconds and
  32 GiB with independently checked models or proofs while retaining the B28+C5, Rank 1–50, memory and runtime gates. This gates Phase 4.5.
- **T4-WV2-89:** solve each of the 89 fixed WV2-89 instances within 600 seconds and 32 GiB with independently checked models or proofs, under the same retained gates and the same integration rule as WV100.
- **T4-VBS:** exact status-sensitive parity with the frozen reference VBS on
  the union. Every independently checked SAT or UNSAT result from each frozen
  configuration qualifies, including LymphoSAT.
- **T5:** exceed the frozen VBS or ship the best verified effort.

**Profile every slowdown.** Before attributing any observed slowdown to search trajectory, noise or a faithful port, profile both arms of a same-CPU pair (perf on a .text-identical pin plus per-pass timing). Attribute the extra time to passes and functions, and rule out an inprocessing pass that runs long while producing nothing used. Such a pass is a scheduling defect to fix generally. **Targeted timing measurement.** Use the cluster early for quick prototypes and parallel diagnostics. Use `cpu-q` for one-off correctness, profiling, ablations and coarse solved-quickly/timeout screens; reserve matched allocations for close relative-performance decisions and final aggregate gates. WV100 checked coverage may close on `cpu-q`;
regressions and close unit-cost comparisons use hwu allocations from §8.1. Close
comparisons use three independent paired waves with candidate and exact-state
baseline as concurrent separate jobs. Record placement, memory, service-rate
counters, proof/check identities and scratch cleanup. Report median and full
range; reject an absolute headline when either arm's range exceeds 2%. Do not
serialize arms, subtract unmatched checker time, or require paired waves for
fuzz and full-width arrays.

**Fixed Rank 1–50 reference.** Keep the approved fifty/frozen Kissat vector as
the retained regression control; full-view reruns are Tactix-only under the
12-CPU/32-GiB/1,200-second contract above.
For WV100 convergence, B28+C5 and Rank 1–50 must each improve aggregate runtime against the starting R16 gates (952.263s and 8,115.735s), with complete coverage. The bounded preservation stage has the separately frozen ceilings in §8.4. Rank1/6 overhead must be repaired generally; Rank6 must also take no longer than Satsuma+Kissat’s frozen203.321s total under comparable conditions (baseline `sc24-26-32g-5000s/results/02913.json`, hwu,c4,32GiB). This is an additional case gate, not a replacement for either suite target. After matching Rank6 performance, audit whether the remaining discovery, planning and validation work is unavoidable; distinguish proof obligations from implementation costs, and pursue substantially more efficient preprocessing through cheaper algorithms, exact reuse and justified early stopping or deferral, preserving correctness and effective preprocessing. The
≤1.01× target is an investigation trigger, not an automatic rejection of a
sound, general mechanism. Localize per-instance deltas with feature-off controls,
traces and repeated matched measurements; separate symmetry work from search.
An unlucky symmetry-induced search trajectory can justify a residual slowdown
when evidence gives no reason to infer a systematically worse search state.
One slower run, valid proofs or fewer variables alone cannot establish this.
Inspect propagation, clause quality and added search work; explain persistent
losses and improve implementation or scheduling before accepting their cost.
Do not discard a productive mechanism merely because its first result is slower.
Any accepted residual cost needs a documented cause and independently checked
WV100 benefit. No arbitrary replacement ratio, routine refresh/best-of
reference, or cross-window unit-cost inference is allowed. Pending checks permit
work, not acceptance; correctness, B28+C5, Rank and memory gates remain.

**Causal convergence and consolidation.** Journal each earliest portable gap
with its cause, strength, evidence, reproducer, owner, and reopening condition.
Compose only independently supported repairs. Complete checked B28+C5 and one
Claude-owned fuzz checkpoint before promotion; feature-off projections and
pending checks cannot replace these gates.

**Checkpoint and promotion contract.** N1--N4 govern every change. A development
checkpoint must state pending verification and known failures; committing it
does not accept it or replace a passing baseline. Promotion requires the declared
API surface, checked proof/model behavior and all retained performance/scheduling
gates. A default-off mechanism still owes N1/N2 for its public surface; disabling
its default schedule establishes compatibility, not acceptance of the mechanism.

**Refactor/N1 gate.** Complete strategy composition and audit the combined
candidate's calculus/API surface—legal instances, branches/phases, boundaries,
budgets and disable switches—before promoting winner-derived changes. Resolve
restrictions with direct API reach/regression and correctness evidence; fuzz
alone is insufficient. §9.5 owns current attribution and Boolean-repair priorities.

Rule/kernel changes may improve general calculus or proof-neutral representation,
but never absorb strategy-owned admission, cadence, effort, deadlines or
benchmark-conditioned choice. Preserve legal instances and proof/reconstruction
obligations even when one scheduled rule motivates an optimization. The automated
rule/policy boundary check remains a required focused guard.

**Worse-side-first priority.** At the earliest common portable state, compare
both throughput and work quality per invocation. Record effort allowance,
entry/exit ticks, overshoot, frontier/order, semantic rewrites, physical pose,
lineage, CPU/memory cost, and downstream yield. Equal ticks with higher cost or
equal cost with weaker work is not parity. Localize semantic or representation
divergence before comparing later work. Clause IDs, arena refs, benchmark names,
and wall time are diagnostics, never alignment keys or solver controls.

**WV100 winner-signature target.** After the refactor gate, treat the five
frozen baselines as an offline VBS. WV100 is permanently the first 100 rows of
the frozen coverage ranking in `benchmarks/tiers/WV100.tsv`: each row has a
checked solve within 1,200 seconds and 32 GiB from exactly one configuration.
All checked solver/status combinations qualify, including LymphoSAT UNSAT.
The fixed set contains 33 SAT and 67 UNSAT targets: 34 LymphoSAT, 57
Satsuma+Kissat, seven AE-Kissat-MAB, and two kissat-sc2024 sole-winner
signatures. Solved entries remain in the denominator; the goal is checked
Tactix coverage of 100/100 under the same 32-GiB/1,200-second contract. The persistent goal continues beyond recovery of the Rank/B28 runtime gates: identify and integrate the relevant techniques from each remaining row's owning baseline into Tactix's shared rules and strategy. No sequential portfolio, external solver stage, or benchmark-specific dispatch qualifies; retain correctness, fuzz, memory and aggregate runtime gates throughout. **WV2-89** is the second fixed tranche, `benchmarks/tiers/WV2-89.tsv`: the 89 rows of ranks 101–250 whose fastest checked configuration solves within 300 seconds (57 SAT, 32 UNSAT; fastest owner 25 LymphoSAT, 21 Satsuma+Kissat, 24 AE-Kissat-MAB, 19 Kissat). Its target is 600 seconds per row. Rows owned by LymphoSAT or Satsuma+Kissat are technique-integration targets; rows that several Kissat configurations solve are a DefaultPolicy parity question (N3), not a licence for per-instance tuning.

**Satsuma coverage obligation.** Preserve all 57 Satsuma+Kissat-owned rows in
the fixed WV100 denominator and their useful root-start transitions. The
retained classification is 51 structured row/row-column cases, five Johnson
cases and one general automorphism/Schreier case; these are attribution aids,
not dispatch labels or necessarily disjoint classes. A class closes only when
every member is independently checked through Tactix's integrated mechanisms.
Use this set as a permanent mechanism regression control. Current integration
order is owned by §9.5 and its subplan; the earlier requirement to finish all 57
before investigating other owners or restart-root SR no longer governs it.
B28+C5 and Rank1–40 own the separate scheduling study, while Rank1–50 retains
the broader final N3 gate. All symmetry reasoning remains at initial/full roots.

Use saved checked baselines, official descriptions and source to attribute each
winning transition before implementation; never rerun valid references, invoke
external solving stages, or dispatch by instance identity. AE-Kissat-MAB's seven
rows remain attribution targets; its rejected immediate two-arm bandit stays out.

**Symmetry integration contract.** Separate colored-CNF detection from seeded
stabilizer-chain fixing. Detection, representation/density, alignment, clique
ordering, traversal, seed, batching, repetition and work limits are typed,
replayable strategy choices. Pin/license/failure-isolate third-party backends.
Treat their output as untrusted: exact-check signed permutations against the live
formula and derived clauses against checker-visible premises before mutation.
Complete transversal mappings, pivot-sensitive DSR and prefix-preservation
obligations are owned by [the X7 calculus](docs/calculus.md#6d-direct-substitution-redundancy-x7);
[implementation notes](docs/implementation-notes.md) own dense-binary graphs,
column-action kernels, exact cached generator words and current producer guards.

Preserve Satsuma's useful simplification/breaking alternation and pure-literal
predecessor. The retained reference root program composes propagation, simultaneous
PureOnly BVE, duplicate removal/ULC AMO completion, Orbitopal closure and one graph
generator application before fresh detection, propagating to a scoped fixpoint.
Its binary-SCC predecessor was rejected by the ranks 34/37/39 controls. Current
typed DefaultPolicy stages are owned by implementation notes; public producers
retain arbitrary order, repetition, budgets and resumption under §5.

Discovery/validation is mutation-free; pre-commit stop/resource/proof failure
publishes nothing. Atomic commit completes proof, bookkeeping and root closure.
A productive generated job fixes/retires variables, shortens clauses or publishes
useful units/binaries; strategy owns that admission without narrowing N1.
Resource-limited prefixes are incomplete, never fixpoints. Commit complete orbits
or invalidate and re-detect; formula/producer/identity changes invalidate cursors,
while compatible budget updates retain progress. Charge deterministic work/replay.
Proof-neutral post-SR fanout advice to VMTF/EVSIDS remains an experiment: type and
replay its measure/order/cap/strength, and compare advice off on the same proof
and formula transition. Rules never alter heuristic scores.

Require Satsuma-scale end-to-end behavior on all 57 and no worse unit cost for
comparable symmetry work. Measure equivalent operations and phase times,
including inapplicable passes; fewer operations alone do not establish lower cost.
The accepted 100s-versus-120s CPU-Q example is neither a universal ratio nor a 10s
requirement. Preserve checked coverage and report it before PAR-2, proof cost,
memory and regressions. Restart-root SR positions, intervals, effort and caches
follow the subplan's bounded Claude review; all 57 and the standing gates remain.
Current protocols belong here and in that subplan, never in dated design snapshots.

### 8.4 Flexibility

Controller foundations also require DefaultPolicy regression runs on all four fixed sets: B28+C5, Rank 1--50, WV100 and WV2--89. Retain the solver verdict and solve-time/resource contract for every previously independently checked row, including aggregate solver ceilings of 8,521.52175s for Rank1–50 and 999.87615s for B28+C5 (the frozen controller-foundations-v1 authority); SAT models remain independently checked and UNSAT regression checking uses the bounded protocol below. Freeze the accepted checkpoint's pins and per-instance results as comparison authorities before implementation; do not substitute aggregate solved counts for row retention. Existing WV coverage/checker exceptions stay explicit and need not be closed merely to accept this interface stage. **Faithful-port retention exception:** a retained row may be lost only by explicit user decision, when same-window attribution shows that the loss comes from DefaultPolicy now matching the owning reference and that the earlier solve depended on non-faithful behavior. Each such row is listed here. Accepted: WV2–89 noL-11-8 (rank 222), lost to the faithful Satsuma→Kissat residual activation seed ([attribution](benchmarks/reports/2026-10-07-wv289-nol118-regression/README.md)). WV100 snw_16_9_CCSpre, lost to the certified structured-root symmetry unit that Satsuma also emits and earlier builds declined; Satsuma+Kissat is UNKNOWN at 5,000 s ([attribution](benchmarks/reports/2026-10-08-snw16-retention-exception/README.md)). A retained row that the candidate misses at the suite limit still counts as retained if it solves within 5,000 seconds, the full solve envelope, on the gate's own file. Rows lost beyond 5,000 seconds need such an exception. Every other retained row and both aggregate ceilings still apply. These regressions complement §8.2's DefaultPolicy/new-random-policy fuzz legs; the initial random policy does not inherit DefaultPolicy's performance gate.

**Bounded regression proof checking.** Routine regression runs use at most 30 minutes (1,800 seconds) of wall time per proof for the entire selected checker pipeline, including conversion/elaboration and final verification; never reset that allowance at each stage. Regression acceptance requires no proof rejection or checker failure. For a comparable baseline complete-pipeline time `t`, allow at most `max(0.30*t, 20 seconds)` additional checking time: candidate completion must be at most `t + max(0.30*t, 20 seconds)`. Equality passes; a larger increase is a significant slowdown. A clean deadline expiration is incomplete checking, not a rejected proof or an automatic regression failure, and does not require an extended recheck merely to close this regression gate. If a candidate times out after already exceeding that baseline-derived limit, it is a check-cost regression; otherwise its completion cost remains censored. When baseline checking is also censored and no comparable completed time exists, report the comparison as inconclusive rather than claim a slowdown or equality; clean censoring alone does not fail the bounded gate. Record stage outcomes and elapsed time; compare completed times directly and label ceiling-limited times as censored, without claiming equality or full independent verification. This exception does not weaken §8.2 fuzz acceptance or the full certificate checks required for formal performance/competition claims.

Periodic refactor checkpoints retain the full B28+C5 and Rank 1–50 gates,
resources and frozen controls in §8.1–§8.3. Focused correctness/API validation
alone does not authorize a new Tier-D campaign.

Default dynamic-macro/static-micro overhead is ≤5% versus a hardcoded twin.
Refactoring must preserve existing configurations' action/work/proof behavior
and runtime within measured noise on Rank 1–50 and B28+C5. Review individual
regressions as well as aggregates; new behavior is opt-in. Flag conflicting N1 repairs.
Demonstrate DPLL, plain CDCL, default replay, preprocessing and external branching
without kernel edits. Broad seeded random strategies vary order, repetition,
parameters, granularity, budgets and noncanonical instances beyond fixed prefixes.
Require effective transitions, reachability and replay, not mostly NoOps.
Ablations use configuration, not forks; test online reconfiguration and replay.

### 8.5 Learned scheduling

- **L-safe:** within +2% PAR-2 of DefaultPolicy on Tier C.
- **L-win:** beat DefaultPolicy on Tier D with gains across at least two
  families.
- Keep learner decision cost ≤0.1% of ticks.
- Validate reward proxies offline before online trust and verify arbitrary
  interruption leaves the solver in a valid solving state.

## 9. Milestones

| Phase | Deliverable | Exit |
|---|---|---|
| 0 | repository, tools, fuzz pipeline, Tier A/B | reproducible baseline |
| 1 | kernel, M1–M3, S1/S2/S4, native DSR, I/O | T1 + green proof fuzz |
| 2 | S3/S5, compatibility DRAT/LRAT, API freeze | T2 + API review |
| 3 | complete reference rule inventory and Tier C | catalog disposition/reach complete |
| 4 | faithful dosing, scheduling verification, and Tier B/C causal convergence | P4-CVG + N4 verification complete + T3 |
| 4.19 | code/document cleanup | no obsolete artifact or duplicated live authority |
| 4.20 | proof-rule and solving-strategy design surveys | two reviewed reports; no implementation |
| 4.21 | once-only SC24–26 reference campaign | frozen five-solver metrics and generated Andrew report at the §8.1 authority |
| Refactor gate | online sub-strategies, complete N1 surface, preserved N2–N4 | §5 composition + §8.2–§8.4 evidence |
| 4.25 | WV100 winner-signature convergence after the refactor gate | T4-WV100 |
| 4.5 | proof-event correlation and novel engineering | proof ranges map to emitting rule/application |
| 5 | tuning, bindings, extensibility proof | ≤5% overhead + CCE test |
| 6 | competition packaging | T5/best effort + submission ready |
| 7a | controller and read-only observation with a random policy | §9.4 interface, execution and replay validation |
| 7 | learned scheduling | L-safe then L-win |

Phase 4.20 owns the two surveys in `docs/design/`. A reviewed proof-rule
candidate states its calculus effect, proof/reconstruction impact, ownership,
and N1/N2 tests; a strategy candidate states features, decision point, source,
cost model, attribution experiment, and kill criterion. The roadmap items below
are candidates, not promises: each requires its owning review, correctness and
reach gates, a bounded experiment, cumulative measurement, and a removal
criterion. No performance feature enters implementation before its review and
the applicable Tier-D comparison and divergence plan are reviewed.

### 9.1 Phase 4.5 — engineering beyond reference parity

T4-VBS gates entry. Every candidate preserves the legal rule surface and
independently checkable native proofs and must earn a measured time/memory gain.

- Add proof-event correlation from each emitting rule/application to its proof
  lines or byte range; this is a fixed Phase-4.5 prerequisite.
- Decide P1's N1 compound-instance surface during survey review; after entry,
  implement generic definitional extension with a checked proof and correct
  model projection.
- Evaluate a zero-copy embedded-`SubSolver` view with local watches, trail,
  compact variables, and proof mapping over immutable main-formula clauses.
- Disposition all seven independent-lineage candidates: binary-resolution
  minimization, blocking restarts, adaptive Reduce intervals, usage-count
  ranking, branch/restart/polarity rotation, `best_inv`, and schedule strings.
- Optimize `Substitute` without changing its equivalent-literal rule or
  proof/reconstruction behavior.
- Factor a shared RUP-consequence emitter and evaluate richer Sweep harvesting:
  units, one-way implications, and equivalence pairs.
- Evaluate substantially faster Factor matching, including the
  [SAT 2026 approach](https://drops.dagstuhl.de/storage/00lipics/lipics-vol377-sat2026/LIPIcs.SAT.2026.29/LIPIcs.SAT.2026.29.pdf),
  and `egg`-style batched Congruence rebuilding at safe root boundaries, with
  native-proof and time/memory gates.
- Concretize Phase 4.5 scope and exit criteria before implementing candidates.

### 9.2 Phase 5 — tuning, bindings, and extensibility

- Complete any PR/SR-learning proof work not required by WV100.
- Add reviewed policy presets, Python bindings, and
  external-branching demonstrations.
- Pass the at-most-5% hardcoded-twin overhead gate and CCE pure-plugin test.

### 9.3 Phase 6 — competition readiness

- Complete and rehearse the §6 competition contract; decide O7 and O8.
- Exit on T5 or publish the best fully verified effort.

### 9.4 Phase 7 — learned scheduling

- **Controller foundations (7a):** specify the action space and implement §5.6 with read-only observation and a seeded random policy, without reward computation or learning. Complete §8.2's two-policy fuzz acceptance, §8.4's four-set DefaultPolicy regression gates and focused interface checks. Follow the versioned interface linked in §5.6 for allowance units, lifecycle and observation/request/result schemas; specification does not replace execution and coverage validation.
- The accepted stabilization and completed Tier-D comparison permit this interface stage despite unmet formal benchmark targets and deferred Tier-D checker-failure investigation; these obligations remain recorded and no N1--N4 or performance gate is relabeled complete. This stage is not a claim of learned improvement.
- **Shared execution and feedback preparation:** realize §5.6's shared-policy contract, preserve default action/state correspondence and integrate ordinary partial trail retention without moving learner changes away from root. Add feedback and offline inspection while selection stays random; no scalar reward or learning is implemented. Complete full Debug and fresh Claude-owned campaigns of at least 100,000 seeds per policy plus retained corpus and targeted nonvacuous action/configuration, partial/full restart, continuation/staleness, interruption, observation/feedback and replay coverage. Require independent completed proofs/models, separate timeout reporting and passing terminal reports, all four §8.4 default regression gates and instrumentation-overhead evidence; retain existing exceptions and no fallback/rollback/forced-resumption/global-loop-budget constraints.
- **Reward and learning:** decide O6, validate reward proxies/attribution offline, then replace random selection with a feedback-informed policy on the same controller. Include it in proof/model fuzzing; pass L-safe before expanding learned context or composed actions, and L-win before claiming improvement. Retain arbitrary-interruption validation and decide schedule distillation (O13).

### 9.5 Organic winner-technique integration

The active milestone is **at least 12 independently verified closures from the
fixed 44-instance gap on one coherent candidate**, with N1–N4, effective
Claude-owned fuzz, full Debug and B28+C5, Rank1–50, WV100 and WV2–89 acceptance.
The [winner-integration subplan](docs/winner-integration-plan.md) owns attribution,
symmetry/guidance design, provenance, shared-action contracts and the subsequent
original 36/full 44 checkpoints. The longer-term aim remains superiority beyond
the saved four-arm portfolio. Use the approved no-MAB `d170b4a5` comparison;
never rerun it. Reward/training remain deferred. This workstream permits reviewed
improvements before formal T4-VBS; it neither accepts Phase 4.5 nor weakens its gates.

**Generality is an integration gate.** Audit new and already integrated
LymphoSAT-derived constructors, local search, theory reasoning and scheduling.
Require a general algorithm and structurally justified applicability. Reject
benchmark fingerprints, unjustified encoding/shape exceptions and target-tuned
thresholds; a semantic recognizer for a supported encoding is legitimate.
Source attribution and correct proofs/models alone do not establish generality.
Promotion needs matched causal benefit and acceptable total overhead on varied,
held-out and nonapplicable inputs, plus all standing gates. Prefer general
Boolean-search or scheduling repairs where they explain the gap. Retire overfitted
techniques even if named targets remain unsolved; 12/44 permits no exception.
Always admit a valid, fully proof-checked symmetry-breaking unit when its action
commits. Singleton/compound-progress heuristics or benchmark timing cannot veto
it; discovery budgets and invalid/stale/uncertified-candidate guards still apply.
DefaultPolicy follows Satsuma's own symmetry practice, not benchmark-chosen subsets of it:
- **(A)** a structured pass repeats only after the previous pass produced symmetry-breaking output. Each repetition is earned through the existing owner-scale tick, zero-yield and size feedback, and unchanged detection state is never re-paid, so useless symmetry analysis adds no material overhead;
- **(B)** repetition stops once any pass reaches a detection limit (`reached_limits`); **(C)** at-most-one completion and row/product refinement also run above 50,000 variables; **(D)** FixAndCut uses the owner's cut point (`find_non_trivial_orbit`).
The search seed stays the owner-faithful original order minus fixed variables, which is what Satsuma→Kissat reparsing yields. Tactix's live-arena reseed is not the owner's handoff. A regression from adopting these is accepted when shown to be noise, such as luck across shuffled copies. Otherwise attribute it to a defective port or a missing coupled mechanism, never revert to a subset.
After (A)–(D) land, try symmetry breaking as restart inprocessing, beyond the owner's one-shot preprocessing. Run it only at full-restart roots on the current irredundant formula, under effort scheduling and (A)'s output-earned repetition. Its DSR obligations must cover learned clauses, by repairing or deleting those whose images are absent. Scheduling reuses Kissat's functions. A run fires only when the irredundant formula changed since the last detection. Its effort is a probe-class share of search ticks since the last run (`SET_EFFORT_LIMIT`), floored at the owner window `max(16·vars, 16Mi)`. Its conflict interval is NLOG2N in runs, with Kissat's formula-size delta scaling. Its delay halves after a fruitful run and doubles after a fruitless one, more aggressive than Kissat's +1.
A run is fruitful only if search used its output since the run: its clauses were propagation reasons or conflict-analysis antecedents, or its units forced further root assignments. Per-run provenance tags record these counts and reduce-deletions, for both this service and the startup symmetry pass. Promote it only on causal benefit under the same noise rule.

**Reasoning and guidance.** Attribute an UNSAT owner's complete analysis before
integrating it with structural authentication and terminal independently checked
DSR. Failed, interrupted or incomplete analysis is formula-neutral; private
continuation state and scheduling feedback may survive. Optional nonterminal
consequences need checked derivations, explicit assumption scope, Boolean
hardness beyond cheap propagation/bounded RUP, actual consumption and causal
solving benefit. SAT-only techniques may use bounded resumable local search:
complete candidates require whole-CNF verification, partial candidates guide
phases only. Preserve generic guidance and shared recognizers. Add no rule-specific
PBP route until measured clausal proof/check cost establishes practical need.

**Retired mechanisms and search attribution.** X17/noL-specific actions,
schedulers, producers, CLI and tests are retired; retain numeric tombstones and
neutral authenticated owners only for independently retained consumers. The
subplan owns Rank 401's actual Satsuma-residual/import/branch-order comparison;
original-seed ablations and alternative serializers are controls, not parity.
Rank 991's shared Authenticate/Continue actions and bounded service remain opt-in
pending cumulative verification and performance/regression acceptance.

**Persistent XOR.** Recognize once using authenticated subimages/affine matrices;
retain source/proof provenance, lease premises or certify durable representations
before deletion, and explicitly handle substitution/elimination. At initial/full
restart roots only, incorporate new root facts, resume bounded Gaussian work,
skip unchanged states and publish checked clauses before `ResumeSearch`. Retained
state survives search; no conditional consequence becomes an unconditional unit.
Use proof-checked satisfiability-preserving extensions, explicit work/memory/proof
budgets and the existing charged tick/interval/fruitlessness-delay mechanisms.
Rank 569 targets substantially below verified Kissat 3359.975s with compact proofs,
useful yield and independently measured checking. The subplan owns lifecycle,
provenance and mixed-CNF tests; fuzz, full Debug and all four gates remain required.

**Cadence.** Aim for a fresh clean candidate within an eight-hour development
window. Continue independent work while long fuzz, Debug, regression and checker
jobs run; repair terminal findings without waiving gates. The host has four cores:
use Slurm and immutable pins for substantial builds, sanitizers and measurements.
Refresh Andrew's report at coherent checkpoints. Follow §8.1's 700-GB storage gate;
clean reproducible unused builds opportunistically below it and preserve evidence.
On a requested handoff, record submitted jobs and exact next actions without
waiting for them or presenting an unvalidated checkpoint as accepted.

## 10. Risks and mitigations

Measure abstraction cost against the hardcoded twin; use independent proof/model
checks, negative oracles, stress schedules and the regression corpus for soundness.
Control noise and overfitting with frozen references, matched attribution, Tier C/D
and per-instance review. Track arena high-water marks and streaming-proof volume.
Keep documentation within budgets, cross-reference checks and one fact/one home.

## 11. Open decisions

Resolve through `revise-plan` concretize mode and record only the minimum needed.

| # | Decision | Decide by |
|---|---|---|
| O4 | LRAT hint strategy for each remaining inprocessing rule | rule implementation |
| O6 | immediate result/actual-effort numerator, common effort currency, zero/terminal handling, ResumeSearch reward boundary and learning algorithm; no retrospective credit assignment | after feedback preparation, before feedback-informed learning |
| O7 | incremental/IPASIR proof interaction | phase-6 entry |
| O8 | retain FRAT only if a consumer appears | phase 6 |
| O12 | contextual features and learned-state persistence | before expanding learned context |
| O13 | distill stable learned schedules into DefaultPolicy | phase-7 exit |
| O17 | attribute the seven WV100 AE-Kissat-MAB signatures using the frozen owner; the always-on integrated bandit failed the fixed gates and was removed | before claiming those rows mechanism-complete |
| O18 | adapt SR discovery effort from useful producer output per unit work, separately from conditional cleanup; preserve explicit per-call budgets and replay | after the Rank17 repair and full preservation gates |
| O19 | optional tuning of sampling probabilities or manual pruning of the named search collection; the initial action/allowance/lifecycle/schema and sampling contracts are owned by the versioned interface in §5.6 | after initial random-controller validation |

Closed decisions have their current contracts in the owning sections; Git
history preserves the decisions they replaced.

## 12. Key references

Kissat 4.0.4; CaDiCaL 3.0.0/CaDiCaL-SC2025; Glucose; CryptoMiniSat; recent SAT
Competition solver descriptions; Järvisalo–Heule–Biere on inprocessing rules;
Nieuwenhuis–Oliveras–Tinelli on Abstract DPLL; DSR/LSR, `dsr-trim`, and Trestle;
DRAT/LRAT and cake_lpr work;
DynamicSAT and Kissat-MAB scheduling; SBVA; and *Cubing for Tuning*. Local
papers, clones, and winner tooling are indexed from `docs/README.md` and
`tools/solvers/`.
