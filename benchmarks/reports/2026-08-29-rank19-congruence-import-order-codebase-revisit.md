# Rank 19 closure, homogeneous Rank 1--40 result, and codebase revisit

Final promotion authority is the sanitizer-repaired successor campaign
`20260829-1853-tierd-ranks1-40-watch-lifecycle` and report
`2026-08-29-watch-lifecycle-final-validation.md`. The table below preserves the
immediate pre-sanitizer 40/40 candidate that localized the lifecycle delta.

## Outcome

The uniform production candidate solves all 40 authoritative Tier-D ranks.
Rank 19 changes from the previous accepted timeout to model-verified SAT; the
homogeneous reproduction takes 775.077 seconds versus frozen Kissat's 729.272
seconds. Rank 36 remains solved at 387.570 seconds. No formula, density, size,
classification, benchmark, rank, literal, conflict, or identity selector was
added.

Candidate PAR-2 is 11,277.844 seconds, versus 12,402.620 for the previous
accepted Tactix (0.909x, 9.1% lower) and 7,654.941 for Kissat (1.473x). On the
39 instances both Tactix versions solve, candidate/previous geometric mean is
1.026x; candidate/Kissat over all 40 is 1.446x. The repair is accepted because
it is a source- and common-state-proven general reference correction, restores
complete coverage, and improves PAR-2. Regressions on Ranks 31, 37, and 40 are
downstream mechanism leads, not grounds for restoring external variable order
or introducing a selector.

## General repairs

1. Vivify, root flush, and ordinary Substitute now overwrite every surviving
   large image inside its source allocation, as pinned Kissat does. This
   preserves Ref, allocation span, header state (including Vivify priority and
   recency), dense-occurrence pose, and pre-collection watch state. Unit and
   binary collapses keep their existing retirement path.
2. Congruence Y1 binary-resolution scans `kernel_vars_in_import_order`.
   Kissat's `all_variables` walks compact internal indices allocated on first
   import; raw stable Tactix `Var` order is external-number order and was not
   equivalent.
3. Focused regressions pin root-flush allocation/header retention, ordinary
   Substitute priority retention, Vivify Ref/span retention, and a Y1 case
   where external-number and import order deliberately disagree.

Campaign `20260829-1300-tierd-rank19-next-decision` records zero physical heap
mismatches across all 122,454 positions at conflict 3139 and all 125,148
positions at conflict 3140 after these repairs. The portable trajectory remains
exact through conflict 5480; adjacent swaps at 5481 and 5497 commute, and the
first non-commuting downstream split is conflict 5502. The focused corrected
Rank-19 run independently checks all 1,297,025 clauses against the SAT model in
831.538 seconds. The homogeneous reproduction is the promotion authority.

## Runtime and memory by rank

Times are solver wall seconds. Memory is sampled peak process-tree MiB.
Previous Tactix comes from
`20260829-1005-tierd-ranks1-40-logical-rewatch`; Kissat comes from frozen
`20260828-1125-tierd-ranks1-40-walk-eliminate-anchor`. Neither comparator was
rerun. C/P and C/K are candidate runtime ratios; C/P is omitted where the
previous candidate timed out.

| Rank | Exp | Candidate | sec | MiB | Previous | sec | MiB | Kissat sec | MiB | C/P | C/K |
|---:|:---:|:---:|---:|---:|:---:|---:|---:|---:|---:|---:|---:|
| 1 | UNSAT | UNSAT | 0.267 | 91.9 | UNSAT | 0.267 | 91.9 | 0.166 | 51.1 | 1.00x | 1.61x |
| 2 | UNSAT | UNSAT | 0.267 | 91.9 | UNSAT | 0.267 | 91.9 | 0.166 | 47.2 | 1.00x | 1.61x |
| 3 | UNSAT | UNSAT | 0.617 | 175.0 | UNSAT | 0.619 | 174.9 | 0.417 | 103.5 | 1.00x | 1.48x |
| 4 | UNSAT | UNSAT | 17.393 | 37.9 | UNSAT | 16.642 | 36.7 | 7.728 | 15.3 | 1.05x | 2.25x |
| 5 | SAT | SAT | 41.430 | 10286.4 | SAT | 41.129 | 10302.1 | 29.762 | 10161.9 | 1.01x | 1.39x |
| 6 | SAT | SAT | 77.188 | 640.6 | SAT | 52.698 | 640.7 | 54.301 | 336.8 | 1.46x | 1.42x |
| 7 | SAT | SAT | 65.868 | 10274.1 | SAT | 65.218 | 10275.5 | 47.889 | 8858.8 | 1.01x | 1.38x |
| 8 | SAT | SAT | 81.293 | 12356.6 | SAT | 80.439 | 12359.3 | 58.206 | 10836.5 | 1.01x | 1.40x |
| 9 | SAT | SAT | 110.184 | 2616.9 | SAT | 99.320 | 2622.3 | 77.685 | 1297.9 | 1.11x | 1.42x |
| 10 | SAT | SAT | 113.843 | 16680.7 | SAT | 111.491 | 16681.5 | 81.692 | 14989.5 | 1.02x | 1.39x |
| 11 | UNSAT | UNSAT | 0.267 | 102.8 | UNSAT | 0.267 | 102.8 | 0.216 | 61.8 | 1.00x | 1.24x |
| 12 | UNSAT | UNSAT | 8.530 | 1865.7 | UNSAT | 8.530 | 1969.7 | 5.224 | 1209.6 | 1.00x | 1.63x |
| 13 | SAT | SAT | 90.757 | 13322.5 | SAT | 89.205 | 13323.1 | 64.319 | 11630.9 | 1.02x | 1.41x |
| 14 | SAT | SAT | 88.457 | 13410.7 | SAT | 87.653 | 13411.3 | 63.064 | 11726.7 | 1.01x | 1.40x |
| 15 | SAT | SAT | 515.277 | 73.5 | SAT | 501.342 | 73.1 | 80.694 | 31.6 | 1.03x | 6.39x |
| 16 | SAT | SAT | 129.317 | 102.7 | SAT | 129.915 | 88.1 | 123.261 | 66.1 | 1.00x | 1.05x |
| 17 | SAT | SAT | 174.301 | 1546.4 | SAT | 259.336 | 1568.0 | 121.354 | 586.6 | 0.67x | 1.44x |
| 18 | UNSAT | UNSAT | 63.370 | 2969.4 | UNSAT | 71.631 | 2923.4 | 138.087 | 2036.6 | 0.88x | 0.46x |
| 19 | SAT | SAT | 775.077 | 533.8 | UNKNOWN | 1200.005 | 584.9 | 729.272 | 180.0 | -- | 1.06x |
| 20 | SAT | SAT | 342.212 | 2108.8 | SAT | 339.273 | 2097.7 | 139.236 | 918.5 | 1.01x | 2.46x |
| 21 | SAT | SAT | 141.037 | 4042.1 | SAT | 172.382 | 3815.5 | 104.431 | 1405.4 | 0.82x | 1.35x |
| 22 | UNSAT | UNSAT | 363.416 | 2626.9 | UNSAT | 368.130 | 2626.9 | 192.829 | 1846.6 | 0.99x | 1.88x |
| 23 | SAT | SAT | 216.317 | 101.1 | SAT | 211.465 | 99.1 | 204.788 | 44.2 | 1.02x | 1.06x |
| 24 | UNSAT | UNSAT | 233.718 | 235.0 | UNSAT | 241.831 | 231.4 | 224.126 | 101.7 | 0.97x | 1.04x |
| 25 | SAT | SAT | 272.231 | 2937.1 | SAT | 275.158 | 2938.9 | 249.066 | 858.6 | 0.99x | 1.09x |
| 26 | SAT | SAT | 276.035 | 101.1 | SAT | 273.216 | 109.1 | 263.483 | 86.3 | 1.01x | 1.05x |
| 27 | UNSAT | UNSAT | 376.204 | 615.3 | UNSAT | 369.287 | 633.6 | 260.531 | 293.8 | 1.02x | 1.44x |
| 28 | SAT | SAT | 267.785 | 1842.3 | SAT | 267.086 | 1849.5 | 249.759 | 785.0 | 1.00x | 1.07x |
| 29 | SAT | SAT | 318.760 | 111.1 | SAT | 306.062 | 107.5 | 296.822 | 67.1 | 1.04x | 1.07x |
| 30 | SAT | SAT | 416.558 | 164.2 | SAT | 406.295 | 133.2 | 286.103 | 94.0 | 1.03x | 1.46x |
| 31 | UNSAT | UNSAT | 787.400 | 642.6 | UNSAT | 414.015 | 652.0 | 278.492 | 262.1 | 1.90x | 2.83x |
| 32 | UNSAT | UNSAT | 607.651 | 615.5 | UNSAT | 740.935 | 635.1 | 289.177 | 295.6 | 0.82x | 2.10x |
| 33 | UNSAT | UNSAT | 516.859 | 636.6 | UNSAT | 426.086 | 738.6 | 332.153 | 261.7 | 1.21x | 1.56x |
| 34 | SAT | SAT | 503.609 | 116.9 | SAT | 631.095 | 121.7 | 342.186 | 44.4 | 0.80x | 1.47x |
| 35 | UNSAT | UNSAT | 462.042 | 633.5 | UNSAT | 492.397 | 623.7 | 322.010 | 265.6 | 0.94x | 1.43x |
| 36 | SAT | SAT | 387.570 | 85.3 | SAT | 385.568 | 80.9 | 368.940 | 51.6 | 1.01x | 1.05x |
| 37 | UNSAT | UNSAT | 658.395 | 628.4 | UNSAT | 454.021 | 619.2 | 374.860 | 265.5 | 1.45x | 1.76x |
| 38 | UNSAT | UNSAT | 793.045 | 123.5 | UNSAT | 745.131 | 107.4 | 364.718 | 45.8 | 1.06x | 2.17x |
| 39 | UNSAT | UNSAT | 472.604 | 393.4 | UNSAT | 472.746 | 427.3 | 397.003 | 169.3 | 1.00x | 1.19x |
| 40 | UNSAT | UNSAT | 510.693 | 368.1 | UNSAT | 394.472 | 365.1 | 430.725 | 153.1 | 1.29x | 1.19x |

## Codebase revisit

- Removed the completed `SAT_EVENT_TRACE_SUBSTITUTE_UNITS` and
  `TACTIX_EVENT_TRACE_ROOT_OWNER` arms, their Tactix call sites, Kissat overlay
  injections, declarations, and static checks. Their negative/owner evidence is
  preserved in the immutable causal campaign; they no longer enlarge the
  maintained diagnostic surface.
- Retained only compact additions to the existing Vivify detailed trace that
  expose Ref, arena position, priority, id, and full live-clause hashes. Those
  fields remain useful for the open common-state/representation comparison and
  add no production selector or non-trace work.
- Removed dead Vivify logical-position inheritance after large terminals became
  genuinely in-place, corrected lifecycle comments, and kept deferred
  reclamation only for unit/binary/deletion terminals.
- Updated the rule-policy boundary checker so the in-place primitive has an
  explicit proof-owning allowlist: kernel, Strengthen, Substitute, and Vivify.
- Withdrew the old strategy-survey recommendation for content-profile and
  LymphoSAT-style preset selection. Experimental mechanisms may be diagnostic,
  but an accepted mechanism must become one uniform default. The parity ledger
  now owns separate closed rows for surviving-large in-place state and
  Congruence Y1 import order.
- Kept implementation notes within their 560-line budget by leaving only
  stable invariants there and putting chronology/data in this report.

## Verification and remaining leads

The post-cleanup focused release set passes 10/10: kernel, strategy config,
Kissat overlay static checks, certificate cleanup, Vivify/N1, Substitute,
Congruence, default-strategy configuration, and rule-policy boundaries. The
first full release pass had 75/76 functional/architectural tests pass and found
only the implementation-notes line-budget excess. The notes were compacted
without raising the budget; the repeated full release suite passes 76/76.

The subsequent ASAN/Debug revisit found one lifecycle omission hidden by
Release assertions: Vivify disconnects the complete large-watch population
before root simplification, but the new in-place primitive initially tried to
unwatch and reconnect each shortened clause immediately. Tactix now carries an
explicit `large_watches_connected` image of Kissat's `watching` state, keeps
binary watches live while large watches are disconnected, and reconnects the
whole large population before any root-unit propagation. A direct kernel test
pins the disconnected rewrite/rebuild transition. The affected sanitizer set
passes 7/7 and the repeated full Release suite passes 76/76. This is a general
mechanism correction, not a formula/rank arm. Its exact binary subsequently
solved 40/40 with PAR-2 11,215.788 seconds, 0.55% below this report's immediate
pre-sanitizer candidate; the successor report owns the final table and verdict.

At this report checkpoint, 33/40 final result files are independently valid and
the remaining UNSAT proof checks for Ranks 22, 27, 31, 32, 33, 35, and 37
continue under Slurm array `1656005`. Solver timing and cleanup do not wait
for proof-check wall time, but the jobs are not canceled and the final manifest
will record every checker outcome and scratch cleanup state.

The largest current runtime leads versus Kissat are Rank 15 (6.39x), Rank 31
(2.83x), Rank 20 (2.46x), Rank 38 (2.17x), and Rank 32 (2.10x). Ranks 31, 37,
and 40 are also the largest regressions versus previous Tactix. Future work
should compare the earliest common reconstruction, propagation, inprocessing
output, clause tiers, heuristic/heap state, and search trajectory on those
instances. Formula-specific profiles and a return to CaDiCaL arena order are
not valid remedies.
