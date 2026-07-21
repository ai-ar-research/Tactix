# BVE on/off A/B across Tier B + C — the flip is dead, BVE stays ON (2026-07-20)

Frozen report. Gates the "default BVE off" decision proposed in
`2026-07-20-bve-bimodal-profiling.md`. Runner: `tools/bench/run.py`, one arm at a
time, jobs=4, same binary (`28593a6`, `TACTIX_NO_BVE` toggling BVE).

## Result: keep BVE ON

| set | BVE on | BVE off | verdict |
|---|---|---|---|
| **Tier C** PAR-2 (solved) | **366.11 (28/30)** | 507.03 (**26/30**) | BVE **on** wins — +2 solves, better PAR-2 |
| **Tier B** PAR-2 (solved) | 42.83 (14/14) | **23.30 (14/14)** | BVE **off** wins — 1.84×, same solves |

BVE is **net-positive on Tier C** (it enables solves) and **net-negative on Tier
B** (PAR-2 only, no solves lost). Solved count outranks PAR-2, so **the simple
default-off flip is off the table** — it would forfeit 2 Tier C solves. The
historical "+3 Tier C solves" justification for enabling BVE is confirmed live.

## Which Tier C solves BVE is load-bearing for (lost when off)

| instance | BVE on | BVE off |
|---|---|---|
| mp1-Nb5T15 | SAT 1 s | **UNKNOWN (timeout)** |
| Ptn-7824-b18 | SAT 161 s | **UNKNOWN (timeout)** |

`mp1-Nb5T15` is the sharpest: 1 s with BVE, unsolvable in 1200 s without. Zero
instances solve with BVE off that don't solve with it on.

## Where BVE is a large PAR-2 sink (both arms solve)

| instance | tier | BVE on | BVE off | effect |
|---|---|---|---|---|
| clique_n2_k10 | B | 221.9 s | 5.7 s | BVE hurts 38.7× |
| rbsat-v760… | B | 8.5 s | 0.58 s | BVE hurts 14.7× |
| jgiraldezlevy.2200… | C | 430.0 s | 66.6 s | BVE hurts 6.5× |
| hwmcc17miters… | B | 123.8 s | 44.7 s | BVE hurts 2.8× |
| ContextModel_output… | C | 101.8 s | 23.5 s | BVE hurts 4.3× |
| — | | | | |
| DLTM_twitter690… | C | 9.4 s | 98.1 s | BVE **helps** 10.5× |
| mod4block / crypto1 / qwh | B | — | — | BVE **helps** 2.3–2.8× |

aes_decry is a wash (Factor-driven proof-check timeout in both arms; see
`bve-bimodal-profiling.md` update — not a soundness bug).

## Routing: the lever is a STRUCTURAL SKIP, not a switch

The flip is dead, but the harmful instances (clique, rbsat, jgiraldezlevy,
ContextModel, hwmcc) are large PAR-2 sinks BVE-on pays. The prize is a predicate
that **skips BVE on the harmful structure while keeping it where it is
load-bearing** (mp1-Nb5T15, Ptn-7824, DLTM_twitter, mod4block/crypto1/qwh) —
best of both: hold 28 Tier C solves AND cut the Tier B/C PAR-2.

Known hard: the naive signals (bound-abort ratio, completion fraction) do NOT
separate harmful from helpful — mod4block (helps) looks like clique (hurts) on
those. The predicate must key on **instance structure** (candidate: dense,
near-uniform-degree binary/cardinality graph — clique is literally every-var
p=1/n=35). Owner: **policy** slot scheduling (N1: rule guard stays pure).
Prototype the predicate OFFLINE against the harmful vs load-bearing instances
before any commit; do not ship a speculative heuristic.

Until then **BVE stays ON by default** (unchanged). `TACTIX_NO_BVE` remains the
diagnostic escape hatch.
