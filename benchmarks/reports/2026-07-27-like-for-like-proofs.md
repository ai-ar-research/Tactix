# Like-for-like standings: every solver timed WITH proof generation

**Date:** 2026-07-27 (late) · **git:** `d0bb6ec` (measurements taken at
`24afa87`/`61138b8`-era binaries; see each window) · **Plan:** rev 72 §8.1
proof-generation protocol · **Author's directive:** user, 2026-07-27 — "compare
with Kissat/CaDiCaL with proof generation. And you should also do proof
generation."

**Allocation shape:** SLURM `cpu-dense-hwulab-q`, 1 cpu / 8 GB per task,
`--throttle 64`, one array per tier (candidate and every baseline in the SAME
window). Numbers here are NOT comparable to the unthrottled `20260727-0922*`
gate windows (L6: compare only across identical shapes).

## 1. What changed and why

Until rev 72 our baselines were timed **without** proof logging while tactix
wrote a DRAT proof on every run. That compares two different workloads. It also
turned out to matter in both directions, so neither the old numbers nor a naive
correction is the honest answer — the tax has to be measured per solver.

Two asymmetries are kept deliberately, and both are stated wherever these
numbers appear:

1. **Scoring.** A failed/missing proof scores *our* instance UNSOLVED. A
   baseline's failed check is **recorded, never scored against it**.
2. **Checker budget.** `max(3600 s, 4 × solve timeout)`, up from `2 ×`.

Asymmetry 1 is not politeness — it is the difference between a true and a false
headline. See §3.

## 2. Tier B (`20260727-212407-B-tactix-drat`, 14 instances, timeout 300 s)

| solver | PAR-2, proofs ON | PAR-2, proofs OFF | logging tax |
|---|---|---|---|
| tactix | 7.96 | 7.44 | **+6.9 %** |
| kissat 4.0.4 | 8.72 | 8.35 | +4.4 % |
| CaDiCaL | 20.06 | 19.10 | +5.0 % |

| comparison | ratio vs kissat |
|---|---|
| both writing DRAT | **0.91×** |
| both proof-free | **0.89×** |
| the old convention (us logging, kissat not) | 0.95× |

All 14/14 solved by all three. On Tier B the honest protocol **helps** us: the
old number understated our standing by ~5 %.

Checker cost, same window: our 255 MB of certificates verified in 104 s;
kissat's 221 MB took 269 s; CaDiCaL's 334 MB took 345 s.

## 3. Tier C (`20260727-212331-C-tactix-drat`, 30 instances, timeout 1200 s)

**As the runner scored it under the OLD budget** — the trap this report exists
to document:

| arm | PAR-2 | solved |
|---|---|---|
| tactix-drat | 319.87 | 29/30 |
| kissat-drat | 374.87 | 27/30 |
| cadical-drat | 505.25 | 26/30 |

That reads as tactix beating kissat by 55 PAR-2. It is an artefact: **2 kissat
proofs and 3 CaDiCaL proofs exceeded the 2400 s checker budget**, and the old
runner scored those instances unsolved. Every one of them was solved.

**Scored honestly** (baseline check failures recorded, not scored):

| arm | PAR-2 | solved | vs kissat |
|---|---|---|---|
| tactix, DRAT | 319.87 | 29/30 | **1.41×** |
| kissat, DRAT | 227.47 | 29/30 | 1.00× |
| CaDiCaL, DRAT | 319.19 | 29/30 | 1.40× |
| tactix, proof-free | 311.12 | 29/30 | 1.38× |
| kissat, proof-free | 226.19 | 29/30 | 1.00× |
| CaDiCaL, proof-free | 309.16 | 29/30 | 1.37× |

**Proof-writing tax on Tier C: tactix +2.8 %, kissat +0.6 %, CaDiCaL +3.2 %.**

So on Tier C the like-for-like protocol **costs us** (1.38× → 1.41×), the
opposite of Tier B. Both directions are reported; neither is selected for.

Against CaDiCaL we are at parity in both columns (1.00× with proofs, 1.01×
without).

## 4. The one place we are clearly ahead: certificate cost

| arm | proof bytes | check time | proofs over the 2400 s budget |
|---|---|---|---|
| tactix | 7 650 MB | 5 231 s | **0 of 15** |
| kissat | 9 926 MB | 13 498 s | 2 of 15 |
| CaDiCaL | 12 058 MB | 13 583 s | 3 of 15 |

Our certificates are the smallest and by far the cheapest to verify — 2.6× less
checker time than either reference on the same 15 UNSAT instances. The
organizers have floated combining solver+checker time (plan §2); on that metric
this window would read very differently from the PAR-2 column.

## 5. Native LRAT is not the scored path (measured, `20260727-204153-C-prooffmt`)

| | DRAT + drat-trim | native LRAT + cake_lpr |
|---|---|---|
| Tier C PAR-2 | 300.72 | 406.13 |
| Tier B PAR-2 | 7.77 | 10.26 |
| Tier C proof bytes | 7.65 GB | **86.4 GB** |
| Tier C check time | 4 468 s | 2 887 s |

LRAT logging costs 15–30 % of solve time and 4–28× proof size for a checking
speedup that **reverses on the largest certificates** (goldcrest 434 s → 573 s,
ascon 78 s → 291 s). It stays the escalation path for proofs drat-trim cannot
handle in budget (`urq45`: drat-trim 1447 s vs cake_lpr 77 s), not the default.

This same array is what exposed the N2 defect fixed in `716948d` — see
PROGRESS L13.

## 6. Honest caveats

1. **Single sample per arm.** The solver is deterministic, so these are exact
   properties of these instances, not estimates — but per-instance spread is
   large and generalization beyond Tiers B/C is unproven.
2. **Throttle 64 vs the unthrottled gate windows.** tactix reads 319.87 here
   and 303.21 in `20260727-092228-C-t3gate2`. Do not mix the two; within this
   window every arm shares the shape.
3. **The T3 gate is not re-judged here.** The gate's rule-neutral subset was
   derived in the unthrottled window; re-deriving it under the new protocol is
   the next gate run, not this report.
4. **CaDiCaL's binary DRAT** is its default; no proof-format flag was passed to
   either baseline beyond the proof path itself.
