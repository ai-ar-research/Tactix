# Shared startup, representation and Substitute execution

This increment follows `d6531a65` and advances the active shared-policy/feedback
preparation goal. It does not close that goal. The
[operation map](2026-10-02-shared-policy-operation-map.md) identifies the audited
scope; [validation.json](2026-10-02-shared-startup-substitute/validation.json)
binds this increment's sources, binaries, focused checks and comparisons.

## Implementation

Typed native Lucky requests now describe each structural startup opportunity,
the canonical cascade, ordinary probe and exact-one refutation substep.
DefaultPolicy and Lucky's registry/cascade paths call the same execution
surface. Policies still select opportunities, parameters and order. Public IDs
0--20, startup exceptions, guards, root closure and certificate boundaries are
unchanged; the cascade retains its original nested root closure.

Shared execution contexts own the existing work-attribution and phase-saving
wires. Startup suppresses search-tick charging, inprocessing also suppresses
phase saving, and speculation suppresses only phase saving. Each restores only
its owned fields, including nested use. Typed materialization, sparse
collection, garbage collection and variable-compaction requests share native
representation support between DefaultPolicy and Reduce. Their watermarks and
root/publication order remain caller choices.

Substitute's public complete helper and DefaultPolicy's complete/budgeted
drivers now share the actual round loop and final watch reconnection. Internal
callbacks provide snapshots, per-round publication and continuation advice.
DefaultPolicy consumes roots then retirements before a later round or final
reconnection. Complete calls retain the historical cumulative-retirement
continuation profile; budgeted calls retain live-active-population tests and
the per-round optional-work cutoff. Zero effort still permits the mandatory
first budgeted round. The executor borrows an already disconnected session,
reconnects an owned session once, and returns a terminal round without
reconnecting. Internal callbacks are not learner selection/feedback boundaries.

## Validation

The combined candidate passes 30 focused Debug tests in 1.45 seconds with proof
shadow enabled. These include startup models/refutations, representation
support, native scheduling, shared transactions, controller execution, and all
Substitute regression tests. Startup parity/counting independent-check scripts
pass VeriPB/CakePB, including normalization and the rejected missing-derivation
negative control.

Before the Substitute cutover, the startup/support snapshot passed all 26 of its
focused tests, including the long Lucky test, in 804.66 seconds. A preserved
Lucky executable also passed its full native fixture. This is correctness
evidence for that earlier snapshot, not a current full-suite or timing claim.
The final 30-test run intentionally excludes that long test; full fresh Debug
acceptance remains outstanding.

The new support fixture checks real propagation work/phase attribution, nested
scope restoration, unowned wires, all sparse-collection outcomes, idempotent
cold materialization and collection identity. The new Substitute fixture checks
owned/borrowed sessions, zero/one/two-round ceilings, internal publication before
next-round observation, stopping after a productive round, and terminal results
overriding permissive continuation advice. Existing publication tests exercise
all eight budget/preprocessing/round combinations against an independent manual
round sequence and compare complete heuristic heaps and variable activity.

Five unmodified `d6531a65` translation units replace the candidate objects in
comparison executables: DefaultPolicy, Lucky, Onehot, Reduce and Substitute.
No source adaptation is required. Six complete fixed DefaultPolicy runs retain
identical work/state/phase/score signatures. All fifteen CNF/proof/model files
match byte for byte. All three completed proofs independently pass
drat-trim/CakeLPR; all three reconstructed models satisfy their original CNFs.
Preserved/current Substitute, publication and scheduling executables also pass
with identical printed output. These comparisons supplement the nonvacuous
publication fixtures; they are not a complete runtime action-map audit.

All five changed translation units compile with optional rule-journal and
event-trace branches enabled in syntax checks. This does not establish runtime
diagnostic equivalence or feedback-instrumentation overhead. A separate Debug
build with both diagnostics enabled passes scheduling, Substitute execution and
publication tests (3/3, 0.10 seconds), including all eight cases' journal
transaction/proof-event and effort assertions. All thirteen documentation
checks pass; the six pre-existing document-size reserve warnings remain.

Development checks exposed an incorrect new-test assumption that terminal
refutation necessarily follows watch disconnection. The corrected fixture
checks the native terminal watch state for both entry profiles. An initial
oracle command used a nonexistent checker directory; the corrected command
checks the already retained identical outputs successfully. An earlier build
list named an absent test target; the final candidate targets build successfully.
None of these initial attempts is counted as passing evidence.

## Limits and next action

Next migrate inprocessing work/parameter scopes and Vivify band execution,
keeping adaptive share allocation and productivity decisions in DefaultPolicy.
Remaining audited operations, replaceable selection/feedback interfaces,
offline records, broad action/state correspondence, instrumentation purity and
overhead, fresh full Debug, both 100,000-seed Claude-owned legs and all four
default regressions remain required by the plan. The final Andrew TeX/PDF must
describe that complete acceptance, separately from this incremental evidence.

Accepted Debug/Release pins and the regression baseline retain their recorded
hashes. No SLURM campaign was submitted and no benchmark residue was removed.
Queue inspection found only an unrelated pre-existing interactive shell
allocation; it was left untouched. No push is authorized.
