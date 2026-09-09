# Current Rank 1–50: memory ownership, matching prefixes, journal repair

## Outcome and next decision

The runtime target is not achieved. Production remains c2a95a6 and cumulative
6107 remains isolated. This checkpoint seals the three memory witnesses,
compares current/native initial decision streams, and repairs an observer-only
Substitute transaction omission. It does not claim a new runtime improvement.

The next memory probe should sample inside BVE, around allocation growth and
collection. End-of-slot retained capacity is insufficient: all three observed
RSS maxima occur during BVE and exceed its end RSS by roughly 0.9–1.4 GiB.
The collector's `seen(arena.size())` scratch is Debug-only and cannot explain
these Release observations. Collection still overlaps old and fresh clause
arenas and rebuilds dense occurrences before the old arena is destroyed;
whether that overlap owns the maximum is an untested hypothesis.

## Memory: checked full-run replay, not instrumented timing acceptance

[6129 summary](../runs/20260908-6129-publication-memory/summary.json) seals
job1676999, ranks21/25/9, unchanged6107 trace pin6127. All three original SAT
models independently check; scratch cleanup, every non-timing statistic, and
emitted proof bytes/hash match the corresponding6115 Release records. SAT
proof-byte equality is a work-replay check, not a substitute for model checking.
All samples and source records are hash-bound. Instrumented time is not priced.

| Rank | Release peak KiB | Observed peak KiB | BVE-end RSS KiB | Dense capacity bytes at that end | Reconstruction capacity words |
|---|---:|---:|---:|---:|---:|
| 21 | 3034340 | 3022936 | 2084380 | 658928856 | 133208160 |
| 25 | 3409704 | 3408248 | 2294084 | 1514360112 | 118652928 |
| 9 | 3226172 | 3224360 | 1797656 | 654510696 | 126518784 |

These are cumulative-candidate memory figures, not accepted-production ones.
Large cold dense/reconstruction capacity is visible, but freeing it after the
peak would not demonstrate a peak-memory repair. No arena replacement,
reconstruction-rule change, tuned growth constant, or source promotion was made.

## Search work: two bounded current/native comparisons

Both diagnostic jobs used the matched12CPU/48000MiB development contract,
one bound thread,1200 solver seconds and25-minute allocation. Candidate6127 and
native6092 are immutable. Requested conflict-stop exits and partial-certificate
cleanup pass; these are not checked full solves or timing acceptance runs.

* [6139 rank21](../runs/20260908-6139-rank21-current-prefix/summary.json),
  job1677029: **18,484,618 selected decision rows match exactly**, through
  10,000 conflicts. The first coarse boundary count difference is Sweep end at
  conflict3787: Tactix3333246 versus Kissat3333288 binaries, with equal active,
  irredundant and redundant counts. This42-binary difference does not establish
  a harmful semantic difference: every decision still matches to the stop.
* [6140 rank4](../runs/20260908-6140-rank4-current-prefix/summary.json),
  job1677030: **169,459 selected decision rows match exactly**, through100,000
  conflicts. All186 projected search boundaries also match. Startup contains
  an extra unchanged candidate Substitute slot. Full Release candidate and
  fresh Kissat have equal conflict/decision/restart/reduction totals; a unit-cost
  explanation is plausible, not proven by this bounded prefix. Propagation
  counters have different conventions and are not a valid naive throughput ratio.

Both observers include startup/warm-up decisions while solver statistics reset.
The recorded unsigned start ordinal wraps; actual stream row counts, complete
stream equality and endpoint checks are the authority, not that ordinal.
Huge diagnostic streams remain local and hash-bound, not staged as ordinary logs.

[6135 lineage](../runs/20260908-6135-rank21-lineage/summary.json) consolidates18
checked historical rank21 records. The major sampled changed-work increase
appears between5858 and5908, after the general Reduce root-publication repair,
not first at6107. Different windows do not isolate patch timing causality.
The existing Rank32 closure must be preserved while repairing coupled cost;
regressing standalone5870 is not part of retained5900 ancestry.

## Observer repair and test consideration

[6136 manifest](../runs/20260908-6136-substitute-journal/manifest.json) freezes
the successor to6107. Unbudgeted composed Substitute now records its entire
transaction, including reconstruction, retirement and final watch reconnection,
under the source effort meter when the rule journal is compiled in. Budgeted
Substitute remains schedule-only: per-round journal coverage is explicitly open.

The actual old journal objects fail the new missing-transaction assertion.
New Release and journal suites each pass99/99, including all eight policy
fixtures. Release SHA86a251c9fec53cdd54c338a5b6cbc32aafe7f117b4ee06ea8e10c22d50bee235
is byte-identical to6107; journal SHA is
e801b90e68e637d9e5b748184982cf67aa0be9062d70848053c42b5db76c799f.
No production instruction, guard, proof rule, scheduling or budget changes.
The deliberate old-object assertion is a metadata regression test, not a solver
soundness failure. Parent-source fuzz and normal promotion gates remain required.

## Comparison authority and hygiene

[6143 table](2026-09-08-fresh-rank50-6143/comparison.md) preserves all50 ranks
of accepted-production/current-Kissat campaign6121/job1676920:100 solver
records,93 checked and cleaned certificates, seven pending validations. There
is no partial aggregate or complete cleanup claim. No published invalidity or
solve loss appears in this snapshot. Checks continue asynchronously.

Matched41–50 campaign6108/job1676783 already uses the same12CPU hardware class
as the original forty; all ten Kissat references check. Frozen50 denominator
7239.010s remains unchanged. Full6107 campaign6115 has50 expected solver answers,
8559.858s/7239.010s=1.182462519x as a solver-time observation, pending final gates.
No cpu-q timing is mixed into this denominator, and its existing campaign is
untouched. The validation companion captures later final-check counts.

No jobs were cancelled, no frozen pins modified, and no manual deletion occurred.
The briefly considered shorter memory allocation was not submitted: a live read
showed the original probes had started, and all completed normally. Claude-owned
fuzz artifacts were not accessed; denied transfer and push were not retried.
Living documents stay within budgets; detailed evidence remains here rather
than expanding the plan or resume surface.
