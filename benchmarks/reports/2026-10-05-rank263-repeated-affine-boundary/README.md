# Rank263 repeated affine lifecycle boundary

This report freezes the first real-target screen of the repeated certified
GF(2) startup lifecycle.  It is diagnostic evidence, not a Rank263 recovery.

The solver source was `0fc675302fe7ef0ba975a57185ed3d292881d2d9`
(repeated-action feature `60c023eb9`).  SLURM job `1750522` built the Release
binary on `cpu-q`; dependent array job `1750523` ran startup doses 8 and 64 on
the unchanged Tier-D Rank263 input with DSR output enabled, explicit
`AffineConsequence`, `Propagate`, and `SemanticVariableAdvice`, and structural
provenance.  No per-propagation feedback trace was enabled.  A separate
10-second root-action trace was job `1750528`.

Both dose arms were byte-identical through the failure boundary.  One producer
page completed with 8 candidates and 8 offers after 1,733,041 units of producer
work.  The controller committed two rows, then its third exact
`AffineConsequence` request returned `InvalidRequest` (status 9) and the solver
reported `UNKNOWN` before search:

- action 1: `x11 xor x118 xor x120 xor x124 = 0`, sources 9 and 10; 131 proof
  additions, 113 deletions, 18 published clauses, and extent growth 8;
- one explicit propagation action: 6 units of work;
- action 2: `x208 xor x212 xor x313 = 0`, sources 133, 134, and 138; 12 proof
  additions, 8 deletions, 4 published clauses, and no extent growth;
- action 3 requested `x2 xor x3 xor x4 xor x119 = 0`, sources 0 and 1, but was
  rejected before mutation.

The aggregate counters are 3 affine requests/applications, 2 effective; 2
committed actions/rows; 143 proof additions, 121 deletions, 22 published
clauses, extent growth 8; 1 propagation request/application, 6 work; 582 total
propagations; no advice attempts, decisions, or conflicts.  Dose 8 ended after
120 ms and dose 64 after 122 ms.  Neither run produced an UNSAT verdict, so no
proof was retained and no proof-verification claim is made.

The controller trace proves that action 3 retained its original owner version,
exact source equation ordinals, native clause IDs, and clauses.  The next
engineering step is therefore a focused wide/direct/wide reproduction and an
audit of the selected-row preflight predicate.  A fix may retain this payload
only if live rederivation proves it still exact; otherwise the page must be
invalidated and rebuilt safely.

`manifest.json` binds the source, binary, input, profiles, provenance, stdout,
and controller trace.  `dose-8.profile` and `dose-64.profile` are the exact
submitted producer profiles.
