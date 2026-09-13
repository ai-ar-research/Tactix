# WV100 mechanism integration and experiment protocol

## Decision

WV100 is the permanent development set for the post-refactor solver work. Its
100 rows are fixed in `benchmarks/tiers/WV100.tsv`; a row stays in the set after
Tactix solves it. The objective is independently checked Tactix coverage of all
100 rows within 1,200 solver seconds and 32 GiB per row, while preserving N1--N4,
B28+C5, Rank 1--50, memory, proof, model and fuzz gates.

The rows are deliberately strong signatures. Each was solved under the frozen
competition contract by exactly one checked configuration: 57 by
Satsuma+Kissat, 34 by LymphoSAT, seven by AE-Kissat-MAB and two by
kissat-sc2024. Existing checked baseline results establish those signatures;
reference solvers are not rerun. Current and future Tactix results only update
the numerator.

Tactix will absorb general mechanisms that explain these wins. It will not run
external solvers as stages, sequence internal solver personalities as a
portfolio, or select behavior from a benchmark name, path, hash, family label
or position in WV100. Formula structure may select a sound rule instance in the
same way that any ordinary rule guard does. MAB scheduling remains an open
design choice until the seven AE rows are attributed.

## Experimental loop

Each mechanism increment follows the same sequence.

1. **Attribute.** Read the official submission description and available
   source. Identify the specific transition that changes the formula, search
   state or branching behavior. A solver label or family match is not a
   mechanism attribution.
2. **Reproduce.** On the affected WV100 rows, capture the state immediately
   before and after that transition. The useful signal is a change from the
   current Tactix timeout or stalled search to a quickly checked solve. Record
   negative controls from structurally similar rows where the transition is
   inapplicable or ineffective.
3. **Generalize.** State a calculus rule or a proof-neutral kernel mechanism
   using formula and solver state only. Define legal instances, soundness
   preconditions, state effects, model reconstruction and proof obligations.
4. **Expose.** Implement the mechanism through the shared typed application
   interface. Settings own every heuristic threshold, admission bound, effort
   limit, search limit and scheduling decision. Rules retain only soundness and
   representation constants.
5. **Verify.** Check focused models or native DRAT/LRAT proofs, explicit and
   enumerated instances, interruption and online resumption. Claude Code owns
   the required fuzz checkpoint; Codex owns implementation and debugging.
6. **Measure.** Count checked WV100 transitions and solves, then run the
   periodic B28+C5 and Rank 1--50 regression gates. Promote only a coherent
   general mechanism whose coverage gain survives the correctness and
   performance gates.

Experiments use immutable solver pins and record the commit, binary hash,
instance hash, arguments, seed, rule settings, allocation, timeout, result,
checker identity, peak memory and cleanup outcome. A short probe may establish
a large causal transition, but it does not establish final WV100 coverage; a
five-minute timeout is inconclusive for a row whose checked winner needed more
than five minutes.
Timing close enough to affect a regression decision uses repeated matched
measurements under the three-wave/2%-range rule in the development plan.

## Resource contracts

| Purpose | Partition | CPUs/task | Memory | Solver / checker limit |
|---|---|---:|---:|---:|
| Focused WV100 mechanism development | `cpu-dense-hwulab-q` by default | 8 | 32 GiB | 300 s / recorded per run |
| Qualitative large-effect probe | `cpu-q` is allowed | record the allocation | 32 GiB | at most 300 s / recorded per run |
| B28+C5 regression gate | `cpu-dense-hwulab-q` | 12 | 32 GiB | 1,200 s / 16,000 s |
| Rank 1--50 regression gate | `cpu-dense-hwulab-q` | 12 | 32 GiB | 1,200 s / 16,000 s |
| Final WV100 coverage | `cpu-dense-hwulab-q` | 8 | 32 GiB | 1,200 s / 16,000 s |

Every task runs one solver thread and has no node constraint. Arrays are full
width unless a correctness or storage constraint is recorded. The current
Tier-D array remains the separately authorized `cpu-q`, four-CPU, 32-GiB,
1,200-second campaign and is not altered to match this protocol.

All real-instance solver runs go through SLURM. Formula, proof, model, converted
proof and checker scratch are anonymous node-local files. The runner installs
cleanup for normal exit, solver timeout, wall timeout, crash, signal, OOM and
cancellation; a missing cleanup/result acknowledgement makes the task
incomplete. Only bounded manifests, statistics and checker summaries return to
the repository. The storage guard checks the full repository every ten minutes;
above 700 GiB it holds new pending repository jobs for ten minutes, never pauses
running jobs, and releases only holds it created.

The final WV100 numerator uses one frozen binary and one fixed strategy program
with identical settings on all 100 rows. Per-row configurations are diagnostic
evidence and do not count toward 100/100. The final configuration also runs the
B28+C5 and Rank 1--50 gates. A legal composition operates on one persistent
kernel: clauses, units, learned clauses, reconstruction state, phases and rule
state survive between actions. Discarding that state, restarting from the input
CNF or budgeting a specialist before launching a fresh fallback is a sequential
portfolio and is forbidden.

## N1 and online-strategy requirements

Every new rule must preserve the completed strategy refactor rather than add a
special execution path. Its public surface must provide:

- validated typed per-call parameters and a disable value where applicable;
- every legal explicit instance, including branch variable and phase if the
  rule branches;
- complete enumeration that is not silently truncated by an unconfigurable
  prefix, candidate cap or effort constant;
- single-instance, caller-selected batch, round and closure granularity when
  those granularities have semantic meaning;
- freely reordered and repeated calls and sequences;
- repeat-to-scoped-fixpoint, explicit finite budget and external stop;
- deterministic seed replay and safe interruption, online reconfiguration and
  resumption without reconstructing the solver.

For substitution redundancy, the complete explicit domain is a candidate
clause plus a finite substitution mapping over the current solver variables.
An additive typed v2 request carries that structured payload; the frozen v1
`uint64_t` instance API remains unchanged. Validation is transactional and a
request becomes `Stale` when the existing arena/touch/strategy mutation token
changes. The general lazy cursor has an unbounded mode that eventually visits
the finite explicit domain without a hidden prefix. Structured detectors are
accelerating producers, not the definition of the legal domain; all their
support, orbit, depth and work limits are validated strategy settings.

Preprocessing, inprocessing, Lucky and Walk actions execute at level zero.
Search-time callers reach that boundary through the explicit `Restart` action.
This declared boundary is part of the solver contract; it does not excuse an
otherwise missing legal rule instance. The broad random strategy uses this same
surface and must exercise effective transitions, noncanonical instances,
orders, repetitions, parameters, granularities, budgets and phases, with replay
and negative controls.

Online strategy reconfiguration is distinct from incremental CNF extension.
The v1 API declares IPASIR-style addition after reasoning outside its contract
and O7 remains open. S1 does not silently broaden that contract. If later work
admits post-SR clause addition, it must first retire every SR-dependent state or
prove the extended formula safe under a separately reviewed incremental rule.

## Mechanism stages

### S0: freeze and attribution inventory

Validate the TSV against its source ranking: exactly 100 unique rows, ranks
1--100, 33 SAT and 67 UNSAT, with the 57/34/7/2 sole-winner split. Merge arriving
current Tactix results without changing membership. For each row, record a
mechanism hypothesis, source location, transition reproducer, negative control
and proof/model route. Unknown attribution stays unknown rather than becoming a
family-specific policy.

Before S1 implementation, run three falsification gates. G0 classifies each
emitted Satsuma obligation as a direct substitution image, RUP discharge, or a
dependency on an earlier symmetry addition, and determines whether each empty
residual follows by unit propagation over those additions. G1 builds an offline
native-LRAT prototype and checks it with the pinned independent checker while
recording expansion, checker time and peak memory. G2 tests whether the same
automorphisms survive Tactix's default root inprocessing, determining the safe
schedule boundary. A failed gate changes the rule/proof design before solver
mutation begins.

### S1: Satsuma symmetry and substitution redundancy (57 UNSAT)

Start with the largest class. The [57-row transition screen](../../benchmarks/reports/2026-09-13-wv100-satsuma-transition.md)
showed that Satsuma returns normally on all rows and turns 52 into `p cnf 1 1`;
the other five retain nontrivial residual formulas. Its priority classification
is 51 structured, five Johnson and one general case. This is attribution
evidence, not a Tactix result.

Introduce a general root `SubstitutionRedundancy` calculus action whose explicit
instance is a candidate clause plus a finite literal substitution witness. The
rule validates the witness against a proof-visible view before mutation. That
view contains every original, learned, Factor and symmetry clause not yet
proof-deleted, plus LRAT-emitted root units and scoped proof-only translation
clauses. DRAT root consequences that were not emitted remain propagation facts,
not population entries. Duplicate identity follows the target format: literal
multiset multiplicity for DRAT and clause id for LRAT. Diagnostic builds extend
the existing proof shadow to assert that this view matches the checker's
population. Garbage collection is not a substitute for this invariant.

Validation records every direct image clause id and every RUP antecedent while
checking the witness; proof generation does not search for hints afterward.
Any fresh translation variables live only in the proof emitter, above the input
identity range, and never become kernel decision variables or model entries.
The native proof line schema and proof-visible population contract must be added
to the calculus and settle O16 after G1 establishes them. Proof and checker cost
must fit the declared run resource/scratch limits and is always reported; no
unsupported fixed expansion ratio is assumed.

Detection and proof construction poll the external stop and consume configured
ticks at bounded obligation boundaries. They stage their result without formula
mutation. The final checked clause insertion is a short atomic commit. Resume
uses the existing mutation token and deterministic producer cursor; stale work
is rejected rather than replayed on another formula state. Fuzz includes a
SIGTERM-latency lane.

Build formula-driven producers in observed class-size order: structured row and
row-column orbitopal symmetries, Johnson/orbitopal-fixing cases, then general
graph automorphisms with generators applied in stabilizer-chain order. An
external SR converter is an oracle during development, not a shipped proof
dependency. Tactix reimplements the mechanism from the published calculus and
official source; it has no runtime dependency on a reference solver or
converter. The existing
20-row pilot's 19 successful SR-to-DRAT-to-LRAT checks establish feasibility;
its one Johnson conversion failure blocks the Johnson substage until it has a
native checked route, but does not remove those rows from S1.

### S2: LymphoSAT specialist mechanisms (34 mixed-status rows)

Use the official 126-specialist source only to attribute each winning
transition. Do not import its formula-profile dispatcher or fallback sequence.
Cluster rows by the underlying general technique, such as finite-field/XOR
reasoning, cardinality structure, decomposition or model construction, and
attack the largest confirmed cluster first. The existing general GF(2) and
one-hot GF(3) Lucky actions are candidate building blocks only where transition
evidence supports them. SAT acceptance always checks the complete live CNF;
UNSAT acceptance requires native checked proof evidence.

### S3: AE-Kissat-MAB and kissat-sc2024 deltas (nine SAT)

For the two SC24 rows, isolate the causal preprocessing, search or scheduling
delta from the official source against the retained Tactix/Kissat lineage. For
the seven AE rows, distinguish MAB arm selection from the underlying VSIDS/CHB
mechanisms and all other source changes. Integrate any general causal mechanism
through the typed strategy interface. Decide whether MAB itself belongs in
Tactix only after an ablation shows that arm selection, rather than another
delta, owns useful coverage.

### S4: convergence and default promotion

Repeat attribution on remaining rows and compose independently supported
mechanisms through strategy configuration. Reordering or repeating rules is
allowed; launching solver personalities in sequence is not. DefaultPolicy
changes only when the combined general schedule preserves reference behavior
and the regression gates. Otherwise the mechanism remains an explicit online
strategy while still counting toward API-level reachability experiments.

## Acceptance matrix

A mechanism increment must pass focused unit/API tests, independent model or
native DRAT/LRAT checks, sanitizer coverage where memory/state changes, seeded
random replay, effective-transition and negative-control tests, interruption
and resume tests, and a Claude-owned fuzz checkpoint. B28+C5 and Rank 1--50 run
periodically after meaningful solver batches and before promotion. Their fixed
membership and retained references are reused; reference solvers are not rerun.

Causal probes use concurrent on/off arms from the same immutable pin, differing
only in typed settings, and counters must prove that the on arm reached and
applied the intended action. Registration/default-off tests require identical
proofs, non-timing statistics and outcomes. Before a mechanism enters the one
fixed WV100 configuration, its enabled detector runs on B28+C5, Rank 1--50 and
structurally similar non-WV100 Tier-D rows to measure inapplicable cost and
false-positive behavior. Detector limits are derived from representation or
explicit strategy budgets, not tuned from WV100 identities.

Every commit is an N1--N4-valid checkpoint. Partial implementation may remain
in the working tree during development, but it is not committed until its
declared API surface is N1-complete, its proof/model paths satisfy N2, and its
default behavior satisfies the current N3/N4 gates. Keeping a rule default-off
does not waive N1 or N2 for callers that explicitly apply it.

WV100 reporting leads with checked solved counts by SAT/UNSAT and winner
signature, followed by time, proof size, peak memory and regression effects.
Proof checking may finish in the background while independent work continues,
but unchecked outcomes cannot close a gate. The final gate is 100/100 checked
within the fixed 1,200-second/32-GiB contract with N1--N4 and the regression
controls intact.

## Claude Code review disposition

Claude Code reviewed this staged design before implementation and approved the
direction subject to closing the S1 entry gaps above. The review was advisory;
Codex evaluated it against the repository rather than copying it verbatim.

- Accepted: proof-visible population equality, structured explicit payload,
  stale mutation tokens, transactional interruptible validation, stabilizer
  order, single-configuration scoring, shared-state portfolio test, same-pin
  ablations, non-applicability cost and the G0--G2 falsification gates.
- Revised: N1 includes a genuinely unbounded exhaustive cursor as well as fast
  structured producers; Johnson is delayed within S1 rather than excluded; the
  existing mutation token is reused instead of inventing another epoch; proof
  auxiliaries remain proof-only rather than entering the solver kernel.
- Rejected: the review's unverified proof-expansion figure and an arbitrary
  per-row proof-size threshold. Authoritative proof bytes, checker time, memory
  and configured scratch limits govern the decision.
- Clarified: online strategy changes remain supported, while post-reasoning CNF
  addition stays under O7 and is not silently added to S1.
