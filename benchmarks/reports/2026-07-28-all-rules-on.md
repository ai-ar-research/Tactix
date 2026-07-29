# Every implemented proof rule ships ON — and every hand-designed admission gate for them is measured-dead

**Date:** 2026-07-28/29 · **Tier B + Tier C**, like-for-like protocol (plan rev 72: every
arm writes DRAT and every UNSAT is checked), SLURM `cpu-dense-hwulab-q`,
1 core / 8 GB, throttle 48. Seven same-window arrays; probes 1571606, 1571678.

## 0. The directive

> "all implemented proof rules need to be turned on!!" — user, 2026-07-28

This closes a drift. The standing baseline (2026-07-25) is that a rule-off arm
is a **diagnostic for a scheduling defect**, never a shipped default; Track R
had nonetheless grown a "ships OFF until its own A/B says it pays" convention,
and X1 `BCE`, X2 `HyperTernaryRes` and X4 `Instantiate` all landed disabled
behind a hardcoded name check. That convention is dead. The audit
(`TACTIX_SLOT_TRACE`) found those three were the ONLY disabled entries; the
default now enables **13 of 13** Inprocess rules.

## 1. The headline

| arm | Tier B PAR-2 | Tier C PAR-2 |
|---|---|---|
| shipped (rules off) | **7.94** (7 windows, 7.69–8.23), 14/14 | **314.25**, 29/30 |
| **all three ON (shipped now)** | **9.27** (5 windows, 9.07–9.62), 14/14 | **349.60**, 29/30 |
| kissat 4.0.4 | 8.55, 14/14 | 226.53, 29/30 |

Turning them on costs **+1.3 PAR-2 on Tier B (≈ 17 %)** and **+35 on Tier C
(≈ 11 %)**, taking Tier B from 0.93× kissat to 1.08×. **No solves are lost on
either tier.** That is the honest price of the directive as of today, and the
scheduling that removes it is the open item — not a reason to disable anything.

Tier C is where the upside is visible, and it is large in both directions
(same window `20260728-230320`):

| instance | shipped | all-on | Δ |
|---|---|---|---|
| minxor128 | 621.03 | **355.86** | −265 |
| T87.2.0 | 1007.76 | **773.94** | −234 |
| sat-bench-trig-bhaskara | 577.22 | **382.51** | −195 |
| ContextModel_output_7_3_9 | 220.77 | **129.06** | −92 |
| j3037_9_rggt_b | 129.73 | 1118.03 | **+988** |
| asconhashv12_opt64_H4_M2-LOD9 | 362.68 | 618.87 | +256 |

Four instances buy 785 s; one loses 988. A mechanism that swings individual
Tier C instances by ±2–8× in BOTH directions is a scheduling problem with real
headroom, not a family of bad rules — which is the same reading §2 forces.

**Tier C at HEAD, for the record** (window `20260728-230139`, rules still off):
tactix 306.55 vs kissat 225.69 = 1.36×, 29/30 each. Certificate cost stays
lopsided in our favour: 7,641 MB / 4,809 s against kissat's 9,926 MB /
16,975 s, with one kissat proof (aes_decry) exceeding the checker budget
outright.

## 2. Why shipping any of them ALONE would have been the wrong call

Same-window per-rule decomposition (`20260728-231212`), reproduced against the
earlier `20260728-081739` window:

| arm | Tier B | solved |
|---|---|---|
| shipped | 8.05 | 14/14 |
| + `BCE` alone | 21.79 | 14/14 |
| + `HyperTernaryRes` alone | **53.26** | **13/14** |
| + `Instantiate` alone | 13.08 | 14/14 |
| **+ all three** | **9.22** | 14/14 |

Every rule alone is a disaster; all three together are nearly neutral. X2 adds
ternary resolvents and BCE deletes blocked clauses, so they partly cancel — a
rule's value here is **not separable from its neighbours**. The ships-OFF
convention had been judging them one at a time, which is the one measurement
that cannot see this.

## 3. Dose is the WRONG control (L7/L12, a third mechanism)

Common-rate sweep, all three together (`20260728-230641`):

| per-mille | 2 | 5 | 10 | 20 | 50 |
|---|---|---|---|---|---|
| Tier B | 9.99 | **25.64** | 19.52 | 9.62 | 13.76 |

Non-monotone. Decomposed per instance, **mod4block alone** swings
13.14 / 232.03 / 138.55 / 13.53 / 48.95 and manufactures the entire aggregate
shape — its 232 s at rate 5 *is* the 25.64. Underneath it the picture is
rate-INDEPENDENT: clique wins 50× at every rate (5.51 → 0.10), x9 gains ~12 %
at every rate, and rbsat/crypto1/mp1 lose at every rate.

The firing probe (1571606) explains the mechanism exactly:

| instance | conflicts, shipped → +BCE | BCE rounds |
|---|---|---|
| clique | 434,299 → **14,315** | **1** |
| rbsat | 80,052 → 322,371 | 2 |
| crypto1 | 81,589 → 129,414 | 3 |

**BCE's entire value is one round on the raw formula; its entire cost is the
rounds after it.** A rate change shrinks each round but never stops round 2 —
which is why the sweep is chaos. (On crypto1 the damage is mostly X2 instead:
36 rounds, 5,544 resolvents, 17× conflicts. `Instantiate` is inert everywhere —
hundreds of probes, 0 literals removed — so its cost is pure watch-list
perturbation, L3's "NoOp is not a no-op".)

## 4. FOUR admission designs, all measured-dead

Each was motivated by a real, measured mechanism. Each helped the instances it
targeted. Each was killed by the same single instance.

| design | Tier B | what it did | mod4block |
|---|---|---|---|
| all-on, no admission | **9.17** | — | 13.17 |
| dose ↓ (rate 2–50) | 9.62–25.64 | kept clique, halved rbsat | 13–232, no trend |
| preprocess-only | 27.68 | rbsat 13.2→9.6, crypto1 6.8→3.3 | **263.87** |
| density gate (L9's, threshold 6) | 51.99 | crypto1 6.69→4.93 | **300.00 TO** |
| density gate at 20 | 29.08 | same | 279.67 |
| yield backoff (100 ticks/unit) | 52.21 | rbsat 12.7→9.8 | **300.00 TO** |
| yield backoff + density | 26.96 | — | 253.45 |

The yield axis looked like the ideal separator and still failed: BCE costs **13
ticks per deletion on clique**, against 477 (summle), 925 (mod4block), 1,063
(rbsat), 3,434 (crypto1), 6,726 (circuit), and spends 472k/885k ticks for
**zero** deletions on battleship/qwh (probe 1571678). An order-of-magnitude
clean feature — and gating on it still lost 43 PAR-2.

**The finding: any PARTIAL suppression of these three rules is far worse than
running all of them unsuppressed** (27–52 vs 9.2). Their value is in the mutual
balance; remove one rule's rounds and the damage another rule does goes
unrepaired. This is the same shape as §2 (no rule is separable) seen from the
scheduling side, and it is consistent with L10's verdict that mod4block's
pathology is instance-intrinsic and undefended by any reference solver.

**Do not re-run these four.** The next lever is not a better hand-designed gate
on this family — four independent ones failed, three of them on the same
instance. It is either (a) a JOINT admission acting on all three at once rather
than per-entry, or (b) the phase-6 learner, for which this is now the sharpest
exhibit in the §5.6 catalogue: a mechanism family where every static gate a
careful engineer would write is measurably worse than no gate at all.

## 5. Soundness

Fuzz on the assert-enabled build of the new default composition:
**10k/0 DRAT** (1571808), **10k/0 native LRAT** (1571809), **10k/0
`--random-policy`** (1571810). Full ctest green in release and debug.

## 6. What shipped

- All 13 Inprocess rules enabled; the hardcoded name exclusions are gone.
- `TACTIX_SLOT_PERMILLE=Name:rate[,...]` kept as a §5.6 dosing arm (the shape
  `TACTIX_WALK_PERMILLE` has).
- The four failed admission mechanisms are NOT in the tree — banked here per the
  negative-arm cleanup convention; recover from git history if a future arm
  wants one.
