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
- **R5:** exceed Kissat by engineering before learned scheduling; learning is
  additional upside rather than the mechanism that first reaches parity.

### 1.1 Non-negotiables

- **N1 — Universal applicability.** At every state, the API exposes every rule
  whose pure guard holds and every legal instance, including branch variable
  and phase. DefaultPolicy may decline an offered action; it may not narrow the
  API surface.
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

Top sequential solvers are predominantly Kissat/CaDiCaL derivatives augmented
with techniques such as MAB scheduling and circuit preprocessing. The frozen
SC24–26 reference set is Kissat 4.0.4, kissat-sc2024, AE-Kissat-MAB,
Satsuma+Kissat, and LymphoSAT; LymphoSAT is a SAT-only target. CaDiCaL, Glucose,
and CryptoMiniSat remain mechanism references, not Tier-D performance arms.

For DefaultPolicy, transliterate the references' mechanism and effect, never
their code. Read Kissat and CaDiCaL; choose the stronger mechanism and use
Kissat as the tie-break when equivalent. Preserve a reference-faithful baseline
when adding a stronger design. An unbacked constant is removed or derived, not
tuned. Proof generation is part of every measured workload. Port gaps are
repaired on the reference source's evidence alone: benchmarks measure a
faithful port but never select what gets repaired, and a regression on a
faithful port indicts the implementation or a missing coupled mechanism, never
the reference mechanism.

The implementation boundary is the smallest complete semantic unit in the
reference, not the nearest local branch.  A port includes every piece of state
that determines that branch's meaning—such as its analysis result, retained
trail, candidate classification, conditional predecessors and follow-ups, and
proof/model transition.  A locally sound proxy or positional heuristic is not
a faithful baseline and must not ship as an approximation.  If the complete
unit cannot land in one increment, keep the gap open and use trace-only
instrumentation until its coupled contract is understood.

For every observed reference mismatch, first classify whether Tactix's current
effect is strictly stronger under the same correctness contract.  Keep and
specify a strict improvement; otherwise repair the mismatch.  If neither claim
can be established from source, trace, proof/model evidence, and the rule
semantics, stop and ask the project owner rather than silently choosing one.

Configuration arms are deliberately scarce. Add an arm only for a concrete,
time-bounded attribution or benchmark question, record that question and its
removal condition, and prefer trace-only instrumentation when it can answer the
same question. Remove an arm as soon as it is neutral, superseded, or its
experiment closes. A permanent knob must support a distinct, continuing use
that cannot be covered by the shipped default plus short-lived experimental
instrumentation; historical behavior alone is not sufficient justification.

Feature-off projections are an encouraged divergence-localization tool. Prefer
disabling the same semantic mechanism in Tactix and a reference solver and
compare those matched trajectories one configuration at a time; this both
tests robustness and exposes causal dimensions hidden by the all-default run.
A one-sided disable is also allowed when it answers a narrower attribution
question. Each projection must name the semantic gate, hypothesis, fixture,
trace anchor, and removal condition. It is diagnostic evidence, not permission
to ship a disabled required mechanism, shrink its budget until inert, retain a
permanent experimental arm, or replace the default-on trajectory and
performance gates.

DefaultPolicy contains no magic strategy numbers. Every numeric value that can
change action choice, order, admission, timing, repetition, effort, candidate
scope, or retention is a descriptively named field in the centralized typed
default-strategy configuration, with its reference or derivation recorded.
Use sites do not embed those values. Algorithmic invariants, representation
constants, enum values, sentinels, and diagnostic-only limits are exempt but
must be identifiable as such. A new or changed strategy value lands with
configuration-wiring and shipped-default tests; a narrow annotated allowlist is
required for any exceptional literal.

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
| S1 | `Restart` | none | 1 |
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

Rules use stable kernel primitives and the five proof primitives; options,
statistics, budgets, guards, lazy instances, tests, and proof tests remain local
to the family. CCE is the phase-5 acceptance test: if its implementation needs
unrelated kernel, policy, rule, or proof-backend edits, repair the architecture.

### 5.4 Revamp discipline

At every phase gate, and when profiles or recurring invariants identify a
structural bottleneck, evaluate a coherent redesign through the `revamp` skill.
Define measurable success, preserve N1–N4, revalidate both default and dynamic
policies, and treat revert as a valid outcome.

### 5.5 Codebase hygiene

At every gate and after major features, use `revisit-codebase` to remove dead or
duplicated machinery and route structural issues to §5.4. N1 capability is not
speculative generality and N1–N4 are never traded for simplicity.

### 5.6 Learned proof-rule scheduling

Phase 7 learns rule/batch choice online within one solving run. DefaultPolicy is
a permanent arm. Learning never re-solves subproblems, discards solver state, or
creates a separate tuning phase. Begin with replay-validated reward proxies and
windowed UCB over macro choice points, then add tabular context and composed
batch experiments under explicit kill criteria. LearnedPolicy joins proof/model
fuzzing and must remain anytime. Gates are in §8.5.

## 6. Competition compliance

Accept DIMACS CNF robustly; emit standard `s`/`v` lines and exit codes 10/20;
write the requested proof; handle SIGINT/SIGTERM; enforce memory limits; package
licenses, attribution, build/run scripts, and a solver description. Automate the
contract in `tools/compete/check.sh` through the `compete` skill.

## 7. Performance strategy

Match reference data-structure discipline, profiling before structural changes.
Implement reference-faithful scheduling before tuning. Optimize hot paths without
weakening the API. Evaluate search, clause quality, inprocessing yield, proof
cost, and memory separately. Novel engineering follows phase-4 parity in the
pinned phase 4.5 pass.

## 8. Evaluation plan

### 8.1 Benchmark tiers

All solver executions use SLURM partition `cpu-dense-hwulab-q`; one
single-threaded solver per array task. Tiers A–C use 32 GB provisioned as four
CPUs at 8 GB/core. Tier D uses the same 32 GB process limit and a 5000 s solve
limit. Record allocation shape; submitted arrays own immutable builds and
manifests. Every UNSAT result is independently proof-checked and every SAT
result model-checked.

Concurrent solver arms use distinct immutable `build/pin-*` directories; each
manifest records the source commit and binary hash. A queued or running job's
pin is never rebuilt, renamed, or shared with another arm. After the owning
campaign is terminal and aggregated, remove an ephemeral pin only after
confirming that no queued/running job or retained manifest references it.
Long-lived development builds are limited to `build/release` and `build/debug`;
durable frozen campaigns carry their own pins under the campaign directory.

A single benchmark task atomically entails solve, proof/model check when a
verdict exists, durable recording of verdict, solve/check time and peak memory,
certificate size/hash and checker provenance, then deletion of the certificate
and every checker-derived proof artifact. Deletion is mandatory after success,
checker failure, timeout, crash, cancellation, or an unclassified result; a
partial proof is not diagnostic authority. No task is complete and no campaign
may be scored while a certificate remains outside its live solver run's scratch
directory. Each checker stage deletes its consumed input immediately; the task
deletes the remaining scratch before publishing its result. Experimental
runners and aggregate checks enforce this
contract; diagnosis reproduces from the pinned solver and manifest.

An early process exit without a valid SAT/UNSAT result is a blocking incident,
not an ordinary performance loss. Preserve its raw exit status, signal, and
diagnostic output; first distinguish the configured timeout, scheduler
cancellation/preemption, and an enforced memory limit or confirmed OOM. If
those do not explain the exit—such as a crash, assertion failure, sanitizer
finding, or unexplained nonzero status—all unrelated development and
performance interpretation pause, and owned in-flight performance jobs whose
results depend on the faulty binary are stopped. Resume only after the same
immutable-pin fixture has a causal reproducer, the defect is fixed and covered
by a regression test, its SAT model or UNSAT proof validates, and the affected
smoke/correctness gate is clean. Benchmark tooling must not flatten this class
into an undifferentiated `UNKNOWN` that can be mistaken for a timeout.

| Tier | Purpose | Set and limit | Cadence |
|---|---|---|---|
| A | correctness/smoke | generated + crafted, 30 s, 32 GB | every commit |
| B | daily signal | 28 frozen SC instances, 300 s, 32 GB | daily/change |
| C | scheduling signal | 30 frozen instances, 1200 s, 32 GB | weekly/major change |
| D | gate scale | SC24+SC25+SC26 union, 1166 unique, 5000 s, 32 GB | frozen reference + phase gates |

The three annual memberships (1200 total) are scoring views over the 1166
unique runs. Never tune on Tier A or mix set versions or resource contracts.
Per-instance results remain authoritative; PROGRESS carries only a headline.

The five reference solvers run exactly once per valid frozen campaign. A run
invalidated by infrastructure or storage failure is not result authority and
is replaced in a new campaign directory rather than repaired in place. Their
immutable authority is
`benchmarks/baselines/sc24-26-32g-5000s/`, containing manifests, pins,
per-instance solve/check metrics, integrity metadata, and a compact generated
LaTeX/PDF report under `report-to-andrew/`, but no successfully checked
certificates. The report is a view of the frozen tables, never a second result
authority. Future time caps are exact truncations of these records.
Lower memory caps are simulations from recorded high-water data and must be
labeled as such; a peak alone cannot determine when memory was crossed. Never
rerun a frozen reference to fill a missing metric—record the limitation.

Until Phase 4.5 begins, divergence repair and convergence iteration use a
separate **debug comparison contract**: new Tactix campaign tasks receive a
16 GiB process limit (two CPUs at 8 GiB/core) and a 1200 s solver timeout. Do
not rerun any reference solver. Instead, derive the matched reference view from
the immutable 32 GiB/5000 s baseline records: truncate solve times exactly at
1200 s, and count a validated baseline solve as eligible under the simulated
16 GiB cap only when its recorded peak RSS is at most 16 GiB. Label every such
comparison as simulated; a result above the recorded memory threshold is a
simulated non-solve, not evidence of the exact crossing time or of an OOM.
This debug contract accelerates iteration and does not replace the canonical
32 GiB/5000 s Tier-D gates.

Submit these debug arrays without an array-concurrency throttle (no `%N`
suffix) or a hard node constraint. Expose the full task array so SLURM can use
all available capacity, including `node017,node018`. Keep one single-threaded
solver per task, the immutable-build and certificate-cleanup contracts, and
enough job wall time for post-solve proof/model checking.

This resource contract applies to the whole pre-Phase-4.5 debugging phase. Every
candidate must pass the full frozen Tier B and a predeclared, mechanism-relevant
Tier C regression subset before it advances to the targeted Tier-D witnesses.
Each debugging round may carry up to ten predeclared Tier-D improvement
witnesses.  These witnesses are selected from matched Kissat-over-Tactix gaps
and are used to prioritize and measure general repairs; they are not promoted
into the regression set, and a repair need not affect all ten through one
mechanism.  Keep the set cumulative where practical so fixes already obtained
remain visible while the remaining gaps receive divergence analysis.
An N4 repair is accepted only as a general mechanism: the witness may reveal
the gap, but must not parameterize the fix.  Instance names/hashes, tier ranks,
per-instance default arms, deadline shifts, and tuned constants are forbidden.
The implementation must follow a reference execution path or carry an explicit
strength argument and accepted-deviation disposition, with focused mechanism
coverage plus the full Tier-B/selected-Tier-C iteration guard.
Choose the Tier C subset from existing causal evidence and include, where the
mechanism has them, a productive witness, its worst measured cost case, and a
low- or zero-yield case; record the exact membership in the campaign manifest
before submission. Compare candidate and shipped Tactix in the same allocation
window. Every SAT model and UNSAT proof must validate, and an unexplained solve
loss blocks the candidate. The selected Tier C subset is an iteration guard,
not a replacement for the complete Tier C or canonical Tier-D gates.

Every instance in a frozen tier remains in its scored population. Replication,
leave-one-out sensitivity, and unrelated-perturbation tests control confidence
and causal attribution only; they never create a may-not-gate exemption, erase
a solve loss, or waive the obligation to pursue every instance that original
Kissat solves under the matched resource contract. A rule-neutral view may
isolate mechanism parity, but it never replaces the full-tier solved-count gate.

### 8.2 Correctness

Every relevant run checks SAT models against the original CNF, UNSAT proofs
with independent checkers, differential status against references, and internal
invariants. The Tier-D campaign follows the SC2026 Main-track contract: the
solver writes `proof.out` in its supplied output directory, uses its registered
DPR, GRAT, or VeriPB pipeline, and an UNSAT result whose checker fails or times
out is unsolved. SAT output includes a competition-valid (possibly partial)
model. Fuzz uniform, structured, mutated, circuit-derived, and fixed
degenerate inputs with recorded seeds. Random-policy and targeted stress-policy
legs exercise legal schedules; validate every oracle with known-bad artifacts.
Shrink failures and retain minimal regressions. Cadence and commands live in the
`fuzz`, `proof-check`, and `stress-policy` skills.

**External debugging escalation.** Invoke Claude when fuzzing or unit testing
discovers a soundness defect or a crash. The primary agent owns performance
divergence, runtime regression, mechanism-fidelity work, and ordinary
implementation debugging without delegating the fix to Claude. If an ordinary
investigation later produces a fuzz/unit-test reproducer that demonstrates
unsoundness or a crash, escalate at that point; the trigger is the reproduced
failure class, not the difficulty of the repair.
For every such escalation, the Codex agent must launch Claude with permission
to edit the workspace, provide the reproducer and bounded repair scope, and
wait until the Claude agent finishes. Codex then reviews and validates Claude's
changes and remains responsible for all soundness gates and final integration.

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

**Causal convergence.** For every coverage or runtime gap, localize the earliest
portable semantic divergence with the structured rule journal. Repair it unless
Tactix is demonstrably stronger or the deviation is explicitly accepted; then
rerun the instance and cumulative guards. Record context, cause, relative
strength, evidence, reproducer, owner, and reopening condition. Feature-off
projections diagnose one semantic mechanism but never replace a full gate.
After the complete T4-K pass, the first divergence set is at most twelve
coverage losses where validated Kissat finishes within 300 s and Tactix does
not finish within 5000 s, balanced across SAT and UNSAT when possible and then
ordered by Kissat time. Diagnose this compact set iteratively; frozen Kissat
measurements remain scoring authority, while instrumented reruns are diagnostic.

Tier-D convergence is cumulative: A closes T4-K; B adds kissat-sc2024; C adds
AE-Kissat-MAB; D adds Satsuma+Kissat; E adds LymphoSAT's SAT solves. At stages
B–E Tactix must preserve prior coverage and be within 1.10× the cumulative
status-sensitive VBS on every annual view and the union. A single
feature-driven Tactix binary is used throughout: content-derived features are
allowed, benchmark names, paths, hashes, identities, and external-solver output
are not. Final T4-VBS requires exact coverage parity, not only the 1.10× score.

Report #solved before PAR-2, SAT/UNSAT split, proof time/size, regressions, and
memory. A solve gained with none lost may be banked despite PAR-2 cost, but the
cost gets a named closer. A staged regression requires a pre-registered sequence,
mechanism, ceiling, recovery point, revert path, and kill criterion; it cannot
cross a gate or cover N1/N2.

### 8.4 Flexibility

Default dynamic-macro/static-micro overhead is ≤5% versus a hardcoded twin.
Demonstrate pure DPLL, CDCL without restarts/inprocessing, scripted reproduction
of default scheduling, preprocessing-only, random macro scheduling, and external
branching without kernel edits. Ablations use configuration, not forks.

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
| 4.25 | staged Tier-D convergence A–E | T4-VBS |
| 4.5 | proof-event correlation and novel engineering | proof ranges map to emitting rule/application |
| 5 | tuning, bindings, extensibility proof | ≤5% overhead + CCE test |
| 6 | competition packaging | T5/best effort + submission ready |
| 7 | learned scheduling | L-safe then L-win |

Phase 4.20 writes `docs/design/phase-4.20-proof-rule-survey.md` and
`docs/design/phase-4.20-solving-strategy-survey.md`. A proof-rule candidate must
state calculus precondition/effect, DRAT/LRAT delta, reconstruction impact,
rule/strategy ownership, and N1/N2 tests. A strategy candidate must state its
features, decision point, reference source, cost model, attribution experiment,
and kill criterion. Only reviewed candidates enter 4.25/4.5 implementation.

Phase 4.5 candidates include proof-event correlation, richer Sweep consequences,
extended resolution, faster Factor matching, and `egg`-style batched Congruence
rebuilding at safe root boundaries. Each must preserve the legal rule surface
and independently checkable native proofs and earn a measured time/memory gain.

## 10. Risks and mitigations

- **Abstraction overhead:** static hot loop, measure against hardcoded twin.
- **Proof unsoundness:** independent checkers, negative oracle tests, stress
  schedules, regression corpus.
- **Model reconstruction bugs:** validate every SAT model against original CNF.
- **Benchmark noise/overfitting:** same-window baselines, fixed tiers, Tier C/D
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
