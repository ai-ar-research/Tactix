# Rank43: incoming conflict differs before shrinking

## Decision-changing result

The current7373 Trace candidate and pinned native5414 observer enter conflict82
with different binary conflict clauses. Both enter analysis at decision4048,
level115. The fully captured stages show:

| State | Current Tactix | Native Kissat |
|---|---|---|
| Incoming conflict | `(1038,-49957)` | `(1038,49959)` |
| Raw learned clause | `(551069,-50235)` | `(551069,50237)` |
| Sorted, shrunk and final learned clause | unchanged | unchanged |
| Nonasserting literal level / backjump |102|105|

[7407](2026-09-11-rank43-conflict-semantics-7407.json) binds and checks the
completed-core7406 evidence. Across the81 incoming conflicts (one OTFS
transaction advances29 to30), the first clause-order difference is48, with
equal literal sets and per-literal level maps. The first conflict literal-set
and level-map difference is82. This is not a claim that every earlier formula,
assignment, antecedent, or learned clause is identical. The earlier physical
order difference remains an upstream question, not an accepted deviation.

The actual learned binary explains the differing jump target. Candidate
`src/policies/default_policy.cpp` derives its target from the pending jump,
with the same ordinary chronological-distance branch as native `src/learn.c`.
At82 no shrink/sort stage changes either clause. Do not change chronological
thresholds, phase selection, shrinking or late BVE/Factor budgets to hide the
downstream split. Determine why incoming propagation reaches different
conflicting binaries first.

## Valid replay, incomplete tails and explicit observer holes

7401/job1681555 ran full unchanged candidate and native solvers. The original
250000-row native event trace capped at85.7403/job1681557 repeated only the
native arm with500000 rows; its first250000 data rows exactly reproduce7401,
but it also capped at85. Both full1--85 attempts remain rejected/incomplete.
The old strict7402 and transaction7404 validators are preserved; neither has a
successful summary. The initial one-event-per-integer-conflict assumption was
invalid for OTFS and conflict-reuse paths, not evidence of a solver failure.

Separate frozen `7403/core-summary-7406.json` accepts only the predeclared
1--82 completed core:62607 candidate and53843 native rows,81 complete analysis
transactions each. It requires raw framing/counter checks, an observed later
conflict before the cap, and exact whole own work, certificate and decision
stream replay against7397. Candidate boundaries replay exactly; macro rows
allow only explicit observer additions. Native boundaries account for added
analysis/end entries. This core does not turn either capped full trace into a
successful one. Independent proof acceptance and cleanup remain separate.

Native `src/eventtrace.c` has a4096-byte clause-payload buffer. At14, all four
candidate learned-stage payloads exceed it and the corresponding native rows
are absent. Missing payloads are neither formula equality nor absent learning.
Binary `ref/glue/used/redundant` metadata differs by representation, so the raw
first metadata difference at6 is not the first semantic conflict difference.
7407 retains raw evidence and compares literal order, sets and level maps
separately; it does not assert cross-representation physical metadata equality.

A further read-only cross-check found the jointly observed final learned
clauses first differ at82 (74 jointly observed final rows, not all81
transactions). At30 the raw clauses agree, the `learned_sorted` rows differ,
and shrunk/final clauses agree. Source7371 `src/rules/analyze/analyze.cpp`
stores `otfs_sorted_trace` after sorting, then overwrites it when the shrunken
clause has at most two literals. This explains that intermediate observer
mismatch. It is not a performance defect, and no submitted observer was edited.
The conflict82 binary stages are unaffected by this overwrite issue.

## Next discriminating experiment, already submitted

7408/job1681558, submitted September11 at13:58:22EDT, uses the same immutable
binaries and copied unchanged runtime wrappers. It enables only selected watch
snapshots for propagated `-1038` with clause poses/large-clause values, and ends
the detail window at82. Source confirms both ordinary-search implementations
snapshot the watch list before scanning and stop at the first conflict.

Validator7409 first requires complete own solver statistics/certificate/decision
replay against7397. Every original completed-core7406 row must replay except
sequence number, with only selected watch rows added. It rejects a capped trace,
missing/changed rows, malformed watch payloads, or merged/noncontiguous
pre-conflict snapshots. Physical order and duplicate entries are retained.
Both binary clauses may or may not be present, and ordered membership alone
cannot prove they were simultaneously false: binary watch payloads omit values.
No repair or root-cause claim is authorized merely by a changed list order.
The later7411 audit and optional7409 seal own publication/replay outcomes;
this report does not presume a pending solver or proof passed.

Full-width allocation remains cpu-dense-hwulab-q,12 logical CPUs,48000MiB,
1200 solver-only seconds,16000 checker seconds inside five hours, no throttle,
no node constraint or address-space limit, one solver/task bound to one granted
thread. No fixed-reference refresh or timing acceptance claim. Watcher51749
uses the maintained named `--job` interface; a positional invocation was rejected
before monitoring and did not submit or restart anything.

## Validation and scope

377 benchmark-tool and22 SLURM tests pass. New7407 clause controls cover
physical-ID/order equivalence, changed sets/levels and eight malformed cases.
The initial new-parser rejection of valid candidate `id` metadata was fixed
with a regression before sealing7407.7408 adds two-prefix/twelve missing-key
controls.7409 adds duplicate-preserving positive and five negative snapshot
controls. Existing core two-positive/six-negative, transaction eight-negative,
watch-payload/replay, native-boundary, macro and full-work checks remain active.
Documentation size/references, default strategy, proof-rule synchronization,
option coverage and rule/policy boundary checks pass. An empty unittest discovery
under `tests` was not counted as the22 SLURM tests; the owning `tools/slurm`
suite was run explicitly. Fixed fuzz-snapshot and memory-enforcement checks pass.

No solver unit tests or duplicate fuzz are needed for these unchanged-binary
diagnostics; new tooling received focused controls. Current-source direct
Codex-owned fuzz7395 remains PASS (50000 generated,48306 checked,1694
unverified timeouts,zero failures). Productionc2a95a6 is unchanged. Current
Rank1--50 remains8138.708/7239.010=1.124284674x, not an aggregate runtime
improvement; root-storage memory gains remain useful and memory parity open.
Preserve established4/32/33/49/46 closures, shelved21 in scoring, and N1--N4.

The Rank31 profile7396 still shows large work and cost excess: inprocessing is
33.1% of sampled excess, with much additional ordinary propagation work. This
shorter UNSAT causal witness does not replace inprocessing unit-cost work or
prove CDCL/scheduling parity. No small leaf optimization can be assumed to close
the aggregate gap.

7411 binds publication, accounting, live Debug7342 process state, owned paths,
and immutable pins. No manual material deletion, old-campaign mutation, Claude
invocation, denied transfer/push retry or production promotion occurred. Active
checkers own their residue; pending is not accepted. Andrew's local unsent
two-page deliverable is `report-to-andrew/20260911-1404-rank43-conflict-selection/`
(`report.tex` and `report.pdf`, compiled twice without Overfull boxes).
