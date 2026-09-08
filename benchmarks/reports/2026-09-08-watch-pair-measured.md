# Watch-pair append: measured exact-work gain

## Outcome

All six Rank-35 solver runs in campaign **5468** have expected UNSAT verdicts.
Each of the three candidate/predecessor pairs has identical complete non-timing
statistics and identical proof SHA-256. Median paired runtime is
**0.983980273x** predecessor, range **0.981238269–0.993027626**: approximately
**1.6%** local reduction. Both complete arm spreads are below 2% of their
medians. This supports retaining the append implementation optimization for
continued development, not promotion before its pending correctness gates.

The [solver checkpoint](../runs/20260907-5468-watch-pair-rank35-runtime/solver-checkpoint.json)
records all six result hashes, every pair, spread and verification state.
**0/6 proofs were independently checked at this snapshot.** Byte identity is
not a proof check; all checkers continue in the existing background tasks.

## Matched evidence

| Pair | Candidate s | Predecessor s | Ratio |
|---|---|---|---|
| 0 | 351.954 | 357.684 | 0.983980273 |
| 1 | 350.361 | 352.821 | 0.993027626 |
| 2 | 348.684 | 355.351 | 0.981238269 |

Candidate median is 350.361 s with range 348.684–351.954 (0.9333% spread);
predecessor median is 355.351 s with range 352.821–357.684 (1.3685% spread).
Every proof has SHA-256
`c6d148443294bda8c11ee6dba357fbbf02145447671b6b7d4d547f1d6df81398`.
Immutable campaign pins, manifest and complete statistics were validated by
the existing exact-work comparator before timing interpretation. This meets
the predeclared three-pair measurement contract, not a best-of observation.

[Implementation authority](2026-09-07-watch-pair-append.md) owns candidate
5466, parent 5459, exact-layout rationale, 180 differential fixtures, and both
83/83 Release suites. This checkpoint changes no source. No new unit tests are
needed for an evidence-only change: the existing fixed state fixture, complete
pair oracle and proof/model gates cover its premise. Production's full fixed
suite passes **77/77 in 5.14 s**. The optimization changes neither scheduling
nor logical work, so this is useful implementation-cost evidence on the chosen
witness. It does not establish a universal 1.6% benefit or per-append cost.

## Remaining scope

The full Rank 1–40 goal remains at most 1.01x Kissat. Latest complete full
view 5465 is **1.173817099x**, from the preceding bound candidate; it excludes
this append optimization and the later Sweep cursor. Its current certificate
count is 64/80 checked, while its frozen solver snapshot records 58/80.
The independent local gains must not be multiplied across windows or subtracted
from that full ratio to manufacture aggregate competitiveness.

Watch-pair B28+C5 5469 remains pending, as do Sweep-cursor timing 5475 and gate
5476. Source 5473 already descends from 5466; no recombination, rebuilding of
submitted pins or extra timing replication occurred. Rank-31/32/37 and Square
causal probes remain queued to address the much larger full-run work gaps.
Fuzz owner session 30094 remains live on 5438, not later candidates. Only its
terminal response was polled; no fuzz resources were inspected. The next
periodic checkpoint must cover all changes through 5473.

The wrap-up audit and Andrew report own final pin, documentation and residue
checks. No manual deletion, promotion, push retry or checker-contract change.
Push remains unapproved. Pending expected verdicts permit investigation but
never certify correctness or promotion; an actual failure pauses dependent
work. Next consume the causal traces and Sweep timing, and close existing
proof/regression gates as their checkers finish.
