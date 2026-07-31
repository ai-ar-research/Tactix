# Progress

Living status. **Sync contract:** update this file in the same commit as (a) completing/starting any checklist item, (b) any DEVELOPMENT_PLAN.md
revision (via revise-plan). If this file and the plan disagree, the plan is authoritative — fix this file.

- **Plan revision tracked:** rev. 75 (2026-07-30)
- **Current phase:** 3 (inprocessing) — since 2026-07-14 (phase 2 closed at T2 the same day, report `benchmarks/reports/2026-07-14-t2-gate.md`)
- **Last updated:** 2026-07-30 (overnight) — **Tier B doubled to 28 and immediately found FOUR order-of-magnitude failures (up to 187× kissat); that now outranks T3.** T3/BCE fully diagnosed and parked on one directive decision (see resume block). Phase timers landed; docs compacted (rev 73); Tier B rev 74. Nightly fuzz 1574586/1574587 running.

## ▶ RESUME HERE (2026-07-30 morning) — phase 3, T3 open, and the priority CHANGED overnight

Read this block, then the top Log entries. Overnight work: 2026-07-29 evening/night entries.

**▶ THE HEADLINE: the augmented Tier B found four order-of-magnitude failures, and they outrank T3.**
Tier B is now **28 instances / 28 families** on one cluster clock (plan rev 74, `ff32e6f`+`77fc578`). First
tactix run on it (`20260729-220951`): **tactix 102.28 PAR-2 at 24/28 vs kissat 9.54 at 28/28** —
`veer_axi…p23_trans` **600 TO vs kissat 3.2 s (187×)**, `or_randxor` 48×, `mp1-klieber2017s` 48×,
`unsat-set-b-fclqcolor` 36×. **Not regressions** — instances the set never contained. Every Tier B number
this project ever recorded was taken on a set omitting its four worst cases. **Start here**, with
`veer_axi` (biggest ratio, 3.2 s reference ⇒ cheap iteration); check for a shared cause before treating them
separately. Do NOT tune on them (§8.1).

**WHERE WE STAND.** Like-for-like, every arm writing *and* checking proofs. **Tier B (28, NEW): 102.28
(24/28) vs kissat 9.54.** Tier B (core 14, comparable to the archive): 9.46 vs kissat 8.84. **Tier C 361.89–
405.38 (29/30) vs cadical 315.42, kissat 229.71** — the spread is real, j3037 straddles the 1200 s cap under
BCE. Tier A 0.44 (200/200) vs 0.45. **NEVER mix 14- and 28-instance Tier B PAR-2** — filter `core14`.

**THE T3/BCE THREAD IS FULLY DIAGNOSED AND PARKED ON ONE DECISION THAT IS YOURS.** Measured: the X1/X2/X4
trio is mutually stabilizing and it is all-on or all-off (L15 a third time, mechanism now named — **BCE was
silently BOUNDING X2**: with BCE off X2 runs 6,048 → 32,155 resolvents and mod4block dies). **Trio OFF gives
Tier B 7.94 AND Tier C 313.20 = 1.00× cadical, i.e. T3's CaDiCaL half MET — better than shipped on BOTH
tiers.** BCE-off alone is the WRONG cut (Tier C 1.0065× but Tier B 50.96 at 13/14, X2 left unbounded).
**I did not flip it:** that is the all-rules-ON directive's scope and it is yours to set
([[no-rule-off-configs]]). The directive-COMPATIBLE alternative, untested and cheap: **X2 has NO governor
while cadical bounds ternary FOUR ways** — `ternaryrounds=2` (we ran 83), `ternaryeffort=8‰` (we use 20),
`ternarymaxadd`, `ternaryocclim`. Porting those is the obvious next move and keeps every rule ON.
NOTE `TACTIX_SLOT_MAX_APPLIES` counts slot CONSIDERATIONS (~3/phase), not productive rounds, so `:2` ≈ rule
OFF — it cannot emulate `ternaryrounds` without a real per-round counter.
**▶ AND THERE IS A CHAIN HERE WORTH TRYING BEFORE TOUCHING THE DIRECTIVE — it would close Tier C with every
rule ON.** Tonight's two "measured-dead" governor results were each measured while the OTHER rule was
unbounded, so neither is actually dead in the configuration that matters:
  1. **Govern X2** (port cadical's `ternaryrounds=2` / `ternarymaxadd` / `ternaryocclim`; we have none).
  2. Then mod4block should no longer NEED BCE, because BCE's whole value there was bounding X2 — which is
     exactly what step 1 does properly.
  3. Then **BCE's occurrence cap becomes affordable**. It was rejected only because it timed out mod4block
     (13.2 s → TO at both 100 and 1000) — i.e. it was killed by mod4block's X2-induced dependency, not on
     its own merits. On j3037, the cap is a **2.8× win (993 → 354 s)**, which is most of the Tier C gap.
  So: X2 governed + BCE capped could give Tier C parity with **all three rules still ON**, satisfying the
  directive and the gate together. Each step is independently measurable and step 1 is cheap. **This is the
  first thing to try on the T3 thread — before the all-on/all-off decision, which becomes moot if it works.**
  Caveat, stated honestly: L15/L17/L18 killed four admission designs and two caps, so the prior on any
  partial-suppression scheme is poor. What is different here is that the two caps were each measured under
  the other rule's runaway; that confound is real and removable, which no previous attempt could say.

**NOT DONE / OWED.** (a) The four Tier B failures — nothing started. (b) Track R: R5 S-c (Sweep candidate
selection; its DoD MUST include fuzz legs on both proof formats, review finding R1) then S-d…S-f, R6–R8 —
and R6 CCE/R7 Condition are rules **cadical ships OFF**, a class decision worth taking once. (c) TB4b.
(d) The LRAT-vs-DRAT bench-runner question. (e) §8.1 still describes Tier C on the M3 clock — same defect
rev 74 just fixed for Tier B; Tier C's `ref_seconds` are M3 seconds.

**NIGHTLY: GREEN, both legs, nothing for you to check.** `--random-policy --nightly` on the
debug/sanitizer build, 50 cores each: **DRAT 200,000/200,000 cases 0 failures** (job 1574586, SAT 98,470 /
UNSAT 66,173 / TIMEOUT 35,357) and **native LRAT 200,000/200,000 cases 0 failures** (job 1574587, SAT 98,412
/ UNSAT 66,086 / TIMEOUT 35,502); `tools/fuzz/failures/` gained nothing. So **66,173 DRAT proofs verified by
drat-trim and 66,086 native-LRAT proofs verified by lrat-check, every one under an arbitrary legal rule
schedule** — the strongest N2 evidence we take, and it covers today's two kernel-touching changes (the
`c timing` dispatch wrapper and `Kernel::bce_occ_limit`). Logs committed as evidence:
`tools/fuzz/slurm-nightly-rev74-{drat,lrat}-*.log`.

**NEXT ACTIONS (expected value order).**

1. **THE SLOT RATE IS SETTLED — 2‰ REJECTED as a constant, do not re-run it** (two Tier C windows, `20260729-094225` and `-112753`: 1.065× then
   1.101× cadical, saving 26.1 then 8.8 PAR-2; Tier B 9.10 → 9.53). It stays the `TACTIX_SLOT_PERMILLE` §5.6 arm. **What it left behind is the
   real asset:** a rate change is a *reproducible per-instance* trade — 28/30 same-sign across both windows, fixing j3037/asconhash and breaking
   ContextModel/em_11/frb45/Ptn-7824 by ~300/300/170/140 s every time. That is the sixth §5.6 dosing family and the cleanest labelled signal any of
   them has. **Do NOT hand-build a static separator on it** — L5 and L15 killed separators that looked this clean. Log 2026-07-29 has the table.
   **So T3's CaDiCaL half is open again, and the next move on it is a different lever than the rate** — the two-instance framing is superseded
   (the rate trade shows the regression is not localizable to j3037+asconhash without paying it back elsewhere).
   **▶ WHERE THE DEBUG-PERFORMANCE THREAD ACTUALLY IS (2026-07-29 evening).** j3037 resolved: the cause is
   **X1 BCE** (Log below + L17/L18), and **BCE-off gives Tier C 318.18 = 1.0065× cadical — T3's CaDiCaL half
   MET** — at the price of ONE Tier B instance, `mod4block` (13.2 s → 600 TO). So the live question is
   narrower than "fix Tier C": **why does mod4block depend on BCE when kissat does it in 18.3 s with no BCE
   at all?** If that dependency is a tactix defect rather than a real need, BCE-off is free on Tier B and the
   gate half comes with it. Probe series `benchmarks/probes/mod4block-bce-dependency.sh`:
   - **Factor-creates-it: REFUTED.** `c factor 0 factorings` on mod4block — our density gate
     (`factor.cpp:206`, threshold 6) suppresses Factor entirely there because the instance is binary-SPARSE
     (~4.1 binaries/var); `no-factor` is bit-identical to shipped (418,796 conflicts both).
   - **▶ CURRENT HYPOTHESIS, inverted from that: Factor is ABSENT, not guilty.** kissat **factors 912
     variables** on mod4block and needs no BCE; we factor none and need BCE. So BCE may be compensating for
     the BVA our own density gate suppresses — i.e. the defect would be the GATE, not BCE. In flight: arms
     forcing `TACTIX_FACTOR_DENSITY=1/0` with BCE off (jobs 1573453/1573454), plus −BCE−BVE / −BCE−Factor /
     −BCE−BVE−Factor (1573448–52) to rule out the elimination interaction.
   - Method note: the first submission of these arms was INVALID — the pinned binary predated the
     occurrence-cap revert, so every arm silently ran a configuration already measured as a mod4block
     timeout. Re-pinned and **verified against a known baseline (12.79 s / 418,796 conflicts / 6,343
     deleted) before trusting any arm.** Do that whenever a probe runs off a pinned copy.

   **(superseded framing kept for the trail) — debug-performance on `j3037_9_rggt_b`, the correct single target.** Ranked by gap to cadical at the shipped default
   (window `20260729-094225`), it is **1162.8 s vs cadical 25.8 (45×) and kissat 161.7 (7.2×) — alone +1137 s = 37.9 of the 47.2 PAR-2 total gap,
   ~80 % of the whole deficit**; the next four (asconhash +489, T87 +399, T105 +384, goldcrest +369) together are less. It is a SAT instance, and
   rules-off it is 138.8 s, so the ON composition costs it ~1000 s. Probes 1573169–77 (`benchmarks/probes/j3037-decompose.sh`) run
   shipped / −BCE / −X2 / −X4 / −all-three / cadical / kissat / cadical−sweep / kissat−sweep in one window: which rule costs it, and whether
   sweep (V5/R5, the one big pass we lack) explains the baselines' advantage. NOTE `ContextModel`/`em_11` are NOT targets — they are fast at the
   shipped default (127 s / 141 s) and only break under the rate cut.
2. **Track R next: S-c** (Sweep candidate selection + environment construction). Definition of done MUST include fuzz legs on both proof formats
   — review finding R1: the sub-solver is exhaustively unit-tested but never fuzzed, and S-c makes it reachable. Then S-d…S-f, R6–R8, each
   landing ON.
3. **TB4b** — the only open Track T item (remaining chrono divergences; detail + the live watch-repair constraint in the Track T checklist).
4. **Decide whether the bench runner should verify LRAT, not DRAT.** `urq45` settled it (probe 1566825): DRAT + drat-trim > 2400 s; native LRAT +
   cake_lpr **77 s, VERIFIED**. Our DRAT-checked tier numbers are hostage to drat-trim's backward search. Route through revise-plan (§2/§8.1 name
   drat-trim explicitly) — a measurement-protocol change, not a solver one.

**CLOSED — do not re-open** (each left a standing lesson): the walk-dose "every lever is two-sided" reading (artefact of a wrong `bumpreasons`
rate, L12); the `bumpreasons` constant sweep (rate 20 shipped, 30 the documented Tier-B-parity alternative); **clause quality** (parity with
kissat at every stage of clause construction, L14); the minimize depth cap (byte-identical across 900/1000/100000).

**STANDING DIRECTIVES + SHIPPED CONFIG.** *Every implemented proof rule ships ON* (user, 2026-07-28: "all implemented proof rules need to be
turned on!!"): 13 of 13 Inprocess rules scheduled, hardcoded ships-OFF exclusions gone (`3fb85bd`); a rule-off arm is a diagnostic for a
SCHEDULING defect, never a shipped default ([[no-rule-off-configs]]). **Scope word: *proof rule*** — S7 `Reorder` is a policy op, NOT covered;
ask before flipping it. Shipped default = the **S4 arrangement** + density-gated BVE doses + the closure-delay ladder + **S6 Walk at
`WALK_PERMILLE` 560** (`0c52fdd` set 640‰, `61138b8` retuned to 560) + reason-side bumping and dynamic tiers (ONE mechanism —
`implementation-notes.md` §9) + `bumpreasons` rate 20 + every implemented proof rule. Reports: `2026-07-25-s4-gate-mp1.md`,
`2026-07-27-tier-c-push.md`, `2026-07-28-all-rules-on.md`. Dosing knobs that are §5.6 arms, not settled constants: `TACTIX_WALK_PERMILLE`
(**shipped 560**; the older "200 balanced / 640 Tier C / 400 Tier B" readings were `bumpreasons`-rate ARTEFACTS, superseded by W4f's re-sweep),
`TACTIX_BUMP_REASONS_RATE` (30 buys Tier B parity for ~49 Tier C), `TACTIX_SLOT_PERMILLE`.

**CLUSTER PROTOCOL.** Plan §8.1 + slurm-jobs skill (`run.py --tier X --slurm`, fuzz via `tools/fuzz/slurm.sh`, sbatch-only). M3-Pro numbers are
INVALID — never compare cluster ↔ M3-Pro (revs 64/65).

**§5.6 phase-6 dosing exhibit — standing, not now.** SEVEN mechanism families: BVE admission (`2026-07-25-crypto1-bve-mechanism.md` /
`-bve-dosing-arms.md`; oracle 489.27 + labeled instances incl. s055), TR dosing (Log 2026-07-25 late), mode schedule (TB6), WALK DOSE (the
2026-07-27 sweeps — first family whose shipped constant is measurably wrong for half the corpus), S7 Reorder (first whose context is the VERDICT,
not a formula/progress feature — Log 2026-07-28), and **the inprocessing SLOT RATE** (`TACTIX_SLOT_PERMILLE`; 28/30 instances carry a same-sign,
near-same-magnitude effect across two windows, yet no single rate wins the tier — Log 2026-07-29), and **X1 BCE ADMISSION** — the cleanest of the
seven: its whole Tier B value is mod4block (+586 s without BCE) and its whole Tier C cost is j3037 (+830 s with it), and on/off, round caps, dose,
rate AND cadical's occurrence cap have each been measured to trade one for the other (L18; Log 2026-07-29).
O16 auto-invention decided at phase-4 entry.

## ▶ CURRENT TIER TABLES (plan §8.1 rev 75 — full per-instance, newest window first)

**Maintenance contract:** every tier run that changes the shipped default, or that a Log entry cites as a
result, replaces the table below. Keep the newest window at the top; drop superseded ones rather than
stacking them. Record the window stamp AND the git SHA — a table without them cannot be reproduced or
compared.

#### Tier B (28) — window `20260731-021612` (git `2a40126`), **new default `factor_size=5`**, every arm writing + checking DRAT

`●` marks the frozen **core14**; the pre-2026-07-29 archive (7.94 / 9.27 / 9.46 …) is computed on
those alone. **Never mix 14- and 28-instance PAR-2.**

| # | instance | c14 | st | tactix | kissat | ×kis |
|---|---|---|---|---|---|---|
| 1 | `sted5_0x24204-50.cnf.xz` |  | UNK | 600.0 **TO** | 8.6 | 69.7 |
| 2 | `veer_axi_yosyshq_appnote_123_veer_axi-p2` |  | UNK | 600.0 **TO** | 3.7 | 161.9 |
| 3 | `or_randxor_k3_n540_m540.cnf.xz` |  | UNK | 600.0 **TO** | 12.9 | 46.5 |
| 4 | `unsat-set-b-fclqcolor-10-07-09.sat05-128` |  | UNK | 600.0 **TO** | 16.8 | 35.7 |
| 5 | `DivS_862_11.cnf.sanitized.cnf.xz` |  | SAT | 129.0 | 19.3 | 6.7 |
| 6 | `mp1-klieber2017s-0500-023-t12.cnf.xz` |  | SAT | 110.8 | 12.9 | 8.6 |
| 7 | `PRP_30_38.cnf.xz` |  | SAT | 47.9 | 16.5 | 2.9 |
| 8 | `2013113162201nw.shuffled-as.sat03-1668.c` |  | SAT | 43.7 | 6.0 | 7.3 |
| 9 | `b22_1.cnf.xz` |  | UNS | 35.4 | 9.0 | 3.9 |
| 10 | `FmlaEquivChain_4_6_6.sanitized.cnf.xz` | ● | UNS | 32.4 | 13.7 | 2.4 |
| 11 | `circuit_48in64out_with_800gates_4in4out_` | ● | SAT | 15.4 | 11.2 | 1.4 |
| 12 | `9vliw_bp_mc.shuffled.cnf.xz` | ● | UNS | 14.7 | 8.8 | 1.7 |
| 13 | `mod4block_2vars_10gates_u2_autoenc-sc200` | ● | SAT | 13.8 | 20.4 | 0.7 |
| 14 | `rbsat-v760c43649gyes10.cnf.xz` | ● | SAT | 13.0 | 6.0 | 2.2 |
| 15 | `mp1-blockpuzzle_5x10_s7_free4.cnf.xz` | ● | UNS | 10.8 | 9.3 | 1.2 |
| 16 | `summle_X4044_steps7_I1-2-2-4-4-8-25-100.` | ● | SAT | 10.0 | 6.7 | 1.5 |
| 17 | `pyhala-braun-sat-35-4-04.shuffled.cnf.xz` |  | SAT | 9.9 | 4.1 | 2.4 |
| 18 | `SCPC-500-5.cnf.xz` |  | UNS | 9.5 | 11.6 | 0.8 |
| 19 | `hwmcc17miters-xits-iso-oski15a08b00s.san` | ● | UNS | 8.5 | 4.0 | 2.1 |
| 20 | `DecompressReader_read_12.cnf.xz` |  | UNS | 8.4 | 6.7 | 1.3 |
| 21 | `x9-08014.sat.sanitized.cnf.xz` | ● | UNS | 8.2 | 11.0 | 0.7 |
| 22 | `combined-crypto1-wff-seed-1-wffvars-450-` | ● | SAT | 6.8 | 4.2 | 1.6 |
| 23 | `SC25_Timetable_C_392_E_45_Cl_25_D_7_T_50` |  | SAT | 6.7 | 11.5 | 0.6 |
| 24 | `qwh.50.1250.shuffled-as.sat03-1655.cnf.x` | ● | SAT | 4.6 | 11.9 | 0.4 |
| 25 | `square.2.0.i.smt2-cvc4.cnf.xz` | ● | UNS | 4.5 | 3.9 | 1.1 |
| 26 | `oddball_13_5_ttf.normalised.cnf.xz` |  | UNS | 3.3 | 19.2 | 0.2 |
| 27 | `battleship-14-26-sat.cnf.xz` | ● | SAT | 1.1 | 7.2 | 0.2 |
| 28 | `clique_n2_k10.sanitized.cnf.xz` | ● | UNS | 0.1 | 6.1 | 0.0 |
|  | **PAR-2 / solved** |  |  | **105.30** (24/28) | 10.12 (28/28) | **10.408** |

Same window, other arms: old default (`factor_size=2`) **104.79** (24/28) — the new default is +0.50, inside
noise; **`s5union` (scope-fraction admission, NOT shipped) 65.86 (26/28)** — it solves `sted5` and
`or_randxor`, and is the open Tier B lever (see Log 2026-07-31).

#### Tier C (30) — window `20260731-021615` (git `2a40126`), **new default `factor_size=5`**, every arm writing + checking DRAT

| # | instance | st | tactix | cadical | ×cad |
|---|---|---|---|---|---|
| 1 | `128_125.sanitized.cnf.xz` | UNK | 2400.0 **TO** | 2400.0 | 1.0 |
| 2 | `T87.2.0.cnf.xz` | UNS | 842.3 | 407.0 | 2.1 |
| 3 | `asconhashv12_opt64_H4_M2-LOD9_m0_2_U19.c` | UNS | 749.9 | 269.0 | 2.8 |
| 4 | `T105.2.0.cnf.xz` | UNS | 659.7 | 276.1 | 2.4 |
| 5 | `goldcrest-and-9.cnf.xz` | UNS | 550.2 | 174.2 | 3.2 |
| 6 | `constraints_25_4_5_12_12_0_0_0.sanitized` | UNS | 422.5 | 201.6 | 2.1 |
| 7 | `Grain_no_init_ver1_out200_known_last105_` | UNS | 382.9 | 265.5 | 1.4 |
| 8 | `minxor128.cnf.xz` | UNS | 375.5 | 291.1 | 1.3 |
| 9 | `sat-bench-trig-bhaskara.cnf.xz` | UNS | 329.8 | 420.2 | 0.8 |
| 10 | `j3037_9_rggt_b.cnf.xz` | SAT | 312.3 | 26.1 | 12.0 |
| 11 | `urq45.cnf.xz` | UNS | 252.7 | 994.2 | 0.3 |
| 12 | `StConn_8_32.sanitized.cnf.xz` | UNS | 247.3 | 948.6 | 0.3 |
| 13 | `constraints_18_0.3_2.sanitized.cnf.xz` | UNS | 244.7 | 254.2 | 1.0 |
| 14 | `mod2c-rand3bip-sat-250-2.shuffled-as.sat` | SAT | 243.7 | 185.5 | 1.3 |
| 15 | `circuit_32in32out_with_500gates_6in6out_` | SAT | 230.2 | 490.7 | 0.5 |
| 16 | `ecarev-110-4099-22-30-7.cnf.xz` | SAT | 214.5 | 191.6 | 1.1 |
| 17 | `ER_400_20_4.apx_2_DC-ST.cnf.xz` | UNS | 208.9 | 173.1 | 1.2 |
| 18 | `php17-mixed-35percent-blocked.cnf.xz` | UNS | 189.9 | 255.7 | 0.7 |
| 19 | `SCPC-1000-18.cnf.xz` | SAT | 176.7 | 26.8 | 6.6 |
| 20 | `EDP3-11000.cnf.xz` | SAT | 137.4 | 222.0 | 0.6 |
| 21 | `em_11_3_4_cmp.cnf.xz` | SAT | 102.6 | 121.1 | 0.8 |
| 22 | `frb45-21-2.used-as.sat04-884.cnf.xz` | SAT | 87.6 | 164.9 | 0.5 |
| 23 | `aes_decry_2_rounds.debugged.cnf.xz` | UNS | 83.2 | 73.8 | 1.1 |
| 24 | `jgiraldezlevy.2200.9086.08.40.41.cnf.xz` | SAT | 79.0 | 16.8 | 4.7 |
| 25 | `Ptn-7824-b18.cnf.xz` | SAT | 78.1 | 10.4 | 7.5 |
| 26 | `DLTM_twitter690_74_16.cnf.xz` | SAT | 75.0 | 276.5 | 0.3 |
| 27 | `mp1-Nb5T15.cnf.xz` | SAT | 67.5 | 129.5 | 0.5 |
| 28 | `1-ET-256-K-70.sanitized.cnf.xz` | UNS | 40.3 | 25.8 | 1.6 |
| 29 | `af-synthesis_stb_50_120_4_sat.cnf.xz` | SAT | 35.0 | 143.0 | 0.2 |
| 30 | `ContextModel_output_7_3_9.bul_.dimacs.cn` | SAT | 20.4 | 64.2 | 0.3 |
|  | **PAR-2 / solved** |  | **327.99** (29/30) | 316.64 (29/30) | **1.036** |

Same window, other arms: old default (`factor_size=2`) **337.50** (1.066× cadical) — the new default is
**−9.51 PAR-2, 1.036×**; `s5union` **357.64 (1.129×, OUTSIDE T3's 10 % band)** — which is why the Tier B
lever is not shipped. `128_125` is unsolved for **every** arm including cadical by **OOM at 8 GB**, not
timeout; it scores 2×TO identically everywhere.

**Noise floor, measured (window `20260731-013523`):** three **bit-identical** arms (a payoff-bound sweep
that turned out to be inert — Log) scored **346.16 / 356.95 / 353.99**, i.e. **10.8 PAR-2 of aggregate
spread between identical configs**, and a per-instance range averaging **24.7 s** (goldcrest alone
**245 s = 72 %**). Read every Tier C delta below that bar as noise, and prefer per-instance
evidence with its own replicate range over an aggregate difference.

**Older windows** (aggregates only — the per-instance detail is in each run's `results.json`):
`20260731-013514-B` 105.30 (24/28) · `20260730-142436-B` 105.40 (24/28) · `20260730-111850-B` 105.21
(24/28) · `20260730-085538-B` 103.24 (24/28) · `20260729-220951-B` 102.28 (24/28), the first run on the
expanded list. Tier C: `20260731-013523` 331.60 (29/30) vs cadical 316.25 · `20260730-142439` 323.67
(29/30) vs 313.18 · `20260730-111853` 324.24 (29/30) · `20260730-085509` 319.19 (29/30) vs cadical 317.04
· `20260729-094225` 361.89 (28/30) at X2 20‰, the pre-fix baseline. **The shipped Tier C ratio moves
1.007–1.066× across windows at an unchanged default — T3's CaDiCaL half is met, but its margin is
window-dependent, so quote a window, never a single best number.** **Pre-2026-07-29 Tier B numbers (7.94 / 9.27 / 9.46, kissat 8.51) are `core14` only.**

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
- **L16 · A per-instance-REPRODUCIBLE effect can still give a non-reproducible tier aggregate — and an aggregate that matches its prediction
  can be a coincidence of sums.** The slot-rate-2‰ arm saved 26.1 PAR-2 in one Tier C window (1.065× cadical, gate met) and 8.8 in the next
  (1.101×, gate missed), while **28 of 30 instances kept the same sign and near-identical magnitude** — the swing came from a few 500–1100 s
  instances whose absolute times move 50–150 s between windows, not from instability in the mechanism. Also: the 26.1 matched the probe's
  predicted 26 while the probe's premise ("only the two target instances move") was false — ±1,100 s of offsetting per-instance moves summed
  to the right number. So (a) a tier PAR-2 near a gate boundary needs a second window before it is a verdict, and (b) agreement between a
  prediction and an aggregate is not evidence for the prediction's mechanism — check the decomposition it claimed. Converse of L12: there the
  aggregate hid per-instance chaos; here the aggregate hid per-instance ORDER. (Windows `20260729-094225` / `-112753`; Log 2026-07-29.)

## Open items (small, none blocking)

- **Task #39** — `--random-policy` livelock on 5 tiny cases in `tools/fuzz/known-hangs/` (N1 robustness, not soundness).
- demo3 is a SEARCH-only mirror by permanent design (stat contract = §8.4 within-noise; the deferred full-slot-mirror enhancement was dropped
  2026-07-25, user decision).
- Stress-pass ledger: V6 unit-channel DONE (`4f2b53e` + congstress nightly); Factor-mid-stable DONE (2026-07-25 arrangement mid-stable legs,
  9k/0). Post-Track-H: random-policy ASan 8000/0 both formats.
- Routed by the 2026-07-29 review (`benchmarks/reports/2026-07-29-review-post-track-r.md`): R2 Reorder has no stress policy; R3
  `benchmarks/probes/` needs a retention rule; R4/R5 small duplication + allocation watch items. (R1 → resume action 2.)

## Non-negotiables status (plan §1.1)

| | Requirement | Status |
|---|---|---|
| N1 | Any applicable rule appliable via API at any time, incl. branching choice | ◐ v1 API frozen (revs 20–21); §8.4 demos 1–3 prove it end-to-end; audits upheld. Two leaks found+fixed (rev 34 payload aliasing; rev 40 off-fixpoint Decide via `trail_tainted`). Final acceptance = CCE plugin (phase 4) |
| N2 | Proof production on every path (DRAT/LRAT) | ◐ DRAT since phase 1; native LRAT 2026-07-14; every fuzz UNSAT checker-verified on all formats under random schedules; audit UPHELD |
| N3 | DefaultPolicy comparable to Kissat (T4 gate) | ◐ T1+T2 passed. **2026-07-30, on the NEW 28-instance Tier B: 102.28 vs kissat 9.54, 24/28 — four instances at 36–187× (resume block). This is the honest N3 picture; the old 14-instance 1.08× was measured on a set omitting them.** Tier B core14 9.46 vs 8.84; Tier C 1.15–1.28× cadical, 1.57× kissat; Tier A 0.98×. Tier D (T4) is where it counts |

## Milestone gates (plan §8.3)

- [x] T1 — **passed 2026-07-14** (Tier A 200/200 PAR-2 0.48; Tier B 9/14 @ 231.6 vs MiniSat 8/14 @ 281.4; report
  `benchmarks/reports/2026-07-14-t1-gate.md`)
- [x] T2 — **passed 2026-07-14** (Tier B 11/14 @ 164.8 vs Glucose 8/14 @ 268.3; props/s geomean 0.92× kissat; report
  `benchmarks/reports/2026-07-14-t2-gate.md`)
- [ ] T3 — within 10% PAR-2 of CaDiCaL on Tier C + CaDiCaL/Kissat parity on the rule-neutral subset (rev 51). **STATUS CHANGED 2026-07-29 by the
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
  **LIVE CONSTRAINT — the list is not six independent items: (A) and watch repair are ONE change.** Reusing a conflict clause as a reason
  requires it to watch the literal it now implies, or a later backtrack strands it unit at a claimed fixpoint (the unrepaired first version
  aborted corpus 002 + 005, "unit large clause escaped propagation"). kissat repairs watches in the same function (`analyze.c`, the
  `conflict_size > 2` swap block); **we have no unwatch primitive**, so the shortcut currently DECLINES when the invariant does not already hold
  — sound, still fires on the common propagation-conflict shape. **Next step = the watch-repair primitive**, which also unblocks the OTFS-round
  case (the shortcut is restricted to the first analysis round because an OTFS-strengthened conflict's clause identity is not pinned down yet).
  - [x] **TB4b(A) · the one-literal-on-conflict-level shortcut IMPLEMENTED, ships OFF** — when exactly one conflict literal sits at the conflict
    level the clause is already asserting, so backtrack to the second-highest level and assign that literal WITH THE CONFLICT AS ITS REASON,
    learning nothing (fewer learned clauses and proof lines) (`Kernel::conflict_reuse`, `TACTIX_CONFLICT_REUSE=1`;
    `1c5f2c5`, measured `88cff2d`). Negative tier-wide — Tier B 8.32 vs 7.62, Tier C 330.17/310.93 vs 312.17/307.07 over two windows, DIRECTION
    only (Tier C noise at throttle 48 is ~6 % — the SAME arm read 330.17 and 310.93). Wins where aimed (FmlaEquiv 26.00 → 22.45 s, −29 %
    conflicts; 9vliw 13.28 → 11.67; crypto1 2.20 → 1.03), loses more on qwh alone (4.04 → 15.71) than the whole aggregate delta. **The verdict is
    on the CRIPPLED version** (parent's live constraint), not on kissat's
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
  MUST include sub-solver fuzz legs on both proof formats (review finding R1) — then S-d…S-f
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
  Reorder and BCE landed in phase 3 Track R, and Sweep is in progress there (R5)*; **decide O16b at phase entry** (PR/SR clause learning as a
  catalog candidate — the checkable form of auto-invented rules; needs dpr-trim or PR→DRAT in the N2 story)
- [ ] Independent-lineage candidates: CMS best_inv rephase (with Walk), rotation presets (O6)
- [ ] Python bindings; policy presets (MAB scheduler, classification) — §5.6 seed
- [ ] Demo policies 4–6 incl. external branching (N1 end-to-end)
- [ ] §8.4 overhead budget: default vs hardcoded twin ≤ 5% on Tier B
- [ ] **Extensibility acceptance test: CCE (X3) as pure plugin (§5.3)**
- **Exit:** T4 (⇒ N3 satisfied)

## Phase 5 — competition readiness

- [ ] Backlog rules as warranted (X2, X4, X5) — *X2/X4 landed in phase 3 Track R; X5 Condition is R7*
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

### 2026-07-31 — ▶ `factor_size = 5` SHIPPED (Tier C −9.51, 1.036× cadical); the Tier B lever is held back and WHY
Report: `benchmarks/reports/2026-07-31-factor-scope-and-admission.md`. The 2026-07-30 union arm bundled TWO
changes; they are separable and pull opposite ways. **Scope (`factor_size` 2→5) ships:** Tier C 337.50 →
**327.99** (1.066× → **1.036×** cadical), Tier B 104.79 → 105.30 (+0.50, noise), no solve-count change, all
four pre-registered KILL instances clean (x9 +0.8, mp1-blockpuzzle −0.0, FmlaEquiv −0.6, mod4block +0.4).
Fuzz on the new default **4 × 10,000 cases, 0 failures** (DRAT + native LRAT, each ± `--random-policy`);
ctest 38/38. **Admission (`factor_scope_percent`) does NOT ship:** it is Tier B **65.86 (26/28)** — solving
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

### 2026-07-31 — the Factor admission trade DECOMPOSED: damage not cost, one phantom, and TWO dead separators
Union admission (`TACTIX_FACTOR_SIZE=5 TACTIX_FACTOR_SCOPE_PCT=50`) is **Tier B +41.7 PAR-2 / +2 solves**
(104.97 → 63.24, 24 → 26; replicated 105.30 → 64.27 next window) and **Tier C −21.4** (322.54 → 343.97,
1.013× → 1.080× cadical, still inside T3's 10 % band). Pre-registered criterion said "B improves AND C does
not regress", so it is **NOT shipped by default**; everything below is why, and what the night eliminated.
- **The two changes are separable and pull opposite ways.** By admission class: instances Factor ALREADY ran
  on (density gate) net **−173** from size 2→5, while NEWLY scope-admitted instances net **+1078**. All the
  damage is the gate. But **100 % of Tier B's gain is also in the newly-admitted class** (its density class is
  exactly +0.0 — no Tier B instance passes the density gate at all), so `factor_size=5` alone cannot merge
  Tier B. The tiers pull on the same lever in opposite directions.
- **Mechanism: DAMAGE, not cost.** Conflicts explode where Factor is newly admitted — ContextModel 335 k →
  2.83 M (8.4×), jgiraldez 3.9×, mp1-Nb5T15 2.4× — while the winners win on search too (sted5 **14× fewer**
  conflicts, 3.78 M → 271 k). Factor's own time is not the lever; the factored formula is what is harder.
- **One "regression" is a PHANTOM.** goldcrest-and-9 (+129.8 in the table) runs **0 factorings and 272,082
  conflicts in BOTH arms** — a bit-identical search — and the probe reproduces 430 vs 431 s. ≈ 130 s of the
  +642.7 Tier C total is measurement variance, not effect. Always check `c factor` before believing a delta.
- **Dead separator 1 — scope threshold.** Winners and losers sit at the SAME scope fraction (j3037 100.0 %
  −99.7 vs ContextModel 99.8 % +400); raising 50 → 90 % cannot separate them. L15 again.
- **Dead separator 2 — payoff bound.** Accepted-gain histograms straddle: winners span mean 1.4 (edp3 — 1686
  of 2717 factorings at δ=1, its whole benefit) to 13.5 (sted5), losers sit at 4.2/4.8 between them. Measured:
  sted5 19 s → 274 s (b8) → 118 s (b16). The bound costs Tier B and cannot buy Tier C.
- **A real N1 bug found by that sweep:** `Kernel::factor_bound` was **unreachable under DefaultPolicy** — the
  rule accepts via `fm ? fm->accept(…) : delta > k.factor_bound` and DefaultPolicy ALWAYS attaches
  `DefaultFactorMatch`, whose `accept` was hardcoded `delta >= 1`. A documented config surface no shipped path
  could exercise; the sweep returned byte-identical proofs, which is how it surfaced. Fixed (bound copied in at
  the attach site; `bound = 0` reproduces `delta >= 1` exactly).
- **New arm, mixed — banked not shipped:** `TACTIX_FACTOR_DEMOTE` repairs a genuine FOCUSED-mode defect —
  `heuristics_reserve_vars` appends fresh vars at `queue_last`, which is where VMTF's decision search STARTS,
  so every variable Factor adds became a most-preferred branching candidate (kissat's
  `adjust_scores_and_phases_of_fresh_varaibles` demotes them; our stable-mode heap was already correct at
  score 0.0). It rescues j3037 207 → 88 s, edp3 74 → 39 s, rxor 49 → 36 s — but costs **sted5 16 → 150 s**,
  one of the two Tier B rescues. Non-separable; default OFF pending its own tier pair.
- Evidence: `benchmarks/probes/fcd-*`, `gridtrace-*`, `demote-*`, `fb-bite-*`; runs `20260731-013514-B-boundsweep`,
  `20260731-013523-C-boundsweep`. Default identity re-verified on mod4block (**418,796** conflicts) for every
  binary built tonight; ctest 38/38.

### 2026-07-30 — R5 increment 2: the widened Factor grid, and **or_randxor (the one true wall) SOLVES**
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

### 2026-07-30 — R1 and R2 both KILLED at tier level: for this rule family the lever is ADMISSION, not dosing
Both top leads from the Tier B timeout diagnosis died on their pre-registered kill criteria; shipped
defaults unchanged throughout, every arm retained as a §5.6 knob.
- **R2** (remove the BVE binary-density gate, `TACTIX_BVE_DOSE_DENSITY=0`): Tier B(28) flat 105.21 → 105.84
  but **Tier C 324.24 → 516.91 with THREE lost solves** (T105/T87/asconhash). Residue worth keeping:
  **`crypto1` IMPROVED** without the gate (7.0 → 3.3), so half of L9's justification is refuted and on
  Tier B it is a ONE-INSTANCE (mod4block) defense — while on Tier C it defends a real and different set.
  The flat Tier B aggregate hid a one-for-one swap (gained `sted5` 600 TO → 2.4 s, klieber 118.4 → 6.5;
  lost mod4block 13.5 → 600 TO). Cost-based admission calibrated too: mod4block is a clean outlier at
  **109,573 resolutions/elimination** vs 33–7,997 for the ENABLE set, but **asconhash needs suppressing at
  151** while eliminating 42,570 vars cheaply — L2/L7 trajectory damage, which cost-per-yield cannot see
  by construction. So cost is a better signal for one class and does NOT subsume density.
- **R1** (meter V4 in ticks + kissat's early exit): Tier B(28) **105.40 → 123.29, a lost solve**; Tier C
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
  and costs fclq 12.6×, the same as `--no-factor`. → R5 above.
- `veer_axi` is a CONSTANT FACTOR, not a wall: **2,199.8 s vs kissat 3.2 (687×)**, TransitiveReduce alone
  585.7 s of it where kissat spends 0.03 s. **`or_randxor` was the only true wall** (42.5 M conflicts
  unsolved at 3,600 s, props/conflict 0.99× — now solved by R5).
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
**It does NOT fix the observed case and the code says so:** lag is 40 s before and 41 s after, because the
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
(1572349). Routed, not fixed (report §"Routed"): **R1 (med) — S-c MUST add sub-solver fuzz legs when it becomes reachable**; R2 Reorder has no stress policy; R3
`benchmarks/probes/` needs a retention rule; R4/R5 duplication + allocation.

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
