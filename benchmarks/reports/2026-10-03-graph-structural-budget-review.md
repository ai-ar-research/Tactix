# Structural graph budgets and stall-cooling review

2026-10-03. Status: source review and eight fixed controls complete; production
profile and N4 acceptance remain OPEN. No solver source or policy changed.

## Outcome

Two constructor phases admit source-derived finite GraphWork bounds, without a
machine-specific work/second conversion. These bounds can replace arbitrary
allowances for complete seed construction and a declared DFS node limit. They
bound charged helper work, not wall time, actual memory or full action work.
The preceding [meter calibration](2026-10-03-graph-effort-calibration.md) remains
the authority for measured cost heterogeneity.

The experimental 100,000-attempt stall anneal has a concrete thermal limitation:
for separation 50 it cannot reach the reference minimum temperature within one
call. This is a scheduling gap to resolve, not proof that the reference reaches
that temperature in every one-second call, and not evidence of a SAT error.
The previous rank 991 success is unchanged; no additional target is closed.

## Preconditions and construction bound

Let n >= 3 be vertices, m undirected edges, and d_v each degree, for a validated
simple undirected graph accepted by the current helper contract. Let
S = sum_v d_v(d_v-1). Counting the current charge sites gives

`construction_bound = 60n + n(n-1) + 28m + S`.

For one breadth-first order, initialization costs 3n, start sorting at most
n(n-1)/2 comparisons, scanning starts n, visiting queue entries n, adjacency
scanning 2m, sorting neighbor lists at most S/2 comparisons, and queue insertion
at most n. Thus one order costs at most 6n + n(n-1)/2 + 2m + S/2.
Two low/high-degree orders give 12n + n(n-1) + 4m + S. Each of the 24 candidates
costs n initialization, at most n reflection, and m scoring: at most 48n + 24m
in total. The sum is the displayed bound. Fully charged candidates alone become
retained best assignments; no comparison can hide an unmetered interrupt.

This formula uses actual graph degrees. A coarser graph-size-only bound follows
from S <= 2m(n-2): `60n + n(n-1) + (2n+24)m`. Neither expression chooses how much
repair to perform or authorizes a production memory allocation. Recognition and
publication are separate phases and not covered by this construction bound.

## Window-search bound

Let q = ceil(n/64), L the requested node limit, A = min(n, starts),
r = min(n-1, separation-1), and C = n(n-1)/2. A conservative charged-work bound is

`window_bound = nq + 3n + 2m + C + An + L * max((r+1)q + 2n + 2m + C, 1+2n+m)`.

The prefix accounts for the bitmap/arrays, filling adjacency bits, sorting starts
and resetting state for at most A starts. At each internal node, charge sites
cover entry, bitmap clearing and recent-vertex unions, scanning vertices and
remaining degrees, shuffling candidates and sorting them. Their sum is bounded
by `(r+1)q + 2n + 2m + C`; the shuffle's n-1 offsets the entry's one. A complete
leaf costs at most `1+2n+m` for entry, assignment creation, optional reflection
and objective validation. The node cap bounds the number of these entries.
Early-choice restriction only decreases this upper bound. Backtracking visits
no new charged operation outside these accounted entries.

This is intentionally conservative, especially the per-node sorting bound.
It must not be installed as an unconditional production allocation simply
because it is finite: completing 50M worst-case nodes can cost far more than the
reference's independent 70-second cutoff. It instead establishes which operation
limit a smaller allowance can truncate. Future conversion to uint64_t must check
arithmetic overflow and the inherited action ceiling; the review helper uses
128-bit arithmetic. Zero/invalid profiles retain their existing semantics.

## Fixed validation

`benchmarks/runs/20261003-graph-structural-budget-review/fixed-controls.cpp`
checks predetermined path/clique graphs at n = 3, 4, 8, 16. For each, all 24 seeds
complete within the bound, and executing with exactly that allowance reproduces
unbounded labels and objectives. Window search uses a 1,000-node limit and the
reference 12-start/28-early-choice profile. Each actual work count is below its
bound; bounded and unbounded status, node count, work, labels and objective agree.
All eight controls pass with assertions enabled. These are fixed local helper
unit checks, not a solver benchmark, fuzz campaign or universal empirical proof.
The source-level charge derivation above owns the general bound.

The immutable standalone helper is retained at
`build/pin-graph-structural-review-499f04f7-01/fixed-controls`. The run manifest
records compiler/flags, source and binary hashes; CSV records actual/bound counts.
No new SLURM job was submitted for these allowed fixed microchecks.

## Annealing horizon

Both implementations use initial temperature max(1, separation^2/3), factor
0.99997 and minimum 0.05. Each attempted self-swap skips cooling. Ignoring floating
rounding, the minimum necessary number of cooling updates is
`ceil(log(0.05/T0) / log(0.99997))`. Attempts must be at least this numerous;
self-swaps can make the required attempt count larger. The recorded arithmetic
also iterates the native multiplication/division recurrence directly.

| Separation | Cooling updates to floor | Temperature after 100,000 updates |
|---:|---:|---:|
| 2 | 109,446 | 0.06638 |
| 21 | 266,202 | 7.31837 |
| 42 | 312,411 | 29.27348 |
| 50 | 324,035 | 41.48736 |
| 64 | 340,492 | 67.97289 |
| 85 | 359,409 | 119.89846 |
| 128 | 386,701 | 271.89154 |

The current 100k stall profile therefore cannot traverse the full cooling range
in any listed case, even if every attempt cools and no effort cut intervenes.
A later independent anneal stage may have more attempts; this finding concerns
the anneal invoked inside a stalled repair. Both reference and native restart
temperature on each such call. A thermal-horizon dose is a candidate for review,
not an adopted default: the reference uses time, not a requirement to finish a
cooling cycle, and cold-state exploration may also matter. Reaching the floor
alone would not close N4 or justify an arbitrary total phase budget.

## Remaining work and validation status

Use the finite-phase derivations when building a policy-owned structural profile;
keep the choices explicit through the shared typed action. Resolve repair,
anneal, stall and final-repair dosing jointly with reference scheduling and real
solver cost, without selecting defaults by synthetic solved counts. Recognition,
completion/publication budgets, identity/replay and effective both-policy reach
remain part of acceptance. No current default is promoted by this review.

The public Claude report still says PREPARING with no fuzz evidence. Known job
1747588 (wgf-smoke) is now COMPLETED by scheduler accounting, but its owner has
not published a disposition; no private artifacts were inspected. Claude CLI
session 76266 and diagnostic Debug session 93541 remain live when polled. The
latter still has 216/217 passes with its final scripted example pending. Full
current-pin Debug, both-policy fuzz/replay, N1–N4 and all four regression gates
remain required. All 44 targets and broader organic integration remain in scope.

Retain the run evidence, immutable binaries and existing checkpoints. No files
were deleted, no historical comparison rerun, and no new proof/CNF residue was
created. Andrew handoff: `report-to-andrew/20261003T200600Z-graph-budget-review/`.
