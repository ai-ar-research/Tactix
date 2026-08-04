# Progress

Living status. **Sync contract:** update this file in the same commit as (a) completing/starting any checklist item, (b) any DEVELOPMENT_PLAN.md
revision (via revise-plan). If this file and the plan disagree, the plan is authoritative — fix this file.

- **Plan revision tracked:** rev. 78 (2026-08-02)
- **Current phase:** 3 (inprocessing) — since 2026-07-14 (phase 2 closed at T2 the same day, report `benchmarks/reports/2026-07-14-t2-gate.md`)
- **Last updated:** 2026-08-03 — **variable retirement (R-a…R-d) SHIPPED** (Tier B 105.30 → 75.41 PAR-2, 24 → 26 solves, across two fixes); plan revs 76/77/78; V6 Congruence's matching gap located and is the next step. **Doc audit the same day corrected the congruence headline (182× → 3.3–3.9×, counter semantics), the window SHA, and the stale T3 gate row — Log.**

## ▶ RESUME HERE (2026-08-03) — phase 3; **next step = V6 Congruence's MATCHING gap** (concrete, instrumented)

Read this block, then the top Log entries. Full per-instance numbers: the tier tables below.

**WHERE WE STAND** (windows `20260802-112851-B` / `20260802-112853-C`, **git `31593d5` + uncommitted R-d**,
every arm writing + checking DRAT). **Tier B 75.41 (26/28)** vs kissat 10.05 = **7.504×** — down from
105.30/24 at session start, via two landed fixes (V3's per-SCC hazard skip, then R-d variable retirement).
**Tier C 358.12 (29/30) vs cadical 317.32 = 1.129×** — **T3's CaDiCaL half is NOT met at the shipped
default**, a deliberate rev-78 trade (#solved wins; the OFF arm is 1.039×). Fuzz green: 4 × 10,000 cases, 0
failures, DRAT + native LRAT × ± `--random-policy` on ASan+UBSan. ctest 38/38.
**Two framings the 2026-08-03 audit corrected — read them before acting on the numbers above.** (i) Tier C's
gap is a TAIL, not a broad loss: the **median Tier C instance is 0.830× cadical** and we are faster on 15 of
29; excluding the three fragile instances the shipped arm is **0.971×** and the OFF arm *inverts* to 1.077×.
(ii) **cadical is not the hard Tier C baseline — kissat is**: same-tier kissat scored **229.64** on
`20260730-085509`, i.e. we are ~**1.56×** kissat there, and Tier C has had NO kissat arm since 07-30.

**▶ THE NEXT STEP — V6 Congruence finds 3.3–3.9× fewer congruences at 7.6× the time.** Measured on
`T105.2.0` (probe 1579468, both solvers, same instance): extraction is COMPARABLE (ours 53.4 M gates vs
kissat 18.5 M, and kissat's XOR share is *higher*), but we produce **3,713 merges against kissat's 14,461
congruent variables** while spending **7.6× the time** (172.9 s vs 22.75 s). V6 owns 32.9 % / 38.2 % of the
clock on `T87`/`T105`, ~462 s across the two.
- **⚠ THE "182×" IN THIS BLOCK WAS A COUNTER-SEMANTICS ERROR (corrected 2026-08-03, audit — Log).** kissat's
  `congruent_matched` (674,830) counts **hash-table find-hits inside `find_gate_lits`**
  (`kissat/src/congruence.c:510`), bumped during gate CONSTRUCTION — it is not an outcome count. The
  like-for-like analogue of our `merges` is `congruent` (`congruence.c:948`,
  `COUNTER (congruent, 1, PCNT_VARIABLES, …)`) = **14,461 / 13,227**. **Real outcome gap 3.9× (T105) and
  3.3× (T87), not 182×.** The lever SURVIVES — 7.6× the time for ~3.9× fewer congruences is still the
  largest single Tier C item — but do not size the fix off the retracted figure.
- **▶▶ THE OUTCOME GAP IS CLOSED AS AN ARTIFACT — V6's GATE MATCHING IS AT PARITY** (probes 1580063/1580064,
  Log). Per kind on T105: **AND 3,423 vs kissat's 4,348 (1.27×), XOR 268 vs 531, ITE 0 vs 18.** kissat's
  `congruent` total BUNDLES a binary-clause equivalent-literal scan (`congruent_equivalences`, 24 % of its
  total) that we implement in **V3 Substitute**, a different rule — so the 3.9× compared our V6 against its
  V6+V3. **Do not re-open "V6 finds fewer congruences"; it does not.**
- **▶ WHAT IS ACTUALLY LEFT, and it is a COST problem:** V6 spends **7.6× kissat's congruence time** for a
  comparable outcome, and owns 32.9 %/38.2 % of the clock on `T87`/`T105`. Concrete lead from the source
  read: **kissat checks the table at CONSTRUCTION** (`congruence.c:1206` — on a hit it merges and returns 0
  *without materializing the gate*), where we materialize every rotation into a `std::vector<CongGate>` and
  table it in a second pass. Attack the time, not the merge count.
- **▶ The one real OUTCOME gap is the UNIT channel: `congruent_units` 2,231 vs our 14 (159×)** — C3's unit
  channel, the only per-kind number where we are not close.
- **ITE IS DEAD — do not spend on it.** ZERO merges from the ~2,076 gates that reach canonicalize, and
  **kissat gets 18**. Both solvers get nothing from ITE here.
- **ALREADY REFUTED, do not redo:** "extraction volume is the anomaly" (kissat extracts the same order);
  arity caps (`CONG_XOR_ARITY` 4 = kissat's; our AND cap 64 ≪ kissat's 1e6); a round/effort budget (16 and 11
  rounds, and NEITHER solver meters congruence by effort); **the proof chain / `add_equivalence`
  (`chain_fail = 0`, measured)**; **the stale-key eviction policy (`stale = 0`, measured)**; **the XOR
  collision rate (0.003 % is NORMAL — kissat gets 531 XOR congruences from comparable volume)**; **C0
  sign absorption (read and correct; kissat's extraction does the identical rotation)**.

**THEN, in order** — re-ranked by the 2026-08-03 audit, which corrected one mis-filing (`or_randxor` was
listed as pure-search; it is rule-SCOPE with a twice-measured fix), bounded three items that were listed
without a ceiling, and filed two that were missing. V6 above remains the headline next step (largest single
Tier C item); (a) below is the cheapest Tier B win and is independent of it.
**(a) RE-MEASURE BROAD FACTOR ADMISSION UNDER PLAN REV 78** (`TACTIX_FACTOR_SCOPE_PCT`) — the highest-value
unclaimed item in the repo, and one env var to test. It was HELD on 07-31/08-01 *because* it pushed Tier C to
**1.129× cadical** — and rev 78 (08-02) then shipped R-d at **exactly 1.129×** under "#solved wins". The
holding rationale was superseded by a plan revision one day later and nobody re-ran it. Broad-alone measured
**65.86 (26/28)** and **64.15 (26/28)** vs shipped 105.65 (24/28), i.e. ≈**−20 PAR-2 and +1 solve** if
`or_randxor` lands at its measured 47.55 s. **Untested composition risk:** broad-alone and R-d BOTH gain
`sted5`, so the union is not additive (residual unique gain ≈ `or_randxor` alone), and stacking two 1.129×
Tier C trades has never been measured. Broad-alone has **no recorded Tier C solve loss** — the 27/30 at
468.43 belongs to broad+`nerb`, a refuted arm. Needs one same-window B+C pair with arms {shipped, broad,
broad+R-d}.
(b) `D-VIVIFY-SCHED` — kissat sorts vivify candidates by usage count and **reuses the
assumed prefix**; we do neither, at a **1.3 % hit rate** (2,937 of 233,588 checked). Its closer's trigger
already fired. **Bound:** Vivify is 0.7–1.9 % of clock everywhere probed, so this is a yield fix, not a cost
fix, and the yield direction is unmeasured.
(c) `D-SUBST-UNBOUNDED` — V3 has no effort/round bound where kissat has `substituteeffort`
10‰ + `substituterounds` 2. **Bound (audit):** kissat WITH both bounds substitutes MORE than we do on T105
(19,531 vs 6,548), so the missing bound is demonstrably not what limits our substitution — expect ~0, and
note the unbounded behaviour is part of what bought `veer_axi`.
(d) **`unsat-set-b-fclqcolor`** — 16.6× kissat's conflicts at engine parity (props/conflict 1.06×, slot
2.3 %): **PURE SEARCH**, no named suspect, ≈−19.5 PAR-2 and +1 solve if cracked. **`or_randxor` is NOT in
this class — it was mis-filed here.** Its slot share (1.7 %) says it is not a rule-*cost* problem, but it is
a rule-*scope* problem with a twice-measured fix; see (a).
(e) **C1/C2 mid-round budget** — C2 Strengthen owns **97.8 s of 109.0 (89.7 %)** of `DivS_862_11` in FOUR
applications, and Strengthen + SubsumeForward are the only slot entries neither dosed nor budget-aware (one
round cannot be interrupted). **Newly filed by the audit: this had no ledger row and no checklist item.**
(f) V4 TransitiveReduce at **18.7 M applications** on T87 (15.4 % of clock) — ceiling ≈−5.7 PAR-2, barely
above the noise floor, and both obvious fixes are tier-level negatives (TD-R1).
(g) Track R · R5 S-c (Sweep candidate selection; DoD MUST include sub-solver fuzz legs on both proof
formats). (h) TB4b's remaining chrono divergences. (i) Bench runner: verify LRAT not DRAT (route via
revise-plan).

**STANDING DIRECTIVES.** *Every implemented proof rule ships ON* ([[no-rule-off-configs]]). **Plan rev 77:
no crude approximations of an understood mechanism** — implement what matches or exceeds kissat, or ship the
approximation with a stated reason AND a named closer in the deviation ledger; an approximation of a known
mechanism is **a defect, not a §5.6 arm**. **Plan rev 78: when #solved and PAR-2 conflict, #SOLVED WINS**
(cost still measured, reported, and given a closer; losing a solve is never covered).

**MEASUREMENT HYGIENE — read before trusting any delta.** Tier C noise floor **~10.8 PAR-2** between
bit-identical arms — **but that bar was itself measured under contention** (window `20260731-013523`,
01:35–03:28, overlapped the full Tier C array `20260731-021615`, 02:16–04:04, by ~1 h 12 m). Treat 10.8 as an
upper bound on quiet-partition noise, and re-measure it on an idle partition before any verdict hangs on it.
**`j3037`, `jgiraldezlevy`, `urq45` are measured perturbation-FRAGILE** (probe 1579386): stable under
replication (≤1.0 %) but **8–23× under unrelated knobs**, so small Tier C aggregates near them
are unreliable — quote per-instance replicate ranges. Same shape as **L20**'s ContextModel. **Never mix 14-
and 28-instance Tier B PAR-2.**
**The current T3 miss is SMALLER THAN THE NOISE FLOOR** (band 349.05, shipped 358.12, miss **+9.07 PAR-2** =
0.84 floors) — so "the CaDiCaL half is missed" is a *reported* status, not a robust measurement. The
retirement-OFF pass at 1.039× sits ~1.8 floors inside the band and IS robust. **`veer_axi` is likewise a
cliff solve:** in the adjacent window `20260802-093011-B-base` the retirement-OFF base solved it in 286.7 s
against a 300 s cap (98.05, 25/28), so the OFF baseline is 98.05–109.60 across builds and one of R-d's two
headline solves sits ~13 s from the cutoff. The within-window A/B stands; the headline "+2 solves" is partly
a cliff effect.

**CLUSTER PROTOCOL.** Plan §8.1 + slurm-jobs skill (`run.py --tier X --slurm`, fuzz via `tools/fuzz/slurm.sh`,
sbatch-only). M3-Pro numbers are INVALID.

## ▶ CURRENT TIER TABLES (plan §8.1 rev 75 — full per-instance, newest window first)

**Maintenance contract:** every tier run that changes the shipped default, or that a Log entry cites as a
result, replaces the table below. Keep the newest window at the top; drop superseded ones rather than
stacking them. Record the window stamp AND the git SHA — a table without them cannot be reproduced or
compared. **Take the SHA from the run's own `manifest.json`/`results.json`, never from the commit that later
shipped the change** — the 2026-08-02 tables were labelled with the ship SHA and were not reproducible.

#### Tier B (28) — window `20260802-112851` (git `31593d5` + uncommitted R-d, see the reproducibility note below), **new default: R-d variable RETIREMENT**, every arm writing + checking DRAT

`●` marks the frozen **core14**. **Never mix 14- and 28-instance PAR-2.**

| # | instance | c14 | st | tactix | kissat | ×kis |
|---|---|---|---|---|---|---|
| 1 | `or_randxor_k3_n540_m540.cnf.xz` |  | UNK | 300.0 **TO** | 13.1 | 22.9 |
| 2 | `unsat-set-b-fclqcolor-10-07-09.sat05-1282.re` |  | UNK | 300.0 **TO** | 17.6 | 17.0 |
| 3 | `sted5_0x24204-50.cnf.xz` |  | SAT | 243.5 | 8.8 | 27.7 |
| 4 | `veer_axi_yosyshq_appnote_123_veer_axi-p23_tr` |  | UNS | 200.0 | 3.7 | 53.5 |
| 5 | `DivS_862_11.cnf.sanitized.cnf.xz` |  | SAT | 127.1 | 19.6 | 6.5 |
| 6 | `PRP_30_38.cnf.xz` |  | SAT | 56.4 | 17.4 | 3.2 |
| 7 | `2013113162201nw.shuffled-as.sat03-1668.cnf.x` |  | SAT | 48.9 | 6.0 | 8.1 |
| 8 | `b22_1.cnf.xz` |  | UNS | 34.8 | 9.7 | 3.6 |
| 9 | `FmlaEquivChain_4_6_6.sanitized.cnf.xz` | ● | UNS | 29.8 | 13.0 | 2.3 |
| 10 | `summle_X4044_steps7_I1-2-2-4-4-8-25-100.cnf.` | ● | SAT | 20.3 | 6.6 | 3.1 |
| 11 | `mp1-klieber2017s-0500-023-t12.cnf.xz` |  | SAT | 18.3 | 13.6 | 1.3 |
| 12 | `pyhala-braun-sat-35-4-04.shuffled.cnf.xz` |  | SAT | 15.3 | 4.1 | 3.7 |
| 13 | `9vliw_bp_mc.shuffled.cnf.xz` | ● | UNS | 14.1 | 8.5 | 1.7 |
| 14 | `mod4block_2vars_10gates_u2_autoenc-sc2009.cn` | ● | SAT | 13.3 | 18.9 | 0.7 |
| 15 | `rbsat-v760c43649gyes10.cnf.xz` | ● | SAT | 13.0 | 5.9 | 2.2 |
| 16 | `circuit_48in64out_with_800gates_4in4out_dist` | ● | SAT | 10.9 | 11.2 | 1.0 |
| 17 | `mp1-blockpuzzle_5x10_s7_free4.cnf.xz` | ● | UNS | 10.7 | 9.2 | 1.2 |
| 18 | `SCPC-500-5.cnf.xz` |  | UNS | 9.5 | 12.2 | 0.8 |
| 19 | `DecompressReader_read_12.cnf.xz` |  | UNS | 8.3 | 7.1 | 1.2 |
| 20 | `x9-08014.sat.sanitized.cnf.xz` | ● | UNS | 8.0 | 10.9 | 0.7 |
| 21 | `hwmcc17miters-xits-iso-oski15a08b00s.sanitiz` | ● | UNS | 7.5 | 3.5 | 2.1 |
| 22 | `SC25_Timetable_C_392_E_45_Cl_25_D_7_T_50.nor` |  | SAT | 7.3 | 9.3 | 0.8 |
| 23 | `qwh.50.1250.shuffled-as.sat03-1655.cnf.xz` | ● | SAT | 4.4 | 11.3 | 0.4 |
| 24 | `square.2.0.i.smt2-cvc4.cnf.xz` | ● | UNS | 3.6 | 3.5 | 1.0 |
| 25 | `combined-crypto1-wff-seed-1-wffvars-450-cryp` | ● | SAT | 3.0 | 4.2 | 0.7 |
| 26 | `oddball_13_5_ttf.normalised.cnf.xz` |  | UNS | 2.6 | 19.4 | 0.1 |
| 27 | `battleship-14-26-sat.cnf.xz` | ● | SAT | 1.2 | 6.9 | 0.2 |
| 28 | `clique_n2_k10.sanitized.cnf.xz` | ● | UNS | 0.1 | 6.1 | 0.0 |
|  | **PAR-2 / solved** |  |  | **75.41** (26/28) | 10.05 (28/28) | **7.504** |

Same window, same binary, retirement OFF (**the arm ran with NO env var set — see the reproduction trap
below**): **109.60 (24/28)**. R-d is **−34.19 PAR-2 and +2 SOLVES** — it gains **`veer_axi` and `sted5`**,
both previously timeouts, and loses none. Shipped under plan rev 78 (#solved wins); its Tier C cost is
recorded below and in ledger row `D-RETIRE-TIERC`.

**▶ REPRODUCIBILITY NOTE for both 2026-08-02 tables (audit 2026-08-03).** The artefacts record git
**`31593d5`** (08-02 10:50), not `a927131` (08-02 17:30) as these headings previously claimed: the binary
`build/pin-rd-c/tactix` was built from an **uncommitted working tree**, R-d first lands in `4f88e10`
(13:15), and `a927131` is the ship commit 6 h after the run. The measured `retire` arm IS behaviourally the
shipped default — `git diff 4f88e10..a927131 -- src/ include/` is only the default flip
(`kernel.hpp retire_enabled false→true`, `main.cpp` env parse) — but **no recorded SHA reproduces this
window.** **Reproduction trap:** at `31593d5` `main.cpp:287` was a *presence* check
(`if (getenv("TACTIX_RETIRE"))`), so `TACTIX_RETIRE=0` on THAT binary turns retirement **ON**. Value parsing
(`*rt != '0'`) exists only from `a927131`. To reproduce the OFF arm at that build, unset the variable.

#### Tier C (30) — window `20260802-112853` (git `31593d5` + uncommitted R-d, see the reproducibility note above), **new default: R-d variable RETIREMENT**, every arm writing + checking DRAT

| # | instance | st | tactix | cadical | ×cad |
|---|---|---|---|---|---|
| 1 | `j3037_9_rggt_b.cnf.xz` | SAT | 1055.3 | 26.2 | 40.23 |
| 2 | `jgiraldezlevy.2200.9086.08.40.41.cnf.xz` | SAT | 808.9 | 17.3 | 46.86 |
| 3 | `T87.2.0.cnf.xz` | UNS | 784.0 | 400.6 | 1.96 |
| 4 | `urq45.cnf.xz` | UNS | 645.1 | 995.1 | 0.65 |
| 5 | `T105.2.0.cnf.xz` | UNS | 565.1 | 258.9 | 2.18 |
| 6 | `asconhashv12_opt64_H4_M2-LOD9_m0_2_U19.c.cnf` | UNS | 533.2 | 254.1 | 2.10 |
| 7 | `minxor128.cnf.xz` | UNS | 493.2 | 277.3 | 1.78 |
| 8 | `goldcrest-and-9.cnf.xz` | UNS | 355.0 | 180.8 | 1.96 |
| 9 | `sat-bench-trig-bhaskara.cnf.xz` | UNS | 343.3 | 413.8 | 0.83 |
| 10 | `ecarev-110-4099-22-30-7.cnf.xz` | SAT | 280.0 | 195.9 | 1.43 |
| 11 | `mod2c-rand3bip-sat-250-2.shuffled-as.sat05-2` | SAT | 247.0 | 186.7 | 1.32 |
| 12 | `circuit_32in32out_with_500gates_6in6out_dist` | SAT | 226.2 | 502.9 | 0.45 |
| 13 | `constraints_18_0.3_2.sanitized.cnf.xz` | UNS | 219.8 | 266.4 | 0.83 |
| 14 | `Grain_no_init_ver1_out200_known_last105_0_u.` | UNS | 216.8 | 262.9 | 0.82 |
| 15 | `ER_400_20_4.apx_2_DC-ST.cnf.xz` | UNS | 200.7 | 176.8 | 1.14 |
| 16 | `php17-mixed-35percent-blocked.cnf.xz` | UNS | 192.6 | 253.7 | 0.76 |
| 17 | `StConn_8_32.sanitized.cnf.xz` | UNS | 184.9 | 1006.5 | 0.18 |
| 18 | `SCPC-1000-18.cnf.xz` | SAT | 172.8 | 27.2 | 6.35 |
| 19 | `constraints_25_4_5_12_12_0_0_0.sanitized.cnf` | UNS | 150.8 | 204.0 | 0.74 |
| 20 | `EDP3-11000.cnf.xz` | SAT | 135.2 | 211.3 | 0.64 |
| 21 | `frb45-21-2.used-as.sat04-884.cnf.xz` | SAT | 94.5 | 165.4 | 0.57 |
| 22 | `DLTM_twitter690_74_16.cnf.xz` | SAT | 74.9 | 248.4 | 0.30 |
| 23 | `Ptn-7824-b18.cnf.xz` | SAT | 72.8 | 10.7 | 6.81 |
| 24 | `aes_decry_2_rounds.debugged.cnf.xz` | UNS | 71.9 | 76.5 | 0.94 |
| 25 | `em_11_3_4_cmp.cnf.xz` | SAT | 70.8 | 125.8 | 0.56 |
| 26 | `mp1-Nb5T15.cnf.xz` | SAT | 70.7 | 134.1 | 0.53 |
| 27 | `1-ET-256-K-70.sanitized.cnf.xz` | UNS | 36.6 | 29.0 | 1.26 |
| 28 | `af-synthesis_stb_50_120_4_sat.cnf.xz` | SAT | 34.0 | 144.3 | 0.24 |
| 29 | `128_125.sanitized.cnf.xz` | UNK | 24.5 **OOM** | 4.0 **OOM** | — |
| 30 | `ContextModel_output_7_3_9.bul_.dimacs.cnf.xz` | SAT | 7.6 | 67.0 | 0.11 |
|  | **PAR-2 / solved** |  | **358.12** (29/30) | 317.32 (29/30) | **1.129** |

Same window, same binary, retirement OFF: **329.63 (29/30) = 1.039× cadical**. **T3's CaDiCaL half is NOT met
at the shipped default (1.129×)** — a deliberate, recorded trade under plan rev 78, since R-d buys +2 Tier B
solves. The cost is **three instances** (j3037 +707.6, jgiraldezlevy +602.4, urq45 +444.4 = **+1,754.3 s**,
i.e. **96.5 % of the +1,817.8 s gross regression** — seven other instances add the remaining +63.5 s — against
a net **+854.75 s**, so the three are what flip the net sign); excluding them R-d *improves* Tier C by ~900 s.
Those three are **measured perturbation-FRAGILE, not damaged** (probe 1579386, `D-RETIRE-TIERC` closed): they
retire 251/1/1 variables respectively and swing 8–23× under unrelated knobs. `128_125` is unsolved for
**every** arm including cadical by **OOM at 8 GB**, not timeout — the table row says OOM, and it flatters both
ratios by contributing 2×1200 s to numerator and denominator alike (dropping it moves shipped 1.129 →
1.172×).

**▶ THE TAIL IS THE GAP — do not read this aggregate as a broad loss (audit 2026-08-03).** The **median
per-instance ratio is 0.830× cadical**; we are FASTER than cadical on **15 of 29** solved instances and below
0.5× on five. 13 losses total +3,563 s against 16 wins totalling −2,339 s. **Excluding the three fragile
instances the shipped arm is 304.97 = 0.971× cadical** (inside T3's band with margin) **and the retirement-OFF
arm inverts to 1.077×** — i.e. which arm looks better depends entirely on whether the fragile three are in the
average. **cadical is also not the hard baseline here: same-tier kissat scored 229.64** (`20260730-085509`),
so Tier C is ~**1.56× kissat**, and no Tier C window has carried a kissat arm since 2026-07-30.

**Noise floor, measured (window `20260731-013523`):** three **bit-identical** arms (a payoff-bound sweep
that turned out to be inert — Log) scored **346.16 / 356.95 / 353.99**, i.e. **10.8 PAR-2 of aggregate
spread between identical configs**, and a per-instance range averaging **24.7 s** (goldcrest alone
**245 s = 72 %**). Read every Tier C delta below that bar as noise, and prefer per-instance
evidence with its own replicate range over an aggregate difference. **Caveat (audit 2026-08-03): this window
ran 01:35–03:28 and overlapped the full Tier C array `20260731-021615` (02:16–04:04) by ~1 h 12 m plus ~20
single-instance probes**, so 10.8 is an upper bound on quiet-partition noise, not a clean measurement of it.
Re-measure on an idle partition before letting a verdict rest on the bar itself.

**Older windows** (aggregates only — the per-instance detail is in each run's `results.json`):
**`20260801-105456-B` shipped 105.65 (24/28) · broad-alone 64.15 (26/28) · nerb-alone 127.42 (23/28) · broad+nerb 113.45 (24/28), kissat 9.91** and **`20260801-092459-C` shipped 323.88 (29/30) vs cadical 315.38 = 1.027×, broad+nerb 468.43 (27/30)** — task #11's factorial; **both shared the partition with a concurrent TB4b array, so absolute PAR-2 is not cross-window comparable** (within-window A/B and the cadical ratio are). ·
`20260731-013514-B` 105.30 (24/28) · `20260730-142436-B` 105.40 (24/28) · `20260730-111850-B` 105.21
(24/28) · `20260730-085538-B` 103.24 (24/28) · `20260729-220951-B` 102.28 (24/28), the first run on the
expanded list. Tier C: `20260731-013523` 331.60 (29/30) vs cadical 316.25 · `20260730-142439` 323.67
(29/30) vs 313.18 · `20260730-111853` 324.24 (29/30) · `20260730-085509` **`x2rate8` arm** 319.19 (29/30) vs
cadical 317.04 (**the arm literally named `shipped` in that window is 397.94 at 28/30** — 319.19 is the arm
subsequently ADOPTED; the gate section below states this correctly)
· `20260729-094225` 361.89 (**29/30**, not 28/30) at X2 20‰, the pre-fix baseline. **The shipped Tier C
ratio moved 1.007–1.066× across these windows at an unchanged default; it is now 1.129× at the R-d default —
so quote a window, never a single best number.** **Pre-2026-07-29 Tier B numbers (7.94 / 9.27 / 9.46, kissat
8.51) are `core14` only** (7.94 and 9.27 are cross-window aggregates, not any single run's PAR-2).

**▶ PROTOCOL BREAK — pre-2026-07-28 Tier C aggregates are NOT comparable to anything after it.** Until the
like-for-like proof protocol settled, an instance whose DRAT check exceeded the 2400 s checker budget was
scored **UNSOLVED even though tactix solved it**, costing a full 2×1200 s penalty. Verified example:
`20260727-082011-C` scored `urq45` unsolved at a **354.5 s solve** because its check took **2400.1 s**.
Such windows overstate tactix's Tier C PAR-2 by up to ~+68. Never quote a pre-07-28 Tier C aggregate
against a current one.

## Standing lessons — measured, do not re-derive

Full detail in the cited commits/reports. Numbers are cited across the repo — **never renumber**.

- **L1 · Removing BVE from preprocessing *alone* loses** (Tier B 20.6→32.8, C −2 solves; `benchmarks/reports/2026-07-20-bve-tierC-ab.md`) — valid
  only as half of the arrangement.
- **L2 · During-search BVE *alone* loses** (three variants, all worse; `775eefc` era). Cause is trajectory damage (L7/L9), NOT per-phase overhead
  — the overhead version was refuted.
- **L3 · `NoOp` is not a no-op:** a probing round that changes nothing still reorders watch lists (battleship 12× conflicts; `6a8d159`).
  Correction 2026-07-21: kissat has NO separate non-mutating probing engine (one shared `proplit.h` template) — **how it tolerates probing's
  watch mutation is UNKNOWN**; any P2-style design is a novel mechanism with novel-mechanism risk.
- **L4 · The BVE dose curve is U-shaped**, not monotone (crypto1: 8.1 s at ~300 eliminations → 45.5 s at 0 → best ~71 via the growth-slack ramp).
  Elimination *quality* beats quantity.
- **L5 · The bimodal-BVE saga is CLOSED** — a Factor↔BVE *ordering* interaction (`5111bc7`). Do not retry: resolvent-subsumption alone,
  escalating bound alone, `--bve-gates=none`, deep Strengthen/Subsume interleaving, static "skip BVE on harmful structure" separators (best 4/13
  misclassifications ≈ chance, `benchmarks/reports/2026-07-20-bve-skip-feasibility.md`). Exception per L9: a *density* gate on during-search
  doses DOES separate.
- **L6 · Measurement hygiene:** solo spot-checks MISLEAD; only same-window A/B at equal concurrency counts; never compare across job counts. (The
  old 20.60–23.01 noise band was M3-era — re-establish bands on the cluster before trusting small deltas.)
- **L7 · Repeated during-search BVE costs SEARCH TRAJECTORY** (r=+0.99 with conflicts; per-phase overhead r=+0.01 DEAD, resolvent work r=−0.04
  DEAD; sweep table in `775eefc`). Chaotic in the interval — no "safe interval" exists, the interval is the wrong control. Method: the
  pre-registered prediction kept a mid-sweep false confirmation out of the writeup.
- **L8 · Ablating ONE feature does not measure that feature** — survivors over-invest, so a single-ablation delta is an inflated upper bound.
  **Always run the all-off control** (it killed the congruence lead on FmlaEquiv and the first chrono verdict). Search features too.
- **L9 · During-search elimination is trajectory poison on binary-SPARSE instances and pays on dense ones** — "when to eliminate" is the measured
  lever, worth −7 B / −32 C PAR-2 from one crude feature (density ≥6), and kissat lacks the decision entirely (4× slower than its own
  `--no-eliminate` on mod4block). Plan §5.6 "elimination-dosing exhibit" (rev 68) makes subsuming these gates a phase-6 obligation. Gate report
  §2 banks the measured-dead knobs (bound coupling, removal-only marking, dosed-BVE delay) + the build-race probe hazard (build → verify →
  submit).
- **L10 · The `--no-eliminate` control triad** (probes 1561949–52, 1562429–30): **mod4block damage is instance-intrinsic** (kissat pays 4.14×
  wall / 4.03× conflicts with its OWN gate scheduling ~100 % of candidates, 763 eliminated in 9 calls — no reference solver defends; the density
  gate is ours alone); **crypto1 is tactix-specific scheduling** (kissat eliminates 601 ≫ our 151 while gaining 2.23× — quantity exonerated);
  **mp1 benefits kissat ~27 %** from elimination (NOT "barely moved"), our post-dedup insensitivity = dedup + dosed-Factor already harvesting it,
  sentinel envelope 5.4–5.8 s stands. Forced doses on mod4block: 34 eliminations / 3.53 M resolutions / 3.70× conflicts, candidate set never
  shrinks (n≈350 across 30 doses) — the decline overhead I3/I7 target.
- **L11 · The rules we still LACK are ≤ 9 % of the gap to kissat on Tier C** (window `20260727-002213-C-kabl`): kissat with sweep + reorder +
  bumpreasons + randec + eagersubsume + definitions ALL disabled loses only 14.3 PAR-2 (228.14 → 242.46) against our 161; `--reorder=0` costs it
  *nothing*. So **V5 Sweep and S7 Reorder are inventory completeness, not the Tier C lever** — the gap is quality and dosing of what we already
  have. Exception: `--bumpreasons=0` costs kissat 37.9 alone (more than all six together — L8 inflation, but the largest single item), which is
  why reason-side bumping was implemented the same night. Do not re-open Sweep/Reorder without new evidence.
- **L12 · Decompose per instance BEFORE reading a trend off a tier aggregate.** The 2026-07-27 Tier B "walk-dose regression" was not a dose
  effect: a 4-instance × 8-dose grid (probe 1567225) showed two drivers dose-FLAT, one dose-CHAOTIC (crypto1, 11.8× spread, no trend), one with a
  real threshold — while all four responded to the `bumpreasons` rate. Fixing it (10 → 20) took Tier B 14.57 → 7.57 and flattened the Tier C dose
  curve (279.8–314.5 over 400–800‰). The aggregate had read a trend off one chaotic instance at one grid point, and a whole "every lever is
  two-sided" narrative was built on it. Same shape as L7, a new mechanism.
- **L13 · The more PERMISSIVE proof format is not the safer one to check against.** DRAT accepts a trivially-true clause for free (negating it
  self-conflicts, so RUP passes without reading the formula); every LRAT checker walks the hint chain and rejects a hint-less line. So a
  congruence bug emitting `<id> <lits> 0 0` sat behind a *verified* DRAT proof on every tier run and fuzz leg until both formats ran over a whole
  tier (2026-07-27, `tierC/T105.2.0`). Cross-format checking is coverage, not redundancy; the two debug asserts in `Proof::add_rup` (no
  tautology, no empty LRAT chain) are the cheap permanent version. Corollary: a checker that OOMs (cake_lpr needed 4.2 GB where the array task
  had 8 GB total) is indistinguishable from a rejection unless the runner records the checker's own output — it did not, and that cost a probe.
- **L14 · An internal cross-instance contrast is NOT a defect signal — get the reference's number on the SAME instance.** "Our minimization
  removes 21.6 % on FmlaEquiv against ~40 % on the other two" looked like a located defect; kissat removes **22.1 %** there and ~42 % on the
  others, i.e. parity all along (2026-07-28, probe 1570882). Same family as L8. Corollary: reference counters may need a non-default build
  (kissat's METRIC tier is compiled out of `-s`), and `tools/solvers/bin/*` are SYMLINKS into the reference clones — build such a variant OUTSIDE
  the repo or the pinned baseline binary is silently replaced.
- **L15 · A rule family's value can be non-separable, and hand-designed admission on such a family is a trap.** X1/X2/X4 alone cost 21.79 / 53.26
  / 13.08 Tier B PAR-2; together 9.22 (shipped 7.94) — they repair each other (X2 adds ternary resolvents, BCE deletes blocked clauses), so the
  one-at-a-time A/B behind three "ships OFF" verdicts measured each rule at its worst. Worse, **four independent admission gates all lost badly
  to NO gate** (dose sweep, preprocess-only, density, yield backoff: 27–52 vs 9.17), each removing one rule's rounds and leaving another's damage
  unrepaired; three died on mod4block alone (L10's intrinsic pathology). Method: measure a family TOGETHER before judging a member, and "a clean
  separating feature exists" is insufficient — the yield axis separated by an order of magnitude (13 vs 477–6,726 ticks/deletion) and gating on
  it still lost 43 PAR-2. (`benchmarks/reports/2026-07-28-all-rules-on.md`.)
- **L17 · A rule's whole damage can live in its FIRST application — then dose, rate and round-count are not
  levers at all, and only per-instance ADMISSION is.** On `j3037_9_rggt_b`, BCE capped at ONE round (7,951
  clauses deleted, 200 k ticks = 0.02 % of the solve's work) costs **952.7 s / 7.35 M conflicts** against
  **148.6 s / 1.28 M with BCE off** — while 48 uncapped rounds cost 978.4 s, i.e. rounds 2–48 are ~free and
  round 1 is everything. µs/conflict is flat (111–129) and `c timing` prices BCE's own runtime at **1.385 s
  of 983 s (0.1 %)**, so the cost is purely what it does to the search. Two corollaries. (a) The opposite
  Tier-B mechanism — "BCE's value is one round on the raw formula, its cost is the rounds after it"
  (all-rules-ON report) — is INSTANCE-SPECIFIC and inverts here; do not carry a dose mechanism across
  corpora. (b) Method: a monotone-looking trend across arms that differ in WHICH RULES RUN is not a
  controlled comparison — the first version of this finding read exactly such a trend off the −X2/−X4 arms
  and got the causality backwards. Same family as L12/L16. (probes 1573169–77, 1573242–44; Log 2026-07-29.)
- **L18 · Every STATIC control over BCE has now been measured, and each one trades j3037 against
  mod4block.** on/off (Tier B 9.46 vs 50.96 — but 9.14 vs 8.72 EXCLUDING mod4block), round cap (L17),
  dose/rate (L15 + the 2026-07-29 rate work), and cadical's occurrence cap (j3037 993 → 354 s; mod4block
  13.2 s → TIMEOUT at 100 AND at 1000). The two instances sit at OPPOSITE ends of the same axis:
  mod4block needs BCE to delete 6,343 clauses in DENSE neighbourhoods, j3037 is destroyed by exactly
  those deletions. **Do not look for a fifth static setting — the exhaustion IS the result**, and it makes
  BCE admission the §5.6 exhibit's cleanest labelled signal. Corollary: a reference's own limit is not
  portable when we depend on the rule and the reference does not (cadical ships `block` OFF). Also note
  kissat does mod4block in 19.4 s with NO BCE, so our dependence there is a tactix-specific crutch.
  (probes 1573320/1573405–07; Log 2026-07-29.)
- **L23 · AN ALL-OR-NOTHING SAFETY SKIP WHOSE PRECONDITION IS PER-ITEM BECOMES UNIVERSAL AT SCALE — scope the
  skip to the granularity the PROOF needs, not to the whole round.** V3 Substitute's step-(A) hazard check
  refused to rewrite when any binary pair had ≥2 live copies (`kernel_delete_binary` cannot name two clauses)
  and skipped the ENTIRE ROUND, on a comment reading "Rare; the erratum's finer per-SCC skip is a future
  refinement". "Rare" was a claim about a LITERAL while the skip was per ROUND, so P(round survives) decays
  with the marked set — and the instances richest in equivalences mark the most literals. Measured 2026-08-01:
  on `veer_axi` V3 detected **1,458,092 non-trivial SCCs**, marked 1.8 M literals, and lost **5 of 5 rounds**,
  substituting **0** variables and wasting all **179,110** equivalences V6 had just handed it — because
  **284 literals out of 406,256 (0.07 %)** were hazardous. The rule was most inert exactly where it would pay
  most. Poisoning only the offending equivalence CLASS (both endpoints) preserves the all-or-nothing property
  the proof actually requires and took Tier B **104.06 → 95.61 with `veer_axi` TO → 238.7 s**. Method: a
  deferred "rare case, coarse handling" note is a MEASURABLE claim — instrument the skip rate before
  believing it, and read the skip's granularity against the size of the set it can veto.
- **L22 · A STRESS KNOB IS WORTHLESS UNTIL IT IS PROVEN TO FIRE ON THE SIZE CLASS THE FUZZ HARNESS ACTUALLY
  GENERATES — and the counter that proves it must sit at the SOURCE.** Task #10's abort paths were fuzzed
  4 × 10,000 cases, 0 failures, and the whole thing was **vacuous**; three independent ways, each of which
  produces a GREEN result that means nothing. (a) **Wrong binary** — the env check ran `pin-t10-b`, built
  before the knob existed, so the arm was silently ignored and the two arms came out byte-identical
  ([[ab-arm-identical-artefacts]], caught by exactly that signature). (b) **Unreachable cadence** — the poll
  sites were gated on `!(++tick & 255)` for throughput, so formulas smaller than one poll window never reach
  a first check at all: measured, `battleship` (65,573 conflicts) aborts **zero** times at every setting, and
  the harness's random CNFs are far smaller. (c) **Blind counter** — the extraction abort sites did not
  increment (a "the caller counts it" comment), and the caller's check sat behind an earlier
  `if (gates.empty()) return`, so a reading of 0 could not distinguish *didn't happen* from *wasn't
  measured*. Fix for all three: count inside `kernel_terminating()` ITSELF (`stats.stop_fuzz_fires`), so
  reachability is independent of any abort site remembering to increment, and bypass the production cadence
  whenever the stress knob is set. Only then did corpus 002/005 show **6 fires**. Method: before trusting a
  stress leg, run the knob against a handful of corpus regressions and require a NON-ZERO fire count.
  (Probes 1578209–13; Log 2026-08-01.)
- **L21 · MEASURE A CANDIDATE FIX AGAINST THE SHIPPED DEFAULT, NOT AGAINST THE BROKEN ARM IT REPAIRS.** The
  task-#11 reason-side arm was probed against *broad admission* — the configuration it existed to repair —
  and read as a 2.4× win on ContextModel (403 → 166 s). Against the **shipped default**, which is what any
  ship decision is actually measured against, the same instance is **19.1 → 189.6 s, still 10× WORSE**. The
  arm was never a fix; it was a partial recovery of damage the default does not have. Its tier verdict then
  came out flatly negative (Tier B **alone** 127.42 vs shipped 105.65, −1 solve). Corollary, and the second
  half of the same error: **do not estimate a tier from a two-instance decomposition** — from ContextModel
  −288 s and jgiraldez −431 s the predicted Tier B/C were ~87/~334, measured 113.45/468.43, because the other
  26 instances moved (`DecompressReader_read_12` 8.5 → 300 TO). That half is L12/L16 committed again, so the
  standing rule now has three sightings. (Report `2026-08-01-task11-factor-admission-mechanism.md` §4.)
- **L20 · AN INSTANCE CAN BE ROCK-STABLE UNDER REPLICATION AND CATASTROPHICALLY FRAGILE UNDER PERTURBATION —
  so a huge regression there is WEAK evidence about the mechanism that perturbed it.** On 2026-08-01 two
  entirely unrelated changes each cost `ContextModel_output_7_3_9` ~**19–20×**: TB4b's conflict-clause reuse
  (20.06/19.97 → 382.74/376.03 = 19.1×, a backjump/learning change) and task #11's broad Factor admission
  (20.4 → 453.7 ≈ 20×, a BVA/preprocessing change). Its OFF replicate pair is **the most reproducible in
  Tier C**, so this is not the L12 noise-chaos pattern — it is stable-but-knife-edge: our default trajectory
  there is unusually GOOD (**20.4 s vs cadical's 64.2 s — we are 3× FASTER**) and almost any substantial
  perturbation falls off it. Consequence for task #11: its founding framing — ContextModel's +464 s proves
  "our Factor is 7–28× worse than cadical's" — is **overstated**, because the same instance pays ~20× for a
  change that has nothing to do with Factor. Method: before attributing a large single-instance regression to
  the mechanism you changed, check whether that instance also collapses under an UNRELATED change; if it
  does, it is measuring fragility, not your mechanism. Distinguish from L12 by the replicate spread — L12's
  crypto1 was chaotic *between identical runs*, this one is not. (Log 2026-08-01; TB4b report §Tier C +
  probes 1577487/1577506.)
- **L19 · READ A REFERENCE OPTION'S SOURCE BEFORE INTERPRETING A PROBE THAT SETS IT — polarity and even
  liveness cannot be inferred from the name or the help string.** The task-#11 diagnosis routed its primary
  lever off the probe `cadical --factordelay=0 --factorthresh=0`. Both halves were misread. (a) **`factorthresh`
  is declared at `options.hpp:125` and never read anywhere in cadical's source** — that half of the probe was
  dead code, so the measured effect belongs entirely to the other flag. (b) **`factordelay` is an ALLOWANCE
  ADDED to the elimination-round count**, not a delay in the plain sense (`factor.cpp:941`:
  `delay_limit = eliminations + delay`, skip when `log_active > delay_limit`; kissat `factor.c:1074` is the
  same shape) — so `=0` is the MOST-delayed setting and the probe showed that delaying Factor **more** costs
  cadical 2.3–9×, the OPPOSITE of the "cadical delays, we blast" reading it was given. (c) Checked against the
  instances, the gate **never binds for either solver**: log10(13,797) = 4 ≤ 0+4 on ContextModel,
  log10(2,200) = 3 ≤ 0+4 on jgiraldez, and our analogue (`default_policy.cpp:1176`) admits at
  active < 10⁵ with zero BVE rounds — **behaviourally identical**. A frozen report's routed lever survived
  three windows on a flag that does nothing and a flag read backwards. Corollary, same family as L14: when a
  probe's verdict rests on a reference's knob, `grep` the knob in the reference's *source*, not its options
  table. (2026-08-01; probe 1577487 + `benchmarks/probes/t11-initticks.sh` header.)
- **L16 · A per-instance-REPRODUCIBLE effect can still give a non-reproducible tier aggregate — and an aggregate that matches its prediction
  can be a coincidence of sums.** The slot-rate-2‰ arm saved 26.1 PAR-2 in one Tier C window (1.065× cadical, gate met) and 8.8 in the next
  (1.101×, gate missed), while **28 of 30 instances kept the same sign and near-identical magnitude** — the swing came from a few 500–1100 s
  instances whose absolute times move 50–150 s between windows, not from instability in the mechanism. Also: the 26.1 matched the probe's
  predicted 26 while the probe's premise ("only the two target instances move") was false — ±1,100 s of offsetting per-instance moves summed
  to the right number. So (a) a tier PAR-2 near a gate boundary needs a second window before it is a verdict, and (b) agreement between a
  prediction and an aggregate is not evidence for the prediction's mechanism — check the decomposition it claimed. Converse of L12: there the
  aggregate hid per-instance chaos; here the aggregate hid per-instance ORDER. (Windows `20260729-094225` / `-112753`; Log 2026-07-29.)

## Open items (small, none blocking)

- **Filed by the 2026-08-03 doc audit** (details in the Log entry of that date):
  - **`D-STRENGTHEN-BUDGET`** — C1 Strengthen / SubsumeForward are the only inprocessing slot entries that are
    neither dosed nor budget-aware, and one round cannot be interrupted. C2 Strengthen owns **97.8 s of 109.0
    (89.7 %)** of `DivS_862_11` in **four applications**. Had no ledger row and no checklist item. Route via
    the calculus skill when it becomes a guard change.
  - **`D-FACTOR-REV78`** — re-measure broad Factor admission (`TACTIX_FACTOR_SCOPE_PCT`) under plan rev 78.
    Held on 08-01 at 1.129× Tier C; rev 78 then shipped R-d at exactly 1.129×. ≈−20 Tier B PAR-2 and +1 solve
    if it reproduces. One same-window B+C pair, arms {shipped, broad, broad+R-d}.
  - ~~**T3 instrument gaps**~~ — **BOTH CLOSED 2026-08-03.** `benchmarks/tiers/C-ruleneutral.list` (24 of 30,
    derived from window `20260727-002213-C-kabl`, frozen with its known defect + closer in the header) and
    `tools/bench/subset_score.py` (scores any Tier C run over the subset AND the full tier from one
    `results.json` — the subset is a scoring VIEW, never a separate run). The kissat arm needed no code:
    `run.py` already takes repeatable `--baseline`, it had simply stopped being passed on Tier C. **Standing
    rule from here: every Tier C run carries `--baseline kissat --baseline cadical`**, or T3's kissat half
    goes unmeasurable again.
  - **Tier D does not exist** (no `D.list`, 0 runs) though N3/T4 gate on it — §11 O9, due at phase-3 exit.
  - **Re-measure the Tier C noise floor on an idle partition** — the 10.8 PAR-2 bar was itself taken under a
    ~1 h 12 m overlap with another full Tier C array.
- **Task #39** — `--random-policy` livelock on 5 tiny cases in `tools/fuzz/known-hangs/` (N1 robustness, not soundness).
- demo3 is a SEARCH-only mirror by permanent design (stat contract = §8.4 within-noise; the deferred full-slot-mirror enhancement was dropped
  2026-07-25, user decision).
- Stress-pass ledger: V6 unit-channel DONE (`4f2b53e` + congstress nightly); Factor-mid-stable DONE (2026-07-25 arrangement mid-stable legs,
  9k/0). Post-Track-H: random-policy ASan 8000/0 both formats.
- Routed by the 2026-07-29 review (`benchmarks/reports/2026-07-29-review-post-track-r.md`; `RV-` namespace — Log key): **RV-R2** Reorder has no
  stress policy; **RV-R3** `benchmarks/probes/` needs a retention rule; **RV-R4/RV-R5** small duplication + allocation watch items.
  (**RV-R1** → Track R · R5's S-c definition of done.)

## Non-negotiables status (plan §1.1)

| | Requirement | Status |
|---|---|---|
| N1 | Any applicable rule appliable via API at any time, incl. branching choice | ◐ v1 API frozen (revs 20–21); §8.4 demos 1–3 prove it end-to-end; audits upheld. Two leaks found+fixed (rev 34 payload aliasing; rev 40 off-fixpoint Decide via `trail_tainted`). Final acceptance = CCE plugin (phase 4) |
| N2 | Proof production on every path (DRAT/LRAT) | ◐ DRAT since phase 1; native LRAT 2026-07-14; every fuzz UNSAT checker-verified on all formats under random schedules; audit UPHELD |
| N3 | DefaultPolicy comparable to Kissat (T4 gate) | ◐ T1+T2 passed. **2026-08-03: Tier B(28) 75.41 vs kissat 10.05, 26/28 = 7.504×** (from 105.30/24 — `veer_axi` and `sted5` both now solve). Tier C 358.12 vs cadical 317.32 = **1.129×**, outside T3's band at the shipped default — a recorded rev-78 trade (#solved wins); the retirement-OFF arm is 1.039×. **vs KISSAT on Tier C ≈1.56×** (kissat 229.64, `20260730-085509`; no kissat arm since). Tier A 0.98×. **⚠ Tier D — the tier N3/T4 actually gate on — HAS NEVER BEEN BUILT** (no `benchmarks/tiers/D.list`, 0 Tier-D runs), so every "kissat parity" statement to date rests on a 28-instance hand-curated Tier B. Building Tier D is a phase-3-exit prerequisite (§11 O9) |

## Milestone gates (plan §8.3)

- [x] T1 — **passed 2026-07-14** (Tier A 200/200 PAR-2 0.48; Tier B 9/14 @ 231.6 vs MiniSat 8/14 @ 281.4; report
  `benchmarks/reports/2026-07-14-t1-gate.md`)
- [x] T2 — **passed 2026-07-14** (Tier B 11/14 @ 164.8 vs Glucose 8/14 @ 268.3; props/s geomean 0.92× kissat; report
  `benchmarks/reports/2026-07-14-t2-gate.md`)
- [ ] T3 — within 10% PAR-2 of CaDiCaL on Tier C + CaDiCaL/Kissat parity on the rule-neutral subset (rev 51).

  **▶▶ CURRENT STATUS (2026-08-03) — READ THIS FIRST; the narrative below is chronological and its last
  entry is superseded.**

  | half | requirement | status |
  |---|---|---|
  | CaDiCaL | ≤ 1.10× cadical, whole Tier C | **NOT MET at the shipped default: 358.12 vs 317.32 = 1.129×** (window `20260802-112853`). Miss = **+9.07 PAR-2 = 0.84 × the measured noise floor**, so the failure is not robustly measured either. **MET by the retirement-OFF arm** (329.63 = 1.039×, ~1.8 floors inside) and by the pre-R-d default (1.0068×, 07-30). Deliberate rev-78 trade for +2 Tier B solves. |
  | Kissat | parity on the rule-neutral subset | **NOT MET, and currently UNMEASURABLE.** Only measurement ever: **1.37 ×**, 2026-07-27 (`benchmarks/reports/2026-07-27-tier-c-push.md`) — four shipped changes ago (X2 8‰, `factor_size=5`, V3 per-SCC, R-d). **No 24-instance subset list is materialized** (`benchmarks/tiers/` has none, `tools/bench/` has no subset filter) and **Tier C has carried no kissat arm since 2026-07-30**, so re-measuring means re-deriving the ablation AND re-adding a kissat arm. |

  **Owed before this gate can be evaluated at all:** (1) materialize the rule-neutral subset as a real list
  under `benchmarks/tiers/`; (2) put a kissat arm back on Tier C. Both are prerequisites, not results.
  **Note (audit 2026-08-03, unverified — worth confirming, not banking):** re-deriving the subset from the
  same rule on window `20260727-002213-C-kabl` reproduces the 24 instances exactly, and on the current
  window the shipped R-d arm scores ≈1.005× cadical on that subset while the OFF arm scores ≈1.081× — i.e.
  R-d may *help* on precisely the subset the gate uses to isolate search quality, with its whole cost landing
  on instances the subset excludes. Confirm with a real run before relying on it.

  **History (chronological).** **STATUS CHANGED 2026-07-29 by the
  all-rules-ON directive.** With the rules OFF the CaDiCaL half was MET (2026-07-27: 303.21 vs 304.08 = 1.00×; re-confirmed same-window
  2026-07-29: rules-off 313.20 vs cadical 314.49 = **1.00×**). With every proof rule ON — the shipped default since `3fb85bd` — **tactix 357.23
  vs cadical 314.49 = 1.14×, i.e. OUTSIDE the 10 % band, so the CaDiCaL half is NOT currently met** (window `20260729-003352`, 29/30 all arms).
  The Kissat half was already not met (1.37× on the 24-instance rule-neutral subset). **BUT the regression is TWO INSTANCES, not a broad loss:
  j3037_9_rggt_b (138.8 → 1152.4, +1014 s) and asconhashv12 (324.4 → 627.4, +303 s) account for 100 % of the +1322 s; the other 28 net to zero,
  with minxor128 −234, T87.2.0 −198 and sat-bench-trig −163 paying for the rest.** Recovering those two recovers the gate. Report
  `benchmarks/reports/2026-07-28-all-rules-on.md`
  **▶▶ THE CaDiCaL HALF IS MET 2026-07-30, WITH EVERY RULE ON.** X2's effort share was the missing
  governor: we gave `HyperTernaryRes` **20‰** where cadical's `ternaryeffort` is **8‰**. Same-window A/B
  `20260730-085509` (all arms writing and checking DRAT): **shipped 397.94 (28/30) → 8‰ 319.19 (29/30)
  against cadical 317.04 = 1.0068×, INSIDE the 10 % band, and it GAINS a solve.** Tier B(28) flat
  (103.24 → 103.68, 24/28 both). Not a single-instance artefact: 7 instances win ≥15 s and 5 lose, and it
  is still **−11.8 PAR-2 excluding j3037** (which itself goes TIMEOUT → 392.8 s). Landed in `EFFORT_SPECS`.
  **The Kissat half is still NOT met** and is what remains of T3.
  **▶ SUPERSEDED 2026-08-02 — the CaDiCaL half is NOT met at the current default.** R-d retirement took Tier
  C 329.63 → 358.12 (1.039× → 1.129×) in exchange for +2 Tier B solves, banked under plan rev 78. This entry
  ("THE CaDiCaL HALF IS MET") stood uncorrected for a day and is what the status table above now governs.
  **▶ TESTED AND REJECTED 2026-07-29: a uniform slot rate of 2‰ does NOT reliably clear this half.** Two same-day windows, same arms/throttle,
  every arm writing and checking DRAT, 29/30 throughout: `20260729-094225` shipped 361.89 / **rate2 335.81 = 1.065× cadical 315.42 (inside)**;
  `20260729-112753` shipped 357.31 / **rate2 348.57 = 1.101× cadical 316.58 (outside)** — the saving shrank 26.1 → 8.8 PAR-2, so the criterion is
  not met robustly. Tier B cost 9.10 → 9.53 (+4.7 %, 14/14). **2‰ is not shipped**; `TACTIX_SLOT_PERMILLE` stays a §5.6 dosing arm. Both halves of
  T3 therefore remain open, and the two-instance framing of the regression is superseded: the rate fixes j3037/asconhash reproducibly and breaks
  ContextModel/em_11/frb45/Ptn-7824 reproducibly (28/30 same-sign across the two windows — Log 2026-07-29).

## Phase 0 — setup (complete 2026-07-09)

- [x] Repo + plan + skills + CI; reference solvers + checker toolchain pinned (`tools/solvers/`, `tools/checkers/`); fuzz harness validated
  against stand-ins + deliberately broken wrappers; benchmark runner; Tier A (200 gen) + Tier B (14 SC,
  `benchmarks/reports/2026-07-09-tier-b-freeze.md`) frozen; kissat study notes `docs/design/kissat-notes.md`
- **Exit:** fuzz pipeline end-to-end ✓; Tier A/B frozen ✓

## Phase 1 — core CDCL kernel

- [x] Kernel (arena, watches, trail, VMTF, phases; later EVSIDS heap + target phases), rules M1–M4 + S1/S2 + S4 SwitchMode/stable, binary DRAT +
  competition I/O, registry + guard/instances/apply contract from day one
- [x] Reduce used-bit retention (the s055 trajectory fix); policy-class refactor (rev 17 — ALL scheduling in the Policy; the load-bearing
  architecture decision, [[working-style]])
- [x] Null results recorded: assign-path inlining (memory-bound), LTO; radix bump sort −7 %; binary-reason jumping REVERTED (needs kissat's
  bigbig classifier)
- [x] T1: failed 2026-07-11 (report ranked the levers) → **PASSED 2026-07-14**
- **Exit:** T1

## Phase 2 — search management + API freeze (2026-07-14)

- [x] S3 Rephase + S5 Lucky; `kernel_propagate` extraction (§5.3 primitive); **native LRAT** (CaDiCaL scheme; kissat has none); **Rule API v1
  FROZEN** (revs 20–21) + demos 1–3 (demo3 stat-exact)
- [x] Carried: bias-corrected per-mode EMAs, kissat restart constants, glue recompute+promote, B,B,I,B,B,O rephase; 9vliw → kissat parity; s055
  trajectory gap closed (residual = 1.42× propagation volume — the standing TB5 lever)
- **Exit:** T2 — **PASSED 2026-07-14**

## Phase 3 — inprocessing

Every rule lands N1-exposed + random-policy-fuzzed regardless of whether DefaultPolicy schedules it; per-rule detail in its design doc + commit,
semantics in `docs/calculus.md`. **Since 2026-07-25 the composition is the S4 arrangement, unconditionally** (probing family + simplifiers +
during-search dosed BVE/Factor, density-gated; the legacy preprocessing-only composition was removed with the negative arms — git history has
it).

- [x] **C1 SubsumeForward · C2 Strengthen + the registry-driven slot** (slot = data; the phase-6 arm space) **· C3 Vivify** (LCM scope) **· V1
  Probe · V2 Backbone · V4 TransitiveReduce** (`kernel_delete_binary` primitive) **· V3 Substitute** (first ρ consumer;
  `docs/design/v3-substitute-design.md`) — all 2026-07-15..17; corpus 002/003 found en route
- [x] **E1 BVE — COMPLETE (E1a–E1d)**: drained-root elimination, RUP-only resolvents, syntactic gate extraction, `--bve-gates` knob; design
  `docs/design/e1-bve-design.md`; `bve_stress_policy` reusable. Remaining: ITE/XOR gates + kitten (O11), phase 4
- [x] **Tier C built and frozen** (30 instances; `benchmarks/reports/2026-07-15-tier-c-freeze.md`)
- [x] **Independent-lineage campaign — 4 measured, 0 adopted**, each with a recorded mechanism; strongest phase-6 arms: CMS usage-count Reduce
  ranking, chronological backtracking (TB4), Glucose blocking restarts. Survey `docs/design/independent-lineage-survey.md`
- [x] **N1/N2/N3 audit + canonical-batch fuzz** (2026-07-15) — upheld; deferred backlog `benchmarks/reports/2026-07-15-codebase-review.md`
- [x] **Nightlies GREEN at scale** (DRAT 20k/0 `nightly-rev59-lrat.log`; native-LRAT 20k/0 `nightly-rev59-native-lrat.log` — **the C4-gating leg,
  both paths draw the same inferences**; congruence-stress 10k/0). Lesson: nightly scale catches what `--quick` misses (corpus 006, 007)
- [x] **Calculus formalized** (rev 36): `docs/calculus.md` + `docs/implementation-notes.md` + the calculus skill; **guard purification COMPLETE**
  for every removable deviation (revs 37–43; only D-ROOT remains, by design). Corpus 004/005/006 regression bed
- [x] **V6 Congruence — ships ON** (2026-07-19): AND+XOR+ITE union-find closure; Tier B −30 %, +1 solve; design + report in `docs/design/` /
  `benchmarks/reports/2026-07-19-*`
- [x] **E2 Factor (BVA) — ships ON** (2026-07-20): net-positive after the duplicate-binary deletion fix; S2a/S2b closed 2026-07-24 (steering
  disconfirmed); the mp1 gap CLOSED 2026-07-25 (input dedup + dosing + composite; gate report) — nothing remains on E2
- [x] **Inprocessing revamp — slot ACTIVATED** (2026-07-19): preprocessing at conflicts=0 + `Kernel::dense` shared index + kissat phase
  structure; report + design docs. Superseded as default by the S4 arrangement (2026-07-25)

### Track C — V6 Congruence completion (rev 56) — COMPLETE 2026-07-25

- [x] **C0** XOR output-sign fix (`1a365da`) **· C1** `add_equivalence` all-or-nothing (D-CONG-SKIP; dropping an inference is sound, an
  unverifiable certificate is not) **· C2** degenerate-ITE filter (gates −23 %, equivalences identical) **· C3** the unit channel (`hwmcc` 48.77
  → 6.67 s; the degeneracy ladder was drat-trim-validated in all four shapes BEFORE implementation and turned out to be kissat's own
  construction) **· C4** LRAT chains REPLAYED not derived (checker-order by construction). Detail: Track C commits + Log 2026-07-21
- [x] **C3b** CLOSED 2026-07-25: AND clash → unit, idempotent dup → unary merge, `x≡¬x` → refutation; dup-hint hazard closed in
  `add_equivalence`; residuals in the D-CONG-UNIT ledger row. Tests ×5 + corpus 008/009/010 + fuzz 10k/0 ×4. `2998029`
- [x] **C4-perf** CLOSED 2026-07-25: hwmcc native LRAT **1680 s → 9.64 s (174×)**, proof byte-identical; root cause was O(nvars) fill-initialized
  scratch per proof-chain op, NOT replay propagation ([[per-call-linear-scratch-hazard]]); fix = persistent scratch + O(marked) restore. **No
  `side_ids` machinery needed** — replay architecture vindicated (the `congruence.hpp:30` breadcrumb is moot). `d2f7695`

### Track H — `H` out of the Kernel (rev 60) — COMPLETE 2026-07-23

`H` (VMTF, score heaps, phases, ranking mode) is policy-owned, OPTIONAL state: `k.h == nullptr` is sound, just unguided. Capability payoff (Bump
swappable without touching `kernel.cpp` — the AE-Kissat-MAB arm site) at **zero PAR-2**; every increment gated on trajectory identity +
byte-identical DRAT.

- [x] **H0–H7** landed (design + rejected alternatives `docs/design/h-relocation-design.md`; detail in the H0–H7 commits + Log 2026-07-22/23).
  Live constraints: H4 pimpl (include cycle forces it; kernel non-copyable); H5 null-`H` legal (Decide → index order; Rephase/SwitchMode guards
  require `H` — a pure precondition of `H`-only ops); H6 sub-policy super-class DEFERRED until a 2nd arm exists (§5.5)
- Method lessons: bit-exactness gates the exercised path, adversarial review the unexercised one (caught a lost include + an unguarded deref no
  gate saw); null-path bugs surfaced only under FULL-suite ASan with the default flipped; a structural check can replace a benchmark (zero
  `k.h->` in the propagation loop killed the indirection worry)

### ▶ Track T — the Tier B tail (TB1–TB6; the bare T1–T5 are the §8.3 gates)

Diagnosed 2026-07-21 (snapshot — mp1/crypto1 rows since RESOLVED by S4+dedup and the density gate; FmlaEquiv/x9 remain the pure-search
residuals): the four tail instances were 69 % of Tier B time, each with a different cause.

- [x] **TB1** · congruence is NOT the FmlaEquiv gap — the all-off control (L8's origin): kissat with ALL simplification off (8.68 s) beats kissat
  with any single simplifier off; congruence's worth there ≤ 0.7 s, instance is pure Tseitin AND. Decomposition: conflicts-to-refutation 3.09× ×
  throughput 1.39× (TB5). Structural-hashing gap (kissat merges 241 vars, we merge 0) is real but ≤ 0.7 s — closed as below-noise; watch for
  other instances
- [x] **TB2** · CLOSED 2026-07-25: x9's outlier premise dissolved — 1.17–1.19× vs kissat, inside the healthy band (probe 1562826 + window
  `1241*`); residual rides the FmlaEquiv-class search-quality work
- [x] **TB3** · RESOLVED 2026-07-25: the stale 801 k → 453 k comment is already gone, and Factor is verifiably non-inert on x9 under the
  arrangement (80 factorings @ 371,995 conflicts, probe 1562469); the in-code x9 claim at `factor.cpp:72` was checked and is fresh (347 k vs 364
  k)
- [x] **TB4** · chrono ON by default (`CHRONO_LEVELS=100`); the first measurement (+13 %) was a PORTING ERROR, not a verdict — analyzing from
  above the conflict level silently DROPPED the conflict on 84 % of chrono backtracks; fixed → −2.4 % clean. **`stats.backjump_stale` MUST stay
  0.** Lesson: found only because a negative result was audited rather than accepted (two-agent audit vs kissat AND cadical)
- [ ] **TB4b** · the remaining chrono divergences from that audit: **watch repair onto highest-level literals**, unit chrono routing, snapshot
  timing, `jumpreasons`, cadical's `chronoreusetrail`. Aims at FmlaEquiv's conflict gap. Port ONE behavior at a time; TB4's lesson is to audit
  any negative verdict before accepting it.
  The old live constraint ((A) and watch repair are ONE change) is DISCHARGED by TB4b(B) below.
  - [x] **TB4b(A) · the one-literal-on-conflict-level shortcut** — when exactly one conflict literal sits at the conflict level the clause is
    already asserting, so backtrack to the second-highest level and assign that literal WITH THE CONFLICT AS ITS REASON, learning nothing (fewer
    learned clauses and proof lines) (`Kernel::conflict_reuse`, `TACTIX_CONFLICT_REUSE=1`; `1c5f2c5`, measured `88cff2d`).
    **THE 2026-07-28 NEGATIVE VERDICT IS RETIRED — it was on a BROKEN arm, not a crippled one** (audit 2026-08-01, report
    `benchmarks/reports/2026-08-01-tb4b-watch-repair.md` §1). `analyze_apply` clears `pending.valid` on the reuse path and the INSTANCE path
    checked it, but **DefaultPolicy's micro-loop did not**: every reuse ran the learn/backjump tail on the PREVIOUS conflict's record — stale glue
    sample, `backjump_apply_to` clamped to a stale `jump_level` (forced to 0 when that record was a learned unit), a real `kernel_backtrack` that
    UNDID the reuse's own assignment, then a `backjump_stale` (TB4's must-stay-0 signal). Only DefaultPolicy was hit, so `--random-policy` and the
    corpus stayed green. Fixed `fc57eff`; six other policies had the same unguarded call and are now guarded
  - [x] **TB4b(B) · THE WATCH-REPAIR PRIMITIVE, and the shortcut un-crippled** (`fc57eff`; diagnostic arm `a489bb7`). `kernel_unwatch_large` (the
    inverse `push_large` never had) + `kernel_promote_watched_lit`; the level ranking stays in the rule (`repair_conflict_watches`). Deliberately
    NARROWER than kissat: it runs only when the shortcut fires, so the ordinary learning path is byte-identical — **proved**: arm-OFF and
    pre-change HEAD wrote byte-identical DRAT totals (357,623,690 B over 11 UNSAT proofs). Design + deviations `implementation-notes.md` §12.
    **The mechanism is CHRONO-ONLY** — on a level-sorted trail "one literal at the conflict level" is impossible (the clause would already have
    propagated), so at `chrono_levels=100` a 90-var random 3-SAT gives **0 reuses over 2,105 conflicts**; two versions of the audit test measured
    nothing before that was understood ([[ab-arm-identical-artefacts]]). New `--reuse-stress-policy` forces the ingredients. ctest 38/38
    release+debug; **six fuzz legs 10k/0 each** (DRAT + native LRAT × {default, `--random-policy`, `--reuse-stress-policy`}, ASan+UBSan, corpus
    10/10 incl. 002/005), `backjump_stale` 0 everywhere. **Measured (report §5), two same-window Tier B runs + a 5-arm attribution window
    `20260801-092756`.** The un-crippling is REAL (DRAT bytes −12.0 %; FmlaEquiv **−18.7 % conflicts / −12 % wall, non-overlapping replicate
    ranges**, where the crippled version gives +1.3 %). Cost: **+2.0 % on Tier B(28)** — but **core14 and new14 give OPPOSITE orderings of
    ON vs CRIPPLED** (core14: ON inside the OFF noise band, CRIPPLED ~7 % above it; new14: ON +2 %, CRIPPLED free), so neither aggregate is the
    verdict — L12/L16 in one table. **The July −9.2 % does NOT reproduce for the un-crippled arm on its own instrument.** **Tier C(30)
    `20260801-094032`, replicate spread 2.40: OFF 331.33/333.73 · ON 343.85 · CRIPPLED 342.63 — ON and CRIPPLED indistinguishable**, and the whole
    +3.4 % is TWO instances (ContextModel **19.1×** off the tier's most reproducible instance, af-synthesis 5.1×) against big wins elsewhere
    (EDP3 0.35×, DLTM 0.41×, em_11 0.57×, T105 0.77×). **qwh 3.1× + those two = pre-registered kills K5 → ships OFF, no default flip
    recommended.** Real home = the §5.6 exhibit: **one** reuse halves crypto1's conflicts, 67 quadruple qwh's — chaotic in the dose (L12), and the
    proof-size axis FLIPS SIGN between tiers (B −12.0 %, C +8.1 %), so "fewer learned clauses" is not a proxy for whether it pays
- [x] **TB5** · CLOSED-REFRAMED 2026-07-25 (probes 1562819–26): per-prop cost is AT PARITY with kissat on all residuals — the
  flat-1.39×-throughput premise was stale; the residual is SEARCH SHAPE (s055 props/conflict 1.43× at conflict parity; FmlaEquiv conflicts 1.53×;
  x9 parity). Watch-layout engineering lane measured-dead; s055 → phase-6 admission exhibit; lesson: kissat's `propagations` stat includes
  probing+warming. Log 2026-07-25 late + `9bbcdf2`
- [x] **TB6** · CLOSED 2026-07-25 (probe 1562605, `TACTIX_FORCE_MODE` knob banked): the era-stale 407 k claim does NOT reproduce — default
  alternation BEATS both pinned modes on FmlaEquiv (577 k vs 809 k focused vs 1.21 M stable); focused pinning is catastrophic on s055 (2.54 M vs
  939 k) and bad on x9, but −23 % on clique. Mode schedule is a context-dependent phase-6 arm, not a static defect

### ▶ Track S — the effort-budgeted target arrangement — S4 CLOSED

- [x] **S1** L7 refuted (`775eefc`) **· S1b** BVE growth-slack ramp, first SubPolicy (`a36b4c2`) **· S1c** needed-bound cache + ceiling 8
  (`489fab1`; lesson: the old dose cap was an *accidental governor*, [[accidental-governor-trap]]; owed: Tier D re-sweep at gate time) **· S2a**
  FactorMatchPolicy, steering DISCONFIRMED (27×27 cap is structural; `paths` −4 % shelved) **· S2b** composite grids land default-OFF
  (single-blast composition regresses; reserved for doses — `docs/design/s2b-composite-grids-design.md`) **· S2** probing rules re-measured
  on-cluster: historical SAT-disruption GONE **· S3** P2 follow-ups (probing stress 16.5k/0; `2ed8bcc`)
- [x] **S4 · the TARGET ARRANGEMENT — GATE PASSED, DEFAULT ON (2026-07-25).** B+C one-unit: arr {B 17.45, C 552.91} vs legacy {B 43.86, C
  537.83}; mp1 5.34 s beats kissat 9.09 (proof VERIFIED); then the density-gated-doses residual fix (`1dbd439`) → **B 10.62, C 520.76**. Root
  causes + measured negatives: `benchmarks/reports/2026-07-25-s4-gate-mp1.md` (its §2 banks the dead knobs, [[s4-gate-negatives]]); dosing design
  frozen `docs/design/s4-factor-dosing-design.md` (§3.e bound coupling REVERSED by measurement). Legacy composition + negative arms removed
  2026-07-25 (user directive)
- [ ] **T3 gate re-measurement** + report to `benchmarks/reports/`

### ▶ Track W — S6 `Walk` local search (plan rev 71) — the T3 lever

Diagnosed 2026-07-26 (debug-performance on the SCPC-1000-18 timeout; report `benchmarks/reports/2026-07-26-scpc-walk-profiling.md`). **Local
search is the largest missing mechanism**: `--walkeffort=0` takes kissat's Tier B PAR-2 from 8.62 to **29.22** (battleship 19.4×, mod4block 8.4×,
crypto1 4.5×, clique 2.1× — our own historical pain set), and our shipped 9.93 already beat that walk-less kissat by 3×. S6 is an `H`-only policy
op (no proof delta, no calculus entry): `src/policies/ops/walk/` + `docs/design/s6-walk-design.md`. **W4f closed 2026-07-27; the only open Track
W item is W4e, a phase-6 hand-off with nothing to build now.**

- [x] **W1** walker core (`8da55c9`): walker-owned value array + CSR occurrence index (kernel untouched ⇒ legal at ANY level, N1), ProbSAT
  break-value scoring with the CB table, budget in `walk_steps`, best assignment into `h->saved`. 7 unit tests; fuzz quick DRAT / native LRAT /
  **random-policy** 10k/0 each (Walk reachable at arbitrary schedule points), re-run on the SHIPPED build DRAT / native-LRAT /
  `--walk-stress-policy` 10k/0 each (jobs 1563687–89; **the two debug legs under ASan+UBSan**)
- [x] **W2** schedule — **Walk AUGMENTS Best, it does not replace it.** Porting kissat's `B,W,I,B,W,O` literally REGRESSED Tier B (13.63 vs
  10.04); the dry-walk control (`TACTIX_WALK_DRY`) located the cause as the LOST BEST CONSUMPTION, not the walk (kissat saves phases at ASSIGN
  time, we save at unassign). Shipped `B,B+W,I,B,B+W,O`; `rephase.count` drives rotation + interval
- [x] **W3** warmup — measured, **NOT adopted** (battleship/rbsat bit-identical, mod4block 1.32 M → 2.07 M conflicts): our saved phases already
  come from a real trail and are propagation-consistent, which is what kissat's warmup buys it. Removed, not kept dead (§5.5)
- [x] **W4** gates: same-window Tier B (3 arms, windows `20260726-0144*`) **8.97** vs 9.88 pre-Walk (−9.2 %), 14/14, proofs verified; same-window
  Tier C (`20260726-0042*`) **386.54 vs 387.83 — FLAT**, 28/30 both — flat because Walk both wins and loses big (SCPC 1200 s TO → 259.9 s,
  Ptn-7824 14.8×, jgiraldezlevy 11.8×, frb45 10.5×, DLTM 2.8× vs **mp1-Nb5T15 3.66 s → TIMEOUT**, alone costing the ~80 PAR-2 SCPC gains).
  **Banked negatives, do not retry blind:** (a) budget anchored on the last inprocessing phase (kissat's own reading) — 9.69 vs 8.97, all of it
  rbsat; (b) accept-a-walk-only-if-it-beats-all-previous — crypto1 1.14 → 16.45 s, Ptn 3.80 → 162.5 s, worse than BOTH arms
- [x] **W4a** `--walk-stress-policy` (`43510fc`): Walk at random decision levels, off propagation fixpoints, tiny→huge budgets, mode flips
  mid-solve; per-step `kernel_check_invariants`
- [x] **W4b** DIAGNOSED (`4eaea4c`; probes 1563690–98, design §6 table): mp1-Nb5T15 poisoning travels through the **target** channel, not the
  dose (a 50× smaller dose changes nothing; suppressing the target copy turns TIMEOUT into a 240 s solve), and **kissat has the same problem
  there** (72.5 s with walk vs 54.0 without). Three knob-gated arms, each a big win on one class and a 3–20× loss on the other:
  `TACTIX_WALK_NO_TARGET`, `TACTIX_WALK_MAX_STEPS`, `TACTIX_WALK_WARMUP` (the last: SCPC 248 → 102 s, beating kissat's 166, but Tier B 8.99 →
  12.30). Non-issue: learned binaries in the walk objective
- [x] **W4c** REFERENCE-MATCHING EXHAUSTED (design §6 ledger): six attempts to match kissat/cadical more closely, all *scheduling* (the flipper
  was already a transliteration) — (1) rotation, (2) effort anchor, (3) warmup, (4) `walkredundant=false`, (5) cadical's both-modes rephase, (6)
  cadical's `phases.prev`; five of six dead or context-dependent, and **(1)/(2)** failed for the one architectural reason in
  `implementation-notes.md` §8. **The residual is a scheduling problem, not a mechanism gap** — five arms sit behind knobs for §5.6
- [x] **W4d** SUPERSEDED by W4f (`5ba6cda`): the flipper audit showed `mp1-Nb5T15` is UNDER-dosed, not over-perturbed (13× the dose turns its
  timeout into 26.1 s), so W4d's "stop scheduling walks when they stop paying" intervention is **DEAD** — and would have been a novel mechanism
  neither reference has. Trail: commit + design §6
- [x] **W4f · walk DOSE gated** by a same-window 9-arm × 30-instance sweep, report `benchmarks/reports/2026-07-27-tier-c-push.md` §1 ("the one
  open Track W obligation") + §8.2 (the re-sweep below), after the single-instance sweep at probes 1563941–49; root defect was a unit confusion in the BUDGET (we and kissat
  scale a per-mille of cache-line search ticks but SPEND in per-entry walk steps; cadical spends in ticks, 1 ≈ 16 entries ⇒ ~13× more scanning —
  design §6). Shipped 50 → **640** (`0c52fdd`; Tier C 392.98 → 322.80, mp1-Nb5T15 TO → 26.1 s) → **560** (`61138b8`; `WALK_PERMILLE`,
  `default_policy.hpp:300`; knob `TACTIX_WALK_PERMILLE`). Lesson (L12 again): the sharp Tier C preference for 640 was measured at `bumpreasons`
  rate 10 and was an ARTEFACT of that wrong rate — re-swept at the shipped rate, Tier C over per-mille 400..800 is 308.8 / 279.8 / 300.1 / 314.5
  / 307.3 = no trend, while Tier B has a clean step (mod4block ~65 s at ≤500, 9.1 s at ≥560), so 560 is the low end of the Tier B plateau with
  that plateau's better Tier C
- [ ] **W4e · PHASE-6 hand-off** (deferred by design, not near-term work): the five knob-gated arms + `stats.walk_minimum` as reward proxy become
  the second dosing mechanism family in the §5.6 exhibit, alongside elimination dosing (rev 68). Nothing to build in phase 3

### Track R — the full reference rule inventory (plan rev 72, user 2026-07-27)

**Numbering:** within this section `R<n>` always means **Track R · R<n>** (the rules below). Three other
`R<n>` namespaces exist — see the key under `## Log` before reading a bare `R<n>` anywhere else.

**User directive:** implement/enable every proof rule Kissat and CaDiCaL have, and measure with proof generation on BOTH sides. Order is
measured-value-per-cost, cheapest first; each lands under the §5.3 contract (one directory + one registry line) with its own stress-policy + fuzz
legs on both proof formats and a same-window gate. Counter-evidence kept so the sequencing stays honest (not a reason to skip the work — a reason
to run the search-quality tracks in parallel): **L11**.

**LANDING CONVENTION (user, 2026-07-28): a rule lands ON.** The earlier "ships OFF until its own A/B says it pays" convention is DEAD — it
contradicted [[no-rule-off-configs]] and judged rules ONE AT A TIME, the one measurement that cannot see this family's dominant effect (**L15**).
Report `benchmarks/reports/2026-07-28-all-rules-on.md`.

**MEASURED CONSTRAINT on the joint-admission lever — narrower than it looks.** mod4block is NON-MONOTONE in how consistently the trio runs: never
9.26, always 13.17, **once-then-stop 263.87** — doing the rewrite then withdrawing the maintenance is far worse than either extreme, so the trio
must run all-solve or not at all. That kills every mid-solve decision (you cannot measure the trio's productivity without running it, and having
run it you cannot stop), leaving only a STATIC per-instance decision, which L5 measured at chance. Phase-6 context feature, not an engineering
lever, absent new evidence.

- [x] **R1 · X2 `HyperTernaryRes`** (cadical `ternary.cpp`) — add-RUP only, no witness. Landed `d0bb6ec`; **ON since `3fb85bd`**
- [x] **R2 · X1 `BCE`** (cadical `block.cpp`) — delete + push-witness, reusing E1's occurrence index and reconstruction stack (kissat 4 dropped
  it). Landed `d0bb6ec`/`1a9afb1`; two defects found by fuzz and fixed (a wrong-SAT from filtering the blockedness test's opposite side; a
  duplicated-binary crash). **ON since `3fb85bd`**
- [x] **R3 · X4 `Instantiate`** (cadical `instantiate.cpp`) — add/delete; the strengthened clause is RUP against `F` alone because `¬(C∖{l})`
  leaves `C` unit and propagates `l` itself. Landed `2085429`; 9 unit tests, fuzz 10k/0 both formats. Fires but yields ~nothing alone (FmlaEquiv
  57,357 probes → 5 literals); **ON since `3fb85bd`** — L15: measured alone is the wrong experiment for this family
- [x] **R4 · S7 `Reorder`** (kissat `reorder.c`) — `H`-only **policy op**, no proof obligation, no calculus entry. Landed `4482bb5`; 8 unit
  tests, fuzz 10k/0 both formats, deviations in `implementation-notes.md` §10. **Ships OFF** (`TACTIX_REORDER`, kissat's 1/2 values) — the one
  Track R result that is not an inventory tick: **parity on Tier B's UNSAT half (10.16 → 10.11) and 3.4× worse on its SAT half (5.28 → 16.99)**,
  a SAT-vs-UNSAT context axis no other §5.6 family carries. **Scope: policy op, NOT a proof rule — the all-rules-ON directive does not cover it;
  ask before flipping**
- [ ] **R5 · V5 `Sweep` + the embedded sub-solver** (kissat `sweep.c`+`kitten.c`; §11 O11 now due) — the largest single item in kissat's own
  ablation (+14 alone) and the biggest build; also unlocks kitten-based **definition extraction** for E1 (the last E1 gap, +6.1 to kissat).
  **DESIGN PASS DONE** — `docs/design/v5-sweep-design.md`: O11 says WRITE a minimal sub-solver, not vendor kitten (verbatim copying is barred
  anyway, and we need LRAT-grade antecedents kitten's core does not give); the real risk is the proof obligation — a swept equivalence is
  entailed but NOT RUP, so the chain must come from the sub-solver's core (strategy b) with V6's replay trick as a fast path (strategy a). Six
  landable increments S-a…S-f; **S-a/S-b done (below), next is S-c** — candidate selection + environment construction, whose definition of done
  MUST include sub-solver fuzz legs on both proof formats (review finding **RV-R1**) — then S-d…S-f
  - [x] **S-a · the embedded sub-solver** (`src/rules/sweep/subsolver.{hpp,cpp}`, `8ac2199`): minimal CDCL (two-watch propagation, 1-UIP,
    restarts, MiniSat-shaped assumptions, tick budget, fixed decision order), owns its arena/watches/trail, never touches `Kernel`,
    deterministic. Verified by exhaustion — **1 M random formulas vs brute force**, half under assumptions. Safety property: an unanalyzable
    conflict returns `Budget`, never `Unsat`
  - [x] **S-b · antecedent recording + core extraction** (`9db22df`): `derivation()` returns ordered lemmas with LRAT chains over environment ids
    plus the core, in the kernel's chain order (ascending trail position, conflict last); binaries moved into the arena in the same change.
    Verified three independent ways — LRAT replay, entailment by enumeration, core discipline — over **207 k refutations**, plus 40 micro cases
    through the REAL drat-trim and lrat-check (`tests/sweep_proofs.sh`). Lesson: the first version of BOTH oracle and emitter produced only
    ONE-STEP derivations, so the lemma chains went untested while green; both now assert a multi-step floor
- [ ] **R6 · X3 `CCE`** (cadical `cover.cpp`) — delete + push-witness; doubles as the §5.3 **extensibility acceptance test** (pure plugin, no
  kernel edit)
- [ ] **R7 · X5 `Condition`** (cadical `condition.cpp`) — conditional autarkies
- [ ] **R8 · the dosing/H-only items kissat ablates**: eager subsumption of learned clauses (+5.9) and random decisions (+7.9) — scheduling arms
  on existing machinery, not new rule families

### Other phase-3 items

- [x] Effort-budgeted scheduler increment 1 + prerequisites (revs 53–55; design `docs/design/effort-scheduler-design.md`)
- [ ] Deferred quality items (2026-07-15 §5.5 sweep): SubsumeForward occs/marks alloc; `binary_ids` `.emplace` on the CDCL hot path — escalate
  via §5.4 only with a profile. (The third, the `literal_redundant` depth cap 900 vs 1000, is CLOSED measured-dead — `88cff2d`, probe 1570502.)
- **Exit:** T3

## Phase 3.5 — engineering ingenuity on top of kissat (PIN, rev 67)

Deliberately unelaborated (§9 row 3.5 + §11 O15): once kissat performance is matched, a dedicated pass for novel engineering beyond the reference
lineage. Concretize at the T3 gate via revise-plan.

## Phase 4 — tuning, bindings, extensibility proof

- [ ] Catalog rules remaining: **S7 Reorder, V5 Sweep** (kitten, O11), **X1 BCE** (S6 Walk moved to phase 3, rev 71) — *reconcile at phase entry:
  Reorder and BCE landed in phase 3 Track R, and Sweep is in progress there (Track R · R5)*; **decide O16b at phase entry** (PR/SR clause learning as a
  catalog candidate — the checkable form of auto-invented rules; needs dpr-trim or PR→DRAT in the N2 story)
- [ ] Independent-lineage candidates: CMS best_inv rephase (with Walk), rotation presets (O6)
- [ ] Python bindings; policy presets (MAB scheduler, classification) — §5.6 seed
- [ ] Demo policies 4–6 incl. external branching (N1 end-to-end)
- [ ] §8.4 overhead budget: default vs hardcoded twin ≤ 5% on Tier B
- [ ] **Extensibility acceptance test: CCE (X3) as pure plugin (§5.3)**
- **Exit:** T4 (⇒ N3 satisfied)

## Phase 5 — competition readiness

- [ ] Backlog rules as warranted (X2, X4, X5) — *X2/X4 landed in phase 3 Track R; X5 Condition is Track R · R7*
- [ ] Docker + run scripts; solver description; license/attribution audit
- [ ] Full toolchain rehearsal on Tier B (solver → proof → verified checker, 16 GB cap)
- **Exit:** T5 or best effort; submission-ready

## Phase 6 — learned scheduling: the RL module (§5.6, rev 26)

The end-game (user directive 2026-07-15): learn on-the-fly which proof rule / batch to apply — order, frequency, budgets. Bandit-first, no deep
learning on the competition path; DefaultPolicy stays the N3 baseline and a permanent arm.

- [ ] Offline replay/counterfactual harness: exact `--trace` replays + §8.4 config-file ablations; validate reward proxies BEFORE any online
  learning is trusted (§8.5)
- [ ] L1: windowed/discounted-UCB bandit over the macro scheduler's slot choice points (DefaultPolicy schedule = permanent arm)
- [ ] LearnedPolicy joins the random-policy fuzz rotation + determinism trace tests
- [ ] Gate L-safe measured: within +2% PAR-2 of DefaultPolicy on Tier C medians
- [ ] L2: contextual bandit, tabular contexts (features per O12; **rev 68: must include the density-gate family — binary density, dose
  productivity, elimination yield — per §5.6's elimination-dosing exhibit**)
- [ ] **Elimination-dosing exhibit obligations (rev 68, §5.6):** per-entry dose on/off joins the scheduled choice points; DefaultPolicy's gate
  thresholds = permanent arm; success = gates matched where right, em_11 recovered where wrong, no hand threshold load-bearing
- [ ] L3 experiment: composed-batch sequencing (tabular RL) under its recorded kill criterion; neighbor-move arm proposals (TACO shape, in-run)
- [ ] Anytime check (§8.5, rev 28): interrupt LearnedPolicy anywhere — no segment re-solves or sits in a non-solving state
- [ ] Offline per-family preset layer (classify → presets as arms)
- [ ] Gate L-win measured on Tier D + per-family attribution report (≥ 2 family wins); O13 distillation decision
- **Exit:** §8.5 gates (L-safe, then L-win); stretch — improves the T5 configuration

## Log

One line per entry: headline + result + pointer (commit/report/probe/job), unless still active context; rationale lives in commit messages and `benchmarks/reports/`. Plan
revisions: DEVELOPMENT_PLAN.md §13. Newest first.

**`R<n>` IS FOUR DIFFERENT NAMESPACES — always read the prefix.** The numbers are cited in frozen reports, so
they are **never renumbered**; they are disambiguated by prefix instead (2026-08-01). The frozen reports and
design docs themselves still say bare `R<n>` (e.g. `docs/design/e2-factorsize-design.md` "§5a′/R5" = TD-R5)
— decode them with this key, which is the only home for it:

| prefix | namespace | owner |
|---|---|---|
| `plan R1–R5` | project REQUIREMENTS — R1 proof certificates · R2 competition I/O · R3 proof-rule API · R4 automatic policy · **R5 exceed Kissat** | DEVELOPMENT_PLAN.md §1 |
| `Track R · R1–R8` | the reference RULE INVENTORY — **R5 = V5 `Sweep`**, R1 = X2, R2 = X1 BCE | PROGRESS.md "Track R" |
| `TD-R1–R5` | the Tier B TIMEOUT-DIAGNOSIS leads — **R5 = Factor scope (`factor_size`)**, R1 = V4 meter, R2 = BVE density gate | `benchmarks/reports/2026-07-30-tierB-timeouts-diagnosis.md` |
| `RV-R1–R5` | the codebase-REVIEW routed items — **R1 = Sweep sub-solver fuzz legs**, R2 = Reorder stress policy | `benchmarks/reports/2026-07-29-review-post-track-r.md` |

The two that bite hardest: **`R5` alone is ambiguous four ways**, and the 2026-07-30 log entries below use the
`TD-` namespace, NOT Track R's.

### 2026-08-03 — ▶▶ CORRECTION #2: V6's gate matching is AT PARITY with kissat. The "3.3–3.9× fewer congruences" was ANOTHER counter-scope artifact
Probes 1580063/1580064 — kissat's **per-kind** `congruent_*` on the same two instances (`kissat-metrics`
build). This is the comparison that should have preceded both of today's congruence entries, and it retires
the lever they ranked #1.

| T105, by kind | kissat | ours | ratio |
|---|---|---|---|
| AND congruences | 4,348 | **3,423** | **1.27×** |
| XOR congruences | **531** | **268** | 1.98× |
| ITE congruences | 18 | 0 | — |
| `congruent_equivalences` | **3,513 (24 %)** | — (we have no such channel in V6) | — |
| `congruent_units` | **2,231** | **14** | **159×** |
| **`congruent` total** | 14,589 | (3,713 merges) | 3.9× |

- **▶▶ OUR GATE MATCHING IS FINE.** AND **1.27×**, XOR **1.98×** — that is parity, not a 3.9× defect. The
  aggregate gap was never gate matching.
- **`congruent` is a BUNDLE, and we were comparing it to one component.** kissat's total folds in
  `congruent_equivalences` — a **binary-clause equivalent-literal scan** run inside the closure
  (`congruence.c:4118-4137`: for a marked literal, find `other` with both polarities present and
  `merge_literals`) — worth **24 % of its congruences**. **We implement that mechanism, in V3 Substitute's
  SCC over the binary implication graph, as a separate rule.** So the 3.9× compares our V6 against kissat's
  V6+V3-equivalent. Same error shape as the 182×, one level up: **the first comparison used the wrong
  counter, the second used the wrong SCOPE.**
- **The one real outcome gap left is `congruent_units`: 2,231 vs our 14 (159×)** — C3's unit channel. That
  is now the only per-kind number where we are not close, and it is the actionable lead.
- **▶ THE XOR "0.003 % COLLISION RATE" IS NORMAL, NOT A DEFECT** — the entry below reads it as a
  canonicalization fault. **Retracted:** kissat gets only **531** XOR congruences on this instance from a
  comparable XOR volume (`congruent_arity_xors` 22.8 M). Millions of XOR gates yielding hundreds of merges is
  what BOTH solvers do. There is no canonicalization bug to fix, and the C0 sign-absorption suspect is
  cleared — I read `canonicalize` (`congruence.cpp:552-578`) and it absorbs sign into `parity` correctly, and
  kissat's own extraction (`congruence.c:2939`) does the **identical** one-gate-per-output rotation we do.
- **WHAT SURVIVES:** V6's **cost**. 7.6× kissat's congruence time (172.9 s vs 22.75 s) for a comparable
  outcome is still real and still 32.9 %/38.2 % of the clock on these two instances. The gap is TIME, not
  merges. One concrete cost lead from the source read: kissat checks the table at CONSTRUCTION
  (`new_xor_gate:1206` — on a hit it merges and returns 0 **without materializing the gate**), where we
  materialize every rotation into a `std::vector<CongGate>` and table it in a second pass.
- **METHOD — this is the fourth mechanism read overturned in 24 h** (TransitiveReduce's "1000×", extraction
  volume, the 182×, and now this). All four failed the same way: **a reference counter was compared without
  first establishing what it counts and what it bundles.** L14 said "get the reference's number on the SAME
  instance"; that is necessary and was not sufficient. **Sharpened rule: before comparing to a reference
  counter, read its DECLARATION and its INCREMENT SITES, and confirm our analogue covers the same scope —
  cite `statistics.h` line + every `INC` site in the writeup.**

### 2026-08-03 — ▶▶ (SUPERSEDED IN PART — see the correction above) V6's DISCRIMINATOR RESOLVES: XOR gates collide **0.003 % of the time**
Probes 1579948 (`T105.2.0`) / 1579949 (`T87.2.0`), the closure funnel added in `014e3b2`
(`TACTIX_CONG_PROBE`). The resume block posed a two-branch discriminator; **it lands unambiguously on
branch 2, and both instances give the same shape.**

| T105 | canon_ok | insert | **collide** | stale | already_eq | **chain_fail** | merged |
|---|---|---|---|---|---|---|---|
| and | 26,397,263 | 26,350,881 | 46,382 | 0 | 42,959 | **0** | 3,423 |
| xor | 26,976,595 | 26,975,777 | **818** | 0 | 550 | **0** | 268 |
| ite | 60 | 30 | 30 | 0 | 30 | **0** | **0** |

| T87 | canon_ok | insert | **collide** | stale | already_eq | **chain_fail** | merged |
|---|---|---|---|---|---|---|---|
| and | 38,994,960 | 38,948,080 | 46,880 | 0 | 43,225 | **0** | 3,655 |
| xor | 39,660,474 | 39,659,561 | **913** | 0 | 612 | **0** | **301** |
| ite | 76 | 38 | 38 | 0 | 38 | **0** | **0** |

- **▶▶ THE FINDING: we emit ~27 M / ~39.7 M XOR gates and they collide 818 / 913 times — 0.0030 % and
  0.0023 %.** Structurally identical XOR gates essentially NEVER meet on their key. **The fault is in the
  KEY / CANONICALIZATION**, and C0's sign absorption (`base_parity`/`r`) is now the only standing suspect.
- **`chain_fail = 0` ON EVERY KIND, BOTH INSTANCES — `add_equivalence` and the proof chain are EXONERATED.**
  That was the other branch of the discriminator and it is dead: not one inference is being dropped for
  want of a checkable chain. (Consistent with `congruence_unjustified` reading 0 on both runs.)
- **`stale = 0` everywhere** — the evict-on-stale-key policy is eating nothing; not a lever.
- **AND collides ~58× more often per gate than XOR** (0.176 % vs 0.0030 % on T105) — so whatever the key
  defect is, it is far worse for XOR, which is also 50.5 % of our emission volume. And **93 % of AND
  collisions are `already_eq`** (42,959 of 46,382): even when AND keys DO meet, the outputs are usually
  already in one class, i.e. that work was genuinely redundant.
- **ITE draws ZERO merges** from 2,076 / 2,292 gates that reach canonicalize (2,016 / 2,216 of them
  `canon_skip`). It is a rounding error, not a lever — consistent with kissat matching only 131 of its own
  247,798 ITEs. **Do not spend on the ITE extractor.**
- **A structural hint worth carrying into the fix:** the absolute collision counts barely move between the
  two instances (AND 46,382 vs 46,880; XOR 818 vs 913) although T87 emits **1.5× more gates**. Collisions do
  not scale with emission volume, which is what you would expect if they come from a small structural core
  and the rest of the emission is keyed apart by something that should not be in the key.
- **NEXT (a real fix now, not a probe):** read C0's key construction against kissat's, specifically where the
  XOR output's sign/parity is absorbed. The prediction to pre-register: if sign lands in the KEY rather than
  in `out_lit`, then two gates over the same variable set with opposite output parity are keyed apart, and
  the fix collapses `insert` into `collide` by orders of magnitude. Measure the funnel before/after — it is
  now the instrument for exactly this.

### 2026-08-03 — ▶ MY ERROR: I rebuilt the binary two in-flight arrays were executing; both runs cancelled and redone
Recorded because it is the [[s4-gate-negatives]] build-race hazard and I walked straight into it **while
holding the memory that names it**. Submitted Tier B (1579587) and Tier C (1579688) against
`build/pin-t3a/tactix` at 21:03, then at 21:08 rebuilt **that same path** with the congruence
instrumentation. Tier B was 107/112 done; **Tier C was 31/150**, so ~119 Tier C tasks would have run a
different binary from the first 31 — arms and instances mixed across two builds, in a window whose whole
purpose is an arm-vs-arm comparison.
- **Cancelled all four job ids** (1579587/1579588/1579688/1579689 — by explicit id, never account-wide) and
  **deleted both run dirs** rather than salvaging. A partially-contaminated window is worth less than no
  window: it produces plausible numbers, which is exactly how [[ab-arm-identical-artefacts]] and the
  `$TMPDIR` collision both did their damage.
- **What made it recoverable:** the watchers fired on cancellation, because the skill's condition is
  "job gone from the queue", not "results file exists". A file-exists watcher would have hung forever.
- **The rule, restated so the next session does not need to rediscover it: a submitted array OWNS its
  binary path until it finishes.** Build the measurement binary into a fresh `build/pin-*` dir, submit, and
  do not touch that dir again — put the NEXT change in the NEXT dir. Resubmitted against `build/pin-t3b`
  (arrays 1579784 B / 1579883 C) and staged the probe binary as a separate copy under the session job dir so
  no later rebuild can reach it either.

### 2026-08-03 — ▶ T3's INSTRUMENT restored: the rule-neutral subset is a real list, and Tier C carries a kissat arm again
T3 has two halves and one of them had been **unmeasurable** for a week — the rule-neutral subset existed only
as a number in a frozen report, and Tier C stopped carrying a kissat arm after 07-30. Both closed.
- **`benchmarks/tiers/C-ruleneutral.list`** — 24 of 30, derived (not re-measured) from the ablation window
  `20260727-002213-C-kabl`: exclude an instance iff ablating Sweep moves EITHER baseline >25 %. Reproduces
  the documented 24/30 exactly, so it stays comparable with the 1.37 × the half was last measured at.
- **The list ships with its own defect recorded in the header**, because the derivation deserves less trust
  than its reuse implies. The ablation is SINGLE-SHOT, so a ratio cannot separate "sweep matters here" from
  "this instance is chaotic" — and **4 of the 6 exclusions (ContextModel, j3037, jgiraldezlevy, urq45) are on
  the measured perturbation-FRAGILE list**, with sweep ratios (1.54–2.54) sitting INSIDE their 8–23 × bands
  (probe 1579386, where the perturbations were walk/bump/chrono — none of them sweep). Only `em_11` (both
  baselines move, 2.65×/2.30×) and `DLTM` (kissat 3.57×) survive that objection. **Closer: re-run the sweep
  ablation with ≥3 repeats and require the sweep delta to exceed the instance's own replicate band.**
- **Consequence, stated up front so it is not banked by accident:** the subset excludes precisely the
  instances where R-d costs Tier C, so **"R-d helps on the subset" may be an artefact of the subset's defect
  rather than a finding.** The arithmetic is now reproducible on demand — `subset_score.py` on window
  `20260802-112853` gives **retire 1.005 × cadical on the subset against 1.129 × on the full tier, and base
  1.081 × subset against 1.039 × full**, i.e. the two views RANK THE ARMS OPPOSITELY. That is a reason to fix
  the subset, not a reason to celebrate.
- **A max-over-five-lacked-techniques criterion was tested and REJECTED**: it excludes 16 of 30 and is plainly
  noise-amplifying — `mp1-Nb5T15` shows **10.18× from `--eagersubsume=0`**, a technique worth **+5.94 PAR-2
  across the whole tier**. Taking a max over five single-shot ablations is L8 inflation five times over.
- **`tools/bench/subset_score.py`** scores any existing Tier C run over both views from one `results.json` —
  no re-running, so the subset number always comes from the same window as the full-tier number.
- The kissat arm needed **no code change**: `run.py --baseline` is already repeatable and was simply not
  being passed. Standing rule: **every Tier C run carries `--baseline kissat --baseline cadical`.**

### 2026-08-03 — ▶▶ DOC AUDIT of the Tier B/C gap claims: **the "182×" congruence headline is a counter-semantics error**, plus 9 further doc-vs-data defects
Read-only audit of every headline number in the resume block, the tier tables and the gate section against
the raw `benchmarks/runs/*/results.json` and the kissat sources. **All four headline PAR-2 claims RECOMPUTE
EXACTLY** (Tier B 75.41471 / kissat 10.04932 / 7.504×; Tier C 358.1191 / OFF 329.6273 / cadical 317.3166 /
1.129× / 1.039×), arms are genuinely distinct runs, no `mismatch:true`, no tactix `proof_ok:false`. **The
standing is real; several of the framings around it were not.**
- **▶▶ D2, the one that matters: "we match 3,713 where kissat matches 674,830" (182×) compares two different
  counters.** kissat's `congruent_matched` is incremented inside `find_gate_lits` on every **hash-table
  find-hit** (`kissat/src/congruence.c:510`), during gate CONSTRUCTION — an internal lookup statistic, not an
  outcome. The like-for-like analogue of our `merges` is `congruent`
  (`congruence.c:948`, `COUNTER (congruent, 1, PCNT_VARIABLES, …)` at `statistics.h:54`) = **14,461 (T105) /
  13,227 (T87)**. **Real outcome gap: 3.9× and 3.3×, not 182×** — and the 08-03 entry below already had the
  right numbers before the correction entry replaced them with the wrong one. **The lever survives** (7.6× the
  time for ~3.9× fewer congruences is still the largest single Tier C item) **but was sized off a figure
  ~47× too large.** Same failure mode as L14, one level deeper: I got the reference's counter on the same
  instance, and then compared it to the wrong one of ours. **Rule going in: when quoting a reference counter,
  quote its DECLARATION (`statistics.h` line + unit) alongside the value.**
- **D1 · the tier tables' git SHA was wrong.** Both artefacts record **`31593d5`**; the tables said
  `a927131`. The binary was built from an **uncommitted tree** (R-d lands in `4f88e10`, 2 h after the run;
  `a927131` ships it 6 h after). The measured `retire` arm IS the shipped default — the intervening src diff
  is only the default flip — but **no recorded SHA reproduces the window**, which is exactly what the tables'
  own maintenance contract exists to prevent. Contract amended: take the SHA from the run's `manifest.json`,
  never from the commit that later shipped the change.
- **D8 · reproduction trap:** the OFF arm is documented as `TACTIX_RETIRE=0`, but at `31593d5`
  `main.cpp:287` was a **presence** check — `TACTIX_RETIRE=0` on that binary turns retirement **ON**. The
  arm actually ran with the variable unset. Value parsing exists only from `a927131`.
- **D13 · the gate section was stale in the one place a reader consults for gate status** — it still ended on
  "▶▶ THE CaDiCaL HALF IS MET 2026-07-30" while the 1.129× miss was recorded correctly 200 lines earlier. T3
  now leads with a current-status table.
- **Framing corrections banked into the tier tables.** (i) **Tier C's gap is a TAIL, not a broad loss**:
  median instance **0.830× cadical**, faster on 15 of 29; excluding the three fragile instances the shipped
  arm is **0.971×** and the OFF arm *inverts* to **1.077×**. (ii) **cadical is not the hard Tier C baseline —
  kissat is**: 229.64 vs our 358.12 ≈ **1.56×**, and no Tier C window has carried a kissat arm since 07-30.
  (iii) The T3 miss (**+9.07 PAR-2**) is **smaller than the noise floor** (10.8); the OFF arm's pass is not.
  (iv) `veer_axi` is a **cliff solve** — the OFF base solved it in 286.7 s against a 300 s cap in the adjacent
  window, so "+2 solves" is partly a cliff effect (the within-window A/B still stands).
- **D6 · the noise floor itself was measured under contention** — `20260731-013523` (01:35–03:28) overlapped
  the full Tier C array `20260731-021615` (02:16–04:04) by ~1 h 12 m. 10.8 PAR-2 is an upper bound on
  quiet-partition noise, presented until now without caveat.
- **D7 · pre-2026-07-28 Tier C aggregates are protocol-incomparable:** an instance whose DRAT check blew the
  2400 s checker budget was scored UNSOLVED **even when tactix solved it** — `20260727-082011` scored `urq45`
  unsolved at a **354.5 s solve** with a 2400.1 s check. Inflation up to ~+68 PAR-2.
- **D3/D4/D9/D11 · four factual slips in the tables**, all verified against raw JSON and fixed:
  `20260729-094225` is **29/30** not 28/30; `20260730-085509`'s 319.19 is the **`x2rate8`** arm (the arm named
  `shipped` there is 397.94 at 28/30); `128_125` is an **OOM at 24.5 s against a 1200 s cap**, not a `TO`
  (and it flatters both ratios — dropping it moves shipped 1.129 → 1.172×); R-d's three losers own **96.5 %**
  of the gross regression, not 100 % (seven others add +63.5 s), against an exact net of **+854.75 s**.
- **Two gaps in the work list, now filed** (Open items + the resume block's ordered list): **C2 Strengthen has
  no budget item** despite owning **89.7 % of `DivS_862_11`** in four applications, and **broad Factor
  admission was never re-measured under plan rev 78** — it was held at 1.129× Tier C on 08-01, and rev 78
  shipped R-d at exactly 1.129× on 08-02. Also: **rev 78 requires a banked trade to carry a named closer, and
  R-d's closer (`D-RETIRE-TIERC`) was discharged as not-actionable**, so the 1.129× cost currently has **no
  open action item at all** — defensible on the evidence (an admission gate would fit noise), but the
  safeguard produced zero follow-up work and that should be a conscious choice, not an accident.
- **Not re-verified, left as-is:** the frag3 replicate bound (`≤0.8 %` → the audit re-read it as ≤1.0 %;
  argument unaffected) and the `7.94 / 9.27` core14 figures (cross-window aggregates, now annotated).

### 2026-08-03 — ▶▶ CORRECTION: congruence's gap is **MATCHING, not extraction** — and ITE extraction is ~dead
Probe 1579468 — kissat's `congruent_gates_*` on T105, the comparison I should have had before reading
anything into our gate count. **It refutes the extraction-volume conclusion I banked an hour earlier.**

| | ours | kissat | |
|---|---|---|---|
| gates extracted | 53,371,397 (11 rounds) | **18,474,261** (7 closures) | 2.9× more, NOT an explosion |
| XOR share | 50.5 % | **58 %** | ours is LOWER — XOR-heavy is normal here |
| **ITE gates** | **2,068** | **247,798** | **~120× fewer for us** |
| ~~**matched**~~ | ~~3,713 merges~~ | ~~674,830~~ | ~~**~182× fewer for us**~~ **← WRONG COUNTER, see below** |
| **congruent (corrected)** | **3,713 merges** | **14,461 `congruent`** | **3.9× fewer for us** |
| congruence time | 172,872 ms | 22,750 ms | 7.6× |

**▶▶ THE "182×" ROW IS RETRACTED (audit 2026-08-03, entry above).** `congruent_matched` counts hash-table
**find-hits inside `find_gate_lits`** (`kissat/src/congruence.c:510`), bumped during gate construction — not
an outcome. The analogue of our `merges` is `congruent` (`congruence.c:948`,
`COUNTER (congruent, 1, PCNT_VARIABLES, …)`) = **14,461 / 13,227**, i.e. **3.9× / 3.3×** — the figure the
entry BELOW already had right. Everything else in this entry stands; the conclusion (matching, not
extraction) stands; only the magnitude was inflated ~47×.

- **What was wrong:** I read "78 M gates, 7.2 per clause" as the anomaly. It is not — kissat extracts the
  same order of magnitude, with a HIGHER XOR proportion. Extraction volume is roughly comparable.
- **What is actually wrong:** we spend **7.6× the time to produce ~~182×~~ 3.9× fewer congruences**
  (3,713 merges vs kissat's 14,461 `congruent` variables — the 182× used `congruent_matched`, a gate-construction
  lookup counter; retracted above). **The defect is in the CLOSURE / KEY MATCHING, not in how much we
  extract.** Note the corrected magnitude weakens the sub-claim that structurally identical XOR gates never
  collide at all: a 3.9× shortfall is equally compatible with partial canonicalization loss, so the
  collisions-attempted-vs-merges-accepted instrumentation is still the right discriminator — just do not
  expect a 100×-scale finding.
- **Second, separate defect: our ITE extractor is ~dead** — 2,068 gates against kissat's 247,798 on the same
  formula, and kissat matches 131 ITEs from them. Ours is a rounding error (0.004 % of our gates vs kissat's
  1 %). C2's degenerate-ITE filter (Track C, "gates −23 %, equivalences identical") is the first suspect.
- **Method note, and the reason this correction exists:** the gate-count ratio looked damning in isolation
  and I nearly built an extractor fix on it. What stopped me was arithmetic that would not close (27 M XOR
  gates need ~86 M clause-participations from a 10.7 M-clause formula) — so I got the reference's counters
  on the SAME instance first (L14) instead of acting. **Two of today's three "obvious" mechanism reads have
  now been overturned by that same check** (TransitiveReduce's "~1000× gap", and this).
- **Next:** instrument the closure — count key collisions attempted vs merges accepted, split by kind. If
  XOR keys are colliding but merges are being rejected, the fault is in `add_equivalence`/the proof chain; if
  they are not colliding at all, it is the canonicalization (C0's sign-absorption is the standing suspect).

### 2026-08-03 — ▶▶ V6 Congruence: kissat finds **3.3× MORE congruences in 12.5× LESS time** — a real rev-77 gap
Probe 1579464, the L14 comparison on the two instances where V6 owns the clock (`T87.2.0`, `T105.2.0` —
both ~3.1 M vars / ~10.9 M clauses).

| | ours | kissat |
|---|---|---|
| **T87** congruence | **253,989 ms (32.9 % of solve)**, 16 rounds, **78,652,093 gates**, **3,979 merges** | **20.38 s (9.85 %)**, **13,227 congruent vars** |
| **T105** congruence | **172,872 ms (38.2 %)**, 11 rounds, **53,371,397 gates**, 3,713 merges | **22.75 s (9.61 %)**, **14,461 congruent vars** |

- **This is outcome (2) of the three I pre-registered: the same job done far more expensively AND with a
  worse result.** Not inherent to the instances (kissat is fast on them), not a scheduling gap (16 and 11
  rounds, not dozens).
- **~~The anomaly is EXTRACTION VOLUME~~ — REFUTED 2026-08-03 by kissat's own counters on the same instance
  (probe 1579468); see the correcting entry above. kissat extracts 18.47 M gates to our 53.37 M (2.9×, not an
  explosion) and its XOR share is 58 % against our 50.5 %, so XOR-heavy extraction is NORMAL here. The real
  gap is MATCHING.**
- **My pre-registered prediction was WRONG and usefully so.** I predicted ~45 rounds (repeated full
  re-extraction) from the veer cost model; it is 16. So the cost is NOT re-running extraction often — it is
  extracting far too much EACH time. That rules out the scheduling fix I would otherwise have built.
- **Under rev 77 this is a "match or exceed" gap, not a dosing question:** kissat is strictly better here on
  both axes at once. Neither solver meters congruence by effort (kissat's controls are structural —
  `congruenceandarity` 1e6, `congruencexorarity` 4, `congruencexorcounts` 2, plus `congruenceonce` /
  `preprocesscongruence`), so a budget is the wrong instrument; the extractor is the thing to fix.
- **▶ GATE-KIND SPLIT (probe 1579467, T105): `26,393,284 and · 26,976,045 xor · 2,068 ite`.** XOR alone is
  **50.5 %** of the volume; for scale kissat's own split (on veer) is ands 59 % / ites 30 % / **xors 11 %**.
- **The arity caps are NOT the cause — we already match or beat kissat**: `CONG_XOR_ARITY` **4** = kissat's
  `congruencexorarity` 4, and `CONG_ARITY_LIMIT` **64** is far STRICTER than kissat's `congruenceandarity`
  1e6. So the gap is in what the extractor *emits*, not what it admits.
- **Structural fact found while reading it:** XOR extraction dedups relations correctly (a `seen` set over
  sorted positive vars) but then emits **ONE GATE PER CHOICE OF OUTPUT** — `s` gates for an `s`-variable
  relation. That is needed for congruence matching, but it means the gate count is `s ×` the relation count
  by construction, and it is the first thing to compare against kissat's counter semantics: **if kissat
  counts RELATIONS where we count (relation × output), the 12.5× is partly a measurement artefact and the
  real gap is smaller than it looks.** Probe 1579468 gets kissat's `congruent_gates_*` on the SAME instance
  to settle that before any code changes.
- **Do not "fix" the extractor until that comparison lands** — the arithmetic already does not close (27 M
  XOR gates would need ~5.4 M relations × 16 clauses = 86 M clause-participations from a 10.7 M-clause
  formula), which means either the emission multiplicity or the counter semantics explains it, and those two
  route to completely different work.
- Also visible in the same runs: **V4 TransitiveReduce at 18,773,159 applications on T87** (118,524 ms,
  15.4 %) and 8,548,660 on T105 — a second, separate lead on the same instances.

### 2026-08-02 — ▶ the three Tier C "losers" are FRAGILE, not damaged — `D-RETIRE-TIERC` CLOSED, no gate built
L20 test (probe 1579386): per instance, 2 base replicates + retire + **three UNRELATED perturbations**
(walk dose, bump rate, chrono off). **The premise inverts.**

| instance | retired vars | base ×2 | retire | walk | bump | chrono | spread |
|---|---|---|---|---|---|---|---|
| j3037 | **251 / 18,913** | 332.69 / 333.90 | 994.17 | **44.74** | 438.14 | **42.62** | **23×** |
| jgiraldezlevy | **1 / 2,200** | 202.42 / 204.30 | 733.13 | 76.69 | 173.04 | 293.50 | **9.6×** |
| urq45 | **1 / 74** | 205.73 / 207.26 | 622.68 | 208.94 | **596.86** | 74.56 | **8.4×** |

- **Retirement is not what hurts them.** It retires **1, 1 and 251** variables there — one variable out of
  2,200 cannot cost 3.6× through removed decisions. The mechanism barely fires; what moves is the trajectory.
- **All three are the L20 shape**: ≤0.8 % between base replicates, **8–23× under unrelated perturbations**.
  `urq45`'s `bump` arm costs **2.9×**, indistinguishable from retire's **3.0×**. `j3037` runs **7.8× FASTER**
  under `chrono=0` and `walk=400` than at the default.
- **`D-RETIRE-TIERC` CLOSED as measured-not-actionable. No admission gate** — fitting one to these three
  would be fitting to noise, the L15/L20 trap that killed five knobs earlier. **R-d's ship decision stands
  and is strengthened**: its Tier C cost is chaos, not damage.
- **Standing consequence:** these three make small Tier C aggregate deltas unreliable. Quote per-instance
  replicate ranges when a verdict rides on them.

### 2026-08-02 — ▶ per-instance RUNTIME BREAKDOWN: search is ~90 % of the median solve, but TWO rules own specific instances
Probe 1579404, all 58 instances, no proof writing. Median inprocessing-slot share: **Tier B 10.9 %, Tier C
7.7 %** — search is ~90 % of a median solve. The tail is where the leads are:
- **▶ V6 Congruence owns the two slowest Tier C UNSATs: `T87.2.0` 274.0 s of 825.7 (58.2 % slot) and
  `T105.2.0` 188.2 s of 507.3 (58.5 %)** — 462 s across two instances. **V6 has arity caps but NO effort
  meter** (it is absent from `EFFORT_SPECS`; kissat has no congruence-effort option either, so this is not a
  rev-77 deficit — but it is the largest single runtime lead on Tier C). Same rule as the §6 SIGTERM defect.
- **▶ C2 Strengthen owns `circuit_32in32out` 146.2 s of 215.8 (80.8 %) and `DivS_862_11` 97.8 s of 109.0
  (89.7 % — the rule IS the solve)**. Strengthen has a 100‰ meter, so the meter is not binding there.
- **The four Tier B timeouts are PURE SEARCH** — slot share 1.7–11.7 % (`or_randxor` 1.7, `fclqcolor` 2.3,
  `sted5` 3.8, `veer_axi` 11.7). No inprocessing tuning will solve them.
- Contrast: `hwmcc17miters` 93.8 % slot, `SC25_Timetable` 72.3 %, `PRP_30_38` 70.7 % — instances where the
  slot legitimately IS the work.

### 2026-08-02 — ▶▶ VARIABLE RETIREMENT (R-a…R-d) SHIPPED: Tier B **109.60 → 75.41 PAR-2, 24 → 26 solves**
The whole arc, compressed — full detail in commits `4cfb984` → `d678001` → `4853df6` → `4f88e10` → `a927131`
and designs `variable-retirement-design{,-v2}.md`.
- **Defect:** 84.03 % of `veer_axi`'s 1.61 G decisions landed on variables V3 had rewritten out of every
  clause (they occur in NO clause, so each decision propagates nothing) — 80,124 decisions/conflict against
  kissat's 147. Nothing retired them; the kernel had no active/inactive notion at all.
- **Shipped mechanism (R-d):** `heuristics_deactivate_var` unlinks a retired variable from the VMTF queue and
  the score heap (kissat `flags.c` `deactivate_variable`), `kernel_unassign` keeps it out, and the SAT test
  counts ACTIVE variables. **N1-clean** — `decide_candidates` enumerates from `k.nvars`, never from `H`, so
  the API still offers every variable (plan §11 O17 / §1.1 scope note, rev 76). `TACTIX_RETIRE=0` reverts.
- **Result:** Tier B **75.41 (26/28)**, gaining `veer_axi` AND `sted5`, no solve lost. Tier C **358.12 =
  1.129× cadical, OUTSIDE T3's band** — a recorded rev-78 trade (#solved wins), ledger `D-RETIRE-TIERC`,
  since the cost is three measured-FRAGILE instances (entry above).
- **Lessons that cost the most to learn.** (1) **Three skip-based mechanisms failed before deactivation
  worked** — decision-time filtering of a scattered set in a recency-ordered queue is a scan by construction;
  kissat removes ONCE at retirement, we were filtering on EVERY decision (**this is the rev-77 exhibit**).
  (2) The 4th attempt failed for a *different* reason and only instrumentation found it: `kernel_unassign`
  silently re-added retired variables (heap re-push / search-hint capture), degenerating R-d **into
  demotion** — the tell was reproducing demote's decision count to **nine significant figures**
  ([[ab-arm-identical-artefacts]] in a new form: not "the knob did nothing" but "the knob did something
  else's job"). (3) Fuzz legs run before that fix were **VOID, not stale** — they exercised the degenerate
  build (L22). (4) Demotion, the crude approximation, was measured and **rejected**: Tier B −7.49 but Tier C
  **+28.39**, ~3× the noise floor.

### 2026-08-02 — plan rev 77 (user): **no crude approximations of an understood mechanism** — the phase-3 guideline
§5's "references are evidence, not templates" gains a **fourth obligation**. Once a reference's mechanism is
understood, implement what is **known to match or exceed** it — not a cheaper lookalike. An approximation
ships ONLY with (a) a stated reason and (b) a **named action item** closing the gap, both in the
engineering-deviation ledger (`docs/implementation-notes.md`), which is already the one home for
"deliberately weaker than the reference + removal path". **An approximation with no named closer is a silent
permanent deficit.**
- **Exhibit, measured yesterday:** V3's hazard check skipped the WHOLE ROUND on the comment *"Rare; the
  erratum's finer per-SCC skip is a future refinement (deferred slot)"* — no action item. It discarded **all
  179,110 equivalences V6 had just found, 5 rounds of 5, over 284 hazardous literals in 406,256 (0.07 %)**,
  and cost a Tier B solve until the per-SCC form landed (−8.45 PAR-2, `veer_axi` TO → 238.7 s).
- **Corollary in the plan text:** an approximation of a known mechanism is **a defect, not an arm** — it must
  not be parked in the §5.6 dosing exhibit.
- **▶ IMMEDIATE APPLICATION — the live demotion decision.** `TACTIX_SUBST_DEMOTE` **is** a crude
  approximation of kissat's `deactivate_variable` (it DEFERS the wasted decisions; kissat removes the
  variables from the queue and heap and stops counting them, so they are never decided at all). Under rev 77
  it may ship only as an interim measure, explicitly labelled, with **R-d** (`variable-retirement-design-v2.md`)
  as the named closer and a ledger entry. Otherwise the guideline says: build R-d instead.
- Measured basis for calling it crude, not merely different: demotion leaves **42 % of decisions still on
  substituted variables**, and kissat's own counters show it collapses the SAME work we do
  (`congruent_matched` **179,812** vs our ~179,110) while spending **435,946 decisions total against our
  1.61 G** — the gap is what happens after the collapse, not the collapse.

### 2026-08-02 — ▶▶ REFERENCE STUDY settles it: we filter at DECISION time, kissat DEACTIVATES once — R-c is measured dead
Design v2 `docs/design/variable-retirement-design-v2.md` (supersedes v1 §4's mechanism; v1's problem
statement stands). Probe 1578771, the full 2×2 on veer_axi:

| arm | veer_axi | conflicts |
|---|---|---|
| off | 210.34 s UNSAT | 20,217 |
| **demote** (rank last) | **137.83 s UNSAT** | 17,557 |
| retire (skip) | **600 TO** | 3,271 |
| retire + demote | **600 TO** | 11,590 |

- **R-c is DEAD in both cells** — with physical segregation AND with the O(1) tail guard. Three fix attempts,
  all banked so they are not retried: heap park/restore (no movement), segregation (throughput 9.6 k/s →
  266 k/s, still TO), O(1) `unassigned_active` guard (no movement). **Three failed mechanisms is the
  finding**: the shape is wrong, not the tuning.
- **▶ WHAT THE REFERENCES DO** (`kissat/src/flags.c` `deactivate_variable`): retirement is a ONE-TIME
  deactivation — `f->active=false`, `kissat_dequeue` out of the VMTF queue, `kissat_pop_heap` out of the
  score heap, and the caller does **`solver->unassigned--`**. cadical carries the same concept as
  first-class `Flags::status {ACTIVE, FIXED, ELIMINATED, SUBSTITUTED, PURE}` with `stats.active--`, lazier at
  the decision site — two independent lineages agreeing on the CONCEPT, differing on mechanism, so the
  concept is what we adopt (plan §5).
- **▶ WHAT WE DO DIFFERENTLY — three, and they compound.** (1) **We filter at DECISION time; kissat removes
  at RETIREMENT time** — we pay per decision forever, they pay once per variable; skipping a scattered subset
  of a recency-ordered linked list is inherently a scan, which is why segregating helped 28× and still lost.
  (2) **Our `unassigned` counts retired variables; kissat's does not** — the load-bearing one: because SAT is
  "all assigned" over ALL variables, retired ones must still be assigned, which FORCED skip-with-fallback,
  and the fallback is the quadratic tail. kissat never assigns them; ρ supplies their values. (3) **Our
  kernel has no active/inactive notion at all** — E1 BVE shares the defect, unmeasured.
- **N1 verified, not assumed:** `decide_candidates` (`decide.cpp:65`) builds the instance surface from
  `k.nvars` filtered on assignment, and never reads `H`'s queue or heap — so `H`-side removal does NOT narrow
  `Decide::instances()`. Exactly the rev-76 line: capability removed = violation, default heuristic declining
  = compliant.
- **The complication R-d must respect:** making the count active-only turns `decide_guard` false while
  retired variables remain unassigned — sound (they occur in no clause; ρ has their values) but it changes a
  **rule guard, i.e. the CALCULUS**. Route R-d2 through the calculus skill + `docs/calculus.md`; do not slip
  it in. `unassigned_active` (R-c2) already implements the required accounting and keeps its recount assert.
- **▶ STANDING RECOMMENDATION while R-d is unbuilt: DEMOTION is the pragmatic win on veer** (210.34 →
  **137.83 s**), already implemented and default OFF. It is a TRADE (earlier windows: FmlaEquiv +37 %, 9vliw
  ~+4 %), so it needs its own same-window tier pair — **the cheapest available progress on the veer gap, and
  it does not depend on R-d landing.**
- **Pre-registered for R-d, because v1's prediction was refuted:** recover veer to ≤ 137.83 s while keeping
  FmlaEquiv 25.80 s and 9vliw 12.81 s, `x9` bit-identical. **If R-d also fails to beat demotion, STOP** —
  ship demotion on a tier pair and record retirement as measured-dead. Do not attempt a fifth mechanism.

### 2026-08-02 — R-a…R-c implemented: R-a/R-b BIT-IDENTICAL, R-c is NOT viable yet (my prediction was wrong twice)
Increments R-a (`Kernel::var_status` + `kernel_var_retired`), R-b (V3 retires at the witness push), R-c
(DefaultPolicy declines retired variables, skip-with-fallback, `TACTIX_RETIRE`, **default OFF**). Design
`docs/design/variable-retirement-design.md`.
- **Implementation detail the design did not pin down:** `decide_guard` is `unassigned > 0` and SAT means
  EVERY variable assigned, so retired variables must stay assignable — R-c is **skip-with-fallback**, not
  skip-outright, or SAT instances become unfinishable.
- **R-a/R-b are inert as designed:** mod4block **418,796** conflicts on both binaries; `x9-08014`, where
  nothing is retired, is **bit-identical** under R-c too (409,967 decisions / 276,755 conflicts).
- **The mechanism works:** `substdec` **0.00 %** on every instance. **R-c helps** FmlaEquiv (33.98 → 25.49 s)
  and 9vliw (14.51 → 12.76 s) in-window.
- **▶ BUT veer_axi REGRESSES 204 → 600 s TIMEOUT, and the pre-registered prediction (design §8) is REFUTED.**
  The design predicted retirement would beat demotion on veer AND avoid the FmlaEquiv/9vliw regressions. The
  second half held; the first failed outright.
- **The premise is NOT what failed.** The intended effect landed exactly: decisions 1.61 G → 5.79 M,
  decisions/conflict **80,124 → 1,777**. What collapsed was THROUGHPUT.
- **Two wrong hypotheses, both banked so they are not retried.** (1) "The heap park/restore is the cost" —
  sank retired scores once at retirement; **moved nothing** (3,257 → 3,258 conflicts). (2) "The queue scan
  length is the cost" — the physical reorder (`+demote`) raised per-decision throughput **9.6 k/s → 266 k/s**
  and it **still times out**.
- **The actual cost, isolated by normalising to REAL-variable decisions:** off does 257 M real decisions in
  204 s; retire+demote does 160 M in 600 s — **~4.7× slower per real decision**. The scan runs in the **TAIL
  of every descent**: once all non-retired variables are assigned, the search hint sits inside the retired
  block and every remaining decision walks ~195 k links before falling back.
- **▶ NEXT INCREMENT (R-c2), precisely specified:** maintain an O(1) count of unassigned NON-RETIRED
  variables (decrement/increment beside `k.unassigned`); when it hits zero go straight to the fallback
  instead of scanning. That removes the only remaining O(retired) path. **Do not attempt another fix without
  it** — the two above were guesses and both cost a probe cycle.
- **Standing meanwhile: DEMOTION is the pragmatic win on veer** (204.43 → **129.60 s**, still default OFF and
  still a trade elsewhere), and it is the fallback if R-c2 does not land.
- **Correctness is not in question:** ctest 38/38; fuzz **4 × 10,000 cases, 0 failures** with `TACTIX_RETIRE=1`
  (DRAT + native LRAT × ± `--random-policy`, ASan+UBSan), no new bundles — model-checking is the load-bearing
  oracle here (a retired variable with no witness is a wrong-SAT, design §5 invariant 2).

### 2026-08-01 — plan rev 76 + the retirement DESIGN: N1 permits retiring rewritten-away variables, as ADVICE
Design `docs/design/variable-retirement-design.md` (DESIGN, not implemented). Plan rev 76 adds and decides
**§11 O17** and adds an N1 scope note to §1.1.
- **The N1 question, resolved:** N1 binds the **API's availability**, not DefaultPolicy's taste.
  `Decide::instances()` must keep enumerating every legal choice and any external/Random/learned policy must
  still be able to decide a retired variable — but the built-in policy may **decline**, through the advice
  channel `Kernel::bve_retry` already exemplifies. **Capability removed = violation; default heuristic
  declining = compliant.** This also corrects our own over-strong `no_ext_decisions` comment, which said
  "narrows the policy's action space, which N1 forbids" — read literally that would forbid retirement too.
- **Reference-grounded:** cadical carries `status ∈ {ACTIVE, FIXED, ELIMINATED, SUBSTITUTED, PURE}` with
  `substituted()` first-class; kissat has `active`/`eliminated`/`fixed` bits. We adopt the CONCEPT, not the
  mechanism — theirs is consulted directly by the decision loop, ours must route through advice so the
  instance surface stays intact.
- **Design shape:** `Kernel::var_status` set by the rule at the point it pushes the reconstruction witness
  (the two facts must not drift), read only by `H`'s two next-unassigned scans. No proof delta, no calculus
  entry — pure heuristic layer. Five increments R-a…R-e, E1 BVE's half measured separately since its
  magnitude is unknown (BVE eliminated 0 on veer).
- **Load-bearing invariant:** a retired variable must have a reconstruction witness, or it is never decided
  AND never reconstructed — a **wrong-SAT**, not a crash. `checkmodel` is the oracle; the DoD therefore
  weights SAT-heavy fuzz and requires a stress policy that deliberately decides retired variables, since
  R-c removes the only coverage that path had.
- **Pre-registered prediction:** retirement should beat the demotion arm on veer AND avoid its FmlaEquiv
  (+37 %) and 9vliw regressions, because those instances have ≤ 0.25 % substituted-variable decisions and so
  should be near bit-identical. **If FmlaEquiv still regresses, the design's premise is wrong** and the cause
  is queue reordering rather than the decisions themselves.

### 2026-08-01 — ▶▶ veer_axi's REAL defect: **84 % of its decisions land on variables V3 already substituted away**
Decomposition after TR was cleared (probes 1578702/1578703/1578705). The reference comparison is brutal and
it is **not** conflict count:

| | tactix | kissat | |
|---|---|---|---|
| conflicts | 20,118 | 2,967 | 6.8× |
| decisions | **1,611,949,266** | **435,946** | **3,697×** |
| **decisions / conflict** | **80,124** | **147** | **545×** |

- **The cause, measured directly** (`c substdec`, new): **1,354,519,748 = 84.03 %** of our decisions land on
  variables V3 Substitute has already rewritten out of every clause. Such a variable occurs in **no clause**,
  so deciding it propagates nothing and only deepens the trail — which is exactly the 1.13 propagations per
  decision and the 80,124 decisions/conflict. Search is 93.7 % of the clock; every rule together is 6.3 %.
- **Nothing retires them.** `substitute.cpp` makes no `heuristics_*` call, and the kernel has no
  active/inactive notion at all — **BVE-eliminated variables have the same gap**. N1 keeps every variable
  decidable, so they may not be REMOVED; but ranking is advice, not narrowing (the distinction that makes
  `no_ext_reason_bump` legal where `no_ext_decisions` is not).
- **Note this is partly SELF-INFLICTED**: before the per-SCC fix V3 substituted 0 variables on veer, so there
  was nothing to waste decisions on. The fix is still strongly net-positive (Tier B −8.45, +1 solve), but it
  converted "V3 does nothing" into "V3 does a lot and leaves 202,986 variables loose in the decision order".
- **Demotion arm (`TACTIX_SUBST_DEMOTE`, default OFF; `heuristics_demote_marked_vars`, the scattered-set
  analogue of the fresh-var demotion):** veer **201.31 → 131.40 s (1.53×)**, decisions 1.61 G → 394 M,
  substdec share 84.03 % → 42.03 %. **But it is a TRADE**: FmlaEquiv **24.41 → 33.56 s (+37 %**, conflicts
  818 k → 1.21 M) and 9vliw 11.82 → 12.27 — both instances where substdec was ≤ 0.25 %, so they pay the
  trajectory perturbation and collect none of the benefit. Tier pair required before any ship decision.
- **The residual says demotion is not the real fix**: even demoted, **42 %** of decisions still land on
  substituted variables, because ranking them last still means deciding them once everything else is
  assigned. The real fix is RETIREMENT — an inactive-variable notion the kernel lacks, which would serve BVE
  too, and which needs an explicit N1 reading (a rewritten-away variable is not a meaningful branching
  choice; its value is fixed by its representative and the reconstruction witness). **Architecture question,
  route via revise-plan/revamp — do not bolt it onto V3.**

### 2026-08-01 — ▶ TransitiveReduce is NO LONGER veer_axi's problem: the V3 fix collapsed it 37×, and the residual is TRAJECTORY
Probe 1578698 (post-V3-fix, shipped binary). The standing "V4 TR is a ~1000× engineering gap on one rule"
framing is **retired** — it was measured before V3 started substituting.

| | before the V3 fix | **now** |
|---|---|---|
| TR time | 109,697 ms (**36.6 %**) | **2,940 ms (1.5 %)** |
| TR ticks | 8,736,859,973 | 434,185,028 |
| TR deletions | 210,966 | **325** |

- **The V3 per-SCC fix fixed TR as a side effect.** V3 now removes 202,986 variables before TR walks, so the
  binary implication graph TR traverses is vastly smaller: **37× less time, 20× fewer ticks**. Any TR
  *engineering* work is now worth ≤ 2.9 s of a 201 s solve. **Do not start there.**
- **The residual TR cost is TRAJECTORY, not runtime** (L7/L9 shape). Ablation: shipped **201.19 s / 20,118
  conflicts** vs `TACTIX_SLOT_DISABLE=TransitiveReduce` **157.25 s / 15,401** — TR costs **~44 s** while its
  own clock is 2.9 s, for a yield of **325 deletions out of 554,654 checks (0.06 %)**. n=1, so indicative,
  not a verdict; but the conflict delta says the mechanism is real.
- **This is NOT a missing-capability gap** (L14, reference on the same instance): kissat solves veer in
  **2.72 s**, and `kissat --transitive=false` in **2.69 s** — its own transitive reduction is worth
  **nothing** there. So "match kissat's TR" is not the lever either.
- **▶ The gap is elsewhere and the ablation bounds it:** TR-off still leaves **157 s vs kissat's 2.72 s =
  58×**. Removing the entire suspect removes ~22 % of the gap. Whatever veer's real problem is, it is not
  TransitiveReduce — and per **L8** a single ablation is an inflated upper bound, so 22 % is generous.
- **Standing:** V4 admission/dosing on veer (325 deletions bought with 44 s of trajectory) is a clean §5.6
  signal in the same family as BVE admission — NOT a shipped rule-off ([[no-rule-off-configs]]: every
  implemented proof rule ships ON; a rule-off arm is a diagnostic for a SCHEDULING defect).
- Also available, unused: `TACTIX_V4_EARLY_EXIT` — our walk deliberately runs to exhaustion where kissat
  breaks at `dst` (`transitive_reduce.cpp` deviation 3, to feed failed-literal detection). That was measured
  only BUNDLED with the tick meter in TD-R1 and killed; at 1.5 % of the clock it is no longer worth
  unbundling for veer.

### 2026-08-01 — ▶▶ SHIPPED: V3's PER-SCC hazard skip — Tier B **104.06 → 95.61**, `veer_axi` TO → **238.7 s** (+1 solve)
The fix for the cause found below, gated on a same-window tier pair from ONE binary
(`TACTIX_SUBST_HAZARD_ROUND=1` restores the old per-ROUND skip, so the A/B isolates exactly the granularity).
Pre-registered ship criteria — Tier B must improve WITH the predicted solve and lose none; Tier C must stay
in T3's band and within the noise floor; no verdict/proof failures — **all met**.
- **Tier B `20260801-154122`: 95.61 (25/28) vs 104.06 (24/28)** — −8.45 and **+1 solve**, and the gained
  solve is the predicted one: **`veer_axi` 300.0 TO → 238.7 s**, **nothing lost**. 202,986 vars substituted
  where the old code substituted **0**.
- **Tier C `20260801-154125`: 330.24 (29/30) vs 328.27** — +1.97, **well inside the ~10.8 noise floor**, no
  solve change, and **1.044× cadical keeps T3's CaDiCaL half MET**.
- **The defect in one line:** the hazard skip's "rare" precondition is per LITERAL, the skip was per ROUND.
  On veer, **284 hazardous literals out of 406,256 (0.07 %)** vetoed 100 % of the work, five rounds running.
  Banked as **L23**.
- **Correctness:** ctest 38/38; fuzz **4 × 10,000 cases, 0 failures** (DRAT + native LRAT × ± `--random-policy`,
  ASan+UBSan), no new bundles in `tools/fuzz/failures/` (all five there date from 2026-07-27); both tier arms
  wrote and checked DRAT with no rejections (12 proofs checked on Tier B, up from 11 — the new solve).
- Instrumentation kept: `c substitute` now reports non-trivial SCCs, literals, vars substituted, and rounds
  skipped/partial — the counters that turned "V3 is inert" into a located defect in two probes.

### 2026-08-01 — ▶ veer_axi CAUSE FOUND: V3's hazard pre-check skips **5 of 5 rounds**; the "rare" case is universal at scale
The counter the entry below asked for, added (`substitute_sccs` / `substitute_scc_lits` /
`substitute_hazard_skips`). It **refuted** the leading hypothesis and found the real cause. Probes
1578294/1578295, 300 s cap:

| instance | non-trivial SCCs (lits) | vars substituted | **rounds skipped on the hazard check** |
|---|---|---|---|
| **veer_axi** | **1,458,092 (1,815,902)** | **0** | **5 of 5** |
| FmlaEquivChain | 328 (1,398) | 0 | 6 of 9 |
| 9vliw_bp_mc | 890 (1,148) | 42 | 5 of 11 |
| x9-08014 | 0 (0) | 0 | 0 of 5 (control: no SCCs ⇒ no skips) |

- **NOT a detection failure — the opposite.** V3 finds **1.46 M non-trivial SCCs** on veer and marks 1.82 M
  literals substitutable. Hypothesis (i) ("V6's binaries aren't in the BIG when V3 runs") is **dead**.
- **The cause is `do_substitution` step (A)**, the duplicated-binary hazard pre-check: a pair with ≥2 live
  copies cannot be deleted by name, so the code **skips the WHOLE ROUND** rather than emit a half-applied
  proof. Its own comment calls this "Rare" and defers "the erratum's finer per-SCC skip".
- **"Rare" is a PER-LITERAL claim while the skip is PER-ROUND, so it inverts at scale**: P(round survives)
  falls as the marked set grows, and the instances with the most equivalences to exploit mark the most
  literals. veer marks ~363 k literals/round → **5/5 skipped**; 9vliw marks ~104 → 5/11 skipped and 42 vars
  actually substituted. The rule is most inert exactly where it would pay most.
- **Consequence:** veer's 179,110 congruence merges are found, proof-logged as binaries, and then **entirely
  wasted** — V3 is 100 % inert there for a reason that has nothing to do with the equivalences themselves.
- **The fix is the one the code already names**: unmark only the offending SCC class (all members sharing
  that representative) and proceed with the rest — per-SCC keeps the all-or-nothing granularity the proof
  needs, without letting one bad pair veto a round of 363 k literals. **NOT yet claimed: that fixing this
  speeds veer_axi up.** V4 TransitiveReduce is still 36.6 % of its clock, and L20 warns against attributing
  an instance's wall time to the mechanism you happen to be holding.
- Method note: the first version of the hazard counter made the round's `return` **unconditional** (inserted
  a statement into a brace-less `if`), so `do_substitution` always returned NoOp — caught immediately by
  `ctest` (test_substitute contradictory-units). ctest 38/38 after the fix.

### 2026-08-01 — ▶ veer_axi: V6 finds **179,110 merges** and V3 Substitute consumes **ZERO** — the owed question, answered
The 2026-07-30 Tier B timeout diagnosis flagged this as owed instrumentation: "`c congruence` prints neither
gates nor merges, so whether Substitute removes the 161,462 equivalences Congruence finds on veer is
UNKNOWN". Both counters added (`congruence_gates`/`congruence_merges`, and `substitute_vars` — `c substitute`
previously printed only ROUNDS, so the consumer side was equally unanswerable). Probe 1578218, 300 s cap:
```
c substitute  5 rounds, 0 vars substituted
c congruence  6 rounds, 2325340 gates, 179110 merges, 0 units, 0 unjustified
c transitive  0 rounds, 999154 checked, 210966 deleted, 2 failed, 8736859973 ticks
c timing      TransitiveReduce 109697 ms (36.6%) in 544088 applications · Congruence 9705 ms · Substitute 310 ms
```
- **V3 is INERT on veer_axi despite 179,110 equivalences being available** — five rounds, 310 ms, zero
  variables substituted. The equivalences are found, proof-logged as binaries, and then **not collapsed**.
  V6's own cost is only 3.2 % here, so this is not V6 being wasteful; it is V6's OUTPUT being dropped.
- **This is a new, concrete lead and it is NOT the TransitiveReduce engineering gap** (which is real and
  separate: 36.6 % of the clock, 8.7 × 10⁹ ticks, **544,088 applications**, against kissat's 0.03 s).
  Hypotheses, untested: V3 detects equivalences independently rather than reading V6's union-find; a budget
  cuts it; the phase order runs V3 before V6 so it never sees the new binaries; or `substitutable()` is
  blocked by something structural on this instance. **Cheap to discriminate and worth doing before any
  TransitiveReduce work** — 179k unconsumed equivalences is a larger prize than a constant-factor fix on a
  rule whose 4× improvement still leaves the instance over the cap.
- **Hypothesis space NARROWED by code read (no cluster time), so the next session can go straight at it.**
  V3 detects equivalences INDEPENDENTLY — iterative Tarjan SCC over the binary implication graph, reading the
  binary watches of `negate(p)` — and never reads V6's union-find. That is correct per the contract (rules
  never call rules), and two explanations are ruled out **by construction**: the Tarjan is explicitly
  complete (the header records the deliberate choice NOT to take CryptoMiniSat's `max_scc_depth`
  early-return, "which silently drops deep SCCs"), and the out-edge scan applies no redundancy filter.
  **What remains: (i) V6's equivalence binaries are not PRESENT/watched when V3 runs** — deleted by V4
  TransitiveReduce (210,966 deleted on this run), garbage-collected, or a phase-order artefact (5 substitute
  rounds vs 6 congruence rounds); **or (ii) SCCs are found and the CALLER rejects them** (`substitutable()`
  reads a `sub[]` selection built outside `substitute_find_sccs`). Discriminate with one counter — SCCs of
  size ≥ 2 found per round — which separates "never detected" from "detected then dropped".
- Instrumentation is default-on, counter-only; ctest 38/38.

### 2026-08-01 — ▶ task #10 DONE: rules now poll the interrupt; worst-case SIGTERM latency 18.4 s → 2.9 s
The §6 defect closed. `stop` was a `Policy::solve` PARAMETER, so no rule could see it — and polling at the
phase round loop + slot boundaries (`0a1e0aa`) bounds latency to ONE APPLICATION, which is exactly the block
that is too long (V6 Congruence, 131,327 ms over 3 applications). A harness escalates to SIGKILL and **the
proof is lost**, so this is N2/§6, not a latency nuisance.
- **The mechanism:** `Kernel::stop` + `kernel_terminating()`, published by every policy through the RAII
  `KernelStopPublisher` (restores rather than nulls, so nested solves compose and an early return cannot
  leave a dangling pointer). NOT policy knowledge — external-environment state, the same category as
  `k.proof`, so the kernel stays policy-free (§5.3).
- **Congruence polls at five SAFE points:** the four read-only extraction loops (abort is free — nothing is
  applied yet, the caller just returns `NoOp`) and the closure worklist, where each completed merge has
  already emitted BOTH its equivalence binaries and its proof chain. **Soundness: aborting means the rule did
  LESS.** Everything done is proof-logged, abort points are between complete units of work, never
  mid-mutation — the contract Factor's tick-budget cuts already satisfy. It cannot change a verdict.
- **Measured** (probe 1578198, T105, `TACTIX_BVE_ADMIT=off`): SIGTERM→exit at 30 s **18.36 → 1.94 s**, at 45 s
  3.07 → 2.91, at 60 s 5.63 → 2.29. Worst case **18.36 → 2.91 s**. `c interrupt` ≥1 on every interrupted
  post-fix run and **0** on the identity run — reachability proven, not inferred.
- **Inert when uninterrupted:** identity leg reproduces the pre-fix proof **byte-for-byte** (68,947,155 B,
  `s UNSATISFIABLE`) in two independent rounds.
- **Correctness:** ctest **38/38 release and 38/38 ASan+UBSan**; new `TACTIX_STOP_FUZZ` stress mode fuzzed
  **4 × 10,000 cases, 0 failures** (DRAT + native LRAT, `--random-policy`, ASan+UBSan) — after **L22**, which
  is the story of the first four legs passing vacuously.
- Residual, not claimed as fixed: 1.9–2.9 s of latency remains unattributed (proof flush, other unpolled
  rules, or the exit path), and the original ~40 s / ~1,100 s case was NOT reproduced on this config — its
  congruence applications were ~18–25 s, not ~44 s. The fix bounds the worst case measured; it is not proven
  against the worst case reported.

### 2026-08-01 — TB4b: the watch-repair primitive lands, and the shortcut's negative verdict turns out to be a DefaultPolicy bug
Report `benchmarks/reports/2026-08-01-tb4b-watch-repair.md`; commits `fc57eff` (primitive + un-crippling +
audit) and `a489bb7` (the attribution arm). Ships OFF — **no default flip recommended**, K5 fired.
- **The July verdict is RETIRED.** `analyze_apply` clears `pending.valid` on the reuse path; the INSTANCE
  path checked it, **DefaultPolicy's micro-loop did not**, so every reuse ran the learn/backjump tail on the
  previous conflict's record — stale glue, a real backtrack that UNDID the reuse's own assignment, then a
  `backjump_stale`. Only DefaultPolicy was hit, so `--random-policy` and the corpus never saw it. Pinned by a
  unit test that aborts on `assert(backjump_guard)` with the fix reverted. TB4's lesson, second time.
- **The primitive:** `kernel_unwatch_large` (the inverse `push_large` never had) + `kernel_promote_watched_lit`.
  Watch-only edit — membership/size/id/arena position untouched, so proofs, the dense index and every LRAT
  chain stay valid. `implementation-notes.md` §12. OFF-path inertness PROVED by byte-identical DRAT totals
  against pre-change HEAD.
- **The mechanism is CHRONO-ONLY** (on a level-sorted trail the shape cannot occur — the clause would already
  have propagated), so at `chrono_levels=100` a 90-var random 3-SAT gives **0 reuses in 2,105 conflicts**; two
  versions of the audit test measured nothing before that was understood. `--reuse-stress-policy` added.
- **Three-way attribution** (5 arms + replicates, `20260801-092756`). The un-crippling is real (DRAT −12.0 %;
  FmlaEquiv **−18.7 % conflicts, −12 % wall** with non-overlapping replicate ranges, where crippled gives
  +1.3 %) and costs **+2.0 % on Tier B(28)**, in three instances — **qwh 3.1× reproduces** (pre-registered
  kill), PRP 1.63×, Timetable 1.65×. **But core14 and new14 order ON vs CRIPPLED OPPOSITELY** (core14: ON
  inside the OFF band 10.27–10.84, CRIPPLED 11.11 above all of it; new14: ON +2 %, CRIPPLED free), so neither
  aggregate is the verdict. The July −9.2 % does NOT reproduce for the un-crippled arm on its own instrument.
- **Tier C(30) `20260801-094032`, replicate spread 2.40:** OFF 331.33/333.73 · ON 343.85 · CRIPPLED 342.63 —
  **ON and CRIPPLED indistinguishable**, so on Tier C the cost is the MECHANISM, not the repair. The whole
  +3.4 % is TWO instances (ContextModel **19.1×** — off a 20.06/19.97 replicate pair, the tier's most stable —
  and af-synthesis 5.1×) against EDP3 0.35×, DLTM 0.41×, em_11 0.57×, T105 0.77×, asconhash 0.83×.
  Proof bytes FLIP SIGN between tiers (B −12.0 %, C +8.1 %): "fewer learned clauses" is not a payoff proxy.
- **Lesson (L12 family):** the effect is CHAOTIC IN THE DOSE — **one** reuse halves crypto1's conflicts, 67
  quadruple qwh's. There is no static separator to look for; this is §5.6 exhibit material, cleanly labelled.
- Fuzz 6 × 10k/0 (DRAT + native LRAT × default / `--random-policy` / `--reuse-stress-policy`, ASan+UBSan),
  corpus 10/10 incl. 002+005, `backjump_stale` 0 everywhere, ctest 38/38 both builds.

### 2026-08-01 — ▶ task #11: mechanism LOCATED and reference-anchored; the fix REFUTED; broad admission's Tier B value REPRODUCED
Report `benchmarks/reports/2026-08-01-task11-factor-admission-mechanism.md` (supersedes the routed lever of
the 2026-07-31 profiling report §6). Four increments, four probes, three tier runs; new lessons **L19–L21**.
- **The routed lever is refuted four ways** — `factorthresh` is dead code in cadical; `factordelay`'s polarity
  was read backwards (`=0` is the MOST-delayed setting, so the probe showed delaying Factor *more* costs
  cadical 2.3–9×); the gate **never binds on either loser for either solver**, i.e. we already match cadical
  exactly; and the first-dose allowance (kissat 700M vs cadical 300M) is **measured inert**, explained by
  Factor's 10.9–104M whole-solve tick spend. **L19.**
- **The mechanism, anchored to the reference** (patched cadical built OUTSIDE the repo, L14): cadical branches
  on ITS extension vars **0.10 %** of decisions on ContextModel where we do **31.39 %** (314×), 0.01 % vs
  0.38 % on jgiraldez — but **8.81 % vs our 6.19 % on sted5, MORE than us**, while adding more ext vars
  overall (627 vs 459). **Cadical DISCRIMINATES rather than suppresses**, which no static priority can do.
  The channel is **reason-side bumping**: on ContextModel ext vars take **2.97× as many bumps per variable**
  as originals and **71.68 % of those are reason-side ONLY**. A BVA extension variable *is* the hub of the
  clauses it replaced, so it is promoted for its structural POSITION, not for evidence — and it
  self-reinforces, which is why one-shot burial moves the share only 31.4 % → 32.2 %.
- **The fix is REFUTED at tier level.** `TACTIX_NO_EXT_REASON_BUMP` (N1-clean, ships OFF) looked excellent
  per-instance (ContextModel 2.4×, jgiraldez 12.3×) and is a **tier-wide negative**. Tier B 4-arm factorial
  `20260801-105456`: shipped **105.65** (24/28) · **broad alone 64.15 (26/28)** · **arm ALONE 127.42 (23/28)**
  · broad+arm 113.45 (24/28). Tier C `20260801-092459`: shipped **323.88 (29/30) vs cadical 315.38 = 1.027×**
  · broad+arm **468.43 (27/30)**, losing mp1-Nb5T15 and j3037 outright. **The arm is independently harmful**,
  not a trade.
- **Two extrapolation errors of mine, banked as L21**: estimating a tier from a two-instance decomposition
  (predicted B/C ~87/~334, measured 113.45/468.43), and measuring the fix against the arm it repaired rather
  than against the shipped default (against the default ContextModel is **19.1 → 189.6 s, still 10× worse**).
- **▶ THE STANDING POSITIVE: broad Factor admission alone reproduces, cleanly** — **64.15 (26/28) vs shipped
  105.65** in a same-window factorial, independently confirming 2026-07-31's 65.86/26. It solves `sted5` and
  `or_randxor`. Its Tier C cost remains the blocker, and every static control over it has now been measured:
  formula shape (0 for 5), when it runs (matched), first-dose allowance (inert), extension-var priority
  (refuted). **Per L18's precedent the exhaustion IS the result** — E2 Factor admission is the best-labelled
  §5.6 dosing exhibit in the repo, with a named channel, an instrumented per-instance feature
  (`c extbumps`) and a measured reference target. The decision belongs to the phase-6 learner.
- Artefacts, all default-off/inert with mod4block identity re-confirmed at **418,796** conflicts:
  `c factor … ticks`, `c extbumps` + `Stats::bumps{,_extension,_ext_reason_only}`,
  `Kernel::no_ext_reason_bump`, `DefaultPolicy::FACTOR_INIT_TICKS`. ctest 38/38 throughout.

### 2026-07-31 — task #11 DIAGNOSED (not fixed): three mechanisms, four causes eliminated, the lever is WHEN Factor runs
**SUPERSEDED IN PART by the 2026-08-01 entry above: its §6 routed lever ("*when* Factor runs") is refuted.**
Report: `benchmarks/reports/2026-07-31-factor-broad-admission-profiling.md`. Why broad Factor admission is
7–28× worse for us than cadical on ContextModel / jgiraldezlevy.
- **cadical's Factor is load-bearing there, not idle** — `--factor=false` costs it 5.9× conflicts on
  ContextModel, and it adds MORE extension vars than we do (627 vs 459) while still being 7× faster. So the
  gate is not the defect and application COUNT is not the story.
- **New instrumentation `c extension`** (`stats.decisions_extension`): **31.4 % of ContextModel's decisions
  land on its 459 extension variables** (jgiraldez 0.38 %, sted5 6.2 %). Previously unanswerable from a run.
- **Decisive ablation** (`TACTIX_NO_EXT_DECISIONS`, diagnostic only — narrowing the action space violates
  N1): ContextModel 450 → **123 s (3.7×)**, jgiraldez 452 → 518 (no effect), **sted5 17 s → killed >528 s**.
  **Three mutually exclusive mechanisms** — ContextModel wants extension vars near-never, sted5 needs them
  freely, jgiraldez doesn't care. That is precisely why every static knob trades one instance for another.
- **Four causes eliminated:** scope threshold (dead), payoff bound (dead), **candidate reduction**
  (`factorcandrounds`, implemented; byte-identical on all 7 probes — a TRUE negative: the filter needs a
  literal occurring <2×, which is 0.72 % / 0.89 % / **0.00 %** of literals on ctx / jgi / sted5), and **our
  own demotion**, which *fails at its purpose* — ctx's extension-decision share moves 31.4 % → 32.2 %,
  because conflict analysis bumps extension vars straight back to `queue_last`. A one-shot reordering cannot
  hold against continuous re-bumping.
- **Surviving lever: WHEN Factor runs.** Forcing cadical eager (`--factordelay=0 --factorthresh=0`) degrades
  it 2.3–9× toward our regime on both instances; we factor in the preprocessing blast, cadical delays BVA
  between eliminations. Plus: extension-variable priority must be **adaptive and persistent**, not one-shot.
  Routed to rule-dev + the scheduling track — **not** to a fifth formula-shape separator (that family is 0/5).

### 2026-07-31 — ▶ `factor_size = 5` SHIPPED (Tier C −9.51, 1.036× cadical); the Tier B lever is held back and WHY
Report: `benchmarks/reports/2026-07-31-factor-scope-and-admission.md`. The 2026-07-30 union arm bundled TWO
changes; they are separable and pull opposite ways. **Scope (`factor_size` 2→5) ships:** Tier C 337.50 →
**327.99** (1.066× → **1.036×** cadical), Tier B 104.79 → 105.30 (+0.50, noise), no solve-count change, all
four pre-registered KILL instances clean (x9 +0.8, mp1-blockpuzzle −0.0, FmlaEquiv −0.6, mod4block +0.4).
Fuzz on the new default **4 × 10,000 cases, 0 failures** (DRAT + native LRAT, each ± `--random-policy`;
legs `1576699/1576700/1576701/1576702`) **plus 2 × 200,000-case nightly legs, 0 failures**
(`slurm-s5def-nightly-{drat-1576904,lrat-1576905}.log`, the LRAT leg under `--random-policy`); ctest 38/38. **Admission (`factor_scope_percent`) does NOT ship:** it is Tier B **65.86 (26/28)** — solving
`sted5` and `or_randxor` — but Tier C **357.64 = 1.129×, OUTSIDE T3's 10 % band**.
- **The entire Tier C regression is TWO instances** (ContextModel +464 s, jgiraldez +396 s = +860 vs a net
  +623); excluding them broad admission would *improve* Tier C by −7.9. **cadical runs Factor broadly with
  no shape gate and solves both** — ContextModel 65.8 s (us 453.7), jgiraldez 16.4 s (us 465.4). So broad
  admission is not wrong; **our Factor is 7–28× worse than cadical's under it**. That is the open lever, and
  fixing it is what merges the Tier B gap. Task #11.
- Mechanism is **search DAMAGE, not cost**: conflicts 2.4–8.4× on the losers, and 14× *fewer* on sted5.
- **Two separators measured dead.** Scope threshold: winners and losers sit at the same scope fraction
  (j3037 100.0 % −99.7 vs ContextModel 99.8 % +400). Payoff bound: gain histograms straddle — edp3 is a
  WINNER at mean gain 1.4 (1686/2717 factorings at δ=1) while sted5 is a winner at 13.5 and the losers sit
  between; measured, it costs sted5 19 → 274 s (b8) / 118 s (b16). **Do not try a third.**
- **N1 bug:** `Kernel::factor_bound` was **unreachable under DefaultPolicy** (`fm ? fm->accept(…) : …`, and
  DefaultPolicy always attaches `DefaultFactorMatch` with `accept` hardcoded `delta >= 1`) — a documented
  config surface no shipped path could exercise. Found only because the sweep returned **byte-identical
  proofs**. Fixed. *An A/B arm producing identical artefacts was skipped, not tested.*
- **Banked arm `TACTIX_FACTOR_DEMOTE`:** fresh extension vars were the MOST-preferred branching candidates
  (`heuristics_reserve_vars` appends at `queue_last` = where VMTF search starts; kissat demotes them; our
  stable heap was already right). Rescues j3037 207→88 s, edp3 74→39, rxor 49→36, but costs **sted5
  16→150 s**. Default OFF pending its own tier pair.
- **NOISE FLOOR, now measured** (the inert bound sweep left three bit-identical Tier C arms): **10.8 PAR-2
  aggregate spread between identical configs**, per-instance mean range **24.7 s**, goldcrest alone **245 s
  (72 %)**. So goldcrest's ±70/45 swings are noise (0 factorings, 272,082 conflicts in BOTH arms), and
  `s5only`'s −9.51 sits INSIDE that band — the flip is justified **per-instance** (af-synthesis −190.9 s
  against a 2.5 s replicate range, conflicts 2.15 M → 318 k), not by the aggregate. Read every Tier C delta
  against this bar from now on.

### 2026-07-31 — the Factor admission trade decomposed (superseded by the two entries above; kept for the lesson)
`2a40126`. First pass of the same investigation: damage-not-cost, the goldcrest phantom, and the two dead
separators — all folded into the entries above and
`benchmarks/reports/2026-07-31-factor-scope-and-admission.md`. **Lesson worth keeping: an A/B arm whose
artefacts are byte-identical was SKIPPED, not tested** — a payoff-bound sweep returned identical 430.5 MB
proofs in all three arms, which is how `Kernel::factor_bound`'s unreachability under DefaultPolicy surfaced.
Check the artefact, not just the score.

### 2026-07-30 — TD-R5 increment 2: the widened Factor grid, and **or_randxor (the one true wall) SOLVES**
*(**TD-R5** = the timeout-diagnosis lead "rule SCOPE" — `factor_size`. NOT Track R · R5, which is V5 `Sweep`.)*
`9f20813` design · `4c13bfd` inc 1 · `b9416c9` inc 2a · `2191466` inc 2b · `687e86f` inc 2c.
`Kernel::factor_size` (default 2 = binary-only, unchanged) widens remainders from literals to sub-clauses;
`TACTIX_FACTOR_SIZE` is the arm. **Target: `or_randxor` 3,600 s TO / 42.5 M conflicts → 47.55 s SOLVED**
(297 factorings, 27,570 large remainders; kissat 12.38 s), inside the design's predicted 15–60 s.
**`fclqcolor` does NOT recover** (78 factorings, still TO) — the scope was necessary for rxor, not
sufficient for fclq; the design's 30–120 s prediction there was wrong.
- **BOTH conditions are needed and that is the open problem:** the widened scope only helps where Factor
  RUNS, and the density gate suppresses it on exactly these binary-sparse instances (rxor: 1 factoring
  without FORCE). Joint admission is UNRESOLVED — a reference study of kissat's real mechanism is in
  flight, because I was inventing a gate instead of reading `factor.c` first (plan §5).
- **Cost work (2c) was structural:** grid resolved ONCE per factoring (was O(occ(f))·m·n, twice over),
  rarest-literal direction search (cadical's `min(|watches|)`), and `find_grid_clause` now charges
  `factor_ticks` so the effort ledger can actually stop it. Before it, every target ran past its timeout.
- **A bug increment 1 hid from me, worth the lesson:** `best_R` became remainder IDS but the re-seed loop
  still indexed `tried[]` with them — an ASan heap-buffer-overflow that **bit-identity on four instances
  passed clean, twice**, because ids stay small at size 2. The regression corpus under `--random-policy`
  caught it. Bit-identity and fuzz are both required; neither subsumes the other.
- Evidence: bit-identical at size 2 on all four gate instances (incl. mp1-blockpuzzle 686 / x9 80
  factorings); corpus 10/10 and 10k/0 on DRAT and native LRAT at size 5, all `--random-policy`; ctest 38/38.
- `stats.factor_large_remainders` added — it is what distinguished "the widened branch never fired" from
  "it fired and never won the delta contest".

### 2026-07-30 — TD-R1 and TD-R2 both KILLED at tier level: for this rule family the lever is ADMISSION, not dosing
Both top leads from the Tier B timeout diagnosis (`TD-` namespace — Log key; NOT Track R's rules) died on
their pre-registered kill criteria; shipped defaults unchanged throughout, every arm retained as a §5.6 knob.
- **TD-R2** (remove the BVE binary-density gate, `TACTIX_BVE_DOSE_DENSITY=0`): Tier B(28) flat 105.21 → 105.84
  but **Tier C 324.24 → 516.91 with THREE lost solves** (T105/T87/asconhash). Residue worth keeping:
  **`crypto1` IMPROVED** without the gate (7.0 → 3.3), so half of L9's justification is refuted and on
  Tier B it is a ONE-INSTANCE (mod4block) defense — while on Tier C it defends a real and different set.
  The flat Tier B aggregate hid a one-for-one swap (gained `sted5` 600 TO → 2.4 s, klieber 118.4 → 6.5;
  lost mod4block 13.5 → 600 TO). Cost-based admission calibrated too: mod4block is a clean outlier at
  **109,573 resolutions/elimination** vs 33–7,997 for the ENABLE set, but **asconhash needs suppressing at
  151** while eliminating 42,570 vars cheaply — L2/L7 trajectory damage, which cost-per-yield cannot see
  by construction. So cost is a better signal for one class and does NOT subsume density.
- **TD-R1** (meter V4 in ticks + kissat's early exit): Tier B(28) **105.40 → 123.29, a lost solve**; Tier C
  flat 323.67 → 325.87. The four pre-registered kill instances did NOT fire (rbsat −1.4, mp1-blockpuzzle
  −0.9, FmlaEquiv −0.4, 9vliw +1.1), so the **2026-07-25 TR-dosing bank does not reproduce** and the meter
  is safe there — but klieber INVERTED, 118.1 → 600 TO, giving **off 18.4 / full 118.1 / partial 600**.
- **THIRD sighting of "partial is worse than either extreme"** after the X1/X2/X4 trio (L15) and BCE's
  round cap (L17/L18). Four dosing attempts have now failed the same way. **Do not retry a V4 budget as a
  klieber recovery.** Arms: `TACTIX_BVE_ADMIT`, `TACTIX_BVE_COST_LIMIT`, `TACTIX_V4_METER`,
  `TACTIX_V4_EARLY_EXIT`.
- Method note: both sub-agent pre-tests were single-instance and rate-based and both predicted the opposite
  of the tier result — L12/L16 on the recovery side.

### 2026-07-30 — ▶▶ T3's CaDiCaL HALF IS MET, with every rule ON: X2 at cadical's 8‰ (`29811a8`)
We gave `HyperTernaryRes` **20‰** where cadical's `ternaryeffort` is **8** — 2.5× the reference's share, and
X2 is the one rule with no internal governor (its ported size limits `ternaryocclim`/`ternarymaxadd` are
INERT, ~38× too loose to bind — `75b6308`). Same-window A/B `20260730-085509`: **Tier C 397.94 (28/30) →
319.19 (29/30) vs cadical 317.04 = 1.0068×, INSIDE the band, gaining a solve**, with Tier B(28) flat
(103.24 → 103.68). Not a single-instance artefact: 12 of 30 move ≥15 s and it is **−11.8 PAR-2 even
excluding j3037** (which goes TIMEOUT → 392.8 s).
- **Replicated in THREE independent windows** — 1.0068× / 1.015× / 1.034×, all 29/30 — well past L16's bar.
- Satisfies the directive and the gate together (user, 2026-07-30: "I don't think we should disable X1, X2
  and X4. We should work on scheduling"): all 13 Inprocess rules stay ON and the fix is one constant.
- Caveat: the sub-agent measured 8‰ at only 1.8×/1.5× on nw/klieber against 53×/6.2× for X2 OFF, so
  `ternaryrounds` (we run 25) remains an untested and likely stronger bound. **T3's Kissat half is still
  NOT met** (1.37× on the rule-neutral subset).

### 2026-07-30 — the four Tier B timeouts are TWO defects, and our SEARCH is not one of them
Sub-agent diagnosis, 209 solver runs over four arrays; report
`benchmarks/reports/2026-07-30-tierB-timeouts-diagnosis.md`, commits `37eb21b`/`b769965`.
- **Our search is not the defect** — on rxor/klieber/fclq our conflict count sits on top of the baseline
  stripped of its load-bearing pass, and **on fclqcolor we beat CaDiCaL outright** (369 s vs its TO@300).
- **DEFECT A — dosing calibrated on binary-DENSE formulas.** Every Tier B instance we lose by >2.3× is
  below 6 binaries/var, the threshold of BOTH elimination gates, so we do zero BVE and zero BVA where
  kissat eliminates 40–86 % of variables. Second half: **TransitiveReduce's ledger is in CANDIDATES against
  a search-tick reference**, so a 20× rate cut is BIT-IDENTICAL — its budget has never bound.
- **DEFECT B — rule-SCOPE.** `kissat --factorsize=2` (kissat restricted to our variant) loses rxor entirely
  and costs fclq 12.6×, the same as `--no-factor`. → TD-R5 above.
- `veer_axi` is a CONSTANT FACTOR, not a wall: **2,199.8 s vs kissat 3.2 (687×)**, TransitiveReduce alone
  585.7 s of it where kissat spends 0.03 s. **`or_randxor` was the only true wall** (42.5 M conflicts
  unsolved at 3,600 s, props/conflict 0.99× — now solved by TD-R5).
- **L14 trap banked:** rxor spends 91.2×10⁹ walk steps on a refutation; the walk-off arm prices them at
  ~4 %. Do not act on them. **L11 upheld** — Sweep is not the lever on any of the four.
- Owed instrumentation: `c congruence` prints neither gates nor merges, so whether Substitute removes the
  161,462 equivalences Congruence finds on veer is UNKNOWN.

### 2026-07-30 — §6 defect: the inprocessing slot never polled the stop flag (partially fixed, `0a1e0aa`)
`*stop` was checked at exactly ONE site (the outer search loop), so a long inprocessing phase left the
solver **deaf to SIGTERM for ~1,100 s** (T105/T87 with BVE admission off, jobs 1575474/1575475, killed by
the SLURM wall having printed nothing). A competition harness escalates to SIGKILL and the proof is lost —
a §6 compliance defect, not a latency nuisance. Polling added at the phase round loop, each slot-entry
boundary, and inside the dosed per-instance loop.
**SUPERSEDED 2026-08-01 — task #10 is DONE (see the 2026-08-01 entry): rules now poll via `Kernel::stop`.**
**It did NOT fix the observed case and the code says so:** lag is 40 s before and 41 s after, because the
block is INSIDE one application — `c timing` names it, **V6 Congruence, 131,327 ms over 3 applications,
89.5 % of a 146 s run that never reached search**, and an UNMETERED slot entry. This bounds latency to one
application instead of one phase; the real fix is a termination flag the RULES poll. Task open.

### 2026-07-29 (night) — the trio's MECHANISM: **BCE was silently bounding X2** (`5a44365`)
Corrects an earlier entry that read mod4block's BCE dependency as a ~14× SEARCH gap of ours. It is not:
that comparison was never like-for-like (our −BCE arm still ran X2, which ADDS ternary resolvents, while
kissat has no ternary module), and the run archive shows we are FASTER than kissat there — 9.30 s vs 20.55
before the trio landed, 13.1–13.6 vs 18.9–19.4 with it. Probe 1574441:

| disabled | wall | conflicts | X2 resolvents | BCE deleted |
|---|---|---|---|---|
| BCE | **600 TO** | 11,587,329 | **32,155** (83 rounds) | 0 |
| BCE + X2 | 24.11 | 810,983 | 0 | 0 |
| **BCE + X2 + X4** | **9.13** | 321,902 | 0 | 0 |
| X2 | 87.23 | 2,651,385 | 0 | 6,310 |
| none (shipped) | 12.89 | 418,796 | 6,048 (17 rounds) | 6,343 |

**Deleting blocked clauses shrinks X2's candidate set** — so with BCE off X2 runs 6,048 → 32,155 resolvents
and the solve dies. That is why removing either alone is catastrophic, and it is what made X2's own
governor (the effort share, above) the fix. L15 a third time, mechanism named.

### 2026-07-29 (night) — Tier B DOUBLED to 28 instances / 28 families, one clock (plan rev 74, `ff32e6f`)
Sub-agent expansion. kissat band [3.33, 19.40] s over 3 repeats, 28/28 solved, 14/14 UNSAT proofs verified;
report `benchmarks/reports/2026-07-29-tier-b-expansion.md`, reserve pool
`benchmarks/probes/tierB-expansion-candidates-2026-07-29.tsv`. **Why:** at n=14 one instance decided A/Bs —
mod4block alone was the ENTIRE Tier B value of the trio (+586 s of a +581 s net) and flipped that verdict
when excluded. Max single-instance share of the reference mass **16.2 % → 7.1 %**; a solved→timeout flip
42.9 → 21.4 points. **Halved, not removed.** The old `ref_seconds` were M3-Pro seconds and the M3→cluster
ratio is 1.13–1.76 per instance, so the whole set had to be re-measured. `core14` preserved as a marked
subset. Two protocol findings folded into the benchmarks skill: the GBD 404 was a HOSTNAME artefact
(`fetch.py` needed no change), and only a LOW-CONTENTION probe can reject a candidate for irreproducibility.

### 2026-07-29 (night) — the augmented Tier B immediately found FOUR order-of-magnitude failures
First tactix run on the 28-instance list (`20260729-220951`): **102.28 PAR-2 at 24/28 vs kissat 9.54 at
28/28** — `veer_axi` 600 TO vs 3.2 s, `or_randxor` vs 12.4, `mp1-klieber2017s` vs 12.5,
`unsat-set-b-fclqcolor` vs 16.5. **NOT regressions** — instances the set never contained, so the weaknesses
were always there and invisible. Every Tier B verdict this project had recorded was taken on a set omitting
its four worst cases. This reset the roadmap above T3 and is what the 2026-07-30 diagnosis then decomposed.

### 2026-07-30 — nightly GREEN on both proof formats: 400k cases, 0 failures (`2c9387c`)
`--random-policy --nightly` on the debug/ASan/UBSan build, 50 cores per leg: **DRAT 200,000/200,000 cases 0
failures** (1574586) and **native LRAT 200,000/200,000 cases 0 failures** (1574587); `tools/fuzz/failures/`
gained nothing. **132,259 UNSAT certificates checked across the two formats** — drat-trim on one, lrat-check
on our own antecedent chains on the other — every one under an arbitrary legal schedule. Covers the
`c timing` registry dispatch wrapper and `Kernel::bce_occ_limit`. Logs committed as evidence.

### 2026-07-29 — the living docs compacted (4,069 → 3,001 lines); two drift sites closed
`011c81f`, plan rev 73. compact-docs pass, two rounds, each draft adversarially audited for LOSS — which is what made the cut lossless and what
found the drift. PROGRESS 1482 → 809, plan 1392 → 1251, implementation-notes 667 → 517, calculus 528 → 424 (this one follows `c498206`'s pass).
**Drift closed:** the shipped walk dose is `WALK_PERMILLE` **560**, not the 640 this file claimed (`0c52fdd` set 640, `61138b8` retuned to 560 once
the `bumpreasons` rate was fixed; the "200 balanced / 640 Tier C / 400 Tier B" readings were rate ARTEFACTS — L12 — superseded by
`2026-07-27-tier-c-push.md` §8.2), and **W4f was complete but never ticked** while still telling readers the constant was 50. Also: the Log had no
entry for `8f6cae4` or `50e8d80`; `implementation-notes` described D-CONG-TAUT as an LRAT-path deviation when it bails in BOTH formats
deliberately (`congruence.cpp:712/802/969`); PROGRESS cited "plan §5.7" for a design-doc section; plan §5 said "Two obligations" over three.
Invariants held: 47 → 48 `[x]` (the delta is W4f), all 47 Log entries + 3 new, all 15 lessons, 19/19 rules with calculus entries, every ledger row
keeping its removal path, ctest 38/38.

### 2026-07-29 — codebase review: drift closed, and the learner's missing reward ids
`revisit-codebase`; report `benchmarks/reports/2026-07-29-review-post-track-r.md`, fixes `3daef1a`. **Worst finding was DRIFT:** four sites said the opposite of what
ships — `implementation-notes.md` §1's X1/X2/X4 rows ("Not scheduled by DefaultPolicy") and `instantiate.hpp` ("ships OFF until its own A/B says otherwise"), all three ON
since `3fb85bd`; the guard table is what a reader trusts for shipped behaviour. **§5.6 gap:** every other inprocessing rule exposes output via `tactix_stat` (13, 17–25),
the three newly-scheduled ones exposed nothing → ids 26–31 added as **(yield, effort) PAIRS**, cost-per-unit-output being the axis that orders their outcomes (BCE: 13
ticks/deletion where it buys a 50× speedup, 7,700–43,000 where it costs). Dead `Stats::learned_raw_lits` removed; `SubSolver::conflicts()` KEPT, reason recorded at the
site (S-f reports it beside `ticks()`). Behaviour-preserving: ctest green release+debug, **Tier A 0.44 (200/200) vs kissat 0.45 unchanged**, fuzz `--random-policy` 10k/0
(1572349). Routed, not fixed (report §"Routed"; the `RV-` namespace — Log key): **RV-R1 (med) — Track R · R5's S-c MUST add sub-solver fuzz legs
when it becomes reachable**; RV-R2 Reorder has no stress policy; RV-R3 `benchmarks/probes/` needs a retention rule; RV-R4/RV-R5 duplication + allocation.

### 2026-07-29 — T3's CaDiCaL half is NOT met with the rules on; the cause is TWO instances
`8f6cae4`, same-window Tier C `20260729-003352` (29/30 every arm): all-on 357.23 = **1.14× cadical, OUTSIDE the 10 % band**; rules-off 313.20 = 1.00×; cadical 3.0.0
314.49 (Kissat half already not met, 1.37×). Recorded in the gate row. **Not a broad regression: j3037_9_rggt_b (138.8 → 1152.4, +1014 s) and asconhashv12 (324.4 → 627.4,
+303 s) are 100 % of the +1322 s**; the other 28 net to zero (minxor128 −234, T87.2.0 −198, sat-bench-trig −163, ContextModel −102) ⇒ recoverable by understanding two
instances, not a scheduling redesign — probe 1572298 (`benchmarks/probes/j3037-firing*`) asks them the Tier B question (which rule, how many rounds). Constraint from the
four dead admission designs: **mod4block is NON-MONOTONE in how consistently the trio runs** (never 9.26, always 13.17, once-then-stop 263.87) ⇒ all-solve or not at all,
which kills every mid-solve decision (you cannot measure the trio's productivity without running it, and having run it you cannot stop) and leaves only a static
per-instance decision — L5 measured that at chance. Report `benchmarks/reports/2026-07-28-all-rules-on.md`.

### 2026-07-28 — EVERY PROOF RULE IS ON; four admission gates are measured-dead
User directive, verbatim **"all implemented proof rules need to be turned on!!"** — closes a drift vs the standing [[no-rule-off-configs]] baseline. `3fb85bd`; report
`benchmarks/reports/2026-07-28-all-rules-on.md` (7 same-window Tier B arrays, probes 1571606/1571678). `TACTIX_SLOT_TRACE` audit: X1/X2/X4 the only disabled entries; **13
of 13** Inprocess rules now scheduled, hardcoded exclusions gone. **The price: Tier B 7.94 → 9.27** (shipped 7.69–8.23 over 7 windows, all-on 9.07–9.62 over 5; kissat
8.55), 0.93× → 1.08×, 14/14 throughout — removing it is the open scheduling item. **No rule separable** (alone BCE 21.79 / X2 53.26 (13/14) / Instantiate 13.08; together
9.22) and **dose is the wrong control (L7/L12 in a third mechanism)** (2/5/10/20/50‰ → 9.99/25.64/19.52/9.62/13.76, non-monotone; mod4block alone swings 13/232/139/14/49
and manufactures the shape). **Underneath it everything is rate-INDEPENDENT: clique wins 50× at every rate.** Mechanism (1571606): BCE's whole value is ONE
round on the raw formula (clique 434,299 → 14,315 conflicts), its whole cost the rounds after (rbsat 2 rounds → 4× conflicts) — **a rate change shrinks each round; it
never stops round 2.** **Four admission designs all dead vs 9.17 for no gate (L15)** (dose reduction; preprocess-only 27.68; density 51.99 / 29.08 at threshold 20; yield
backoff 52.21), each killed by mod4block (263.87 / TO / TO) though each helped its targets (crypto1 6.69→4.93 density, rbsat 12.7→9.8 yield) and the yield axis separated
by an order of magnitude (13 vs 477–6,726 ticks/deletion; 472k/885k ticks for ZERO deletions on battleship/qwh, 1571678) — and it STILL lost 43 PAR-2: **any partial
suppression breaks the mutual repair**. Out of the tree, banked in the report — do not re-run. Soundness: 10k/0 DRAT (1571808), native LRAT (1571809), `--random-policy`
(1571810); ctest ×2. Kept `TACTIX_SLOT_PERMILLE=Name:rate[,...]` as a §5.6 dosing arm, in the shape of `TACTIX_WALK_PERMILLE`.

### 2026-07-28 — Track R · R5 increment S-b: the refutation is now a payable proof obligation
`9db22df`: `derivation()` returns ordered lemmas with LRAT chains over environment ids + the core (design `docs/design/v5-sweep-design.md` **§4** — a swept equivalence is
entailed but NOT RUP, so the chain must come from this solver's own resolution); chain order is the kernel's (ascending trail position, conflict last,
`kernel_lrat_chain`) and every lemma follows from the ENVIRONMENT ALONE, which lets lemmas survive across assumption sets and restarts. Two refutation shapes: a level-0
conflict yields the empty clause from the reason cones of its literals; a falsified assumption yields `¬a₁ ∨ … ∨ ¬aₖ` over exactly the assumptions in the cone of `¬a`
(`src/rules/sweep/subsolver.hpp:147 refute_empty`). Contradictory assumptions yield NOTHING (tautological target, refused by `Proof::add_rup` — L13). Binaries moved into
the arena (every clause needs an id) — net simplification. Verified three ways (LRAT replay, entailment by enumeration, core discipline) over **207,503 refutations**,
56,915 under assumptions, plus `tests/sweep_proofs.sh` — 40 micro cases through the REAL drat-trim + lrat-check, ASan+UBSan clean. **Method lesson:** the first version of
BOTH oracle and emitter produced only one-step derivations, so the chain mechanism was untested while green — both now assert a multi-step floor (L14 shape).

### 2026-07-28 — Track R · R5 increment S-a: the embedded sub-solver, brute-force verified
`8ac2199` — O11 executed (`docs/design/v5-sweep-design.md` §3: write ~600 focused lines, not transliterate kitten's 2877); the three deliberate divergences from the
kernel (per-literal watch vectors, fixed decision order, MiniSat-shaped assumptions) are documented atop the .cpp. **Verified by exhaustion** because nothing else reaches
it yet (no proof until S-b ⇒ checkers and fuzz oracles are blind): `tests/test_subsolver.cpp` enumerates all 2^n assignments — **1,000,000 iterations green**, 334 k under
assumptions, ASan+UBSan clean. Safety property for S-b onward: an unanalyzable conflict returns `Budget`, never `Unsat`. Not a rule yet — Sweep registers at S-c/S-d.

### 2026-07-28 — Track R · R4 `Reorder` lands (ships OFF): the loss is ENTIRELY SAT-side
`4482bb5` — a policy op (`H`-only, no proof delta, no calculus entry) + two new `H` primitives (`heuristics_score_set`, `heuristics_rescale_scores`); guard deviations in
`implementation-notes.md` §10 (we drop kissat's `level == 0`, a call-site property, so a policy may reorder MID-TRAIL). **The A/B decomposes** (window
`20260728-190456-B-shipped`, all arms DRAT-checked, 14/14): shipped 7.72 / stable-only 13.55 / both-modes 14.14 / kissat 8.40 — **UNSAT at parity (10.16 → 10.11 / 10.36),
SAT 3.4× worse (5.28 → 16.99 / 17.93)**, real UNSAT wins cancelling real losses; re-ranking discards the decision/phase continuity a model is assembled from, a refutation
does not need it. **A new §5.6 axis — a SAT-vs-UNSAT CONTEXT signal** no static gate can read, unlike the four formula/progress-dosed families; confirms L11 (kissat's own
`--reorder=0` costs it −0.7 PAR-2) while sharpening it. 8 unit tests + ctest ×2, probe 1571114, fuzz 10k/0 DRAT (1571053) + native LRAT (1571054).

### 2026-07-28 — Track R · R3 `Instantiate` lands (ships OFF); two doc-sync gaps closed
`2085429` — X4 under the §5.3 contract reusing C3 Vivify's probing scope; the strengthened clause is RUP against `F` **alone** because `¬(C∖{l})` leaves `C` unit and
propagates `l` itself, so the chain cites `C` at `l`'s trail position (cadical's construction). 9 unit tests, ctest green, fuzz 10k/0 DRAT (1570888) + native LRAT
(1570889). **Ships OFF** (Track R convention; measurement arm `TACTIX_SLOT_ENABLE=Instantiate`) and **alone it pays nothing** (probe 1570890: 57,357 probes → 5 literals
on FmlaEquiv, conflicts 938,595 → 1,025,143; cadical also ships it OFF, keeping the idea as `vivifyinst`); Tier-B A/B `20260728-181900-B-shipped` shipped 7.66 /
+Instantiate 12.67 / kissat 8.52, the whole loss in crypto1 + mod4block — mis-scheduled, not expensive (L7/L9 shape); later reversed by the all-rules-ON directive (L15).
**Doc-sync:** X1 BCE had no calculus entry and no guard row (landed 2026-07-27 without them) and `calculus.md` still called BCE "phase 4" — now §6c (X1) + §6d (X4) + both
guard rows; the golden registry list in `tests/test_api_c.c` moved by one, the update that guard exists to force.

### 2026-07-28 — CLAUSE QUALITY CLOSED: minimize+shrink is AT PARITY with kissat, the 21.6 % target is dead
`639d50e`; report `benchmarks/reports/2026-07-28-clause-quality.md` §6 (probe 1570882). The control ran first and killed the lever: **kissat gets 22.1 % on FmlaEquiv
against our 21.6 %** (42.2 % vs 40.8 % on 9vliw, 43.7 % vs 38.5 % on x9), so the contrast that generated the target reproduces in kissat and every stage is at parity (raw
1-UIP 1.10/1.08/0.91×, final size 1.11/1.10/0.99×; kissat's cumulative mean learned size on FmlaEquiv is **16.91, not ~10**) — **L14**. Counter notes: METRIC-level in
kissat (`./configure --metrics`), and such a build must be staged OUTSIDE the repo because `tools/solvers/bin/kissat` is a SYMLINK into the clone (in-place rebuild
silently replaces the pinned baseline); in kissat's default config `literals_minimized` is identically 0 (`shrink=3` returns early), so only the SUM minimized+shrunken is
comparable; kissat keeps no cumulative glue counter. **FmlaEquiv's 2.49× conflicts is therefore not a clause-quality deficit** — the residual routes to search GUIDANCE,
not the analysis pipeline.

### 2026-07-27 (late) — N2 DEFECT FOUND AND FIXED: native LRAT emitted hint-less lines
`716948d`. **Found by measurement, not fuzzing:** the 6-arm format array (`20260727-204153-C-prooffmt`) verified `T105.2.0`'s DRAT (99.8 MB, 22.3 s) while cake_lpr AND
lrat-check both rejected its 1.12 GB LRAT — the array task's own failure was a *second*, independent defect, a cake_lpr OOM under the 8 GB cgroup (it needs 4.2 GB). Root
cause: `congruence_rup_chain` returned SUCCESS with an **empty hint chain** for a trivially-true lemma, so all three ladders wrote `<id> <lits> 0 0`. Fix at the one
shared choke point (false on an empty chain, caller skips the merge — D-CONG-TAUT; the XOR matcher's `top < 1` path REPLAYS both binaries; `add_equivalence_chained`
all-or-nothing; both call sites count `congruence_unjustified`), plus two debug asserts in `Proof::add_rup`/`add_rat` making the class self-detecting. **Lesson L13** (the
permissive format is not the safer one to check against; cross-format checking is coverage, not redundancy). Fuzz 10k/0 congruence-stress+LRAT (1569454) and 10k/0
default+LRAT (1569455) on the assert-enabled debug build; probes 1569178 / 1569447 / 1569453.

### 2026-07-28 — clause-quality probe: the conflict gap is ONE INSTANCE, not a pattern
Report `benchmarks/reports/2026-07-28-clause-quality.md` (probes 1570461/64; new `learned_lits`/`learned_glue_sum` counters + `c learnedsize`/`c reduction` lines):
conflicts vs kissat: FmlaEquiv 2.49×, 9vliw 1.15×, **x9 0.78× — fewer conflicts and still not faster** — so "our clause quality is worse" retires as a general claim and
becomes an FmlaEquiv-family statement. Its proposed "21.6 % → ~40 %" lever was killed the same day by the parity control above. **The depth cap is NOT it** (probe
1570502): `TACTIX_MINIMIZE_DEPTH` 900/1000/100000 byte-identical, closing the deferred `literal_redundant` depth-cap item as measured-dead. Correction on file: the
probe's first pass ran with `conflict_reuse` ON, so its 1.77×/0.94× figures described an experimental build. The x9/9vliw puzzle (conflict parity-or-better, no win on
time) is TB5's per-conflict-cost question, kept separate.

### 2026-07-28 — Track R, round 2: dosing recovers most of the damage, not enough to ship
`1c5f2c5` — effort specs for `HyperTernaryRes` and `BCE` at 20‰ (BATCH + `budget_aware`, NOT per-instance `dosed`: both build a round-local index) and BCE's cost model
fixed to ONE arena pass per round into a literal→clause index carrying redundant clauses too (`Kernel::dense` is irredundant-only), replacing an O(nlits × arena) round.
Same-window Tier B `20260728-081739-B-shipped`, undosed → dosed: BCE 239.91 (9/14) → **22.01** (14/14), X2 27.56 → **53.06** (13/14), both 198.91 (10/14) → **10.84**, vs
shipped 7.75 / kissat 8.55 — 11× and 18× better, nothing pays, both stayed OFF (reversed days later). Two keepers: **the pair beats either alone** (L15's first sighting —
X2 adds clauses, BCE removes blocked ones) and **dosing made X2 WORSE**, so 20‰ is the wrong rate rather than dosing the wrong idea — **the per-mille sweep is the open
lever**, and exactly the constant the phase-6 learner should subsume.

### 2026-07-27 (late) — Track R A/B: BOTH new rules ship OFF (measured, decisive)
`d0c7c2d`; window `20260727-223610-B-shipped`, throttle 48, all arms writing DRAT, each rule enabled in the S4 slot via `TACTIX_SLOT_ENABLE`: shipped 7.89 (14/14), +X2
27.56 (14/14), +BCE 239.91 (**9/14**), +both 198.91 (10/14), kissat 8.67 — BCE turned 6 instances into timeouts (`summle_steps7`, `mod4block`, `hwmcc17miters`, `9vliw`,
`FmlaEquivChain` → 600 s), X2 cost `mod4block` 9.7 → 270.0. Rules CORRECT (10k/0 fuzz both formats); the SCHEDULING is not. Two causes: **no `EffortSpec`** (our slot runs
enabled rules unbudgeted where both references dose them hard — "enabled in our slot" ≠ "enabled in cadical") and **BCE's naive cost model** (full arena scan per
literal). Re-confirms L11 from the other side: importing the reference inventory does not import its performance. Later reversed by the all-rules-ON directive — L15:
one-at-a-time was the wrong experiment for this family.

### 2026-07-27 (late) — plan rev 72 + Track R opened: like-for-like measurement, full reference rule inventory
`d0bb6ec`. **User directive:** compare against Kissat/CaDiCaL with proofs on BOTH sides, and implement every proof rule the references have. **Measured tax, Tier B same
window** `20260727-212407-B-tactix-drat` (report `benchmarks/reports/2026-07-27-like-for-like-proofs.md`): tactix 7.96 on / 7.44 off (+6.9 %), kissat 8.72 / 8.35 (+4.4
%), cadical 20.06 / 19.10 (+5.0 %) → like-for-like Tier B **0.91× kissat** (both logging) or **0.89× (both proof-free)**, where the old proof-free convention reported
0.95× and understated us; drat-trim's cost is not ours alone (kissat 221 MB → 269 s vs our 255 MB → 104 s). Plan rev 72: §8.1 proof protocol (every arm writes proofs;
asymmetric scoring — our failed check = unsolved, a baseline's recorded only; checker budget `max(3600 s, 4 × timeout)` replacing `2 ×`) + the reference rule families
pulled into phase 3. **X2 `HyperTernaryRes` implemented** first (resolvents collapsing to ≤ 3 literals, `add_rup` with a fixed two-parent chain = E1's resolvent argument,
no new primitive, no witness; calculus §6c, 7 unit tests; registered but not yet scheduled, `TACTIX_SLOT_ENABLE` its measurement arm).

### 2026-07-26 — S6 `Walk` lands (plan rev 71): SCPC-1000-18 TO → solved, Tier B 9.84 → 8.94
`8da55c9`; diagnosis first via debug-performance, report `benchmarks/reports/2026-07-26-scpc-walk-profiling.md`: the sole actionable Tier C timeout is a **MISSING RULE**,
not a search residual — SCPC-1000-18 has provably zero inprocessing surface, cadical's `walkeffort` 800/80/8/0 gives a monotone dose–response (3.0 s / 24.7 s / 452.7 s /
TO), kissat's `--walkeffort=0` times out too, and our search matches BOTH walk-less baselines conflict-for-conflict (at 3600 s the pre-Walk build burned 109.6 M conflicts
/ 24.0 G props and still returned UNKNOWN, probe 1563267). Generalization: neutralizing kissat's walk costs it Tier B 8.62 → 29.22 and Tier C 221.19 → 282.48; our shipped
9.93 already beat walk-less kissat by 3×. **The port needed a correction:** kissat's `B,W,I,B,W,O` (Walk REPLACES Best) regressed us to 13.63, and a dry-walk control
proved the cause was the lost Best consumption, not the phases — kissat saves phases at ASSIGN time, we save at unassign. Shipped `B,B+W,I,B,B+W,O`: Tier B **8.97 vs
9.88**, SCPC 1200 s TO → **259.9 s**, Tier C **FLAT (386.54 vs 387.83)**, the SCPC gain cancelled exactly by mp1-Nb5T15 3.66 s → TIMEOUT. Two dosing knobs measured dead
(kissat's own effort anchor; accept-only-if-better); warmup measured and NOT adopted (our saved phases are already propagation-consistent); "when to walk" joins "when to
eliminate" in the rev-68 §5.6 exhibit. Fuzz ×3 legs 10k/0.

### 2026-07-25 night — C4-perf CLOSED: hwmcc native LRAT 1680 s → 9.64 s (174×); Track C complete
`d2f7695`: the replay-propagation theory was WRONG — 97 % of cycles were memset, from O(nvars) fill-initialized scratch per proof-chain op (three sightings of one shape:
`big_path_chain`'s trio ~13 MB/bridge, `congruence_rup_chain`'s per-lemma `seen`, vivify's twin; probes 1563224/1563236). Fix = persistent scratch restored in O(marked);
proof BYTE-IDENTICAL, lrat-check 1.8 s, LRAT ~1.3× DRAT; replay architecture vindicated. [[per-call-linear-scratch-hazard]]

### 2026-07-25 late — C3b CLOSED: AND degeneracies + contradictory merge; D-CONG-UNIT closed
`2998029`: AND clash → unit, idempotent dup → unary merge, `x≡¬x` → refutation at both merge sites; AND conclusions are directly RUP so the ladder's top==0 replay covers
every chain; the duplicated-hint hazard sat in `add_equivalence`'s bridge assembly (closed by order-preserving id dedup). Calculus + ledger synced same commit; corpus
008/009/010; fuzz 10k/0 ×4.

### 2026-07-25 late — congruence extraction engineering: hwmcc 9.15 → 6.62 s, trajectory-identical ×2
`c6d388f` (AND/OR extraction literal-major over the phase OccIndex, tag sort restoring exact clause-major emission order, arena-scan fallback kept for N1) + `947ab4e`
(XOR/ITE ClauseIndex + dedup sets on a fixed 5-slot SmallKey with exact vector ordering/equality semantics); conflicts EXACT ×5, fuzz 10k/0 ×2 each. Standings after: **B
9.93 first sub-10, A 0.71** (`96f321a`). **Lane STOPPED** — the remainder is a CongGate-SoA revamp-scale change.

### 2026-07-25 late — B losses decomposed per-instance; TR budget arms BANKED NEGATIVE; walk cost priced
`8b95dfc` (probe 1562919): FmlaEquiv = conflicts 1.53× (chrono active both sides, TB4b live); 9vliw = TR's 28 %-of-cycles exhaustive walk; hwmcc = the
congruence-extraction pipeline (fixed above); crypto1 = the admission exhibit; clique parity. A TR meter defect (candidates vs ticks) found and priced
(`stats.transitive_ticks` kept, trajectory-identical), but **BOTH honest-budget arms lose same-window** (B 12.22 / 13.92 vs 10.6x; mp1 out of envelope) — exhaustive
unbudgeted TR is load-bearing, and TR dosing joins the §5.6 exhibit as its second mechanism family.

### 2026-07-25 late — s055 restart-rate lead closed: machinery faithful, delta = glue distribution
`1bb8e79`: line-by-line audit vs kissat restart.c/smooth.c/reluctant.h found ALL faithful (trigger, margin, EMA 33/1e5+bias, reluctant 1024/2^20, staleness, level-0,
both-mode reuse) — the 34-vs-41 conflicts/restart delta is the EMA responding to hotter learned glue, a clause-quality residual (since closed at parity, L14).

### 2026-07-25 late — clean T3 window: 398.74 vs CaDiCaL 304.21 = ratio 1.31×
`cede6d4`, window `20260725-193123-C-t3clean`, baselines proof-free: proof writing had inflated them ~4 %; tactix window noise ±0.6 %. (Protocol superseded by
like-for-like, plan rev 72.)

### 2026-07-25 late — TB5 closed-reframed: per-prop AT PARITY; residual = search shape; TB2 closed
`9bbcdf2` (probes 1562819–26): s055 conflicts at PARITY (938,705 vs 927,770), per-prop ≈1700 vs ≈1615 cycles, so wall 1.34× is props/conflict 1.43×; FmlaEquiv = conflicts
1.53× × volume 1.21× at per-prop parity; x9 1.17× ≈ parity ⇒ the flat-1.39×-throughput premise was STALE, the watch-layout engineering lane is measured-dead, TB2
dissolves. kissat ablations (1562823/24): eliminate saves ~10 % of props at identical conflicts, walker neutral, vivify-off FASTER (chaos), forced-BVE worse (32.5 vs 30.6
s) → s055 labeled into the phase-6 admission exhibit. Lesson: kissat's `propagations` includes probing + warming, so our ratios are conservative.

### 2026-07-25 — closure-delay ladder DEFAULT ON; C 395.56 @ 28/30 (T105+T87 cracked); TB6 closed
`3cef985`+`2639a5a`: kissat's delay ladder on unproductive unmetered closures, yield = the registry Ok/NoOp contract (defect: 24 zero-yield congruence closures on
FmlaEquiv, probe 1562606; arm probe 1562607 → FmlaEquiv 24 → 20 s at IDENTICAL conflicts, 9vliw −10 %, hwmcc/urq45 byte-identical). Arrays `20260725-1241*`: B 10.56 vs
10.67, C **395.56** vs 398.94 at equal solves, fuzz 10k/0 ×2 (1562610/11) → default ON (`TACTIX_NO_CLOSURE_DELAY` reverts); same window **T105 + T87 SOLVE** (proofs
checked; were TOs), C 528.66 → 395.56 in one day. **TB6 closed** (probe 1562605, `TACTIX_FORCE_MODE` banked): the era-stale 407 k claim does not reproduce — default
alternation BEATS both pinned modes on FmlaEquiv, focused pinning is catastrophic on s055 and −23 % on clique ⇒ another context-dependent phase-6 arm.

### 2026-07-25 — TB5 profile: gap = 1.53× conflicts × 1.23× props/conflict × ~1.2× per-prop; TR quadratic memset FIXED (7 % of cycles)
Perf on FmlaEquiv (probes 1562599–1601): wall 2.27× = instructions 1.72× × IPC deficit 1.32× (memory-bound: 3.9× L1d, 3.9× dTLB misses); TB1's 3.09× conflicts is stale,
now 1.53× (576,941 vs 377,701); `kernel_propagate` ≈156 vs ≈125 cycles/prop. **Hotspot landed** (`9763d88`): `transitive_reduce_try` built a fresh Walk per dosed
candidate — three O(2·nvars) zero-fills + 3 allocations each, **7.1 % of ALL cycles**; fix = persistent thread_local scratch re-inited only on nvars growth,
behavior-identical by the Walk clean-between-calls invariant. Gates: conflicts EXACT on FmlaEquiv/crypto1/s055 (1562602), ctest 31/31 ×2, fuzz 10k/0 ×2 formats
(1562603/04). Remaining leads: TR decline overhead (2.77 M checked → 3,263 deleted), congruence machinery ~14 % on a 0-merge instance, per-prop 1.25×.

### 2026-07-25 — I8 arrays + yield probes: dosing admission is the learner's; oracle 489.27; user no-rule-off baseline
Report FROZEN `benchmarks/reports/2026-07-25-bve-dosing-arms.md` (3-arm B+C matrix; oracle 489.27 vs shipped 528.66; cadence-open best on 11/30 C; shipped lost to the
bve-off CONTROL on C that window) — and with it the **standing user directive: no rule-off configs, ever; rule-off arms localize SCHEDULING defects**
([[no-rule-off-configs]]). Yield probes (clean array 1562588; the first, 1562578, was CONTAMINATED by array tasks sharing `$TMPDIR/inst.cnf` — hazard banked in memory):
**no runtime signal separates cadence winners from losers** (yield and res/elim ranges overlap fully; minxor/Grain WIN with conflicts UP so conflict-rate proxies misfire;
mod2c loses off 20 eliminations). Drip defect fixed; admission routed to phase-6 with the oracle table + 13 labeled instances (`bcef79d`); no new static gate (L5).

### 2026-07-25 — I3 cadence arm measured: flips crypto1 AND em_11 to profit; NOT default (x9/mod4block); I8 arrays next
`5d9269d`, `TACTIX_BVE_CADENCE` (A/B arm) = completion allowance + one dose/phase on the existing NLOG2N interval, i.e. kissat's call shape: **crypto1 512k conflicts/13 s
vs 566k/16 s dose-off** (first during-search BVE profit there, probe 1562435), **em_11 158 → 41 s** (gate false positive CONFIRMED; forced drip 78 s, cadence 41 s; probe
1562438); mp1 held/cheaper, battleship identical. NOT a default: same-window Tier B arrays `20260725-10295{8,9}-B-*` gave shipped 10.92 vs cadence 11.97 — x9 +10 s (tiny
completed doses perturbing the TB2-chaotic instance, probe 1562469) and mod4block intrinsic (L10). **Four instances, four optimal dosing policies — the §5.6 exhibit's
strongest data yet.** Fuzz 10k/0 ×2 formats cadence-forced (1562436/37); ctest 31/31.

### 2026-07-25 — BVE-tolerance I2 CLOSED: recurrence (drip-paced, incomplete) is the crypto1 mechanism; report frozen
`227ab97`, report `benchmarks/reports/2026-07-25-crypto1-bve-mechanism.md`. `TACTIX_BVE_ONESHOT` (temp policy-side A/B knob): crypto1 same-window {dose-off 566k/16 s ·
drip 1.83M/68 s · oneshot 631k/17 s}; the ramp probe saturates at depth 245 @ slack-16 and LOSES (L4's U-shape reproduced during-search). Verdict sharpened: **completion
+ geometric spacing, not non-recurrence** — kissat's 601 eliminations come from 4 completed calls at ≈2× conflict intervals, largest last. mp1/battleship guards held;
mod4block hurt by even ONE dose (L10 intrinsic). Probes 1562431/1562434; fuzz 10k/0 ×2 oneshot-forced (1562432/33); ctest 31/31 ×2.

### 2026-07-25 — BVE-tolerance I1 CLOSED: trajectory-class confirmed; kissat has no mod4block defense
`197dd62`, probes 1562429 (kissat -s -v, both arms) + 1562430 (tactix 3-arm same-window): all four I1 discriminators resolved → **L10**; the shipped gate is dose-off
byte-identical on mod4block (455,682 conflicts both). Routing: I2 next, I3's gate-removal branch demoted (kissat analogy refuted), the debug-performance lane stays closed
(damage is conflicts, not ticks). Housekeeping: `benchmarks/probes/` tracking policy decided — commit ONLY cited probes, delete uncited at wrap-up; its home is
`benchmarks/probes/README.md` (it resolves `docs/design/bve-tolerance-plan.md` §5.7 — NOT a plan section, which the old "plan §5.7" citation implied).

### 2026-07-25 — review fix-now executed: items 6–10 + rev-69 API + two measured negatives (task #7 CLOSED)
Two recorded NEGATIVES: the binary_ids flat-map replacement is trajectory-identical but uniformly ~1–3 % SLOWER (probe 1562428, `f325eb5` — site comment banks it) and GC
cost is ≪1 % (probe 1562204). Landings `13ce7d8` (items 1–5: dead residue, slot collapse, comment truth), `0e5ac25` (flat counts + in-rule env pin deleted), `610a1a7`
(plan rev 69 — stat ids 22–25 + config setters + contract test), `068fa96` (items 7+8 — shared proof idioms became the kernel primitives `kernel_emit_empty_from_pair`,
`PendingUnits`, `kernel_replace_clause`, replacing 4+3 rule-side copies byte-identically and aligning the latent glue divergence to 0-for-irredundant). Gates: ctest ×2
per step; 21k fuzz / 0 across 5 legs both formats incl. bve/congruence/factor stress; Tier A 0.73 / B 11.26 (noise band).

### 2026-07-25 — GC cost measured: a NEGATIVE — well under 1 % everywhere
Review §c item 2 resolved by temporary instrumentation (probe 1562204; counters NOT kept, user decision): heaviest case walks ~2 GB over 296 s (ascon; Grain 1.1 GB/407 s)
at ~10 GCs/phase — all ≪1 %. Closed as a recorded negative (L7 pattern). Standing constraint: per-apply GC under fine-grained Ref-payload dosing would be quadratic —
phase-6 note.

### 2026-07-25 — plan rev 68: elimination dosing = named phase-6 learning target
"When to eliminate" recorded as §5.6's **elimination-dosing exhibit** (−7 B / −32 C PAR-2 from one crude feature; kissat lacks the decision; crypto1 = the mechanism
ceiling). Phase-6 obligation: the learner subsumes the three density gates (thresholds = permanent arm, gate features = L2 contexts, dose on/off = a scheduled choice
point).

### 2026-07-25 morning — residual pair fixed: density-gated BVE doses; B 10.62 = 1.29× kissat, beats CaDiCaL
`1dbd439`: during-search BVE doses density-gated (≥6, the Factor axis) — crypto1 66.7 → 16.1, mod4block 60 → 13.6, mp1/battleship held; **B 17.45 → 10.62** (fresh
same-window kissat 8.26 / CaDiCaL 19.06), **C 552.91 → 520.76**. kissat `--no-eliminate` control (probe 1561949): mod4block's anti-elimination is UNIVERSAL (kissat 4×
faster without) so the gate is policy kissat lacks, while **crypto1 is a genuine mechanism gap** (kissat's elimination helps 4.1 vs 9.2 where ours hurt) → the
BVE-tolerance study. En route: `TACTIX_NO_BVE_LEVEL` bisect knob (level pacing was 24 s of the 57 s crypto1 regression; superseded by the gate).

### 2026-07-25 overnight — the mp1 gap MERGED (5.34 s vs kissat 9.09); S4 gate PASSED; arrangement default ON
Six commits `85e43b9`..`675a47c`; story in `benchmarks/reports/2026-07-25-s4-gate-mp1.md`. **Root cause was input hygiene:** 137,700 duplicated input binary pairs on mp1
inflated Factor's seeds AND silently disabled BVE project-wide (the duplicate-binary hazard NoOp'd every candidate — 0 eliminations EVER vs kissat's 661, found via
slot-trace when the arrangement's "deferred" BVE turned out never to run). Parse-time dedup (`beace90`) + BVE duplicate tolerance (`e0e73e7`) + increment-2 dosing
(`1e50f73`) + density-scoped composite (`c15571f`) → mp1 74 → 5.34 s. Gate: B 17.45 + C 552.91 vs legacy B 43.86 / C 537.83 → one-unit pass → default flipped
(`TACTIX_NO_ARRANGEMENT` reverts), A 0.82/199. Negatives banked (report §2): bound coupling reversed, removal-only marking not adopted, dosed-BVE delay reverted, plus a
build-race postmortem. ~162k fuzz / 0 across five code states both formats; ctest 31/31 ×2 per increment; mp1 proof VERIFIED.

### 2026-07-24 late — increment 1 lands; C verdict judged (superseded overnight)
C same-window `20260724-2213*`: head7 475.46/27 vs arr5 703.67/24 — gate NOT met *at that point* (goldcrest was a drat-trim 2400 s checker timeout on a 752 MB proof, the
§5.3 proof-size risk, live); superseded by the overnight pass, and a later stale re-assert of this verdict (`378975c`) was corrected in `ddfe3e6`. **Dosing increment 1**
(`85e43b9`): `stats.factor_ticks` + quit points, `vivify_tick_budget` → uniform `Kernel::work_budget`, `factor_bound`/`factor_composite` configs, budget-boundary stress
modes; ctest ×2, fuzz 27.5k/0 over 6 legs, mp1 dump BYTE-IDENTICAL vs `cebba5f`. **Open-items audit** (26-agent workflow): 66 items — 52 valid with homes, 9 closed
(V5-Sweep conditional not-promoted, P2-unknown watch, L7 ramp next-step, §8.1 revise owed, V6 stress owed, paths ship question, shakedown watch, Track-H deferral record).

### 2026-07-24 — S4 campaign (one day, 8 commits): mechanism complete, B won −36 %
Correction that started it (`53adb79`): the first "deferral" arm never ran BVE (interval parked at UINT64_MAX) so the arrangement was unmeasured; NLOG2N growing interval
wired (`c30a10d`). Attribution probes: Vivify's unbounded round = the dominant diffuse C cost, unmetered closures re-running every phase second, Congruence load-bearing
on urq45; dosing `e17c76b` (`vivify_tick_budget` + new-material gating) also surfaced a **latent mid-stable heap bug** — stable-mode extension vars never joined the score
heap, a segfault reachable by any N1 policy. Adaptive dosing (`143b320`, `0510177`: kissat delay + tier split, redundant tiers always, irredundant delay-gated) → **arr5 B
18.16** (2.2× kissat, beating kissat on battleship + summle); KEEP_PREBVE loses both tiers. Method lesson re-learned: solo-probe recoveries did not reproduce at array
concurrency (goldcrest 328 → TO) — L6. ctest 31/31 ×5; fuzz 6 legs × 10k, 0 fails.

### 2026-07-24 — S2b: composite grids default-OFF; every non-arrangement mp1 lever dead
kissat IS composite (verified: `resize_factoring` + re-arming, +1,303 further factored vars on OUR dump), but post-hoc composition cannot rescue our formula and our
single-blast composition REGRESSES the proxy 1.37 M → 2.18 M (L4). Fourth disconfirmed mechanism; **convergence: mp1 = S4** — kissat spreads 855 factorings over 16 dosed
rounds and our blast is the whole difference. Design `docs/design/s2b-composite-grids-design.md`.

### 2026-07-24 — FmlaEquiv diagnosed: pure search-quality gap (3.5×)
Decomposition probe 1560297: props/conflict at parity with kissat, conflicts 3.5× kissat's all-simplification-off control ⇒ not engine speed, not a missing rule — a
search-heuristic residual; joins x9/TB2. Suspects then named: TB4b, TB6 (38 switches here), minimization depth, retention — of these only the minimization depth cap is
since closed measured-dead (probe 1570502, `TACTIX_MINIMIZE_DEPTH` byte-identical) and TB6 was closed 2026-07-25 on this very instance (probe 1562605); TB4b and retention stay live.

### 2026-07-24 — S2a Increment 3: steering disconfirmed; width gap → S2b
`TACTIX_GRID_TRACE` added, proxy re-pinned on-cluster (kissat-on-our-dump 1.37 M vs native 305,691 — conflict counts are machine-independent). **The 27×27 cap is
tie-order-INVARIANT ⇒ structural, not steering**; the `paths` arm is −4 % and shelved behind `TACTIX_FACTOR_MATCH=paths`; the `sparse` arm reads 2.43 M, so dense-first
preference is load-bearing.

### 2026-07-24 — battleship RESOLVED: Tier B 14/14 (det fix + density 6)
Two landings (`benchmarks/reports/2026-07-24-battleship-divergence.md`): **cross-STL determinism** (trail tie-break in analyze's sort, sorted-key iteration in congruence
— now a standing invariant, `implementation-notes.md` §5; the old M3 14.1 s was basin luck) and the **Factor density gate 8 → 6** (battleship's ratio ∈[6,7) had been
excluded from its only robust path). Tier B 14/14 @ 32.23, Tier A unchanged, fuzz 28k/0.

### 2026-07-23 — Tier C re-baseline: ceiling-8 confirmed; fresh pins
ceil-8 439.72/27 vs ceil-16 449.62/27 ⇒ ceiling stays 8; fresh pins kissat 221.68/29, CaDiCaL 302.64/29 (runs `20260723-2314*`); the tactix outlier loss was SCPC-1000-18
(CaDiCaL 25 s) — later the Track W trigger.

### 2026-07-23 — S2a Increments 1+2: FactorMatchPolicy (bit-exact) + adversarial stress GREEN
Inc 1: grid selection → `FactorMatchPolicy` (2nd SubPolicy; null = built-in, bit-exact, Tier A proof bytes identical; design `docs/design/s2a-factor-matcher-design.md`),
plus the `.claude/skills/slurm-jobs/` skill (`09d6864`). Inc 2: the adversarial arm `--factor-stress-policy` (hash/tied/reversed scores, maximizing accept) — 18k+ cases,
0 failures both formats, KEPT as a durable stress axis. Framing corrected en route: reference kissat runs its OWN BVA by default (not SBVA), and a 4-agent design workflow
trusted a measured ablation over a code-read hypothesis.

### 2026-07-23 — migrated to the SLURM cluster (revs 64/65) + S1c + H7
**Migration COMPLETE in one session** (`7a276f4`…`fc07389`): builds, tools, cache, `tools/fuzz/slurm.sh`, `run.py --slurm` validated; all M3 PAR-2 pins retired
(conflict/resolution counts carry over); s055 solo 1.40× = TB5, no anomaly; fuzz cadence then scaled 10× (plan rev 66). Shakedown lesson: a toolchain change re-rolls
trajectory on sort-tie-sensitive instances (battleship 14 s → ~300 s from gcc-13/libstdc++ alone) — re-validate the fragile set after any compiler/stdlib switch. **S1c**
(`489fab1`): needed-bound cache + ceiling 16 → 8 — efficiency ALONE regressed crypto1 because the old ~1.0 M dose cap was an *accidental governor* keeping it out of the
U-shape; B 16.66 → 15.08; owed: Tier D re-sweep at gate time. **H7**: `H` ownership out of the kernel, trajectory-identical to `95b5740` on all 200 A + 14 B; lesson — the
two null-path bugs surfaced only under FULL-suite ASan with the default flipped to null-`H`, because bit-exact gates cannot see unexercised paths.

### 2026-07-22 — Track H H0–H6 · plan revs 59/60 · chrono ON by default
H0–H6 landed (see the Track H section); revs 59/60 changed the model — `H` is not part of σ, and is optional + policy-owned, on the user's sharpening that N1 needs the
branching *choice* exposed, not a *ranking* to exist. **The chrono saga** (`8e0bd01`): the first +13 % verdict was a porting error — analyze never backtracked to the
conflict level, so 84 % of chrono backtracks silently DROPPED the conflict; fixed → −2.4 %, ON by default, found only because the negative result was not accepted
(two-agent audit vs kissat AND cadical). `s055 = 932,762` retired as a regression check (chrono deliberately changed the trajectory). L8 extended twice by its own subject
matter; L7 refuted outright (the pre-registered prediction is why the refutation stuck).

### 2026-07-21 — V6 Congruence completed · effort scheduler increment 1
V6 C0–C4 (`1a365da`..`33437ca`) — lessons: the degeneracy ladder was drat-trim-validated BEFORE implementation (and then turned out to be kissat's own construction);
`TACTIX_CONG_PROBE` scoping corrected the plan three times (ITE discards were redundant work, not missing inference); replay beats derivation for proof chains
(checker-order by construction); a lemma literal already forced TRUE is also a conflict. Effort scheduler (revs 53–55, design `docs/design/effort-scheduler-design.md`):
the ledger's win was clause RECLAMATION, not the budget (per-dose collect = byte-identical conflicts, hwmcc 60 s vs DNF); P1 did not do what it was built for → L7; P2 is
a novel mechanism, not a port → L3. Architecture correction (user-caught): the rule MAINTAINS advice, `instances()` OFFERS everything, the policy DECLINES — the instance
surface is the action space, never narrowed for efficiency.

### 2026-07-20 — E2 Factor lands (S0→S2 partial) · Tier B head-to-head · mp1 diagnosis
Factor S0+S1 were proof-correct but net-negative; the inertness root cause was `kernel_delete_binary` refusing duplicated pairs so grids were never deleted → bloat; fix
`kernel_delete_binary_all_copies` → **ships ON, net-positive** (x9 801k → 453k). *A note from this day — "global dedup is NOT viable, dups are load-bearing" — was
measured on the LEGACY composition and is SUPERSEDED by 2026-07-25's parse-time dedup + arrangement; the S4 gate report is authoritative.* Faithful-port trap recorded:
kissat's DISTINCT-partner scoring measured WORSE than our raw watch count. Clean Tier B baseline 14/14 for the first time, PAR-2 70.0 vs kissat 6.26
(`benchmarks/reports/2026-07-20-tierB-vs-kissat.md`); orphan fuzz processes had been contaminating wall-clock (fix: `RLIMIT_CPU` backstop, `5462989`; cases preserved in
`tools/fuzz/known-hangs/`). mp1 isolated via `TACTIX_DUMP_FACTORED` — grids weak on mp1, good on x9 → S2 → S4 → solved.

### Phase 3 — inprocessing (2026-07-15 → 19)

- **2026-07-19 — V6 stages 1–3 + LRAT**: `hwmcc` UNSAT in 101.7 s (the forever-TO), needs XOR **and** ITE together; `clique` is NOT a congruence instance (search-bound);
  all 2^k parity clauses are required for XOR soundness and XOR/ITE equivalence binaries are NOT simple-RUP (hence the ladder).
  `benchmarks/reports/2026-07-19-congruence-tierB.md`.
- **2026-07-19 — negative result:** no toggle of existing rules cracks the hard UNSAT TOs; effort budgets are necessary but not sufficient (battleship broke at ANY budget
  — L3's instance).
- **2026-07-19 — inprocessing revamp, slot ACTIVATED:** diagnosis = mid-search root restarts derail SAT trajectories + O(n²) per-apply rebuilds; fix = preprocessing at
  conflicts=0 + `Kernel::dense` + kissat phase structure; urq45 1055 s/4.38 GB uncheckable → 86 s/0.53 GB verified.
  `benchmarks/reports/2026-07-19-inprocessing-revamp.md`.
- **2026-07-18 — E1 BVE (design → E1d):** a resolvent is RUP from its two parents, so BVE needs no new proof primitive (correcting the V3-era note), and gate reduction
  changes only which resolvents are generated, never the witness. The slot-activation assessment first said DEFER (battleship, one lost SAT instance) — the revamp fixed
  the named cause a day later. `benchmarks/reports/2026-07-18-e1-slot-activation.md`.
- **2026-07-18 — corpus 007:** V3 derived two contradictory root units and silently skipped the second → wrong-SAT. Lesson: run the nightly on the settled binary;
  `--quick` misses this class.
- **2026-07-17 —** stress-policy skill (rev 44; random-policy under-samples rare-but-legal schedules); guard purification revs 40–43 (D-LEVEL-1's root cause was a stale
  watch scan — inprocessing rules must DRAIN the root closure, not demand a fixpoint); calculus restructure; V3 complete (first ρ consumer); D-LEVEL-2; D-SPLIT
  (corpus-006 state unrepresentable); V4 + `kernel_delete_binary` primitive + `test_tr_bruteforce` oracle; N1 payload hole fixed (rev 34, [[n1-handoff-hazard-family]]).
- **2026-07-16 —** corpus 003/004/005 fixed; V1/V2/C1/C2/C3 + root-flush landed; N1/N2/N3 audit upheld.
- **2026-07-15 —** Tier C frozen (`benchmarks/reports/2026-07-15-tier-c-freeze.md`); the CaDiCaL pin was later found contention-inflated 2×
  ([[measurement-contamination]]).

### Phase 2 → T2 (2026-07-14)

- **T2 PASSED** (Tier B 11/14 @ 164.8 vs Glucose 268.3; props/s 0.92×). S3+S5, `kernel_propagate` extraction, native LRAT, API v1 frozen, demos 1–3.
  `benchmarks/reports/2026-07-14-t2-gate.md`.

### Phase 1 → T1 (2026-07-09 → 14)

- **T1 PASSED** (Tier A 200/200 @ 0.48; Tier B 9/14 vs MiniSat). M1–M4, S1/S2/S4 + stable mode, policy-class refactor (rev 17). Reports
  `benchmarks/reports/2026-07-11-t1-gate.md` (the failed first attempt, which ranked the levers) and `2026-07-14-t1-gate.md`.

### Phase 0 — setup (2026-07-09)

- Repo + plan + skills + CI; fuzz harness + benchmark runner; Tier A/B frozen; baselines + checkers pinned (`benchmarks/reports/2026-07-09-tier-b-freeze.md`).
