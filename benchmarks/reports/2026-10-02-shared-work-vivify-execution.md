# Shared native work settings and Vivify execution

This increment follows `72151c3a` and advances the active shared-policy/feedback
goal. It does not establish full preparation acceptance. The
[operation map](2026-10-02-shared-policy-operation-map.md) owns the initial audit;
[validation.json](2026-10-02-shared-work-vivify/validation.json) binds this
increment's sources, binaries, preserved comparisons and terminal checks.

## Implementation and invariants

Native work and material-setting types now have one neutral definition, with
their namespaces, enum values and serialized/API behavior unchanged. The online
executor's private setting scopes are removed. Shared execution installs work
budget, offset, boundedness and midround settings; each lifetime acquires only
its requested work wires. Online applications restore their caller's profile,
including bounded zero versus unlimited work. Native DefaultPolicy doses clear
their owned budget wires and restore midround state at the original completion
boundary. Inherited work settings remain unowned. Material scopes retain the
original restoration of both discovery fields, including inherited settings.

DefaultPolicy's dosed candidate calls and budgeted batches now use those shared
lifetimes. Policy decisions still select residual allowances, minimum handoffs,
Substitute's mandatory first round and forward midround control. Shared tick and
journal-meter scopes retain native clear-on-exit behavior. No formula/trail
rollback, automatic policy fallback or global loop budget is added.

Vivify's band executor owns scope installation, cumulative budget and spent
offset, tick attribution, native journal metering and registered execution.
DefaultPolicy retains band order, cumulative share allocation, equal-tier
merging, focused tier selection, irredundant delay and productivity decisions.
Its dose scope clears the same native fields on return. A shared registered-rule
request also replaces ordinary DefaultPolicy and online dispatch, preserving
canonical and semantic-instance handling and the online propagation follow-up.
Family-specific parameter installation and candidate/publication/dense sessions
remain migration work.

## Validation

The combined proof-shadow Debug candidate passes 18 focused tests in 21.51
seconds. Coverage includes native scheduling, online execution and both C APIs,
random online/controller policies, Substitute publication, all affected Vivify
ordering/compaction/retry/cache tests and the two new fixtures. Three additional
controller retention/restart/transaction tests pass in 0.10 seconds. A separate
Debug build with rule journaling and event tracing enabled passes five relevant
runtime tests in 6.33 seconds, including scheduling and publication assertions.
Full fresh Debug acceptance remains pending; the current inventory has 199
tests.

The new work fixture exercises nested bounded/unlimited/inherited settings,
clear-versus-restore lifetimes, unowned fields, material restoration and real
propagation tick attribution. Twenty-four band pairs cover all six Vivify
scopes, zero/positive budgets and zero/nonzero offsets. The shared executor
matches direct native execution on status, formula, trail and work counters.
Existing scheduling fixtures prove cumulative unused-tier budget, equal-tier
merge and productivity-delay behavior; online fixtures prove real zero-work
ceilings and caller restoration.

Two unmodified `72151c3a` translation units replace the candidate objects in
comparison executables: DefaultPolicy and the online executor. No source
adaptation is needed. Six complete DefaultPolicy runs preserve work/state/phase/
score signatures and all fifteen input/proof/model files. Sixteen complete
random-controller runs use eight fixed seeds on SAT/UNSAT fixtures with a
six-family offered collection. Their deterministic transcripts, proofs, inputs
and models match, giving 56 additional byte-identical files. Only elapsed-nanos
records are excluded from transcript comparison. Native scheduling and online
execution tests also pass with identical printed output.

Across these complete runs, all eleven finished UNSAT certificates independently
pass drat-trim/CakeLPR and all eleven reconstructed SAT models satisfy their
original CNFs. This fixed-fixture reach does not replace the full action-space
and configuration coverage required of fresh fuzz acceptance.

An initial new signature-test build referenced a nonexistent Proof close method;
the fixture uses the existing flush operation before inspection. That failed
build is excluded from passing evidence. The wiring checker initially rejected
four fields after their setters moved. It now names the shared adapters and
requires both policy request and installation evidence; all fourteen missing-
request/installation negative controls are rejected. All thirteen documentation
checks pass, with six pre-existing size-reserve warnings. The initial four-test scope gate also
passed; the combined checks above follow the final candidate build.

## Limits and next action

Next share family-parameter installation and root preparation, then candidate
publication and dense-session lifetimes, preserving native order and ownership.
Remaining mapped operations, replaceable selection/feedback, offline records,
broad preserved-default action/state correspondence and instrumentation purity/
overhead are still required. Full new Debug, both fresh 100,000-seed
Claude-owned legs, targeted coverage, all four default regressions and final
Andrew acceptance handoff remain open under the plan.

Accepted Debug/Release pins and regression-baseline hashes are unchanged. No
SLURM campaign was submitted and no benchmark residue was removed. Queue
inspection found the same unrelated interactive shell allocation; it was left
untouched. No push is authorized. This checkpoint makes no performance claim.
