# Asynchronous proof checks and the Rank-35 warm-up witness

## Decision and current outcome

The user explicitly requested continuing while proof checks run. Plan §8.1 now
treats an expected solver verdict with pending checking as a provisional pass
for investigation, implementation and subsequent dependent measurements.
Checkers continue and are revisited at checkpoints. Pending is not verified;
final checked gates and promotion still require actual results. A rejected
proof/model interrupts affected work under §8.2. No gate was weakened.
The maintained runtime runner already publishes provisional records before
checking, and its summary merges them with final results; no runner change was
needed. This policy change uses `revise-plan` with the user's explicit approval.

Production remains `c2a95a6`, measured at 1.2787518x Kissat. The ≤1.01x target
is **not achieved**. No new semantic repair, promotion or fuzz was performed.
The [review](2026-09-07-runtime-review.md) owns retained/rejected historical
work; the [preceding report](2026-09-07-rank35-heap-localization.md) owns
5396–5405 and the validated immutable observer lineage.

## Runtime evidence used without blocking

Read-only campaign `20260907-5407-provisional-runtime-views` freezes both
80-record solver views, input authorities, per-record source hashes, times,
work and memory. All expected solver exits were present; no known final
record was invalid. At that snapshot 5383 had 79 checked records and 5395 had
76. Their full-view times, usable provisionally for investigation, are:

| View | Tactix seconds | Kissat seconds | Ratio |
|---|---:|---:|---:|
| 5383 Vivify | 8217.738 | 6764.776 | 1.214783461 |
| 5395 coupled dense-entry | 8378.399 | 6762.780 | 1.238898648 |

Subsequently **5383 completed 80/80 checked and clean**, both solvers solving
all 40 instances (22 SAT, 18 UNSAT), no timeout. Its sealed summary is now the
authority. At this checkpoint 5395 has 77/80 checked, with tasks 35, 42 and 43
still checking under job 1669252. The 5393 coupled B28+C5 gate is also complete:
66/66 checked/clean, no timeout/solve loss, candidate/production 0.922327604x.
The earlier 5389 publication gate is 66/66 checked/clean at 0.950317696x.
These do not discharge consolidation fuzz or the remaining full-view gate.

The coupled variant is not automatically better because its memory ownership
is improved. Against the earlier Vivify window, Rank 37 grows from 376.839 to
634.348 seconds (+257.509), with conflicts 5,057,588 to 11,972,299. Rank 27
falls from 499.203 to 347.514 seconds (−151.689). These cross-window deltas
locate work regressions, not replicated paired causal effect sizes. Rank 37
deserves investigation after the current bounded Rank-35 causal unit.

## Warm-up localization, not yet a repair

All probes use the immutable 5402 direct observer pins. They are stopped
proof-on diagnostics, never checked solves or acceptance timings. Each arm
has a separate SLURM task, 12 logical CPUs, 48,000 MiB, full-width submission,
one solver, no node constraint or throttle. Final prefixes, trace/pin hashes,
expected stops and task-owned scratch cleanup are checked by retained helpers.

- **5406**, job 1669559: selected reinsertion at conflict 13,413 yields the
  same complete heap hash `53eede24a1c86de1`. Root assignment/removal of
  variable 109797 at 13,414 has the same portable removal pose. Thus that
  root-retirement hypothesis is not a demonstrated defect.
- **5408**, job 1669577: last ordinary pre-Rephase decision, conflict 13,560,
  decision 17,798,663, has 308,772 unique heap variables with identical
  membership, scores, assignments and physical order; both heaps are valid.
  Both final prefixes reproduce 5405's 17,872,584 cumulative decision rows
  and hash `00720e20bd6be748`; the two narrow decision streams are identical.
- **5409** is an analysis helper, not a solver campaign. Both selected event
  streams contain 166 rows. Raw decision/level differences arise because
  Kissat increments those counters before picking, Tactix after picking;
  warm-up does not increment ordinary decisions. Dropping those contextual
  counters while retaining event/conflict/mode/full payload reveals the first
  selected portable difference at row 127 (zero-based), in warm-up backtrack
  insertions at conflict 13,563. Tactix inserts 109796 from slot 51 before
  109815/109877 at 120/121. Kissat inserts 109815/109877 at 119/120, then
  109796 at 125. Full selected payloads and hashes are retained in
  `5408/portable-event-summary.json`. There are 18 differing selected rows.
- **5410**, job 1669611: immediately after the **first warm-up decision**,
  literal 145, level 1, conflict 13,563, ordinary counter 17,798,663, both
  heaps contain 429,257 variables and are entirely identical in portable
  fields. Prefixes, decisions, selected events and boundaries reproduce 5408
  byte-for-byte per arm. Thus the intervening conflicts and initial root
  restart have not left a different entry heap at this observed boundary.

Both solvers' backtrack implementations traverse the discarded trail forward
and reinsert absent active variables. Their warm-up loops make the same
decisions but propagation can produce different implication/trail orders.
**Inference:** compare selected warm-up implications and reasons, then their
watch/assignment producers; do not change heap tie-breaking or root retirement
without a causal source-backed witness. Common entry state and selected
insertion differences do not establish every intervening heap operation or
watch list identical. No runtime gain is claimed for this localization.

## Validation, preservation and next action

The preceding frozen validation owns 77/77 production fixed tests and 76/76
tests in each of the two diagnostic builds. This continuation changes no
solver source. All new complete diagnostic arms stop normally, validate
hashes and delete scratch. One premature 5410 comparator invocation found
pending result files, not a solver/test failure; its helper now reports pending.
No new fuzz was launched or inspected. No additional data was deleted; prior
5401 cancellation cleanup remains owned by its frozen report and audit.

Living docs remain within their required budgets. Detailed evidence is kept
here and in the frozen campaign summaries; PROGRESS carries only current
authorities, checks and next actions. A new timestamped Andrew TeX/PDF report
and validation index accompany this checkpoint; earlier reports are untouched.
Push remains unapproved after the prior auto-review rejection.

Next: use existing narrow implication filters at conflict 13,563 for 109796,
109815 and 109877, align by warm-up decision/level and compare sorted reasons
and implication order. Follow the first differing producer before implementing
a general repair. Revisit 5395 checks at checkpoints, not as a prerequisite
for this investigation.
