# Broad Factor admission: HELD (loses a Tier C solve) — and T3's both halves, measured

**Date:** 2026-08-03 · **Frozen on landing** (doc discipline).
**Windows:** Tier B `20260803-211056-B-shipped`, Tier C `20260803-211105-C-shipped`.
**Binary:** `build/pin-t3b/tactix`, git **`014e3b2`** (taken from the run manifest, not from a later ship
commit). **Shape:** `cpu-dense-hwulab-q`, 1 core / 8 GB per task, `--throttle 48`, every arm writing and
checking DRAT.

**Arms.** `shipped` = current default (R-d retirement ON) · `broad` = default +
`TACTIX_FACTOR_SCOPE_PCT=50` · `broadnord` = broad + `TACTIX_RETIRE=0` · baselines kissat and (Tier C)
cadical. **Tier C carries a kissat arm for the first time since 2026-07-30**, which is what makes T3's
second half measurable at all.

---

## 1. The decision: broad admission is HELD

| | Tier B (28, 300 s) | Tier C (30, 1200 s) |
|---|---|---|
| shipped | 73.33 (26/28) | **350.56 (29/30)** |
| **broad** | **46.79 (27/28)** | **448.72 (27/30)** |
| broadnord | 68.55 (26/28) | 342.97 (29/30) |
| kissat | 9.81 (28/28) | 228.48 (29/30) |
| cadical | — | 313.75 (29/30) |

Broad admission is a large Tier B win — **−26.5 PAR-2 and the 27th solve**, taking `or_randxor` from TO to
65.5 s and `sted5` from 240.7 s to 13.0 s — and it **loses a Tier C solve**. plan decision (a927131) is explicit: *"a
change that LOSES a solve is not covered by this priority no matter what it does to PAR-2."* **It does not
ship.**

### The two Tier C solves broad "loses" are not the same thing

| instance | shipped | broad | broadnord |
|---|---|---|---|
| `j3037_9_rggt_b` | 1092.2 s SAT | **1200 TO** | 218.5 s SAT |
| `urq45` | 598.0 s UNSAT, VERIFIED | **619.3 s UNSAT**, check blew the 4800 s budget | 201.7 s UNSAT, VERIFIED |

- **`j3037` is a genuine solver loss** and is what holds the change.
- **`urq45` is NOT a solver loss.** Broad *solved* it in 619.3 s and returned the correct verdict; the DRAT
  check exceeded its budget, and the runner scores that as unsolved. This is the same checker-budget mode
  that scored `urq45` unsolved on 2026-07-27 at a 354.5 s solve. It is a measurement artifact, and it is
  why the run banner says "CORRECTNESS BUG" — **no arm returned a wrong verdict** (`mismatch` is false
  everywhere, verified against the raw JSON).
- **Caveat that does not change the decision:** `j3037` is one of the three measured perturbation-FRAGILE
  instances (8–23× swings under unrelated knobs). Our own three arms span **218.5 s → 1092.2 s → TO** on it
  in this single window, which is inside its documented chaos band. So "broad loses j3037" needs replication
  before it is called a settled effect. The burden, however, is on shipping, not on holding.

### R-d and broad admission are ANTAGONISTIC on Tier C

Either alone is fine; together they lose solves.

| arm | Tier C |
|---|---|
| R-d only (`shipped`) | 350.56 (29/30) |
| broad only (`broadnord`) | **342.97 (29/30)** |
| **both** (`broad`) | **448.72 (27/30)** |

On Tier B the ordering inverts — the combination is the only arm that reaches 27/28. This is a genuine
interaction, not a dosing question, and it is the reason the composition had to be measured rather than
inferred from the two separate 2026-07-31/08-01 results.

---

## 2. T3, both halves, measured

The rule-neutral subset (`benchmarks/tiers/C-ruleneutral.list`, 24 of 30) scored from the same window via
`tools/bench/subset_score.py` — so both views come from one run, never two.

| arm | subset PAR-2 | full PAR-2 | subset / kissat | subset / cadical | full / cadical |
|---|---|---|---|---|---|
| **shipped** | **327.96** | 350.56 | **1.271×** | **0.987×** | **1.117×** |
| broad | 326.33 | 448.72 | 1.264× | 0.982× | 1.430× |
| broadnord | 375.48 | 342.97 | 1.455× | 1.130× | 1.093× |
| kissat | 258.10 | 228.48 | — | — | — |
| cadical | 332.38 | 313.75 | — | — | — |

| T3 half | requirement | measured | status |
|---|---|---|---|
| **CaDiCaL** | within 10 % on Tier C | **1.117×** | **NOT met**, by 0.017 — far inside the noise floor |
| **Kissat** | parity on the rule-neutral subset | **1.271×** | **NOT met** — but improved from **1.37×** (2026-07-27), the first measurement in a week |

**The Kissat half is the binding constraint, as suspected.** On the subset we are already *better than
CaDiCaL* (0.987×) while sitting 1.271× kissat. Optimising against cadical — the number the runner has been
emitting — was steering on the easier baseline. Against kissat on the full tier we are **1.534×**.

---

## 3. What this changes

1. **Broad admission stays an arm.** Its Tier B prize (+1 solve, −26.5 PAR-2) is real and reproduced across
   three windows; what blocks it is the R-d interaction on `j3037`. The next experiment is not another dose:
   it is whether the two can be composed without the interaction — and the first thing to price is whether
   `j3037`'s loss replicates at all, given the instance's measured fragility.
2. **T3's kissat half is the phase-3 exit constraint**, and it is now instrumented and trending
   (1.37× → 1.271×). Every Tier C run must keep `--baseline kissat --baseline cadical`.
3. **The checker budget is now visibly load-bearing on results, not just on wall clock.** `urq45` cost the
   broad arm a scored solve for a checking reason. Any change to the checker budget must not shrink OUR
   arms' budget; the safe reduction is to stop verifying BASELINE proofs, which in this window consumed
   **6.07 h of 9.08 h (67 %)** of all checking and cannot change a verdict by construction.
