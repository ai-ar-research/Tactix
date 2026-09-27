# Phase 4.20 solving-strategy and source survey

**Status:** design report; no performance mechanism is implemented here.
**Sources:** Tactix, Kissat 4.0.4 (`kissat/`), and the four winner trees under
`winners/`. Frozen results, not source reputation, decide priority in Phase 4.25.

**2026-08-29 contract update:** the former content-profile and LymphoSAT-style
preset-selection proposals in this survey are withdrawn. Production is one
homogeneous Kissat-aligned default. Formula size, density, classification,
benchmark identity, rank, literal identity, and conflict count do not select a
strategy. Pinned Kissat's `classification.small` has no consumer;
`classification.bigbig` gates only ordinary fast binary jump-reason assignment.
Production BVE, Factor, forward subsumption, and Vivify remain enabled at
100‰, 50‰, 100‰, and 100‰ for every formula. Kissat creation order is the
default; `TACTIX_ARENA_ORDER=1` is a manually selected CaDiCaL diagnostic arm.
An experimental mechanism may remain explicit while it is measured, but an
accepted mechanism must ship uniformly or not ship at all.

## Result and order

Stage A should first remove local avoidable cost and close divergences against
Kissat 4.0.4. Later winner mechanisms enter only at their cumulative stage:
SC24 initial fast elimination, AE's VSIDS/CHB bandit, then Satsuma symmetry
breaking. They are candidate general mechanisms, not formula selectors. This
order prevents an experimental layer from masking a weaker core solver.

## Tactix source findings

| Priority | Finding | Proposed optimization and acceptance |
|---|---|---|
| A0 | repeated `O(nvars)` scratch initialization in BVE, Probe/Backbone, C1/C2, HBR/BCE and other rounds | persistent epoch/touched scratch owned by the rule support service; identical candidates, counters, proof SHA, and allocation profile |
| A0 | C1/C2 and several inprocessors rebuild overlapping literal occurrence structures | mutation-epoch read-only occurrence service with explicit irredundant/redundant views; invalidate on every formula mutation; bit-identical order |
| A0 | Factor counts large occurrences and later rematches them through a different lifecycle | retain the counted `qlauses` occurrence stream through selection; identical grids/proofs, fewer scans |
| A1 | `main.cpp` mixes CLI, environment posing, execution, and two strengthen print schemas | split frontend/config/reporting; one typed statistic schema; compile/tests only, no performance claim |
| A1 | public-instance queries repeatedly allocate temporary vectors | caller/reusable scratch with mutation-stamped snapshots; preserve enumeration order and purity |

These are hypotheses until counters and profiles show the relevant path. Apply
them one at a time under Tier A plus the triggering Tier-D instances; do not
bank a code-size cleanup as a speedup.

## Rule/strategy separation audit

The kernel currently stores a large *posed strategy frame*: `work_budget`,
`vivify_scope/reuse/flush`, BVE/Factor bounds and candidate controls, BCE/X2
limits, Reduce tiers, conflict-reuse/Analyze knobs, Sweep resumption, and a
`FactorSelection*` (`src/kernel/kernel.hpp:690–770`). Two items cross the boundary
more deeply:

1. `saved_phases` points from Kernel into policy-owned heuristic storage and the
   assignment primitive writes through it.
2. `factor_selection` points from Kernel to a policy object which the E2 rule
   calls during grid construction.

This does not presently make proof validity policy-dependent, but it violates
the plan's stronger claim that Kernel has no policy knowledge and makes stale
pose leakage possible between rule calls. It also duplicates configuration:
typed DefaultStrategyConfig → DefaultPolicy members → Kernel fields → rules.

**Proposed boundary:** introduce a short-lived `RuleApplicationContext` owned by
the dispatcher, containing budget, candidate scope, ranker/admission callbacks,
and observation sinks. Pass it to canonical application; keep the public bare
API context maximally permissive. Persistent resumption stays in policy, while
formula epochs/cursors required for correctness stay in rule state. Replace
the saved-phase pointer with an assignment observer installed by the session,
not a Kernel field named after policy H. Migrate one rule at a time and require
null/default trajectory identity, random-policy N1/N2, and no hot-loop overhead.

Do not move soundness checks, proof-chain construction, model witnesses,
mutation order, or deterministic construction of a supplied instance into the
context. Conversely, admission, ranking, cadence, effort allowance, and
resumption are strategy even when a reference implements them inside a rule.

## Stage A — Kissat 4.0.4

Use frozen per-instance gaps in descending coverage/PAR impact. For each, the
rule journal identifies the earliest portable divergence before any tuning.
Candidate families are the A0 costs above, remaining N4 accepted deviations,
and reference mechanism mismatches. Every production candidate uses one
uniform configuration over the complete evaluation set.

Exit remains every Kissat solve plus ≤1.10× PAR-2 on SC24, SC25, SC26, and
their union. If a mechanism does not move its causal fixture, remove the arm.

## Stage B — kissat-sc2024

A source diff against `kissat/` isolates two intentional scheduling differences
amid numerous later public fixes: SC24 defaults `fastel=1` and removes the
`factordelay` option. Tactix currently excludes elimination from preprocessing
(`default_policy.cpp:2116`) and carries a measured Factor admission deviation.

First test an explicit initial-fast-elimination diagnostic pose matching SC24's
occurrence/resolvent limits; record its formula delta, proof/model cost, and
downstream effect. If accepted, it becomes the uniform default for every
formula; formula shape cannot activate it. Test Factor timing separately. Do
not port SC24's reversed fresh-variable ordering or congruence defects visible
in the source diff; those are not strategy improvements.

## Stage C — AE-Kissat-MAB

AE adds a second stable-mode CHB heap and selects EVSIDS versus CHB at stable
restarts. Its CHB uses age-weighted propagation rewards; restart reward,
momentum, adaptive exploration, UCB selection, and root restart on arm switches
are visible in `restart.c:113–205`. Tactix tested this mechanism in AM177:
WV87 verified in 1.620s versus the retained AE owner time of 29.358s. Later
broader evidence did not justify keeping the two-arm implementation, so the
user directed its removal. The frozen AE binary and measurements remain
reference material; Tactix stable search uses EVSIDS alone.

## Stage D — Satsuma+Kissat

The winner runs Satsuma BSR once, writes a DSR prefix, solves the transformed
CNF, and checks the combined proof with dsr-trim. Symmetry detection and BSR
clause choice are strategy; adding a non-entailed symmetry breaker is a proof
rule. Therefore Stage D depends on P3 in the proof-rule survey. Until a native
DRAT/LRAT story exists, collect symmetry features/yield only and do not mutate
the formula.

## Withdrawn Stage E — LymphoSAT content selection

LymphoSAT profiles clause-length, polarity, Horn, binary-graph, occurrence,
span, and block-structure features, then checks 126 ordered threshold matchers
before a Kissat fallback. The matchers use content, but many windows are narrow
enough to act as family fingerprints (for example Purdom variable/clause ranges).

That selection architecture is outside the current project contract. Even a
small feature vector would make production behavior depend on formula content
through predicates pinned Kissat does not use. LymphoSAT remains useful as a
source of general search mechanisms and as a frozen VBS comparator, but Tactix
will not add content matchers, family fingerprints, or automatically selected
presets. A generally useful mechanism must be evaluated as one homogeneous
candidate and, if accepted, become part of the uniform default.

## Implementation queue after frozen results

1. Rank Stage-A misses from frozen Kissat data and profile the first causal path.
2. Apply the smallest A0 optimization or semantic repair that addresses it.
3. Close Stage A before activating the SC24 fast-elimination experiment.
4. Add AE or Satsuma mechanisms only at Stages C and D, and only as candidates
   for one uniform default.
5. Preserve one homogeneous binary and cumulative correctness/performance
   guards throughout; never add a content/rank/benchmark selector.
