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

The frozen SC24–26 performance set is Kissat 4.0.4, kissat-sc2024,
AE-Kissat-MAB, Satsuma+Kissat, and SAT-only LymphoSAT. CaDiCaL, Glucose, and
CryptoMiniSat remain mechanism references, not Tier-D arms.

For DefaultPolicy, transliterate mechanism and effect, never code. Read Kissat
and CaDiCaL, choose the stronger mechanism, and use Kissat as the tie-break for
equivalent designs. Preserve a faithful baseline beside any stronger design;
derive or remove unbacked constants, never tune them. Proof generation belongs
in every measured workload. Reference source selects port repairs and
benchmarks measure them, never the reverse; a faithful-port regression
indicates an implementation error or missing coupled mechanism, not a reason
to weaken the reference.

Port the smallest complete semantic unit, including all state that gives an
observed branch meaning: analysis result, retained trail, candidate class,
conditional predecessors/follow-ups, and proof/model transition. A local proxy
or positional heuristic is not a faithful baseline. If the unit cannot land
whole, keep the gap open and trace until its contract is understood. For each
mismatch, retain and specify a provably stronger Tactix effect under the same
correctness contract; otherwise repair it. If source, trace, proof/model
evidence, and semantics establish neither conclusion, ask the project owner.

For convergence work, equivalent sets or terminal results are not enough when
the reference consumes order or physical state. Compare the earliest common
state before a trajectory split and include every decision-observable part of
the mechanism: iteration and admission order, retained clause/watch layout,
propagation order, heuristic update order and values, heap/queue topology, and
the clocks charged by the operation. Do not infer a cause from a later semantic
divergence while an earlier common-state mismatch remains unexplained. A
repaired discrepancy lands with a differential regression test for the exact
state transition that was wrong. Performance evidence promotes the repair only
after that mechanism check; it does not excuse or conceal an upstream parity
gap.

Once source and common-state evidence establish a sound, general reference
repair, retain it through downstream performance regressions and investigate
the next coupled mechanism from that honest baseline. A regression can expose
another missing state transition, ordering rule, or representation cost; it is
not by itself evidence that the established repair was wrong. Discard such a
repair only if stronger evidence disproves its reference fidelity, correctness,
generality, or an N1--N4 obligation. Record and bound diagnostic alternatives,
but do not restore an older compensating behavior as the production default.

Add a configuration arm only for a concrete time-bounded question with a
removal condition; prefer trace-only instrumentation and remove neutral,
superseded, or completed arms. A permanent knob needs a distinct continuing
use beyond the shipped default and temporary instrumentation; historical
behavior alone is insufficient.

Prefer matched feature-off projections in Tactix and the reference, with a
one-sided disable allowed for narrow attribution. Name the semantic gate,
hypothesis, fixture, trace anchor, and removal condition. A projection is
diagnostic: it cannot justify shipping a required mechanism disabled or inert,
retaining an experimental arm, or replacing default-on performance gates.

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

At phase gates or structural bottlenecks, use `revamp` to define measurable
redesign success, preserve N1–N4, revalidate default and dynamic policies, and
treat revert as valid.

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

All solver executions use SLURM partition `cpu-dense-hwulab-q`, with one
single-threaded solver per task. Allocate 8 GiB per **physical** core. Because
the current nodes use SMT-2, request two SLURM logical CPUs per physical core:
for example, 32 GiB uses four physical/eight logical CPUs. Tier D uses a 32 GiB
SLURM task-cgroup limit and 5000 s solve limit. Record the allocation shape,
independently check every UNSAT proof and SAT model, and keep these execution
invariants:

- Concurrent arms own distinct immutable `build/pin-*` directories and
  manifests recording source commit and binary hash. Never mutate or share a
  queued/running pin. Remove an ephemeral pin only after its campaign is
  terminal and aggregated and no job or retained manifest references it.
  Long-lived development builds are only `build/release` and `build/debug`;
  frozen campaigns retain pins under their campaign directories.
- One SLURM job or array task may execute only one solver invocation. Never
  serialize predecessor and candidate solver runs back-to-back inside one
  allocation; submit independent arms so they remain parallelizable subject to
  available resources. Targeted Phase-4.25 diagnostics should prefer
  `node018` when it is available, while full-width arrays retain the no-hard-
  node-constraint contract below.
- A task atomically solves, checks any verdict, records verdict, solve/check
  time, peak memory, certificate size/hash, and checker provenance, then deletes
  every certificate and checker derivative on every exit path. A task with
  residue is incomplete and its campaign is unscorable; reproduce diagnostics
  from the immutable pin and manifest, not a retained partial proof. Each
  checker deletes its consumed input immediately, and the task clears remaining
  scratch before publishing its result.
- Proof checking is asynchronous during runtime iteration. After an expected
  solver verdict, treat pending checks as provisional passes for continuing
  investigation, implementation and subsequent measurements, including dependent
  work; do not wait for certificates before proceeding. Keep checkers running,
  retain pin-to-successor dependencies, and revisit outcomes at checkpoints.
  Label pending results unverified: provisional continuation does not close a
  checked gate or authorize final promotion. A rejected proof/model immediately
  pauses affected dependent work and invokes the §8.2 repair workflow.
- An early exit without a valid result is a blocking incident, not a performance
  loss. Preserve its status, signal, and diagnostics; distinguish timeout,
  scheduler cancellation/preemption, and enforced or confirmed OOM. A crash,
  assertion, sanitizer finding, or unexplained nonzero exit pauses unrelated
  development and performance interpretation and stops dependent owned jobs
  until the pinned fixture has a causal reproducer, regression-tested fix,
  validated proof/model, and clean affected correctness gate. Tooling must not
  flatten this class to `UNKNOWN`.

| Tier | Purpose | Set and limit | Cadence |
|---|---|---|---|
| A | correctness/smoke | generated + crafted, 30 s, 32 GB | every commit |
| B | daily signal | 28 frozen SC instances, 300 s, 32 GB | daily/change |
| C | scheduling signal | 30 frozen instances, 1200 s, 32 GB | weekly/major change |
| D | gate scale | SC24+SC25+SC26 union, 1166 unique, 5000 s, 32 GB | frozen reference + phase gates |

The three annual memberships (1200 total) are scoring views over 1166 unique
runs. Never tune on Tier A or mix set versions or resource contracts.
Per-instance results are authoritative; PROGRESS carries only a headline.

The five reference solvers run once per valid frozen campaign. Infrastructure-
or storage-invalidated runs are replaced in a new directory, never repaired in
place. `benchmarks/baselines/sc24-26-32g-5000s/` owns their manifests, pins,
per-instance metrics, and integrity metadata; its generated Andrew report is a
view, not result authority, and checked certificates are deleted. Derive future
time caps by exact truncation. Label lower-memory projections as simulations
from peak RSS: they neither locate the crossing time nor prove OOM. Never rerun
a frozen reference to fill a missing metric.

Before Phase 4.5, the **debug comparison contract** gives ordinary hard-memory
Tactix tasks two physical/four logical CPUs, a SLURM `--mem=16G` task cgroup,
and 1200 s solver timeout. Do not add `RLIMIT_AS`: virtual reservations are not
resident memory and can reject a task below the intended physical-memory
ceiling. Derive the matched reference view without rerunning: truncate the
immutable 32 GiB/5000 s record
at 1200 s and admit a validated solve under the simulated 16 GiB cap only when
recorded peak RSS is at most 16 GiB. Label the projection and any memory-based
non-solve as simulated. This iteration contract never replaces canonical gates.
Submit full-width arrays without `%N` or hard node constraints, including
`node017,node018`, while retaining one solver per task, immutable pins,
certificate cleanup, and enough checker wall time.

Treat memory convergence as an independent acceptance dimension. On matched
current-Tactix/current-Kissat jobs with the same input, proof mode, and resource
contract, a Tactix solve loss or materially higher peak memory has priority over
a runtime-only gap. Merely falling below the task-cgroup cap does not close it:
require comparable peak and live-state ownership, or a source-backed general
reason for any residual excess. Attribute formula, search, inprocessing, proof,
and checker memory separately; a resource exit is not a correctness failure.

Every debug candidate passes full Tier B plus a predeclared mechanism-relevant
Tier C subset before targeted Tier D. A round may carry up to ten predeclared
Kissat-over-Tactix improvement witnesses; they prioritize general
repairs but do not become regressions or require one shared mechanism. The
witness may reveal an N4 gap but never parameterize its repair: names, hashes,
ranks, per-instance arms, deadline shifts, and tuned constants are forbidden.
Follow a reference path or document a stronger effect and accepted deviation,
then require focused coverage and the Tier-B/Tier-C guard. Keep witnesses
cumulative where practical. Predeclare Tier C from causal evidence with
productive, worst-cost, and low/no-yield cases where available, recording exact
membership in the manifest before submission; compare shipped and candidate
Tactix in the same window. Validate all proofs/models; an unexplained solve
loss blocks promotion. This subset and rule-neutral projections are
diagnostics, never full Tier C/D replacements.

Every frozen-tier instance remains scored. Replication, leave-one-out, and
unrelated perturbations improve confidence and attribution; they create no
gate exemption and never waive a matched Kissat solve.

### 8.2 Correctness

For every change, explicitly consider whether new or updated unit tests are
needed. Check coverage of the affected behavior, boundaries, and failure modes;
add focused tests for uncovered cases. When no test change is needed, record a
brief coverage-based reason in the change's validation summary. This applies
to implementation, policy, refactoring, tooling, and documentation changes;
it does not replace the existing correctness or benchmark gates.

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

**Claude-owned fuzzing.** Every time a solver fuzz job is needed, the Codex
agent must invoke Claude Code to prepare and submit that job and to own its
monitoring, artifact inspection, oracle interpretation, cleanup audit, and
terminal report. Codex supplies the bounded candidate, immutable pins, stress
scope, resource contract, and acceptance criteria, then may continue independent
development while Claude runs the campaign in the background. Codex does not
probe the fuzz job, logs, retained cases, or intermediate/final artifacts, or
independently recompute or reinterpret the report. Claude's
terminal report is authoritative: `pass` closes the fuzz gate; `failure`
pauses dependent work and hands a bounded reproducer and failure class to
Codex for direct repair under §8.2. Codex does not rerun a passing fuzz
campaign for confirmation. This ownership rule is specific to fuzzing; normal
unit tests, benchmark guards, proof witnesses, and performance diagnostics keep
their existing independent-validation contracts. Run periodic checkpoints after
meaningful solver-change batches during extended runtime work, not only at final
consolidation; unchanged rebuilds and diagnostic-only work need no duplicate
campaign. Pending fuzz results do not pass a gate or authorize promotion. A
reported correctness failure pauses dependent work and triggers the repair path.

Each generated fuzz leg uses 50 physical cores when cluster capacity permits,
not 50 SMT threads: on the current SMT-2 nodes this is 100 logical CPUs,
400,000 MiB of scheduler memory, and 50 harness workers (one per physical
core). Claude records the requested allocation, worker count, and achieved
concurrency in its terminal report. Preparation, small fixed-corpus checks, and
intrinsically narrower reachability probes may use less with a recorded reason;
a generated fuzz leg may not silently fall back to one or two workers. This is
parallelism within the one required campaign, not campaign replication, and it
does not change seeds, case counts, or acceptance oracles.

Every submitted fuzz leg owns an immutable, read-only snapshot of all harness
scripts, Python modules, stress scenarios, and corpus inputs it executes.  Its
job log records the snapshot's aggregate SHA-256.  A batch job must never
execute a live repository script: editing a script while Bash is waiting for a
child can otherwise shift the interpreter's saved file offset and turn a clean
oracle result into a post-oracle shell failure.  Remove the snapshot only after
a successful terminal exit; retain a failed job's snapshot for the fuzz owner's
failure handoff and cleanup audit.

**Direct debugging ownership.** Codex diagnoses and repairs implementation,
performance, unit-test, crash, sanitizer, proof/model, and soundness failures
directly; Claude debugging or repair review is not required. Preserve the
original failure, establish a causal reproducer, add a regression, and validate
the fix with the affected invariants and independent proof/model oracles before
resuming dependent work. Do not weaken correctness or promotion gates.
Claude still owns fuzz execution, monitoring, interpretation, and cleanup as
specified above. For a fuzz failure, Claude supplies a bounded repair handoff
with the reproducer, command, and failure class; Codex may inspect and repair
that handoff, but does not independently inspect the campaign or reinterpret
its report. A repair does not itself close the required fuzz gate.

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

**Targeted timing measurement.** A long-instance performance comparison used
to accept or reject a solver repair runs as three independent paired waves.
Each wave submits the candidate and its exact-state baseline as two separate,
concurrent one-solver jobs; never serialize arms in one allocation, and cross
their node/L3 placement over the three waves when practical. On the current
EPYC 9454 nodes, each timed job requests 12 logical CPUs and 48,000 MiB from
SLURM so its mask owns one complete six-physical-core L3 domain and its scheduler
memory reservation matches the partition's effective 8,000-MiB-per-physical-
core capacity. The exact MiB request lets all 16 L3 domains fit within the
node's configured 769,024 MiB; do not round it up to `--mem=48G`.
The solver itself remains single-threaded and bound to one granted hardware
thread. Do not add an address-space limit to this 48,000-MiB timing allocation;
record peak RSS and VmPeak, score 16-GiB compatibility from measured peak RSS,
and confirm a decision-changing hard-memory claim separately in a SLURM
`--mem=16G` task. The larger timing allocation is capacity accounting, not
permission to ignore memory competitiveness. Stage inputs,
proofs, and checker work on verified node-local scratch, and record topology,
cross-partition occupancy, perf service-rate counters, proof/check identities,
and cleanup. Decide primarily from the median of the three paired
candidate/baseline ratios and report its complete range, never best-of. An
absolute runtime headline is admissible only when each arm's complete
shared-node range is at most 2%; otherwise retain the paired result but reject
the absolute observation as environmentally unstable. Proof-on solver time,
proof-off diagnostic time, and checker time are separate measurements and may
not be priced by subtracting unmatched runs. Never request whole-node
exclusivity or all-memory reservation for timing or diagnosis.

This three-wave rule applies only to decision-grade performance evidence. It
does not repeat or multiply fuzzing: each required fuzz checkpoint remains one
Claude-owned campaign under §8.2. It likewise does not multiply correctness
gates or ordinary diagnostic probes; those retain their predeclared contracts.
Full-width tier arrays retain their no-throttle, no-node-constraint allocation
contract and are interpreted through aggregate evidence rather than this
targeted timing protocol.

**Fixed Rank 1–50 reference.** The runtime target is aggregate ≤1.01× Kissat
over the approved fifty: preserve the original forty and append the seven UNSAT
and three SAT selections in `benchmarks/baselines/rank1-50-selection-v1/`.
Keep the frozen Rank 1–40 Kissat vector; measure ranks 41–50 once on the same
12-logical-CPU/48,000-MiB contract and append those values, never cpu-q timings.
Future full-view arrays run Tactix only with matching inputs, options, proof
mode and limits. No routine reference refresh or best-of denominator. These
cross-window scores do not isolate unit cost; targeted predecessor waves and
missing native diagnostics remain allowed. Pending checks permit provisional
work, not acceptance. Correctness, B28+C5 and memory gates remain unchanged.

**Causal convergence.** For every coverage or runtime gap, localize the earliest
portable semantic divergence with the structured rule journal. Repair it unless
Tactix is demonstrably stronger or the deviation is explicitly accepted; then
rerun the instance and cumulative guards. Record context, cause, relative
strength, evidence, reproducer, owner, and reopening condition. Feature-off
projections diagnose one semantic mechanism but never replace a full gate.

**Phase-4.25 consolidation workflow.** Vivify/collection, Substitute, residual
BVE, and Congruence investigations may proceed without an intervening fuzz
campaign only while every repair remains isolated behind its own immutable
pins, manifest, focused tests, and performance evidence. Such a repair is
explicitly *unfuzzed investigation material*: it is neither promoted nor final.
At the consolidation point, compose only the independently supported repairs,
run the complete B28 plus predeclared C5 checked gate on that combined
candidate, and then invoke one Claude-owned fuzz campaign before the required
report, commit, and push. That fuzz/wrap-up checkpoint does not terminate
convergence: immediately resume the next demonstrated current-Tactix/current-
Kissat gap from the newly accepted pin. A soundness defect, crash, sanitizer failure,
rejected SAT model, or rejected UNSAT proof discovered at any earlier stage
immediately interrupts this batching and triggers Codex's direct debugging and
repair workflow in §8.2.

**Proof-rule/strategy boundary during convergence.** A repair may enlarge a
rule's general calculus or the kernel's proof-neutral representation machinery,
but it must not move candidate admission, cadence, effort allowance, deadlines,
or benchmark-conditioned choice into that rule or kernel primitive. The rule
owns preconditions, semantic effect, proof/reconstruction delta, and legal
instances; the strategy owns when and how much of that legal surface to invoke.
Representation optimizations must preserve this separation even when they are
first motivated by one scheduled inprocessing rule, and the automated
rule/policy boundary check remains a required focused guard.

**Worse-side-first divergence priority.** The journal must record every exact
portable mismatch, but investigation and repair effort prioritizes the earliest
divergence at which Tactix is measurably worse along either of two axes:

1. **Throughput:** at a common semantic state, Tactix performs the same
   normalized logical work but takes materially more wall time, CPU time, or
   memory traffic to do it.
2. **Work quality:** over comparable elapsed time or normalized cost, Tactix
   produces a weaker result--for example fewer useful learned clauses, longer
   learned clauses at comparable validity and scope, fewer eliminations or
   simplifications, less pruning, weaker retained state, or a worse validated
   solve trajectory.

Compare both axes for each localized rule invocation: matching ticks alone does
not establish parity when their real cost differs, and matching runtime alone
does not establish parity when the resulting state is weaker. Raw counts are
diagnostic rather than intrinsically better; judge their utility by portable
semantic consequence and downstream validated trajectory. Also record greater
logical work, later scheduling, and higher retained memory when they explain a
worse result. A Tactix-stronger or behaviorally neutral mismatch is documented
and may be accepted without convergence; do not spend a campaign making it
reference-identical unless it blocks localization of the first worse boundary,
violates an exact contract, causes a cumulative regression, or lacks a sound
general justification. When several mismatches coexist, compare their
downstream impact and pursue the earliest causal worse boundary rather than the
earliest difference in raw trace order.

**Exact inprocessing effort parity.** The diagnostic contract covers every
inprocessing rule, not only the rule implicated by the current witness. At each
common portable semantic state, its normalized ledger records the rule and
invocation, effort reference and scaled allowance, entry/exit logical ticks,
overshoot, and terminal budget comparison. Candidate-oriented rules also record
portable candidate identity and order, candidate entry/exit ticks and exact
delta, disposition, cost-category totals where the reference exposes them, and
the semantic set, physical pose, and lineage of every rewrite. Clause ids,
arena references, benchmark identities, and wall time are diagnostic metadata,
never alignment keys or solver controls. Require exact candidate frontier and
logical-cost agreement with the reference while states remain common; after a
semantic or representation divergence, first localize that owner rather than
forcing counters to agree across different work. Keep this instrumentation
compile-time gated and disabled by default, with replay/normalization tests.
For every matched invocation also record elapsed CPU/wall time and normalized
yield (learned-clause count and length distribution, eliminations,
simplifications, pruning, and retained-state deltas where applicable), so a
rule that matches logical ticks but runs slower--or consumes similar time while
producing weaker work--is treated as a first-class divergence.

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
