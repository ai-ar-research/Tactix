# Development Plan: A Competitive, Proof-Rule-Driven SAT Solver

Working name: **Tactix**. Reference: Kissat 4.0.4. Current status:
[PROGRESS.md](PROGRESS.md). Git history records changes to this specification.

This document owns stable scope, architecture, roadmap, gates, and deliberately
open decisions. Status belongs in PROGRESS; rule semantics in
`docs/calculus.md`; guards and deviations in `docs/implementation-notes.md`;
evidence in frozen reports and run results. Scope is sequential,
single-threaded, deterministic solving.

## 1. Goal and requirements

- **R1:** emit machine-checkable DRAT and native LRAT for UNSAT; reconstruct and
  validate models for SAT.
- **R2:** implement competition DIMACS I/O, result lines, exit codes, proof-file
  handling, resource limits, and signal behavior.
- **R3:** expose every applicable proof rule and legal instance through a public
  API; rule order is not hardcoded into the kernel.
- **R4:** ship an automatic DefaultPolicy with state-of-the-art scheduling.
- **R5:** beat every frozen competition winner by engineering before learned
  scheduling; learning is additional upside, not the first route to parity.

### 1.1 Non-negotiables

- **N1 — Universal applicability.** At every declared action boundary, the API
  exposes every rule whose pure guard holds and every legal instance, including
  branch variable and phase. Search actions are available during search;
  inprocessing actions and root-phase Lucky are defined on the level-zero root
  formula `F|U` after an explicit Restart-to-root transition. DefaultPolicy may
  decline an offered action; it may not narrow the legal API surface.
- **N2 — Proof/model correctness.** Every policy and rule path produces valid
  DRAT/LRAT evidence or a valid reconstructed SAT model.
- **N3 — Competitive default.** A shipped DefaultPolicy reaches at least Kissat
  parity on Tier D. Flexibility never excuses a slow default.
- **N4 — Scheduling floor.** DefaultPolicy scheduling—order, effort, delays,
  watermarks, quality bounds, ramps, and couplings—is at least as sophisticated
  as Kissat's. Every gap has a closure or an accepted deviation with evidence
  and a reopening path in the scheduling-parity ledger.

N1–N4 are immutable requirements, not work items that can be opened, closed, or
preserved. Their verification is `pending` or `complete`. The
scheduling-parity ledger contains open or dispositioned findings used to verify
N4; new evidence may add a finding and return verification to pending without
changing N4 itself.

Priority is N1/N2 soundness, N3 performance, N4 mechanism quality, then other
goals. A soundness regression is never staged.

## 2. Landscape and reference policy

The frozen SC24–26 performance set is Kissat 4.0.4, kissat-sc2024,
AE-Kissat-MAB, Satsuma+Kissat and SAT-only LymphoSAT. CaDiCaL, Glucose and
CryptoMiniSat are mechanism references, not Tier-D arms.

DefaultPolicy transliterates mechanisms and effects, never source code. Read
Kissat and CaDiCaL; choose the stronger mechanism and use Kissat to break ties.
Preserve a faithful baseline beside stronger work. Derive or remove unbacked
constants; do not tune them. Measure with proofs enabled. Source determines
reference repairs; a regression calls for investigating implementation errors
and coupled mechanisms, not weakening the reference.

Port complete semantic units: analysis result, retained trail, candidate class,
conditional predecessors/follow-ups, and proof/model transition. A local proxy
is not a faithful baseline. Keep incomplete units open. For each mismatch,
prove a stronger Tactix effect under the same correctness contract or repair it;
if source, traces and semantics establish neither, ask the project owner.

Compare the earliest common state before a split, including iteration/admission
order, clause/watch layout, propagation, heuristic values/update order,
heap/queue topology and charged clocks. Equal sets or terminal results do not
establish equivalence when order is observable. Do not infer causes from later
semantic divergence while an earlier common-state mismatch is unexplained.
Each repair needs a differential regression for the exact wrong transition;
performance evidence cannot conceal an upstream parity gap.

Retain sound, general reference repairs through downstream regressions; discard
one only when stronger evidence disproves its fidelity, correctness, generality
or an N1–N4 obligation. Bound diagnostic alternatives rather than restoring an
older compensating default. Compare downstream utility as specified in §8.3.

Diagnostic arms require a concrete time-bounded question and removal condition;
prefer trace-only instrumentation and remove completed, neutral or superseded
arms. A permanent knob needs a continuing use beyond historical behavior.
Prefer matched feature-off projections; permit one-sided disables for narrow
attribution. Record gate, hypothesis, fixture, trace anchor and removal condition.
A diagnostic projection cannot justify disabling a required shipped mechanism,
retaining an experimental arm or replacing default-on performance gates.

DefaultPolicy contains no magic strategy numbers: every strategy number affecting
choice, order, admission, timing, repetition,
effort, candidate scope or retention belongs in centralized typed configuration,
with a descriptive name and reference or derivation. No embedded use-site values.
Configuration wiring and shipped-default tests accompany changes; exceptional
literals need a narrow annotated allowlist. Algorithmic/representation constants,
enums, sentinels and diagnostic-only limits must be clearly identifiable.
Move all heuristic cutoffs, budgets and search limits out of rules into validated,
fully configurable strategy settings. Rules keep semantic/representation facts;
caller limits never narrow legal public actions or become soundness preconditions.

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

Rules compose five proof primitives: `add-RUP`, `add-RAT(pivot)`, `delete`,
`restore`, and `push-witness`. Guidance-only operations change `H`, not the
formula, but remain API-exposed under N1. Terminal UNSAT requires a checkable
empty-clause derivation; terminal SAT requires an original-formula model after
reconstruction.

## 5. Architecture

Layers are frontends → policies → registered rules/policy operations → kernel →
proof backend. The kernel owns arenas, watches, trail/reasons, reconstruction,
limits, and statistics; it has no policy knowledge. DefaultPolicy uses a static
micro loop and reference-strength macro scheduler. Scripted, replay, random, and
learned policies drive the same public surfaces.

DIMACS, proof, model, reconstruction, and public-API literals have permanent
external identities. Search state uses dense internal indices behind
kernel-owned bidirectional maps. Formula-neutral variable compaction is a
kernel support service; policy decides when to invoke it, and no proof rule or
guard depends on that scheduling decision.

Architecture obligations:

1. Survey multiple reference lineages before designing a subsystem.
2. Keep one implementation per rule, shared by static and dynamic modes.
3. Separate pure guards from policy advice and engineering restrictions.
4. Measure abstraction cost and redesign abstractions that cost N3.
5. Port mechanisms with their effect; source counters must prove they fire.
6. Begin from faithful reference behavior; deviations carry the argument.

**Strategy composition.** Decompose DefaultPolicy into branching/analysis,
restart/mode, database-maintenance, phase-guidance and inprocessing sub-strategies.
Keep their cohesive configuration and runtime-state ownership; static hot dispatch
and the shared rule implementations remain intact.

All strategies use typed, validated per-call parameters, explicit instances,
granularity and budgets. Support single legal instances, caller-selected batches,
rounds and closures; arbitrary reordered/repeated rules or sequences; scoped
fixpoint repetition or explicit budget/stop. Shared execution preserves state,
proof and reconstruction invariants without hiding strategy choices.

Inprocessing programs and root-phase Lucky execute at level zero. A caller that
is currently searching composes an explicit `Restart` before either operation;
the executor does not suspend, rewrite or replay a non-root decision trail.

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

Support services—classification, reconstruction, internal checking, occurrence
indexes—are not schedulable rules. Phase 3 requires every catalog family to be
implemented and reached, or to have a reasoned no-implement disposition with
evidence and a reopening path.

### 5.3 Extensibility contract

A new proof rule adds one `src/rules/<name>/` module and one
`TACTIX_REGISTER_RULE` line. A guidance operation similarly adds one
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

Phase 7 learns rule/batch choice online within one run, with DefaultPolicy a
permanent arm. It never re-solves or discards state. Start with replay-validated
reward proxies and windowed UCB, then tabular context and composed batches under
kill criteria. LearnedPolicy joins proof/model fuzzing and remains anytime;
gates are in §8.5 and sequencing in §9.4.

## 6. Competition compliance

Accept DIMACS CNF robustly; emit standard `s`/`v` lines and exit codes 10/20;
write the requested proof; handle SIGINT/SIGTERM; enforce memory limits; package
licenses, attribution, build/run scripts, and a solver description. Automate the
contract in `tools/compete/check.sh` through the `compete` skill.

## 7. Performance strategy

Profile before structural changes; preserve reference data-structure discipline
and scheduling before tuning. Optimize without weakening the API, measuring
search, inprocessing, proof, and memory costs separately. Novel engineering
starts only in Phase 4.5.

## 8. Evaluation plan

### 8.1 Benchmark tiers

All solver runs use SLURM `cpu-dense-hwulab-q`, one single-threaded solver per
task. Allocate 8 GiB per **physical** core. On SMT-2, request two logical CPUs per
physical core (32 GiB = four physical/eight logical CPUs). Canonical Tier D
uses a 32 GiB task-cgroup limit and 5000 s solve limit. User-authorized
experimental allocations are recorded separately and never redefine a gate.
Thus a 16 GiB check uses two physical/four logical CPUs in one SLURM `--mem=16G` task cgroup.

- Every arm owns an immutable pin and manifest. One task runs one solver; never
  serialize arms, throttle full-width arrays, constrain their node placement, or
  mutate a queued/running pin.
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

**Claude owns fuzzing.** Codex invokes Claude Code for each needed solver-fuzz
checkpoint, supplying candidate/pins, scope, resources and acceptance criteria.
Claude owns preparation, submission, monitoring, artifact inspection, oracle
interpretation, cleanup audit and the terminal report. Codex may continue
independent development, but must not inspect the campaign or recompute/reinterpret
its report. Claude's terminal pass closes only that fuzz gate; a failure pauses
dependent work and supplies a bounded reproducer, command and failure class for
Codex to repair. Do not rerun a passing unchanged campaign for reassurance.

Run one checkpoint after each meaningful solver-change batch; unchanged rebuilds
and diagnostics need no duplicate. Generated legs normally use the 50-physical-
core harness contract; a bounded corpus or reachability checkpoint may use less
with the reason recorded. Every leg uses an immutable read-only harness snapshot
and records its hash. A successful job removes that snapshot; a failed job
retains it for Claude's handoff. Pending fuzz never authorizes promotion.

**Codex owns debugging.** Diagnose and repair implementation, performance,
unit-test, crash, sanitizer, proof/model and soundness failures directly; delegate
debugging or repair review to Claude only at the user's explicit request.
Preserve the original failure, establish a causal reproducer and regression,
and validate affected invariants and independent proof/model oracles before
resuming dependent work. Do not weaken gates. A bounded fuzz handoff is available
for direct repair; the rest of that campaign stays Claude-owned, and the repair
alone does not close its required fuzz gate.

### 8.3 Performance gates

- **T1:** all Tier A; at least MiniSat on Tier B.
- **T2:** at least Glucose on Tier B; propagation within 20% of Kissat.
- **T3:** on both Tier B and Tier C, solve every instance solved by original
  Kissat and achieve at most 1.10× its PAR-2 under the matched allocation
  contract. A rule-neutral view may diagnose a mechanism but cannot replace
  either full-tier requirement. Gates phase 4.
- **T4-K:** on each annual Tier-D view and their union, cover every Kissat 4.0.4
  solve and reach at most 1.10× its PAR-2.
- **T4-VBS:** exact status-sensitive parity with the frozen reference VBS on
  the union; LymphoSAT contributes SAT results only. This gates Phase 4.5.
- **T5:** exceed the frozen VBS or ship the best verified effort.

**Targeted timing measurement.** Decision-grade long-instance comparisons use
three independent paired waves. Each wave submits candidate and exact-state
baseline as separate concurrent jobs. On EPYC 9454 use 12 logical CPUs, exactly
48,000 MiB, one pinned solver thread, and no address-space limit. Record
placement, peak RSS/VmPeak, service-rate counters, proof/check identities, and
anonymous-scratch cleanup. Report the median and full ratio range; reject an
absolute headline when either arm's range exceeds 2%. Do not serialize arms,
subtract unmatched proof/check time, or extend this repetition requirement to
fuzz and full-width tier arrays.

**Fixed Rank 1–50 reference.** Keep the approved fifty/frozen Kissat vector as
12-CPU/48,000-MiB regression controls; matched full-view reruns are Tactix-only.
The ≤1.01× target is deferred, not achieved or a winner-stage prerequisite.
No routine refresh/best-of denominator or cross-window unit-cost inference.
Pending checks permit work, not acceptance; correctness/B28+C5/memory gates stay.

**Causal convergence and consolidation.** Journal each earliest portable gap
with its cause, strength, evidence, reproducer, owner, and reopening condition.
Compose only independently supported repairs. Complete checked B28+C5 and one
Claude-owned fuzz checkpoint before promotion; feature-off projections and
pending checks cannot replace these gates.

**Refactor/N1 entry gate.** Complete strategy composition and audit the combined
candidate's entire calculus/API surface—legal instances, branches/phases,
declared action boundaries, budgets and disable switches—before winner-driven development.
Resolve restrictions with direct API reach/regression and correctness evidence;
fuzz alone is insufficient. Stop further Kissat-only tuning.

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

**Twenty winner-coverage targets.** After the refactor gate, treat the five
frozen baselines as an offline VBS and select ten SAT plus ten UNSAT checked
1,200-second/32-GiB wins over genuine Tactix timeouts. Use existing censored
data; exclude OOM, crash, missing, and checker-failure cases. Prefer rarer
coverage and LymphoSAT-only SAT wins, then freeze provenance and tie-breaks.
Rerun Tactix and only each target's fastest qualifying baseline in a matched
full-width window (12 logical CPUs, one thread, 32 GiB, 1,200 solver seconds).
Proof/model checking gates acceptance but does not block independent work.
Study and report the winning mechanism before implementing it. Integrate general
mechanisms in Tactix, never a sequential portfolio or identity-conditioned rule.
Preserve prior coverage; report solved SAT/UNSAT counts before PAR-2, proof cost,
memory, and regressions. T4-K and final exact T4-VBS coverage remain required.

### 8.4 Flexibility

Evaluate B28+C5 and Rank 1–50 periodically at meaningful refactor checkpoints,
using the same resources and benchmark definitions as prior runs, frozen candidates
and matched baseline controls. Defer the full Tier D campaign until refactor
implementation and correctness/API validation are complete.

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
| 1 | kernel, M1–M3, S1/S2/S4, DRAT, I/O | T1 + green proof fuzz |
| 2 | S3/S5, LRAT, API freeze | T2 + API review |
| 3 | complete reference rule inventory and Tier C | catalog disposition/reach complete |
| 4 | faithful dosing, scheduling verification, and Tier B/C causal convergence | P4-CVG + N4 verification complete + T3 |
| 4.19 | code/document cleanup | no obsolete artifact or duplicated live authority |
| 4.20 | proof-rule and solving-strategy design surveys | two reviewed reports; no implementation |
| 4.21 | once-only SC24–26 reference campaign | frozen five-solver metrics and generated Andrew report at the §8.1 authority |
| Refactor gate | online sub-strategies, complete N1 surface, preserved N2–N4 | §5 composition + §8.2–§8.4 evidence |
| 4.25 | five-winner coverage convergence after the refactor gate | T4-VBS |
| 4.5 | proof-event correlation and novel engineering | proof ranges map to emitting rule/application |
| 5 | tuning, bindings, extensibility proof | ≤5% overhead + CCE test |
| 6 | competition packaging | T5/best effort + submission ready |
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
  implement generic definitional extension with checked DRAT/LRAT and correct
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

- Decide O16's PR/SR-learning proof story.
- Add reviewed policy presets/MAB scheduling, Python bindings, and
  external-branching demonstrations.
- Pass the at-most-5% hardcoded-twin overhead gate and CCE pure-plugin test.

### 9.3 Phase 6 — competition readiness

- Complete and rehearse the §6 competition contract; decide O7 and O8.
- Exit on T5 or publish the best fully verified effort.

### 9.4 Phase 7 — learned scheduling

- Validate replay and reward proxies offline, then implement RL1 windowed UCB
  with DefaultPolicy as a permanent arm and LearnedPolicy in proof/model fuzzing.
- Pass L-safe before contextual RL2 or composed-batch RL3; retain kill criteria
  and an arbitrary-interruption anytime test.
- Add offline per-family presets, measure L-win across at least two Tier-D
  families, and decide whether to distill stable schedules (O13).

## 10. Risks and mitigations

- **Abstraction overhead:** static hot loop, measure against hardcoded twin.
- **Proof unsoundness:** independent checkers, negative oracle tests, stress
  schedules, regression corpus.
- **Model reconstruction bugs:** validate every SAT model against original CNF.
- **Benchmark noise/overfitting:** fixed references, matched attribution, Tier C/D
  gates, per-instance review.
- **Documentation drift:** one fact/one home, enforced budgets and cross-reference
  checks, concise PROGRESS resume block.
- **Memory/proof volume:** arena storage, streaming proofs, tracked high-water
  marks and proof bytes.

## 11. Open decisions

Resolve through `revise-plan` concretize mode and record only the minimum needed.

| # | Decision | Decide by |
|---|---|---|
| O4 | LRAT hint strategy for each remaining inprocessing rule | rule implementation |
| O6 | learned scheduler arm sets and UCB constants | phase 7 |
| O7 | incremental/IPASIR proof interaction | phase-6 entry |
| O8 | retain FRAT only if a consumer appears | phase 6 |
| O12 | contextual features and learned-state persistence | phase 7 RL2 |
| O13 | distill stable learned schedules into DefaultPolicy | phase-7 exit |
| O16 | admit PR/SR learning with an N2 checker/translation story | phase-5 entry |

Closed decisions have their current contracts in the owning sections; Git
history preserves the decisions they replaced.

## 12. Key references

Kissat 4.0.4; CaDiCaL 3.0.0/CaDiCaL-SC2025; Glucose; CryptoMiniSat; recent SAT
Competition solver descriptions; Järvisalo–Heule–Biere on inprocessing rules;
Nieuwenhuis–Oliveras–Tinelli on Abstract DPLL; DRAT/LRAT and cake_lpr work;
DynamicSAT and Kissat-MAB scheduling; SBVA; and *Cubing for Tuning*. Local
papers, clones, and winner tooling are indexed from `docs/README.md` and
`tools/solvers/`.
