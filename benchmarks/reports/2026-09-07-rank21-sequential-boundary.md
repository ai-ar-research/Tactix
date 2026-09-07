# Rank-21 sequential-rewrite boundary

Objective: Rank 1--40 within 1% of Kissat through general implementation
repairs, with B28+C5 regression gates. The objective remains unmet.
This continues `4d0c3c97e7f6170bf57e701801178e7375ac32af` and
[the original-core investigation](2026-09-07-original-core-convergence.md).
The preceding goal turn was progress: checked ablation 5330 isolated
sequential rewriting as sufficient to change Rank-21 work; physical
publication alone did not. This continuation changes no production solver.

## First-conflict exclusion

5331/job 1668664 compares core-only, sequential, and Kissat on the unchanged
Rank-21 input (`pj2008_k80`, compressed SHA
`07b881ae5d1057ccbfdd564ddebf1a8f6e45582542b96aee21974ccdcd446450`).
All three tasks terminate and delete scratch. The two Tactix arms have
27 byte-identical macro snapshots through conflict 1 (trace SHA
`1f56a521326aa6b695594bfa21663d7707b90431171445e0c69c858f142ae944`).
The initial Sweep is present and changes neither arm differently in these
recorded counts, formula hashes, or assignment/queue-state digest. This does
not establish identical unobserved watch order.

Kissat's trace is invalid: that pin does not implement `SNAPSHOT_ONLY`, so
import `phase_save` fills the 10,000-row cap. Preserve it as observer failure,
not solver failure or reference evidence. 5332 adds a read-only early filter
to a fresh Kissat trace build; original stopped pins are not rebuilt.

## Extended boundary probe

5332/job 1668667 reuses the two immutable Tactix trace pins, with the corrected
Kissat observer. It extends the conflict stop to 10,001, retains simplification
boundaries, and adds formula plus order-sensitive watch hashes only at Sweep
boundaries. Boundary-only filtering omits the terminal analysis event;
configured stopped exit codes and runner cleanup are checked separately.
Observed snapshots do not prove that unobserved decisions match. All three
tasks hit the 600-second diagnostic cap (exit 124) and deleted scratch;
none reached the selected conflict 10,001. Source
inspection explains the measurement problem: filtering emitted rows does
not suppress all hot-path payload work, while native restart snapshots scan
large global state. These instrumented times are not solver performance.

5333/job 1668671 instead leaves global EventTrace unopened and uses the
existing standalone inprocessing boundary logger. It runs complete proof-on
solves with the existing independent model checker and cleanup pipeline.
The pinned runner differs only by four lines selecting the standalone output
path after environment sanitation; oracle logic is unchanged. All three
full proof-on SAT solves have independently verified models and clean scratch.
The Tactix traces match through row 120. Row 121, the **second** Sweep end at
conflict 23,889, first differs: core has 621,085 binary copies, sequential
621,089. Decisions (25,465,792), propagations (397,081,740), active variables
(321,017), trail (53,471), large-clause populations (764,168 irredundant,
1,130 redundant), total ticks (1,135,282,758), and search ticks (908,605,759)
remain equal. This is a clause-population difference, not just tick accounting.

Kissat has already taken a different trajectory by this point: its corresponding
phase is at conflict 23,890, with a substantially different active formula.
Do not compare that state as if it were the same transaction. The exact Tactix
ablation is the producer witness; reference mechanisms require matched-state
analysis or a focused fixture.

All campaigns use full-width arrays, 12 logical CPUs/48,000 MiB, one bound
proof-on solver per task, and no node constraint or throttle. Stopped probes
5331/5332/5334 use a 600-second cap; checked full solves 5333/5335/5336 use
1,200 seconds for solving and 16,000 seconds for checking. Instrumented
times are not performance acceptance. The isolated candidates remain unfuzzed
and unpromoted; production is still the fuzz-passing `de3d9332...` Release.

## Exact binary-copy witness

5334/job 1668674 stopped cleanly but selected the first Sweep at conflict
23,889. Its complete 1,007,634-copy multisets are byte-identical; retain this
as a selector mistake and earlier-boundary exclusion, not the changed target.
5335/job 1668676 explicitly selects ordinal 1 and runs both solvers through
independent model checking. Both models pass and scratch is clean; their full
standalone boundary traces are byte-identical to 5333.

The complete selected binary multisets differ only by four extra copies in
the sequential arm. Each pair already has one copy in core and has two in
sequential; there are **no new unique binary clauses** and no core-only pairs:

| External literals | Core copies | Sequential copies |
|---|---:|---:|
| `(-364071, 212244)` | 1 | 2 |
| `(-364068, 212244)` | 1 | 2 |
| `(-306948, 211419)` | 1 | 2 |
| `(-306945, 211419)` | 1 | 2 |

The comparisons validate export markers/counts, pin identity, verified SAT
models and cleanup; `5335/summary.json` records exact trace hashes. This proves
binary multiplicity at the selected boundary, not full-formula or watch-order
equality. It does not establish that the four duplicates alone cause the
downstream regression; sequential rewriting can also change unobserved order.
5336/job 1668678 selects those four pairs in the existing standalone lifecycle
observer to distinguish large-clause collapse, rewrite, and bridge producers.
Both full SAT models pass and scratch is clean; their full boundary traces
are byte-identical to 5333. Core has 12 selected lifecycle rows at the target
conflict, sequential 28. Both create each pair once (`add-new`); only sequential
has four `sweep-physical-target` and four `add-copy` rows, each reaching two
copies. No selected `sweep-large-collapse-source` row occurs. Endpoints are
unassigned at every selected event. The extra copies are rebuilt from binary
images in `do_substitution`, not born from large-clause collapse. Target-only
rows do not identify source endpoints, and standalone operation has no global
rule-scope label; correlate source/target images and emitted equivalences next.

## Gates, validation and next action

No production solver edit is made in this continuation. Release passes all
77 CTests (5.17 seconds). All 30 documentation budgets pass, with four
near-cap warnings; no requirement or N4 deviation is removed to shorten docs.
5314 now has 79/80 checked/clean records, zero invalid records; Kissat Rank 18
is the remaining checker. Aggregate solved time remains provisionally
8,643.178/6,759.074 = 1.2788x. B28+C5 5315 remains 66/66 checked/clean,
final-r2/starting aggregate 1.01897x. Neither establishes the 1.01x objective.

Next: correlate 5336's binary-image targets with their source pairs and emitted
equivalence order, compare the mechanism against Kissat on matched state,
and construct a focused fixture before
changing proof-rule behavior. Preserve proof lifetimes: all captured pair
proofs are emitted before source-deleting rewrites in the isolated candidate.
Do not promote sequential rewriting or add benchmark-specific gating.

All 15 tasks across 5331--5336 are terminal with runner-confirmed scratch
cleanup; the seven full solves have checked models. No manual benchmark
deletion was needed. Normal run-local pins and raw evidence are retained;
fuzz artifacts were not independently inspected. The Andrew TeX/PDF is in
`report-to-andrew/20260907-1331-rank21-sequential-boundary/`. Local checkpoint
publication remains unapproved after the earlier auto-review rejection.
