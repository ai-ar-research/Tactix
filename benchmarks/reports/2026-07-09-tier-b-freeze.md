# Tier B freeze — 2026-07-09

- **Hardware (reference machine, O1):** Apple M3 Pro (6 performance + 6
  efficiency cores), 36 GB RAM, macOS 15.7.7. All tier-freeze timings at
  **jobs = 1** (sequential, one instance at a time).
- **Git:** bae2f78 (plan decision (bae2f78) Tier B definition: 10–15 instances, Kissat
  5–30 s band, ≤ 2 per family).
- **Reference solver:** Kissat 4.0.4 (pinned `rel-4.0.4`,
  `tools/solvers/setup.sh`).

## Method

1. **Pre-filter from published SC results** (per user direction — no blind
   sweep over all ~1200 instances): downloaded the official detailed
   main-track runtime CSVs for SC 2023 and SC 2024 (satcompetition.github.io;
   SC 2025 publishes no CSV — only a GBD track link — so 2025 instances are
   not represented; revisit with O9). 712 unique instances with verified
   verdicts across both years.
2. **Proxy band:** kept instances whose Kissat-family reference runtime
   (kissat-sc2024 for 2024, Kissat_MAB_prop for 2023) was in [8, 70] s —
   the competition cluster is roughly 2× slower per core than the M3 Pro,
   so this brackets the local 5–30 s target. 95 instances qualified;
   capping at 1 SAT + 1 UNSAT per family gave **44 candidates**, fetched
   from the Global Benchmark Database by hash (`tools/bench/fetch.py`).
3. **Local sweep:** Kissat 4.0.4, sequential, 60 s cap, wall-clock. 22 of
   44 landed in the 5–30 s band (13 SAT / 9 UNSAT). Every instance that
   solved agreed with the competition verdict. 8 proxy-band instances
   timed out locally (cluster/M3 runtime variance — exactly why the plan
   demands local timing rather than trusting published numbers).
4. **Selection:** 14 instances — 7 SAT / 7 UNSAT, **14 distinct families**,
   times spanning 5.4–27.6 s. Frozen in `benchmarks/tiers/B.list`
   (runtimes) and `B.sources` (GBD hashes for re-fetching).

## The frozen set

| Instance | Family | Status | Kissat s |
|---|---|---|---|
| combined-crypto1-wff-...-overlap-2 | cryptography | SAT | 5.43 |
| rbsat-v760c43649gyes10 | rbsat | SAT | 7.49 |
| battleship-14-26-sat | battleship | SAT | 9.05 |
| summle_X4044_steps7_I1-2-2-4-4-8-25-100 | summle | SAT | 10.18 |
| circuit_48in64out_with_800gates_... | random-circuits | SAT | 14.42 |
| qwh.50.1250.shuffled-as.sat03-1655 | quasigroup-completion | SAT | 20.10 |
| mod4block_2vars_10gates_u2_autoenc | modcircuits | SAT | 27.58 |
| hwmcc17miters-xits-iso-oski15a08b00s | hardware-miter | UNSAT | 5.49 |
| square.2.0.i.smt2-cvc4 | bitvector | UNSAT | 6.21 |
| clique_n2_k10 | clique-formulas | UNSAT | 6.91 |
| 9vliw_bp_mc.shuffled | hardware-verification | UNSAT | 11.43 |
| mp1-blockpuzzle_5x10_s7_free4 | puzzle | UNSAT | 12.06 |
| x9-08014.sat.sanitized | knights-tour | UNSAT | 12.98 |
| FmlaEquivChain_4_6_6 | equivalence-chain-principle | UNSAT | 19.52 |

Total Kissat time 168.8 s ≈ 2.8 CPU-min — within the plan's 2–8 CPU-min
budget; at `--jobs 4` a full Tier B run is ~1 min of wall clock plus proof
checking.

## Validation run (runner end-to-end, jobs = 4)

`tools/bench/run.py --tier B --solver kissat --jobs 4`:
**PAR-2 15.18, 14/14 solved (7 SAT, 7 UNSAT), all 7 DRAT proofs verified**
(221.3 MB of proofs, 695.9 s of drat-trim). PAR-2 at jobs=4 runs ~25% above
the jobs=1 mean (12.06) — memory-bandwidth contention, exactly why reports
must state concurrency and comparisons must hold it fixed.

The validation also caught a runner bug, now fixed: drat-trim reads plain
DIMACS only, so run.py decompresses `.xz`/`.gz` formulas before checking
(first run reported all 7 proofs "rejected" in 0.3 s — an impossible check
time; treat instant-verdict proof checks with suspicion).

## Honest notes

- 14 instances means one instance swings PAR-2 by ~7%: per the §8.1 caveat,
  compare per-instance medians across repeated runs; Tier C carries the
  statistically meaningful claims.
- SC 2025 main track is absent (no published CSV); the set skews 2024
  (11 of 14) + 2023 (3 of 14). Revisit alongside O9 (Tier D refresh) when
  SC 2026 data lands.
- The 5–30 s band was measured with proofs *off*; with DRAT logging on,
  runtimes are a few percent higher — the band classification is robust to
  that.
