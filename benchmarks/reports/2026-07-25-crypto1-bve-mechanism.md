# crypto1 BVE mechanism — during-search elimination damage decomposed (2026-07-25, frozen)

Closes the "crypto1 mechanism study owed" debt (commit `8a6e7ec`; S4 gate
report residual). Executes increments **I1 + I2** of
`docs/design/bve-tolerance-plan.md` under its binding reviewer findings:
every decisive comparison below is a same-window in-probe arm (finding 4 /
L6), and the finding-6 misreading of the control's mp1 arm is corrected here
before freezing. Probes cited (committed under `benchmarks/probes/`):
`1561949–52` (the 06:44 control batch), `1562429/1562430` (I1),
`1562431/1562434` (I2), fuzz logs `1562432/1562433`.

## 1. The `--no-eliminate` control triad (probe 1561949; corrected reading)

| instance | kissat default | kissat `--no-eliminate` | verdict |
|---|---|---|---|
| mod4block | 18.47 s | 4.46 s | kissat pays **4.14× WITH** elimination → damage is **instance-intrinsic** |
| crypto1 | 4.13 s | 9.22 s | kissat **gains 2.23×** → our crypto1 loss is **tactix scheduling** |
| mp1 | 9.22 s | 12.63 s | kissat gains **~27 %** (finding-6 correction: NOT "barely moved"); our post-dedup insensitivity = dedup + dosed Factor already harvesting that value; sentinel envelope 5.4–5.8 s stands |

Carried confound: `--no-eliminate` also removes kissat's in-eliminate
forward subsumption, so a fraction of each kissat delta is subsumption.

## 2. I1 — the damage is trajectory-class, and kissat has no defense (probes 1562429/1562430)

- kissat mod4block: **715,338 vs 177,632 conflicts (4.03×)** with
  elimination → trajectory-class (the plan's work-class branch is dead;
  debug-performance lane stays closed).
- kissat's admission is **not activity-starved** on mod4block: ~100 % of
  candidates scheduled per call through call 6 (1178→531 taper over 9
  calls), 763 eliminated — and it is harmed anyway. **No reference solver
  defends mod4block; the density gate is policy kissat lacks** (L9/L10).
  The kissat-analogy argument for the plan's I3 activity gate is refuted;
  finding 1's structural critique anticipated this.
- tactix forced doses on mod4block (`TACTIX_BVE_DOSE_DENSITY=0`): 34
  eliminations / 3.53 M resolutions / **3.70× conflicts**; the slot trace
  shows the candidate set never shrinks (n≈350 across 30 doses, 13+
  phases) — the pay-full-enumeration-to-decline overhead (gating claim 2)
  and the long multi-phase tail (round-shape claim 4), both confirmed.
- Shipped density gate = dose-off **byte-identical** on mod4block and
  crypto1 (455,682 / 566,404 conflicts).

## 3. I2 — recurrence vs content (probes 1562431/1562434, `TACTIX_BVE_ONESHOT`)

crypto1, all arms same-window:

| arm | conflicts | eliminated | wall |
|---|---|---|---|
| dose-off (shipped gate) | 566,404 | 0 | 16 s |
| drip-dosed (density=0, per-phase) | **1,831,099 (3.23×)** | 151 | 68 s |
| ONESHOT, ramp ceiling 8 | 630,889 (+11 %) | 71 | 16–17 s |
| ONESHOT, ramp ceiling 16 | 715,961 (+26 %) | 245 | 19 s |
| ONESHOT, ramp ceiling 32 | = ceiling 16 (saturated) | 245 | 18 s |

- The damage is the **drip-paced recurrence**: one completed early dose is
  nearly free where per-phase drips cost 3.23× conflicts. The plan's
  CONTENT branch (≥ ~40 s) is decisively dead; the E2 dry-run contingency
  was not needed.
- **Depth in one call does not buy kissat's profit**: more eliminations in
  one early blast = more conflicts (the L4 U-shape, reproduced in the
  during-search one-shot regime), and the pool saturates at 245 by
  slack-16.
- Falsifier upheld in every arm: wall time moved proportionally with
  conflicts (L7's trajectory reading).

Guards (same probe): mp1 held (one-shot 156,174 vs shipped 178,335
conflicts, 5 s both — its recurring completed doses stay profitable:
120 eliminated over phases); battleship byte-identical (110,125);
mod4block: even **one** completed dose costs 2.5× conflicts — intrinsic
(§2), untouched by any recurrence policy.

## 4. Mechanism verdict

The plan's RECURRENCE label resolves, with the discriminating property
sharpened: **completion + spacing, not recurrence per se.** kissat profits
on crypto1 from *recurring* elimination — 4 completion-gated calls at
geometric conflict intervals (hit at ≈13.5 k / 27 k / 57 k / 106 k
conflicts; eliminating 89 / 121 / 92 / **299** — the largest call LAST,
after search has simplified the formula), bound ladder advancing only on
completion, totalling 601 eliminated by 162,604 conflicts. Our harmful arm
differs in exactly two properties: doses fire **every phase** (far denser
than geometric conflict spacing) and are **budget/level-paced drips** that
rarely complete a schedule (the retired `TACTIX_NO_BVE_LEVEL` bisect had
already attributed 24 s of the 57 s regression to level pacing). No
single-call variant reaches kissat's profit (§3) — the profit needs
*completed calls interleaved with search progress*.

## 5. Routing

- **I3, refined**: port kissat's eliminate **cadence** to the arrangement —
  geometric conflict-interval admission for the BVE dose (kissat kimits
  shape) + a completion allowance per admitted dose (the one-shot's 700 M
  carve-out, per dose), on top of the already-landed completion-gated
  bound ladder (`bve_level_`). Policy-side, N1-clean. Gates: crypto1
  toward kissat's call shape with mp1/battleship held; **the density gate
  stays for admission** (mod4block is intrinsic, §2) — per finding 3, no
  default flip before the I8 tier arrays.
- **I5** (relevancy ordering) queues behind cadence; the 245-at-slack-16
  saturation says the candidate pool is not the binding constraint, but
  ordering may still shape what completed calls eliminate.
- `TACTIX_BVE_ONESHOT` is retained as an A/B arm (temporary knob, one
  env check on the dose path).
- em_11 (the density gate's known false positive) re-checked under
  cadence, not before.

## 6. Soundness

ctest 31/31 in both configs at the knob commit; fuzz **10 k / 0 (DRAT) +
10 k / 0 (LRAT)** with the one-shot schedule forced
(`TACTIX_BVE_ONESHOT=1 TACTIX_BVE_DOSE_DENSITY=0`), logs
`tools/fuzz/slurm-i2-oneshot-{drat,lrat}-156243{2,3}.log`.
