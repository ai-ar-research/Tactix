# Factor-mark compaction repair and Rank 36 divergence revisit

## Disposition

Retain the uniform Factor literal-mark compaction repair.  It fixes a concrete
Kissat-state transport bug: Tactix compacted variable identities without
moving Factor's persistent per-polarity marks, while Kissat moves those bits as
part of each compacted variable's flags.  The repair introduces no new policy
consumer, admission predicate, formula profile, rank selector, benchmark
exception, or tuned boundary.

Do not claim a Rank 36 performance closure.  Repaired Tactix and Kissat are
behaviorally exact through the retained approximately 127k-conflict trace, but
repaired Tactix still reaches the 1,200-second solve cap.  The next owner lies
after that common frontier.  Factor's direct cost is not the wall-time owner:
Tactix spends 1.092 seconds in Factor during the timeout run, versus 0.71
seconds in Kissat's successful same-core run.

## Fresh homogeneous watermark baseline

Campaign `20260828-2200-tierd-ranks1-40-factor-watermark-fresh` ran all 80
solver arms with immutable binaries.  Under the user's explicit instruction to
treat unfinished proof checking as passed, the provisional aggregate is:

- fresh/previous Tactix geometric mean: 1.003758x SAT, 0.987593x UNSAT,
  0.996451x overall;
- fresh/previous wall total: 11,182.991 versus 11,174.714 seconds;
- fresh/Kissat geometric mean: 1.459425x SAT, 1.265117x UNSAT, 1.368544x
  overall;
- fresh/Kissat wall total: 11,182.991 versus 7,670.300 seconds.

Thus the watermark candidate is essentially neutral overall.  Its important
trade is Rank 29's old timeout to 306.942-second SAT, against Rank 36's old
365.851-second SAT to timeout and Rank 20's 317.014 to 467.965-second loss.
Rank 34 improves slightly from 1,003.155 to 992.149 seconds but remains a large
gap.

All SAT results except the Rank 36 timeout have independently checked models.
Seventeen UNSAT proof arms completed verification before cancellation; the
remaining nineteen UNSAT proof-only tails are recorded as assumed-pass, not as
independently verified.  No certificate residue remains in the campaign.

Each cell below is wall seconds / peak RSS KiB.  Timeout RSS uses the harness's
sampled process-tree peak.

| Rank | Exp | Fresh Tactix s / KiB | Previous Tactix s / KiB | Kissat s / KiB |
|---:|:---:|---:|---:|---:|
| 1 | UNSAT | 0.267 / 93752 | 0.267 / 93752 | 0.217 / 49480 |
| 2 | UNSAT | 0.217 / 93744 | 0.217 / 93744 | 0.166 / 49480 |
| 3 | UNSAT | 0.617 / 181272 | 0.617 / 181272 | 0.419 / 103524 |
| 4 | UNSAT | 7.828 / 25664 | 9.231 / 22732 | 7.728 / 13020 |
| 5 | SAT | 41.430 / 10569820 | 41.781 / 10569820 | 30.113 / 10402844 |
| 6 | SAT | 52.548 / 687260 | 52.398 / 687260 | 54.300 / 348488 |
| 7 | SAT | 64.567 / 10521940 | 64.667 / 10521940 | 48.141 / 9108968 |
| 8 | SAT | 80.339 / 12655524 | 80.291 / 12655524 | 58.610 / 11118260 |
| 9 | SAT | 98.623 / 2685660 | 98.226 / 2683312 | 78.494 / 1326324 |
| 10 | SAT | 111.147 / 17079136 | 111.447 / 17078844 | 82.247 / 15348204 |
| 11 | UNSAT | 0.267 / 106920 | 0.267 / 106920 | 0.217 / 60692 |
| 12 | UNSAT | 8.580 / 2139396 | 8.630 / 2138936 | 5.175 / 1234820 |
| 13 | SAT | 88.762 / 13637868 | 88.960 / 13637852 | 64.620 / 11928096 |
| 14 | SAT | 88.460 / 13729972 | 87.809 / 13729972 | 63.217 / 12018716 |
| 15 | SAT | 503.081 / 80352 | 521.347 / 73284 | 80.697 / 29380 |
| 16 | SAT | 128.175 / 96572 | 130.574 / 98472 | 123.364 / 62836 |
| 17 | SAT | 175.359 / 1630184 | 176.010 / 1630352 | 121.752 / 598216 |
| 18 | UNSAT | 72.337 / 3132368 | 71.989 / 3132368 | 137.176 / 2083048 |
| 19 | SAT | 432.157 / 574040 | 432.182 / 574040 | 722.746 / 177552 |
| 20 | SAT | 467.965 / 2281136 | 317.014 / 2148424 | 137.379 / 943996 |
| 21 | SAT | 163.018 / 3937420 | 162.867 / 3935964 | 105.181 / 1523196 |
| 22 | UNSAT | 386.258 / 2687012 | 385.516 / 2687012 | 191.824 / 1918620 |
| 23 | SAT | 212.394 / 105544 | 216.047 / 102604 | 205.648 / 42644 |
| 24 | UNSAT | 233.395 / 235492 | 234.450 / 235492 | 225.182 / 101304 |
| 25 | SAT | 278.345 / 3027544 | 278.871 / 3027528 | 248.207 / 888184 |
| 26 | SAT | 274.189 / 99912 | 278.252 / 100540 | 262.971 / 88108 |
| 27 | UNSAT | 439.151 / 657192 | 438.656 / 656948 | 260.534 / 298192 |
| 28 | SAT | 268.184 / 1892412 | 266.081 / 1892412 | 250.530 / 809920 |
| 29 | SAT | 306.942 / 115672 | TO / 183360 | 299.001 / 64864 |
| 30 | SAT | 403.741 / 134872 | 419.161 / 166604 | 288.679 / 93572 |
| 31 | UNSAT | 435.320 / 644224 | 436.629 / 645548 | 278.264 / 265660 |
| 32 | UNSAT | 379.277 / 660644 | 380.734 / 660644 | 291.719 / 299996 |
| 33 | UNSAT | 391.751 / 659232 | 393.253 / 659232 | 331.498 / 271380 |
| 34 | SAT | 992.149 / 131332 | 1003.155 / 131552 | 344.718 / 40672 |
| 35 | UNSAT | 415.466 / 648180 | 417.957 / 648180 | 325.949 / 269268 |
| 36 | SAT | TO / 123964 | 365.851 / 98904 | 368.487 / 49284 |
| 37 | UNSAT | 432.738 / 638212 | 433.435 / 638212 | 375.991 / 269140 |
| 38 | UNSAT | 550.304 / 81392 | 572.494 / 93768 | 366.496 / 44840 |
| 39 | UNSAT | 468.868 / 467204 | 469.424 / 467204 | 396.328 / 170528 |
| 40 | UNSAT | 528.771 / 371184 | 527.953 / 371184 | 436.315 / 160924 |

## Rank 36 implementation defect

The pre-repair trace matched Kissat's startup and conflict-16,005 Factor calls,
then first differed at conflict 27,298.  The first five popped seeds were exact;
the sixth was external literal `-1217` in Tactix and `-11` in Kissat, both with
score 48.  Tactix accepted 66 grids while Kissat accepted 68.

The source audit found that `kernel_compact_variables` remapped BVE retry state,
Backbone priority, touch epochs, import ranks, values, reasons, and heuristic
state, but not `factor_marks`.  Those marks are literal-indexed, persistent
between Factor calls, and polarity-sensitive.  Leaving them in old numbering
after a variable compaction changes exactly which tied candidates enter the
next heap.

The repair builds a `2 * new_nvars` mark image and maps each surviving positive
and negative literal through `old_to_new`.  Marks on retired variables vanish.
The separate `factor_literals_marked` counter remains cumulative, matching
Kissat and the already repaired completed-call watermark.

The focused test poses asymmetric marks on two surviving literals and one
retired literal, compacts variables, and proves that polarity and external
identity survive, the retired mark disappears, and the cumulative watermark is
unchanged.  A Debug invariant now pins `factor_marks.size() == 2 * nvars`.

## Behavioral validation

Campaign `20260828-2130-tierd-rank36-factor-mark-compaction-frontier` uses one
repaired Tactix trace plus one Kissat extension.  Exact normalized results are:

| Conflict | Tactix grids | Kissat grids | Seeds/summaries/members |
|---:|---:|---:|:---:|
| 0 | 5913 | 5913 | exact |
| 16005 | 105 | 105 | exact |
| 27298 | 68 | 68 | exact |
| 59676 | 14 | 14 | exact |
| 105874 | 2 | 2 | exact |

The semantic trajectory comparator reports no search-trajectory divergence
through the repaired Tactix trace extent, approximately 127k conflicts.  This
closes the observed Factor grid/order discrepancy.  It does not prove that all
later Factor calls or downstream state are exact; the full run's final 6,174
Tactix versus 6,172 Kissat factorings shows that a later divergence eventually
exists.

## Runtime and downstream behavior

Campaign `20260828-2140-tierd-rank36-factor-mark-compaction-runtime` ran
repaired Tactix and Kissat sequentially on node016, bound to CPU 43, without
event tracing:

| Solver | Verdict | Wall s | Peak KiB | Conflicts | Decisions | Propagations |
|:---|:---:|---:|---:|---:|---:|---:|
| repaired Tactix | UNKNOWN/cap | 1200.14 | 113696 | 17316598 | 51870347 | 3831060701 |
| Kissat | SAT/model checked | 420.86 | 59804 | 6559620 | 23281243 | 1671010827 |

At the cap Tactix has consumed at least 2.852x Kissat's wall time, 1.901x RSS,
2.640x conflicts, 2.228x decisions, and 2.293x propagations.  The model emitted
by Kissat satisfies all 111,739 clauses over 1,375 variables.  The accepted
pre-repair watermark binary also timed out at 1,200.004 seconds in the fresh
homogeneous campaign, so this diagnostic does not show a new runtime regression
from the compaction fix; it shows that the fix alone is insufficient.

## Final codebase revisit

- `factor_marks` was the unique persistent per-polarity kernel field omitted
  from variable compaction.  `eager_marks` and probing stamps are scratch and
  are correctly reset; other persistent per-variable state is already mapped.
- Every production factor-bit set remains centralized in
  `kernel_mark_factor_literal`; Factor's pop is the only direct production
  clear.  The cumulative counter and completed-call scheduler watermark keep
  their distinct lifecycles.
- Fresh Factor variables still receive ordinary activation state, reverse
  zero-score demotion in the stable heap, and Kissat-order VMTF prepending once
  per application.  Existing focused tests cover that state.
- Factor remains uniformly enabled at 50 permille.  Production BVE, forward
  subsumption, and Vivify remain at 100/100/100 permille.  Classification and
  arena-order consumers are unchanged.
- No rank, benchmark identity, literal, conflict count, formula size, density,
  or classification selector appears in the repair.  Kissat creation order
  remains the production default; the CaDiCaL order remains diagnostic only.
- The rule emits the same proof-bearing Factor rewrite and retains its complete
  N1 instance surface.  Strategy still owns only admission/effort and consumes
  rule advice after the rule boundary.  N1--N4 remain intact.

## Verification and stop state

- Full Release suite: 73/73 pass in the current build configuration.
- Focused Debug ASAN/UBSAN kernel, Factor, and scheduling tests pass with leak
  detection disabled because LeakSanitizer cannot operate under this harness's
  tracing environment.
- Rank 36 Kissat SAT model: independently checked.
- Campaign certificate residue: zero.
- Cluster: no diagnostic or benchmark jobs remain; only the user's pre-existing
  interactive allocation is active.

This is the requested stopping point.  On resume, begin after the exact
approximately 127k-conflict Rank 36 frontier and compare the next reconstruction,
propagation order, inprocessing output, clause-tier state, and decision heap.
Do not revert the compaction fix, restore CaDiCaL ordering, or add a
formula/rank-specific Factor gate.
