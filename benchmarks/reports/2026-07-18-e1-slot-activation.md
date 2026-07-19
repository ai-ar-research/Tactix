# E1 slot-activation assessment (phase-3 exit, T3 direction)

**Date:** 2026-07-18 **HEAD:** 9a51ec7 (E1 complete: E1a–E1d)
**Hardware:** Apple M3 Pro, 12 CPU, 36 GB **Concurrency:** jobs=4
**Tier:** B (frozen 2026-07-09, 14 SC-2023/2024 instances), timeout 300 s
**Question:** now that E1 (BVE) has landed — the trigger the C2 deferral named
("the slot enters the default schedule when V1/E1 give it cross-class payoff") —
is it net-positive to put the inprocessing slot on the DefaultPolicy schedule
(`SLOT_IN_DEFAULT`)?

## Setup

- Baseline = `SLOT_IN_DEFAULT=false` (current main). Slot builds flip it true.
- Slot compositions swept via temporary env scaffolding (discarded, not merged):
  `TACTIX_SLOT` ∈ {default = {backbone,bve,probe,strengthen,vivify}, `bve`
  = BVE only}; `TACTIX_SLOT_MULT` = inprocessing-interval multiplier.
- Tier A cleared first (sequencing rule): baseline **200/200, PAR-2 0.52**.

## Results (Tier B, PAR-2 = mean(runtime if solved else 2×timeout))

| solver | PAR-2 | solved | note |
|---|---|---|---|
| **baseline (slot off)** | **116.2** | **12/14** | reference |
| slot full, mult=1 | 157.6 | 11/14 | net-negative |
| slot full, mult=4 | 156.8 | 11/14 | net-negative |
| slot BVE-only, mult=1 | 157.5 | 11/14 | net-negative |
| kissat 4.0.4 | 6.2 | 14/14 | reference |
| cadical | 11.6 | 14/14 | reference (T3 target) |

Per-instance (seconds; `TO` = 300 s timeout):

| instance | exp | base | bve-only | full m1 | full m4 |
|---|---|--:|--:|--:|--:|
| hwmcc17miters-xits-iso-oski15a08b00s | UNSAT | TO | TO | TO | TO |
| clique_n2_k10 | UNSAT | TO | TO | TO | TO |
| mp1-blockpuzzle_5x10_s7_free4 | UNSAT | 191 | 221 | 241 | 209 |
| circuit_48in64out_800gates | SAT | 65 | 70 | **9** | 14 |
| mod4block_autoenc-sc2009 | SAT | 46 | **5** | 38 | 11 |
| x9-08014 | UNSAT | 35 | 36 | 42 | 39 |
| FmlaEquivChain_4_6_6 | UNSAT | 27 | 32 | 20 | 19 |
| combined-crypto1-wff | SAT | 20 | 15 | 17 | 57 |
| **battleship-14-26-sat** | **SAT** | **18** | **TO** | **TO** | **TO** |
| summle_X4044_steps7 | SAT | 12 | **5** | 11 | 27 |
| 9vliw_bp_mc | UNSAT | 8 | 10 | 15 | 11 |
| square.2.0.i | UNSAT | 2 | 2 | 2 | 2 |
| qwh.50.1250 | SAT | 1 | 1 | 1 | 1 |
| rbsat-v760 | SAT | 1 | 8 | 11 | 5 |

## Findings

1. **Naive activation is net-negative on Tier B** (~157 vs baseline 116) in every
   composition and interval tried. The gap is dominated by **one lost SAT
   instance**: `battleship-14-26-sat`, 18 s at baseline → **timeout in every slot
   config**. Remove that one instance and BVE-only would be ≈ baseline or better.

2. **The blocker is inprocessing disrupting a productive SAT search, not a broad
   slowdown.** Causally confirmed: with the BVE slot effectively disabled
   (`TACTIX_SLOT_MULT=1000`), battleship solves in **13.8 s SAT**; enabled, it
   does not finish in 90 s. It is **interval-independent** (mult=1 and mult=4 both
   time out), so even an early inprocessing round derails it — the composed
   root-restart-to-0 (which gives the slot a quiescent root) repeatedly discards a
   near-complete assignment. This is the classic "inprocessing hurts SAT" hazard.

3. **Inprocessing genuinely helps structured instances** — the payoff the deferral
   was waiting for is real: `circuit_800gates` 65→9 (full suite), `mod4block`
   46→5 and `summle` 12→5 (BVE-only). But:
   - **BVE alone is not the payoff.** BVE-only does *not* crack the circuit
     (65→70); the circuit win needs the full V-rule suite. So E1's landing is
     necessary context but not itself the cross-class lever on this set.
   - The two hard UNSAT miters (`hwmcc17miters`, `clique_n2_k10`) time out in
     **every** config — inprocessing does not crack them within 300 s.

## Decision — DEFER activation (mechanism recorded)

`SLOT_IN_DEFAULT` stays `false`. Activation is net-negative on Tier B and, more
importantly, **loses a SAT instance the baseline solves**, in every composition
and at every interval. This is not a "close, just tune the interval" situation —
the interval does not fix it.

The slot is nonetheless *close*: the wins are real and concentrated, and the loss
is a single, well-characterized failure mode. The gating work is **phase-4**, not
a knob:

1. **SAT-safe inprocessing scheduling** (the direct blocker): don't derail a
   productive SAT search — gate inprocessing on stable mode / an effort or time
   budget per round, or reconstruct the trail after a pass instead of the full
   root restart. Re-measure battleship as the canary.
2. **Diagnose battleship precisely**: is it the root-restart or BVE's formula
   change? (A "root-restart-only, no rule" control isolates it.)
3. **The hard UNSAT miters** need their own look — likely want the currently
   default-off rules (SubsumeForward / TransitiveReduce / Substitute) or a
   stronger BVE (gates on the elimination order), not just the current five.

## Scope note

This is **not** a T3 evaluation. T3 (within 10 % PAR-2 of CaDiCaL on Tier C) is a
Tier-C gate; on Tier B the baseline (116) is ~10× CaDiCaL (11.6), dominated by
instances no config here solves. T3 remains far off and is not the purpose of
this assessment — the deliverable is the adopt/defer decision, decided: defer.
