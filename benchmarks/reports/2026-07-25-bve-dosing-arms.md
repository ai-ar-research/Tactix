# BVE dosing arms — Tier B/C matrix + oracle headroom (2026-07-25, frozen)

The I8-shaped gate-validity measurement owed by `docs/design/bve-tolerance-plan.md`
(adapted: the cadence arm from the I2/I3 mechanism work replaces the plain
density=0 arm as the interesting treatment; the plain drip numbers live in the
probes cited by the mechanism report). All arms same-window per tier, identical
allocation shape (cpu-dense-hwulab-q, 1 core / 8 GB, no throttle), solver under
test with proof checking in every arm. Binary at `5d9269d`.

**Standing user baseline (2026-07-25, recorded during this study):** a
configuration that turns a proof rule fully off is NEVER a candidate default —
rule-off arms are diagnostics that localize *scheduling defects*, and the
scheduling is what gets fixed. `bve-off` below is therefore a control, not a
candidate, and the same framing applies to hard suppressors like the density
gate (already a named phase-6 subsumption target).

## 1. Tier-level matrix (PAR-2)

| arm | Tier B (14) | Tier C (30) |
|---|---|---|
| shipped (drip doses + density gate) | **10.92** (14/14) | 528.66 (26/30) |
| cadence (`TACTIX_BVE_CADENCE=1`, gate on) | 11.97 (14/14) | — |
| cadence-open (cadence + `TACTIX_BVE_DOSE_DENSITY=0`) | — | 524.55 (26/30) |
| bve-off (`TACTIX_SLOT_DISABLE=BVE`, **control only**) | 11.57 (14/14) | **516.55** (26/30) |
| **oracle best-of-three (C)** | | **489.27** |

Runs: B `20260725-102958/102959/111932-B-*`; C `20260725-103903/103932/104001-C-*`.
The four standing C TOs (SCPC-1000-18, 128_125, T105, T87) time out in **every**
arm — the T3 TO residual is not a dosing question.

## 2. Tier C per instance (1200 s timeout; PAR-2 cell = time, TO = 2400)

| instance | verdict | shipped | bve-off | cadence-open |
|---|---|---:|---:|---:|
| 128_125 / SCPC-1000-18 / T105 / T87 | ? | TO | TO | TO |
| StConn_8_32 | UNSAT | 692.1 | 660.0 | 734.0 |
| Grain_no_init… | UNSAT | 582.9 | 429.7 | **373.5** |
| sat-bench-trig-bhaskara | UNSAT | 565.4 | 547.8 | 610.1 |
| goldcrest-and-9 | UNSAT | 435.8 | 436.1 | **288.9** |
| minxor128 | UNSAT | 363.1 | 357.3 | **255.2** |
| constraints_25_4_5_12… | UNSAT | 362.9 | 367.3 | **215.8** |
| constraints_18_0.3_2 | UNSAT | 362.2 | 359.4 | 392.0 |
| asconhashv12… | UNSAT | 320.9 | 324.6 | 329.7 |
| frb45-21-2 | SAT | 286.3 | 226.4 | 282.7 |
| circuit_32in32out… | SAT | 265.3 | 266.4 | 276.7 |
| ecarev-110-4099-22-30-7 | SAT | 259.6 | 259.3 | 238.1 |
| mod2c-rand3bip-sat-250-2 | SAT | 252.8 | 249.7 | 375.7 |
| php17-mixed-35pct | UNSAT | 217.8 | 173.3 | 196.2 |
| ER_400_20_4 | UNSAT | 188.3 | 191.8 | 191.1 |
| em_11_3_4_cmp | SAT | 181.6 | 177.7 | **48.5** |
| urq45 | UNSAT | 170.8 | 167.1 | 154.3 |
| jgiraldezlevy.2200.9086 | SAT | 135.6 | 133.3 | **11.0** |
| ContextModel_output_7_3_9 | SAT | 116.3 | 116.3 | 352.5 |
| j3037_9_rggt_b | SAT | 107.6 | 107.0 | **51.3** |
| aes_decry_2_rounds | UNSAT | 95.2 | 98.8 | 124.7 |
| af-synthesis_stb_50_120_4 | SAT | 94.3 | **38.2** | 156.0 |
| DLTM_twitter690_74_16 | SAT | 76.7 | 77.3 | 87.0 |
| Ptn-7824-b18 | SAT | 60.0 | 61.9 | 323.8 |
| EDP3-11000 | SAT | 36.9 | 40.4 | 35.5 |
| 1-ET-256-K-70 | UNSAT | 25.9 | 25.8 | 24.8 |
| mp1-Nb5T15 | SAT | 3.6 | 3.7 | 7.4 |

Cadence-open is the best arm on **11/30** instances; proof volume grows under
it (10.28 GB / 5218 s check vs shipped 8.16 GB / 4018 s — goldcrest alone
1907 s, within its 2× budget; the §5.3 proof-size risk stays live).

## 3. Tier B per instance (deltas vs shipped)

Cadence (gate on): sole real mover **x9 +10.09** (12.61→22.70; near-identical
elimination work — timing perturbation on the TB2-chaotic instance, probe
1562469); all others ±1 s. Bve-off control: loses **x9 +14.01** and
**9vliw +5.00**, wins FmlaEquiv −3.19, qwh −2.58, rbsat −2.48, mod4block
−0.24; net 11.57 vs 10.92.

## 4. Findings

1. **The three static arms are within ≈12 C PAR-2 points** — and the shipped
   density-gated drip does NOT beat the bve-off control on Tier C (528.66 vs
   516.55). L9's "−32 C" was measured against dose-everywhere-drip, not
   against off; the gate's C-side value does not replicate against the
   stronger control.
2. **The oracle over three static schedules gains ≈39 C PAR-2 points**
   (489.27 vs 528.66) with 7 instances swinging >100 s each way — the
   concrete, same-window quantification of the §5.6 elimination-dosing
   exhibit's headroom. Four instances, four best schedules (crypto1/em_11 →
   cadence; x9/9vliw → shipped drip; af-synthesis/php17/frb45 → no doses;
   mod4block → no elimination by anyone, L10).
3. **Scheduling-defect decomposition** (user baseline: these are defects to
   fix, not rules to disable):
   - *Drip shape* (proved by I2): budget-cut per-phase drips are the crypto1
     damage class; completion+spacing (cadence) fixes it where the formula
     rewards elimination (Grain, goldcrest, minxor128, constraints_25,
     em_11, jgiraldezlevy, j3037, urq45, ecarev).
   - *Admission*: cadence-open activates dosing on instances the shipped
     gate kept quiet, and several are hurt badly (Ptn +264, ContextModel
     +236, mod2c +123). The gate currently suppresses winners and losers
     alike (em_11/jgiraldezlevy were false positives; Ptn/ContextModel are
     true positives).
   - *No static CNF feature separates them* (L5 held again): the next lever
     is RUNTIME feedback — per-call elimination yield / post-call trajectory
     — measured by the follow-up yield probes (array 1562578).
4. Tier B still pays for BVE only through x9 (chaotic) and 9vliw; FmlaEquiv/
   qwh/rbsat would each be 2.5–3.2 s faster without the current doses —
   B-side scheduling headroom exists too.

## 5. Routing

- Fix direction (user directive): a **universal BVE schedule with runtime
  feedback** — cadenced completed calls plus a productivity/backoff signal
  that quiets the Ptn/ContextModel/af-synthesis class without a static gate,
  targeting ≥ bve-off on every measured instance. Yield probes (1562578)
  supply the discriminating signal candidates.
- The density gate stays shipped UNTIL the feedback schedule beats it on
  B+C arrays (finding-3 discipline); knobs `TACTIX_BVE_ONESHOT` /
  `TACTIX_BVE_CADENCE` remain A/B arms.
- Phase-6 exhibit gains this report's oracle table as its baseline data.
