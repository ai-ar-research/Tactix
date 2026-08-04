# Broad Factor admission — re-test at HEAD, and the retirement of the `j3037` veto

**Date:** 2026-08-04 · **Base:** git `6018366` (`build/pin-bfa`, byte-identical to `build/pin-tb4b`)
**Windows:** Tier B `20260804-154329-B-shipped` (array 1582011) · Tier C `20260804-154341-C-shipped`
(array 1582116) — one submission each, idle partition, both baselines carried.
**Probes:** 1581982 (j3037 discriminator, 28 tasks) · 1582186 (round 2 + frb45, 22 tasks) ·
1582267 (ContextModel/mp1 mechanism check).

**VERDICT: HOLD — but the reason has moved.** The instance that held the change on 2026-08-03
(`j3037`) is refuted as a gate: it is a measured lottery, and in this window broad is **26× FASTER**
on it. The hold now rests on `ContextModel_output_7_3_9`, whose damage under this specific knob
**replicates across three independent windows**, and on the T3 gate metric, which broad moves the
wrong way on both halves.

---

## 1. What broad admission actually is (settled statically, no jobs)

`TACTIX_FACTOR_SCOPE_PCT` is strictly additive: the scope test is guarded by `!admit`
(`src/rules/factor/factor.cpp:600`), so it only fires where the density gate
(`binary_ids.size() >= 6 * num_output_vars`, `:581`) already rejected. Both quantities are
computable from the CNF. Classifying every tier instance at t=0:

| tier | density-admits (broad inert) | **broad flips Factor ON** | rejected by both |
|---|---|---|---|
| B (28) | 10 | **14** | 4 |
| C (30) | 6 | **19** | 5 |

**The 50 % scope threshold is a LONG-CLAUSE DETECTOR, not a fine-grained separator.** Nearly every
instance carries ≥95 % of its clauses at size ≤5, so the only formulas it excludes are those with
genuinely long clauses — `crypto1` 24.3 %, `mod4block` 7.9 %, `urq45` 30.8 %, `Grain` 29.5 %,
`constraints_18` 38.3 %, `constraints_25` 43.2 %, and the two `circuit_*` at 0.0 %. Everything else
is admitted. **"Broad admission" is closer to "Factor ON almost everywhere" than the name suggests**,
and the report/ledger language should say so.

For `j3037` specifically: 18,913 vars, 39,716 binaries = **2.10 binaries/var** against a gate of 6.0,
and **no clause longer than 4**, so its scope-5 fraction is **100 %**. On that instance shipped never
admits Factor and broad admits it unconditionally — it is Factor-OFF vs Factor-ON for the whole
solve. Confirmed empirically in every probe task: **0 factorings** in every shipped arm, ~1,690 in
every broad arm.

**Caveat on the static reading — it is a t=0 LOWER BOUND, not an exact control.** The gate is
re-evaluated on the live formula, so an instance can cross the threshold mid-solve. `mod4block`
(7.9 % at t=0, predicted inert) in fact moved from TIMEOUT to 245.8 s under broad. The three
genuine Tier B controls behaved as predicted (ratios 0.96 / 1.04 / 1.14) and all five Tier C
controls did (0.84 / 0.99 / 1.00 / 1.06 / 0.99), so the method holds — but membership in the
control set must be argued per instance, never assumed from t=0 alone.

## 2. The `j3037` veto is REFUTED (probes 1581982 + 1582186)

The solver is deterministic, so a "distribution" has to be sampled with knobs that are mechanically
independent of Factor's admission arithmetic. 18 paired draws `{shipped, broad}` across chrono,
bump-rate, bump-limit and walk-dose perturbations.

**Controls all passed.** `p00`/`rep` bit-identical in both arms (3,281,464 and 161,045 conflicts;
wall 394.6 vs 414.2 and 18.3 vs 17.7 — a 3–5 % in-window contention estimate). Every shipped arm
0 factorings, every broad arm ~1,690.

**Mechanism control.** `TACTIX_FACTOR_DENSITY=0` reaches Factor-ON without touching a line of scope
code and came out **bit-identical to broad** — 161,045 conflicts / 1,686 factorings — and identical
again under a second perturbation (2,660,558 both). **On `j3037`, broad IS Factor-ON**, by two
independent routes.

**The 08-03 pattern (shipped solves, broad times out) did not reproduce in ANY of the 18 pairs.**
Broad never timed out; shipped timed out once (`chrono90`, 8.8 M conflicts) and came within 5 % of
the cap once. Median conflict ratio broad/shipped over the 11 strictly-clean draws is **0.82** with
an IQR spanning 0.44–1.24 — median slightly favouring broad, spread over a factor of 2.8. That is
the quantitative lottery signature.

Pre-registered **Gate 2 = LOTTERY** on two independent triggers (broad wins at least once; the
shipped arm's own solved/TO outcome is not constant across perturbations). `j3037` is disqualified
as a gate. Round 1 yielded only K′=5 strictly-clean draws against a pre-registered floor of 6 —
because most perturbations left broad's short 161 k-conflict path untouched — so round 2 was run
rather than the rule reinterpreted; K′=11 final.

**In the tier window itself `j3037` went shipped 467.7 s → broad 18.0 s (26× FASTER)**, independently
corroborating LOTTERY in the direction that favours shipping. This was registered as a falsifiable
prediction before the run.

## 3. `frb45-21-2` is newly measured chaotic — and it re-prices the TB4b gate

Included in probe 1582186 because the TB4b Tier C gate (below) hinged on it.

- **Bit-stable under replication:** `frb-p00` and `frb-rep` both 7,856,563 conflicts.
- **`chrono` is entirely inert there:** 90/105/115 all give the identical 7,856,563.
- **6.2× under unrelated knobs:** `walk600` 105.1 s (2,138,451 conflicts) → `bumplim9` 652.3 s
  (8,476,285).

Same signature as the fragile three (L20 family). **`frb45` may not gate a per-instance verdict**,
and the TB4b Tier C regression attributed to it (90.2 → 588.6 s, 6.53×) sits inside its own
measured perturbation band.

## 4. The tier pair

### Tier B (28) — window `20260804-154329`, git `6018366`

| arm | all 28 | ex-`mod4block` (documented lottery, `2c77bce`) |
|---|---|---|
| shipped | 86.51 (25/28) | 67.49 (25/27) |
| **broad** | **38.39 (28/28)** | **30.70 (27/27)** |
| kissat | 9.78 (28/28) | 9.45 (27/27) |
| cadical | 40.38 (27/28) | 39.29 (26/27) |

**Broad solves all 28 — three solves gained, PAR-2 more than halved, and it edges past cadical.**
`or_randxor` TO → 19.4 s · `mod4block` TO → 245.8 s · **`unsat-set-b-fclqcolor` TO → 244.3 s**.
That last is the standing item (b), "the last Tier B timeout" — broad admission closes it outright.

### Tier C (30) — window `20260804-154341`, git `6018366`

| arm | all 30 | ex-`j3037` | ex-`j3037`+`frb45` |
|---|---|---|---|
| shipped | **309.05 (29/30)** | 303.58 (28/29) | 293.05 (27/28) |
| broad | 414.70 (28/30) | 428.38 (27/29) | 422.30 (26/28) |
| kissat | 230.74 (29/30) | 233.27 (28/29) | 238.26 (27/28) |
| cadical | 312.02 (29/30) | 321.90 (28/29) | 327.76 (27/28) |

**Broad loses one solve: `ContextModel_output_7_3_9`, 10.0 s → 1200 TO (120×).** No proof rescue
fired anywhere in the window and no verdict mismatch — this is a genuine timeout, not a scoring
artefact.

The damage is concentrated, not diffuse: 7 instances >10 % worse, 4 >10 % better, 19 flat.
`ContextModel` **+2,390 s** and `mp1-Nb5T15` **+801 s** are 3,191 s of a +3,170 s total delta — i.e.
**the whole Tier C regression is two instances**, against real wins (`j3037` −450 s, `Ptn-7824`
−90 s, `DLTM` −73 s). Quarantining both leaves broad at **+8.89 PAR-2**, ~1.3 noise floors.

## 5. Why `ContextModel` is a real loss and `j3037` was not

The distinguishing evidence is replication **for this specific knob**, across three bases:

| window | git | shipped | broad | |
|---|---|---|---|---|
| 2026-08-01 (task #11, L20) | — | 20.4 s | 453.7 s | 22.2× |
| 2026-08-03 | `014e3b2` | 7.7 s | 421.6 s | 54.9× |
| **2026-08-04** | `6018366` | 10.0 s | **1200 TO** | **120.2×** |

Three windows, three bases, same sign, monotonically worse. `j3037`'s effect flipped sign between
windows; `ContextModel`'s does not. L20 already documents `ContextModel` as stable-under-replication
but knife-edge under perturbation, with our default trajectory there unusually good — and broad is
exactly the kind of substantial perturbation it falls off. **Per the pre-registered rule, a lottery
finding on `j3037` disqualifies `j3037` and nothing else**; a new loss is real until it goes through
the same discriminator, and this one additionally replicates.

## 5b. The damage is FACTOR ITSELF, not the scope knob (probe 1582267)

Both Tier C regressions re-run at HEAD under three arms: shipped, broad, and `TACTIX_FACTOR_DENSITY=0`
(Factor-ON reached through the density gate, sharing no code with the scope branch).

| instance | arm | verdict | wall | conflicts | factorings |
|---|---|---|---|---|---|
| `ContextModel` | shipped | SAT | 9.5 s | 178,044 | 0 |
| | broad | **TO** | 1200 | 6,365,394 | 578 |
| | **dens0** | **TO** | 1200 | 6,378,735 | 578 |
| `mp1-Nb5T15` | shipped | SAT | 4.9 s | 135,591 | 0 |
| | broad | SAT | 812.2 s | 11,249,172 | 540 |
| | **dens0** | SAT | 815.3 s | **11,249,172** | 540 |

- **`mp1-Nb5T15` under broad and under dens0 is BIT-IDENTICAL** — 11,249,172 conflicts, 540 factorings.
  `ContextModel`'s two arms differ by 0.2 %, which is only where the 1200 s wall landed. **Two independent
  admission routes, one trajectory: the scope knob is innocent and Factor's PRESENCE is the cause.**
- **It is trajectory poison, not overhead** — 36× the conflicts on `ContextModel` and **83×** on
  `mp1-Nb5T15`. Same shape as L7 (during-search BVE costs trajectory, r=+0.99 with conflicts, per-phase
  overhead dead) and L9.
- **Consequence for the routing:** the open question is *on which instances Factor should run*, not *what
  the scope threshold should be*. Tuning the threshold cannot separate these cases, because the threshold
  is not what reaches them. This is the L9 "when to eliminate is the measured lever" result, now
  reproduced for Factor with per-instance labels on both sides.

## 6. The gate metric decides it

T3's binding half is kissat parity on the rule-neutral subset (`tools/bench/subset_score.py`,
24 of 30):

| arm | subset PAR-2 | ×kissat | ×cadical |
|---|---|---|---|
| shipped | 351.55 (23/24) | **1.346×** | **1.069×** |
| broad | 391.63 (23/24) | 1.500× | 1.191× |

**Broad moves phase-3 exit the wrong way on BOTH halves.** With the pre-registered rule already
returning HOLD on the solve count (`SC_ex(broad)` 27 < `SC_ex(shipped)` 28), this is not a close
call for the gate that actually matters.

## 7. Consequences and routing

1. **HOLD broad admission as a blanket knob.** It is +3 Tier B solves and −1 Tier C solve, and it
   costs the T3 gate on both halves.
2. **Retire the `j3037` veto.** It held this change on 2026-08-03 and is measured a lottery
   (K′=11). Any future writeup citing that hold must cite this refutation.
3. **The blanket knob is the wrong instrument, and this is now cleanly measured on both sides.**
   Factor admission is worth **3 Tier B solves** (including the last Tier B timeout) and costs
   **1 Tier C solve** plus two concentrated regressions. Remove those two instances and Tier C is
   within ~1.3 noise floors. That is precisely the §5.6 admission-dosing exhibit shape: a
   per-instance decision no static gate has managed (L15's four dead gates, L5's chance-level
   separator, and the 2026-07-30 reference study finding no such axis in kissat or cadical).
   **This window is the strongest labelled signal that family has** — 14/28 B and 19/30 C flipped,
   with per-instance outcomes on both sides.
4. **Item (b) `fclqcolor` is no longer blocked on "needs broad admission"** — it is blocked on
   *targeted* admission. Its own defect (acceptance saturating at 78 factorings; the 27×27 grid cap)
   is unchanged and still the thing to price.
5. **`frb45` joins the may-not-gate set** alongside `mod4block`, `j3037`, `jgiraldezlevy`, `urq45`,
   `ContextModel`.

## 8. Method notes

- **Two of this experiment's four decisive facts cost zero jobs** — the admission classification and
  the control-set prediction both came from arithmetic on the CNF plus two guard conditions in
  `factor.cpp`. The reachability pre-flight the design called for was answered before it ran.
- **The control set is a free noise estimator.** Instances where a knob provably cannot act must run
  identical trajectories; their wall spread is the in-window contention estimate (measured 0.84–1.14
  here). Better than a synthetic replicate because it costs nothing and rides along in the gate run.
- **Pre-registration paid twice.** It forced round 2 when K′ came in at 5 (rather than declaring the
  rule satisfied), and it forbade quarantining `ContextModel` by analogy with `j3037` — which would
  have shipped a change that costs the gate.
- **A prediction registered in the favourable direction was kept.** "If `j3037` is a lottery the new
  window should not reproduce the 08-03 pattern" was recorded before the run and came back 26× in
  broad's favour; it is reported here even though it is the outcome that argued for shipping.
