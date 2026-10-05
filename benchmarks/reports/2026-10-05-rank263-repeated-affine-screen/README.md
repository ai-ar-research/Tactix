# Rank263 repeated affine export screen

This report freezes the first real Rank263 run after repairing retained-page
selection across repeated certified GF(2) actions. It establishes that the
lifecycle repair works and that repeated static row export is the wrong
scaling mechanism. It is not a Rank263 recovery.

The immutable Release source was
`b6a4b36b1f507f2d6a3668a8295bafc5ab9db3f7`, which contains the lifecycle
repair as `30cb2a054`. Job `1750561` built the binary on `cpu-q`; dependent job
`1750562` ran the unchanged Rank263 CNF for 300 seconds with native DSR output,
the exact prior dose-64 profile, the `AffineConsequence`, `Propagate`, and
`SemanticVariableAdvice` families, an exact controller trace, and structural
provenance. The proof checker had an independent 1,800-second allowance but
was correctly not invoked because the solver returned `UNKNOWN` at the cap.

The lifecycle defect is gone. All 1,328 affine requests applied effectively,
with no invalid, stale, exhausted, or interrupted action. The producer ran 166
times and supplied eight fresh offers each time. The controller committed all
1,328 rows and 1,326 explicit propagation actions, then continued normal
search. This replaces the earlier boundary of two committed rows followed by
a third `InvalidRequest` before search; see the sibling
`2026-10-05-rank263-repeated-affine-boundary` report.

The successful lifecycle exposed the scaling problem:

- the rows generated 678,602 proof additions and 615,268 deletions;
- 63,334 clauses remained published and 37,748 extension variables were
  introduced;
- the solver performed 321,627,759 propagations, 1,059,914 decisions, and
  254,098 conflicts in 299,991 ms;
- propagation service calls consumed 138,148 ms, or 46.1% of the solve;
  affine actions consumed another 15,939 ms, or 5.3%;
- search itself covered 22,176 intervals and 246,317 configuration-attributed
  conflicts, but did not refute the instance.

For comparison, the pre-fix dose-64 arm ended after 122 ms with two effective
rows, 143 proof additions, 121 deletions, 22 published clauses, eight extension
variables, and 582 propagations; it had not begun search. The current run is
therefore positive lifecycle evidence but negative mechanism evidence. Making
ever more static reduced rows available at successive restarts materializes a
large auxiliary formula and spends about half the run propagating it. It does
not reproduce CryptoMiniSat's useful behavior on this instance: the reference
run solved in 7.12 seconds after building one 736-by-745 matrix and reported
only four matrix propagations and no matrix conflicts.

The next implementation should be the persistent authenticated matrix already
planned by the foreground lane. It should retain the matrix across search and
export only globally entailed clausal reasons or conflicts when the current
trail makes them useful, certifying each export through the native DSR route
before CDCL consumes it. A further quota-tuning variant was deliberately not
run: bounding static export can isolate overhead, but it does not repair the
missing Gaussian propagation mechanism.

The exact controller trace and provenance are retained under
`benchmarks/probes/scratch/cms-r263-agent/rank263-main-b6a4b36b1/results/`;
their hashes and all immutable pins are in `manifest.json`. The trace is 183 MB
and the provenance is 175 MB, so neither raw stream is duplicated in this
concise report.
