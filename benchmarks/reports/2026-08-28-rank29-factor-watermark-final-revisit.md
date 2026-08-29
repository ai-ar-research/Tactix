# Rank 29 Factor watermark repair and final codebase revisit

## Disposition

Accept the homogeneous post-Factor candidate in
`20260828-1900-tierd-ranks1-40-post-factor-fresh` provisionally under the
explicit non-blocking proof-check assumption, then promote the uniform Factor
material-watermark repair described here.  Do not restore a formula profile,
ordering selector, benchmark exception, rank exception, or tuned conflict
boundary.

The repair closes Rank 29: accepted Tactix timed out at 1,200.004 seconds; the
repaired solver returns SAT in 363.350 seconds with an independently verified
model.  Pinned Kissat takes 298.194 seconds.  Both repaired Tactix and Kissat
perform exactly 32,532,310 decisions and 10,609,587 conflicts, so the remaining
1.218× wall gap is throughput/representation cost rather than a search
trajectory discrepancy.  Peak RSS is 115,160 KiB versus Kissat's 65,988 KiB.

## Accepted homogeneous baseline

The 40-rank candidate completed all 80 solver runs.  Relative to the preceding
accepted Tactix version, its geometric-mean ratios are 1.085182× SAT,
0.970383× UNSAT, and 1.031931× overall.  Relative to pinned Kissat they are
1.453686× SAT, 1.271973× UNSAT, and 1.368907× overall.  Proof-tail completion
was deliberately not awaited; SAT models completed their independent checks,
and UNSAT solve timings are provisional under the user's stated assumption.

Each data cell is `wall seconds / peak RSS KiB`; `TO` is the 1,200-second solve
timeout.  “Candidate” is the accepted post-Factor homogeneous baseline before
the Rank 29 watermark repair.

| Rank | Expected | Candidate s / KiB | Previous accepted s / KiB | Kissat s / KiB |
|---:|:---:|---:|---:|---:|
| 1 | UNSAT | 0.267 / 93752 | 0.267 / 97888 | 0.217 / 49480 |
| 2 | UNSAT | 0.217 / 93744 | 0.267 / 97884 | 0.166 / 49480 |
| 3 | UNSAT | 0.617 / 181272 | 0.668 / 189528 | 0.467 / 103524 |
| 4 | UNSAT | 9.231 / 22732 | 7.178 / 27152 | 7.779 / 13012 |
| 5 | SAT | 41.781 / 10569820 | 21.050 / 10530588 | 30.013 / 10401700 |
| 6 | SAT | 52.398 / 687260 | 99.778 / 1287052 | 54.451 / 350588 |
| 7 | SAT | 64.667 / 10521940 | 56.404 / 10521368 | 48.354 / 9108376 |
| 8 | SAT | 80.291 / 12655524 | 67.973 / 12655524 | 58.406 / 11117848 |
| 9 | SAT | 98.226 / 2683312 | 106.288 / 2967768 | 78.344 / 1326324 |
| 10 | SAT | 111.447 / 17078844 | 94.714 / 17079368 | 82.307 / 15348204 |
| 11 | UNSAT | 0.267 / 106920 | 0.267 / 111052 | 0.217 / 60692 |
| 12 | UNSAT | 8.630 / 2138936 | 8.731 / 2269416 | 5.175 / 1234820 |
| 13 | SAT | 88.960 / 13637852 | 74.988 / 13641344 | 64.168 / 11928096 |
| 14 | SAT | 87.809 / 13729972 | 74.187 / 13729052 | 63.620 / 12019988 |
| 15 | SAT | 521.347 / 73284 | 278.114 / 81476 | 80.697 / 28540 |
| 16 | SAT | 130.574 / 98472 | 127.922 / 95828 | 123.712 / 65076 |
| 17 | SAT | 176.010 / 1630352 | 290.288 / 1643492 | 122.451 / 598216 |
| 18 | UNSAT | 71.989 / 3132368 | 78.459 / 4182168 | 138.282 / 2082576 |
| 19 | SAT | 432.182 / 574040 | TO / - | 718.013 / 183132 |
| 20 | SAT | 317.014 / 2148424 | 315.499 / 3056440 | 139.634 / 943996 |
| 21 | SAT | 162.867 / 3935964 | 160.224 / 4356548 | 104.087 / 1436436 |
| 22 | UNSAT | 385.516 / 2687012 | 357.553 / 2949212 | 193.737 / 1919296 |
| 23 | SAT | 216.047 / 102604 | 210.451 / 102996 | 205.797 / 42368 |
| 24 | UNSAT | 234.450 / 235492 | 245.509 / 234952 | 224.105 / 101304 |
| 25 | SAT | 278.871 / 3027528 | 240.298 / 2578184 | 248.796 / 888728 |
| 26 | SAT | 278.252 / 100540 | 423.896 / 159040 | 263.526 / 87808 |
| 27 | UNSAT | 438.656 / 656948 | 405.055 / 635088 | 261.833 / 298192 |
| 28 | SAT | 266.081 / 1892412 | 591.285 / 2909524 | 251.783 / 809936 |
| 29 | SAT | TO / - | TO / - | 298.194 / 65988 |
| 30 | SAT | 419.161 / 166604 | 399.964 / 169772 | 286.827 / 93572 |
| 31 | UNSAT | 436.629 / 645548 | 434.999 / 649516 | 278.049 / 265660 |
| 32 | UNSAT | 380.734 / 660644 | 694.284 / 685892 | 291.071 / 299644 |
| 33 | UNSAT | 393.253 / 659232 | 477.103 / 684900 | 332.298 / 265240 |
| 34 | SAT | 1003.155 / 131552 | 345.507 / 112056 | 344.520 / 42796 |
| 35 | UNSAT | 417.957 / 648180 | 436.994 / 649808 | 323.372 / 269268 |
| 36 | SAT | 365.851 / 98904 | 59.659 / 67152 | 367.520 / 50164 |
| 37 | UNSAT | 433.435 / 638212 | 368.210 / 672112 | 377.930 / 269140 |
| 38 | UNSAT | 572.494 / 93768 | 533.252 / 105296 | 366.381 / 44104 |
| 39 | UNSAT | 469.424 / 467204 | 430.095 / 472460 | 398.405 / 170528 |
| 40 | UNSAT | 527.953 / 371184 | 530.006 / 398528 | 433.159 / 187552 |

## Rank 29 localization

The bounded diagnostics form one causal chain:

1. The portable prefix remained exact through the retained conflict-2315
   frontier.  The trajectory trace then located the first decision split at
   conflict 2969 / decision 4636: Tactix chose `-336`, Kissat `-384`.
2. Formula and heap hashes were still exact at conflict 2969, but Tactix had
   1,541 active variables and a trail of 524 versus Kissat's 1,539 and 522.
   After common decision `-22`, Tactix correctly propagated `-287` for its own
   state; the watch transaction therefore excluded propagation as the owner.
3. Both solvers accepted the same 1,139 initial Factor grids in the same order.
   Tactix alone invoked Factor again at conflict 2969 and accepted ten more
   grids.  Kissat did not make that second call.
4. The old gate scanned current `factor_marks` bits.  Factor's own divider and
   quotient additions left bits armed, so Tactix treated its own output as new
   material.  Pinned Kissat instead compares cumulative
   `statistics.literals_factor` with `limits.factor.marked` and advances the
   latter only after a completed call, after the call's own additions.

The implementation now centralizes every production factor-bit set in
`kernel_mark_factor_literal`, increments the cumulative counter only on a
zero-to-one transition, reports whether Factor drained its schedule, and lets
the policy advance its watermark only after such a completed call.  A
budget-interrupted call remains eligible.  On the repaired bounded replay,
Tactix has exactly the 1,139 initial grids, no conflict-2969 Factor call, exact
formula/heap state, and the same `-22,-220,-384` decisions as Kissat.

## Final codebase revisit

- All production factor-bit writes are centralized; the only direct production
  assignment is Factor's intentional bit clear when it pops a seed.  Tests may
  still pose raw diagnostic state.
- The cumulative counter is written only by clause/material lifecycle code;
  `factor_completed` is written by Factor; the Default inprocessing scheduler
  is their only policy consumer.  The configuration-wiring audit classifies
  both rule-produced fields.
- No removed profile (`Dose5`, `Factor5`, `NoBve`, `NoBveLight`, or
  `StartupFactorOnly`) exists in production.  No formula, size, density,
  benchmark, rank, identity, or conflict selector was added.
- Kissat creation order remains the production default.  CaDiCaL arena order
  remains reachable only through the explicit `TACTIX_ARENA_ORDER=1`
  diagnostic arm.
- Classification consumers are unchanged: `small` remains write-only and
  `bigbig` gates only ordinary binary jump-reason assignment.  Production
  BVE/Factor/forward-subsumption/Vivify efforts remain invariant at
  100/50/100/100 permille.
- Factor's proof-bearing rewrite and complete N1 instance surface remain in the
  rule.  The strategy owns only call admission and the completed-call
  watermark; no rule gained access to `H`.  N1--N4 and the proof-rule/strategy
  boundary are preserved.

The remaining Rank 29 gap is unusually clean: logical search counts are exact,
while Tactix is 65.156 seconds slower and uses 49,172 KiB more peak RSS.  If
work resumes, profile common-state throughput and retained representation; do
not change Factor admission or search policy.  Rank 15 and Rank 34 remain the
largest SAT search-quality gaps in the accepted 40-rank baseline and should be
localized from common state under the same no-selector rule.

## Verification and stop state

- Release: 76/76 tests pass, including micro-proofs, classification/default
  strategy checks, N1 boundaries, scheduling parity, and solve reset.
- Focused Debug ASAN/UBSAN: scheduling and Factor pass with leak detection
  disabled for the harness.
- Rank 29: SAT/model verified, no proof artifact or scratch residue.
- No additional homogeneous candidate was launched after the accepted 40-rank
  run; Rank 29 used one fixed diagnostic witness only to validate the general
  mechanism repair.

This is the requested stopping point.  Resume from general common-state
throughput/representation profiling, not from a formula-dependent strategy.
