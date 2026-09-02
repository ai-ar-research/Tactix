# Sweep's O(n) representative reschedule

Date: 2026-09-02

## Outcome

Commit `70e3807` replaces the per-representative linear find, erase, and
insert on the Sweep schedule vector with an O(1) reschedule front and
slot-addressed tombstones that reproduce the former consumption order
exactly. It is trajectory-preserving: identity probe `4753` is byte-identical
in proofs, models, and every non-timing counter on Ranks 22, 21, 39, 24, 9,
and 4 against the pushed `bdb0d44` pin, and Rank 22 drops from 493.6 s to
392.2 s (Rank 21 315.5 -> 311.8 s; the others within noise).

**Gates on pin `70e3807` (Release SHA `2c341010e510ac2f`).** Homogeneous
Rank 1--40 campaign `4754` (job `1660078`): 40/40 expected verdicts, every
non-timing counter identical to the accepted `4701` record on all 40 ranks,
DRAT certificates identical on every rank whose independent check had
finished at aggregation, total 9,335.1 s against 9,511.6 s on the front-end
pin `bdb0d44` and 9,597.0 s on `f3ac562` (Rank 22 -96.6 s, Rank 27
-24.7 s, Rank 21 -12.0 s; the other ranks within node noise). B28+C5
campaign `4755` (job `1660079`): 66/66 expected verdicts, paired PAR-2 ratio
0.547 against the frozen shipped pin, within the 1.10 guard. Fuzz quick
(`1660076`) and random-policy (`1660077`) legs: 10,000 cases each, 0
failures. Kissat's total is 7,675.2 s; T4-K needs 8,442.7 s.

Attribution caveat for the next step: profile `4756` retained raw perf data
for Rank 33, but dwarf unwinding resolves only a fraction of the samples
through the shared rebuild helpers (`kernel_collect_garbage`,
`kernel_rebuild_large_watches`, dense occurrence rebuilds, logical-position
sorts), so per-calling-rule attribution of that cost needs in-code
accounting (charge the helpers to the rule on the stack) rather than more
sampling.

## How it was found

The per-rule decomposition in
`benchmarks/reports/2026-09-02-frontend-unit-cost.md` put Sweep at parity in
total over Ranks 1--40 but at 189.6 s versus Kissat's 33.9 s on Rank 22 at
similar search work (63,886 versus 65,820 conflicts). Profile `4752`
(code-identical `-g` pin, dwarf call graphs) charged about 100 s of it, 70 s
self time plus 40 s of `memmove`, to `sweep_reschedule_representatives`:
for each of its 121,397 rescheduled representatives it ran `std::find`,
`erase`, and `insert` on a schedule holding up to 3.9 million entries.
Kissat's `schedule_inner` is an O(1) move on a doubly linked schedule.

## The repair

The resumable schedule keeps its tail vector and adds a LIFO reschedule
front consumed before it. A variable's live entry is addressed by a slot
(tail index, or front index with a flag), so a reschedule tombstones the
earlier copy and pushes the new one in O(1); consumption skips tombstones.
The order the round loop sees is exactly the vector's: representatives
last-in first-out at the pop end, then the tail in order, a variable's
earlier copy removed. `sweep_live` replaces the size-minus-cursor predicates
for the retained and completed schedule, and `sweep_next_scheduled` is the
read-only peek the tests use instead of indexing the vector.

## Validation

- Release 75/76 tests (doc-size pre-existing); the config-wiring table
  classifies the two new schedule fields.
- Identity probe `4753` (job `1660064`), gate invocation, six ranks: pass.
- Fuzz on pin `70e3807` (Release SHA `2c341010e510ac2f`): jobs `1660076`
  (quick) and `1660077` (random policy); gates `4754` (Rank 1--40) and
  `4755` (B28+C5): results above.
