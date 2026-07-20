# Tier B head-to-head — tactix vs Kissat 4.0.4 (2026-07-20)

**Frozen report.** A clean same-machine comparison of DefaultPolicy against the
pinned Kissat baseline, taken right after the E2 Factor duplicate-binary fix
landed. Establishes an honest current gap to measure future work against.

## Setup

- **Hardware:** Apple M3 Pro, 12 cores, macOS (arm64).
- **git:** `5462989` (Factor ON: `c0290da` + CPU-backstop `5462989`).
- **Tier:** B, 14 instances, 300 s timeout, **jobs = 4** (timing precision).
- **tactix:** `./build/release/tactix` (DefaultPolicy), proof-checked on every
  UNSAT (drat-trim). **kissat:** `tools/solvers/bin/kissat` (4.0.4), no proof
  logging (performance reference).
- Both solvers were run on an **uncontended** machine. (An earlier run this day
  was invalidated: 6 orphaned fuzz processes had pegged 6/12 cores for 13.5 h —
  see `tools/fuzz/known-hangs/` and the RLIMIT_CPU backstop in `5462989`. Conflict
  counts are contention-independent; wall-clock from before that fix is not
  trustworthy, which is why the older "~2× off" Tier C figure is superseded here.)

## Aggregate

| solver | PAR-2 | solved | SAT/UNSAT | proof check |
|---|---:|---:|---|---|
| **tactix** | **70.00** | 14/14 | 7 / 7 | 7 checked, 2601 MB, 822 s |
| **kissat** | **6.26** | 14/14 | 7 / 7 | — |

Both solve everything, so PAR-2 = mean solve time. **tactix is ~11× slower on
average.** The gap is **bimodal**, not uniform.

## Per-instance (sorted by tactix/kissat ratio)

| instance | verdict | tactix (s) | kissat (s) | ratio | lever |
|---|---|---:|---:|---:|---|
| clique_n2_k10 | UNSAT | 259.35 | 4.45 | 58× | raw search (1.6 GB proof) |
| hwmcc17miters-…-oski15a08b00s | UNSAT | 116.09 | 2.35 | 49× | miter — full inprocessing |
| battleship-14-26-sat | SAT | 222.10 | 5.21 | 43× | hard SAT search |
| mp1-blockpuzzle_5x10_s7_free4 | UNSAT | 158.82 | 7.38 | 22× | Factor (S2/S4) |
| combined-crypto1-…-wffvars-450 | SAT | 36.59 | 3.26 | 11× | SAT search |
| summle_X4044_steps7 | SAT | 25.90 | 4.49 | 5.8× | — |
| FmlaEquivChain_4_6_6 | UNSAT | 41.69 | 8.57 | 4.9× | Sweep (V5) |
| mod4block_2vars_10gates_u2 | SAT | 53.07 | 13.85 | 3.8× | — |
| x9-08014.sat | UNSAT | 22.34 | 8.26 | 2.7× | Factor (helped) |
| 9vliw_bp_mc.shuffled | UNSAT | 14.16 | 6.03 | 2.3× | — |
| square.2.0.i.smt2-cvc4 | UNSAT | 5.98 | 2.61 | 2.3× | — |
| rbsat-v760c43649gyes10 | SAT | 7.00 | 4.68 | 1.5× | — |
| circuit_48in64out_…_dist128 | SAT | 12.32 | 8.66 | 1.4× | — |
| **qwh.50.1250.shuffled** | SAT | **4.52** | 7.83 | **0.58×** | **tactix wins** |

## Trajectory (same Tier B list — we now solve all 14)

| date | solved | PAR-2 | what landed |
|---|---:|---:|---|
| 2026-07-11 | 7/14 | 322.9 | (baseline) |
| 2026-07-14 (T2 gate) | 11/14 | 164.8 | S4 SwitchMode + stable mode + reduce used-bit retention |
| 2026-07-19 | 13/14 | 77.75 | V6 congruence (cracked hwmcc-miters) |
| **2026-07-20 (now)** | **14/14** | **70.0** | E2 Factor + duplicate-binary fix; **clique now solved** |

In ~9 days: **7/14 → 14/14 solved**, **PAR-2 322.9 → 70.0 (4.6× better)**. On 07-19
`clique_n2_k10` was the lone permanent timeout (search-bound, no inprocessing lever
applies — kissat solves it congruence-off in 44 s); this run it finishes at 259 s,
so the whole set is now cleared. The ~11× gap to Kissat is therefore measured on a
set we *fully solve*, having gone from failing half of it. (clique's exact
07-19→07-20 crossing from TO→259 s is not firmly attributed — plausibly the clean
machine and/or this session's changes; it sits near the 300 s edge either way.)

## Interpretation

- **~Half the set is competitive** (≤ 3× — circuit, rbsat, square, 9vliw, x9),
  and tactix **beats** Kissat on `qwh`. Our core CDCL search is in the right
  ballpark; this is consistent with tactix beating factor-less Kissat on mp1
  (4.19 M vs 9.9 M conflicts) measured separately.
- **Four instances (clique, hwmcc, battleship, mp1) are 22–58× slower and are
  77 % of tactix's total time.** Drop them and the aggregate is ~3×. The gap is
  concentrated, not diffuse.
- The four outliers are the known levers: `clique`/`battleship` want stronger
  search; `hwmcc`/`mp1` want mature inprocessing (elimination + vivification +
  sweeping + a real SBVA Factor). No single missing rule closes it — it is
  Kissat's *accumulated* inprocessing + search tuning.

## Consequences / next levers (by leverage)

1. **E2 Factor S2 (SBVA selection) + S4 (large remainders)** — mp1 22× → target
   Kissat's 13× conflict cut (task #37). Factor is ON and net-positive now but
   binary-only.
2. **V5 Sweep** — `FmlaEquivChain` 4.9× (the profiled equivalence-chain lever).
3. **Search quality on clique/battleship** — the residual raw-search gap; no
   inprocessing lever applies to `clique` (it emitted a 1.6 GB proof = enormous
   conflict count).

This is the baseline to beat. Re-run this exact set (same tier list, jobs = 4,
uncontended) after each lever lands and compare the per-instance ratios.
