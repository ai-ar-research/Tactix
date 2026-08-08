# E2 Factor: shipping the scope half, and why the admission half is held back

**Date** 2026-07-31 · **git** `2a40126` (measurement), default flip in the commit
that carries this report · **Tier B(28)** window `20260731-021612` · **Tier C(30)**
window `20260731-021615` · noise window `20260731-013523`.
**Allocation shape** (identical for every number here): partition
`cpu-dense-hwulab-q`, 1 cpu/task, 8 GB/cpu, `--throttle 48`. Every arm — ours and
both baselines — writes and checks DRAT (plan §8.1 decision (d0bb6ec)).

Frozen on landing (doc discipline). Supersedes nothing; extends the R5 line in
`docs/design/e2-factorsize-design.md` and the 2026-07-30 reference study.

## 1. What shipped, and what did not

| change | Tier B(28) | Tier C(30) | verdict |
|---|---|---|---|
| **`factor_size` 2 → 5** (scope) | 104.79 → 105.30 (+0.50, noise) | **337.50 → 327.99 (−9.51)**, 1.066× → **1.036×** cadical | **SHIPPED** |
| `factor_scope_percent = 50` (admission) | **104.79 → 65.86, 24 → 26 solved** | 337.50 → **357.64**, **1.129× — outside T3's 10 % band** | held as an arm |

The two were measured together as one arm on 2026-07-30 and looked like a single
trade. They are separable, they pull in opposite directions, and only one of them
passes its gate.

## 2. Why the split is the right cut — decomposition by admission class

Splitting Tier C's per-instance deltas by *which* gate admits the instance:

| class | net Δ PAR-2 seconds |
|---|---|
| already **density**-admitted (only `factor_size` changes them) | **−173** |
| **newly scope**-admitted (the gate change) | **+1078** |

So the scope widening is Tier-C-*positive* on the instances Factor already ran on,
and every bit of the damage comes from newly admitting instances. But **100 % of
Tier B's gain is also in the newly-admitted class** — Tier B's density class is
exactly **+0.0**, because *no* Tier B instance passes the density gate at all.
That is the whole difficulty in one line: the two tiers pull on the same lever in
opposite directions, so no single setting of it is jointly optimal.

## 3. Mechanism: search damage, not Factor's cost

Conflicts on the newly-admitted losers, shipped → union:

| instance | conflicts | ratio | factorings |
|---|---|---|---|
| ContextModel | 335,333 → 2,830,051 | **8.4×** | 459 |
| jgiraldezlevy | 1,312,822 → 5,071,531 | 3.9× | 76 |
| mp1-Nb5T15 | 1,456,950 → 3,507,100 | 2.4× | 540 |

and on the winners, the same lever runs the other way — `sted5` needs **14× fewer**
conflicts (3,782,520 → 271,014). Factor's own runtime is not the variable; the
*factored formula* is what search finds harder or easier. Any fix therefore has to
change **what Factor produces**, not how much time it is given — which retires the
dosing framing for this rule family a second time (cf. 2026-07-30 R1/R2).

## 4. Two candidate separators, both measured dead

**Scope threshold.** Winners and losers sit at the *same* scope fraction — j3037
100.0 % (−99.7 s) against ContextModel 99.8 % (+400 s), mp1-Nb5T15 100.0 % (+143 s).
Raising 50 → 90 % cannot separate them. (L15, fifth sighting.)

**Payoff bound** (kissat's `reduction > bound`, `factor.c:939`). Accepted-gain
histograms *straddle*: `edp3` is a winner at **mean gain 1.4** with 1686 of its 2717
factorings at δ=1 — its entire benefit is low-gain factorings — while `sted5` is a
winner at 13.5, and the losers sit at 4.2 / 4.8 *between* them. Measured on sted5:
19 s → **274 s** (bound 8) → **118 s** (bound 16). The bound costs Tier B and cannot
buy Tier C.

Do not try a third static separator. The evidence now says the discriminator is not
a formula statistic.

## 5. The real defect, and it is ours

The entire Tier C regression is **two instances** — ContextModel +464 s and
jgiraldezlevy +396 s, together +860 s against a net of +623 s. Excluding just those
two, broad admission would *improve* Tier C by −7.9 PAR-2.

CaDiCaL runs Factor **broadly, with no formula-shape admission at all**, and solves
both comfortably:

| instance | ours (shipped) | ours (union) | **cadical** |
|---|---|---|---|
| ContextModel | 20.3 s | 453.7 s | **65.8 s** |
| jgiraldezlevy | 76.0 s | 465.4 s | **16.4 s** |

Broad admission is therefore not inherently wrong — **our Factor behaves 7–28×
worse than cadical's under it**. That is a fixable defect in our rule, not a cost of
the gate, and fixing it is what would make the Tier B lever free.

## 6. Two defects found on the way

**`Kernel::factor_bound` was unreachable under DefaultPolicy.** The rule accepts via
`fm ? fm->accept(…) : delta > k.factor_bound`, and DefaultPolicy *always* attaches
`DefaultFactorMatch`, whose `accept()` was hardcoded `delta >= 1`. A documented N1
config surface that no shipped path could exercise — it surfaced only because a
bound sweep returned **byte-identical proofs** (430.5 MB in all three arms). Fixed:
the bound is copied in at the attach site; `bound = 0` reproduces `delta >= 1`
exactly. *Lesson: an A/B arm that produces identical artefacts has not been tested,
it has been skipped — check the artefact, not just the score.*

**Fresh extension variables were the most-preferred branching candidates.**
`heuristics_reserve_vars` appends new vars at `queue_last`, which is exactly where
VMTF's decision search starts, so every variable Factor adds jumped to the head of
the decision order. Kissat demotes them (`adjust_scores_and_phases_of_fresh_varaibles`);
our stable-mode heap was already correct (push at score 0.0 sits at the bottom of a
max-heap), so this is a **focused-mode-only** defect. `TACTIX_FACTOR_DEMOTE`
implements the repair and is **banked, not shipped**: it rescues j3037 207 → 88 s,
edp3 74 → 39 s, rxor 49 → 36 s, but costs **sted5 16 → 150 s**. Non-separable, so it
needs its own tier pair rather than a ride-along.

## 7. Measurement hygiene — the noise floor, measured

The inert bound sweep accidentally produced **three bit-identical Tier C arms**.
Their spread *is* the measurement noise:

- aggregate: **346.16 / 356.95 / 353.99** → **10.8 PAR-2 between identical configs**
- per instance: mean range **24.7 s**; `goldcrest-and-9` alone **245 s (72 %)**

Consequences applied throughout this report:
1. `goldcrest-and-9`'s apparent −69.9/+45.1 swings are **noise** — confirmed
   independently: 0 factorings and **272,082 conflicts in both arms**, a
   bit-identical search, reproduced at 430 vs 431 s.
2. The shipped Tier C ratio has moved **1.007–1.066× across windows at an unchanged
   default**. T3's CaDiCaL half is met, but its margin is window-dependent — always
   quote a window.
3. `s5only`'s **−9.51** aggregate sits *inside* that ±10.8 band, so it is **not**
   defensible from the aggregate alone. It is defensible per-instance:
   af-synthesis **−190.9 s against a 2.5 s identical-config range** (conflicts
   2,152,249 → 318,157, reproduced standalone at 224 → 37 s), constraints_25 −51.6
   (range 15.0), j3037 −15.6 (range 2.5). Stated plainly: *the aggregate is
   suggestive, the per-instance mechanism is what justifies the flip.*

## 8. Acceptance gate for the shipped change

From `docs/design/e2-factorsize-design.md` §5:

- **KILLS — all clean.** x9 +0.8 s, mp1-blockpuzzle −0.0, FmlaEquivChain −0.6,
  mod4block +0.4. None of today's Factor beneficiaries regresses.
- **Bit-identity at `factor_size = 2`** held for every binary built (mod4block
  **418,796** conflicts), so the flip is a default change, not a code change.
- **Fuzz** on the new default: **4 × 10,000 cases, 0 failures** — DRAT and native
  LRAT, each with and without `--random-policy`. ctest 38/38.
- **CONFIRMS not met by this change alone:** `or_randxor` and `fclqcolor` need the
  *admission* half, which is held back. That is the open item.

`128_125.sanitized.cnf.xz` is unsolved for **every** arm including cadical by **OOM
at the 8 GB cap**, not timeout. It scores 2×TO identically everywhere and biases no
comparison, but it is an OOM and the tier list should say so.

## 9. Open

The Tier B gap is **not** merged. The lever that merges it (+2 solves, −39 PAR-2) is
the admission half, and it is blocked on §5: our Factor under broad admission is
7–28× worse than cadical's on ContextModel and jgiraldezlevy. Fix that, and the
gate becomes free. Do not fix it with a fourth formula-shape separator.
