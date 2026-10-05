# X17 propagated-root audit

The rank401 diagnostic is blocked before discovery because the exact input has
10 explicit units.  The safe relaxation is narrower than residualizing the
formula: X17 can retain its exact-clause theory contract while admitting
unrelated level-zero assignments.

## Existing invariants

- Native domain discovery accepts an ALO candidate only when every candidate
  literal is live and unassigned.  AMO completion then requires every exact
  binary pair.  Therefore every variable in an offered domain is unassigned.
- Tuple indexing maps only the negations of values from those unassigned,
  disjoint domains.  An exact ALO, AMO, or tuple source clause over an offered
  relation consequently contains no assigned variable.
- Owner capture retains the exact external literals and native proof identity
  of every source ALO, AMO, and tuple clause.  Planning proves the exact-AND
  definition by fresh-pivot RAT and each residual exclusion by global RUP from
  those exact clauses.  These redundancy arguments do not depend on the
  current partial assignment.
- Apply introduces one fresh variable.  Every nonfresh literal in every new
  definition or consequence is an unassigned domain literal.  Only the
  already-supported impossible-prefix consequence can be a new unit; it fixes
  the fresh extension and leaves propagation explicitly pending.
- A session is bound to one Kernel.  Every retained owner is exactly recaptured
  before another offer or application.  If a later root assignment fixes a
  source-domain variable, the relevant clauses are no longer capturable and
  the owner becomes stale.  An assignment on an unrelated variable does not
  change the retained source.
- Projection reconstructs the fresh variable as the exact conjunction of its
  prefix inputs.  That identity holds for every total external model,
  independent of which unrelated literals were fixed at level zero.
- Public candidate publication occurs after the composed propagation action.
  The publisher removes root-fixed or retired variables before staging advice,
  and advice application rechecks root values and activity.  Thus an existing
  level-zero trail does not make a published variable branchable incorrectly.

## Required code change

Both X17 guards should require a consistent, conflict-free, fully propagated
root whose trail contains only level-zero assignments, matching the public
root invariant already used by X16.  They must not merely delete the current
`trail.empty()` check.

Exact source capture should continue rejecting retired literals, but should
ignore a whole clause when any literal in it is root assigned.  It must not
shorten or residualize that clause.  This is conservative and proof neutral:
because every expected source clause uses only selected unassigned domain
variables, an ignored clause cannot be an exact expected ALO, AMO, or tuple.
Keeping only literal-identical clauses preserves source ownership, native proof
IDs, RAT/RUP premises, replay, and retained-owner comparison.

No planner, proof step, session representation, projection record, semantic
candidate shape, or action payload needs to change.

## Focused acceptance tests

1. Add an unrelated explicit unit to the three-domain fixture.  After root
   propagation, native discovery must offer the same nine prefixes; one action
   must apply, preserve the old trail/reasons, allocate the expected fresh
   extension, and leave the Kernel invariant-valid.
2. Fix a selected domain variable at level zero.  Native discovery must not
   offer that domain, and an explicit selection must decline atomically without
   committing a key, owner, action ID, proof byte, or provenance byte.
3. Replace an exact tuple with a larger clause whose extra literal is root
   false.  X17 must not treat the residual as an exact tuple.  This negative
   test enforces the no-residualization contract.
4. Retain an owner with an unrelated root unit and apply a second prefix.  Then
   root-fix a source-domain variable and verify exact recapture reports stale.
5. Generate a DSR proof on a tiny no-UP UNSAT n-ary fixture with an unrelated
   root unit, force one public X17 action, finish by CDCL, and run the pinned
   `dsr-trim`/`dsr-compress`/`srcheck` pipeline.  Check the structural journal
   and semantic-candidate publication in the same run.

No benchmark rerun is justified until these tests pass.
