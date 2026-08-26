# Rank-27 decision, Congruence, and Substitute consolidation

## Outcome

The cumulative production-only lazy decision-journal description,
deferred Congruence arena reclamation, and Substitute affected-owner traversal
are accepted as one source increment.  Production Release SHA-256 is
`acf9e716e78ea8bded71ee5121a975223f3510fcc72f84fab79865903aa2e013`;
the exact source/test diff SHA-256 is
`ee5351da527f1b174eba5b6b8a146b8807fc9b63d65a1c997e398b2cff31a411`.
Each repair preserves N1--N4 and the proof-rule/strategy boundary; none changes
candidate order, logical ticks, scheduling, learning, pruning, proof semantics,
or retained logical state.

## Performance and exactness

- Lazy decision text preserves the frozen 64-row Rank-27 trace and all
  175,272,633 decisions while improving one exact full run from 365.249 to
  359.799 seconds.  Its predeclared checked B+C screen passes 12/12.
- Deferred Congruence reclamation removes exactly two productive prefix
  collections.  Its exact full Rank-27 pair improves 358.115 to 356.788 seconds
  and Congruence 14.432 to 13.366 seconds.  Its checked B+C screen passes 12/12
  and improves Congruence on all six cases.
- Substitute visits retiring-literal binary owners and their mirrors in the
  former global import/sign order.  Its exact full Rank-27 pair improves
  358.157 to 354.539 seconds, Substitute 21.704 to 18.528 seconds, and RSS
  672,116 to 608,084 KiB.  Its checked B+C screen passes 12/12 and improves
  Substitute on all six cases.

Frozen combined campaign
`benchmarks/runs/20260826-1300-BC-decision-congruence-substitute-consolidation/`
owns the 28 Tier-B plus five predeclared Tier-C identities and validators.
Array/aggregate `1648120`/`1648129` passes 66/66: each arm has 17 independently
checked SAT models and 16 independently verified UNSAT proofs, no solve loss or
invalidity, zero cleanup/certificate residue, and candidate/baseline PAR-2
1446.060/1449.704 = 0.997486.  Summary SHA-256 is `b57977a…eec4`.

## Claude-owned fuzz

Claude's authoritative report classifies
`benchmarks/runs/20260826-1330-decision-congruence-substitute-consolidation-fuzz/`
terminal PASS.  Five legs complete 50,000/50,000 generated cases with zero
failures: 20,927 SAT models and 17,727 native-LRAT proofs validate
independently, and pinned Kissat agrees on all 38,654 verdict cases.  Corpus is
135/135 with zero failures; dedicated reach is 40/40 and exceeds every required
floor.  Claude reports no sanitizer finding, crash, invariant failure, rejected
model/proof, differential disagreement, or certificate/scratch residue.

Stress jobs `1648215`/`1648217` return wrapper exit 127 only after `fuzz.py`
prints and enforces their complete zero-failure summaries.  The cause is a live
`run.sh` comment-length edit shifting Bash's saved file offset while it waited
inside the long-running child.  Claude records this as a post-oracle
infrastructure artifact.  It triggers no solver repair, but future fuzz jobs
must execute an immutable harness snapshot.

## Next action

Land and freeze this source as the next accepted Tactix baseline.  Close the
immutable fuzz-harness snapshot defect as an isolated tooling increment, then
resume broad search-quality convergence against the accepted current-Kissat
comparator without reopening closed collection, Substitute, residual BVE,
Congruence-layout, or equal-score heap-tie arms absent new exact worse-side
evidence.
