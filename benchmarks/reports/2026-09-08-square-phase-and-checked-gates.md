# Short Square phase witness and checked regression gates

## Decision-changing evidence

The complete [Square stream comparison](../runs/20260907-5471-square-bound-prefix/summary.json)
validates two clean proof-on stops at 10000 conflicts. The first **5848**
decision rows agree. At conflict **501**, decision **5849**, level **67**,
Tactix selects **-43773** and Kissat **43773**; neither is warming. Both
therefore select the same variable with opposite polarity. This is a diagnostic
split, not evidence that either choice is intrinsically stronger or incorrect.

The hashed component traces put Tactix's first post-search Congruence at 2518
conflicts. Native traces put its first switch at 1000 and rephase at 1001.
Thus later inprocessing work differences do not explain the first decision
split. Startup physical clause-count differences remain visible; matching
decisions alone does not prove identical propagation or saved-phase history.
Source inspection of the pinned Tactix `heuristics_decide_lit` and native
`kissat_decide_phase` shows saved-phase/initial fallback in this focused mode;
this motivates inspecting saved state rather than changing phase priorities.

New campaign **5482**, job **1671600**, reuses the exact 5471 binary pins and
input. It records selected-variable phase saves/picks from startup through
conflict 502, then preserves the original 10000-conflict stop. The manifest
pins the parent summary SHA and first split. Its comparator requires identical
complete per-arm decisions, prefix and component-boundary bytes before phase
interpretation. No solver or observer implementation changed. Pending results
are not inferred from queue state. Next identify the earliest differing phase
save or choice, then trace its assignment owner if necessary.

[Rank 31](../runs/20260907-5470-rank31-bound-prefix/summary.json) now has two
clean 100000-conflict stops: **all 51,491,857 decisions agree**, as do whole
prefix fields/digests. Its [portable boundary comparison](../runs/20260907-5470-rank31-bound-prefix/boundary-localization.json)
matches **276** boundaries. This rules out a decision split inside that window,
not the later full-run gap or unit-cost excess. Consume the higher-priority
extended Rank-37 and Rank-32 probes before extending this later Rank-31 loss.
Partial diagnostic certificates were hashed and deleted, not checked proofs
of complete verdicts.

## Closed B28+C5 gates

Both unchanged regression campaigns now have **66/66 checked and clean**
records, no timeouts, and no solve loss. Each arm solves B28 (14 SAT/14 UNSAT)
and C5 (3 SAT/2 UNSAT). The baseline is production Tactix, not Kissat.

| Campaign/source | B candidate/production | C5 candidate/production | Combined |
|---|---|---|---|
| [5455 / phase-budget 5450](../runs/20260907-5455-eliminate-limit-BC-gate/summary.json) | 0.843662399 | 0.988469673 | 0.935340828 |
| [5464 / bound lifecycle 5459](../runs/20260907-5464-bound-advance-BC-gate/summary.json) | 0.923445931 | 1.024374357 | 0.986955104 |

5455 candidate/production B totals are 344.901/408.814 s, C5 697.310/705.444 s.
5464 totals are B 384.232/416.085 s, C5 723.406/706.193 s. Do not conceal
5464's C5 slowdown or attribute differences between these campaign windows to
the bound repair. Their per-instance authorities retain timing regressions and
peak RSS. Both candidate C5 peak RSS values are 3,891,048 KiB versus production
4,457,100 KiB; this does not close the separate per-rank Kissat memory gap.

These are checked regression gates, not promotion. The latest full Rank 1–40
view remains **1.173817099x Kissat**, 66/80 checked at this checkpoint; the goal
is **at most 1.01x**. Watch-pair's measured local gain and newer Sweep cursor
are not included in that full view. Sweep timing 5475 is running; do not price
its incomplete pairs. Latest candidates remain isolated and unfuzzed.

## Validation and handoff

Production CTest passes **77/77 in 20.76 s**. New unit tests were considered:
there is no solver/observer/checker change; existing fixtures and unchanged
complete-stream/certificate oracles cover the evidence-only and configuration
changes. The generated runner is syntax-checked, and the boundary wrapper
executes on the actual 51-million-row validated prefix with 276 matching
boundaries. A subsequent mechanism repair requires focused transition tests.

The scoped wrap-up audit checks named non-fuzz campaigns, pin/result/trace
hashes, certificate cleanup, docs and the Andrew TeX/PDF. No manual data
deletion, source promotion, push retry or separate-checker contract change.
Claude owner session 30094 remains pending; only its terminal response is
polled. It covers 5438, not subsequent repairs through 5473. Continue proof
checks and development concurrently; actual correctness failures pause
dependent work. This turn is progress: two gates closed, two causal windows
resolved, and a discriminating phase-history follow-up submitted.
