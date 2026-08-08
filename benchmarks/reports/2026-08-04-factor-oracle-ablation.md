# The Factor ON/OFF oracle — how much per-instance admission is actually worth

**Date:** 2026-08-04 · **Base:** git `91afdc4` (`build/pin-facablate`)
**Windows:** Tier B `20260804-165514-B-shipped` (array 1582275) · Tier C `20260804-165516-C-shipped`
(array 1582342) — three arms + both baselines, one submission per tier, idle partition.

**Question.** Our Factor admission is an instance-level density gate. What is the CEILING on getting
that decision right, and how much of it does the gate capture? Answered by measuring the two trivial
policies and taking the per-instance best.

**Arms.** `shipped` = the density gate (`binaries >= 6*vars`) · `facon` = `TACTIX_FACTOR_DENSITY=0`
(always admit) · `facoff` = `TACTIX_NO_FACTOR=1` (never run Factor). The ORACLE is the per-instance
minimum of `facon` and `facoff` — not a runnable policy, the bound on any admission mechanism.

---

## 1. The numbers

| arm | Tier B (28) | Tier C (30) |
|---|---|---|
| `facoff` — never | 150.14 (22/28) | **285.90 (29/30)** |
| `shipped` — density gate | 86.42 (25/28) | 296.82 (29/30) |
| `facon` — always | **34.59 (28/28)** | 406.55 (28/30) |
| **ORACLE** best(on, off) | **32.25 (28/28)** | **254.55 (29/30)** |
| kissat | 10.09 (28/28) | 228.43 (29/30) |
| cadical | 41.01 (27/28) | 308.97 (29/30) |

**Headroom the gate leaves on the table: Tier B +54.17 PAR-2 and +3 solves; Tier C +42.27 PAR-2**
(6.3 noise floors at the measured 6.72 bar) **and 0 solves.**

## 2. The finding: our gate is never the best of the three arms

- **Tier B wants Factor ON.** `facon` (34.59, 28/28) is within **2.3 PAR-2 of the oracle** and solves
  everything. The gate is far too conservative here — it costs 3 solves.
- **Tier C wants Factor OFF.** `facoff` (285.90) beats the gate by **10.9 PAR-2, 1.6 noise floors**.
  The gate is (mildly) too permissive here.
- So the gate does real discrimination — it beats the *wrong* trivial policy on each tier by a wide
  margin (Tier B: 86.42 vs `facoff` 150.14; Tier C: 296.82 vs `facon` 406.55) — but **on neither tier
  is it the best of the three**, and it is mis-calibrated in OPPOSITE directions on the two.

## 3. The labels, which are the point

| | Factor HELPS | Factor HURTS | neutral |
|---|---|---|---|
| Tier B | **12** (6 of them TIMEOUT → solve) | 2 | 14 |
| Tier C | **3** | **10** | 17 |

Per-instance, >1.3× either way. Tier B: helps `clique_n2_k10`, `DecompressReader`,
`mp1-blockpuzzle`, `or_randxor`, `mod4block`, `fclqcolor` (all TO → solve), plus `sted5`, `oddball`,
`summle`, `crypto1`, `x9`, `rbsat`; hurts `qwh` (4.7 → 45.1 s) and `pyhala` (0.9 → 7.4 s).
Tier C: helps `j3037` (442.7 → 18.7 s), `Ptn-7824` (128.2 → 40.4), `DLTM` (122.2 → 49.2); hurts ten,
worst `ContextModel` (9.7 → TO), `mp1-Nb5T15` (5.0 → 840.7), `jgiraldez` (4.6 → 212.6), `frb45`
(47.4 → 559.7).

**The tiers genuinely disagree about Factor**, and not marginally: 12-helps/2-hurts against
3-helps/10-hurts. Tier B is calibrated to kissat 3.5–19 s instances and Tier C is far harder, so
"Factor's value falls as instances get harder" is the obvious candidate feature — and it is testable,
not assumed. Note this is a *tier-level* correlation over 58 instances, not a per-instance predictor;
L15's four dead gates are what happens when such a correlation is turned straight into a threshold.

## 4. What this does NOT license

**It does not license porting kissat's per-grid acceptance bound.** That inference was made and
refuted three ways on 2026-08-04 (verification of the 07-30 study against live source):

1. **P0's own arm:** kissat with `--eliminatebound=0` — its bound pinned to zero — *improved*
   `mod4block` 18.35 → 6.54 s and was fine-or-better nearly everywhere. **The bound is not what saves
   kissat.**
2. **The gain histograms straddle** (`2026-07-31-factor-scope-and-admission.md:62-66`): winners at
   mean δ **1.4** (`edp3`) and δ **13.5** (`sted5`), losers at **4.2/4.8 between them**. No static
   delta threshold separates them; bound 8 cost `sted5` 19 s → 274 s.
3. **The coupling measured negative three times**, including 2026-08-04 with composition ON.
   Recorded verdict: do not re-try without a new mechanism.

**And the damage is not about the admission route at all** (probe 1582267): `ContextModel` and
`mp1-Nb5T15` reproduce **bit-identically** whether Factor is reached via the scope knob or via
`density=0` (`mp1` 11,249,172 conflicts under both). It is Factor's PRESENCE, not the threshold.

## 5. A defect in the gate itself, found while reading

**`k.binary_ids` is size-monotone**, by design — deletion keeps the key (`kernel.hpp:472-476`) — and
**learned binaries enter the same map** (`analyze.cpp:441`). So the gate's numerator is "binaries ever
created", not live binary density, and the denominator (`kernel_num_output_vars`) is fixed at the
original variable count. **The gate therefore LOOSENS monotonically over the solve:** an instance
below the threshold at `conflicts == 0` can cross it purely by learning, with no structural change to
the formula.

Two consequences. (a) It is not the formula-shape test its comment describes — it is partly a proxy
for elapsed search. (b) It explains the anomaly in the 08-04 broad-admission window, where
`mod4block` (7.9 % scope fraction, predicted inert at t=0) in fact moved TIMEOUT → 245.8 s: a t=0
classification is a LOWER BOUND on reachability, and the crossing happens mid-solve.

## 6. Routing

The prize is real and large (+3 solves / −54 PAR-2 on B, −42 on C) and the obvious mechanisms are
measured dead, which is precisely the §5.6 admission-exhibit shape. This experiment's per-instance
labels over 58 instances, with both arms and the oracle, are the labelled dataset that exhibit needs.
Held under **N4** (plan decision (f22423c)): the density gate is a crude stand-in of our own invention for a
mechanism neither reference has, so it is a defect to be closed, not an arm to be tuned — but the
closer must be a *new* mechanism, not the per-grid bound.
