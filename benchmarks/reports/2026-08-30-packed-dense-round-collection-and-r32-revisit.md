# Packed dense stream, elimination-round collection, and Rank-32 revisit

Date: 2026-08-30
Production base: `1c719fc8e23c94868164a9559491f2c3550efac1`
Combined Release binary: `85b832fe859bd082cf698f1ecdef6a975116410c2b2eb94b90c4eefaaf76a3e2`
Pinned Kissat: `8af8e56f174b778aef3aa45af9f739b2a5f492c2`

## Outcome

Retain both general repairs:

1. Eliminate's consumer-only mixed binary/large occurrence stream is one
   32-bit tagged word per entry. Binary deletion performs stable physical
   compaction instead of leaving an 8-byte entry with a live tombstone.
2. Every completed consistent authored BVE round performs whole-arena
   collection, including zero-yield rounds, and suppresses only the redundant
   generic phase-exit collection.

They change no rule admission, effort, classification consumer, formula/rank/
benchmark predicate, or strategy selection. Production remains formula-
invariant at BVE 100 permille, Factor 50 permille, forward subsumption 100
permille, and Vivify 100 permille. Kissat creation order remains unconditional;
CaDiCaL/raw-reference order is diagnostic only. N1--N4 are unchanged.

The combined homogeneous Rank-1--40 campaign solves all 40 authoritative
instances with the expected verdict. Its total time improves 0.88% versus the
fresh accepted binary, while its per-rank geometric mean is flat. There is no
aggregate performance reason to discard either repair.

## Direct mechanism evidence

### Packed dense representation

Campaign `20260830-0115-tierd-rank21-packed-dense-allocator`, job `1656543`,
compares base and packed on the same node with 310 aligned slot records and no
non-memory state difference.

- Peak dense capacity: 1,515,570,396 -> 978,998,696 bytes, saving 536,571,700.
- Allocator-usable dense storage: 1,601,424,576 -> 1,075,397,376 bytes, saving
  526,027,200.
- Process peak: 4,134,988 -> 3,574,968 KiB, saving 560,020 KiB (13.54%).
- Allocation count is identical; diagnostic runtime is 169.753 -> 166.703 s.

This proves the representation saving at its owning high-water point. It does
not promise the same process-peak reduction on every formula: Rank 32, for
example, has another high-water owner.

### Exact round collection

The isolated three-wave Rank-21 screen preserves all non-time behavior; median
runtime is 1.0047x and median RSS 0.9778x, saving 91,640 KiB.

The fixed B28+C5 cumulative guard
`20260829-1800-BC-round-collect-guard`, job `1656454`, is terminal:

- 66/66 valid results, 17 SAT plus 16 UNSAT per arm;
- every model and proof independently verified;
- zero cleanup or certificate residue;
- shipped/candidate PAR-2 2,439.313/2,445.782 s, ratio 1.002652;
- pass threshold 1.10.

The only non-time statistic differences are 7--76 avoided stale eager-
subsumption attempts on five pairs. Collection resets the four-entry recent-
clause register at the reference lifecycle boundary; outcomes agree.

## Rank-32 causal closure

The accepted Rank-32 gap is 601.285 s versus Kissat 289.685 s. Exact commit
bisect confined the trajectory jump to `9fecb8f`, which combined logical-
position watch reconstruction and Kissat's `subsumeocclim=1000` correction.

The four-way mechanism split established:

- the 1000-entry occurrence cap completely owns the first Strengthen work and
  formula difference at conflict 15,351;
- logical reconstruction has no effect through that boundary and begins later,
  at the Vivify dense resume at conflict 228,994.

The cap witness is clause 3,178,372,
`(-404938 \/ 77795 \/ 404898)`. Its first mixed stream has 155 entries. Cap 100
skips the stream and retains an earlier strengthening; cap 1000 reaches the
binary `(-404938 \/ 404898)` at position 130 and deletes the candidate by
subsumption. Pinned `forward.c` deliberately lets later subsumption override a
remembered strengthening. The production cap is therefore correct and remains.

At the first reconstruction boundary, 161,794 nonempty watch lists have zero
identity, population, capacity, or order-independent multiset difference. Six
lists differ only in sequence; 2,781 slice offsets differ from allocation
interleaving. The final alias trace explains all 13,432 changed ordinals as one
stable displacement caused by exactly six relocated live clauses preserving
their inherited logical positions. No watch or clause is missing or duplicated.
Logical reconstruction is therefore source-faithful and remains the production
default. The remaining Rank-32 gap is downstream Vivify/propagation/search
behavior, not permission to restore raw-reference ordering or a formula gate.

Frozen diagnostic campaigns are `20260829-1730`, `20260829-1915`,
`20260829-2315`, `20260829-2345`, `20260830-0015`, `20260830-0130`,
`20260830-0230`, and `20260830-0300`; each owns its exact classification and
hashes.

## Homogeneous Rank-1--40 candidate

Campaign `20260830-0345-tierd-ranks1-40-packed-round-collect`, job `1656548`,
uses exactly one uniform solver task per rank, no throttle, no node constraint,
no grid, and no comparator rerun. The fresh fully checked campaign
`20260829-1930-tierd-ranks1-40-accepted-fresh` supplies accepted Tactix and
Kissat values.

| Metric | Combined candidate | Accepted Tactix | Kissat |
|---|---:|---:|---:|
| Solved / 40 | 40 | 40 | 40 |
| SAT / UNSAT | 22 / 18 | 22 / 18 | 22 / 18 |
| Total / PAR-2 seconds | 11,123.861 | 11,222.888 | 7,675.236 |
| Candidate ratio | 1.000 | 0.99118 | 1.44932 |
| Geomean candidate ratio | 1.000 | 1.00005 | 1.42985 |
| Maximum sampled tree RSS KiB | 17,081,072 | 17,081,072 | 15,315,568 |

Selected ranks:

| Rank | Verdict | Candidate s | Accepted s | Cand/acc | Kissat s | Cand/K |
|---:|:---:|---:|---:|---:|---:|---:|
| 15 | SAT | 497.086 | 515.019 | 0.965 | 80.788 | 6.153 |
| 19 | SAT | 771.403 | 761.931 | 1.012 | 739.902 | 1.043 |
| 20 | SAT | 342.868 | 347.381 | 0.987 | 140.142 | 2.447 |
| 29 | SAT | 304.508 | 317.158 | 0.960 | 297.121 | 1.025 |
| 31 | UNSAT | 779.883 | 784.328 | 0.994 | 277.345 | 2.812 |
| 32 | UNSAT | 598.209 | 601.285 | 0.995 | 289.685 | 2.065 |
| 36 | SAT | 384.012 | 384.210 | 0.999 | 368.510 | 1.042 |
| 38 | UNSAT | 740.671 | 787.259 | 0.941 | 366.258 | 2.022 |
| 40 | UNSAT | 505.741 | 510.240 | 0.991 | 432.318 | 1.170 |

The full per-instance runtime and memory table is
`benchmarks/runs/20260830-0345-tierd-ranks1-40-packed-round-collect/comparison-live.md`.
At solver completion all 22 SAT models and seven UNSAT proofs were independently
verified. Eleven UNSAT proof checks remain nonblocking and must be allowed to
finish: Ranks 18, 22, 27, 31, 32, 33, 35, 37, 38, 39, and 40.

## Codebase revisit

The revisit inspected every changed production path and its pinned Kissat
owner, not only formatting:

- all four binary-deletion variants maintain the packed mixed image by stable
  compaction, including duplicate retention and remove-all paths;
- BVE candidate inspection compacts stale large refs, while arena collection
  rebuilds every relocated reference before any later consumer;
- unconditional zero-yield collection matches `eliminate.c:416-509`;
- phase-exit suppression cannot hide a production tail rule: post-BVE
  Instantiate/BCE/CCE remain explicit default-off non-Kissat diagnostics, and a
  productive diagnostic tail sets the generic collection obligation again;
- no new classifier, option, effort, formula, density, rank, benchmark, or
  identity consumer appears in the source diff;
- exact classification still has only Kissat's `bigbig` fast-assignment
  consumer; `small` remains write-only;
- focused Kernel/BVE/SlotPose/config/N4 tests pass. The full Release suite had
  already passed 76/76 on the exact combined source.

The audit found no dead production mechanism requiring deletion. An apparent
duplicate return in a chunked display was explicitly rechecked and did not
exist. Documentation detail was moved here because the size gate correctly
identified `PROGRESS.md` as historical narrative rather than a resume surface.

## Remaining work

1. Let the eleven proof checks finish and record final zero-residue status.
2. Run the required Claude-owned fuzz campaign. External Claude access to this
   private workspace remains blocked pending explicit user authorization; do
   not substitute a different fuzz owner.
3. Commit and push only after that consolidation gate. The previous push also
   remains blocked by private-repository egress.
4. Resume general performance convergence at the largest SAT gap, Rank 15
   (6.153x), using the next common-state Vivify/propagation/search divergence.
   Do not reopen the now-closed occurrence cap, logical reconstruction, Factor
   admission, creation-order default, or any formula-specific profile.
