# Online strategy composition: review proposal

Proposed migration, not implemented behavior or a claim that N1 is complete.
DEVELOPMENT_PLAN.md owns the contract; PROGRESS.md owns the active next step.

## Goal and evidence

Refactor DefaultPolicy into cohesive, configurable sub-strategies before new
winner-benchmark solver work. All legal rule instances and branch variables /
phases must remain publicly callable (N1); preserve independent DRAT/LRAT and
original-input model checking (N2), competitiveness (N3) and scheduling quality
(N4). Codex implements/debugs. Claude Code reviews this plan and owns later fuzz.

The latest coherent experimental source is immutable7491, used by full50/7508
and cpu-q/7524. Its default_policy.cpp has4124 lines. DefaultStrategyConfig
already groups Branching, Analysis, Restart, Mode, Reduction, Rephase, Reorder
and Inprocess settings. Existing branching/reduction/BVE/Factor/candidate-order
sub-policy seams are useful, but implementation and state remain intertwined.
SlotEntry mixes rule identity, order, budgets, cursors, delays, watermarks,
follow-ups and telemetry. Preserve useful reference behavior, not this coupling.

RandomPolicy currently samples a generic rule's first16 instances or its
canonical application. More seeds cannot make that fixed prefix complete.
Replay currently records rule/payload, not future parameter/budget updates.
Open ledger findings include root restrictions, CCE binary victims, C1/C2
filters and rule caps. Independent7448/7453/7457 repairs are not in7491.

## Ownership and configurable application

Split search guidance, restart/mode control, database maintenance, phase
guidance and inprocessing into cohesive sub-strategies owning configuration
and runtime state. Keep their different decision signatures; do not force a
single universal virtual choose() interface. Preserve static hot micro dispatch.
DefaultPolicy becomes their reference composition and search arbitration.

Separate three concepts:

1. Rule semantics: pure guard, complete legal instances, state effect and proof /
   reconstruction obligations. No default scheduling or heuristic cutoffs.
2. Invocation request: selected rule/instances, typed parameters, work budget,
   requested granularity and stopping policy. Explicitly immutable for each
   atomic application; defaults are supplied by the caller's strategy.
3. Executor: validates requests, applies shared rule implementations, preserves
   representation/proof invariants and reports actual outcomes and work.

Illustrative interface, not existing code:

```cpp
Request request{
  .rule = rule_id,
  .parameters = typed_parameters,
  .selection = explicit_instance_or_selector,
  .granularity = SingleInstance{},
  .budget = WorkBudget{...}
};
StepResult result = executor.advance(request, continuation, solver, stop);
```

Parameters are schema-checked, validated, publicly settable/readable and fully
serialized. Distinguish semantic choices identifying a legal action from
heuristic choices controlling how to find/schedule it. No rule imports a
DefaultPolicy header or reads hidden global/env strategy settings. CLI/env
frontends adapt to the same typed configuration; they are not separate knobs.

Every heuristic number controlling depth, admission, candidate/occurrence
limits, clause sizes, search effort, retention, retries or proof-capture limits
belongs to a strategy setting with justified defaults. Audit named constants
and Kernel defaults as well as numeric literals. Algebraic arities, encodings,
sentinels, soundness facts and representation widths remain documented constants,
not adjustable correctness conditions. Storage/resource limits must not silently
erase legal instances; grow, stream, or explicitly report exhaustion.

## Granularity, repetition and order

Support one explicit legal semantic instance, a caller-selected subset/batch,
one complete pass over a declared scope, and repeated rule/sequence applications.
Compound instances need an additive public constructor/handle if frozen64-bit
payloads cannot express them. Keep v1 ABI and stable registry IDs compatible.
Complete lazy enumeration needs cursor/paging or equivalent direct selection;
sampling must reach beyond a fixed prefix. Mutation invalidates stale handles.

Arbitrary granularity means caller-selected semantic work units, not exposing
half-written proof steps or inconsistent intermediate watch/trail state. Existing
atomicity (notably learn/backjump) remains explicit pending a separately proved
finer contract. An atomic step may overshoot a budget; document/report the bound,
poll at safe points, and never claim a hard cap that cannot be enforced.

Use a typed program builder, not an initial schedule-string language:

```cpp
sequence({
  run(congruence, parameters_a, one_instance, budget_a),
  run(sweep, parameters_b, selected_batch, budget_b),
  repeat(until_fixpoint(scope) || exhausted(shared_budget),
         sequence({run(strengthen, ...), run(bve, ...)})),
  run(vivify, ...)
});
```

Rules may be reordered, omitted, repeated or interleaved with search through
public actions. StepId identifies each occurrence separately from RuleId;
repeated occurrences cannot accidentally alias cursors/delays. Shared phase
budgets and per-invocation/per-occurrence budgets have explicit lifetimes and
units. Duplicating a step must not replenish its enclosing allowance.

Fixpoint must name its scope and parameters. NoOp, an exhausted enumeration,
one unsuccessful candidate and a budget cutoff are not equivalent to fixpoint.
A repeated sequence needs an effect-free complete pass of its chosen actions
under a valid scope, not just the last rule reporting NoOp. Another rule's
mutation invalidates affected quiescence claims and continuations. Novel
candidate generation and deletion must be included in closure semantics.
Unbounded user programs may fail to terminate; provide explicit stop/cancel,
observable no-progress status and configurable fairness, not hidden truncation.

Result separates inapplicable, completed, no semantic change, scoped fixpoint,
budget exhausted, interrupted, conflict, SAT/UNSAT and invalid request. It
reports counters, semantic effects, continuation validity and configuration/state
versions. Diagnostic counters alone are not semantic progress. Stop, resource
failure and budget exhaustion never imply UNSAT or completeness.

## Fully online configuration

Expose observe/configure/advance controls. A caller can change order, parameters,
selection, granularity and budgets between safe applications without restarting
the solver or discarding formula/learned clauses. Configuration updates are
validated transactionally and take effect at the next declared safe boundary.
Capture their revision in results and replay. Define which sub-strategy state
is retained, transformed, reset or rejected for each setting change; do not
silently reset clocks/credits or carry invalid cursors across incompatible plans.
Budget accounting survives pause/resume according to its declared scope.

Initial concurrency contract is single-threaded: external changes occur only
between advance calls; asynchronous cancellation is separately supported. Do
not introduce data races to promise arbitrary mid-instruction updates. For long
applications expose safe yield points and continuations, not private mutation.

Correctness preparation must not become hidden scheduling. Representation-only
adapters preserve the specified state. Root backtracking is a visible policy
action, not a silent substitute for the requested non-root rule application.
Audit D-ROOT against the calculus: implement legal non-root behavior where
required or leave N1 pending, never weaken the abstract rule to match code.

## Broad random strategy and validation

Default, scripted, random and future learned strategies use the same interfaces.
Seeded random behavior spans applicable rules, phases/variables, late and compound
instances, candidate order, repeated/reordered sequences, parameters, granularities,
budget regimes, online changes and stop/resume. Preserve support even when
weighted progress advice makes tests more productive. Do not impose the default
pipeline or rely on a tiny set of hand-selected schedules.

Constructive fixtures prove difficult choices reachable; stochastic campaigns
explore combinations. Measure effective changes as well as selections, NoOps,
distinct orders, repeats, late instances, coverage of dense/sparse/root/non-root
states and stop reasons. Negative controls must detect canonical-only selection,
first16-only sampling, fixed order, hidden disabled rules, mostly NoOps and
missing configuration/budget information in replay. Finite tests cannot prove
all possible schedules; audit and explicit completeness arguments remain needed.

## Staged migration and acceptance

1. Freeze the baseline and audit every registered/catalog action: abstract guard,
   actual guard, enumeration, explicit apply, compound payloads, constants,
   default option path and restriction witnesses. Resolve which independent
   pending fixes compose safely; do not edit active pins or fuzz-owned sources.
2. Obtain Claude Code's design feedback and record dispositions. Select the
   smallest complete interface slice; Codex retains implementation/debugging.
3. Introduce request/result/configuration/continuation seams and a default
   adapter without changing action order, work, proof bytes or clocks. Prove
   exact default replay; isolate necessary N1 behavior repairs from neutral moves.
4. Extract cohesive sub-strategies and move heuristic ownership incrementally.
   Resolve N1 restrictions with direct-API witnesses and general fixes. Add
   reordering/repetition and safe online updates through the shared executor.
5. Extend broad RandomPolicy and full replay, with deterministic reach fixtures,
   coverage-negative controls and interruption/reconfiguration stress cases.
6. For every change decide whether tests must be added. Run relevant guard,
   instance/parameter, budget-boundary, stale-handle, state/reconstruction,
   native DRAT/LRAT and sanitizer tests. Both focused testing and Claude-owned
   fuzz are required, not substitutes. Claude prepares, executes, monitors,
   interprets, shrinks and reports fuzz; Codex repairs bounded failure handoffs.
7. Preserve original failures, add regression cases, repair and repeat affected
   gates until complete. Require checked B28+C5 and retained full50/memory/runtime
   controls and meaningful-change fuzz checkpoints; never rerun passing unchanged
   fuzz merely for reassurance. Pending proofs allow independent development,
   not promotion. Default abstraction overhead retains the <=5% obligation.
8. Update bounded docs and Andrew reports; land supported coherent work. N1 must
   have no unresolved restriction with all required reach/correctness evidence
   before winner-driven solver development. Do not claim universal soundness
   or N1 completeness solely because finite tests passed.

Preserve per-stage immutable rollback/reproduction points. Do not revert a
proven N1/soundness repair just to recover an old timing number. Keep existing
cpu-q/7524 and proof-check jobs unchanged; Rank50<=1.01x is deferred, not achieved.
Later winner targets remain10SAT+10UNSAT at1200s/32GiB, selected from latest Tactix
and existing baselines, then matched on hwu with each fastest qualifying baseline.

## Requested Claude critique

Review this proposal only; do not implement, run tools/solvers, inspect files or
fuzz campaigns. Identify blocking N1 loopholes, misleading fixpoint/granularity
claims, budget and mutable-state ownership errors, performance risks, weak
random support and migration traps. Recommend concrete interface corrections
and the smallest credible first slice. Distinguish blockers from refinements.
