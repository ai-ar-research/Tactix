# battleship-14-26-sat: toolchain trajectory divergence → the density gate was the disease

**Date:** 2026-07-24 (overnight session). **Git:** det-fix landing (see PROGRESS
Log). **Hardware:** cluster `cpu-dense-hwulab-q`, job-array protocol (plan §8.1
rev 64), one core + 8 GB per run. **All runs this report cites are same-cluster,
same-binary-family (gcc-13/libstdc++) unless marked M3.**

## Symptom

After the 2026-07-23 migration (new machine AND new toolchain), battleship went
14.1 s / 1.95 M conflicts (M3, Apple clang/libc++, P2-era pin) → ~300 s TO at
27.1 M conflicts (probe 1559098). Tier B: 13/14.

## Diagnosis ladder (probes, 900 s caps)

1. **Cross-STL tie-order instability found and fixed** — `analyze.cpp`'s
   per-conflict block sort had no tie-break (equal-level literals in
   `std::sort`'s implementation order → watched literals → whole trajectory
   forks between libstdc++/libc++); congruence's ITE/XOR extraction iterated
   `unordered_map`s (gate order → closure merge order). Both determinized
   (total-order tie-break / sorted-keys iteration). **Necessary — but not the
   battleship lever:**
2. **Every canonical tie order is the same stuck basin** on the raw formula:
   orders A=(level,trail desc) / B=stable / C=(level,trail asc): all 900 s TO at
   71.5–75.1 M conflicts, **0 learned units**, ~195 mode switches (probes
   1559440/674/675). Chrono-off × 2 orders: identical, and `chrono 0` — chrono
   never fires here (1559691/92). The M3 14.1 s was **basin luck**, not parity.
3. **kissat ablation (Step 3):** kissat 6.6 s default; `--eliminate=false`
   5.2 s; `--chrono=false` 2.6 s — **no single pass is load-bearing; kissat
   never searches the raw formula**: it **factors 182 vars (50 %, 5
   factorization rounds) + eliminates 104 (29 %)** (probe 1559693).
4. **Our Factor fired 0 factorings — the `density = 8` short-circuit closed the
   gate.** battleship's binary density ratio is in **[6, 7)** (bracket probe
   1559695); the overfit magic-8 (shipped 2026-07-21 to protect mod4block,
   ratio 4.1) excluded it by less than one unit.
5. **Open the gate → solved.** `TACTIX_FACTOR_DENSITY≤6`: **78 factorings →
   SAT in 175 s / 12.4 M conflicts** (probes 1559694/95). BVE ramp-16 stacked
   on top adds nothing (identical trajectory, still 0 eliminations —
   kissat's eliminations come from *interleaved* factor/eliminate rounds, the
   S4 arrangement, not from a bigger single-blast bound).

## Verdict (the four causes)

**Missing rule application** — Factor exists but was gated off exactly here.
Not engine speed (identical binary solves it when Factor fires), not chrono,
not tie order (though the tie order HAD to be determinized first to make any
of this reproducible across toolchains — that fix stands on its own).

## Fix landed

- Determinism: total-order tie-break in the analyze block sort (level desc,
  trail desc — allocation-free; `stable`/`asc` measured equivalent-in-noise on
  Tier B: A 62.96 / B 60.07, battleship TO under all three), sorted-keys
  iteration in congruence extraction. Cross-STL reproducibility is now by
  construction; fuzz 18 k cases 0 failures (quick 10 k, random-policy+LRAT 4 k,
  congruence-stress+LRAT 4 k).
- **Factor density default 8 → 6** (same stopgap class as 1→8; the effort
  scheduler still supersedes it, plan rev 53): battleship in, mod4block (4.1)
  still out. Gate: Tier A + Tier B arrays (numbers added below on landing).

## Residual (honest)

175 s vs kissat 6.6 s (~26×). The remainder is the **S4 full arrangement**
(interleaved factor/eliminate rounds — kissat runs 5, we run 1) plus the
absent V5 Sweep / phase-4 Walk. battleship joins mp1 as S2a-Increment-3 /
S4 evidence. 12.4 M conflicts with 0 learned units on a SAT puzzle also
stays noted as a search-quality observation (TB-class).

## Gate results

- **Tier B (density=6, det-fix binary): 14/14, PAR-2 32.23** (kissat same
  window 8.28) — from 13/14 @ 61.78 pre-fix / 62.96 det-only. Per-instance:
  ONLY battleship moved (TO → 184.5 s); the other 13 within noise of their
  density-8 twins (runs `20260724-000206-B-dens6-tierB` vs
  `20260723-232801-B-det-tierB`).
- **Tier A (density=6): 199/200, PAR-2 0.78** — unchanged (s055 = the known
  TB5 throughput miss; the gate still excludes random 3-SAT by ~an order of
  magnitude of density ratio).
- ctest 31/31 both presets; fuzz on the det-fix code: quick 10 k / 0,
  random-policy+LRAT 4 k / 0, congruence-stress+LRAT 4 k / 0.
