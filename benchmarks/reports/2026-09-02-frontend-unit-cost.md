# Front-end unit cost: block scanner, blocked liveness filter, allocation-free ingest

Date: 2026-09-02

## Outcome

Two trajectory-preserving repairs to the DIMACS front end land as commits
`86a7705` and `bdb0d44` on top of the accepted `c44a538` (production behavior
`f3ac562`). They change no rule, schedule, or clause order: every proof,
model, and non-timing counter is byte-identical to the accepted records, and
the gain is pure unit cost on the input path that every instance pays.

**Gates on pin `bdb0d44` (Release SHA `1aaad2bbae57d670`).** Homogeneous
Rank 1--40 campaign `4750` (job `1659956`): 40/40 expected verdicts, every
non-timing counter identical to the accepted `4701` record on all 40 ranks,
DRAT certificates identical on every rank whose independent check had
finished at aggregation, total 9,511.6 s against 9,597.0 s (-85.4 s; the
gain sits on the huge ranks: Rank 5 -11.8, Rank 8 -9.6, Rank 10 -13.0,
Rank 13 -9.7, Rank 14 -11.0, Rank 19 -23.9 s, with the long UNSAT ranks
inside +-16 s of node noise). B28+C5 campaign `4751` (job `1659957`): 66/66
expected verdicts, paired PAR-2 ratio 0.547 against the frozen shipped pin,
within the 1.10 guard. Kissat's total is 7,675.2 s; T4-K needs 8,442.7 s.

## How the target was found

Campaign `4746` (same window, node017) ran the accepted `f3ac562` pin and the
pinned Kissat comparator on the two largest zero-conflict ranks with the gate
invocation (node-local decompressed DIMACS, DRAT proof) and Tactix's front-end
and Substitute phase timers enabled:

| Rank 10 phase | Tactix | Kissat |
|---|---|---|
| DIMACS parse | 29.9 s | 17.2 s |
| Substitute (2 rounds) | 23.4 s (rewrite 16.8 s) | 9.9 s |
| Congruence | 36.2 s | 36.9 s |
| Sweep | 4.1 s | 2.5 s |
| lucky + scaffolding outside the inprocessing slot | 14.6 s | 4.6 s lucky + about 9 s unprofiled init |
| wall | 114.6 s | 81.5 s |

Rank 5 (3.65 GB, 58.6M variables) is the pure parse case: 36.0 s versus
20.4 s with everything after the parse at parity. The same decomposition
holds on Ranks 7, 8, 13, and 14 from the accepted `4701` records against
Kissat's `1930` profiles (Congruence at parity on every one).

Campaign `4747` profiled a Release build with `-g` whose `.text` section is
byte-identical to the accepted pin (`94822bd2a1513460`), with dwarf call
graphs and source-line attribution. On Rank 5 the per-byte reader and the
whitespace loop carried 17% of all samples, the three random cache lines
touched by the binary-liveness Bloom filter per key another 8%, clause
ingest 18% (three heap allocations and a sort per large clause), and
zero-filled vector growth about 10%. On Rank 10 the same filter also cost
6.6 s out of line in Congruence's binary-candidate scan.

## The repairs

- **Block scanner** (`86a7705`): `parse_dimacs` scans one input block with
  the cursor and limit in locals and refills before any loop concludes the
  input ended. `parse_dimacs_with_block` exposes the block size so the tests
  parse the same formula at every block size from 1 byte up and require the
  same import fingerprint and the same error text for malformed input.
- **Blocked liveness filter** (`86a7705`): `BinaryLivenessIndex` keeps four
  bits per entry but places a key's three probe bits inside one 512-bit
  block, so a membership test or insertion touches one cache line. It stays
  a no-false-negative prefilter; the mirrored watch scan remains the exact
  authority (`SP-DRAT-BINARY-KEYS-1`).
- **Allocation-free ingest** (`bdb0d44`): `ingest_clause` dedups large
  clauses in place through a parse-scoped literal mark array, keeping
  first-occurrence file order, and activates variables from the normalized
  clause (the same population the raw file literals activated).
  `kernel_record_import` returns at once for an already-ranked variable.

## Identity probes

Same-window A/B against the accepted pin on Ranks 1, 4, 5, 10, 12 with the
gate invocation, recording proof SHA-256, model SHA-256, and every
non-timing counter:

| Probe | Rank 5 parse | Rank 10 parse | Rank 10 solve | identity |
|---|---|---|---|---|
| accepted `f3ac562` | 35.8 / 36.6 s | 29.9 / 30.3 s | 82.0 / 82.5 s | authority |
| `4748` increment 1 | 27.9 s | 22.8 s | 80.0 s | all five ranks byte-identical |
| `4749` increments 1+2 | 24.7 s | 20.3 s | 80.0 s | all five ranks byte-identical |

Kissat parses the same files in 20.4 s (Rank 5) and 17.2 s (Rank 10).

## Validation

- Release 75/76 tests (the failing test is the pre-existing doc-size budget).
- Fuzz on pin `bdb0d44` (Release SHA `1aaad2bbae57d670`): quick leg job
  `1659954`, 10,000 cases, 0 failures, 9,819 reached a verdict, input
  restatement reached on 3,251 cases; random-policy leg job `1659955`,
  10,000 cases, 0 failures, 8,836 reached a verdict.
- Gates: campaigns `4750` (Rank 1--40) and `4751` (B28+C5), results above.

## What remains on the huge ranks

Rank 10 after the repairs: parse 20.3 s versus Kissat 17.2 s; Substitute
23.4 s versus 9.9 s. The `4747` profile attributes Substitute's excess to
several pieces of 2--4 s each (SCC search 4.3 s, the rewrite loops about
8 s self time, root sparse collection 3.9 s, per-clause unwatch and dense
maintenance about 4 s) rather than one hot spot; the 4.4 s
`kernel_canonicalize_binary_watches` is Congruence's dense entry and sits at
parity with Kissat's `enter_dense_mode` + `extract_binaries`. Kissat
disconnects all large watches before substitution and reconnects once;
Tactix keeps the accepted lazy in-place order (`SP-SUBSTITUTE-WATCH-1`), so
a unit-cost repair there must batch the per-clause removals without changing
the resulting list order. A Substitute-rooted call-graph profile is the next
measurement.

## The larger gaps are search-regime divergences, not unit cost

On the nine ranks where both solvers run identical trajectories (equal
conflict counts: 6, 15, 16, 20, 23, 26, 29, 30, 36), Tactix's search time is
within 3--7% of Kissat's except Rank 6 (1.59x). The remaining 1,900 s of the
Rank 1--40 gap sits on ranks whose trajectories diverge:

| Rank | Tactix | Kissat | conflicts T / K | note |
|---|---|---|---|---|
| 25 | 267.9 | 253.8 | 296k / 577k | Tactix 10.4k propagations per conflict versus 1.75k with near-identical BVE (566,508 vs 566,301 eliminated); Kissat spends 171 s in lucky |
| 21 | 324.1 | 104.5 | 126k / 93k | 1,289 versus 507 decisions per conflict; BVE 56.5 s versus 22.2 s; Sweep 22.4 s versus 7.6 s |
| 34 | 1,080.8 | 344.0 | 11.0M / 4.3M | chaotic SAT (585.8 s / 6.7M conflicts in another draw) |
| 27 | 496.5 | 260.0 | 7.4M / 4.1M | UNSAT; lifecycle repairs converged the prefix, residual open |
| 37 | 561.8 | 377.5 | 7.9M / 6.4M | UNSAT |
| 22 | 490.7 | 192.0 | 64k / 66k | inprocessing 367 s versus Kissat simplify 78 s: Sweep 189.6 s vs 33.9 s, BVE 68.6 s vs 5.7 s, Substitute 32.6 s vs 4.9 s |

Rank 25 is a propagation-per-decision regime difference and needs the
differential-trace method. Ranks 22 and 21 are inprocessing gaps at similar
search work, and the per-rule totals over all 40 ranks (Tactix versus Kissat
profile buckets, seconds) show where they generalize:

| rule | Tactix | Kissat | excess |
|---|---|---|---|
| BVE / eliminate | 417.4 | 194.1 | +223.3 |
| Factor | 315.8 | 129.8 | +186.0 |
| Vivify | 711.5 | 576.6 | +134.9 |
| Substitute | 273.5 | 169.6 | +103.9 |
| Congruence | 309.4 | 245.8 | +63.6 |
| Sweep | 574.4 | 564.9 | +9.5 |
| Backbone | 71.8 | 69.4 | +2.4 |

On the crafted UNSAT family (Ranks 31, 33, 37) the work is identical and
only the unit cost differs: Rank 33 performs 7,953 factorings against
Kissat's 7,874 in 26.9 s versus 9.2 s (2.9x), eliminates 364,546 variables
against 368,873 in 29.6 s versus 16.0 s (1.9x), and merges 67,292 congruent
gates against 67,115 in 12.0 s versus 5.7 s (2.1x); Ranks 31 and 37 repeat
the same ratios. Vivify differs in work there (Kissat vivifies about three
times as many clauses), so it is a scheduling question, not a unit-cost one.
Campaign `4752` (job recorded in its manifest) profiles Ranks 33 and 22 with
the code-identical `-g` pin and dwarf call graphs so the next step can start
from Factor's, BVE's, Congruence's, and Sweep's own statements.
