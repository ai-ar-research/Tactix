# Factor fresh-activation lifecycle and codebase revisit

Date: 2026-08-28

## Outcome

Tactix now reproduces Kissat's complete heuristic lifecycle for variables
created by Factor.  A fresh variable is first activated with the ordinary
cumulative EVSIDS score `1 - 1 / variables_activated`, inserted into the
stable heap when applicable, and only then demoted to score zero in reverse
creation order.  Its VMTF demotion remains the existing forward walk and
front insertion.  The event consumer uses the recorded `[first,end)` range.

This is a uniform mechanism repair.  It adds no classification consumer and
no formula-size, density, benchmark, rank, conflict, or identity predicate.
Factor remains enabled at 50 permille.  N1--N4 and the proof-rule/strategy
boundary are unchanged.

## Reference audit

The relevant pinned Kissat sequence is split across two files:

1. `flags.c::activate_literal` increments `variables_activated`, computes
   `1 - 1 / variables_activated`, updates the score heap, and pushes the
   unassigned variable when stable.
2. `factor.c::adjust_scores_and_phases_of_fresh_variables` walks Factor's
   fresh stack backwards and updates every score to zero before performing
   the queue rewrite.

Tactix previously skipped the first half and allocated Factor extensions at
zero.  The final score vector therefore looked equivalent, but the missing
up/down sift changed the physical heap topology among later equal-score
variables.  No additional consumer was needed: Factor already publishes one
`FreshVars` event and DefaultPolicy already consumes it immediately after the
rule application.

## Implementation and cleanup

- `Heuristics::activation_count` carries the cumulative activation rank.
- `heuristics_reserve_vars` gives every newly attached variable its ordinary
  activation score before stable-heap insertion.
- `heuristics_seed_activation_order` resets the provisional allocation image,
  seeds only variables actually activated by parsing, and initializes the
  cumulative rank from that retained order.
- `heuristics_demote_fresh_vars` zeroes the exact recorded fresh range in
  reverse order before rewriting VMTF state.
- The revisit removed the latent over-broad range assumption: the consumer
  now honors `FreshVars.end` instead of extending through current `nvars`.
- Tests whose purpose requires an artificial all-zero starting heap now state
  that precondition explicitly instead of relying on allocation side effects.

## Exact Rank-15 evidence

The pre-repair physical heap first differed during the common Probe round at
conflict 493,616 and was already unequal by conflict 494,000.  With the repair,
both the physical heap hash and the heap-plus-score hash match Kissat at the
former boundary and at every retained checkpoint:

`494000, 500000, 600000, 750000, 900000, 1000000, 1100000, 1200000,
1300000, 1400000, 1500000, 1550000, 1560000, 1565000, 1566500, 1566540,
1566544`.

At conflict 1,566,544 the complete conflict clause, score-bump sequence,
62-variable reinsertion sequence, physical heap, and score image are exact.
The later decision sequence still differs: Tactix chooses `-17,-30`, while
Kissat chooses `-30,-17`; external variable 17 consequently appears at level
9 rather than 10 in conflict 1,566,547.  The swap happens while selecting from
the common heap after ordinary assignments, so the next owner is assigned
state/reconstruction or the input to lazy heap removal, not Factor score
production.  Do not reopen formula profiles or add an ordering selector.

Retained comparison traces are under
`benchmarks/runs/20260828-2050-tierd-rank15-next-divergence/`.  The focused
five-variable regression pins the exact post-demotion heap
`{3,2,1,0,4}`.

## Latest homogeneous timing context

The preceding committed candidate `a8b03e5` has all 40 solver results and
39/40 completed independent checks in
`benchmarks/runs/20260828-1725-tierd-ranks1-40-bve-dense-compact/`.
Provisional geometric-mean ratios are:

| Population | Candidate / accepted Tactix | Candidate / Kissat |
|---|---:|---:|
| 22 SAT ranks | 1.0775x | 1.4436x |
| 18 UNSAT ranks | 1.0190x | 1.3649x |
| all 40 ranks | 1.0507x | 1.4076x |

The exact per-rank runtime and peak-RSS join is reproducible with
`python3 benchmarks/runs/20260828-1725-tierd-ranks1-40-bve-dense-compact/provisional.py`.
Notable SAT rows are Rank 6 at 52.798/99.778/54.299 seconds
(candidate/accepted/Kissat), Rank 15 at 521.444/278.114/80.688 seconds,
Rank 16 at 131.330/127.922/122.964 seconds, Rank 20 at
390.303/315.499/140.444 seconds, and Rank 36 at
366.952/59.659/370.358 seconds.  This new activation-lifecycle repair has not
received another cluster campaign; none was submitted during this diagnostic
and review pass.

## Codebase revisit findings

- Classification remains confined to the pinned Kissat wiring: `small` is
  write-only and `bigbig` controls only ordinary binary jump-reason assignment.
- Production BVE/Factor/forward-subsumption/Vivify efforts remain invariant at
  100/50/100/100 permille.  Diagnostic arms cannot self-select.
- The fresh-variable publication remains rule output; all score, heap, phase,
  and VMTF handling remains policy-owned.  Factor does not include or inspect
  `H`.
- The cumulative activation counter intentionally survives variable
  compaction, matching Kissat's cumulative statistic.  Parse-time reseeding is
  the only reset because a new `Heuristics` session owns a new solve image.
- The exact event end is now consumed, so a future deferred or batched caller
  cannot accidentally demote unrelated later variables.
- No new production profile, formula exception, benchmark exception, or rank
  exception exists in the changed code or its adjacent call graph.

The next implementation investigation should start at the assigned-variable
set presented to stable-heap selection immediately after conflict 1,566,544.
Compare reconstruction/root assignment membership before instrumenting heap
mechanics; the heap implementation and its conflict updates are already exact
at that boundary.

## Validation

- Release: 76/76 tests pass.
- Debug focused: Analyze, SwitchMode, EventTrace, and Factor pass with leak
  detection disabled for the ptraced environment.
- ASAN/UBSAN focused: the same four tests pass.
- Micro proofs pass as part of Release.
- Rank-15 heap and score hashes match at all retained checkpoints listed above.
- No proof-check result was awaited to obtain or report solver timing.
