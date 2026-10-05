# Arbitrary-width affine reason proof

## Purpose

A Gaussian matrix should publish one implication or conflict reason.  Exporting
the complete parity CNF costs `2^(width-1)` clauses and repeated X9 row export
already scales poorly on Rank263.  The shared parity certificate therefore now
supports one arbitrary-width reason clause.

## Contract

After exact authenticated equations have been accumulated, falsifying the
requested reason must assign every variable still present in their XOR and
must violate its parity.  Extra variables are permitted only within the
authenticated source universe; they weaken the consequence.  Duplicate,
zero, out-of-range, incomplete, or parity-satisfying requests decline before
proof output.  A successful request adds one RUP clause.  The existing helper
retirement then deletes the root unit and every balanced-XOR definition while
retaining the reason and permanently reserving helper identities.

## Independent check

The focused proof combines

```
x1 xor x2 xor x3 xor x4 xor x9 xor x10 = 0
x5 xor x6 xor x7 xor x8 xor x9 xor x10 = 1
```

to export `(x1 ∨ x2 ∨ x3 ∨ x4 ∨ x5 ∨ x6 ∨ x7 ∨ x8)`.  Setting `x1` through
`x8` false leaves two contradictory binary parities over `x9,x10`, but ordinary
unit propagation cannot see that contradiction before the exported reason.
The repository test runs forward `dsr-trim`, `dsr-compress`, and Trestle over
every proof addition; all stages report verified UNSAT.  Debug and ASan/UBSan
focused tests also pass.

## Remaining boundary

This is a proof primitive, not a live matrix action.  The next adapter must
revalidate the matrix owner and exact source combination, map the returned
reason into this certificate, preflight proof/namespace/database costs, publish
the clause atomically, and only then expose it to CDCL propagation or conflict.
