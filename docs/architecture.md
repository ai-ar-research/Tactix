# Architecture

Tactix is a CDCL SAT solver in which every solver step is a registered rule
that a strategy selects through a C interface. This document describes how
the solver is organized, how one rule application travels from a strategy to
the kernel and the proof, and how the built-in default strategy schedules
rules. The catalogue of rules is [rules.md](rules.md); their formal semantics
are in [calculus.md](calculus.md).

## Overview

```
 +----------------------------+      +-----------------------------+
 |  Default strategy          |      |  User-defined strategies    |
 |  (DefaultPolicy, Kissat-   |      |  (C programs, controllers,  |
 |   and CaDiCaL-based)       |      |   random/fuzz policies)     |
 +-------------+--------------+      +--------------+--------------+
               | calls                 results ^    |
 +-------------v-------------------------------+----v--------------+
 |  Front end: include/tactix.h                                     |
 |   strategy interface: rules_applicable, rule_instances,          |
 |     apply / apply_instance, decide, online_configure / advance,  |
 |     support transactions, install_parameters, stat, solve        |
 |   input: read_dimacs, add            proof: tactix_proof         |
 +-------------------------------+----------------------------------+
                                 | request          ^ outcome
 +-------------------------------v------------------+---------------+
 |  Back end                                                        |
 |   rule registry --guard/apply--> kernel state <--> heuristics H  |
 |   (proof rules,                  (clauses, watches,  (scores,    |
 |    policy operations)             trail, reasons)     phases)    |
 |                                    |          |                  |
 |                          proof backend   reconstruction stack    |
 +------------------------------------+----------+------------------+
                                      v          v
                              UNSAT certificate  SAT model
                              (DSR / DRAT /      (original
                               LRAT / PBP)        variables)
```

A strategy calls the front end; the back end checks the request against the
rule's guard, applies the rule to the shared state, writes the proof steps
and reconstruction data, and reports the outcome. The command-line solver
runs the default strategy. The default strategy is written against the same
operations: in the compiled solver it binds them directly, and the
`tactix-public` example issues every one of them through `tactix.h` and makes
the same decisions and writes the same proofs.

Any interleaving the interface allows has the shape

```
(simplification | structural reasoning | local search)* ;
(search ; restart-to-root ; (simplification | structural reasoning | local search)*)*
```

Root-level operations run at decision level zero, reached by `Restart` with
keep level 0; search operations run in between. Learned clauses, formula
transformations, root assignments and proof state carry over across every
boundary.

## Layers

### Kernel

The kernel (`src/kernel/`) owns the solver state and knows nothing about
policies. In the notation of [calculus.md](calculus.md) the state is
`σ = ⟨F, R, τ, ρ, π⟩`:

| Component | Representation |
|---|---|
| `F`, `R`: irredundant and redundant (learned) clauses | Large clauses (three or more literals) live contiguously in one 32-bit word arena with a header carrying size, flags, glue and proof id. Binary clauses exist only in the watch lists; their proof ids are kept in a separate map. |
| watches | Per-literal lists in one shared arena (Kissat's vector scheme); binary watches are one word, large watches a blocking literal plus a clause reference. |
| `τ`: trail | Assigned literals with reasons and *true* decision levels, so the trail need not be level-sorted (chronological backtracking). |
| `ρ`: reconstruction stack | Witness-labelled clauses pushed by equisatisfiable deletions (BVE, Substitute, BCE, CCE, Condition), replayed in reverse to complete a model. |
| `π`: proof cursor | A pointer to the proof writer; null when no proof is written. |
| occurrence indexes | Built on demand for inprocessing (dense mode during elimination). |
| identities | DIMACS, proof, model and API literals use permanent external identities; the kernel uses dense internal indices behind bidirectional maps, and may compact them. |
| statistics | Counters exposed read-only through `tactix_stat`, including per-rule work meters. |

Invariants the kernel maintains (detailed in
[implementation-notes.md](implementation-notes.md) §3):

- `F₀` is equisatisfiable with `F ∪ ρ`, `F` implies `R`, and every transition
  is justified by proof steps a checker accepts.
- The proof never sees a root unit the kernel lacks, and the kernel publishes
  a root consequence to the proof before deleting the clause that implied it.
- Writing a proof never changes a decision: runs with and without a proof are
  identical.
- No rule, kernel primitive or policy reads elapsed time, and every ordering
  that affects the trajectory is a strict total order, so runs are
  deterministic.
- Clause references are valid only within one arena generation; any retained
  reference is revalidated after a possible collection.

### Rules and the registry

Each proof rule is one module under `src/rules/<family>/` that registers a
guard, a canonical application and optional instance callbacks with one
`TACTIX_REGISTER_RULE` line. Guidance-only *policy operations* live under
`src/policies/ops/<name>/` and register with `TACTIX_REGISTER_POLICY_OP`. The
registry (`src/rules/registry.cpp`) orders all of them by a frozen id list and
is the single place through which the C interface, the default inprocessing
slot and the random/fuzz policies discover operations. There are no
switch statements over rules.

The guard is the soundness precondition; it never encodes a scheduling
heuristic. Each entry also has a dispatch tier:

| Tier | Members | Dispatch |
|---|---|---|
| Micro | Propagate, Decide, ConflictAnalysis | Statically compiled into the default strategy's search loop; ordinary registry calls everywhere else. |
| Macro | Restart, Reduce, Lucky and the search-guidance policy operations | Individually scheduled. |
| Inprocess | the simplification and structural rules | Members of the root inprocessing slot; the default scheduler builds its program from this tag. |

Rules own their semantics, guards, instances, statistics and proof steps.
Strategies own heuristic parameters and budgets, passed to a rule as typed
per-call parameters or as a permanently installed profile. Large rules are
split into recognition, planning (a count-only preflight of the complete proof
transaction) and atomic publication; on a decline, interruption or exhausted
budget, they change nothing. Shared engines that are not rules sit beside
them: an embedded CDCL sub-solver (`src/engine/subsolver.*`, used by Sweep
and by BVE's definition extraction), GF(2) helpers (`src/engine/affine/`),
proof-certificate planners (`src/proof/*_certificate.hpp`) and the DejaVu
graph-automorphism backend for symmetry detection (`src/symmetry_backend/`).

### Heuristic state H

Search guidance (`src/policies/heuristics.*`) is outside the kernel's logical
state: the VMTF queue, the EVSIDS score heap, saved, target and best phases,
and the stable/focused mode flag. The kernel holds only an optional, opaque
pointer to it, and every rule is sound with no heuristic state attached.
Policy operations (Rephase, SwitchMode, Reorder, Walk, LocalSearchGuidance,
SemanticVariableAdvice) and the decision heuristic change only H.

Rules report events that concern guidance (new root assignments, retired
variables, fresh Factor variables, substituted variables). The guidance
publication mode (`tactix_set_guidance_publication`) decides whether those
events update H after each public call (`TACTIX_GUIDANCE_NATIVE`, the default
strategy's behaviour) or are discarded (`TACTIX_GUIDANCE_DISCARD`, the
interface default).

### Policies

A policy implements `Policy::solve` (`src/policies/policy.hpp`) and decides
which rule to apply, when, with which instance and settings. Policies never
touch rule internals; rules never push scheduling state.

- **DefaultPolicy** (`default_policy.cpp`, with `default_restart.hpp`,
  `default_periodic.hpp`, `default_inprocess.hpp`) is the shipped strategy. It
  is composed of a branching sub-policy (VMTF/EVSIDS with random decision
  sequences), restart, mode, database-reduction and phase controls, and an
  inprocessing scheduler. All its settings are collected in
  `DefaultStrategyConfig` (`default_strategy_config.hpp`), including the
  inprocessing program and the structured startup program
  (`structured_root_strategy.cpp`).
- **Public random qualification client** (`tools/fuzz/public_random_main.cpp`)
  is external to the production frontend and includes only `tactix.h`. It
  generates varied one-action-at-a-time root/search/restart trajectories and
  reports source coverage to the fuzz harness.

The typed execution layer (`src/strategy/`) sits between policies and rules.
It defines request types for every rule and support transition, per-rule
parameter structs, work budgets and tick attribution, the online program
executor behind `tactix_online_*`, and the action port through which
DefaultPolicy issues every action. Because the default strategy reaches rules
only through this port, the same strategy can be bound either to the
registered functions directly (production) or to the public C interface (the
`tactix-public` twin).

### C API

`include/tactix.h`, implemented in `src/api/`, is the only public surface.
See [c-api.md](c-api.md) for usage. Its groups:

- **Input and proof**: `tactix_add`, `tactix_read_dimacs`, `tactix_proof`.
  Input closes at the first reasoning call; a proof writer must be attached
  before it.
- **Stepping**: `tactix_rule_count`, `tactix_rule_name`,
  `tactix_action_kind`, `tactix_rules_applicable`, `tactix_rule_instances`,
  `tactix_apply`, `tactix_apply_instance`, the semantic-instance pair,
  `tactix_apply_condition`, `tactix_decide_candidates`, `tactix_decide`.
- **Online composition**: `tactix_online_configure_v*` validates a program of
  typed steps (rule, parameters, granularity, repetition, work budget) as one
  transaction; each `tactix_online_advance` performs at most one atomic rule
  application and returns a result record (status, work, progress, stop
  reason). Programs can be replaced between advances without losing solver
  state.
- **Configuration**: setters such as `tactix_set_bve_bound`,
  `tactix_install_parameters` (a permanent profile read by later
  applications and by `tactix_solve`), `tactix_install_work_settings`, with
  read-back through `tactix_config`.
- **Support transactions** (`tactix_support`): lifecycle and representation
  transitions that are not proof rules, such as the preparation restart, root
  preparation, garbage collection, variable compaction, entering and leaving
  the inprocessing phase bracket, and warm-up.
- **Observation**: `tactix_status`, `tactix_value`, `tactix_stat`.
- **Default strategy**: `tactix_solve` runs DefaultPolicy from the current
  state, so a client can apply its own sequence first and then hand over.

### Proof backend

The proof backend (`src/proof/proof.*`) receives typed events from rules and
streams them to disk; it never holds the proof in memory. The events are RUP
addition, RAT addition, SR addition with a substitution witness, deletion,
derived root unit, empty clause, and a terminal pseudo-Boolean refutation.
Fresh proof-only variables (helper definitions used only inside a
certificate) get permanent identities from a separate namespace and are never
reused.

| Format | `tactix_proof` constant / CLI flag | Content |
|---|---|---|
| PBP (CLI default) | `TACTIX_PROOF_PBP` / `--pbp` | whole-run PBP; pinned VeriPB elaboration followed by verified CakePB checking |
| automatic | `TACTIX_PROOF_AUTOMATIC` / `--auto-proof` | retained compatibility route: binary DSR, or PBP for a complete refutation found at the original-input boundary |
| DSR | `TACTIX_PROOF_DSR_BINARY`, `_ASCII` / `--dsr` | DRAT plus SR additions; the native format |
| DRAT | `TACTIX_PROOF_DRAT_BINARY`, `_ASCII` / `--drat` | compatibility; SR steps are unavailable |
| LRAT | `TACTIX_PROOF_LRAT` / `--lrat` | compatibility, with antecedent hints |

In automatic mode the backend stages the input-normalization steps privately.
If a startup Lucky instance (XOR summation or count conservation) refutes the
original formula, the run emits a pseudo-Boolean proof; otherwise the first
ordinary action commits the staged DSR prefix and the format is fixed. A rule
whose proof step a format cannot express declines before mutating anything.

After an UNSAT answer the solver prints the checker route:

| Route | When | Checking pipeline |
|---|---|---|
| `dsr-cake_lpr` | no witnessed (SR) line was written, so the DSR stream is DRAT | drat-trim, then the verified checker CakeLPR |
| `dsr-trestle` | SR lines were written | `dsr-trim` to LSR, then the independent Lean checker Trestle |
| `pbp` | pseudo-Boolean startup or explicit whole-run refutation | pinned VeriPB elaboration, then verified CakePB against the original CNF |

`tools/bench/proof_oracle.py` runs these pipelines against the original CNF;
see [getting-started.md](getting-started.md#check-a-proof). The proof
discipline that keeps kernel and checker views consistent (root units
published before reason deletion, temporary lemmas deleted before later SR
steps, no witnessed line closing the refutation) is part of the calculus and
of [implementation-notes.md](implementation-notes.md) §1.

### Front end and parser

`src/main.cpp` is the command-line solver (options in [cli.md](cli.md)). It
reads DIMACS CNF through the streaming parser in `src/frontend/parse.cpp`,
which accepts comments anywhere and arbitrary whitespace, treats the `p cnf`
header as advisory, and reads `.gz` (zlib) and `.xz` (external `xz`) input.
Clause finalization (duplicate literals, tautologies, units, the empty
clause) is shared with `tactix_add`. On SAT the solver reconstructs a model of
the original variables from the reconstruction stack, dropping extension
variables, and prints `v` lines; on UNSAT it flushes the proof before
printing the verdict. SIGINT, SIGTERM and SIGXCPU stop the run with
`s UNKNOWN`. The production frontend runs DefaultPolicy only; other options
change its switches or recording/proof behaviour.

## Life of a rule application

A call `tactix_apply_instance(solver, rule_id, instance)` proceeds as follows.

1. **Input boundary.** The first reasoning call closes input and fixes the
   proof's clause-id boundary.
2. **Lookup and guard.** The id must name a registered, non-retired rule and
   the rule's guard must hold; otherwise the call returns `-1`. A
   non-canonical payload for a rule without an instance surface is rejected.
3. **Startup boundary.** Unless the instance belongs to the original-input
   boundary (Lucky 19, 20, 22), dispatch closes that boundary; in automatic
   proof mode this commits the DSR format.
4. **Dispatch.** The registry wrapper charges the call to the rule's call and
   time counters, advances the strategy epoch that controllers observe, and
   stages any theory-variable candidate the rule publishes for
   SemanticVariableAdvice.
5. **Application.** The rule reads and mutates the kernel through kernel
   primitives. Each primitive writes its proof event as it changes the
   formula, always adding a clause before deleting the clauses it replaces,
   and pushes reconstruction witnesses for equisatisfiable deletions. Derived
   root units are asserted; propagating their consequences is usually left to
   a following `Propagate`.
6. **Guidance publication.** Events concerning H (root units, retired and
   fresh variables) are applied to H or discarded according to the
   publication mode.
7. **Result.** The rule's result is mapped to the return code (`0` progress,
   `1` no-op, `2` conflict, `10` SAT, `20` UNSAT). Debug builds check the
   kernel invariants after every call.

The strategy then observes the outcome through the return code,
`tactix_status`, statistics deltas (`tactix_stat` before and after) and, for
online steps, the result record. The default strategy brackets each
inprocessing round with the inprocessing phase support transactions
(occurrence index, watch disconnection, sparse resumption); a custom strategy
can issue the same bracket through `tactix_support`, and a rule applied
outside a bracket sets up what it needs for that call.

## How the default strategy schedules rules

DefaultPolicy reimplements the scheduling of Kissat, with CaDiCaL and
Satsuma mechanisms where those are the reference for a technique, and adds
structure-specific startup actions. Where a mechanism has a reference
counterpart, its schedule follows that reference; the per-mechanism record
of how closely, and of accepted differences, is the
[scheduling-parity ledger](scheduling-parity-ledger.md).

### Startup

1. Restart to the root (retiring any conflict recorded by earlier public
   calls).
2. Original-input certificates: the Ramsey refutation (Lucky 22), then the
   XOR-summation and count-conservation refutations (Lucky 19, 20), which can
   end the run with a pseudo-Boolean proof.
3. Graph-labeling authentication (Lucky 26) and a bounded GF(2) closure of
   the root when the whole formula is a recognized XOR system.
4. Structure-specific Lucky constructors and refutations (instances 6,
   10–18, 23 and 24), each bounded by its typed profile.
5. The structured startup program: the affine constructor (Lucky 8) and
   symmetry breaking through SubstitutionRedundancy producers (orbitopal,
   clausal and negation fixing, at-most-one completion), with pure-literal
   elimination and a BCE cleanup between stages, under one shared work
   budget, following Satsuma's schedule including its changes for large
   domains.
6. The direct-CSP constructor (Lucky 9), Kissat's early Lucky cascade
   (instances 0–5) and the one-hot constructor (Lucky 7).
7. Attach the heuristic state, collect root-satisfied clauses, and run the
   preprocessing round of the probing group (below), then Kissat's initial
   fast elimination.
8. Kissat's late Lucky cascade, graph-labeling continuation (Lucky 27),
   formula classification, and initialization of all search limits.

### Search loop

Each iteration propagates. On a conflict it runs ConflictAnalysis with the
chronological-target instance (backjump to the asserting level unless that
would undo more than a fixed number of levels, in which case it backtracks
one level). Otherwise it checks, in order:

| Check | Mechanism |
|---|---|
| Reduce | Interval grows with the square root of the number of reductions. Learned clauses are kept by glue tiers, whose boundaries are recomputed from the used-glue histogram; a fraction of the remaining candidates is deleted; variable and arena compaction are coupled to reduction. |
| SwitchMode | The first focused phase ends after a conflict count that grows as `n·log⁴ n`; each stable phase lasts as many propagation ticks as the preceding focused phase. |
| Restart | Focused mode: fast and slow exponential moving averages of glue with a margin (Glucose-style). Stable mode: reluctant doubling (Luby sequence). Restarts reuse the part of the trail that the next decision would rebuild. At a full restart, retained root services (graph-labeling continuation and, when enabled, the affine matrix) run in the root window. |
| Reorder | Periodic structural re-ranking of the active heuristic at the root. |
| Rephase | Stable mode only; interval grows as `n·log³ n`. Rotation: best, Walk, inverted, best, Walk, original. The Walk slot runs local search with an effort proportional to recent search work and falls back to best phases if Walk is inapplicable. |
| Probing group | Inprocessing round, see below. |
| Elimination group | Inprocessing round, see below. |

If none fires, Decide picks the next literal from the active heuristic.

### Inprocessing slot

Inprocessing runs at the root in two groups, as in Kissat's `probe` and
`eliminate`. Each group has a conflict deadline that grows as `n·log n`
(probing) or `n·log² n` (elimination) in the number of rounds, scaled by
formula size; a group is skipped when nothing relevant changed since its last
round (no reduction since the last probing round; no new elimination
candidates, root units or subsumption material since the last elimination
round). Each pass in a round receives an effort budget in its own work
currency (propagation ticks, resolution attempts, etc.), computed as a share
of the search ticks spent since the previous round, with a minimum. The
inprocessing phase bracket (occurrence index, watch disconnection, sparse
resumption) wraps each round.

The shipped program, in order:

| Group | Rules |
|---|---|
| Probing | Congruence, Substitute, Backbone, Vivify, Sweep, Substitute, TransitiveReduce, Backbone, Factor |
| Elimination | Strengthen (fused forward subsumption), BVE |

Substitute runs after Congruence and after Sweep to apply the equivalences
they found. Elimination follows Kissat: a bounded number of rounds, each a
forward-subsumption pass followed by BVE over score-ordered candidates with
gate detection, with the clause-growth bound raised in steps as rounds
complete.

Registered inprocessing rules that are not in this program (Probe,
HyperBinaryRes, HyperTernaryRes, standalone SubsumeForward, Instantiate, BCE,
CCE, Condition, SubstitutionRedundancy and the structural rules) remain
available to custom strategies; a custom `DefaultStrategyConfig` program can
reorder, omit, repeat or add steps and set per-step effort shares and
parameters.

## Source tree

| Path | Responsibility |
|---|---|
| `include/tactix.h` | The public C interface. |
| `src/main.cpp` | Command-line solver: options, signal handling, output, DefaultPolicy dispatch. |
| `src/frontend/` | Streaming DIMACS parser and shared clause finalization. |
| `src/api/` | Implementation of `tactix.h`. |
| `src/kernel/` | Solver state: clause arena, watch and occurrence arenas, trail, propagation, reconstruction stack, identity maps, root-unit publication, invariant checks. |
| `src/rules/registry.*` | Rule registration, frozen id order, timed dispatch. |
| `src/rules/<family>/` | One directory per proof rule (for example `bve/`, `vivify/`, `sweep/`, `substitution_redundancy/`). |
| `src/rules/backjump/` | The backjump executor fused into ConflictAnalysis (not a registered rule). |
| `src/rules/support/` | Shared failed-literal and occurrence-index helpers. |
| `src/rules/lucky/` | Lucky probes, model constructors, startup certificates, and TseitinCycleBreaking. |
| `src/rules/semantic_extension/` | Recognition, certificate planning and publication for the GF(2), GF(3), CSP, exact-cover and independent-set rules. |
| `src/rules/direct_php/` | Direct pigeonhole refutation action. |
| `src/policies/` | DefaultPolicy, its configuration and heuristic state H; retained internal fixtures support structured-action unit tests. |
| `src/policies/ops/<name>/` | Policy operations: Rephase, SwitchMode, Reorder, Walk, LocalSearchGuidance, SemanticVariableAdvice. |
| `src/strategy/` | Typed execution layer: requests, per-rule parameters, budgets, online program executor, DefaultPolicy's action port, feedback records. |
| `src/proof/` | Proof writer and serializers; certificate planners (pigeonhole, cardinality, Ramsey). |
| `src/engine/` | Embedded CDCL sub-solver; GF(2) projection and search helpers. |
| `src/symmetry_backend/` | Boundary to the DejaVu automorphism library (built with exceptions, isolated from the core). |
| `src/population/` | Read-only observation of raw, checker and solver clause populations, used by tests. |
| `src/diagnostics/` | Opt-in event trace and rule journal (compiled only in diagnostic builds). |
| `third_party/` | DejaVu. |
| `examples/` | Strategies written against `tactix.h`. |
| `tests/` | Unit, API and proof tests (`ctest`). |
| `tools/checkers/`, `tools/bench/`, `tools/fuzz/` | Checker setup, proof-checking script, fuzzing harness with proof and model oracles. |

## Further reading

| Document | Contents |
|---|---|
| [rules.md](rules.md) | Catalogue of every rule and policy operation. |
| [calculus.md](calculus.md) | Authoritative semantics: preconditions, effects, proof steps, soundness. |
| [proof-rules.html](proof-rules.html) | Interactive guide with worked examples per rule. |
| [implementation-notes.md](implementation-notes.md) | Implemented guards, engineering deviations, representation invariants, proof-backend contract. |
| [controller-interface.md](controller-interface.md) | Public-interface random qualification grammar and coverage contract. |
| [scheduling-parity-ledger.md](scheduling-parity-ledger.md) | Per-mechanism comparison of the default schedule with Kissat, CaDiCaL and Satsuma. |
| [c-api.md](c-api.md), [cli.md](cli.md) | Library and command-line usage. |
| [getting-started.md](getting-started.md) | Build, solve, check a proof. |
