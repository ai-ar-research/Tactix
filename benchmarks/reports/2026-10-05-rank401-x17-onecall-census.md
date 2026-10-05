# Rank 401 X17 one-call census

## Immutable run

CPU-Q build job `1750713` completed `0:0` from exact source
`aab6e83f4d6e797c6d127c0cd0e0863799e1e62d`.  Its Release binary is
`f7f191f9593bcb12c9bbe52e2ceb8418fd93f6db533a7009310fdf4e430d6b3e`.
Anonymous-runtime validation job `1750731` completed `0:0` and checked the
SAT/model, UNSAT/DSR, crash, and timeout fixtures.

The immutable one-row campaign is
`benchmarks/runs/20261005-rank401-x17-onecall-census-aab6e83f4`.
Its manifest SHA-256 is
`3b2433b6366ecca7c893e47dc6c8fffd43a36dbb0e0087f3e69db7fa9c2057c4`.
The arm uses explicit DSR, controller policy 2, only family
`NaryCspPrefix`, native producer `nary-csp`, and the bounded one-call census.
It has no controller trace, full feedback, or retained provenance.

CPU-Q array job `1750735` ran task 0 for Tier-D rank 401
`52aaf653a79ca14fa1127cda32aa94ce-noL-11-10.sanitized.cnf.xz`.  The solver
hit the exact 60-second cap and returned `UNKNOWN`; therefore no model or proof
checker ran.  Slurm reports `FAILED 1:0` because this expected-SAT row did not
produce an accepted verdict, not because the detector probe or runtime failed.
Anonymous solver and certificate artifacts were deleted.

## Recognition boundary

The flushed, retained one-call line is:

```text
c nary-csp-census candidate-domains=7473 exact-domains=0 relation-keys=0 tuples=0 first-decline=insufficient-exact-domains reason=9 status=2 complete=1 work=67238 offers=0
```

The matching terminal counter is:

```text
c controller-rule NaryCspPrefix requests=0 applications=0 effective=0 exhausted=0 stale=0 interrupted=0 work=0 overshoot=0
```

This is a complete detector decline, not a resource stop.  X17 reaches its
domain scan, but none of the clause candidates has the explicit pairwise
binary AMOs required by `complete_amo`; relation indexing consequently never
starts.

## What the authoritative CNF means

A static census of the exact checker input finds 363 positive ternary ALO
clauses, 7,458 all-negative ternary clauses, ten positive units, and no binary
clauses.  Variables are consecutive three-colour cells.  The negative clauses
form 2,486 distinct three-cell relation keys; every key has exactly the three
same-colour forbidden tuples, and every cell pair occurs in at most one key.

The missing AMOs are not Boolean consequences that X17 should silently assume.
The CNF says that each active cell chooses a nonempty set of colours and that
no hyperedge may share one colour at all three endpoints.  It is nevertheless
safe to restrict every nonempty colour set to a singleton: changing selected
values from true to false preserves every negative clause and retains one
literal in each ALO.  Pairwise AMOs are therefore an equisatisfiable
strengthening, not a RUP implication.

The existing certified ULC path is the right proof boundary.  For a pair
`(not ai or not aj)`, it authenticates that the positive pivot has only its
ALO occurrence and emits the clause pivot-first as RAT/SR; the only pivot
resolvent is tautological.  Current source already contains that DSR/DRAT
certificate path and the indexed X17 producer, with focused proof fixtures.

## Smallest sound next step

Do not relax `complete_amo` or treat the noL domains as exact without a formula
transition.  Run one explicit, bounded ULC exact-one-completion action before
X17, then propagate, then take one X17 census/offer page from the same live
root.  The causal gate is:

1. ULC publishes independently certified pairwise AMO binaries (up to 1,089
   on the unassigned 363-domain shape);
2. propagation reaches a root fixpoint;
3. X17 reports nonzero exact domains and the expected indexed boundary of up
   to 2,486 relation keys and 7,458 tuple occurrences;
4. only then may a selected X17 action be tested for an independently checked
   DSR result.

The failed one-call gate does not justify a 300-second target run, and none was
submitted.
