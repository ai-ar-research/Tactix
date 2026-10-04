.po 1i
.ll 6.5i
.ps 16
.ft B
.ce
Restored LymphoSAT SAT constructors: terminal regression result
.ps 11
.ft R
.ce
Tactix development checkpoint
.ce
October 4, 2026
.sp 2
.ti 0.25i
Frozen checkpoint 156afef6b is not accepted. B28+C5 passes, but Rank1--50
exceeds its aggregate solver-time ceiling and WV100/WV2 lose seven retained
SAT solves. All 272 rows are terminal. There is no wrong verdict, rejected
model, rejected proof, or missing result.
.sp
.nf
Set         Gate   Solver seconds         Checked  Censored  Timeouts  Lost
B28+C5      pass     998.177 / 999.876       32        1         0       0
Rank1--50   fail   9,530.106 / 8,521.522     42        8         0       0
WV100       fail   2,646.841                  93        2         5       5
WV2--89     fail   3,781.591                  75        2        12       2
Total       fail                              242       13        17       7
.fi
.sp
.ti 0.25i
Solver limits are 1,200 seconds except WV2's 600 seconds. Proof checking is
bounded at 1,800 seconds. Twelve censored checks satisfy the agreed cost bound;
the WV100 ncc row is inconclusive because its baseline was already censored.
Censoring is not independent proof verification, but it is not checker failure.
.sp 2
.ft B
Diagnosis
.ft R
.sp
.ti 0.25i
The frozen DefaultPolicy omitted historical affine constructor instance 8.
All seven lost retained solves and the three largest Rank regressions have one
baseline affine call and no candidate affine call. Six MDP rows were solved
directly by the old affine constructor. Rank cabp-V-nos6, WV100 Break_20_72,
and the WV2 timetable entered affine without producing a model, so their
association is trajectory evidence rather than proof of a direct constructor
solution.
.sp
.ti 0.25i
Mutable commit 60446aa0d restores instance 8 after exact-one admission and
before structure-changing preprocessing. Its policy-path test requires the
productive case to solve with zero decisions and conflicts. That repair
postdates the frozen binary and does not change this campaign's negative
verdict.
.sp 2
.ft B
Next check
.ft R
.sp
.ti 0.25i
Use the repaired source first on the six directly solved MDP rows and the three
trajectory-sensitive cabp, Break, and timetable rows. Do not rerun the
historical comparison or all four gates until this subset is recovered. The
parallel model-guidance work keeps Walk and CDCL as separate actions over the
same exact candidate and records availability, installation, use, overwrite,
improvement, and export without assigning a reward.
.sp
.ti 0.25i
The technical report is benchmarks/reports/
2026-10-04-restored-lymphosat-sat-regression-terminal.md. Raw evidence remains
under the four benchmarks/runs/20261004-restored-lymphosat-sat-* directories.
