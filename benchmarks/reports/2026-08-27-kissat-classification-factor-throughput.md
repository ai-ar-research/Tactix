# Exact Kissat classification and general Factor throughput

Date: 2026-08-27

## Outcome

Tactix no longer selects a production strategy from formula size, density,
benchmark identity, or rank.  Its formula classification now reproduces the
pinned Kissat implementation exactly and is wired to exactly Kissat's one
classification consumer.  The unsupported Dose5, Factor5, NoBve, NoBveLight,
and StartupFactorOnly profiles are gone.  Production BVE, Factor, forward
subsumption, and Vivify remain enabled with invariant effort settings of
100 permille, 50 permille, 100 permille, and 100 permille respectively.

Removing the old Factor population ceiling exposed a general throughput defect
rather than a reason to restore a formula gate.  A representation and growth
repair preserves Factor's selected work while reducing Rank 9 Factor time from
52.335 to 6.746 seconds and total solve time from 166.072 to 120.799 seconds.
No admission predicate, formula classifier consumer, or benchmark exception was
added.

## Reference behavior and implementation

The pinned Kissat classifier counts `clauses = binaries + irredundant_large`.
It sets `small` iff `clauses <= 100000`.  Only a non-small formula can be
`bigbig`, using the literal reference comparison
`100000 * binaries >= 990 * clauses`.  The apparent unit mismatch is
intentional parity: Kissat's `bigbigfraction=990` per-mille option is passed to
a percent helper, so the effective boundary is 0.99 percent, not 99 percent.
Tactix uses 128-bit intermediate arithmetic to retain the predicate without
overflow.

The consumer audit found:

- `classification.small` is write-only;
- `classification.bigbig` gates only ordinary fast binary jump-reason
  assignment; and
- learned-binary assertion and conflict reuse use Kissat's second assignment
  site and deliberately bypass `bigbig`.

Tactix refreshes the classification at the same initial-search and
post-Reduce/Probe/Eliminate boundaries.  Classification never mutates
`DefaultStrategyConfig`.  Explicit configuration and environment overrides
remain available as diagnostic arms, but nothing selects them from formula
shape or benchmark identity.  The static ownership checker rejects the former
profile names, formula-shape inputs in `DefaultPolicy`, benchmark `.cnf`
literals, tier literals, and instance-identity APIs.

The Factor repair replaces repeatedly rebuilt hash sets and remainder vectors
with compact watches, interned immutable remainders, and epoch marks.  Kernel
identity, trail, and frame vectors now grow geometrically, and heuristic import
order/inverse state appends incrementally when Factor introduces extension
variables.  Profiling before the repair attributed 28.23 percent of samples to
`kernel_new_extension_vars`, including 19.43 percent in
`heuristics_reserve_vars`; this was a general repeated-allocation defect.

## Correctness evidence

The Release suite passes 76/76, including the following new coverage:

- exact `small` and `bigbig` threshold boundaries;
- the sole ordinary-propagation consumer and the ungated learned-binary path;
- invariant serialized production configuration over a size/density grid;
- preservation of explicit typed diagnostic overrides;
- rejection of benchmark-specific or legacy-profile policy code; and
- Factor representation, extension-variable growth, N1, and scheduling
  fixtures.

`tools/docs/check-default-strategy-config.py` and `git diff --check` pass.  The
trace-enabled suite previously passed 75/75 and the event-trace sanitizer run
passed.  N1--N4 remain in force; the parity ledger records the removed gate,
the general throughput repair, and the remaining Factor-quality discrepancy.

The independent fuzz result is recorded in the final campaign section below.

## Focused and cumulative performance

Three paired Rank 9 waves preserve exactly 51,357 factorings, 8,623,146
offers, 130,578,868 Factor ticks, 41,322,140 decisions, and 50,577 conflicts.
Candidate Factor/solve seconds are 6.746/120.799, 6.798/122.201, and
6.693/120.554; predecessor values are 52.335/166.072, 53.068/167.524, and
53.241/167.430.

The B28+C5 cumulative campaign
`benchmarks/runs/20260827-2530-BC-factor-amortized/` is performance-complete for
all 66 arms.  Candidate/predecessor PAR-2 is 1,433.058/1,329.014 seconds, a
1.078287 ratio that passes the frozen 1.10 guard.  SAT rows require independently
verified models.  In accordance with the user's explicit instruction, UNSAT
checker completion is non-blocking and is assumed to pass; the checker lane may
continue in the background and any later contrary result reopens correctness.

The homogeneous Rank 1--40 campaign
`benchmarks/runs/20260827-2540-tierd-ranks1-40-factor-amortized/` uses one
candidate binary for every rank.  It is performance-complete with 36 valid
solves, timeouts at ranks 15, 26, 30, and 36, and PAR-2 22,116.682 seconds.
This improves the exact-classification pre-repair candidate by 2,335.589 seconds
(9.551624 percent) and accepted Tactix by 640.172 seconds (2.813095 percent).
Current Kissat remains substantially ahead at 7,682.181 seconds PAR-2.  Each arm
has 29 checker-coupled and 11 observation-fallback rows; the same non-blocking
proof assumption applies.

## Remaining risk and next mechanism

The residual is not evidence for reinstating a formula profile.  Rank 20 is the
clearest general witness: current Tactix performs 498,112 factorings in 51.437
seconds and produces a verified SAT model in 577.242 seconds, while current
Kissat performs 493,961 factorizations and solves in 140.185 seconds.  Tactix
then reaches 463,455 conflicts and 6,173,754 decisions versus Kissat's 81,993
conflicts and 1,229,783 decisions.  The next comparison surface is therefore
selected Factor grids/order and the first post-Factor state/search divergence.
Any repair must remain formula-independent and retain Factor at its 50-permille
default.

## Independent fuzz campaign

Claude's authoritative terminal verdict for
`20260827-2550-kissat-classification-factor-extension-fuzz` is PASS.  Jobs
1653050--1653054 completed successfully.  The campaign replayed 81/81 corpus
entries and ran 30,000/30,000 generated cases with zero correctness,
sanitizer, invariant, model, proof, or differential failure.  It independently
checked 15,979 SAT models, accepted 10,974 native LRAT proofs, and agreed with
Kissat on all 26,953 terminating verdict cases; 3,047 bounded nonterminations
carry no oracle claim.

Direct reach covered Factor remainder materialization/caching in 32 cases,
cached-remainder reuse in 29, epoch writes in 32, and direct interned-remainder
matches in 30.  Twenty-seven cases performed at least two single-variable
extension reserves and reached spare amortized capacity plus incremental
compact-import order/position appends.  All stated lifecycle identities held.
The random-policy leg exercised 28 rule families under arbitrary legal
schedules with periodic invariants.  The owner reported N1--N4 preserved and
zero certificate, proof, scratch, queue-reference, or tracked-file residue.
