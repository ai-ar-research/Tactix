# Rank569 conditional-clause causal check: no material benefit

## Disposition

The two independently RUP-validated clauses recovered from the failed
four-cube analysis changed ordinary search, but did not demonstrate useful
progress toward solving Rank569.  In matched 60-second runs, neither the
original formula nor the formula with the two clauses solved.  The conditional
arm processed 8.06% more conflicts and 7.70% more decisions, and emitted a
7.02% smaller proof prefix, but fixed-time search volume and prefix size do not
order two incomplete UNSAT searches by distance to a proof.

This branch is retired.  Its full discovery cost was 359,164,226 Kitten ticks
and 152,428 subsolver conflicts.  The two successful derivations alone cost
159,160,369 ticks and emitted 25,091,987 bytes of independently checked proof.
The changed 60-second trajectory does not repay that work.  There is no
production integration, unconditional unit, full-length Rank569 rerun, or
closure claim.

## Contract and logical precondition

Campaign `benchmarks/runs/20261007-rank569-conditional-clause-causal60`
used solver binary SHA-256
`10942fbedbb8a66712ffb439c93601d6a74639f4f2548e43c05a0f477598299e`.
The original plain CNF SHA-256 was
`06d08c3b44a98a2eea13d37b2bb5fe60913fc1d38989997595ddedbcb10b1d9b`;
the augmented CNF SHA-256 was
`b5421bfbc5fabc615def96aa9e7f0f17deb490746637594702b3f8dc55646f1e`.
The augmented file preserved the exact original clause sequence, SHA-256
`094970f9865fb4608189ba55c198485427417ce72deb6a089d225c2f18729d15`,
and appended only:

```text
21 9 -3411 0
-21 -9 -3411 0
```

The preceding frontier-cube report records the independent forward
`drat-trim -f -U` checks.  Both derivations were accepted over the exact
original CNF, reported zero RAT lemmas, contained no deletion records, and
ended in the clauses above.  Therefore these are entailed conditional clauses,
not satisfiability-preserving RAT additions being misused as logical reasons.
Neither clause is root RUP on the original clause set.

SLURM job `1755130` ran the original arm first and the conditional arm second
inside one allocation on `node017`, pinned to the same CPU.  Each solver call
had a 60-second limit and 10-second TERM grace.  Both used `--auto-proof` and
`--default-graph-labeling=on`; persistent affine search remained at its default
off setting.  Inputs were warmed before timing.  The order was frozen before
submission and was not selected after observing results.

## Result

| metric | original | plus two clauses | change |
|---|---:|---:|---:|
| disposition | timeout / incomplete | timeout / incomplete | no closure |
| wall time | 60.00 s | 60.00 s | matched |
| conflicts | 1,416,647 | 1,530,870 | +8.06% |
| decisions | 2,305,503 | 2,482,935 | +7.70% |
| propagations | 174,214,395 | 180,521,262 | +3.62% |
| search ticks | 3,390,208,594 | 3,547,845,046 | +4.65% |
| learned-clause mean size | 32.19 | 28.34 | -11.96% |
| proof-prefix bytes | 161,275,065 | 149,945,970 | -7.02% |

The original prefix SHA-256 was
`a086a28c158b0d721cb9ebe11f4d0541df356437b5a700595b70472464e7af73`;
the conditional prefix SHA-256 was
`3ab02ef31f95a0ad3dfe5d69cf6cb4fcba937834f3c38dac029efd12ccf26721`.
They are incomplete proof prefixes and were retained only by size and hash.
Both solver calls exited through the timeout wrapper with status 124 and
printed complete statistics footers.  No SAT or UNSAT result was produced.

The conditional arm spent less time in inprocessing (6.464 seconds versus
7.589 seconds) and learned shorter clauses on average.  Those differences can
explain why it processed more conflicts while writing fewer proof bytes.  They
do not establish a causal reduction in time to contradiction, especially
after charging the much larger analysis that produced the two clauses.

The machine-readable result is preserved as `result-summary.json`.  Raw
stdout, timing records, frozen scripts, manifests, and exact inputs remain in
the campaign directory.
