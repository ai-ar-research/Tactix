# Certificate-gap evidence boundaries after Rank541

This note records two unresolved fixed44 certificate rows for which the saved
metadata identifies a concrete boundary but the original certificate was
deleted.  It does not change the solver or checker route and does not claim a
target recovery.

## Rank1004: one witnessed event, no retained event

The current Tactix result produced a 4,750,744,327-byte binary DSR certificate.
Its solver counters report exactly one substitution-redundancy application and
five witness literals.  No other counter reports a witnessed rule.  The
canonical eager `dsr-trim` stage exited 255 after 99.654 seconds at 4,528,336
KiB RSS with `Ran out of memory on xrealloc()`.

This is a route/representation cliff, not evidence that the certificate is
invalid.  Kissat-sc2024's 4,235,308,992-byte witness-free DRAT certificate for
the same CNF verified in 8,949.579 seconds.  Satsuma+Kissat's
4,784,285,076-byte DSR certificate hit the same `dsr-trim` allocation failure.
The exact Tactix certificate was deleted and the identity audit finds only its
deleted result record.  Therefore the lone SR candidate cannot now be tested
for an equivalent RUP/RAT publication, and a generic RUP-first solver change
would be speculative.

## Rank1075: saved cross-revision comparison

Both saved Tactix executions solve the same original CNF and report the same SR
shape: 32 applications, 32 applied, and 160 witness literals.  They differ in
solver revision, proof-opening mode, checker binary, and checker mode.

| Field | 2026-09-18 checked result | 2026-09-28 unresolved result |
|---|---:|---:|
| Effective solver source | `a31bbe994` + patch `452ca017...` | `d170b4a51` |
| Solver binary | `2ef87798...` | `2fc7421f...` |
| Solver proof option | direct binary DSR | `--auto-proof`, selected binary DSR |
| Solve time | 179.149 s | 176.838 s |
| Decisions | 125,351,472 | 130,388,620 |
| Conflicts | 1,519,019 | 1,122,884 |
| Learned clauses | 1,334,094 | 983,469 |
| Mean learned size | 28.15 | 34.76 |
| Approx. learned literals | 37,554,746 | 34,184,781 |
| Reductions / deleted learned clauses | 173 / 630,735 | 141 / 395,737 |
| Restarts | 40,848 | 45,019 |
| BVE eliminated / attempted resolvents | 346,290 / 80,961,722 | 323,282 / 85,522,153 |
| Factor materialized / reused | 7,834,421 / 2,200,221 | 7,403,911 / 1,753,254 |
| SR applications / witness literals | 32 / 160 | 32 / 160 |
| Certificate bytes | 967,690,200 | 1,088,746,007 |
| `dsr-trim` invocation | `-q -f -c` (forward, every lemma) | no options (backward/core trim) |
| `dsr-trim` result | verified in 4,926.561 s | timed out at 14,393.005 s |
| Final Trestle | verified in 1,430.701 s | not reached |

The certificate grew 121,055,807 bytes (12.51%) even though learned clauses
fell 26.28% and their estimated literal volume fell 8.97%.  The SR population
is unchanged.  The growth must therefore come from changed clause contents or
non-learned proof traffic, but aggregate counters do not attribute bytes to a
rule.  BVE attempted 4,560,431 more resolvents, but this counter includes
discarded attempts and cannot be equated with emitted proof additions.

The effective source diff contains several plausible confounders: automatic
DSR/PBP output selection, DSR root-unit/deletion transaction preservation and
batching, bounded deletion spooling, and changed BVE attempt/scheduling
semantics.  These are general proof/search changes; the saved counters do not
isolate one as the cause.

The most specific falsifiable hypothesis is checker-side: for this proof shape,
forward `dsr-trim -q -f -c` can be materially faster than backward core
trimming.  The earlier forward run completed in 4,926.561 seconds, whereas the
later backward run did not finish in 14,393.005 seconds.  This is not yet a
causal result because both the certificate and checker binary changed.  A valid
test requires one retained certificate checked by both modes with the same
checker pin.  Neither Rank1075 certificate remains, so no bounded checker-only
job can perform that test now.

The complementary solver-side test is to count proof additions, deletions, and
serialized bytes by producing rule.  It would distinguish learned traffic from
BVE, Factor, root-unit transaction, and other inprocessing traffic on the next
ordinary retained run.  The current evidence does not justify changing proof
publication or disabling a rule.

## Exact boundary

No unresolved certificate-only fixed44 proof remains at its recorded byte size
under `build/` or `benchmarks/runs/`.  Rank1004 and Rank1075 therefore stop at
the same exact boundary: their metadata supports testable general hypotheses,
but deleted certificate bytes prevent the paired checker or event-level audit.
No new experiment was run and no solver/checker code was changed.
