# Inprocessing revamp — activating DefaultPolicy simplification

**Date:** 2026-07-19 **Hardware:** Apple M3 Pro, 12 CPU, 36 GB
**Branch:** worktree-inproc-safe (off main 604831b) **Concurrency:** jobs=4
**Design:** `docs/inprocessing-revamp-design.md`

## What landed

DefaultPolicy now runs a **SAT-safe bounded simplification phase** at conflicts=0
(preprocessing, before search state exists): **BVE + Strengthen + SubsumeForward**
in the kissat eliminate-round structure (a few outer rounds; per-variable BVE runs
to a global budget, the batch simplifiers run once per outer round). BVE reads a
new **shared dense occurrence index** (`Kernel::dense`, O(occ) per elimination);
the batch rules keep it consistent through the kernel add/GC hooks. `SLOT_IN_DEFAULT`
is now `true`. The probing V-rules (Probe/Vivify/Backbone) were measured and
**deferred** — net-negative (they disrupt SAT search) without effort budgets.

## Tier B (14 instances, 300 s)

| config | PAR-2 | solved |
|---|--:|:--:|
| baseline (slot off) | 116.2 | 12/14 |
| **revamp (BVE+Str+Sub)** | **111.6** | 12/14 |
| revamp + V-rules (deferred) | 169.4 | 11/14 |
| *kissat / cadical* | *6.2 / 11.6* | *14/14* |

Net-positive (−4.6) but modest: structured wins (circuit 65→10, mod4block 46→9)
against small-instance overhead. **Tier B under-represents inprocessing** — its
5–30 s band is where simplification cost shows and payoff doesn't.

## Tier C (30 instances, 1200 s) — the decisive tier

| solver | PAR-2 | solved | notes |
|---|--:|:--:|---|
| **CaDiCaL (clean, this session)** | **160.6** | 30/30 | the real reference |
| **revamp (inc3r)** | **356.2** | **28/30** | |
| tactix-base (slot off, this session) | 552.5 | 25/30 | inflated by urq45 proof-check contention |
| *tactix-base (pinned 07-15)* | *406.6* | *28/30* | contention-caveated |
| *CaDiCaL (pinned 07-15)* | *336.96* | | **contention-inflated — do not use** |

**The revamp is strongly net-positive on Tier C**: it **solves 3 instances the
baseline times out on** (a contention-independent result) — `urq45`,
`circuit_32in32out`, `Ptn-7824` — plus large speedups (`Grain` 654→375, `frb45`
161→19, `af-synthesis` 136→9). The headline: `urq45` (Urquhart parity) goes from
**1055 s + an uncheckable 4.38 GB proof** (base) to **86 s + a 0.53 GB proof that
verifies** (revamp) — inprocessing both accelerates it and shrinks the certificate
8×. It also has losses (`jgiraldezlevy` 62→512, `T87` 242→546): the SAT-disruption
pattern persists, but the UNSAT wins dominate.

## T3 gate — NOT met (correcting the pinned target)

The pinned T3 references were measured under contention and are **unreliable**:
clean CaDiCaL this session is **160.6**, not the pinned 336.96 (2× inflation).
Against clean CaDiCaL the revamp (356) is **~2.2×**, well outside the T3 band
(within 10%). **T3 remains an open phase-3 goal.** The gap is dominated by the
2 remaining timeouts (`hwmcc-miters`, `clique` — which kissat/cadical crack via
probing/vivification, i.e. the deferred V-rules) and a broader search-quality gap.

## Soundness & regression

- DefaultPolicy DRAT 1000/0 + native-LRAT 1000/0; random-policy native-LRAT
  2000/0 (N1, arena path); 27/27 unit tests. No mismatches, no proof rejections
  (`urq45`'s 4.38 GB baseline proof is a resource limit, not a solver bug).
- Tier A: **200/200, PAR-2 0.49** (baseline 0.52) — no regression.
- s055 bit-exactness is intentionally retired (DefaultPolicy now runs BVE);
  `demo3` relaxed to §8.4 within-noise (dense makes bit-exact API reproduction
  infeasible; N1 applicability preserved).

## Verdict

Adopt. The revamp is a sound, real improvement to DefaultPolicy (+3 solved on
Tier C, urq45 rescued, −12% to −35% PAR-2 vs baseline) that finally makes
inprocessing net-positive. It does **not** reach T3 — that needs the V-rules made
SAT-safe (per-technique effort/tick budgets + mode gating) and further search
tuning, tracked as phase-3/4 follow-ons.
