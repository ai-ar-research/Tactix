# Rank569 affine admission and lifecycle audit

## Result

The persistent affine frontend did not miss the original XOR population, run
after startup elimination, require a complete-formula affine image, or exhaust
an admission budget on Rank569.  It saw the original clauses before initial
FastEliminate and admitted all 24 exact XOR equations as an authenticated
subimage.  Those equations are 24 disjoint output comparisons, so their matrix
has rank 24 but no eliminable row overlap and no root consequence.  Later
native elimination made 46 support variables unavailable; sound existential
projection then removed every disjoint row.  This explains the zero affine
yield.  It does not explain the 4 GB terminal proof as a recognition failure.

This is a negative source and evidence audit.  It adds no solver code, launches
no solver run, changes no Default option, and makes no solve, speed, proof, or
closure claim.

## Evidence binding

The exact input is
`tierD24-26/778e0fd8f034b7cfd539b1b523ed4c4a-lec_mult_CvW_12x12.sanitized.cnf.xz`,
compressed SHA-256
`dbadc0aa9040c4394a12709e838d677c584934186a9154284e70051e2ed69446`.
The static census is
[`../2026-10-07-rank569-persistent-xor-structural-negative/census.json`](../2026-10-07-rank569-persistent-xor-structural-negative/census.json),
SHA-256
`3ff9522c3a53d88af03e717eca12e2144c957590086b996e21338693410e1d2a`.

The live counter authority is commit
`727266a48cb971a6a416e721754854e03fc21966`, Release binary SHA-256
`9cd335f271e3299d6f6225fb2db29b7672045ac096a66d7c02badfe57d95e38b`,
and job `1753557` under
`benchmarks/runs/20261007-rank569-rescan-727266a48-r4-on-300s`.
Its result JSON has SHA-256
`9355540808bdb68fd57ece4fd7247c066e0c9f61367bad0a824826b896c42457`.
The source archive SHA-256 recorded by the frozen campaign is
`b1ecdd298ba373ede9ed6bb4b812e3d0d20bf23abd433b71f727913da2c9acd7`.
At that commit the relevant Git blob IDs are:

- `src/policies/default_policy.cpp`:
  `748cf77fa3e057f5d73285d3c3532f888a7f2f2f`;
- `src/rules/semantic_extension/affine_matrix_search.cpp`:
  `ce8fbbe0f173794e6b5956145b885295d7c22066`;
- `src/rules/semantic_extension/affine_root_image.cpp`:
  `25ac0504650e82a27793a9ac077daec15dea7216`.

The existing functional census is
`benchmarks/probes/20261007-rank569-functional-signatures/result.json`,
SHA-256
`91107824085dde05a504016894d2518beb58dc07a7ed0c7e571b9156999a8363`.

## What exists in the original CNF

The raw formula has 3,426 variables and 10,231 clauses: 6,756 binary
clauses, 3,474 ternary clauses, and one width-24 terminal OR.  It has no unit
clause.  Variables 25 through 3,402 are 3,378 two-input functional gates, each
encoded by three clauses.  Variables 3,403 through 3,426 are 24 two-input XOR
gates, each encoded by four clauses.  Thus the original XOR population is 24
equations authenticated by 96 physical clauses.

The equations cover 72 variables.  Every support variable occurs in exactly
one equation: the support-frequency histogram is `{1: 72}`, with no overlap.
The matrix therefore has rank 24 immediately, but XORing any pair of rows
cannot cancel a coordinate.  The equations only state
`z_i = left_i xor right_i`.  The terminal `z_0 or ... or z_23` clause is not
affine, so these 24 rows plus the OR do not imply an affine unit.

## Admission happens before FastEliminate

In the exact `727266a48` source, `DefaultPolicy::run` performs startup root
propagation, read-only structural authentication, the affine root closure, and
then persistent `initialize_affine_search` plus its initial root service around
lines 4,600--4,629.  Initial FastEliminate does not run until around lines
5,025--5,038.  With no original unit clause, the preceding propagation cannot
remove an XOR premise.  Persistent recognition therefore sees the original
clause image rather than a simplified remnant.

The two admission modes explain the apparently mixed initialization footer:

```
c affine-matrix-init 2 attempts, 1 applied, 1 incomplete-image,
  0 exhausted, 0 interrupted
```

The standalone affine-root closure asks for `CompleteSnapshot`, so rejecting a
mixed nonlinear formula as `IncompleteImage` is expected.  The persistent
owner explicitly asks for `AuthenticatedSubimage`, which accepts the 24 XOR
equations while measuring, but not requiring, complete coverage.  Its counters
confirm success:

- recognition rank gains: 24;
- admitted sources/equations: 24;
- discovered groups: 10,158;
- recognizer work: 34,284;
- initialization work: 5,109,896;
- resource declines: 0.

`admitted_sources=24` counts admitted equations in this diagnostic surface,
not physical clauses.  Their 96 clause premises are retained through premise
groups.  The separate footer
`c affine 1 calls, 24 equations, ... 1 budgets` belongs to the Lucky affine
model candidate search; it is not the persistent matrix admission result.

## Retention and projection

The retained owner survives source deletion and does not rescan the original
clauses on an elapsed-tick clock.  The 300-second run made one recognition
initialization and 56 root-only refresh/service calls, with zero search-level
services.  The same run reports four alias proof additions and 28 finally
active equations after the 24-equation admission, consistent with checked
alias maintenance extending the owner.  Premise maintenance used 1,285,832
work and the owner retained 9,291,088 bytes.

At root maintenance, variables that no longer map to a live current variable
are explicitly collected and passed to `AffineMatrix::project`.  The run
reported 46 projected variables, maintenance rank gains/losses 13/39, and
final retained rank/support 0/0.  With disjoint rows, existentially projecting
the first unavailable coordinate of a row deletes that whole row because no
second row contains the coordinate.  The premises and original equation owner
remain durable; the live Gaussian matrix soundly loses an equation that cannot
be expressed over current variables.  This is explicit elimination handling,
not re-recognition or provenance loss.

Keeping all 72 variables merely to preserve the rows is not supported by this
evidence.  The initial intact matrix already produced no event, and a retained
disjoint row needs two of its three variables fixed at root before it forces
the third.  The run published and consumed zero affine reasons or conflicts.

## Previously exhausted nearby hypotheses

The deterministic functional census evaluated all 3,402 functional gates on
16,384 sampled primary assignments.  It found 181 cross-cone signature groups
and 471 cross-cone pairs.  Exact enumeration for support at most 16 and native
proof probes reduced the nonstructural candidates to already-consumed
Substitute relations or one-to-38-conflict Boolean-easy units/equivalences.
The support-above-16 candidate list contains the higher miter-output
equalities already tried by the output subsolver, plus constant-like relations
through variable 45.  Generic signature rescanning is therefore not a new
experiment.

The output subsolver independently proved outputs 3,403 through 3,410, but
fresh 100-million-tick arms for 3,411 and above exhausted.  A retained-learning
prototype reached two more low outputs before stalling and its ten-unit causal
gate did not improve the 300-second solve.  Direct branching on middle/high
miter outputs also increased all measured search-work counters.  These results
rule out more XOR rescanning, low-output publication, and target-output
branching as the next Rank569 increment.

## Narrow nonlinear cut hypothesis

The smallest untried proof-producing diagnostic is bounded case decomposition
of the first hard output, not another theory consequence.  Output 3,411 compares
two functions whose combined primary support is exactly
`{1..9, 13..21}`.  The preceding independently proved output 3,410 has support
`{1..8, 13..20}`.  Split the assumption `3411` on the newly introduced primary
pair 9 and 21.  Four complete subsolver refutations would derive the checked
clauses
`~3411 or ~cube`; ordinary resolution of those four clauses derives the single
unit `~3411`.  Failed, SAT, exhausted, interrupted, or stale cases publish
nothing.  The same construction remains root-only and needs no conditional
search consequence.

This is falsifiable with one bounded diagnostic: cap each of the four cases,
retain learned state only under its exact assumptions, export every complete
case through the existing proof path, independently check the combined unit
derivation, and compare total ticks and proof bytes with the recorded unsplit
100-million-tick failure.  Stop if any case fails its cap, if the combined
proof is not independently accepted, or if total work/proof growth offers no
credible route to the remaining outputs.  A success on 3,411 would justify
testing the analogous `(10,22)` cut for 3,412; it would still not justify
Default enablement or a full Rank569 run by itself.

This hypothesis is a general bounded Boolean case-splitting mechanism for
combinational miters.  Its limitation is explicit: higher outputs introduce
more primary pairs, so recursive cubes can grow exponentially.  The first hard
output is the cheap gate for whether the structural support boundary offsets
that cost.  No claim is made that it will.
