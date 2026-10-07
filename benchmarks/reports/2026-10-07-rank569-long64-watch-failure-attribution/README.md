# Rank569 long64 watch failure: source-consistent, artifact unavailable

## Disposition

The retained Rank569 checker error is precisely consistent with the newly
localized `LONGTYPE` deleted-clause predicate defect.  The defect supplies a
complete source-level path from a soft user deletion to the observed missing
backward watch.  It does not prove that this particular 4.116 GB certificate
would verify after the one-line checker repair because the certificate and all
derived checker artifacts were deleted by the frozen campaign contract.

No Rank569 certificate remains to recheck.  A filesystem audit found no
retained benchmark artifact large enough to be the 4,115,525,606-byte current
proof or the 3,165,736,883-byte older proof.  Both authoritative reports also
state that their proof artifacts were deleted.  Regenerating either proof is
not justified: the current candidate took 3,627.980 seconds and emitted a
4.116 GB proof, so checker repair alone would still miss both the 3,359.975
second runtime target and the compact-proof target.

## Exact failure

Job `1754535` produced solver exit 20 and certificate SHA-256
`9dd8775c0b685071aefd276a52d51dc8369de2e55c6fb4fe147722b5f35ed6c0`.
The 64-bit `dsr-trim` pin, SHA-256
`94f5fc0c122b7996207fb38f9010180f2d1e6b61b3008a3ef515ab5d971bc614`,
parsed 90,194,257 proof lines and 45,073,931 additions.  Backward checking
then failed at addition line 45,073,817:

```text
Clause 45084048 not in watch pointers for literal 18
```

The empty clause was addition line 45,073,831, so the failure occurred only 14
additions earlier.  The exact result JSON has SHA-256
`18a8867a5159ebe2bbecbe05ce89bc5db2198cd1f6d68d4cc6b0b7bc0c75254c`.

## Source path

The audited checker source archive is
`benchmarks/runs/20261007-dsr-capacity-long-audit-r3/source.tar`, SHA-256
`397557639afc3a58b7a46979a4bb77e09b664b049b7017d71355435ba08632ad`.
It binds upstream `0b8cc0ef4d228c059994923cbc66abeb9a42ea14` plus only the
repository forward-elaboration patch.  `src/global_data.c`, SHA-256
`a76c6d1f5538d4f9c9215305c6402920298b73d161af19a6db44ef60cdf92399`,
defines the deleted marker as the most-significant bit of `srid_t` and returns
the raw masked value through an `int` function:

```c
#define IS_DELETED_CLAUSE(x) ((x) & SRID_MSB)
int is_clause_deleted(srid_t clause_index) {
  return IS_DELETED_CLAUSE(formula[clause_index]);
}
```

Under `make long`, `SRID_MSB` is bit 63.  Converting a value with only bit 63
set to the 32-bit `int` return type yields zero on the audited platform.
Consequently `is_clause_deleted` reports false for a soft-deleted clause.
Boolean normalization, `((x & SRID_MSB) != 0)`, removes that truncation; the
separate Rank147 localization owns its qualification evidence.

The backward lifecycle in `src/dsr-trim.c`, SHA-256
`03fd2e875945edb1374032ef7da3427c36203a88c20fcb693054425b598266ff`,
then predicts the Rank569 message:

1. Forward parsing of a user deletion removes both watches and calls
   `soft_delete_clause`.
2. Backward `restore_wps_for_user_deleted_clauses` restores the clause and its
   two watches only when `is_clause_deleted(del_id)` is true.  The truncation
   makes this branch false in the 64-bit build.
3. A later `uncommit_clause_and_set_as_candidate` unconditionally calls
   `remove_wp_for_lit` for both literals of a nonunit clause.
4. `remove_wp_for_lit` fails with exactly `Clause ... not in watch pointers for
   literal ...` when the skipped restoration left no matching watch.

This is stronger than a generic similarity between checker failures: the
localized false predicate suppresses the exact operation whose absence the
Rank569 fatal message reports.

## Evidentiary limit and next use

The deleted proof prevents confirming that clause 45,084,048 was one of the
soft-deleted clauses affected by the predicate.  It also prevents a repaired
64-bit replay, a forward/backward comparison, extraction of the surrounding
deletion neighborhood, or independent Trestle verification.  The older
Rank569 proof ended in checker exit 139 rather than a retained missing-watch
message, so this audit does not retroactively attribute that failure.

The checker repair should be qualified on retained certificates and then used
for newly generated proof-side fixed-44 results.  It may recover checker
outcomes across the 13-row proof-side cohort without changing solver search.
Rank569 should wait for a solver candidate that is already materially faster
and smaller before paying another full generation cost.  A repaired checker
result by itself would not satisfy the user's Rank569 performance goal.
