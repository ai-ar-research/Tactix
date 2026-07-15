# Tier C freeze — 2026-07-15

- **Hardware (reference machine, O1):** Apple M3 Pro (6P+6E), 36 GB RAM,
  macOS 15.7.7. Freeze timings at **jobs = 1** (sequential), kissat 4.0.4
  (pinned `rel-4.0.4`), 360 s cap (band edge 300 s + margin).
- **Definition (plan rev 23):** ~30 SC instances, Kissat runtime ≤ 300 s
  locally; our solver's timeout 3000 s (10× headroom). Weekly driver +
  the T3 gate set (within 10% PAR-2 of CaDiCaL).

## Method

1. **Pre-filter from published SC results** (Tier B method): SC 2023
   (`Kissat_MAB_prop` runtimes) + SC 2024 (`kissat-sc2024`) detailed main
   CSVs, 712 unique verified instances; proxy band [8, 600] s (cluster ≈
   2× slower than the M3 Pro brackets local [5, 300]); the 14 Tier B
   instances excluded (freeze decision: the tiers stay disjoint — B's
   signal is already collected on every meaningful change). 307 in-band.
2. **Candidate fetch:** family-capped selection of 154 (74 SAT / 80
   UNSAT, 61 families) fetched from GBD by hash; a stratified 60-candidate
   subset (60 families, spread over the band) was timed locally — sized
   2× the rev-23 target instead of sweeping all 154.
3. **Local sweep:** 60 runs, sequential, 360 s cap. 55 reached verdicts —
   **all 55 agree with the competition verdicts** — 5 timed out
   (cluster/local variance, as in the Tier B freeze); 42 landed in the
   local [5, 300] band.
4. **Selection:** one instance per family (median in-band representative
   where a family offered several), then the 15 slowest SAT and 15
   slowest UNSAT family picks.
5. **Proof-checkability screen (first validation run):** two
   correct-UNSAT picks produced DRAT proofs of 513 MB and 5.4 GB that
   blew the runner's 6000 s drat-trim cap (`crafted_n12_d6_c4_num23`,
   `hash_table_find_safety_size_27`). Proof checkability is first-class
   (plan §2: unverifiable = unsolved), so both were swapped for the next
   in-band UNSAT family picks. Final: **30 instances, 15 SAT / 15 UNSAT,
   30 distinct families**, kissat 30.7–260.0 s, total 3369 s ≈ 56 CPU-min
   at jobs=1. Frozen in `benchmarks/tiers/C.list` (+ `C.sources` GBD
   hashes; `fetch.py --tier C` rebuilds the cache bit-for-bit).

## Validation run (runner end-to-end, jobs = 4)

`tools/bench/run.py --tier C --solver tools/solvers/bin/kissat --jobs 4`
(`benchmarks/runs/20260715-050141-C-kissat`): **PAR-2 128.30, 30/30
solved (15 SAT / 15 UNSAT), all 15 DRAT proofs verified** — 9.9 GB of
proofs, 12811 s of drat-trim. Every verdict matches the frozen list.
This run is the kissat jobs=4 reference for future same-concurrency
comparisons; note the checking bill (~3.6 h) dominates a Tier C
proof-checked run — schedule weekly runs accordingly, and expect native
LRAT to cut it (the plan §2 bet, measurable here).

## Honest notes

- **30 instances ⇒ one instance moves PAR-2 by ~3%** (rev 23's deliberate
  size/cost trade); per-instance medians over repeated runs remain the
  claim standard (§8.1), and Tier D stays the only statistically strong
  set.
- The set skews SC 2024 (as did Tier B); SC 2025 publishes no CSV —
  revisit with O9 (Tier D refresh) when SC 2026 data lands.
- The 360 s sweep cap right-censors: instances kissat solves in
  (300, 360] s were observed but excluded by band; instances beyond the
  cap are unknown-time, not misclassified.
- Family cap: the plan's ≤ 3 was never needed — one-per-family was
  feasible at target size (better diversity than Tier B's 2-per-family
  could achieve at its size).
- macOS has no `timeout(1)`; the sweep's first attempt failed with
  ERROR127 across the board and was rerun via a Python
  `subprocess.run(timeout=)` wrapper — instant-uniform sweep results are
  the tell (same lesson as the Tier B freeze's instant proof checks).
