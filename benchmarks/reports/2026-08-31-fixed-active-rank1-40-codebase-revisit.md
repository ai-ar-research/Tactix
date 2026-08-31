# Fixed-variable active state, Rank 1--40 validation, and codebase revisit

Date: 2026-08-31

## Outcome

Tactix's shared live-active population now matches Kissat's state semantics:
a semantic level-zero fixed variable is inactive even though Tactix retains
its assignment and stable identity for proof and model reconstruction. A
non-root search assignment remains active. This is one formula-independent
kernel predicate repair, not a new strategy, option, or classification
consumer.

The homogeneous Rank 1--40 campaign `20260831-3760-tierd-ranks1-40-fixed-active`,
Slurm job `1657639`, uses Release SHA
`5743b0a65b76b3cc92d6011b9021744d231c0544cbfc35f14350d63aa5a03698`
for every rank. All 40 provisional solver records have the expected verdict;
the total is 10,290.269 seconds versus 10,573.791 for the immediately
preceding homogeneous candidate. Geometric mean improves 2.6%. All 22 SAT
models and all 18 UNSAT proofs are independently verified. Every checker
record is valid and clean.

The candidate is a general positive increment and should be retained. It does
not close all convergence work: Rank 33 regresses sharply, Rank 17 remains
2.58x Kissat, and peak memory remains owned by Rank 10's representation.

## Reference semantics and repair

Pinned Kissat `flags.c` gives `solver->active` explicit lifecycle ownership.
`kissat_mark_fixed_literal` sets the fixed flag and immediately calls
`deactivate_variable`; that function clears the active flag, decrements the
population, dequeues the variable, and removes it from the stable heap. Search
assignments do none of those things and remain active.

Tactix deliberately cannot identify fixedness with physical retirement. It
retains root assignments and stable variables for N1 reconstruction and N2
proof chains. The former `kernel_active_variables` therefore counted every
non-retired identity, including root-fixed variables. Rank 17 exposed the
error: Tactix reported 944,336 active identities at first BVE entry even
though only 328,081 were unassigned and scheduled. Its 616,255 retained trail
identities account for the difference.

The repaired helper excludes an assigned variable exactly when its semantic
assignment level is zero. The level test matters: a current non-root decision
or implication is still active in Kissat. Focused regression coverage pins
both sides of this distinction.

## Consumer audit

The revisit audited every production call to `kernel_active_variables` and
the corresponding pinned Kissat source. No additional consumer was invented.

1. Factor admission uses the population in Kissat's `kissat_factoring`
   `log10(active) <= eliminations + factordelay` predicate. Tactix's existing
   Factor entry predicate consumes the corrected population; Factor remains
   uniformly enabled at 50 per mille.
2. Substitute round continuation snapshots `solver->active` and continues
   only when a round removes an active variable. Tactix's existing per-round
   productivity test now observes the same state definition.
3. Probe and Eliminate outer fixed points compare active population before and
   after a round in pinned `probe.c` and `eliminate.c`. Tactix's existing
   authored phase loop already uses the shared helper for this comparison.
4. Two Trace-only sites print the population for diagnostics. They do not
   influence Release behavior.

Heuristic membership remains separately and correctly represented by
`heuristics_var_active`, which tests physical VMTF queue membership. Variable
collection admission already treats retired and root-fixed identities as
inactive on its compact representation. BVE candidate membership is not
routed through this count; campaign `3740` proves that the repaired value does
not change the first BVE schedule, formula, scores, or search fields.

This separation is intentional and clean: semantic population, physical
heuristic membership, collection population, and BVE occurrence membership
are related reference states but are not interchangeable abstractions.

## Diagnostic observer revisit

The original global Rank 17 trace was invalid as an observer: both solvers
timed out with header-only output because high-volume trace hooks incurred
cost even when the requested window discarded their rows. The replacement
observer is compiled only under `TACTIX_ENABLE_EVENT_TRACE`. It remains idle
until the first BVE schedule, writes a symmetric one-row summary plus optional
members, and exits before the first candidate. The Kissat overlay follows the
same contract.

The Release binary does not contain either standalone schedule environment
name. `TACTIX_ARENA_ORDER` remains present solely as the already-documented
explicit CaDiCaL diagnostic arm; Kissat creation order remains the
unconditional production default.

The observer closed its purpose and is small enough to retain as a bounded
future divergence tool. It is not a production profile, benchmark selector,
or automatic diagnostic.

## Causal evidence

Campaign `3710` first showed different schedule hashes and populations.
Campaign `3720` then compared every member: 322,663 variables are common,
5,418 occur only in Tactix, 6,132 only in Kissat, 72,684 common members have
different occurrence pairs, and 320,706 have different heap positions.

Campaign `3730` showed that the first Eliminate conflict boundary is nearly
identical (18,268 versus 18,269), but the physical states are not. Tactix has
1,910,024 irredundant clauses, 66,106 binaries, and 616,255 retained root
assignments; Kissat has 1,699,758 irredundant clauses, 254,564 binaries, and a
5,593-entry trail. The first BVE schedule discrepancy is therefore upstream
state, not a schedule-admission bug or direct Factor cost.

Campaign `3740` changes only Tactix's active summary from 944,336 to 328,081.
Campaign `3750` then solves Rank 17 in 311.993 seconds versus 449.150 before
the repair. The broad campaign reproduces that result at 314.130 seconds.

## Homogeneous aggregate

| Measure | Candidate | Preceding | Ratio | Kissat | Ratio |
|---|---:|---:|---:|---:|---:|
| Total seconds | 10,290.269 | 10,573.791 | 0.9732x | 7,675.236 | 1.3407x |
| Geometric mean | -- | -- | 0.9739x | -- | 1.3303x |
| SAT seconds | 4,520.427 | 5,034.213 | 0.8979x | 4,057.914 | 1.1140x |
| UNSAT seconds | 5,769.842 | 5,539.578 | 1.0416x | 3,617.322 | 1.5951x |
| Solved | 40/40 | 40/40 | -- | 40/40 | -- |

Largest improvements versus the preceding candidate are Rank 19
(452.403 to 166.486 seconds), Rank 31 (628.742 to 461.093), Rank 17
(449.150 to 314.130), and Rank 34 (807.219 to 692.338). Rank 36 remains
closed at 385.209 seconds versus Kissat's 368.510.

Rank 33 is the material regression: 395.771 to 753.767 seconds. Smaller
regressions occur on Rank 37 (475.753 to 499.286), Rank 6 (86.450 to
108.434), Rank 22 (420.106 to 436.346), and Rank 40 (420.114 to 433.339).
This is downstream trajectory evidence to localize from a common state, not
grounds to revert the general repair or introduce a rank/formula gate.

The full per-rank runtime and memory table is frozen in
`benchmarks/runs/20260831-3760-tierd-ranks1-40-fixed-active/comparison-live.md`.

## Memory and remaining owners

Peak process-tree RSS is unchanged at 16,987,860 KiB, still Rank 10. The
active-state repair is not a representation compaction and was not expected to
remove retained arrays, clauses, watches, or proof/model identity storage.
Rank 10 remains the first memory owner; medium instances still frequently use
two or more times Kissat's RSS.

The next mechanism priorities are:

1. Localize Rank 33's first common-state divergence after the active-state
   repair, retaining the repair throughout the investigation.
2. Continue Rank 17 upstream from its already-different first-BVE physical
   formula/root/search state rather than changing BVE or Factor admission.
3. Revisit Rank 10 representation ownership for portable memory convergence.

Release passes 76/76 and Trace 75/75 after the revisit. N1--N4, the proof-rule
and strategy boundary, uniform production defaults, and the exact Kissat
classification consumer set remain unchanged. No formula, density, size,
benchmark, identity, rank, or arbitrary production gate was added.
