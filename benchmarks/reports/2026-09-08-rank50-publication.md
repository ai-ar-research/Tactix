# Rank1–50 target and Substitute lifecycle repair

## Approved scope and matched starting point

The user replaced the Rank1–40 runtime objective with Rank1–50, retaining the
aggregate ≤1.01× Kissat target, B28+C5 regression gates and general component
repairs. The approved ten additions are seven UNSAT and three SAT cases from
the preceding cpu-q proposal. The complete view is25 SAT/25 UNSAT. Membership
and input hashes are frozen in `benchmarks/baselines/rank1-50-selection-v1/`.
The proposal remains a frozen historical artifact; its pending-choice language
is superseded by this explicit approval and the plan.

Campaign6108/job1676783 ran current6049 and the original pinned Kissat on the
new ten instances. It requests12 logical CPUs,48000MiB,1200 solver-only seconds,
16000 checker seconds and5h allocation on cpu-dense-hwulab-q, without a node
constraint or throttle. Each solver has its own task; pairs are instance-major.
The runner/checkers are byte-identical to the original fixed-reference campaign.
Node/affinity/L3 placement is recorded; this is the same allocation contract,
not a claim that every pair occupied the identical physical core.

All20 solver outputs match expectations. The new fixed Kissat vector totals
7239.010s: the original40 retain6810.066s exactly and ranks41–50 add428.944s.
No cpu-q timing is included. The fixed50 manifest records per-row provenance so
later certificate audits find the correct campaign. It is a single predeclared
measurement vector, not best-of. Some certificates remain pending.

Before the new repair, current6049 totals8807.881s over50, or1.216724524×Kissat.
The immutable starting snapshot has46 candidate results checked/clean and16
of20 new paired results checked. These counts can advance without rewriting
the snapshot. All50 expected results and the identical6049 binary are bound.
This combines the original40 and new10 across time windows; it is a starting
fixed-reference score, not an isolated unit-cost comparison.

| Rank | Status | Kissat s | Tactix6049 s | Ratio |
|---|---|---:|---:|---:|
|41|UNSAT|11.585|15.090|1.303×|
|42|UNSAT|22.752|66.525|2.924×|
|43|UNSAT|46.089|126.542|2.746×|
|44|UNSAT|38.827|58.866|1.516×|
|45|UNSAT|45.187|79.399|1.757×|
|46|UNSAT|80.043|147.014|1.837×|
|47|UNSAT|98.418|142.110|1.444×|
|48|SAT|14.441|135.187|9.361×|
|49|SAT|22.555|161.070|7.141×|
|50|SAT|49.047|432.962|8.827×|

Full identities and source/result bindings are in the membership manifest and
`2026-09-08-rank50-start.json`. The larger SAT losses make Rank50 a priority
alongside existing Rank37/21; short UNSAT cases remain useful debugging probes.
No benchmark selector or tuned constant is added to the solver.

## General repair and causal evidence

Completed6100/job1676781 retains the immutable6090 observer binaries and fully
replays6092's ordinary traces, macro heaps, prior auxiliaries and partial proof
bytes. At the first Substitute call at conflict272979/decision76952961, both
heaps enter identical. They remove the same117 members but leave296 positions
different. Tactix removes root91252 before110 eliminations; Kissat removes it
after those eliminations, before the remaining six removals.

The6103 driver links actual Tactix heap objects. Replaying each observed removal
order from the shared incoming heap exactly reconstructs that arm's complete
output heap: zero differing positions in both cases. Positive equal-score
fixtures and a wrong-pose negative control validate the replay. This explains
the observed heap transformation; it does not yet prove repaired full-search
trajectory or runtime parity.

The6107 successor changes only DefaultPolicy publication and adds one test.
Each Substitute round now consumes its roots and then its import-ordered
retirements before another round or final watch reconnection derives more
roots. Budgeted and unbudgeted policy calls are covered. Public formula rules,
guards, proof deltas, reconstruction, score comparator and round budgets/caps
remain unchanged. The public rule-only API stays independent of heuristics.
This is scheduling/lifecycle repair, not heap tie tuning.

The new fixed fixture constructs two SCC collapses and an earlier arena clause
that becomes unit only at a later round/reconnection. Eight cases cover
initial/search calls, budgeted/unbudgeted calls, and one/two-round caps; they
pin full heap positions/scores, values and active flags. Old6049 fails the
expected heap assertion. The first expanded fixture omitted preprocessing
admission and therefore did not invoke the rule in those cases.6111 fixes
only that test setup; both complete Release suites then pass99/99 and the
independently built solver binaries have identical hashes. Failed logs remain.

The changed source and patch are frozen under campaign6107. No submitted
parent source or pin was rebuilt. Existing public API, proof/reconstruction,
invariant and slot-budget coverage remains in the full suites. New-source
proof/model benchmark checks and Claude-owned fuzz are still acceptance gates;
the denied fuzz transfer is not retried or bypassed.

## Running gates, tooling and next action

- 6114/job1676803: unchanged B28+C5,66 tasks, frozen production comparison arms.
- 6115/job1676804: candidate-only full50,50 tasks, against the new fixed vector.
- 6108/job1676783: solver phase complete; remaining original-formula checks run.

These are independent campaigns with immutable runners/pins. Full-tier cpu-q
5784 and the original40 fixed reference remain untouched. Development proceeds
while certificates check; no pending result is called verified.

`tools/bench/fixed_runtime.py` now supports both frozen40 and50 schemas, defaults
new preparations to50, and checks original-forty identity and resource parity.
Scoring requires all50, and resolves appended reference certificates through
their own source campaign. New tests reject partial40/49 views, changed original
timings and4-CPU reference substitution. The24 benchmark protocol tests pass;
six selection rejection controls protect membership and paired task mapping.

Next inspect both cumulative gates, trace repaired Rank37 against cached6100
native evidence, and pursue the largest remaining full50 discrepancy. Trace
and journal builds of6107 still need validation before claiming diagnostic
parity. No runtime improvement, promotion, or goal completion is claimed here.
The plan and status change follows explicit user approval; the implementation
skill keeps the repair in policy and requires the added regression coverage.
Andrew's matching report is local, not sent. No push or denied fuzz retry.
