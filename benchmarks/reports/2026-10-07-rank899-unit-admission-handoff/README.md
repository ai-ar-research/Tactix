# Rank899 certified-unit handoff

This handoff stops new experiments at the user-requested natural boundary.  It
does not qualify a new candidate or claim a Rank899 speedup.

## Stable evidence

- Original `mp1-Nb7T42` input SHA256 is
  `fa316d083ef8735356f6976e7c442249f0e625b4e664803fb22ae809623929ef`.
  The actual Satsuma residual is
  `benchmarks/runs/20261007-rank899-actual-handoff-boundary-r3/pins/residual.cnf`,
  SHA256
  `792bb4abf3ba5423a32c6777307d0a6a560fea20f767712a9208aa0b85906a74`.
  Satsuma adds the certified positive unit `40310` and otherwise preserves the
  input clause order.
- Final pre-change source archive
  `benchmarks/runs/20261007-firstopen-original-seed-source-r2/source.tar`
  has SHA256
  `ab70cbb55e8d9ea06e2e16a4254ccefd729ad66d9edf3a4cb7a63b428ec7476e`.
  Its Release solver SHA256 is
  `b26efc0fb133787a797820d40c759ae552e2cf4eaabde4bee990f27947890bcf`.
  Fresh full Debug is green 353/353 at commit `06b63069b`.
- Bounded trace job **1755460** used that source with only startup
  FastEliminate disabled, the exact residual above, and bundled Kissat 4.0.4
  with `--fastel=0`.  On one CPU, all 12,723 decisions, 50 analyses and 50
  learned clauses match exactly through conflict 50.  It compares no
  implications or preprocessing snapshot and establishes no runtime result.
  The portable comparison SHA256 is
  `a07f433365d6bdd573f7b16a8271336f59a98e57a77512dfab1d2b43f451efec`.
  Full evidence and limitations are committed in
  `benchmarks/reports/2026-10-07-rank899-trajectory-attribution/` at
  `5c6e653f7`.
- Historical MAB solves explain the old 173-second rows, but MAB is retired
  after failing all four regression tranches.  They authorize neither a
  controller restoration nor suppression of a valid symmetry unit.

## Required source change and current state

The user requires every valid, fully proof-checked symmetry-breaking unit to
publish.  The current uncommitted owner patch removes singleton suppression in
both generator fixing and FixAndCut while retaining compound admission only for
a lone nonforcing binary cut.  Its owned source/test paths are
`src/rules/substitution_redundancy/{substitution_redundancy.hpp,symmetry.cpp}`
and `tests/test_online_inprocess.cpp`; the direct agent owns the adjacent plan,
progress and implementation-note edits.

Focused job **1755498** exposed a test-only status assumption.  Successor
**1755527** still has Debug exit 134 at the new synthetic singleton-nonforcing
control: truncating an automatically discovered multirow chain to one fixing
does not guarantee that its only emitted row is one nonforcing cut.  The
production unit-admission paths are not implicated by that assertion.  ASan
had no terminal exit file when this handoff was written.  Release **1755536**
and full Debug **1755537** depend on successful completion of 1755527 and must
not be used as candidate evidence.  Their frozen source archive
`benchmarks/runs/20261007-symmetry-units-clean-source-r3/source.tar` has SHA256
`615fc5f498e2649e4ef93d6f7340a0e2939d7f7e21f194718a9ae7f38599e1a5`;
it records the failed test state, not a qualified pin.

Repair the focused control without weakening source semantics.  A stable
fixture may provide one explicit same-sign swap generator on an invariant
formula so FixAndCut has exactly one nonforcing orbit cut.  Existing tests must
continue to show that `RequireForcingMember` declines complete nonforcing
batches, compound mode admits multirow nonforcing prefixes, and both public and
Default-configured compound mode expose a singleton forcing unit.  The focused
DSR fixture must finish with independent no-option dsr-trim/Trestle
every-lemma verification and retain its invalid-unit negative control.

## Next bounded Rank899 gate

After the repaired source is committed, source-archived, built in Release on
SLURM and passes the focused proof gate:

1. Reuse the frozen 1754883 boundary extractor on the original CNF with normal
   structured startup.  Require root unit `40310`, exact canonical live-clause
   equality with the actual Satsuma residual, matching live presentation and
   activation queue, and no additional formula difference.  Check the actual
   default behavior rather than infer it from the source: structured root must
   grow the trail, `fastel_root_suppressed` must increment, and startup
   FastEliminate rounds must remain zero.  The governing source guard is
   `default_should_run_fast_elimination_after_structured`, which returns false
   after structured trail growth.  A fresh-residual control needs explicit
   FastEliminate-off because an input unit predates that structured snapshot.
2. Only if that boundary and the independent proof fixture pass, compare the
   unchanged b26 Release and the repaired candidate sequentially on the same
   allocated CPU for at most 300 seconds each.  Use the original compressed
   CNF, `--auto-proof --default-graph-labeling=on`, identical limits and bounded
   disposable scratch.  Accept SAT only on exit 10 plus independent full-CNF
   model verification.  UNKNOWN, file cap, timeout or storage failure is
   incomplete.
3. Report the certified-unit effect separately from the historical 107.102s
   Satsuma+Kissat result.  Do not add another search policy, benchmark dispatch,
   unit-withholding exception, MAB, noL or X17.  The exact 50-conflict match
   supplies no evidence for such a change.

Run fresh quota admission before the causal pair and keep user storage below
700 GB.  No new Rank899 causal job was launched after the wrap-up request.
