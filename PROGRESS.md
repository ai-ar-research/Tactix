# Progress

**Tactix — a proof-rule-driven SAT solver.** Every simplification is an explicit proof rule that a policy
schedules through a public API, so an LLM/RL policy can drive the search later; scope and architecture are
in `DEVELOPMENT_PLAN.md` (authoritative), the abstract rules in `docs/calculus.md`, the implemented guards
in `docs/implementation-notes.md`. The four non-negotiables (§1.1) are **N1** any applicable rule appliable
via the API at any time · **N2** a DRAT/LRAT certificate on every path · **N3** DefaultPolicy performance
comparable to Kissat · **N4** DefaultPolicy's *scheduling* at least as sophisticated as Kissat's strategy.
Their live status is the table below.

Living status. **Sync contract:** update this file in the same commit as (a) completing/starting any checklist item, (b) any DEVELOPMENT_PLAN.md
revision (via revise-plan). If this file and the plan disagree, the plan is authoritative — fix this file.

- **Plan revision tracked:** rev. 82 (2026-08-04) — rev 80 made **N4, the strategy floor**, a non-negotiable; rev 81 renamed the §5.6 RL stages `L1`–`L4` → `RL1`–`RL4`
- **Current phase:** 3 (inprocessing) — since 2026-07-14 (phase 2 closed at T2 the same day, report `benchmarks/reports/2026-07-14-t2-gate.md`)
- **Next gate:** **T3** (§8.3) — NOT met on either half and both regressed on 2026-08-05; see the gate row and the tier tables.
- **Last updated:** 2026-08-05 late — compaction pass (2182 → this). Status lives in the anchor below; completed work is compressed to
  result + pointer, with the detail in the cited commit / report / design doc.

## ▶ RESUME HERE (2026-08-06) — phase 3 · **next = re-run Tier B + Tier C on the LEDGER FIX (branch `debug-solve-losses`)**

**▶▶ THE FIVE OVERNIGHT SOLVE LOSSES ARE ONE DEFECT, AND IT IS FIXED (branch `debug-solve-losses`,
`7b9f9d2` + `8b27c63`). THE EFFORT LEDGER WAS WRAPPING.** `set_entry_limit` sizes a dose as the
UNSIGNED delta `search_ticks(k) - e.anchor`; the anchor is written INSIDE a phase and `search_ticks`
was `k.stats.ticks - inprocess.ticks` with `inprocess.ticks` credited only at phase EXIT — so the
reading inflated during a phase and snapped back at its end, i.e. **it was not monotone**. An anchor
from late in one phase could exceed the reading early in the next, and the subtraction wrapped. A wrap
does not enlarge the dose, it **deletes** it: `delta ≈ 2^64/10 = 1.845e18`, and the two `budget_aware`
closures that read `Kernel::work_budget` — **Sweep and Congruence** — ran to exhaustion. Measured on
`PRP_30_38`: Sweep `handed` **5 534 023 222 146 936 200 = exactly 3 × 2^64/10**, spending 412 G ticks
across 260 570 environments and 73 % of the solve; with the fix, `handed` 60 854 300, 54 environments,
`wrapped 0`, and the instance solves. **SCHED-D2 was the TRIGGER, not the defect** — 2.4× the probing
occasions halves the search window between phases, which is precisely the condition for `now < anchor`.
D2 stays (it is N4, both references split the clock). Fix = monotone `search_ticks` + saturating
subtraction at all four ledger sites + a `c slot … wrapped N` reachability counter.
**Twelve-instance probe (job `1585610`, node018, both halves in ONE window): loss-set PAR-2
8400 → 1944, win-set 2462 → 2285 (the control half is slightly BETTER), solved 7/12 → 11/12.** So the
recovery is not paid for out of the wins. Two unrecovered/unexplained, stated as such: `mod4block` is
TO on the `ea15617` binary too (the may-not-gate lottery, no conclusion), and `em_11`'s SOLVE returns
but at 585.7 s vs a 38.1 s `before` draw — a trajectory residual the wrap does not own, exactly as the
Sweep-governor report predicted for it. ctest **46/46**; fuzz DRAT / native-LRAT / `--random-policy`
10 000 cases each, **0 failures** (`1585685`/`1585686`/`1585687`). Report:
`benchmarks/reports/2026-08-06-solve-loss-diagnosis.md`.

**▶ THE EXACT NEXT STEPS, in order.** (1) **Full Tier B + Tier C on the fix** — twelve extreme
instances are not a tier, and every `Sweep`/`Congruence` dosing number taken since `58a46de` was
measured against a budget that did not bind, so the T3 reading has to be retaken. Two pin dirs,
node-pinned, `--baseline kissat --baseline cadical`. (2) **One `ledgerfix + TACTIX_SLOT_CLOCKS=1`
arm**, pre-registered in the report §5.4: one cadence on the UNFIXED tree still scored 3558 against
the fix's 4229 on those twelve, so up to ~670 PAR-2 may be D2 cost that survives a sound ledger —
nothing measured so far separates "D2's cost was entirely the wrap" from "D2 costs on top of it".
(3) Re-price Sweep's and Congruence's per-mille shares now that the ledger binds.

**▶ NOTE FOR EVERY FUTURE SLOT A/B.** `TACTIX_SLOT_CLOCKS` / `TACTIX_SLOT_WATERMARK` now exist, so the
whole Stage-D/E/F/G set is ablatable from one binary. The first cut of the clock hatch advanced BOTH
clocks after one phase and thereby starved the entire elimination family (`c bve 0 rounds`) while
"restoring" all five losses — caught only by reading `c slot-clock`'s `eliminate phases 0`, not the
wall clock. Read the telemetry before banking any arm.

## ▶ PREVIOUS ANCHOR (2026-08-05) — the Sweep-governor Tier C A/B (specced, unrun)

**NOTHING IN FLIGHT.** (Historical: the final Tier B + Tier C at HEAD `c7beaa0` — landed as windows
`20260806-084944-B-shipped` / `20260806-084947-C-shipped`; those are the numbers the anchor above
diagnoses.) Tier B array `1585269` on node017,
Tier C array `1585354` on node018 — two pin dirs from ONE commit with **byte-identical binaries**
(`219b10d7…`), both baselines carried, one tier per node so no arm/node correlation inside a tier and
the two tiers do not contend. Partition was verified idle before submitting. ctest **46/46**, corpus
14/14, all four conformance checkers green.

**▶▶ SWEEP'S DEGRADATION IS DIAGNOSED AND FIXED (branch `sweep-governor-fix`); THE TIER C A/B IS SPECCED
AND UNRUN.** Not the meter — the **enforcement point**. The round ceiling was polled only *between* seeds
and never inside `sweep_env_build`, so one seed outspent its dose **73×** (`constraints_18_0.3_2`: handed
665 M ticks, spent 48,685 M; 304.8 s of a 597 s solve, bit-identical trajectory, 99.996 % in the BFS
occurrence walk). Fix = poll the budget inside the expansion loop (cadical's discipline), 44 lines, all in
`src/rules/sweep/`. Overshoot **≤1.005× on all four** probe instances; `constraints_18` **−475.6 s**,
`bhaskara` **−65.7 s**, `em_11`/`minxor128` unchanged. **`minxor128` and — post-Stage-D — `em_11` are a
DIFFERENT mechanism** (Sweep's own wall there is ~2 s / fractions of a second and its ledger is 1.01–1.04×;
their cost is the emitted equivalences changing the search), so no governor fix touches them; still run
paired draws on `minxor128` before treating its solve flip as a verdict. **Next step: run the Tier C A/B**
— arms `govon` / `govoff` (`TACTIX_SWEEP_ENV_BUDGET=0`), pre-registered reachability + `spent/handed`
counters and ship criterion in `benchmarks/reports/2026-08-05-sweep-governor-diagnosis.md` §9. **It should
precede the SCHED-D2 pricing run**, since Stage D multiplies Sweep's occasions and would otherwise
re-measure this defect and blame the clocks.

**▶▶ THE MERGED-TREE FUZZ RUN'S 38 "LRAT" FAILURES ARE NOT OURS — HEAD IS NOT FUZZ-RED.** Job `1584461`
was **not** a native-LRAT leg: its solver argv has no `--lrat`, so it ran native **DRAT** (which
drat-trim VERIFIED) plus the harness's `drat-trim -L` → `lrat-check` **conversion cross-check**, and that
is what rejected. **kissat's and cadical's own DRAT proofs are rejected identically on the same CNFs,
byte-identical message**, while our native LRAT verifies on every one under both `lrat-check` and
`cake_lpr`. Cause: drat-trim's parser deletes duplicate literals as it reads the CNF and `-L` then cites
input clauses by id, while lrat-check re-reads the undeduped file (and, second class, an input empty
clause makes `-L` write a zero-byte proof). Both are checker-vs-checker disagreements. Fixed **in the
harness**: `conversion_crosscheck_unsound()` stands the cross-check down on exactly those two classes and
`fuzz.py` reports `oracles skipped: {...}` per leg so it can never vanish quietly; the native-LRAT oracle
is deliberately NOT gated. `selftest.sh` pins all of it — and its own `expect_clean kissat` arm was red
for the same reason until now (masked by the pre-`lrat_verified()` oracle). Report:
`benchmarks/reports/2026-08-05-drat-trim-conversion-crosscheck.md`. **Re-verified at two seeds: six legs,
6000 cases each, jobs `1584462`–`1584467`, ALL 0 failures** — including seed `20260709`, the exact
population that produced the 38, with the restatement path reached by **1321 of 1916 UNSAT answers**.
Two seeds is now the standing expectation for a proof-path change: one seed is not a population.
The skip counter also sizes what was being lost — **~1475 of 6000 cases (25 %)** were dropping the
conversion cross-check silently before this.

**Where we stand.** Tier B **70.71 (26/28)** · Tier C **419.91 (28/30)** vs kissat 228.45 / cadical 312.27
(window `20260805-115039-C-c5`, git `0418770`, node017-pinned). **T3 is NOT met on either half and BOTH
regressed in a day** — see the tier-table block, which carries the numbers and the decisive experiment.

**▶ THE EXACT NEXT STEP.** One same-window **Tier C A/B: Sweep ON (shipped) vs `TACTIX_SLOT_DISABLE=Sweep`**,
at HEAD, node-pinned, `--baseline kissat --baseline cadical`. Two pin dirs if B and C go out as concurrent
arrays (CLAUDE.md build-dir rule). **Why this and not Stage D:** Tier C went 309.05 (29/30) → 419.91 (28/30)
across 08-04 → 08-05 while kissat stayed flat (230.74 → 228.45), and the one thing that changed is V5 Sweep
going from unscheduled to scheduled at kissat's 100‰. The last Sweep ON/OFF comparison (S-f) predates the
N2 fix, so it scored a *correct* UNSAT as unsolved on a rejected proof and cannot be reused. Supporting
datapoint from the blast-radius survey: **`constraints_18_0.3_2` runs 42 Sweep rounds, emits nothing, keeps
a bit-identical trajectory, and still costs 519.3 s vs 269.8 s (1.92×)** — a governor problem, not a
correctness one. **If Sweep is the cause, the fix is its effort share, NOT switching the rule off**
([[no-rule-off-configs]]; every implemented proof rule ships ON).

**▶ STAGE D IS LANDED AND ENTIRELY UNPRICED** (branch `sched-stage-d`, three commits, ctest 43/43, fuzz
green on DRAT / native LRAT / `--random-policy`×DRAT; **STAGE F re-fuzzed the same three legs on top,
10,000 cases each, 0 failures — jobs `1584731`/`1584732`/`1584733`**). D1 authored order · D2 the two clocks · D3 the slot
gates. **D2 is the one expected to move numbers** (probing at ~2×, not the ~5× the ledger headline implies —
see its Log entry). The three A/B specs are in the Log entries and are the next Stage-D work; D3's spec
says to check its `blocked` counter first and abandon the window if it is zero.

**▶ STAGE E IS LANDED AND UNPRICED** (branch `sched-stage-e`, four commits, ctest **44/44**, fuzz green — DRAT 10,000/0, native LRAT 4,000/0,
`--random-policy`×DRAT 4,000/0, corpus 14/14). E1 candidate order + resumption as policy objects · E2 the per-rule ordering arms · **E3 the effort
ceiling** (`SP-EFFORT-CEIL-1`, taken mid-stage; ported and **measured inert** — see its Log entry before spending a window on it). The one finding
that changes the picture: **Probe was advancing 0.3 % of its candidate list per dose and replaying the same head on all 45 phases** — E1's cursor
fixes that, and it is unpriced. Three A/B specs are in the Log entries; E1's and E2's are worth windows, E3's says explicitly not to run it.
**▶ STAGE F IS LANDED AND UNPRICED** (branch `sched-stage-f`, ctest **44/44**, fuzz green ON THE MERGED
TREE — DRAT `1584769`, native LRAT `1584770`, `--random-policy`×DRAT `1584771`, **10,000 cases each, 0
failures**, restatement path reached on 2,855). F1 the
touched-epoch substrate (a per-variable EPOCH STAMP, not a bit — so it has no clear pass a scheduling decline
can starve, which is D3's `SP-SLOT-5` lesson applied) · F2 resume by RE-ARM, **and the C1→BVE bound coupling T5
asks for REFUSED** as self-wedging (§13.3 `SP-REPEAT-COUPLE`) · F3 BCE/HyperTernaryRes/Instantiate, which forced
a second stamp array for DELETIONS · F4 Factor's seed marks · F6 the redundant half, **shipped OFF** because it
only weakens the filters until F7 gives it a consumer. **Measured at the source** (probes `1584708`/`1584753`,
counters only): `rbsat` C1 partner visits 165,272 → 79,686 and C2 5.83 M → 2.57 M · `mod4block` BCE ticks
14.9 M → 7.1 M with MORE deletions · `9vliw` BCE 175,810 literals skipped; `advances` == round count everywhere,
so the filters fire and none has wedged. **On the MERGED tree WHICH consumer carries it is per-instance**
— `9vliw`/`rbsat`/`battleship` shift to Factor and BCE while `mod4block` is still C1/C2 at 782,717
skipped — which is why the family prices as one arm. Re-read the Log before attributing a delta to any
single item. **F5 is NOT done and is routed to `revamp`** — it needs a per-CLAUSE
bit and the per-variable substrate cannot express it (see its checklist entry). **F7 (C1/C2 scope ← the tier
band) is the open follow-up and F6 is its prerequisite.** The A/B specs are in the Log; `TACTIX_SUBSUME_HOLD`'s
arm says explicitly not to run it while `c subhold` reads 0.

**▶ THEN.** Stage G — the genuine revamps, starting with `SCHED-G2` (is C1 doing anything?), the cheapest
decisive check on the board.

**▶ OWED MEASUREMENTS (each a job; none blocks the next step).**
- **ALL THREE STAGE-D ITEMS are specced and unrun** — arms, pre-flight counter and ship criterion in each
  Log entry. Pre-flight counters: `c slot-order` (D1), `c slot-clock` phase counts (D2), `c slot-clock`
  `blocked` (D3).
- **`SCHED-C6` A/B is specced and unrun** — arms, pre-flight counter and ship criterion are in its Log
  entry. Pre-flight: `c eagersub` non-zero on ON, exactly `0 subsumed of 0 tried … register 0` on
  `TACTIX_EAGER_SUBSUME=0`.
- **BVE ramp slack-8 ceiling** — Tier C/D re-sweep owed since `489fab1`; knob `TACTIX_BVE_RAMP_MAX`,
  reachability probe already written at `benchmarks/probes/scratch/bveramp-reach.*`.
- **`SCHED-C8` (BVE ITE/XOR gates) A/B is specced and unrun** — arms `--bve-gates=all` vs
  `--bve-gates=equiv,andor`, two pin dirs, pre-flight counter and ship criterion in its Log entry.
  Reachability already proven: `9vliw_bp_mc` reports `306 ite, 16 xor; 6,101,504 resolvent pairs saved`.
- **`SCHED-G1` (Congruence budget) A/B is specced and unrun** — arms `budget` (shipped) vs `nobudget`
  = `TACTIX_SLOT_PERMILLE=Congruence:1000000`, optional `share20`; pre-flight = `c congruence … budget
  stops` non-zero on the shipped arm and exactly 0 on `nobudget`. Full spec in the design doc §7. **The
  exhibit `T105.2.0` is a Tier C timeout in BOTH arms** — a win shows up elsewhere; do not let the
  exhibit gate the verdict.
- **`randec` (SCHED-C7) is UNPRICED and has NO KNOB.** `randec_enabled` (`default_policy.hpp`) is
  initialised true and never assigned. One line in `main.cpp` makes it A/B-able.
- **C1/C3/C4 have nothing to price** — C1/C3 landed unconditionally, C1's only lever confounds by also
  moving reduce's tiers, and C4 is a pure refactor. Closed, do not re-open.
- **Tier D** (793, frozen) has never been RUN — a phase-3-exit prerequisite (§11 O9). `--tier D` still
  lacks a per-track flag for T4's SC25-set PAR-2; wire that first.

**▶ READ BEFORE TRUSTING ANY DELTA.** `--aggregate` prints a **fragility line** — the minimum number of
instances whose removal flips the sign. **`k=1` means the run has NOT answered the question.** On Tier C a
solve flip is worth 2400 PAR-2, so read **`#solved`**, not the aggregate. Same-window noise floor **6.72**
(idle partition); never compare absolute Tier C PAR-2 across windows. A verdict resting on `mod4block`,
`j3037`, `jgiraldezlevy`, `urq45`, `frb45` or `ContextModel` alone is not a verdict — run paired draws.
**Do not grow that set** (user directive).

**▶ TRUST NOTHING THAT RESTED ON A NATIVE-LRAT FUZZ LEG BEFORE 2026-08-05.** `fuzz.py`'s LRAT oracle tested
`"VERIF" not in output.upper()`, which matches `c NOT VERIFIED` too, so **every LRAT rejection in the
project's history was recorded as a pass**; only crashes ever failed an LRAT leg. Fixed. Any past
"native-LRAT green" claim established nothing and must be re-earned, not re-read.

## ▶ CURRENT TIER TABLES (plan §8.1 rev 75 — full per-instance, newest window first)

**Maintenance contract:** every tier run that changes the shipped default, or that a Log entry cites as a
result, replaces the table below. Keep the newest window at the top; drop superseded ones rather than
stacking them. Record the window stamp AND the git SHA — a table without them cannot be reproduced or
compared. **Take the SHA from the run's own `manifest.json`/`results.json`, never from the commit that later
shipped the change** — the 2026-08-02 tables were labelled with the ship SHA and were not reproducible.

#### Tier B (28) — window `20260805-115027-B-c5` (git `0418770`), SHIPPED DEFAULT = `c5` (demote ON); `demoff` = `TACTIX_FACTOR_DEMOTE=0`. No baseline carried in this window.

| # | instance | st | c5 | demoff |
|---|---|---|---|---|
| 1 | `or_randxor_k3_n540_m540.cnf.xz` | UNK | **TO** | **TO** |
| 2 | `unsat-set-b-fclqcolor-10-07-09.sat05-1282.re` | UNK | **TO** | **TO** |
| 3 | `mp1-klieber2017s-0500-023-t12.cnf.xz` | SAT | 239.7 | 232.6 |
| 4 | `DivS_862_11.cnf.sanitized.cnf.xz` | SAT | 132.4 | 134.7 |
| 5 | `PRP_30_38.cnf.xz` | SAT | 131.3 | 131.5 |
| 6 | `b22_1.cnf.xz` | UNS | 35.4 | 34.3 |
| 7 | `qwh.50.1250.shuffled-as.sat03-1655.cnf.xz` | SAT | 32.7 | 17.3 |
| 8 | `veer_axi_yosyshq_appnote_123_veer_axi-p23_tr` | UNS | 29.5 | 29.6 |
| 9 | `combined-crypto1-wff-seed-1-wffvars-450-cryp` | SAT | 23.5 | 23.2 |
| 10 | `summle_X4044_steps7_I1-2-2-4-4-8-25-100.cnf.` | SAT | 16.0 | 15.7 |
| 11 | `9vliw_bp_mc.shuffled.cnf.xz` | UNS | 15.4 | 12.3 |
| 12 | `FmlaEquivChain_4_6_6.sanitized.cnf.xz` | UNS | 14.5 | 14.5 |
| 13 | `circuit_48in64out_with_800gates_4in4out_dist` | SAT | 14.1 | 14.2 |
| 14 | `mod4block_2vars_10gates_u2_autoenc-sc2009.cn` | SAT | 14.0 | 13.9 |
| 15 | `x9-08014.sat.sanitized.cnf.xz` | UNS | 13.5 | 13.4 |
| 16 | `SCPC-500-5.cnf.xz` | UNS | 12.7 | 8.4 |
| 17 | `mp1-blockpuzzle_5x10_s7_free4.cnf.xz` | UNS | 12.0 | 10.2 |
| 18 | `pyhala-braun-sat-35-4-04.shuffled.cnf.xz` | SAT | 8.4 | 8.4 |
| 19 | `battleship-14-26-sat.cnf.xz` | SAT | 6.9 | 5.9 |
| 20 | `hwmcc17miters-xits-iso-oski15a08b00s.sanitiz` | UNS | 5.9 | 6.0 |
| 21 | `2013113162201nw.shuffled-as.sat03-1668.cnf.x` | SAT | 5.9 | 5.9 |
| 22 | `square.2.0.i.smt2-cvc4.cnf.xz` | UNS | 5.9 | 5.9 |
| 23 | `SC25_Timetable_C_392_E_45_Cl_25_D_7_T_50.nor` | SAT | 5.3 | 5.3 |
| 24 | `rbsat-v760c43649gyes10.cnf.xz` | SAT | 2.5 | 16.5 |
| 25 | `sted5_0x24204-50.cnf.xz` | SAT | 1.1 | 1.1 |
| 26 | `oddball_13_5_ttf.normalised.cnf.xz` | UNS | 0.9 | 1.5 |
| 27 | `DecompressReader_read_12.cnf.xz` | UNS | 0.5 | 0.5 |
| 28 | `clique_n2_k10.sanitized.cnf.xz` | UNS | 0.0 | 0.0 |
|  | **PAR-2 / solved** |  | **70.71** (26/28) | **70.10** (26/28) |

*(timeout 300s)*

#### Tier C (30) — window `20260805-205741-C-sweepon` (git `ea15617`), node017. `sweepon` = SHIPPED DEFAULT; `sweepoff` = `TACTIX_SLOT_DISABLE=Sweep`

| # | instance | st | sweepon | sweepoff | kissat | cadical |
|---|---|---|---|---|---|---|
| 1 | `128_125.sanitized.cnf.xz` | UNK | **TO** | **TO** | **TO** | **TO** |
| 2 | `minxor128.cnf.xz` | UNK | **TO** | 992.5 | 231.7 | 285.6 |
| 3 | `T87.2.0.cnf.xz` | UNS | 841.2 | 839.0 | 273.2 | 431.8 |
| 4 | `T105.2.0.cnf.xz` | UNS | 645.1 | 645.5 | 298.9 | 285.8 |
| 5 | `constraints_18_0.3_2.sanitized.cnf.xz` | UNS | 592.8 | 301.7 | 190.1 | 246.3 |
| 6 | `sat-bench-trig-bhaskara.cnf.xz` | UNS | 501.1 | 613.2 | 355.3 | 389.0 |
| 7 | `goldcrest-and-9.cnf.xz` | UNS | 420.7 | 372.3 | 100.7 | 166.5 |
| 8 | `StConn_8_32.sanitized.cnf.xz` | UNS | 393.8 | 391.1 | 294.7 | 982.2 |
| 9 | `frb45-21-2.used-as.sat04-884.cnf.xz` | SAT | 312.5 | 313.2 | 92.4 | 159.9 |
| 10 | `DLTM_twitter690_74_16.cnf.xz` | SAT | 258.3 | 250.6 | 124.9 | 250.7 |
| 11 | `j3037_9_rggt_b.cnf.xz` | SAT | 250.2 | 261.7 | 141.2 | 22.9 |
| 12 | `ecarev-110-4099-22-30-7.cnf.xz` | SAT | 247.2 | 271.7 | 224.4 | 193.5 |
| 13 | `circuit_32in32out_with_500gates_6in6out_dist` | SAT | 236.9 | 237.8 | 111.6 | 481.2 |
| 14 | `af-synthesis_stb_50_120_4_sat.cnf.xz` | SAT | 228.6 | 202.2 | 139.5 | 137.7 |
| 15 | `mp1-Nb5T15.cnf.xz` | SAT | 220.5 | 218.7 | 77.5 | 125.1 |
| 16 | `constraints_25_4_5_12_12_0_0_0.sanitized.cnf` | UNS | 203.2 | 203.0 | 134.7 | 184.2 |
| 17 | `php17-mixed-35percent-blocked.cnf.xz` | UNS | 202.6 | 202.4 | 179.2 | 247.5 |
| 18 | `Grain_no_init_ver1_out200_known_last105_0_u.` | UNS | 201.5 | 201.3 | 228.0 | 262.7 |
| 19 | `ER_400_20_4.apx_2_DC-ST.cnf.xz` | UNS | 198.5 | 198.7 | 153.8 | 174.7 |
| 20 | `asconhashv12_opt64_H4_M2-LOD9_m0_2_U19.c.cnf` | UNS | 169.1 | 170.0 | 104.2 | 254.3 |
| 21 | `urq45.cnf.xz` | UNS | 125.1 | 135.8 | 117.0 | 984.5 |
| 22 | `mod2c-rand3bip-sat-250-2.shuffled-as.sat05-2` | SAT | 109.5 | 109.5 | 40.7 | 183.9 |
| 23 | `SCPC-1000-18.cnf.xz` | SAT | 74.7 | 74.1 | 168.1 | 25.7 |
| 24 | `aes_decry_2_rounds.debugged.cnf.xz` | UNS | 67.9 | 68.0 | 89.1 | 78.4 |
| 25 | `em_11_3_4_cmp.cnf.xz` | SAT | 39.4 | 197.8 | 105.5 | 109.8 |
| 26 | `Ptn-7824-b18.cnf.xz` | SAT | 38.1 | 37.3 | 237.9 | 9.9 |
| 27 | `1-ET-256-K-70.sanitized.cnf.xz` | UNS | 31.0 | 29.8 | 60.2 | 26.7 |
| 28 | `ContextModel_output_7_3_9.bul_.dimacs.cnf.xz` | SAT | 22.2 | 22.2 | 38.8 | 63.2 |
| 29 | `EDP3-11000.cnf.xz` | SAT | 22.0 | 22.1 | 63.5 | 196.5 |
| 30 | `jgiraldezlevy.2200.9086.08.40.41.cnf.xz` | SAT | 21.5 | 22.2 | 113.8 | 16.8 |
|  | **PAR-2 / solved** |  | **382.51** (28/30) | **333.51** (29/30) | **229.69** (29/30) | **312.57** (29/30) |

*(timeout 1200s)*

**Reading this table.**
- **SCHED-C5's verdict: NEUTRAL, and the default STAYS ON.** Same `#solved` on both tiers (26/28, 28/30),
  so the pre-registered ship criterion — keep it unless demote *loses a solve* outside the may-not-gate
  set — is met. Tier B −0.62 PAR-2 favouring OFF is **FRAGILE at k=2**; Tier C +10.25 favouring ON is
  **UNDECIDED**, since `frb45-21-2` alone is 102 % of it and removing that one instance flips the sign to
  −0.23. `frb45` is in the may-not-gate set, so this is exactly the shape that may not gate alone.
  The knob is priced as **measurement-neutral**; it ships ON on reference-parity grounds, not on PAR-2.
- **The free control set held**: nine instances Factor cannot admit at t=0 ran identical trajectories.

**▶ T3, BOTH HALVES** (`tools/bench/subset_score.py`, subset = 24 of 30, window `20260805-205741-C-sweepon`):

| arm | vs kissat (subset) | vs cadical (full) | solved |
|---|---|---|---|
| `sweepon` = shipped | **1.722×** | **1.224×** | 28/30 |
| `sweepoff` | **1.459×** | **1.067×** — *inside the 10 % band* | 29/30 |

**▶▶ THE SWEEP GOVERNOR HYPOTHESIS IS CONFIRMED, AND THE COST IS CONCENTRATED.** Same-window A/B:
Sweep ON is **+49.00 PAR-2 worse and loses a solve** (`minxor128`, TO at 1200 s vs solved at 992.5 s —
a near-miss Sweep pushes over). **With Sweep off the CaDiCaL half of T3 would be MET (1.067×)** and the
kissat half improves 1.722 → 1.459×.
**But the cost is TWO instances, not a broad loss:** `constraints_18_0.3_2` **+291.1 s** (the instance the
blast-radius survey already flagged — 42 rounds, ZERO emissions, bit-identical trajectory) and
`minxor128` **+207.5 s**; removing both flips the sign to **+7.62 favouring Sweep ON**, and Sweep wins
real time elsewhere (`em_11_3_4_cmp` **−158.4 s**, `sat-bench-trig-bhaskara` **−112.0 s**).
**So this is a DOSING/TARGETING defect, not a case against the rule** — spend without yield on a couple of
instances, exactly what a productivity-aware governor is for. **SWEEP SHIPS — USER DIRECTIVE, 2026-08-05, NON-NEGOTIABLE.** The degradation is the defect to fix, not
the rule. Neither disabling it nor shrinking its budget until it is inert counts as a fix; after any
governor change Sweep must still fire and still find equivalences, proven by `c sweep`/`c sweepemit`.
**Leading hypothesis under investigation (branch `sweep-governor-fix`): the METER.** Stage B audited
every rule's currency and found C1/C2 under-charged **2712×**; Sweep's `EFFORT_SPECS` row landed in S-f
**after** Stage B and never got that audit. A rule consuming ~50 % of wall on a nominal 100‰ share is
prima facie evidence the currency is wrong — the sub-solver owns its own arena/watches/trail, so its
propagation may not be charged. Second hypothesis: no productivity memory (42 rounds, 0 equivalences,
nothing learns). Third: per-round environment rebuild.
**Owed before this is a verdict rather than a direction:** the aggregate is FRAGILE at k=2 and a single
solve flip is single-instance by construction — run paired draws on `minxor128`, the protocol that has
now found a lottery 5 times out of 5.

**▶ AND A THIRD BAR: ARMS MAY NOT SHARE A NODE (found 2026-08-04 while adding the oracle column).**
Array task index is `arm × n_instances + instance`, so SLURM fills nodes in INDEX order and **arm 0 lands
systematically on one node**. Measured in `20260804-165514-B`: `shipped` ran **28/28 on node017** while
`facon` split 17/11 and `facoff` 19/9. So a small within-window arm delta can carry a node/contention
artefact — and `shipped` was the strict per-instance best on 12/28 there, typically by 1–6 %.
**The 6.72 floor is NOT contaminated** — its three bit-identical arms ran **30/30 on node017**, so it is a
SAME-NODE bar. That is the uncomfortable part: **6.72 may UNDERSTATE the noise for a comparison whose arms
straddle nodes.** Before believing a within-window delta under ~2 floors, check the `node` field per arm in
`results.json`. Proper fix (unfiled, needs the runner): interleave task indices so every arm is spread
across nodes identically.

**▶ WITHIN-WINDOW vs ACROSS-WINDOW ARE DIFFERENT BARS, and the second is BIGGER (measured 2026-08-04).**
The 6.72 floor is between bit-identical arms *inside one window*. **Across windows the same shipped solver
moved 12.23 PAR-2**: Tier C **309.05** (`20260804-154341`) vs **296.82** (`20260804-165516`) — two runs the
same day, on an idle partition, at commits that differ only in documentation, i.e. **byte-identical solver
behaviour**. That is **1.8 within-window floors**. Tier B was stable across the same pair (86.51 vs 86.42).
**Consequence: never compare an absolute Tier C PAR-2 across windows** — only same-window arms at equal
concurrency, now with a number attached. Quoting "where we stand" from one window is precise to about ±12 on Tier C.

**Noise floor — USE 6.72 for SAME-WINDOW arms (window `20260804-030938`, idle partition, three
bit-identical arms: 364.50 / 362.17 / 357.78, per-instance median 4.8 s).** The older 10.8 below is the CONTENDED bar; keep it only for
runs that shared the partition.
**The older 10.8 bar** was three bit-identical arms in a CONTENDED window (`20260731-013523`, which overlapped a full Tier C array by ~1 h 12 m), so it is an upper bound on contended noise, not a measurement of quiet-partition noise. Use 6.72 for idle, 10.8 only for runs that shared the partition.

**Older windows** — aggregates only, per-instance detail in each run's `results.json`, and **not cross-window comparable** (see the bar above). The shipped Tier C ratio moved 1.007–1.066× across the 07-29→07-31 windows at an UNCHANGED default, which is the whole argument for same-window A/B. **Never mix 14- and 28-instance Tier B PAR-2** — pre-2026-07-29 Tier B numbers are `core14`.

**▶ PROTOCOL BREAK — never quote a pre-2026-07-28 Tier C aggregate against a current one.** Before the like-for-like proof protocol settled, an instance whose DRAT check exceeded the checker budget was scored UNSOLVED even though we solved it (verified: `urq45`, a 354.5 s solve scored unsolved on a 2400.1 s check), overstating our Tier C PAR-2 by up to ~+68.

## Standing lessons

**▶ They live in [`docs/lessons.md`](docs/lessons.md)** — 22 active entries, freely revisable, and
**never cited by number in any document**: when one informs a decision, write the finding and its primary
evidence instead. A hostile re-validation on 2026-08-04 DELETED six that rested on single draws from
instances since measured chaotic.

**What stays HERE, because it is STATUS and not a lesson — the may-not-gate set:** `mod4block`, `j3037`,
`jgiraldezlevy`, `urq45`, `frb45`, `ContextModel`. A per-instance verdict on any of them **may not gate a
change alone** ([[chaotic-instance-veto]]); perturb the baseline with a mechanically-inert knob first and
judge on the remainder or a replicated distribution. **Do not grow the set** (user directive).

## Open items (small, none blocking)

- **`D-BVE-SCRATCH` — a FIFTH sighting of the per-call linear-scratch hazard, in a hot rule.**
  `bve_detect_gate` allocates `std::vector<int> pos_bin(2*nvars,-1)` and `neg_bin(...)` **per candidate**,
  and `bve_eliminate` two more `2*nvars` byte vectors — four O(nvars) fill-inits per elimination
  ATTEMPT. Same shape that cost 174× on native LRAT before the persistent-scratch + touched-restore
  fix (Track C · C4-perf). Found while adding ITE/XOR extraction, which deliberately added none of its
  own (its per-pivot signature index is O(P+N)). Route: `revisit-codebase`. Not urgent, but BVE runs on
  every elimination phase, so the constant is paid a lot.

- **`D-STRENGTHEN-BUDGET`, now scoped to "DOSE C1/C2".** C1 SubsumeForward / C2 Strengthen are the only
  inprocessing slot entries that are neither dosed nor budget-aware, and one round cannot be interrupted —
  C2 owns **97.8 s of `DivS_862_11`'s 109.0 s in four applications**. Interruptibility itself is measured
  negative and reverted (kept as a §5.6 arm behind `TACTIX_MIDROUND_BUDGET=1`; report
  `benchmarks/reports/2026-08-04-midround-budget-negative.md`), because at 100‰ the budget binds ~once per
  solve and a stopped round merely DEFERS its candidates. The **dose** axis is live (1‰ → DivS 11.5 s).
  Route via the calculus skill when it becomes a guard change.
- **Proof-rescue residuals.** `D-LRAT-FALLBACK` **SHIPPED 2026-08-04** (`3c3de3b`): a DRAT check that
  exhausts its budget triggers a re-**solve** with `--lrat` + cake_lpr (a conversion is impossible — it
  needs the same backward search that just timed out); a genuine REJECTION still scores unsolved.
  Left open: the rescue reuses `checker_budget` rather than its own knob; LRAT was 3.6× the DRAT size on
  `urq45` and cake_lpr's RSS 4.02 GiB against an 8 GB cap; and the per-task SLURM wall doubled to
  `2*(timeout + checker_budget) + 120` for a path that fires rarely. Revisit if Tier D ever rescues.
- **Standing rule: every Tier C run carries `--baseline kissat --baseline cadical`**, or T3's kissat half
  goes unmeasurable again. The instrument is otherwise CLOSED (2026-08-03) —
  `benchmarks/tiers/C-ruleneutral.list` (24 of 30, frozen with its known defect + closer in the header) and
  `tools/bench/subset_score.py`, which scores the subset AND the full tier from one `results.json`; the
  subset is a scoring VIEW, never a separate run. **Caveat:** the list comes from a single-shot sweep
  ablation and 4 of its 6 exclusions are perturbation-fragile instances. A replicated re-derivation exists
  (15 of 30) and was deliberately NOT adopted — the T3 verdict is the same either way (Log 2026-08-04).
- **Citation pairing:** when citing `benchmarks/reports/2026-08-03-broad-factor-and-t3-halves.md`, cite
  `…/2026-08-04-broad-factor-admission-retest.md` with it — the latter supersedes its per-instance
  conclusion (the `j3037` veto was a measured lottery).
- **Task #39** — `--random-policy` livelock on 5 tiny cases in `tools/fuzz/known-hangs/` (N1 robustness, not soundness).
- demo3 is a SEARCH-only mirror by permanent design (stat contract = §8.4 within-noise; the deferred
  full-slot-mirror enhancement was dropped 2026-07-25, user decision).
- Stress-pass ledger: V6 unit-channel DONE (`4f2b53e` + congstress nightly); Factor-mid-stable DONE
  (2026-07-25, 9k/0); post-Track-H random-policy ASan 8000/0 both formats. **Read every pre-2026-08-05
  native-LRAT figure in this file through the oracle defect recorded in the anchor.**
- Routed by the 2026-07-29 review (`benchmarks/reports/2026-07-29-review-post-track-r.md`; `RV-` namespace):
  **RV-R2** Reorder has no stress policy; **RV-R3** `benchmarks/probes/` needs a retention rule;
  **RV-R4/RV-R5** small duplication + allocation watch items. (**RV-R1 DISCHARGED 2026-08-05** — the Sweep
  sub-solver now has fuzz legs on both proof formats with the detector reached at the source.)

## Non-negotiables status (plan §1.1)

| | Requirement | Status |
|---|---|---|
| N1 | Any applicable rule appliable via API at any time, incl. branching choice | ◐ **No open breach.** The one the N4 audit found — `Factor`'s density predicate NoOping inside `factor_apply`, so **no API caller could apply a legal factoring on a binary-sparse formula** while the guard reported applicable — was CLOSED 2026-08-04 by MOVING the predicate into DefaultPolicy's slot as `Kernel::factor_admit` (SCHED-N1FIX; DefaultPolicy byte-identical, `tests/test_factor_n1.cpp` fails on the pre-fix build). **The shape to watch for: guard-true / apply-NoOp** — it is why V5 Sweep was deliberately left unregistered until it had a proof delta. Otherwise: v1 API frozen (revs 20–21); §8.4 demos 1–3 prove it end-to-end; audits upheld; two earlier leaks found+fixed (rev 34 payload aliasing; rev 40 off-fixpoint Decide via `trail_tainted`). Final acceptance = CCE plugin (phase 4) |
| N2 | Proof production on every path (DRAT/LRAT) | ◐ **No open breach at HEAD.** Two were CLOSED on 2026-08-05 and the merged tree is GREEN: six fuzz legs at TWO seeds, 6000 cases each, 0 failures (`1584462`–`1584467`). The 38 `lrat-check` rejections seen mid-session were **drat-trim's `-L` CONVERTER, not our emitter** — kissat's and cadical's own proofs are rejected identically on the same inputs while our native LRAT verifies under lrat-check and cake_lpr; the harness now stands that cross-check down on the two unsound input classes, visibly counted (`…-drat-trim-conversion-crosscheck.md`). **Two breaches were CLOSED the same day.** (1) **`Sweep` S-e emitted every general-path lemma with INVERTED literal parity** — both sub→kernel decode sites tested `(sl & 1u) == 0`, so each lemma was written negated (the verdict and `F` were always correct; only the proof lines were wrong); fixed by one shared `sweep_kernel_lit` (`8796b26`), `asconhashv12` now verifies on drat-trim AND cake_lpr, regression corpus `012`, `sweep_proofs` extended over the real emitted proof, `--sweep-stress-policy` added. (2) **the frontend deduped repeated input literals**, so an input clause's id named a different clause for the checker than for us and any LRAT chain citing it failed (`16b6e75`). **And the INSTRUMENT was broken: `fuzz.py`'s native-LRAT oracle accepted `NOT VERIFIED`**, so every LRAT rejection in project history was recorded as a pass. Reports `benchmarks/reports/2026-08-05-sweep-proof-rejection.md`, `…-sweep-n2-blast-radius.md`, `…-sweep-se-polarity-fix.md`, `…-sweep-n2-postfix-survey.md`, `…-input-clause-normalization-n2.md`. Otherwise: ◐ DRAT since phase 1; native LRAT since 2026-07-14; the 2026-07-15 audit was upheld and did not cover Sweep, which did not exist then |
| N3 | DefaultPolicy comparable to Kissat (T4 gate) | ◐ T1+T2 passed. **2026-08-05** (window `20260805-1150`, git `0418770`): Tier B(28) **70.71 (26/28)** · Tier C **419.91 (28/30)** vs kissat 228.45 (29/30) and cadical 312.27 (29/30). T3 halves **BOTH NOT MET and both REGRESSED** vs 08-04 — kissat subset **1.727×** (was 1.346×), cadical **1.345×** full (was 0.990×) — with kissat flat across the two windows, so this is a real regression rather than drift. Tier A 0.98×. **Tier D BUILT AND FROZEN 2026-08-03** (793 instances) — never RUN, and running it is a phase-3-exit prerequisite (§11 O9); `--tier D` still has no per-track flag for T4's SC25-set PAR-2 |
| **N4** | **DefaultPolicy's SCHEDULING at least as sophisticated as Kissat's default strategy** (rev 80, user 2026-08-04) | ✗ **KNOWN UNMET, MEASURED TWICE.** Pass 1 (rule families): 216 mechanisms, **132 gaps (27 high)**. Pass 2 (SCHED-A3, the REFERENCE's option set): all **418** options enumerated, **+15 gaps (4 HIGH) and 6 CORRECTIONS to pass 1**. Ledger LANDED and living at `docs/implementation-notes.md` §13 (§13.4–§13.6 = pass 2); full audit frozen at `docs/design/2026-08-04-n4-scheduling-parity-audit.md`; remediation staged as **Track SCHED** below, which also carries the four structural highs and the confirmed gaps. **N4 is a *mechanism* floor** — it may not be discharged by a tier number, and never deferred to §5.6 |

## Milestone gates (plan §8.3)

- [x] T1 — **passed 2026-07-14** (Tier A 200/200 PAR-2 0.48; Tier B 9/14 @ 231.6 vs MiniSat 8/14 @ 281.4; report
  `benchmarks/reports/2026-07-14-t1-gate.md`)
- [x] T2 — **passed 2026-07-14** (Tier B 11/14 @ 164.8 vs Glucose 8/14 @ 268.3; props/s geomean 0.92× kissat; report
  `benchmarks/reports/2026-07-14-t2-gate.md`)
- [ ] T3 — within 10% PAR-2 of CaDiCaL on Tier C + CaDiCaL/Kissat parity on the rule-neutral subset (rev 51).

  | half | requirement | status |
  |---|---|---|
  | CaDiCaL | ≤ 1.10× cadical, whole Tier C | **NOT MET: 1.345×** full / 1.357× subset (window `20260805-115039-C-c5`, git `0418770`). **Regressed from 0.990× full on 08-04** — it had been MET. |
  | Kissat | parity on the rule-neutral subset | **NOT MET: 1.727×** (same window). Still the further-away half, and it too regressed, 1.346× → 1.727×. |

  **Both halves moved the wrong way in one day while kissat's own numbers stayed flat (230.74 → 228.45),
  so the partition is not the explanation.** What changed in between is V5 Sweep going from unscheduled to
  scheduled at 100‰. Recovering T3 now runs through Sweep's governor, not through new mechanisms — the
  decisive experiment is in the tier-table block above. Do not re-open (c′), the pre-Sweep TB4b question:
  it is subsumed by a much larger effect.

  **History, compressed — each line is knowledge, the detail is in the cited report.**
  - **The all-rules-ON directive (2026-07-29) changed the target, not the solver.** With rules OFF the
    CaDiCaL half was MET (1.00× same-window); with every proof rule ON it was not. Judge the rule family
    together — `2026-07-28-all-rules-on.md`.
  - **X2's effort share was a missing governor, and fixing it MET the CaDiCaL half on 07-30** with every
    rule on: `HyperTernaryRes` at 20‰ against cadical's `ternaryeffort` **8‰** — same-window
    397.94 (28/30) → **319.19 (29/30)** vs cadical 317.04, and it gained a solve. Landed in `EFFORT_SPECS`.
  - **A uniform slot rate of 2‰ does NOT clear this half reliably** — two same-day windows gave 1.065×
    (inside) and 1.101× (outside); the saving shrank 26.1 → 8.8 PAR-2. Not shipped; `TACTIX_SLOT_PERMILLE`
    stays a §5.6 dosing arm. Its per-instance effects are reproducible in BOTH directions (fixes
    j3037/asconhash, breaks ContextModel/em_11/frb45/Ptn-7824), which is what retired the "the regression
    is two instances" framing.
  - **R-d retirement (2026-08-02) traded the CaDiCaL half for +2 Tier B solves**, banked under plan rev 78
    (Tier C 329.63 → 358.12).

  **Both instrument prerequisites are DONE** (2026-08-03) — see Open items for the standing rule and the
  subset's caveat.

## Phase 0 — setup (complete 2026-07-09)

- [x] Repo + plan + skills + CI; reference solvers + checker toolchain pinned (`tools/solvers/`, `tools/checkers/`); fuzz harness validated
  against stand-ins + deliberately broken wrappers; benchmark runner; Tier A (200 gen) + Tier B (14 SC,
  `benchmarks/reports/2026-07-09-tier-b-freeze.md`) frozen; kissat study notes `docs/design/kissat-notes.md`
- **Exit:** fuzz pipeline end-to-end ✓; Tier A/B frozen ✓

## Phase 1 — core CDCL kernel (complete)

- [x] Kernel (arena, watches, trail, VMTF, phases; later EVSIDS heap + target phases), rules M1–M4 + S1/S2 + S4 SwitchMode/stable, binary DRAT +
  competition I/O, registry + guard/instances/apply contract from day one
- [x] Reduce used-bit retention (the s055 trajectory fix); **policy-class refactor (rev 17 — ALL scheduling in the Policy)**, the load-bearing
  architecture decision
- [x] Null results recorded: assign-path inlining (memory-bound), LTO; radix bump sort −7 %; binary-reason jumping REVERTED (needs kissat's
  bigbig classifier)
- **Exit:** T1 — **PASSED 2026-07-14**

## Phase 2 — search management + API freeze (complete 2026-07-14)

- [x] S3 Rephase + S5 Lucky; `kernel_propagate` extraction (§5.3 primitive); **native LRAT** (CaDiCaL scheme; kissat has none); **Rule API v1
  FROZEN** (revs 20–21) + demos 1–3 (demo3 stat-exact)
- [x] Carried: bias-corrected per-mode EMAs, kissat restart constants, glue recompute+promote, B,B,I,B,B,O rephase; 9vliw → kissat parity; s055
  trajectory gap closed (residual = 1.42× propagation volume — the standing TB5 lever)
- **Exit:** T2 — **PASSED 2026-07-14**

## Phase 3 — inprocessing

Every rule lands N1-exposed + random-policy-fuzzed regardless of whether DefaultPolicy schedules it; per-rule detail in its design doc + commit,
semantics in `docs/calculus.md`, guards + deviations in `docs/implementation-notes.md`. **Since 2026-07-25 the composition is the S4 arrangement,
unconditionally** (probing family + simplifiers + during-search dosed BVE/Factor; the legacy preprocessing-only composition was removed with the
negative arms — git history has it).

- [x] **C1 SubsumeForward · C2 Strengthen + the registry-driven slot** (slot = data; the phase-6 arm space) **· C3 Vivify** (LCM scope) **· V1
  Probe · V2 Backbone · V4 TransitiveReduce** (`kernel_delete_binary` primitive) **· V3 Substitute** (first ρ consumer;
  `docs/design/v3-substitute-design.md`) — all 2026-07-15..17; corpus 002/003 found en route
- [x] **E1 BVE — COMPLETE (E1a–E1d)**: drained-root elimination, RUP-only resolvents, syntactic gate extraction, `--bve-gates` knob; design
  `docs/design/e1-bve-design.md`; `bve_stress_policy` reusable. ITE/XOR gates landed 2026-08-05 (SCHED-C8). Remaining: kitten
  definition extraction (O11), phase 4
- [x] **Tier C built and frozen** (30 instances; `benchmarks/reports/2026-07-15-tier-c-freeze.md`)
- [x] **Independent-lineage campaign — 4 measured, 0 adopted**, each with a recorded mechanism; strongest phase-6 arms: CMS usage-count Reduce
  ranking, chronological backtracking (TB4), Glucose blocking restarts. Survey `docs/design/independent-lineage-survey.md`
- [x] **N1/N2/N3 audit + canonical-batch fuzz** (2026-07-15) — upheld; deferred backlog `benchmarks/reports/2026-07-15-codebase-review.md`
- [x] **Nightlies GREEN at scale** (DRAT 20k/0; congruence-stress 10k/0). Method lesson: nightly scale catches what `--quick` misses (corpus
  006, 007). *(The native-LRAT nightly figure is void — see the fuzz-oracle defect in the anchor.)*
- [x] **Calculus formalized** (rev 36): `docs/calculus.md` + `docs/implementation-notes.md` + the calculus skill; **guard purification COMPLETE**
  for every removable deviation (revs 37–43; only D-ROOT remains, by design). Corpus 004/005/006 regression bed
- [x] **V6 Congruence — ships ON** (2026-07-19): AND+XOR+ITE union-find closure; Tier B −30 %, +1 solve; design + report in `docs/design/` /
  `benchmarks/reports/2026-07-19-*`
- [x] **E2 Factor (BVA) — ships ON** (2026-07-20): net-positive after the duplicate-binary deletion fix; S2a/S2b closed 2026-07-24 (steering
  disconfirmed); the mp1 gap CLOSED 2026-07-25 (input dedup + dosing + composite; gate report) — nothing remains on E2
- [x] **Inprocessing revamp — slot ACTIVATED** (2026-07-19): preprocessing at conflicts=0 + `Kernel::dense` shared index + kissat phase
  structure. Superseded as default by the S4 arrangement (2026-07-25)

### Track C — V6 Congruence completion (rev 56) — COMPLETE 2026-07-25

- [x] **C0** XOR output-sign fix (`1a365da`) · **C1** `add_equivalence` all-or-nothing (D-CONG-SKIP: dropping an inference is sound, an
  unverifiable certificate is not) · **C2** degenerate-ITE filter (gates −23 %, equivalences identical) · **C3** the unit channel
  (`hwmcc` 48.77 → 6.67 s; the degeneracy ladder was drat-trim-validated in all four shapes BEFORE implementation and turned out to be kissat's
  own construction) · **C4** LRAT chains REPLAYED not derived (checker-order by construction)
- [x] **C3b** (`2998029`) AND clash → unit, idempotent dup → unary merge, `x≡¬x` → refutation; residuals in the D-CONG-UNIT ledger row
- [x] **C4-perf** (`d2f7695`) hwmcc native LRAT **1680 s → 9.64 s (174×)**, proof byte-identical; the cause was O(nvars) fill-initialized scratch
  per proof-chain op, NOT replay propagation ([[per-call-linear-scratch-hazard]]) — fix = persistent scratch + O(marked) restore, so **no
  `side_ids` machinery was needed** and the replay architecture stands

### Track H — `H` out of the Kernel (rev 60) — COMPLETE 2026-07-23

- [x] **H0–H7**: `H` (VMTF, score heaps, phases, ranking mode) is policy-owned OPTIONAL state — `k.h == nullptr` is sound, just unguided — bought
  at **zero PAR-2**, every increment gated on trajectory identity + byte-identical DRAT. Design + rejected alternatives
  `docs/design/h-relocation-design.md`. Live constraints: H4 pimpl (include cycle; kernel non-copyable); H5 null-`H` legal (Decide → index order;
  Rephase/SwitchMode guards require `H`, a pure precondition of `H`-only ops); H6's sub-policy super-class was deferred until a 2nd arm existed
  and **was un-deferred by Track S · S1b** (`a36b4c2`) — `src/policies/subpolicy.hpp` has held the `SubPolicy` base since. Five categories now:
  Branching, Reduction, BveBound, FactorMatch, CandidateOrder (SCHED-E1)
- Method knowledge kept: bit-exactness gates the EXERCISED path and adversarial review the unexercised one (which caught a lost include and an
  unguarded deref no gate saw); null-path bugs surfaced only under FULL-suite ASan with the default flipped; and a structural check can replace a
  benchmark (zero `k.h->` in the propagation loop killed the indirection worry)

### ▶ Track T — the Tier B tail (TB1–TB6; the bare T1–T5 are the §8.3 gates)

Diagnosed 2026-07-21: four tail instances were 69 % of Tier B time, each with a different cause. **TB1/TB2/TB3/TB5/TB6 CLOSED** — detail in
`docs/log/2026-W30.md`/`2026-W31.md` and the cited probes.

- [x] **TB1** congruence is NOT the FmlaEquiv gap (kissat with ALL simplification off runs it in 8.68 s, faster than with any single simplifier
  off); the structural-hashing gap is real but ≤ 0.7 s · **TB2** x9's outlier premise dissolved (1.17–1.19× vs kissat) · **TB3** Factor verifiably
  non-inert on x9 · **TB5** per-prop cost is AT PARITY with kissat on all residuals — the residual is SEARCH SHAPE, and the watch-layout
  engineering lane is measured-dead · **TB6** default mode alternation BEATS both pinned modes
- [x] **TB4** chrono ON by default (`CHRONO_LEVELS=100`). The first measurement (+13 %) was a PORTING ERROR, not a verdict — analyzing from above
  the conflict level silently DROPPED the conflict on 84 % of chrono backtracks; fixed → −2.4 %. **`stats.backjump_stale` MUST stay 0.** Found
  only because a negative result was audited rather than accepted
- [ ] **TB4b** the remaining chrono divergences. **(A) the one-literal-on-conflict-level shortcut** and
  **(B) the watch-repair primitive** are DONE (`fc57eff`; the 2026-07-28 negative verdict was retired as
  a BROKEN arm, not a crippled one — report `2026-08-01-tb4b-watch-repair.md`). **Ships OFF**: ON and
  CRIPPLED are indistinguishable on Tier C and the proof-size axis FLIPS SIGN between tiers, so it is a
  §5.6 exhibit, not a default. **Unit chrono routing SHIPPED** 2026-08-04 (`8118c68`, `veer_axi` 6×) —
  and introduced the N2 defect fixed in `52a4e8f`. **`jumpreasons` is ported but ships OFF (SCHED-C8); still
  unported: cadical's `chronoreusetrail`.** `j3037`/`urq45` chrono sensitivity remains UNEXPLAINED.

### ▶ Track S — the effort-budgeted target arrangement — S4 CLOSED

- [x] **S1** the per-phase-overhead explanation for repeated during-search BVE is REFUTED — the cost is search TRAJECTORY, not the root descent
  (`775eefc`) · **S1b** BVE growth-slack ramp, the first SubPolicy (`a36b4c2`) · **S1c** needed-bound cache + ceiling 8 (`489fab1`; the old dose
  cap was an *accidental governor*, [[accidental-governor-trap]]). **The ceiling's stated warrant was RETRACTED evidence** — the "crypto1 BVE
  U-shape" was drawn through single draws on a dose-chaotic instance; what survives is one same-window Tier B A/B (HEAD 16.66 / ceil-16 17.76 /
  ceil-8 15.08 / flat fixed-8 15.85) that is single-instance-dominated on a tier with **no measured noise floor**. The Tier C/D re-sweep is owed
  and is the only thing that would warrant the constant · **S2a** FactorMatchPolicy, steering DISCONFIRMED (the 27×27 cap is structural) ·
  **S2b** composite grids land default-OFF (`docs/design/s2b-composite-grids-design.md`) · **S2** probing rules re-measured on-cluster: the
  historical SAT-disruption is GONE · **S3** P2 follow-ups (probing stress 16.5k/0; `2ed8bcc`)
- [x] **S4 · the TARGET ARRANGEMENT — GATE PASSED, DEFAULT ON (2026-07-25).** B+C as one unit: arr {B 17.45, C 552.91} vs legacy {B 43.86,
  C 537.83}; mp1 5.34 s beats kissat 9.09 (proof VERIFIED); the density-gated-doses residual fix (`1dbd439`) then gave **B 10.62, C 520.76**.
  Root causes + measured negatives: `benchmarks/reports/2026-07-25-s4-gate-mp1.md` (its §2 banks the dead knobs, [[s4-gate-negatives]]); dosing
  design frozen at `docs/design/s4-factor-dosing-design.md` (§3.e bound coupling REVERSED by measurement)
- [ ] **T3 gate re-measurement** + report to `benchmarks/reports/`

### Track W — S6 `Walk` local search (plan rev 71) — SHIPPED ON, CLOSED for phase 3
*(its phase-6 hand-off is `W4e`, in the Phase 6 section)*

**▶ WALK IS ENABLED BY DEFAULT AND THERE IS NO OFF SWITCH.** `ROTATION` fires it on 2 of every 6 rephase events
(`default_policy.cpp:809-814`, `{BEST, WALK, INVERTED, BEST, WALK, ORIGINAL}`) at `WALK_PERMILLE = 560`
(`default_policy.hpp:310`); `walk_guard` is only `!inconsistent && h != nullptr && nvars > 0` (`walk.cpp:411`). No `TACTIX_NO_WALK` exists.
Witness on a stock build: `c walk 12 walks (12 improved, 106,336,087 flips)`.

Local search was the largest missing mechanism (`--walkeffort=0` costs kissat Tier B 8.62 → 29.22). `H`-only policy op, no proof delta:
`src/policies/ops/walk/`, design `docs/design/s6-walk-design.md` (§6 = the full ledger of what was tried).

- [x] **W1/W4a** walker core + `--walk-stress-policy` (`8da55c9`, `43510fc`) — ProbSAT/CB scoring, kernel untouched (legal at any level, N1)
- [x] **W2** schedule — **Walk AUGMENTS Best, it does not replace it.** Kissat's `B,W,I,B,W,O` ported literally REGRESSED Tier B (13.63 vs 10.04);
  the cause was the lost Best consumption, not the walk (kissat saves phases at ASSIGN, we at unassign — `implementation-notes.md` §8). Shipped
  `B,B+W,I,B,B+W,O` · **W3** warmup measured and NOT adopted (our saved phases already come from a real trail)
- [x] **W4** gate PASSED, shipped ON: Tier B **8.97 vs 9.88** (−9.2 %), Tier C FLAT. **Banked negatives, do not retry blind:** effort anchored on
  the last inprocessing phase (9.69 vs 8.97); accept-only-if-better-than-all-previous (worse than both arms)
- [x] **W4b/W4c/W4d** the `mp1-Nb5T15` poisoning is the **target** channel, not the dose, and **kissat has it too**; six reference-matching
  attempts exhausted; W4d's "stop walking when it stops paying" is DEAD (it was under-dosing, and would have been a novel mechanism neither
  reference has). Five arms sit behind knobs for §5.6
- [x] **W4f** walk DOSE swept and SHIPPED at 560‰ (`0c52fdd` → `61138b8`): the root defect was a UNIT confusion — we scale a per-mille of search
  ticks but SPEND in walk steps (~13× less scanning than cadical). Tier C 392.98 → 322.80, `mp1-Nb5T15` TO → 26.1 s. The earlier sharp preference
  for 640 was an artefact of a then-wrong `bumpreasons` rate; re-swept, 400..800 shows no trend
  (`benchmarks/reports/2026-07-27-tier-c-push.md` §1/§8.2)

### Track R — the full reference rule inventory (plan rev 72, user 2026-07-27)

**Numbering:** inside this section `R<n>` means **Track R · R<n>**. See the namespace key at the top of `## Log` before reading a bare `R<n>`
anywhere else.

**User directive:** implement/enable every proof rule Kissat and CaDiCaL have, and measure with proof generation on BOTH sides. Order is
measured-value-per-cost, cheapest first; each lands under the §5.3 contract (one directory + one registry line) with its own stress-policy + fuzz
legs on both proof formats and a same-window gate. **Counter-evidence kept so the sequencing stays honest** (a reason to run the search-quality
tracks in parallel, not to skip the work): turning off EVERYTHING we lack costs kissat only **14.32 Tier C PAR-2** (sweep + reorder + randec +
eagersubsume + definitions, whole-tier same-window ablation `20260727-002213-C-kabl`), **of which sweep alone is 14.00** — the other four are
inside the noise floor and are not separately resolved.

**LANDING CONVENTION (user, 2026-07-28): a rule lands ON.** The earlier "ships OFF until its own A/B says it pays" is DEAD — it contradicted
[[no-rule-off-configs]] and judged rules ONE AT A TIME, the one measurement that cannot see this family's dominant effect: **X1/X2/X4 measured
alone are 21.79 / 53.26 / 13.08 Tier B PAR-2 against 9.22 for all three together** — they repair each other
(`benchmarks/reports/2026-07-28-all-rules-on.md`).

**MEASURED CONSTRAINT on any joint-admission lever — narrower than it looks.** `mod4block` is NON-MONOTONE in how consistently the trio runs:
never 9.26, always 13.17, **once-then-stop 263.87**. The trio must therefore run all-solve or not at all, which kills every mid-solve decision and
leaves only a STATIC per-instance one — and the best such separator measured 4/13 misclassifications, i.e. chance
(`benchmarks/reports/2026-07-20-bve-skip-feasibility.md`). A phase-6 context feature, not an engineering lever, absent new evidence.

- [x] **R1 · X2 `HyperTernaryRes`** (cadical `ternary.cpp`) — add-RUP only, no witness. `d0bb6ec`; **ON since `3fb85bd`**
- [x] **R2 · X1 `BCE`** (cadical `block.cpp`) — delete + push-witness, reusing E1's occurrence index and reconstruction stack (kissat 4 dropped
  it). `d0bb6ec`/`1a9afb1`; two defects found by fuzz and fixed (a wrong-SAT from filtering the blockedness test's opposite side; a
  duplicated-binary crash). **ON since `3fb85bd`**
- [x] **R3 · X4 `Instantiate`** (cadical `instantiate.cpp`) — the strengthened clause is RUP against `F` alone because `¬(C∖{l})` leaves `C` unit
  and propagates `l` itself. `2085429`. Fires but yields ~nothing alone (FmlaEquiv 57,357 probes → 5 literals); **ON since `3fb85bd`** — alone is
  the wrong experiment for this family
- [x] **R4 · S7 `Reorder`** (kissat `reorder.c`) — `H`-only **policy op**, no proof obligation, no calculus entry; deviations in
  `implementation-notes.md` §10. `4482bb5`. **Ships OFF** (`TACTIX_REORDER`, kissat's 1/2 values) — the one Track R result that is not an
  inventory tick: **parity on Tier B's UNSAT half (10.16 → 10.11) and 3.4× worse on its SAT half (5.28 → 16.99)**, a SAT-vs-UNSAT context axis no
  other §5.6 family carries. **Scope: policy op, NOT a proof rule — the all-rules-ON directive does not cover it; ask before flipping**
- [ ] **R5 · V5 `Sweep` + the embedded sub-solver** (kissat `sweep.c`+`kitten.c`; §11 O11) — the largest single item in kissat's own ablation
  (+14 alone) and the biggest build; also unlocks kitten-based **definition extraction** for E1 (the last E1 gap, +6.1 to kissat). Design
  `docs/design/v5-sweep-design.md`: O11 says WRITE a minimal sub-solver rather than vendor kitten (verbatim copying is barred, and we need
  LRAT-grade antecedents kitten's core does not give); the real risk is the proof obligation — a swept equivalence is entailed but NOT RUP.
  **S-a…S-e are DONE; only S-f (scheduling) is open.**
  - [x] **S-a** the embedded sub-solver (`src/rules/sweep/subsolver.{hpp,cpp}`, `8ac2199`) — minimal CDCL, own arena/watches/trail, never touches
    `Kernel`, deterministic; verified by exhaustion against brute force on **1 M random formulas**, half under assumptions. Safety property: an
    unanalyzable conflict returns `Budget`, never `Unsat`
  - [x] **S-b** antecedent recording + core extraction (`9db22df`) — `derivation()` returns ordered lemmas with LRAT chains over environment ids,
    in the kernel's chain order. Verified three ways over **207 k refutations** plus 40 micro cases through the REAL drat-trim and lrat-check
    (`tests/sweep_proofs.sh`). Knowledge: the first version of BOTH oracle and emitter produced only ONE-STEP derivations, so the lemma chains
    went untested while green — both now assert a multi-step floor
  - [x] **S-c** candidate selection + environment construction (`src/rules/sweep/sweep.{hpp,cpp}`), DETECTION ONLY. **The soundness argument is
    `env ⊆ F` LITERALLY** — every environment clause is a clause the kernel holds, carried with its own proof id in `SweepEnv::clause_ids` — so
    root-false literals are KEPT rather than stripped (kissat strips; a stripped clause has no id to cite) and a root unit joins only when
    `unit_ids[v] != 0`. Deliberately left UNREGISTERED until it had a proof delta (an always-`NoOp` rule is the guard-true/apply-NoOp shape).
    Two defects the tests caught first: the seed filter "has watch-list occurrences" is wrong (a large clause is watched on two literals only) and
    uniting a class with its own negation built a two-cycle that HUNG `repr_find`. **RV-R1 discharged**: fuzz on both formats with the detector
    reached at the SOURCE (102 of 411 runs ran a round — 8,174 environments, 807 equivalences), against a detector-OFF control
    ([[ab-arm-identical-artefacts]])
  - [x] **S-d** emission via the RUP fast path — assume `x ∧ ¬y` in the MAIN kernel at probing scope and replay (V6's `congruence_rup_chain`,
    exported as `congruence_replay_rup`); **both directions must replay before either binary is added**. The replay runs in DRAT mode too, where
    it is not chain construction but the RUP TEST. This is where the registry line, the `docs/calculus.md` V5 entry and the default-on decision
    landed. It also shipped an N2 defect (see the Log) and, unnoticed, SCHEDULED Sweep
  - [x] **S-e** emission via the general path — the sub-solver's core mapped back to main-formula ids and replayed as a resolution chain; anything
    unmappable **declines**. Worth 8 of 4380 claims on random formulas (larger on miters). It shipped with the inverted-parity N2 breach, since
    fixed — see the N2 row
  - [ ] **S-f · scheduling.** Sweep IS scheduled (kissat's own 100‰ `sweepeffort`) and budgeted, and since 2026-08-05 the budget is
    **enforced where the work happens** — `sweep_env_build` polls the round ceiling inside the occurrence walk, not only between seeds
    (`SweepBudget`; cadical's `kitten_ticks_limit_hit` discipline). Before that one seed could outspend the round dose **73×** and the
    scan was uninterruptible. Overshoot is now ≤1.005× on all four probe instances; `constraints_18_0.3_2` **−475.6 s**,
    `sat-bench-trig-bhaskara` **−65.7 s**, `em_11`/`minxor128` unchanged. Report
    `benchmarks/reports/2026-08-05-sweep-governor-diagnosis.md`; ledger rows **SP-SWEEP-1** (closed) / **SP-SWEEP-2**, **SP-SWEEP-3** (open).
    **The Tier C A/B is SPECCED AND UNRUN** — arms `govon` / `govoff` (`TACTIX_SWEEP_ENV_BUDGET=0`), pre-registered counters and ship
    criterion in the report §9; it should precede the SCHED-D2 pricing run. `TACTIX_SWEEP_PROBE` must not be set in any tier run
- [x] **R6 · X3 `CCE`** (cadical `cover.cpp`, the ASYMMETRIC variant the literature still calls CCE) — delete + push-witness, one segment per CLA
  step plus one for the blocked exit. **ON** (landing convention). Calculus §6c; guard row + ledger `D-CCE-SCOPE` in `implementation-notes.md`.
  **cadical-only, and it ships `cover`=0**: kissat's `cover.h` is a coverage-ASSERTION macro, not clause covering — verified at the source, and
  default-OFF in a reference is not disqualifying (`block`=0 and R2 BCE landed ON anyway).
  - **The witness is the whole rule.** Record only `C` + the CLA additions (`D_i`), never the ALA literals, and push in application order — `ρ`
    replays in reverse. What licenses the smaller recorded clause while blockedness is tested against the LARGER extension is the **ALA-closure
    lemma**: a model of `F∖{C}` that falsifies `D_i` must falsify every ALA literal too, because each was justified by a clause of `F∖{C}` whose
    other literals are already false. The **ATA exit with no CLA step in front of it pushes NOTHING** — `C` is RUP there.
  - **The two sides quantify differently and it is NOT symmetry.** CLA/blockedness runs over redundant clauses too (over-quantifying only makes the
    test stricter; filtering that side is X1's fuzz-2204 wrong-SAT shape). **ALA is irredundant-only for an ABSTRACT reason** — the lemma needs the
    justifying clause satisfied when `ρ` replays, and S2 Reduce deletes redundant clauses with no witness.
  - **§5.3 VERDICT: the rule body is a pure plugin; the REGISTRATION SURFACE is not one line but SEVEN central tables.** No kernel primitive, no
    proof-backend change, no other rule touched, no `ρ` format change — `kernel_push_witness` took CCE's multi-segment sequences unchanged (R7's
    blocker, a witness CUBE, does not arise: cadical `cover.cpp:411` pushes a single witness literal and so do we). What a new rule DOES have to
    edit centrally: `CMakeLists.txt` · `Stats` in `kernel/kernel.hpp` · `EFFORT_SPECS` + a meter + `AUTHORED_SLOT_ORDER` in `default_policy.cpp` ·
    `check-effort-specs.py`'s table · `main.cpp`'s stats line · `tests/`. Plus **the frozen C-ABI rule ids RENUMBER** (`test_api_c.c`'s golden list;
    documented as deliberate). See the Log entry for the argument that only the `Stats` one is a real architecture pressure point.
  - Reachability at the SOURCE, shipped DefaultPolicy, 59 structured/circuit fuzz-generator cases: **26 rounds, 4223 candidates extended, 2411 ALA +
    154 CLA literals added, 34 deleted (6 blocked / 28 ATA)**, against an exactly-zero `TACTIX_SLOT_DISABLE=CCE` control. `tests/test_cce.cpp`
    exhaustively cross-checks reconstruction over 400 random formulas: **3605 deletions, 18926 models that genuinely needed `ρ`, all repaired**.
  - Fuzz **GREEN, five legs, 0 failures**: stress×DRAT `1585216` / stress×native-LRAT `1585217` (4000 each) · `--random-policy`×DRAT `1585220` /
    ×native-LRAT `1585221` (4000 each) · DefaultPolicy quick `1585223` (10000); corpus 14/14 on every leg. ctest **45/45**.
  - **A/B SPEC READY, DELIBERATELY UNRUN** (a Tier C A/B was live) — arms + pre-registered counters + ship criterion in the Log entry below.
- [x] **R7 · X5 `Condition`** — **CLOSED 2026-08-05 as a reasoned NO-IMPLEMENT**, not deferred. Criterion
  fixed before investigating; value deliberately excluded from it. It fails on REPRESENTABILITY:
  `Kernel::reconstruction` carries one witness LITERAL that must lie INSIDE the clause, while a
  conditional autarky's witness is a CUBE over outside variables — split into single-literal segments
  each would sit inside its own satisfaction test (BCE's old wrong-SAT shape), and restricted to one
  in-clause literal the rule collapses to X1 BCE, which ships ON. kissat has no such rule at all;
  cadical ships `condition`=0. Measured anyway: fires on 13/30 Tier C, no solve flips, +11.3 % wall.
  Ledger `SP-COND-1` (`implementation-notes.md` §16) carries the reopening path.
- ~~**R8**~~ — MOVED to Track SCHED (`SCHED-C6`/`C7`) 2026-08-04: it was scheduling work filed under the rule inventory. Track R is now purely
  new rule FAMILIES (R5–R7), which is what makes it parallelisable against Track SCHED.

**▶ TRACK R'S INVENTORY IS NOW CLOSED except R5's scheduling half (S-f) and R6 CCE (in flight).** R7 was the last unimplemented family and it is
closed by decision, not by code — so no further reference rule family is owed. Read §16 before proposing X5 again.

### ▶ Track SCHED — the scheduling floor (plan rev 80 §1.1 N4, §5 obligation 5)

**Namespace:** `SCHED-<stage><n>` is its own id space — never read a bare `B1`/`C2` here as Track S or Track R. (Renamed from "Track N4" hours
after it landed, because `N4` must always mean the non-negotiable; the two frozen 2026-08-04 design docs still say "Track N4".)
**Work list:** the scheduling-parity ledger (`docs/implementation-notes.md` §13, living) + the frozen audit
`docs/design/2026-08-04-n4-scheduling-parity-audit.md` + the diagnosis `docs/design/2026-08-04-scheduling-sophistication-diagnosis.md`.
**216 mechanisms audited, 132 gaps (27 high).** Themes **T1** dropped wires · **T2** repetition-not-dose · **T3** right formula wrong meter ·
**T4** invented instance gates · **T5** cross-keyed ramps · **T6** emergent order.
**Tags:** `[wire]` = wiring a mechanism we already own · `[arch]` = new architectural support · `[revamp]` = routes out via the revamp skill.

**Standing protocol (stated once, applies to every item).** Same-window Tier B+C A/B, fresh `build/pin-n4-<item>` ([[build-path-ownership]]).
**Every item names a SOURCE-LEVEL readout, pre-registered and checked FIRST — an arm whose source counter did not move was not tested**
([[ab-arm-identical-artefacts]]; task #10's abort paths passed 4 × 10,000 fuzz cases while VACUOUS three independent ways), and the knob goes IN
THE COMMAND, never through the environment. Noise floor 6.72; **#solved beats PAR-2**; the may-not-gate set may not gate alone. Rule-touching
items carry DRAT + native-LRAT + random-policy legs; items opening NEW schedules add a stress-policy leg.
**▶ EVERY PRIOR DOSE NEGATIVE MEASURED THROUGH THE BROKEN METER IS VOID** and must be re-derived now that Stage B has landed — named: the V4
ticks-meter arm, the V4 early-exit arm, the C1/C2 mid-round budget arm, every per-entry per-mille sweep on a ticks-metered rule, and the BVE
ramp-ceiling 8-vs-16 choice.

**Track exit (checked at the phase-3 gate):** no HIGH ledger row remains `WE_ARE_CRUDER`/`WE_LACK_IT` without a closure or an accepted deviation
row carrying a measurement, AND the ledger has been regenerated from the REFERENCE's option set (SCHED-A3).
**▶ EXIT STATUS (2026-08-05).** Half two is **DONE and now machine-checked** (SCHED-A3, ctest `option_coverage`). Half one is **NOT** met: of §13.2's
HIGH rows, **eleven are now closed** (`SP-SLOT-2`/`SP-SLOT-3`/`SP-CAD-1` by Stage B · `SP-VIVIFY-1` by C1 · `SP-FACTOR-2` by C5 ·
`SP-BVE-GATE-1` by **C8** · `SP-JUMP-1` by **C9** — ported, ships OFF, default decided by a pre-registered A/B ·
`SP-SLOT-4` by **D1** · `SP-SLOT-1` by **D2** · `SP-SLOT-5` by **D3** · `SP-EFFORT-CEIL-1` by **E3**, ported and
measured INERT, its A3 headline corrected — kissat has no upper clamp at all), and **two are accepted deviations
carrying a measurement and a removal path** (`SP-FACTOR-1`; `SP-TIER-2` by **C10** — cadical's "eight consumers"
audit down to two live mechanisms; Stage F has since populated the redundant-stamp class as **F6**, and
measured it counterproductive — the extra stamps only weaken a filter every consumer scopes to irredundant
clauses, costing `battleship` **820×** the stamps for **zero** additional skips, so F6 ships OFF).
**`SP-REPEAT-*` is now WE_ARE_CRUDER rather than WE_LACK_IT — Stage F landed the substrate and five of its
six consumers** (F1 C1/C2 · F2 the resume half, with the bound coupling REFUSED on wedge grounds as a THIRD
accepted deviation, `SP-REPEAT-COUPLE` · F3 BCE/HyperTernaryRes/Instantiate · F4 Factor · F6 the redundant
half, shipped off). **What keeps the row open is two named items, both with a written closure path:**
`SCHED-F5` (Vivify's per-CLAUSE leftover bit — routed to `revamp`, since `Clause::flags` has no spare bit and
the per-variable substrate cannot express it) and `SCHED-F7` (C1/C2's scope ← the tier band, which F6 exists
to unblock). Everything Stage F landed is UNPRICED.

- [x] **SCHED-N1FIX · DONE 2026-08-04 — the N1 breach is closed** by MOVING `Factor`'s density predicate out of `factor_apply` into DefaultPolicy's
  slot (beside BVE's identical, N1-clean one), reaching the rule as `Kernel::factor_admit`, default TRUE. Both pre-registered gates passed:
  DefaultPolicy **byte-identical** (same conflicts, factorings and proof SHA on 4 instances including `sted5`, which is BELOW the threshold so the
  gate actually fires there), and `tests/test_factor_n1.cpp` drives the REGISTRY surface and **fails on the pre-fix build**. `test_factor.cpp` no
  longer needs `TACTIX_FACTOR_FORCE` — that workaround was itself the symptom. Whether to KEEP the predicate is now a separate measured question
  (`benchmarks/reports/2026-08-04-factor-oracle-ablation.md`)

#### Stage A — the instrument and the guard rails (no behaviour change; nothing later ships without these)

Causes (D) and (E) of the diagnosis are INSTRUMENT defects — the eval loop selects for gates, and the register has the wrong schema. Fix the
instrument before spending a tier window on a mechanism.

- [x] **SCHED-A1 · per-entry slot telemetry** `[arch]` (`TACTIX_SLOT_STATS=1`, one `c slot` line per entry; diagnostic only, off by default, proofs
  byte-identical). **It carries a SELF-CHECK** — `occasions` must equal `admitted` + every decline reason, else the line prints
  `***UNACCOUNTED***`, because telemetry that is silently incomplete reads as a finding. That check caught two holes in the first cut. First
  readings on `sted5`: `Factor` declined 33/33 by the density advice and `BVE` 30/33 by its dose-density gate — i.e. both elimination-family rules
  were switched off almost entirely, which `c stats` alone could never distinguish from "ran and found nothing"; `BCE` is budget-declined 6/33
  while `Instantiate`/`Vivify` are 22/33, the shared-meter starvation Stage B exists to fix
- [x] **SCHED-A2 · config-wiring conformance test** (`tools/docs/check-config-wiring.py`, ctest `config_wiring`) — classifies all 29 Kernel
  scheduling-config fields POLICY / RULE / ENV / API; **a POLICY-tagged field with no writer in `default_policy.*` FAILS** (the `factor_bound`
  shape, the most common high-severity ledger row) **and an unclassified NEW field fails too**. Both failure modes verified by breaking them.
  The judgement call recorded because it is the substance: `factor_bound` is tagged **API, not POLICY** — wiring it is measured negative three
  times and kissat at `--eliminatebound=0` is fine, so it is an accepted gap awaiting a new mechanism. **ENV rows are the status quo, not the
  target** (e.g. `factor_demote_fresh` was ENV while both references demote unconditionally — since fixed by C5)
- [x] **SCHED-A2b · effort-spec conformance test** (`tools/docs/check-effort-specs.py`, ctest `effort_specs`) — one level up from A2:
  **registering an `Inprocess` rule IS scheduling it**, so each of the 14 scheduled rules must be METERED (an `EFFORT_SPECS` row — 12) or
  ALTERNATE-GOVERNOR (`Congruence`/`Substitute`, whose watermark + delay ladder the checker re-greps, so deleting the mechanism FAILS). A METERED
  row must also be USABLE: non-null meter, non-zero permille, and `dosed || budget_aware` — a share alone bounds how OFTEN a round starts, never
  how much one round does. Four deliberate breaks verified, one re-run THROUGH ctest. **The defect it locks out actually shipped**: V5 Sweep
  registered with no spec ran unmetered on every phase (`9vliw` ~14 s → unsolved at 300 s), and `check-config-wiring` (the mirror image), the unit
  tests (they call `sweep_apply` directly) and fuzz (it is a *performance* regression) all missed it.
  **Open, found while classifying:** `Congruence`'s alternate governor bounds occasions, not work — one application is priced at ~44 s on
  `T105.2.0` (`default_policy.cpp:1044`). Metering it is a task, not an accepted shape
- [x] **SCHED-A3 · regenerate the ledger from the REFERENCE's option set** `[arch]` — **BOTH HALVES DONE.** Enumeration (2026-08-04):
  `docs/implementation-notes.md` §13.4–§13.6 — all **418** options (kissat 161 + cadical 257), **148 newly read at
  their use site**, 83 dismissed, 187 matched to a pass-1 row, **15 new gaps and 6 corrections**.
  **Conformance test (2026-08-05): `tools/docs/check-option-coverage.py`, ctest `option_coverage` (44/44).** It re-enumerates both option sets from
  the reference headers and fails on any option the ledger does not account for: 83 DISMISSED (roster parsed FROM §13.6, globs expanded against the
  live set and every published count re-checked), 124 CARRIED (22 script-side entries for family rows that do not spell members out, each naming an
  anchor that must still exist), 209 CITED (name appears in §13 + the frozen audit + the diagnosis + this track), 2 EXEMPT (`radixsortlim`,
  `terminateint` — sort cutoff and callback cadence, named and reasoned rather than globbed away). **Five deliberate breaks verified**: deleting the
  `SP-BVE-BWD-1` row orphans `elimbackward`; deleting `SP-ARENA-1` fires both the anchor check and two UNACCOUNTED options; a fake `sweepslack`
  option fails; a fake option hidden INSIDE the dismissed `block*` glob fails on the group count; an unknown option-macro spelling aborts rather
  than under-counting. It is a **coverage** check, not a semantic one — §13.5's 6 corrections are the base rate for what it cannot verify.
  **The 4 HIGH gaps A3 surfaced, and where they went — all four are now closed, and THREE of the four had a wrong premise:**
  ~~`SP-BVE-GATE-1`~~ ITE/XOR gate extraction was genuinely absent (BVE charged the full P·N where both references pay P+N−2, the T4
  antidote) — **CLOSED by C8**, the one row that was right as written · ~~`SP-TIER-2`~~ — **CLOSED by C10** as audit + accepted deviation:
  the "eight cadical consumers" are three inert at cadical's own defaults, four instances of the same `mark_added`, and one gating a
  clause class we never schedule; kissat has TWO behavioural consumers and we have both · ~~`SP-EFFORT-CEIL-1`~~ — **CLOSED by E3**, its
  claim that *every* reference budget has an upper clamp WRONG (kissat has no `maxeffort`; our budget is kissat's own macro) ·
  ~~`SP-JUMP-1`~~ — **CLOSED by C9**, its "default-ON in both references, per-conflict, free" billing wrong on all three counts.
  **Standing method note, both traps live in this pass:** **read MACROS, not just grep** — cadical's `factorthresh` is token-pasted
  (`limit.hpp:150` `opts.NAME##thresh`, instantiated `factor.cpp:962`), so the identifier never appears literally and reading it as dead code
  produced two wrong published claims; and **polarity is not inferable from a name** (`factordelay`=0 is the MOST delayed setting)
- [x] **SCHED-A4 · the ORACLE column ships, on by default** — `tools/bench/run.py` reports the per-instance best over the SCORED arms (baselines
  are references, never candidates) plus each arm's gap to it, and persists it to `results.json`. No flag; `--aggregate` recomputes it for any past
  run, and every existing per-arm number is byte-identical on re-aggregation. **Caveat recorded in the code:** an oracle is a min over K arms and
  therefore also a min over K measurement NOISES — adding a third arm that was itself a blend of the other two moved Tier C by 4.00, inside the
  6.72 floor. Judge a gate against the oracle over the policies it chooses BETWEEN, and require it to beat both always-on and a fair share of the
  oracle gap. **This de-selects cause D at the instrument** — the only level at which a selection pressure can be removed

#### Stage B — an honest meter (T3). *Landed; every dose number taken before it is unreliable.*

- [x] **SCHED-B1** ceiling at RUN time · anchor on the ADMITTED path · per-entry `thresh` (`806ce20`+`8b5667c`) — parts 1-2 alone made the batch
  pre-check *unfireable*; part 3 is the fix.
- [x] **SCHED-B2** private per-rule tick meters `Kernel::tick_sink` (`80ec42d`)
- [x] **SCHED-B3** V4 currency → ticks **+ a resumption cursor** (`33940b8`) — the meter alone banked negative twice; `instances()` replayed its
  head every round, so an honest budget bought STARVATION. `Kernel::transitive_cursor` rotates (N1-safe: `apply_instance` still takes any source).
- [x] **SCHED-B4** C1/C2 currency → per-partner-visit (`33940b8`, cadical `subsume.cpp:86,362,459`) — the old meter counted CANDIDATES where the
  cost is the occurrence lists' length. Under-priced by **2712×** (`DivS` Strengthen), 622× (`DivS` Subsume), 238× (`9vliw` Strengthen), which is
  the mechanism behind C2 owning 89.7 % of `DivS` inside four rounds without ever exceeding its budget.
- [x] **SCHED-B5** SwitchMode growth-key off-by-one + `search_ticks` scope (`c33cba8`) — the key was `(c/2)+1` vs kissat's `c/2` (`mode.c:90`), one
  switch AHEAD, so focused mode ran long forever after; and `search_ticks` missed minimize/shrink (`minimizeticks`=1 in the shipped reference).
- [x] **STAGE B GATE (B1–B5), `24eba22`:** Tier B **−13.23** / Tier C **neutral** ex-lottery; both deciders (`sted5`, `j3037`) shown lotteries by
  paired draws. Report `benchmarks/reports/2026-08-04-sched-stage-b-gate.md`. **B3/B4/B5 added no measurable Tier C value** and are kept because
  each closes a demonstrated reference gap — worth saying plainly rather than folding into a green headline.

#### Stage C — the free wires (T1/T5): mechanisms we own and never wired

- [x] **SCHED-C1 · Vivify ← the dynamic tiers** `[wire]` — `VIVIFY_TIER2 = 6` while `k.tier2` was recomputed every reduce and read by NOTHING;
  kissat's `vivify_tier2_limit` reads the dynamic tier whenever `vivifyfocusedtiers` is set, **and that option defaults to 1**. Both scope sites
  now read `k.tier2`; the test pins that moving the tier moves the candidate set. *(Generalisation settled by `SCHED-C10`: cadical's eight consumers
  audit down to two live mechanisms, and cadical's own vivify is on the static 2/6 — `vivifycalctier`=0 — so C1 is kissat's shipped behaviour, not both.)*
- [x] **SCHED-C2 · Factor acceptance bound ← `bve_level_`** `[wire]` — **CLOSED AS AN ACCEPTED DEVIATION, NOT AS CODE** (2026-08-05): the wire is
  refuted three ways (port REVERSED, mp1 5.3 → 14.9 s · no static bound separates winners from losers, bound 8 costs `sted5` 19 → 274 s · with
  composition ON the arms are byte-identical because `bve_level_` is **0 where Factor runs**, so the coupling cannot even fire), and kissat with its
  OWN bound pinned to 0 is fine-or-better (`mod4block` 18.35 → 6.54 s). Row + measurements + removal path (R1 §5.6 admission exhibit · R2 show
  `bve_level_` moved at Factor's occasions BEFORE re-running · R3 SP-BVE-GATE-1 changes what the bound means): `implementation-notes.md` §13.3.
- [x] **SCHED-C3 · BVE bound-ramp advance guarded to phase completion** `[wire]` — the budget half was already there (`!dose_cut`); the FIXPOINT
  half was not. kissat advances only when `complete = !remain && !last_round_eliminated` (`eliminate.c:544`), and the second conjunct matters: a
  pass that still eliminated something has not exhausted the CURRENT bound.
- [x] **SCHED-C4 · split `TACTIX_REDUCE_SQRT` into cadence and ramp** `[wire]` — one env var moved both, so neither was ever priceable. Now
  `TACTIX_REDUCE_CADENCE=sqrt|arith` and `TACTIX_REDUCE_RAMP=0|1`, with the old name kept as a compound alias so banked numbers reproduce.
- [x] **SCHED-C5 · demote fresh extension vars** `[wire]` — landed 2026-08-05 default-ON (`TACTIX_FACTOR_DEMOTE=0` is the off arm) and **PRICED
  NEUTRAL on both tiers**, shipping on reference-parity grounds. **The port had been the MIRROR IMAGE of both references** — our re-insert loop ran
  DESCENDING, putting the OLDEST fresh var at the queue front, so the knob was nearly inert and every pre-08-05 reading of it measured a different
  mechanism (`rbsat`: 415,947 conflicts / 16.4 s descending vs **88,663 / 2.84 s** ascending). The old title's second half, "retire the invented
  gates (T4)", is **scoped out and NOT done** — of the five T4 rows two are refuted by the reference option set and one is misfiled (ledger
  §13.5 C7/C8), and the three that survive are the binary-density family, whose retirement IS the broad-admission experiment (verdict HOLD), i.e.
  item (b′), not a Stage C wire.
- [x] **SCHED-C6 · eager subsumption of learned clauses** `[arch]`, ships ON at `eager_subsume`=4 (kissat's own default); `TACTIX_EAGER_SUBSUME=0`
  is the A/B arm, source counter `c eagersub`, **UNPRICED**. **NOT a rule, and that is the substance:** it deletes only REDUNDANT clauses, whose
  deletion S2 `Forget` already licenses unconditionally, so the subsumption test SELECTS victims rather than licensing the deletion — a registry
  entry would put a heuristic in a guard. It lands as a sub-step of M3 `ConflictAnalysis` (the seat OTFS subsumption already occupies), so **no
  `EFFORT_SPECS` row is owed**. kissat's 4-slot shift register, not cadical's backwards walk over `clauses` — a flat variable-record arena cannot
  be walked backwards at all.
  **▶ THE FINDING WORTH KEEPING — WHERE IT RUNS IS THE WHOLE RESULT, and no on/off A/B could see it.** The obvious call site (the tail of
  `analyze_apply`, the one chokepoint every driver shares) is nearly worthless, and provably so: `kernel_assign_ref` stamps a forced literal at the
  max level of its reason's other literals, and a candidate `C ⊋ D` contains `D`'s UIP, so `C`'s forced literal sits at or above the conflict level
  and is ALWAYS still a reason there. Measured over 60 Tier A instances: pre-backjump **8,297 deletions / 28,000 reason-skips**, and a
  freed-by-backjump probe of **28,000 — all of them**; moved into `backjump_apply_to` (kissat's own position) it is **30,613 deletions / 0
  reason-skips**, firing on 48 of 60 instances. **A 3.7× swing from placement alone.**
  **One deviation from kissat that cannot be dropped:** we skip a candidate that is still a trail reason. Kissat does not check; we must, because a
  garbage reason breaks `kernel_collect_garbage` (corpus 003) and puts a DELETED id into the next LRAT chain — the N2 half. By the theorem above
  DefaultPolicy reaches that branch **zero** times, so **no fuzz or tier leg can cover the guard that protects N2**; it is pinned by
  `tests/test_analyze.cpp::test_eager_subsume` case (c) instead ([[ab-arm-identical-artefacts]]). Docs: `implementation-notes.md` §15 + M3's
  calculus entry
- [x] **SCHED-C7 · random decisions (`randec`)** `[arch]` — `H`-only, no proof delta, no calculus entry (the Rephase/Reorder category). Ported from
  `decide.c:57-113` with the option defaults READ rather than assumed: `randec=1`, `randecfocused=1`, **`randecstable=0`**, so sequences run in
  FOCUSED mode only — the half a "kissat does random decisions" summary gets wrong. Tracked as an END CONFLICT rather than a countdown
  (equivalent, and needs no hook on the conflict path); splitmix64 with a fixed seed, because a random policy that is not replayable is not
  testable. Reachability measured: `9vliw` 90 sequences / 648,250 random decisions, `rbsat` 226 / 305,053, both still solving. **UNPRICED, and it
  has NO KNOB** — see the anchor.
  *(C6 and C7 moved here from Track R · R8. kissat's own single-technique ablation prices them at +5.9 and +7.9 PAR-2 — treat those as CEILINGS,
  since the survivors of a single ablation over-invest.)*
- [x] **SCHED-C8 · BVE gate extraction ← ITE + XOR** `[wire]` — closes HIGH ledger row **`SP-BVE-GATE-1`** (§13.2). E1c had shipped gate
  extraction with a `--bve-gates` knob but only **EQUIV + AND/OR**; ITE is default-ON in BOTH references and XOR default-ON in cadical, so an
  ITE/XOR-defined variable was charged the full P·N and failed `resolvents ≤ pos+neg+bound` where both references eliminate it. Now
  `--bve-gates=all|none|equiv,andor,ite,xor`, both new types default-ON, and the row is an ACCEPTANCE lever: gate clauses g⁺/g⁻ turn P·N into
  g⁺·(N−g⁻) + (P−g⁺)·g⁻ (ITE: 2P+2N−8 · XOR arity m: 2^(m−1)(P+N−2^m)).
  **This is the SYNTACTIC half only.** kitten-based *definition* extraction (kissat `definitions`=1) is phase 4 / Track R · R5 and is NOT in scope.
  **Proof delta unchanged (RUP-only), and the obligation is discharged the same way as E1c's:** both new shapes are COMPLETE gates — their clauses
  alone entail `x ↔ φ`, and every gate×gate resolvent is a tautology (ITE clashes on the condition or a branch; two opposite-pivot parity clauses
  must disagree on some input), so `resolve_gate` stays false and nothing but entailed resolvents is skipped. The antecedent×antecedent argument
  is written out in `calculus.md` §6b E1. Detection is EXACT: kissat's shortened-clause stand-in (`ifthenelse.c match_ternary_ref`, which then
  re-enables gate×gate) is deliberately not ported. Work bounds are the references' own — kissat's `nter² > occlim` for ITE, cadical's
  `elimxorlim`=5 as `BVE_XOR_ARITY_LIMIT`.
  **Reachability (pre-registered, checked FIRST — [[ab-arm-identical-artefacts]]):** counters `c bve-gates … ite … xor; … resolvent pairs saved`.
  Probe `benchmarks/probes/bvegate-reach.sh`, 120 fuzz-population cases under `--bve-stress-policy`: **ITE 47 · XOR 318 · 5215 resolvent
  pairs saved** (equiv 63, andor 244). The fuzz circuit generator gained an **ITE op** (`generators.py` gen_fuzzsat, 4-clause MUX) so the ITE path
  is not fuzzed vacuously — XOR was already reachable through its XOR op.
  **Fuzz GREEN on four legs — both proof formats × two schedules** (600 cases each + the 14-entry corpus each,
  ASan/UBSan debug build): `--bve-stress-policy` native DRAT **0/600** (SAT 324 / UNSAT 196) · `--bve-stress-policy
  --lrat` **0/600** (SAT 326 / UNSAT 195) · `--random-policy` native DRAT **0/600** (SAT 302 / UNSAT 170) ·
  `--random-policy --lrat` **0/600** (SAT 300 / UNSAT 169). Every UNSAT proof-checked, every SAT model-checked,
  `tools/fuzz/failures/` empty. ctest **44/44** including `micro_proofs` and `sweep_proofs`.
  **UNPRICED — the A/B is specced and unrun; see the Log entry.**
- [x] **SCHED-C9 · `jumpreasons` (SP-JUMP-1)** — **PORTED, ships OFF** behind `TACTIX_JUMP_REASONS=1`; counter `c jumpreason`
  (`stats.jumped_reasons`). Row + closure record: `implementation-notes.md` §13.2. **Three of the row's premises were wrong.** (a) It is
  **kissat-only** — `grep jumpreasons cadical/src` is empty, and cadical is the reference that also emits LRAT. (b) The propagation-site gate is a
  formula-SIZE gate, not the binary-fraction one: `classify.c:9` sets `bigbig=false` for `clauses <= smallclauses`=**1e5**. (c) **"Free" is false
  here, and the reason is N2**: a jumped reason names a clause the checker does not hold, so `chain_record`, `kernel_reason_walk` and the
  delete-binary live-reason guard (×2) all expand it back into its two real antecedents. **And the expansion cannot be done at record time** —
  doing it recursively there was fuzz-RED (12/1500 native-LRAT rejections): whether the resolved-away literal needs an antecedent depends on the
  FINAL clause, and if it lands in the clause its reason is a **satisfied hint**, the same trap the shrink UIP is already guarded against. The
  shipped form defers to `chain_expand_jumped`, run from `chain_emit` against the final clause; it iterates, so jump CHAINS still work.
  **This IS the phase-1 revert** (`e26492b`, "binary-reason jumping (kissat jumpreasons)"): `summle` −27 %, `9vliw` **+2×**. Its stated escape
  hatch — "needs the bigbig classifier" — is **refuted by (b)**: `9vliw` is 179,492 clauses / 88.8 % binary, so kissat's gate is ON there and the
  classifier would not have saved it. What is left is one laptop-era single draw on a kernel with no chrono/OTFS/reason-side bumping/dynamic
  tiers/inprocessing, so it ships OFF and gets re-derived rather than banked as a deviation. **Counters, so no arm is vacuous:** OFF is
  byte-identical to the pre-port binary (47 fuzz CNFs, 31 with proofs, 0 diffs, `c jumpreason` = **0**); ON reads **78,231,117** over 52 and moves
  conflicts on 7; under `--random-policy` it reads **6,618** over 30. Fuzz ON, seed 20260805, 1500 cases each, 0 failures: native DRAT (614 UNSAT) ·
  native LRAT (615 UNSAT) · `--random-policy` DRAT (488 UNSAT); corpus 14/14 both ways; ctest 44/44 release and ASan/UBSan debug. **A second seed
  is owed** — the standing expectation for a proof-path change — and belongs with the A/B window, since HEAD's own proof path is unchanged.
  **No `calculus.md` change, and that is a claim, not an omission:** M3's precondition, effect and proof delta are untouched — the jump
  only changes WHICH existing clauses the RUP chain cites, never what is added or deleted. It is not `H`-only either (unlike C7 it moves the LRAT
  chain), which is why the engineering price is written down in `implementation-notes.md` instead of being waved through.
  **▶ THE A/B THAT DECIDES THE DEFAULT (pre-registered, not yet run).** Same-window **Tier B + Tier C**, two pin dirs from ONE commit with matching
  binary shas (CLAUDE.md build-dir rule), node-pinned, `--baseline kissat --baseline cadical`. Arms, knob IN THE COMMAND:
  `A0 = build/pin-jump-b/tactix` (shipped, OFF) · `A1 = env TACTIX_JUMP_REASONS=1 build/pin-jump-b/tactix`; Tier C the same out of
  `build/pin-jump-c/`. **Pre-registered counter, checked FIRST: `c jumpreason` must be 0 on A0 and > 0 on A1 for every instance** — an arm whose
  counter did not move was not tested ([[ab-arm-identical-artefacts]]). **Ship criterion:** A1 ships ON only if it is non-worse on BOTH tiers
  (#solved first, then PAR-2 beyond the 6.72 floor) **and** `9vliw_bp_mc` — the instance that decided the phase-1 revert, and the specific
  falsifier here — is not regressed; a single-instance headline is not a verdict, so a decider gets paired draws ([[chaotic-instance-veto]]). If
  A1 loses, the row converts to an accepted deviation in §13.3 with THIS measurement, and the removal path is a per-formula gate (kissat's
  `bigbig`), which needs a classify call in DefaultPolicy — Stage D territory, not a Stage C wire.

- [x] **SCHED-C10 · the dynamic-tier consumer audit (`SP-TIER-2`)** — closes the HIGH row as an **audit + accepted
  deviation** (`implementation-notes.md` §13.3), not as a wire, and the audit is the result. **The "one consumer here,
  eight in cadical" headline does not survive reading the eight at their use sites:** `walk.cpp:858` and
  `walk_full_occs.cpp:673,758` are unreachable at cadical's shipped defaults (`walkredundant`=0, plus `walkfullocc`=0 for
  the pair), `clause.cpp:140`/`:237`/`sweep.cpp:1165`/`decompose.cpp:630` are the SAME action (`mark_added`), and
  `subsume.cpp:388` gates a clause class our C1/C2 never schedule. Of the direct reads the row also listed, `reduce.cpp:35`
  is the flush pass (`flush`=0) and **both `promote` sites read the tier only for a LOG string and `stats.promoted1/2`** —
  in kissat too (`promote.c:11`), so there is nothing there to wire. **Polarity finding in the other direction:** cadical's
  vivify is on the STATIC 2/6 by default (`vivifycalctier`=0); it is kissat's `vivifyfocusedtiers`=1 that makes SCHED-C1
  the reference's shipped behaviour, so C1 was right about one reference of two. Also recorded: kissat's live macros are
  `TIER1 = tier1[0]` but `TIER2 = tier2[**1**]` (`internal.h:255-256`, the `focusedtiers` form `#if 0`'d out).
  **Landed as code:** `Stats::tier2_band_dynamic/static` inside the aging walk `reduce_apply_refs` already does (free), a
  `c tiers` stats line, and `TIER1_STATIC`/`TIER2_STATIC` named in `reduce.hpp`. Behaviour-neutral — counters only.
  **The blast radius is real, which is why this is not a "the gap is small" excuse** (probe `1584747`,
  `benchmarks/probes/scratch/tier2/`): `battleship` live tiers **26/34**, **1,025,943 redundant clause-visits admitted by
  the dynamic tier vs 0 by the static 6**; `9vliw_bp_mc` 3/10, +35.5 %; `rbsat` 2/6, +3.3 %; `mod4block` 1/6, +0.2 %.
  **Why closed anyway:** every remaining consumer filters a set we do not populate — SCHED-F1's `touched` stamps only
  IRREDUNDANT creations and `subsume_forward.cpp:44`/`strengthen.cpp:24` reject every redundant clause — so wiring the
  tier now filters the empty set ([[ab-arm-identical-artefacts]], predicted rather than discovered). Removal path = three
  named triggers with pre-registered counters in §13.3: **T1** F1's producers gain the redundant path (then the admission
  test is the full three-disjunct predicate, incl. the survivor envelope `lim.keptglue/keptsize` we lack) · **T2** C1/C2
  gain a redundant scope · **T3** Vivify gains kissat's tier1/tier2/tier3 buckets — **we never vivify tier3** although
  `default_policy.cpp:1481` sizes the redundant share as kissat's "3+3+1 = 7". T3 is the only non-statistics,
  non-incremental `k.tier1` consumer either reference has outside reduce.

#### Stage D — the authored schedule and the two clocks (T6 + the global slot rows)

- [x] **SCHED-D1 · an explicit, authored phase order AS DATA** `[wire]` — landed 2026-08-05. `AUTHORED_SLOT_ORDER` (a table
  `default_slot_order()` exposes) replaces `strcmp` + `move_to_back`; membership stays registry-derived and an unauthored rule is appended, with
  `test_scheduling` failing the build if one exists. Fixes all four SP-SLOT-4 accidents at once: Substitute (ELS) now precedes Backbone/Probe/
  TransitiveReduce/Factor, Congruence opens the pass and feeds Substitute, Vivify precedes TransitiveReduce, BCE closes instead of opening.
  **UNPRICED**; arm `TACTIX_SLOT_ORDER=legacy`, source counter the `c slot-order` line (`authored`/`legacy` + the live sequence).
- [x] **SCHED-D2 · split the slot into a PROBE group and an ELIMINATE group** `[arch]` — landed 2026-08-05. `SlotGroup` + two `SlotClock`s;
  the phase body is now `run_slot_phase(k, group, stop)` and the search loop checks probing first (kissat `search.c:214-217`, cadical
  `internal.cpp:322-325`). Probe clock = `probeint`100·NLOGN, eliminate clock = `eliminateint`500·NLOG2N, both size-scaled — kissat's formulas, not
  cadical's near-constant `probe.cpp:980` spacing, so only ONE thing changes. **The "5×" in `SP-SLOT-1` is the BASE-CONSTANT ratio, not the realized
  one**: the cumulative NLOGN-vs-NLOG2N ratio is ~2.4× and grows only as √log, and **kissat itself runs 11 probings vs 5 eliminations on `sted5`**
  where we now run 12 vs 6 (one clock gave the probing family 6). Group mapping is the references' own membership, argued row by row in the table.
  **UNPRICED and expected to move numbers** — arms/spec in the Log entry; source counter `c slot-clock`.
- [x] **SCHED-D3 · slot-level new-material watermark + the reduce coupling** `[arch]` — landed 2026-08-05, one gate per clock: ELIMINATE waits for
  new material (`Stats::bve_marks` or new root-fixed vars since phase entry — kissat `eliminate.c:33-38`, cadical `elim.cpp:75-80`), PROBE waits for
  a reduce since its last phase (cadical `probe.cpp:23`). First phase of each clock exempt. **Both are SAFETY VALVES and measured near-inert**
  (`blocked` counters below) — `test_scheduling::test_slot_gates_decline_without_new_material` is what proves the mechanism, because a tier run
  cannot tell "correct and quiet" from "wired to nothing". **The one real finding is a measured trap:** counting kissat's/cadical's mark-bit FLIP
  instead of the mark EVENT wedges the elimination clock shut for the rest of the solve here — our `bve_retry` bit is cleared only by a BVE attempt
  and BVE's admission can decline every dose, so no flip ever happens. `sted5` went 274 k → 1.15 M conflicts, 6 elimination phases → 1. Recorded at
  `Stats::bve_marks`.

#### Stage E — order and resumption become POLICY OBJECTS (the architecture the RL agent needs)

- [x] **SCHED-E1 · `CandidateOrderPolicy` sub-policy + generalized `SlotEntry::cursor`** `[arch]` — landed 2026-08-05, two commits.
  **(1) The 660-line `run_slot_phase` Stage D's report nominated for breakup is split** along the effort ledger's own DOSED/BATCH axis
  (`slot_entry_admissible` · `slot_entry_thresh_declines` · `run_dosed_entry`+`bve_dose_declined` · `run_batch_entry`+`factor_entry_declines`/
  `factor_dose_config`/`run_budgeted_batch`/`run_vivify_dose`), phase state made explicit in `SlotPhaseCtx`, control flow in `SlotStep`. Pure
  restructuring, gated the way this project gates a pure move: **byte-identical DRAT + identical trajectory + identical per-entry `c slot`
  telemetry on 4 Tier B instances** (job `1584689`, probe `e1-split-identity.*`), sted5 included so Factor's declining path is exercised.
  **(2) `CandidateOrderPolicy` joins the SubPolicy surface** (the FIFTH category — the H6 super-class it would have needed already landed with
  S1b, so the answer was reparent, not invent), and **SCHED-B3's `Kernel::transitive_cursor` becomes `SlotEntry::cursor`**, off the rule and
  onto every dosed entry that opts in (Probe/Backbone/TransitiveReduce ON, BVE OFF — its schedule is occ⁺·occ⁻-ordered, not key-ordered, and it
  already owns `bve_retry`). Arm `TACTIX_CURSOR_<RULENAME>=0|1`; counter the `c cand-order` line.
  **▶ THE MIGRATION IS EXACT AND THE GENERALIZATION IS NOT INERT** (job `1584716`, probe `e1-cursor.*`): with the two NEW cursors off, all four
  instances are **byte-identical** to the pre-E1 build, so moving B3's cursor rule→policy changed nothing; with them on, **Probe rotates on 44 of
  45 doses and advances 2,719 of 878,094 enumerated candidates (0.3 %)** on `9vliw` — i.e. before this, Probe re-probed the same ~60-literal head
  on every one of 45 phases and never saw the other 99.7 %. Backbone 3.6 %. That is B3's starvation finding at a far larger scale, and it is the
  reason the item exists. **UNPRICED** — conflicts move both ways on the 4 probes (`b22_1` −6.8 %, `FmlaEquivChain` −2.4 %, `sted5` identical,
  `9vliw` +4.9 %; proofs shrink on three of four); A/B spec in the Log entry.
- [x] **SCHED-E2 · the ordering arms, one per rule, each independently gated** `[wire]` — landed 2026-08-05. Six arms behind
  `TACTIX_ORDER_<RULENAME>` (spelled from the registry name, so a newly dosed rule gets its own knob for free): `native` (**the shipped default
  everywhere and it stays there — no arm ships without a measurement**) · `reverse` · `random` (seeded splitmix64, replayable) · `occ-asc` ·
  `occ-desc` · `negocc-desc` · `heuristic`. Three are reference ports read at the use site: **kissat `transitive.c:270-294` sorts its transitive
  probes by the untried mark then EVSIDS score (stable) / VMTF stamp (focused) ascending** — we enumerate by literal index — and **cadical
  `probe.cpp:621-624,685,763` probes the literal with the MOST negated binary occurrences first** (rank + pop-from-the-back, which reads backwards
  from the comment alone). A knob on an entry the policy never orders **aborts** rather than being ignored ([[ab-arm-identical-artefacts]]).
  **REACHABILITY PROVEN AT THE SOURCE** (job `1584734`, probe `e2e3-arms.*`): on `9vliw` all four arms reorder — Probe `negocc-desc` 718,018 of
  718,068 positions moved, Backbone `occ-desc` 717,132, TransitiveReduce `heuristic` 436,819, BVE `occ-asc` 148,409; BVE reads `lists 0` on
  `b22_1`/`sted5` because it is never dosed there, and the counter says so instead of the arm passing vacuously. **UNPRICED** — the four-arm bundle
  moves `9vliw` 348,630 → 234,825 conflicts and its proof 64.0 → 43.4 MB, but that is ONE instance and a BUNDLE; the A/B spec in the Log entry
  prices one rule at a time.
- [x] **SCHED-E3 · the effort budget's UPPER clamp (ledger `SP-EFFORT-CEIL-1`)** `[wire]` — landed 2026-08-05, added to Stage E because an entry
  whose window can grow without bound is never cut, and an entry that is never cut makes E1's ordering question moot for it. **The row's headline
  was wrong and reading the use sites is what showed it:** there are THREE reference schemes — kissat `SET_EFFORT_LIMIT` (`kimits.h:135-170`,
  **floor only; kissat ships no `maxeffort` option at all**, and ours is a transliteration of exactly this), cadical's ticks macro (neither), and
  cadical's PROPAGATIONS scheme, the only clamped one. That scheme is an ORDERED composition — `max(mineff)` → `min(maxeff)` → `max(2·active())`
  — whose LAST step is a floor that can lift the delta back above the ceiling (`elim.cpp:781-787`, `subsume.cpp:351-357`); reading it as a clamped
  range gets it backwards, and `test_scheduling::test_effort_ceiling` pins the order. Ported in that order to the entries whose currency matches a
  clamped cadical technique: BVE↔elim 2e9 resolutions, SubsumeForward/Strengthen↔subsume 1e8 partner visits. TransitiveReduce deliberately gets
  none — cadical's 1e8 is PROPAGATIONS and our meter is a cache-line proxy. **MEASURED INERT and that is the result** (job `1584734`): the shipped
  constants give `clamped 0 floored 0` on every entry of `9vliw`/`b22_1`/`sted5`; forced to 1e5 the clamp fires 18/21/13 times, **and `b22_1` and
  `sted5` are still byte-identical**, so those rounds never spend to their limit either. It ships ON (one compare per occasion) on parity grounds
  with the inertness recorded, not as a green headline. Arms `TACTIX_EFFORT_CEIL=0` / `TACTIX_MAXEFF_<RULENAME>=N`. **Not closed by this:
  Congruence/Substitute still have no meter at all** — that is `SCHED-A2b`'s open finding and the 131,327 ms Congruence application belongs to it.

#### Stage F — repetition suppression (T2): the touched-epoch substrate

- [x] **SCHED-F1 · the substrate + its first consumer (C1/C2)** `[arch]` — landed 2026-08-05. `Kernel::touched`
  (a per-variable EPOCH STAMP, not a bit) + `touch_epoch` + `kernel_touch_sync_fixed`; C1/C2's canonical rounds
  filter their candidate list with `kernel_clause_new_material` and advance their own watermark only on a round
  that completed. **UNPRICED**; off arm `TACTIX_TOUCH_MARKS=0`, source counter the `c touch` line.
- [x] **SCHED-F2 · C1/C2 completeness + resume** `[arch]` — landed 2026-08-05. A cut round RE-ARMS the
  candidates it never reached (kissat forward.c:589-645) instead of blocking its watermark, so the next
  round resumes at the tail; `Kernel::subsume_complete`/`strengthen_complete` expose the fixpoint.
  **The C1→BVE BOUND coupling T5 asks for is REFUSED**, with the argument, at `implementation-notes.md`
  §13.3 `SP-REPEAT-COUPLE`: neither reference does it, and the naive conjunct is self-wedging (BVE's
  resolvents are C2 material, so a productive C2 would hold the bound at 0 forever — SP-SLOT-5's failure
  exactly). The reference's REAL wire (an incomplete C1/C2 holds the elimination outer loop open) landed
  instead, off arm `TACTIX_SUBSUME_HOLD=0`, and is **measured INERT** — sentinel `c subhold` = 0.
- [x] **SCHED-F3 · X1/X2/X4 repetition suppressors** `[arch]` — landed 2026-08-05. BCE, HyperTernaryRes and
  Instantiate now filter and resume their canonical rounds on the substrate. BCE forced the substrate's
  second half: it is the one consumer that needs DELETIONS (removing an opposite occurrence is what blocks
  a clause), so `Kernel::removed` is a second stamp array rather than folding deletions into `touched`,
  which would have weakened C1/C2's filter where the measured saving is. **The delay ladder for metered
  entries is DEFERRED, not done** — it lives in DefaultPolicy's slot loop (`!e.meter`) and extending it to
  metered entries is per-entry productivity bookkeeping, i.e. Stage E's file and Stage E's shape.
- [x] **SCHED-F4 · Factor per-literal marks** `[arch]` — landed 2026-08-05. kissat's `flags->factor`
  (inline.h:60-66, consulted factor.c:922) on the substrate: the seed heap is built only from literals
  with new material, instead of an O(live literals) pass every round whose `score_of` charges
  `factor_ticks` proportional to each watch list — i.e. the round was spending its own budget rebuilding a
  heap that mostly could not have changed. Coarser than kissat on polarity (our stamps are per variable).
  All three of the round's early exits are reachable, so the F2 re-arm is live here.
- [x] **SCHED-F5 · Vivify leftover prioritization** `[arch]` — **DONE 2026-08-05 via `SCHED-G3`**, which is
  where Stage F routed it. The finding that sent it here stands: kissat's `Clause::vivify` is a per-CLAUSE
  PRIORITY bit, the stage-F substrate stamps VARIABLES, and re-arming a cut round's tail over variables
  arms every clause over them, so the partition goes inert. **The `Clause::flags` bit WAS the right
  answer** — `glue`'s 24 bits saturate in `set_glue` and glue is bounded by clause size, so bit 8 is free
  and 23 bits (8.4 M) is as unreachable as 24 was; the side-table alternative was correctly rejected,
  since the arena `Ref` is not GC-stable and the only GC-stable name a clause has is its proof id (a hash
  lookup for one bit). Semantics are kissat's `vivify.c:265-302, 1311`: armed on every scheduled
  candidate, cleared the moment one is TRIED, and **re-armed across the whole scope when the leftover set
  empties**, so it can only prioritise, never suppress. It is also the first resumption Vivify has ever
  had — a BATCH rule, so Stage E's `SlotEntry::cursor` never applied to it and every round restarted from
  the same head, the defect E1 found in Probe. Reachability is a PAIR, not one count
  ([[ab-arm-identical-artefacts]]): `c vivify2` prints `prio` and `rearm`, and on `Ptn-7824-b18` only
  **100 of 423 rounds** re-armed, so on the other 323 the bit really did select a tail.
- [x] **SCHED-F6 · the REDUNDANT half of the substrate** `[arch]` (SP-TIER-2's residue, requested after C10) —
  landed 2026-08-05, **SHIPPING OFF** behind `TACTIX_TOUCH_REDUNDANT=1`. `kernel_likely_kept` is cadical's
  `likely_to_be_kept_clause` with all THREE disjuncts, including the one we had no counterpart for — the
  reduce **survivor envelope**, now published as `Kernel::kept_glue`/`kept_size` out of `reduce_apply_refs`
  (cadical `reduce.cpp:147-156`). **OFF is the argued default, not an oversight:** extra stamps only ever
  WEAKEN a round filter, and every stage-F consumer scopes itself to irredundant clauses, so with no
  consumer for the redundant band this would be a measured cost buying nothing. **The pre-registered
  anti-vacuity pair is counted unconditionally** — `c touchred` prints `touch_redundant_dynamic` vs
  `touch_redundant_static6`, so `dynamic > 0` proves the stamps fire and `dynamic != static6` proves the
  scope MOVES with `k.tier2`; a single count could show neither. Pinned by
  `test_subsume.cpp::test_touch_redundant_scope`, which covers all three disjuncts separately.
  **The check PASSED** (probe `1584772`): `battleship-14-26-sat` **345,038 dynamic vs 0 static-6**,
  `9vliw` 236,570 vs 49,229, `rbsat` 265,255 vs 147,855 — so F7's set is large and, on `battleship`,
  did not exist at all before C10's dynamic tiers.
- [ ] **SCHED-F7 · C1/C2 scope ← the tier band** `[wire]` — the CONSUMER half, and F6 is its prerequisite.
  `subsume_forward.cpp:44` / `strengthen.cpp:24` reject every redundant clause outright; cadical uses
  `likely_to_be_kept_clause` as the scope test instead (`subsume.cpp:388`), so redundant clauses inside the
  band are candidates. Soundness is not the obstacle — a redundant clause is an unconditionally safe VICTIM
  (S2 `Forget` licenses its deletion with no side condition), and the redundancy check that must stay is on
  the SUBSUMER (`subsume_forward.hpp`'s binary-subsumer argument). **Do not land it before F6's stamps are
  shown non-zero on a real instance**, or it filters a set nobody populates and the arm comes back
  byte-identical, which is the exact trap C10 declined to walk into.

#### Stage G — the genuine revamps (each needs its own design pass)

- [x] **SCHED-G1 · Congruence: budget + honest pricing** `[revamp]` — **the metering half is DONE; the
  value-driven closure is SPLIT OUT to `SCHED-G4` with a diagnosis that changes what it should be.**
  Design (frozen) `docs/design/2026-08-05-g1-congruence-governor-design.md`; ledger row `SP-CONG-BUDGET-1`.
  **The A2b headline was stale**: one application is **8.25 s**, not 44 s — but 17 of them are
  **140,303 ms = 47.1 %** of a 300 s `T105.2.0` solve, still the largest single cost in the codebase.
  **Profile (`TACTIX_CONG_TIME`, stable to ±2 % across all 17 rounds):** extraction **57 %** — of which
  **ITE extraction alone is 27 %**, yielding 2,280 gates and **zero merges** on this instance — closure
  25 % (its table/`occ` half is 19.5 %, four times `canonicalize`), and **gate-vector teardown 16.6 %**
  (~4.9 M `CongGate`, each owning heap vectors). The old attribution ("extraction plus the index sort")
  is right about the majority and **misses the teardown and the table half**; **proof emission is
  0.006 %** and is not a target. **Governor: METERED *and* closure-gated** — 100 ‰, `congruence_ticks`,
  `budget_aware`, opening round exempt (`CONGRUENCE_INIT_TICKS`, Factor's shape) because the profile
  says that is where the yield is (`hwmcc17miters`: ONE 2.97 s round, 710,722 merges, instance refuted).
  `EffortSpec`/`SlotEntry` gain a `closure` flag and the two `!e.meter` declines are re-keyed to it, so
  the watermark + ladder still bound how OFTEN a round starts while the ledger bounds how much one does;
  Substitute keeps both through the no-row default. **`T105.2.0`: 47.1 % → 4.4 % of the solve with 72 %
  of the merges retained.** Conformance table moved ALT → METERED with a third evidence regex, four
  deliberate breaks re-verified. **Do not quote "44 s" again, and do not quote the 2,231-vs-14 unit gap
  as a closure weakness — see G4.**
- [ ] **SCHED-G4 · Congruence's UNIT channel — three mechanisms, not one** `[arch]` — what `SCHED-G1`'s
  diagnosis found under the 2,231-vs-14 gap. The recorded mechanism ("value-driven closure") is not
  the explanation on its own: **no merge we accept has a root-assigned participant on any instance
  measured** (`val_merge` 0 everywhere; `val_gate` 2,071 of 19.06 M items on `T105`). Reading kissat's
  `kissat_congruence` end to end splits the gap three ways — **(Y1)** `find_units`
  (`congruence.c:4029-4080`), a binary-watch pre-pass we **do not have at all** (`(l∨o)` + `(l∨¬o)` ⇒
  `l`; nothing to do with gates; V1 Probe finds these as failed literals but reached 13,686 of 6.19 M
  literals on `T105`); **(Y2)** the value cascade, which is self-reinforcing and empty for us **because
  Y1 is what seeds it**; **(Y3)** `find_equivalences`, kissat's binary half — **for us that is V3
  Substitute**, so part of the 2,231 is an accounting overlap, not a gap. **Do Y3 first: it is free**
  (re-derive the 2,231 at `--congruencebinaries=0`) and it may shrink the target by most of itself.
  Y1 is small and one-step RUP; Y2 changes the closure's relationship to `τ` and is the large one.
- [x] **SCHED-G2 · C1 vs C2: is C1 doing anything?** `[wire]` — **ANSWERED 2026-08-05, and the answer is NO.**
  Over 7 instances C2 absorbs **698,341 of C1's 698,351** subsumptions (**residual 10**, all on one
  instance), and dropping C1 would save **2.7–41.6 % of the C1+C2 pair's metered cost** at zero yield
  (**19–42 % on the four instances where C1 has non-trivial yield**; the low end is an instance where C1
  deletes 36 clauses in total). C1's own effort share is duplicated work. **The follow-up is a DOSE, not a
  rule-off** ([[no-rule-off-configs]]) — A/B spec in the Log entry, unrun.
- [x] **SCHED-G3 · Vivify cross-candidate trail reuse** `[revamp]` — **DONE 2026-08-06** (`d10200a`).
  The reuse LOOP was never the mechanism; the ORDER is (cadical `vivify.cpp:35-42`), so the probe and
  candidate orders land with it and the off arm reverts both. Validity invariant: every DB mutation is
  preceded by `reset_to_root()`, so no trail crosses a change. Also took F5's `Clause::flags` bit and
  C10's tier3 hand-off. Reuse 24–27 %, exactly 0 with the hatch off. **UNPRICED** ·
  [ ] **SCHED-G5 · binary-first watch layout** `[revamp]` — NOT STARTED. *(V5 Sweep is the largest block of reference scheduling we lack, but it is
  already tracked as **Track R · R5** — not repeated here as a second checkbox.)*

#### Stage H — recorded residuals (cheap, additive, no dependency)

- [x] **SCHED-H1** termination polling — **COMPLETE 2026-08-06, all three shares.** X1/X2/X4 (`d234ffa`,
  six sites), Vivify (`d10200a`, with G3), C1/C2 (`c7beaa0`, six sites incl. the `*_try` pair, because
  `work_budget` is a round device and an N1 policy driving `apply_instance` could not be stopped by
  either governor). **No `!(++tick & 255)` window anywhere** — kissat's own shape, and the one
  construction the task-#10 vacuity cannot recur in; removing a cadence the C1/C2 cut had reintroduced
  recovered 13–14 % of reachability. Counters `c interrupt2`/`c interrupt3` increment inside
  `kernel_rule_interrupt`, so a site cannot be added uncounted. **`battleship-14-26-sat`'s
  7.3 s post-SIGTERM exit is NOT C1/C2** — ruled out by counters, see the C1/C2 entry.
  · [x] **SCHED-H2** per-solve reset — **DONE 2026-08-06.** Fifteen state groups audited against the
  references' use sites; most leaked. Worst: `inprocess.{initialized,ticks}` pairs with the kernel's
  single tick meter, so on a fresh kernel `search_ticks()` UNDERFLOWED. `DefaultPolicy::reset_schedule`
  RE-BASES rather than zeroes, so one reset serves a fresh formula and an incremental continuation.
  Pre-fix failure was stark — solve #2 on a reused policy vs solve #1 on a fresh one: **300 vs 3,583
  conflicts, 7,033 vs 165,241 proof bytes**, the reused arm skipping preprocessing entirely. ctest
  `solve_reset`; a second guard proves the shipped SINGLE-solve path stays bit-identical, which is why
  no A/B is owed. **This was an N1/§5.6 correctness bug**: a learned policy runs many solves per
  process, so a leak makes its reward depend on episode order · [x] **SCHED-H3** Reduce
  `first_reducible` scan bound — **CLOSED, measured-marginal, NOT implemented.** The bound binds on
  40.2 % of 59 GB of arena words, but those words are **0.15 % of propagation traffic at the median,
  3.25 % at the worst instance**. Deciding finding: kissat's watermark is stable only because its
  collector re-partitions the arena every reduce; ours copies live clauses in order and never
  partitions, so **11 of 57 instances measure under 1 % skippable**. Counters shipped so the claim
  stays auditable; *fusing the three arena walks* is the better-value change and needs no invariant

**▶ WHY THIS SERVES PHASE 6 (the user's explicit requirement).** Every `[arch]` item adds *observability or control* the learned policy needs and
cannot get today: A1 gives per-entry occasions/declines/yield (the reward-shaping signal, and the labels the §5.6 admission exhibit lacks); B2's
private meters make per-rule cost attributable, without which no credit assignment over rules is possible; E1 makes candidate ORDER a policy
object — and under a cutting budget **ordering IS the scheduler**; F1's touched-epoch substrate lets an agent ask "what is new since I last
acted?". **None of it narrows `instances()` or touches a guard**, and the one genuine N1 breach in the codebase is *repaired* by this track.

### Other phase-3 items

*(The `(a)`/`(b′)`/`(c′)` labels other entries refer to are defined here — they are the Factor-admission /
T3 thread, not a separate track.)*

- [x] **(a) broad Factor admission, re-tested at HEAD — HELD** (2026-08-04; report
  `benchmarks/reports/2026-08-04-broad-factor-admission-retest.md`). The `j3037` veto that held it on 08-03
  is RETIRED as a measured lottery; the real loss is `ContextModel`, which replicates 3/3 windows, plus the
  T3 cost on both halves.
- [ ] **(b′) TARGETED Factor admission** — the live follow-up. The blanket knob is the wrong instrument, not
  the wrong idea: the oracle ablation leaves **+54.17 PAR-2 and +3 solves on Tier B and +42.27 on Tier C**
  on the table, Tier B wants Factor ON while Tier C wants it OFF, and the shipped gate is the best arm on
  neither. Note the gate's own defect (it loosens monotonically over the solve) and the 0-for-5 record of
  hand-designed formula-shape separators before proposing a sixth threshold.
- [x] ~~**(c′) T3's kissat half moved 1.271× → 1.346× across TB4b**~~ — **SUPERSEDED 2026-08-06.** A far
  larger effect landed on top of it: T3 went to 1.72× kissat / 1.22× cadical once V5 Sweep was
  scheduled, and the same-window A/B plus the governor diagnosis attribute that to Sweep's
  unenforced effort ceiling (overshoot 73.2×). Do not re-open the TB4b question in isolation;
  re-derive from the post-governor-fix tier numbers. Old text: **(c′) T3's kissat half moved** — a real number attached to an unclosed
  question, **now subsumed by the much larger 2026-08-05 Sweep regression; do not re-open it first.**
- [x] Effort-budgeted scheduler increment 1 + prerequisites (revs 53–55; design `docs/design/effort-scheduler-design.md`)
- **Exit:** T3

## Phase 3.5 — engineering ingenuity on top of kissat (PIN, rev 67)

Deliberately unelaborated (§9 row 3.5 + §11 O15): once kissat performance is matched, a dedicated pass for novel engineering beyond the reference
lineage. Concretize at the T3 gate via revise-plan.

## Phase 4 — tuning, bindings, extensibility proof

- [ ] **Decide O16b at phase entry** (PR/SR clause learning as a catalog candidate — the checkable form of
  auto-invented rules; needs dpr-trim or PR→DRAT in the N2 story).
- [ ] **§5.5 hygiene, conditional (moved from phase 3, 2026-08-04):** SubsumeForward occs/marks alloc and
  `binary_ids` `.emplace` on the CDCL hot path — **escalate via §5.4 ONLY with a profile**, which is why
  this is not phase-3 work. (The third item, the `literal_redundant` depth cap 900 vs 1000, is CLOSED
  measured-dead — `88cff2d`.)
- [ ] Independent-lineage candidates: CMS best_inv rephase (with Walk), rotation presets (O6)
- [ ] Python bindings; policy presets (MAB scheduler, classification) — §5.6 seed
- [ ] Demo policies 4–6 incl. external branching (N1 end-to-end)
- [ ] §8.4 overhead budget: default vs hardcoded twin ≤ 5% on Tier B
- [ ] **Extensibility acceptance test: CCE (X3) as pure plugin (§5.3)** — **the GATE check only.** The
  rule itself is phase-3 work (**Track R · R6**), so this row is where the phase-4 exit criterion is
  *evaluated*, not a second implementation task.
- **Exit:** T4 (⇒ N3 satisfied)

## Phase 5 — competition readiness

- [ ] Backlog rules as warranted — *X2/X4 landed in phase 3 Track R; X5 Condition is Track R · R7*
- [ ] Docker + run scripts; solver description; license/attribution audit
- [ ] Full toolchain rehearsal on Tier B (solver → proof → verified checker, 16 GB cap)
- [ ] **Input validation for `compete`:** a DIMACS clause missing its final `0` is a real divergence and is
  NOT fixed — no checker accepts the FILE, so nothing on the emission side can repair it (sized in
  `benchmarks/reports/2026-08-05-input-clause-normalization-n2.md` §2.1).
- **Exit:** T5 or best effort; submission-ready

## Phase 6 — learned scheduling: the RL module (§5.6, rev 26)

The end-game (user directive 2026-07-15): learn on-the-fly which proof rule / batch to apply — order, frequency, budgets. Bandit-first, no deep
learning on the competition path; DefaultPolicy stays the N3 baseline and a permanent arm.

- [ ] Offline replay/counterfactual harness: exact `--trace` replays + §8.4 config-file ablations; validate reward proxies BEFORE any online
  learning is trusted (§8.5)
- [ ] RL1: windowed/discounted-UCB bandit over the macro scheduler's slot choice points (DefaultPolicy schedule = permanent arm)
- [ ] LearnedPolicy joins the random-policy fuzz rotation + determinism trace tests
- [ ] Gate L-safe measured: within +2% PAR-2 of DefaultPolicy on Tier C medians
- [ ] RL2: contextual bandit, tabular contexts (features per O12; **rev 68: must include the density-gate family — binary density, dose
  productivity, elimination yield — per §5.6's elimination-dosing exhibit**)
- [ ] **Elimination-dosing exhibit obligations (rev 68, §5.6):** per-entry dose on/off joins the scheduled choice points; DefaultPolicy's gate
  thresholds = permanent arm; success = gates matched where right, em_11 recovered where wrong, no hand threshold load-bearing
- [ ] **W4e · Walk dosing hand-off** (moved from Track W 2026-08-04) — the five knob-gated Walk arms
  (`TACTIX_WALK_NO_TARGET`, `_MAX_STEPS`, `_WARMUP`, rotation, effort anchor) + `stats.walk_minimum` as a reward proxy become the **second dosing
  family** in the §5.6 exhibit, alongside elimination dosing. Each arm is a measured big-win-on-one-class / 3–20×-loss-on-the-other, which is
  exactly the labelled signal a learner needs. **Nothing to build before phase 6** — the arms already exist behind knobs.
- [ ] RL3 experiment: composed-batch sequencing (tabular RL) under its recorded kill criterion; neighbor-move arm proposals (TACO shape, in-run)
- [ ] Anytime check (§8.5, rev 28): interrupt LearnedPolicy anywhere — no segment re-solves or sits in a non-solving state
- [ ] Offline per-family preset layer (classify → presets as arms)
- [ ] Gate L-win measured on Tier D + per-family attribution report (≥ 2 family wins); O13 distillation decision
- **Exit:** §8.5 gates (L-safe, then L-win); stretch — improves the T5 configuration

## Log

### 2026-08-06 — Track R · R6: X3 CCE lands ON, and the §5.3 extensibility test comes back MOSTLY YES

Branch `track-r6-cce`, base `accb362`. The rule is cadical's `cover.cpp` transliterated: grow the
candidate by asymmetric literal addition (ALA — literally unit propagation of `¬C` over `F∖{C}`) and
covered literal addition (CLA — a literal shared by every clause resolving on some pivot), then delete
the ORIGINAL clause when the extension is blocked or falsified. **cadical is the only reference**:
kissat's `cover.h` is `#define COVER(COND)`, a coverage assertion, and it has no `cover` option at all.
cadical ships `cover`=**0**, which per the landing convention is not disqualifying (`block`=0 too, and
R2 BCE shipped ON).

**▶ THE §5.3 ACCEPTANCE TEST: the rule body is a pure plugin; the REGISTRATION SURFACE is seven tables.**
Nothing in `src/kernel/`, `src/proof/`, or any other rule's directory changed to make CCE fit — no new
primitive, no widened interface, no `ρ` format change. In particular `kernel_push_witness` absorbed
CCE's *multi-segment* deletions unchanged, which is worth stating because R7 X5 Condition blocked on
exactly that surface: a conditional autarky needs a witness CUBE over variables OUTSIDE the clause,
where `ρ` stores one witness literal INSIDE it. CCE never needs that — cadical `cover.cpp:411` pushes a
single witness literal per segment and so do we. What a new rule DOES have to edit centrally:
`CMakeLists.txt` · `Stats` (`kernel/kernel.hpp`) · `EFFORT_SPECS` + its meter + `AUTHORED_SLOT_ORDER`
(`default_policy.cpp`) · `check-effort-specs.py`'s classification table · `main.cpp`'s stats line ·
`tests/CMakeLists.txt`. Plus the frozen C-ABI rule ids RENUMBER by the name sort (`test_api_c.c`'s golden
list says to expect it). **Only ONE of those is a real architecture pressure point, and it is `Stats`:**
the `EffortSpec` meter is `uint64_t(*)(const Kernel &)`, so a rule's budget currency must be a field on
the shared `Kernel::Stats` — a rule cannot carry its own counter and still be titratable. Everything else
is a *classification* table that fails loudly and on purpose when a rule is added (that is
`check-effort-specs.py`'s stated design) or a build-system list. **Recommendation: leave it. Do not
widen the meter to an opaque handle to buy a literally-one-line registration** — the counters have to be
readable by `main.cpp`, the API, and every probe script anyway, and a per-rule counter blob would make
the reachability discipline this project runs on harder, not easier. The honest amendment is to §5.3's
wording, not to the code: "one directory + one registry line + a declared counter, budget row, slot
position and test".

**The proof obligation, which is what decided the design.** Proof δ = `delete` + `push-witness`·k; the
ATA exit reached with no CLA step in front of it is `delete` alone (`C` is RUP against `F∖{C}` there, so
no witness exists to owe). Segments are pushed in application order — `(u_1, D_0=C), (u_2, D_1), …` and
for the blocked exit `(w, D_n)` — where `D_i` is the recorded clause: `C` plus the CLA additions, **and
not the ALA literals**. The ALA literals are omitted deliberately, and what makes that correct is the
**ALA-closure lemma**: if a model of `F∖{C}` falsifies every literal of `D_i`, it falsifies every ALA
literal too, since each was added because some `D ∈ F∖{C}` had all its other literals already in the
extension. So the smaller recorded clause is exactly the right trigger even though blockedness is tested
against the larger one. Include the ALA literals instead and the FIRST segment stops implying `C`; omit
the lemma and the flip can break a clause blocked only via an ALA literal. Full argument in
`docs/calculus.md` §6c.

**The asymmetry that is easy to get backwards.** CLA/blockedness quantifies over every live
non-root-satisfied clause, redundant included — over-quantifying only makes the test stricter, and
filtering that side is precisely X1 BCE's fuzz-2204 wrong-SAT. **ALA is irredundant-only, and for an
ABSTRACT reason, not an engineering one:** the lemma needs the justifying clause to be satisfied when `ρ`
replays, and S2 Reduce deletes redundant clauses with no witness at all.

**Evidence.** `tests/test_cce.cpp` (45/45 ctest, all four checkers green) carries an exhaustive
reconstruction cross-check: 400 random formulas, **3605 deletions (2251 blocked / 1354 ATA), 1486 covered
literals added, 28926 models of the reduced formula enumerated of which 18926 genuinely needed `ρ` — all
repaired**. Reachability at the SOURCE under the shipped DefaultPolicy, 59 structured/circuit
fuzz-generator cases: **26 rounds, 4223 extended, 2411 ALA + 154 CLA, 34 deleted (6 blocked / 28 ATA)**,
against an exactly-zero `TACTIX_SLOT_DISABLE=CCE` control ([[ab-arm-identical-artefacts]]). Under
`--cce-stress-policy` on the same population: **170,840 extended, 32,348 covered literals, 4625 deleted
in 49 of 55 cases** — so the stress legs below are not vacuous. Fuzz **26,000 cases, 0 failures, five
legs, corpus 14/14 each**: stress×DRAT `1585216` · stress×native-LRAT `1585217` · `--random-policy`×DRAT
`1585220` · `--random-policy`×native-LRAT `1585221` (4000 each) · DefaultPolicy quick×DRAT `1585223`
(10000). The stress policy (`src/policies/cce_stress_policy.cpp`, KEPT) covers every root to a fixpoint
with no budget, draws the per-literal instance surface, and interleaves BCE and BVE so `ρ` carries CCE's
multi-segment sequences between other rules' — it exists because the shipped dose is 4 per mille and a
wrong witness here is a wrong SAT ANSWER, not a rejected proof.

**▶ A/B SPEC — WRITTEN, DELIBERATELY UNRUN** (a Tier C A/B was live; a second would have contaminated it).
Same-window Tier B + Tier C pair, TWO pin dirs (`build/pin-r6-cce-b` / `-c`, same commit, matching binary
shas), node-pinned, `--baseline kissat --baseline cadical`.
- **Arms:** `cceon` = HEAD as shipped · `cceoff` = `TACTIX_SLOT_DISABLE=CCE`, **in the command, never the
  environment**. The off arm is a SCHEDULING diagnostic, not a candidate default ([[no-rule-off-configs]]).
- **Pre-registered source readout, checked FIRST:** on the `cceon` arm `cce_tried`, `cce_deleted`,
  `cce_cla` and `cce_blocked` must all be non-zero tier-wide, and on `cceoff` all four must be exactly 0.
  A run failing either half was not tested. Report the per-instance `c cce` line, not just the aggregate.
- **Ship criterion:** #solved beats PAR-2; noise floor 6.72; the may-not-gate set may not gate alone.
  **The rule ships ON regardless of the headline** — this prices the DOSE, not the rule ([[no-rule-off-configs]],
  and X1/X2/X4 measured ALONE were 21.79/53.26/13.08 Tier B PAR-2 against 9.22 together).
- **The second question, and the one more likely to pay:** CCE's 4 per mille is cadical's `covereffort`,
  the lowest rate in its option set, ported to a *different currency* (`cce_ticks` counts clause literals
  scanned, not propagations). If arm 1 is inside the noise floor, sweep `TACTIX_SLOT_PERMILLE=CCE:r` for
  `r ∈ {4, 20, 100}` on Tier B before concluding anything about the technique.
- **Also owed, and NOT measured here** (no timing claim is made in this entry — the local allocation is
  oversubscribed): CCE's share of clock, and whether the per-candidate tick cap (`CCE_CANDIDATE_TICKS`,
  ours, not cadical's) binds. Ledger row `D-CCE-SCOPE`.

### 2026-08-06 — SCHED-H3: the bound binds on 40 % of the words, and the words are 0.15 % of the traffic

**CLOSED, measured-marginal, and deliberately NOT implemented.** The row is real — we are cruder than
kissat here — but the thing it makes cheaper is not worth a new kernel invariant, and the reason our
version of the bound would under-deliver is structural, not a tuning question.

**WHAT THE REFERENCE BOUNDS.** kissat keeps `solver->first_reducible`, the arena offset of the earliest
redundant clause, and starts BOTH the candidate collection and the `used`-aging tick there — `reduce.c:161`
`reference start = compact ? 0 : solver->first_reducible;` feeding `collect_reducibles`, which ages in the
same pass. The watermark is maintained at three sites (`collect.c:153-196`: `update_first_reducible`,
`kissat_update_first_reducible`, and the sweep's recomputation). **cadical has no equivalent.** We walk the
whole arena **three** times per reduce — `reduce_candidates` to enumerate, then `reduce_apply_refs` to age
and (since SCHED-F6, same day) to publish the survivor envelope — where the audit
(`docs/design/2026-08-04-n4-scheduling-parity-audit.md` §8, `WE_ARE_CRUDER`, severity **low**) recorded
two. The watermark would bound all three: every one of them only reads redundant clauses.

**THE MEASUREMENT** (jobs `1584979` + `1585096`, Tier B + Tier C = 58 instances, 400 s cap, counters only —
`Stats::reduce_scans/_scan_words/_skippable_words` on the `c redscan` line, so the numbers are
contention-invariant and no wall clock is claimed; script `benchmarks/probes/h3-redscan.sh`, summary
`…-summarize.py`). Measured on this branch over base `009a4e2`, i.e. **before** the Sweep-governor and
Stage-F merge; the verdict is an order-of-magnitude one and neither merge changes Reduce's arena walk in
kind — SCHED-F6 adds a third walk, which is folded in below rather than re-measured. 46 answered, 3813 reductions, and the skippable figure is deliberately the OPTIMISTIC
reading (a garbage redundant clause does not pin the watermark), i.e. an **upper bound** on the win:

| | value |
|---|---|
| enumeration walk, all instances | 14,765 M arena words = **59.1 GB** read |
| skippable by the watermark | 5,942 M words = **23.8 GB**, **40.2 %** |
| walk as a share of propagation cache-line traffic | **0.15 % aggregate** (the headline), **median 0.23 %**, max **3.25 %** (`T105.2.0`) |
| per-instance skip % | min **0.0**, median **17.7**, max **98.8**; **11 of 57 under 1 %** |

**Multiply the first two rows and the share by 3** for the aging and survivor-envelope walks. So the honest
sizing: Reduce's arena traffic is ~0.7 % of the propagation proxy at the median and ~9.8 % at the extreme
(`T105.2.0`, a 400 s timeout with 31 reductions), and the bound removes 40 % of that — **~0.3 % median,
~3.9 % at the one worst instance**, against a proxy that itself **overstates** the time, because the arena
walk is a sequential prefetchable stream while propagation's ticks are random-access watch-list lines.

**WHY OUR SKIP IS BOTH SMALLER AND ERRATIC — the part that decides the row.** kissat's watermark is a stable
`[irredundant | redundant]` boundary because its collector RE-PARTITIONS the arena at the end of every
reduce (`collect.c:213 move_redundant_clauses_to_the_end`, called from `kissat_sparse_collect`,
`collect.c:626`). Our `kernel_collect_garbage` copies live clauses **in order** and never partitions, so the
watermark is pinned by whichever surviving redundant clause happens to sit earliest — hence 11 instances
under 1 % and a 0.0 % floor. During-search rules that add IRREDUNDANT clauses (BVE resolvents, Factor
definitions, Vivify) append them after the learned ones, so the two classes stay permanently interleaved.
**Porting the watermark alone buys the smaller half of kissat's win; the partitioning collector that buys
the other half is not a low-severity row.**

**Gates for the counters themselves:** ctest **45/45**, all four checkers green, corpus 14/14, and the same
merged-tree fuzz legs as H2 (`1585219` / `1585222`, 6000 cases each, 0 failures). The C10 counters in
`reduce_apply_refs` are untouched — `test_restart_reduce::test_tier2_band_tracks_the_live_tier` passes.

**What ships:** nothing but the three counters and the `c redscan` line, kept so the claim stays auditable
and so the answer can be re-checked in one array if the collector ever partitions — which is the change
that would make the bound worth having. No knob, no Kernel watermark, no new invariant
(the honest form of SCHED-E3's outcome: price it, publish the number, decline).

**Two adjacent rows this measurement prices, NOT taken here.** (a) The audit's neighbour — kissat's
`if (!clauses_redundant) return false` early-out, which we lack and pay the full walk plus a sort of an
empty vector for. (b) **FUSING the three walks**, which is now the better-value change of the two and needs
no new invariant at all: kissat does the aging inside `collect_reducibles`. Not taken because
`reduce_candidates` is deliberately side-effect-free so an introspecting policy can enumerate without aging
the database, and the F6 envelope has its own contract — fusing means renegotiating both, which is a
rule-contract question, not a scan bound.
### 2026-08-06 — SCHED-H1 (the X1/X2/X4 third): `budget_aware` was never the same thing as interruptible

`H1` is split three ways by file; **this entry covers BCE / HyperTernaryRes / Instantiate only.** C1/C2 and
Vivify are owned separately and the checklist item stays open until all three land.

**What the three checked BEFORE.** All three are `budget_aware` batch entries and all three polled
`Kernel::work_budget` mid-round — **and nothing else. `kernel_terminating` appeared in exactly one rule in
the whole tree** (V6 Congruence, five sites, task #10). So the answer is the same for all three: none of
them was already correct, and a round that had begun ran to completion after an interrupt arrived. The two
tests are not interchangeable — `work_budget` is 0 whenever the policy leaves a round unmetered, X2's
second governor is an OUTPUT ceiling, and *neither* is consulted on the single-instance `*_try` path an N1
policy may drive directly.

**SIX poll sites, two per rule — the round loop plus the one inner loop whose cost is unbounded:**
- **X1 BCE** — `bce_apply`'s literal loop, and `bce_try_indexed`'s candidate SCAN. The scan is the
  unbounded part: `bce_occ_limit` ships 0, so one literal's blockedness test is |cands| × |opposite|. The
  DELETION loop is deliberately **not** polled — it is the rule's output, already bounded by the scan that
  produced it, and cutting it would only discard work already paid for.
- **X2 HyperTernaryRes** — `ternary_apply`'s pivot loop, and `ternary_try`'s per-PAIR loop. Per pair, not
  per outer clause: `ternary_exists` scans a whole occurrence list, and the whole ARENA when no dense index
  is attached, so one pair is already the largest unit worth running to completion.
- **X4 Instantiate** — `instantiate_apply`'s candidate loop, and `instantiate_one`'s eligible-literal loop.

**Soundness — aborting means the rule did LESS, and every site sits between complete units.** BCE: a
deletion is witness-onto-ρ + `del` + garbage-mark, and the scan is read-only with `marked` all-zero at the
poll, so an abort costs only untested candidates; those already found blocked were tested against the FULL
opposite side and deletion only shrinks it. X2: each resolvent is one `add_rup` plus one kernel add, and
`res` is scratch. X4: the poll sits where a failed `probe_pair` has already unwound the trail and closed
the probing scope and a successful one has returned — the clause is fully rewritten or untouched, never
half-instantiated. In all three the round-loop cut reuses the BUDGET cut's exit, so SCHED-F2's re-arm runs
and the untried tail is not silently marked done. **No guard, precondition or instance surface changed** —
nothing for the deviation ledger.

**Counted at the SOURCE.** `kernel_rule_interrupt(k, site)` polls AND does both increments
(`rule_interrupted` + the rule's own); **no call site increments anything**, so a new abort site cannot be
added uncounted. New line `c interrupt3  BCE n, X2 n, X4 n aborts` — `c interrupt` alone is a sum over
every polling rule and cannot say whether a PARTICULAR round is interruptible.

**All three of task #10's vacuity modes closed, explicitly** (its abort paths passed 4 × 10,000 cases while
proving nothing):
1. *Wrong binary* — both probe scripts print `strings <binary> | grep -c interrupt3` before any arm runs.
2. *Unreachable cadence* — **there is no cadence.** Every site sits at the top of a heavyweight iteration
   (one literal's scan / one resolution pair / one probe), so the FIRST candidate reaches the first check.
   Nothing is gated on `!(++tick & 255)`, which is what made task #10's poll sites unreachable below one
   poll window.
3. *Blind counter* — the increment is inside the predicate, and every sweep carries a `STOP_FUZZ=0` control
   that reads 0/0/0.

**NON-ZERO ON A REAL INSTANCE (job `1585204`, `9vliw_bp_mc`, Tier B, the instance where all three rules run
under DefaultPolicy):** rate → `c interrupt3` / verdict — `0` → **BCE 0, X2 0, X4 0**, UNSAT (control) ·
`10000` → **BCE 61, X2 2, X4 1**, UNSAT · `1000` → **BCE 55, X2 6, X4 5**, UNSAT · `100` → **BCE 62, X2 4,
X4 58**, UNSAT. Reproduced independently at `100000` on jobs `1585090` and `1585200` (BCE 15, X2 1, X4 0,
identical across both — 100000 is too sparse for X4). Two more Tier B instances in the same job, at rate
`100`: `hwmcc17miters` **BCE 31, X2 2, X4 13** and `square.2.0` **BCE 36, X2 6, X4 49**, both still correct.
**The verdict is never wrong at any rate**, which is the point: the abort paths are new control flow over
the proof stream.

**And on the size class the FUZZ HARNESS generates** (job `1585095`, 299 CNFs from `generators.generate`
with fuzz.py's own `(seed+i, i)` derivation, run on the exact `build/debug` binary the legs use) —
`STOP_FUZZ` 0 / 1000 / 100 / 10 → source fires 0 / 322 / 3057 / 1394, BCE 0 / 102 / 1405 / 694, X2
0 / 85 / 1320 / 433, X4 0 / 2 / 6 / 53. (Fires FALL from 100 to 10 because rules that abort sooner do less
work and therefore poll less; X4 rises because its sites sit early in a round.) Repeated under
**`--random-policy`**, which is the schedule the stress legs actually run (job `1585213`, 119 cases):
0 → 0/0/0 · 100 → source 689, **BCE 235, X2 75, X4 17**. **This is the check task #10 skipped**, and it is
what licenses the stress legs below.

**Fuzz — 7 legs, 0 failures, corpus 14/14 on each.** Native DRAT `1585158` (10,000) · `--random-policy` ×
native DRAT `1585186` (4,000) · `--random-policy` × native LRAT `1585159` (4,000) · and the four that
exercise the new control flow, with the knob **passed in the command** (`/usr/bin/env TACTIX_STOP_FUZZ=n`,
recorded verbatim in each log's `solver=` line, because an exported variable that never reaches the
solver looks exactly like a clean run): `STOP_FUZZ=1000` × DRAT `1585183`, × LRAT `1585184`;
`STOP_FUZZ=100` × DRAT `1585187`; `STOP_FUZZ=10` × LRAT `1585194`. The arms are **not** identical
artefacts: TIMEOUT counts move 733 (no knob) → 730 (1000) → 718 (100) → 734 (10, and SAT 2014 → 2015),
so the aborts are changing trajectories rather than the knob going nowhere.

**Unit-pinned, all six sites** (`test_bce` / `test_ternary` / `test_instantiate`, one `test_interrupt` each,
three blocks apiece): flag set → the round cuts at the first candidate; flag set → the single-instance
entry cuts inside the inner loop; flag clear → the control that shows the abort is what suppressed the
deletion/resolvent/rewrite. Each also asserts the per-rule counter is exactly 1, so a site that stopped
being reached fails the test rather than passing quietly. **No site is unreachable-by-schedule** — none
needed the SCHED-C6/C9 treatment.

**Residual, not fixed here and not ours:** `battleship` takes **7.3 s** to exit after SIGTERM (job `1585090`)
with `c interrupt 0` and BCE/X2/X4 at 0 rounds — so that latency is in search, the proof flush, or one of
the rules the other two H1 shares cover. ctest **44/44 release and 44/44 ASan+UBSan**; `config_wiring`
(36 fields, 0 failures — three Stats counters were added, no Kernel CONFIG field), `effort_specs`,
`option_coverage`, `check-progress` all green.

### 2026-08-06 — VOID WINDOW: the Sweep-governor Tier C A/B is not usable, and why

`20260805-234459-C-govon` (array `1584791`) **must not be cited.** Both our arms read **25/30** where they
have read 28–29/30 all evening, and the cause is the measurement environment, not the solver:

- **4 array tasks were OUT_OF_MEMORY-killed** at 8 G/cpu (`128_125` died at t=24 s rather than timing out).
- **336 job records touched node017 during the window's 21 minutes**, and the window was node017-pinned.
- A 30-instance Tier C probe from the R7 investigation (`1585043`) overlapped it for ~10 of those 21
  minutes, partly on the same node.

**Root cause is coordination, not any one agent.** Eight parallel agents were each told "small sbatch
probes are fine" with no cross-agent budget, so collectively they saturated the partition. Each probe was
individually reasonable. **The standing rule this yields: correctness probes may run concurrently
(counters are contention-invariant), but any window whose OUTPUT is a time must own a quiet partition** —
check `squeue` is clear of other agents' work before submitting, not merely clear of tier arrays.

The Sweep governor fix itself is unaffected: its evidence is counters and per-phase splits
(`spent/handed` 73.2× → 1.0005×, build 304.835 s vs sub-solver 6 ms), which contention cannot move.
**Owed: re-run this A/B on a quiet partition before D2 is priced.**

### 2026-08-06 — Track R · R7 CLOSED WITHOUT CODE: X5 `Condition` needs a witness CUBE, and `ρ` holds one literal

Branch `track-r7-condition`. The last unimplemented family in Track R's inventory, closed as a
**reasoned non-implementation**. Criterion fixed BEFORE investigating, so the answer could not be
back-fitted: land iff **C1** a reference has it as a proof rule (default-OFF explicitly *not*
disqualifying — cadical's `block` is default 0 and kissat 4 dropped BCE, yet R2 landed BCE ON),
**C2** it fits §5.3 (proof delta in the closed vocabulary **and** witness expressible on the existing
`ρ`, no kernel edit), **C3** it can actually fire here. Value/PAR-2 was deliberately excluded as a
criterion — the all-rules-ON directive plus the X1/X2/X4 repair-each-other result (21.79/53.26/13.08
alone vs 9.22 together) means "worth only 14.32 PAR-2" cannot close a rule on its own.

**C1 PASSES, but the inventory's billing was wrong.** Verified at the source rather than from the row:
**kissat does not have this rule in any form** — no `condition.c`, no autarky module; the only
`condition` hits under `kissat/src/` are congruence's ITE *gate condition sets* and "conditional
equivalences", an unrelated notion. cadical-only, and **default OFF** (`options.hpp:58`). Same class of
correction `SP-JUMP-1` made to a "both references" billing the day before.

**C2 FAILS, and that is the decision.** `Kernel::reconstruction` stores `[rest…, witness, len]` and
`kernel_extend_model` tests satisfaction over the **whole** segment before setting the **single**
trailing witness true (`kernel.cpp:1270-1306`) — so the witness is one literal *and* it lives inside the
clause. A conditional autarky's witness is a **cube** whose literals are in general outside `C`; that is
precisely what "globally" blocked adds over "blocked". It cannot be split into several single-literal
segments (each would put its own witness inside its own satisfaction test — the wrong-SAT shape BCE
already hit once), and restricted to one in-clause literal the rule **collapses to X1 BCE**, which R2
ships ON. **Not a we-are-behind-kissat gap:** kissat's stack is the same shape (`weaken.c:36-44`, one
tagged witness literal); cadical had to add a witness *section* (`extend.cpp:86-107`) for this one
technique and its own header says so. Landing X5 = change the `ρ` format + `kernel_extend_model` + all
five `kernel_push_witness` call sites (BCE ×2, BVE, Substitute ×2) = **architecture RFC**, not a
"one directory + one registry line" landing. **Checked so the two Track R items are not bundled on a
false premise: R6 CCE does NOT need it** — `cover.cpp:411` pushes a single witness literal — so the
revamp would buy exactly one default-off cadical-only rule.

**C3 is reachable but buys nothing, and one obvious objection is REFUTED by the measurement.** Probes
`1585043` (paired cadical `--condition=true` vs default, same node, all 30 Tier C, 400 s cap) and
`1585084` (`--stats`, 6 instances). **Fires on 13 of 30**, 2–550 clauses (`constraints_18` 550,
`minxor128` 385, `constraints_25` 211, `j3037` 96, `frb45` 87, tail down to 2); `circuit_32in32out` and
`urq45` conditioned **0** after 2 and 41 conditionings — those zeros are "ran and found nothing", not
"never ran". **No solve flips either way**: both arms return the same 26 verdicts and the same 4
non-verdicts (3 timeouts + `128_125`, OOM-killed on *both* arms). **Total wall 5699.2 s off → 6343.1 s
on, +643.9 s (+11.3 %)**. Per-instance is a lottery so the aggregate is directional, not a verdict
([[chaotic-instance-veto]]): `j3037` +286.3, `ContextModel` +227.0, `Ptn` +161.1 against `EDP3` −134.1,
`af-synthesis` −122.0, `mp1` −95.2. Cost ≈ **1.4 M condition propagations per deletion** on
`ContextModel` (28.76 M over 26,992 candidates for 21). **The witness cube is SMALL** — `condautrem`
1.0 (`em_11`) / 2.1 (`j3037`) / 5.2 (`minxor128`) / 5.6 (`ContextModel`) against *initial* autarky parts
of 4 k–11 k — which **refutes** the natural reading of cadical's own TODO at `condition.cpp:784` that
`ρ` would blow up. `ρ` growth is not the problem; representability is. And `em_11`'s cube averaging
exactly **1.0** means those are plain blocked clauses our X1 already takes, so the genuinely-new yield
is the `cube > 1` remainder, smaller still.

**Recorded:** `implementation-notes.md` **§16** (full argument + reopening path, ledger id
`SP-COND-1` — verified free), a pointer note in `calculus.md` after X1, and a correction to §13.6's
dismissal roster, which had `condition*` filed as "recorded, not analysed" (arithmetic untouched; ctest
`option_coverage` green). **No code, no registry line, no `EFFORT_SPECS` row, no A/B** — there is
nothing to price. **What reopens it:** (1) a `ρ` revamp giving `kernel_push_witness` a witness section
(the RFC; also the prerequisite for set-blocked / PR elimination), (2) yield evidence from an
**X1+X3+X5 joint** arm — judging this family one rule at a time is the measurement it defeats — or
(3) cadical flipping `condition` on by default.

### 2026-08-05 — ▶ SWEEP'S DEGRADATION DIAGNOSED AND FIXED: the effort ceiling was never enforced where the work is

Branch `sweep-governor-fix`, base `97e9476` (post-Stage-D). **The meter hypothesis is REFUTED as stated
and the real defect is the ENFORCEMENT POINT.** `sweep_ticks` charges honestly — a Sweep build tick costs
0.39–9.5 ns against a kernel search tick's ~33 ns amortized, the same kind of unit within ~3.5× — but
`sweep_detect_round` polled the round budget only *between* seeds and `sweep_env_build` had no poll at
all. `max_env_vars`/`max_env_clauses` bound what an environment HOLDS, not the scan that finds it, so
after the variable cap is reached the BFS keeps walking every remaining occurrence and scanning every
clause only to reject it. **`constraints_18_0.3_2`: handed 665,325,800 ticks, spent 48,684,784,694 —
73.2× over its dose**, 304.8 s of the 597 s solve, 99.996 % of it in `sweep_env_build`, sub-solver 6 ms,
per-round setup 0.000 s (so NOT `dense_connect`/setup). Trajectory bit-identical (conflicts and
`stats.ticks` equal to the digit on both arms), so 100 % of it is overhead. Stage D amplifies it: 45 → 82
occasions. **Fix (44 lines, all in `src/rules/sweep/`): `SweepBudget` polled inside the expansion loop —
cadical's own discipline (`sweep.cpp:197-205,1577-1580`), and declining to expand is always sound since
the environment only shrinks and stays ⊆ F.** Overshoot → **≤1.005× on all four** probe instances;
`constraints_18` **720.5 → 244.9 s (−475.6)**, `bhaskara` **364.0 → 298.3 (−65.7)**, `em_11` +13.3 (node
noise), `minxor128` unchanged. `TACTIX_SWEEP_ENV_BUDGET=0` reverts the inner poll for the A/B arm.
**Two mechanisms the four-instance framing conflates, now separated:** `constraints_18` is 100 % Sweep
OVERHEAD (bit-identical trajectory, fixed here); `minxor128` and — post-Stage-D — `em_11` are 100 %
TRAJECTORY (Sweep's own wall is 1.95 s / fractions of a second, ledger 1.01–1.04×, and it emits 961 / 21
equivalences that make the search worse). No governor change addresses those, and this does not claim to.
**MEASURED NEGATIVE, do not re-try: a Sweep resumption cursor.** `Kernel::sweep_cursor` in
`transitive_cursor`'s shape fixes a real defect on paper (45 rounds re-sweeping the same seed) but makes
`em_11` **INERT** — 378 environments, 0 equivalences, straight back to its Sweep-off time. The repetition
is re-examination under a formula that changed between rounds. kissat can move on because its schedule is
a small occurrence-filtered, occurrence-ranked set that COMPLETES a pass (and `sweep_completed` then
doubles the caps); ours is every active variable, so a pass never completes. Recorded as **SP-SWEEP-3**;
the closer is kissat's schedule, not a cursor. **SP-SWEEP-2** (the productivity delay ladder — kissat
`sweep.c:1718-1721`, gated here on `!e.meter` so a metered repeater can never back off) stays open and
belongs to Track SCHED. Gates: ctest **44/44**, `sweep_proofs` 41 derivations through real drat-trim +
lrat-check, `micro_proofs` OK, **nine fuzz legs 0 failures** (random-policy native DRAT + native LRAT +
sweep-stress + forced-general + ASCII DRAT). Reachability at the SOURCE on the shipped default:
1,468 rounds, 7,092 environments, 3,441 equivalences, **3,558 binaries and 293 general-path pairs** over
1,497 generated cases — both proof paths non-zero, so no green leg here is vacuous. Report
`benchmarks/reports/2026-08-05-sweep-governor-diagnosis.md`.
### 2026-08-05 — SCHED-G2 + SCHED-H1 (C1/C2): fuzz, five legs, 0 failures

ctest **44/44**, all four checkers green (`config_wiring` · `effort_specs` · `option_coverage` ·
`check-progress`), regression corpus **14/14 on every leg**. Debug build (`-O1 -g` + ASan/UBSan).

| job | leg | cases | failures |
|---|---|---|---|
| `1585205` | DefaultPolicy × native DRAT | 10,000 | **0** |
| `1585206` | `--random-policy` × **native LRAT** | 6,000 | **0** |
| `1585207` | `--random-policy` × native DRAT | 6,000 | **0** |
| `1585208` | **`TACTIX_STOP_FUZZ=97`** × `--random-policy` × native LRAT | 6,000 | **0** |
| `1585209` | **`TACTIX_SUBSUME_SHADOW=1`** × `--random-policy` × native DRAT | 6,000 | **0** |

**Re-fuzzed after the rework** (helper adopted, cadence removed, `*_try` sites added), same three
legs that touch the new control flow, jobs `1585256` (`TACTIX_STOP_FUZZ=97` × `--random-policy` × native
LRAT), `1585257` (`--random-policy` × native LRAT) and `1585258` (`TACTIX_SUBSUME_SHADOW=1` ×
`--random-policy`): **6,000 cases each, 0 failures, corpus 14/14**. Verdict counts track the un-stressed
leg (SAT 3036–3040, UNSAT 1883–1887), so the extra abort sites still change no answer.

The last two are the legs that matter here and neither existed before. **`1585208` is the abort leg**: the
stress cadence bypasses the 256-poll window, so every C1/C2 poll site is live on cases far smaller than one
window — the exact vacuity that made task #10's abort paths pass 40,000 cases while inert. Its verdicts
(SAT 3041 / UNSAT 1902) match the un-stressed `1585206` (SAT 3041 / UNSAT 1887, the difference being
timeouts resolved, not answers changed), so interrupting a round mid-way changed no verdict and every UNSAT
still produced a checkable LRAT chain. **`1585209`** fuzzes shadow mode itself, i.e. C1 declining an
inference it found — the soundness direction the G2 instrument depends on.

### 2026-08-05 — SCHED-G2 (2/2): C1 is INERT — C2 absorbs 698,341 of its 698,351 subsumptions

Probe `1585160` (23 tasks, counters only, `benchmarks/probes/g2-c1-shadow.*`, binary `build/pin-g2h1` at
`7e5abee`). Thresholds were declared in the entry below **before** the run: absorption ≥ 0.9 ⇒ inert.

**Measured absorption: 0.99999.** `c1shadow` on the shadow arm, against the shipped arm's `forward_subsumed`:

| instance | C1 yield (shipped) | shadow observed | C2 deleted | C2 rewrote | **residual** |
|---|---|---|---|---|---|
| `DivS_862_11` | 674,584 | 675,481 | 522,737 | 152,744 | **0** |
| `mod4block` | 15,943 | 16,895 | 16,310 | 585 | **0** |
| `9vliw_bp_mc` | 5,386 | 4,960 | 3,707 | 1,243 | **10** |
| `j3037_9_rggt_b` | 699 | 683 | 676 | 7 | **0** |
| `constraints_18_0.3_2` | 296 | 296 | 256 | 40 | **0** |
| `rbsat-v760c43649gyes10` | 36 | 36 | 36 | 0 | **0** |
| `battleship-14-26-sat` | 0 | 0 | — | — | C1 has no yield at all |

Not vacuous: `observed` tracks the shipped arm's `forward_subsumed` to within 0.2 % on the two big
instances, so the shadow arm really is finding the same set ([[ab-arm-identical-artefacts]]).

**And C1 is not merely inert — it is net NEGATIVE on cost.** Partner visits (`c subsume`/`c strengthen`),
with the shadow arm's C2 standing in for a C1-less C2 (it is the same formula evolution):

| instance | conflicts on/shadow | C1 | C2 | pair | C2 without C1 | **pair saving** |
|---|---|---|---|---|---|---|
| `DivS_862_11` | 2002 / 2002 | 3.178 G | 7.657 G | 10.835 G | 8.615 G | **−2.220 G (−20.5 %)** |
| `constraints_18_0.3_2` | 27,478 / 27,478 | 493.5 M | 690.9 M | 1184.4 M | 691.1 M | **−493.3 M (−41.6 %)** |
| `mod4block` | 2.607 M / 2.557 M | 179.8 M | 281.3 M | 461.1 M | 290.1 M | **−171.0 M (−37.1 %)** |
| `9vliw_bp_mc` | UNSAT both | 12.47 M | 43.01 M | 55.48 M | 45.01 M | **−10.47 M (−18.9 %)** |
| `rbsat` | SAT both | 0.14 M | 5.018 M | 5.161 M | 5.024 M | −0.14 M (−2.7 %) |

`DivS_862_11` and `constraints_18_0.3_2` are **conflict-identical across the arms**, so those two rows are
like-for-like; the others are indicative. The saving is a LOWER bound: shadow-mode C1 still re-arms its
unreached tail (SCHED-F2), which can only make MORE material look new to C2, so the shadow C2 column
over-states what a C1-less C2 would cost. `constraints_18_0.3_2` is the sharpest exhibit — **493.5 M
partner visits to delete 296 clauses, every one of which C2 takes anyway.**

**Why this was predictable from the source, and why nobody could have banked it without the probe.**
C1's `subsumes_marked` is C2's `check_marked` at zero flips, on the same candidate predicate and the same
1000/1000 limits, so C1 has no unique CAPABILITY band. Both references agree: kissat's
`forward_subsume_and_strengthen` (forward.c) and cadical's `subsume_round` (subsume.cpp) are ONE fused
pass. **Splitting them into two rules is our five-primitive decomposition, and it is right for N1 — but
giving each half its own 100 ‰ effort share is not a decomposition, it is a duplicated dose.** What the
source could not settle is REACH, and reach is where a real gap would have shown up; the probe says the
gap is 10 clauses on one instance.

**▶ THE FOLLOW-UP IS A DOSE, NOT A RULE-OFF** ([[no-rule-off-configs]]: every implemented proof rule ships
scheduled, and this is a diagnosed SCHEDULING defect, which is exactly what a rule-off arm is a diagnostic
*for*). `SubsumeForward` keeps its guard, its `instances()` and its `apply_instance` — a phase-6 policy or
an API caller may well want pure subsumption without strengthening, and N1 requires it stay available.

**A/B SPEC (unrun).** Arms, same window, node-pinned, TWO pin dirs for a B+C pair:
- **A** shipped. **B** `TACTIX_SLOT_PERMILLE=SubsumeForward:5`.
- **Pre-flight, or the window is wasted:** in B, `c subsume` ROUNDS must drop and `c strengthen … subsumed`
  must rise by roughly the `forward_subsumed` that disappeared. If `c subsume` rounds are unchanged the
  knob did not bind — abandon the window rather than read the PAR-2.
- **Ship criterion:** Tier B and Tier C PAR-2 both no worse at equal solve counts. The prior is a win on
  the C1-heavy instances (`DivS_862_11`, `constraints_18_0.3_2`) and neutral elsewhere.
- **Do NOT read `j3037_9_rggt_b`**: the shadow arm solved it in 34 s while the shipped arm timed out at
  300 s, and that instance is already on the record as a measured lottery ([[chaotic-instance-veto]]).

### 2026-08-06 — SCHED-H1 (the C1/C2 third): SIX poll sites, no cadence, and a residual ruled out

`D-STRENGTHEN-BUDGET` recorded C2 owning **97.8 s of `DivS_862_11`'s 109.0 s in four applications**, with
no way to stop a round once it began. `default_policy.cpp` polls `stop` at every slot boundary, which
bounds SIGTERM latency to ONE application — and one application here is most of the solve. A harness then
escalates to SIGKILL and **the proof file is lost**: an N2/§6 defect, not a latency nuisance.

**`budget_aware` was not termination here either**, exactly as the X1/X2/X4 third found. Both rules poll
`Kernel::work_budget` mid-round and polled nothing else; `work_budget` is 0 whenever the policy leaves a
round unmetered, and — the part that matters for N1 — **neither governor is consulted at all on the
single-instance `*_try` path**, which is the path a phase-6 policy driving `apply_instance` per candidate
would live on. Two of the six sites are there.

**Six sites, because C2 alone has four entries into unbounded work.** C1: the candidate loop, plus
`subsume_forward_try`. C2: the **flush pre-pass** over the whole arena, the candidate loop, the **cascade**
that re-checks one shrinking candidate (up to `CLSLIM` = 1000 full partner scans on a single clause), plus
`strengthen_try`. A poll in the candidate loop alone leaves a round deaf for the whole flush pass.

**Every site sits where nothing is half-mutated** — top of an iteration, `marks` all-zero, no clause
part-rewritten — and everything already done is proof-logged. **Dropping an inference is sound; leaving
`F`/`R`/`τ`/`ρ` inconsistent or emitting an unverifiable certificate is not** (Track C's `D-CONG-SKIP`).

**An interrupted round is bookkept as an INCOMPLETE round — with one correction found in self-review.**
The first cut of this reused SCHED-F2's budget-cut bookkeeping wholesale: re-arm the unreached tail, then
advance the watermark. **That is a starvation bug on the abort path.** Re-arm-then-advance is only
equivalent to "carry on next round" because the re-arm covers the tail of `candidates` — and **C2 can
abort inside the flush pre-pass, before `candidates` exists at all**, so there is no tail and advancing
would retire material the round never looked at (SCHED-B3's starvation, reintroduced through the new
control flow). Both rules now treat an interrupt as the one cut that **advances no watermark and re-arms
nothing**: the next round is offered the identical set, and the abort path stops touching variables and
perturbing every other rule's filter on the way out of a solve that is being terminated. Pinned in both
unit tests (`*_touch_mark == 0`, `*_mark_advances == 0` after an abort).

**▶ THE VACUITY GUARD, which is the point of the item.** Task #10's abort paths passed **4 × 10,000 fuzz
cases while inert**, because the sites were gated on `!(++tick & 255)` and nothing smaller than one poll
window ever reached a first check — and every CNF the harness generates is smaller than that. The first
cut of this entry reproduced that mistake (a 256-candidate mask with a `k.stop_fuzz ||` bypass in front)
and it is **gone**: the hazard is closed STRUCTURALLY, not tested around.
- **No cadence at all.** One poll per heavyweight iteration, so the FIRST candidate reaches the first
  check. A candidate is up to `CLSLIM × OCCLIM` partner visits, so the poll is free at that granularity.
- The predicate is `kernel_rule_interrupt(k, k.stats.subsume_interrupted)` — the X1/X2/X4 share's helper,
  which polls **and** performs both increments, so **no call site increments anything** and a new abort
  site cannot be added uncounted.
- `subsume_interrupted` / `strengthen_interrupted` (`c interrupt2`, the `c interrupt3` line's counterpart)
  attribute a fire to the rule; `rule_interrupted` alone cannot, since five rules now share it.
- The unit tests drive each entry point separately with the flag already raised, the landed
  `test_bce.cpp` shape, each with a negative control on the same formula. C2's candidate-loop and cascade
  sites cannot be reached that way — the flush pre-pass always polls first — so they are reached through
  the stress cadence, with `strengthen_checked > 0` at the abort as the witness that the flush completed
  first, and a sweep that finds nothing fails loudly rather than leaving the sites unproven.

**No behaviour change without an interrupt**: with `stop == nullptr` and `stop_fuzz == 0` each site is one
predicate call and a branch, and `c interrupt2` reads `0, 0` on every normal solve (all 14 non-interrupt
tasks of probe `1585160`). **So this needs no A/B.**

**▶ NON-ZERO FIRE COUNTS ON REAL INSTANCES, all on the SHIPPED (cadence-free) binary.** Probe `1585247`,
counters only — no timing is claimed and none is needed:
- **Real SIGTERM**, i.e. the actual §6 path. `0 stress fires` on every one of these, which is what proves
  the abort came from `*k.stop` and not from the diagnostic knob:
  - `DivS_862_11` at 25 s → **C1 1, C2 0**. The C1 round it interrupted had done **2,335,884,737 partner
    visits** and was still going.
  - `DivS_862_11` at 60 s → **C1 0, C2 1**, that C2 round at **2,504,317,968 visits**.
  - `constraints_18_0.3_2` at 40 s → **C1 0, C2 1**.
  - `j3037_9_rggt_b` at 40 s → **0, 0**: the signal landed outside C1/C2. Recorded because a probe that
    only reports its hits is not evidence about the rest.
  - All exited cleanly with `s UNKNOWN` and a flushed proof. **Before this item the same SIGTERM waited
    out a two-billion-visit round** — the SIGKILL-loses-the-proof scenario, reproduced and then closed.
- **Stress cadence** on real instances: `DivS_862_11` **9 C1 + 9 C2** (of 30 source fires), `9vliw_bp_mc`
  **0 C1 + 6 C2** (of 21), `rbsat` **0 C1 + 5 C2** (of 7). **Verdicts unchanged from the shipped arm** —
  `9vliw` UNSAT, the other two SAT — so aborting mid-round changed no answer.
- **At FUZZ scale, which is where the last vacuity hid** (probe `1585259`, 400 harness-generated cases,
  `benchmarks/probes/h1-fuzzscale-reach.*`): **5,326 C1 aborts and 5,510 C2 aborts** over 62,987 source
  fires. The same probe on the cadenced first cut read 4,694 / 4,817 over 49,800 — **removing the cadence
  raised reachability by 13 %/14 %**, which is the mask's cost stated as a number instead of an argument.

**▶ THE `battleship-14-26-sat` RESIDUAL IS NOT C1/C2 — ruled out by counters, not by a timing run.** The
X1/X2/X4 third handed over a 7.3 s post-SIGTERM exit on that instance with `c interrupt 0` and BCE/X2/X4
at zero rounds. Probe `1585160` ran it to a SAT verdict, so its counters are C1/C2's ENTIRE involvement
in that solve: **C1 one round, 196 candidates, 1,274 partner visits; C2 one round, 196 candidates, 15,106
partner visits — and both rounds COMPLETE.** Sixteen thousand partner visits cannot be seven seconds, and
a round that completed was never in flight to be interrupted. **The 7.3 s is search, Vivify, or the proof
flush; C1/C2 are excluded.** Routed back rather than chased — narrowing it further needs a timing
measurement, which this window cannot honestly produce.

### 2026-08-05 — SCHED-G2 (1/2): the instrument, and what will count as decisive

**The question is not "does C1 fire".** C1 fires — `forward_subsumed` has always been non-zero. The
question the track asks is whether C1 removes clauses **C2 would not have handled anyway**, and counting
subsumptions cannot answer it: every clause C1 deletes is a clause C2's fused round could also delete.

**Half of it is settled in the SOURCE, with no run at all.** C1's per-pair test `subsumes_marked` is
exactly C2's `check_marked` with the flip count pinned to zero; both rules use the same candidate
predicate (live · irredundant · not root-satisfied · `size <= CLSLIM`), the same **1000/1000**
`CLSLIM`/`OCCLIM`, the same size-ascending order, and the same connect-at-least-occurring-literal index —
and C2 additionally probes the NEGATED phase and cascades. **So C1's deletable set is a subset of C2's,
and C1 can have no unique capability band.** `test_subsume.cpp::test_shadow_ledger` states that as
behaviour rather than as a source reading, over both the large-subsumer and the binary-subsumer case, so a
future divergence in either rule's limits or discovery breaks the build here.

**What the subset argument cannot settle is REACH** — whether C2's next round actually gets to the same
pair, given its own budget, its own `strengthen_touch_mark`, and the early exit a derived unit forces.
That is the whole remaining question, and it is dynamic.

**The instrument: shadow mode, not a plain ablation.** `TACTIX_SUBSUME_SHADOW=1` makes C1 run its full
round and RECORD each victim's clause id (`Kernel::shadow_victims`) instead of deleting it; Strengthen
retires the id on every path that removes the clause (fused subsumption · the flush pre-pass's
root-satisfied delete · a strengthen rewrite, which retires the old id and gets its own counter). The
output is `c c1shadow observed / deleted + rewritten by C2 / residual`.

**Why this beats `TACTIX_SLOT_DISABLE=SubsumeForward`.** The trajectory under shadow mode IS the C1-less
trajectory — C1 takes no action — so the ledger measures the counterfactual in the SAME run that produces
it. A plain ablation only yields two aggregates from two diverged trajectories, and can never say that
*this* clause was picked up. It is also biased the safe way: absorption by any rule other than C2 (BVE
above all) is deliberately not counted, so `residual` OVER-states C1's unique yield.

**Declared before the run.** With `observed` non-zero on real instances:
`absorbed/observed >= 0.9` ⇒ **C1 is inert**, its 100 ‰ share is duplicated work and the elimination group
should be re-dosed. `<= 0.5` ⇒ **C1 earns its share** and the reach gap is real. Between the two ⇒ report
the split rather than a verdict, and name which instances carry the residual. `observed == 0` on every
instance is not a "C1 is inert" result — it is a broken probe, and the arm must be rejected
([[ab-arm-identical-artefacts]]).

### 2026-08-05 — SCHED-G3: Vivify trail reuse (+ F5's clause bit, tier3, H1's Vivify half)

Branch `sched-g3-vivify`. Design frozen FIRST at `docs/design/2026-08-05-vivify-trail-reuse.md`; ctest
**44/44**, all four config/doc checkers green.

**The finding that shaped the item: the reuse loop is not the mechanism, the ORDER is.** Both references
say so at the use site. cadical `vivify.cpp:35-42` is explicit — *"instead of building a trie we simply
sort clauses and literals in such a way that we get the same effect … pick a global literal order …
favor literals with more occurrences first … then sort clauses lexicographically"* — and kissat does the
same in `vivify_sort_clause_by_counts` + `worse_candidate`. Our probe order was **clause literal order**
and our candidate order was `(irredundant, size, ref)`, under which adjacent candidates share a leading
literal only by accident. **A reuse loop bolted onto that scheduling would have measured a reuse rate
near zero**, which is the [[ab-arm-identical-artefacts]] outcome predicted rather than discovered. So
the orders landed WITH the reuse, and the off arm reverts both — an off arm that kept the new order
would not be measuring the mechanism.

One deliberate divergence: kissat permutes the literals IN THE ARENA and therefore brackets the round
with `flush_large_watches`/`watch_large_clauses`. We keep the order in a round-scoped side vector and
sort only the local probe list, so neither arena is touched and there is no watch rebuild.

**The validity invariant for a reused prefix (design §3, VP1/VP2/VP3) rests on ONE rule:** every
operation that can invalidate a retained trail — rewrite, deletion, root unit, GC — is preceded by
`reset_to_root()`, so a trail is never carried ACROSS a database change, only across candidates that
changed nothing. There is no generation counter to get wrong because no state outlives what it
describes. That one rule also discharges `kernel_probing_end`'s "no clause added/deleted/relocated in a
scope" contract, its restore precondition, and kissat's `reestablish_watch_invariant_for_candidate`
(which exists only because kissat shrinks clauses in place at a non-zero level; we never mutate above
the root). Three things it does NOT discharge, each handled explicitly:
- **Root values vs current values.** `is_candidate` and the probe list read `k.value`, correct only
  because the level used to be 0 on entry. The root-satisfied **deletion** path goes through the same
  test, so with a retained trail a speculatively satisfied clause could have been deleted — the one way
  this change could produce a WRONG answer rather than a slow one. Both now read `root_value`
  (kissat's `kissat_fixed`).
- **The candidate as a trail reason.** The prefix was propagated ignoring the PREVIOUS candidate, so the
  current one may be a reason on it. Not only a quality issue: in the implied-negation case the
  candidate's id is the CLOSING LRAT hint, and a reason walk that also reached it would emit the same id
  twice in one chain — a hint already satisfied on replay is a rejection, not a warning. Both references
  check exactly this and backtrack below it (kissat `vivify.c:995-1005`, cadical `vivify.cpp:940-965`),
  and so do we, in O(|c|).
- **Leftover levels.** A value is read only where `k.level == i - 1`, i.e. where only this candidate's
  assumptions hold — kissat's `assert (!value || LEVEL (lit) <= level)`. The reuse branch reads nothing.
  We additionally trim `k.level` to `|probes|` up front so the trail cannot even hold an assumption
  outside the candidate.

**The count order forced a second closure, and it was not optional.** Under clause order the implied
literal was usually the next position and the lazy `v > 0` test caught it; under the count order it can
be anywhere later, so without kissat's **eager implied-literal scan** (`vivify.c:1078-1090` — mark the
probe literals, scan what each propagation newly assigned) the new order would strictly LOSE
strengthenings the old one found. `test_lcm_scope_and_satisfied_delete` failed on exactly that and is
what surfaced it. Still open: kissat's `better_implied` refinement (`vivify.c:1101-1117`).

**REACHABILITY — the mechanism is live, and the off arm proves the counters move** (probe `1585201`,
`benchmarks/probes/scratch/g3/`, three real Tier C instances):

| instance | reused | probes | reuse rate | tier3 | rearm/rounds |
|---|---|---|---|---|---|
| `1-ET-256-K-70` (UNSAT) | 62,605 | 165,663 | **27.4 %** | 1,177 | 24/158 |
| `EDP3-11000` (SAT) | 142,650 | 451,380 | **24.0 %** | 800 | 21/73 |
| `Ptn-7824-b18` (SAT) | 830,950 | 2,198,892 | **27.4 %** | 5,468 | 100/423 |

`TACTIX_VIVIFY_REUSE=0` on `EDP3-11000`: **0 reused**, and only **31,050 candidates checked** against
the ON arm's **122,548** for the same tick budget — a 3.9× widening of what the budget reaches.
`TACTIX_VIVIFY_TIER3=0`: **tier3 0**, confirming the fourth pass is what populates that band.
cadical claims 30–50 % of decisions saved; 24–27 % is the same order.

**▶ `vivified` COLLAPSES AND THAT IS NOT THE OBJECTIVE — read solve time.** The count drops hard:
`EDP3-11000` 1,155 vs the off arm's 1,938, `Ptn-7824-b18` **9,211 vs 364,859**. My first reading of that
was that the yield went the wrong way; it is the wrong number to read. Paired single-instance times, same
node, same binary, 400 s cap:

| instance | shipped (both ON) | `TACTIX_VIVIFY_REUSE=0` | `TACTIX_VIVIFY_TIER3=0` |
|---|---|---|---|
| `EDP3-11000` | **28.6 s** | 29.8 s | **62.8 s** (2.2×) |
| `Ptn-7824-b18` | **273.9 s SOLVED** | **400 s TIMEOUT** | — |

So on both instances the shipped arm vivifies far less and is FASTER — on `Ptn` the off arm burns
15.97 M probe assumptions to strengthen 364,859 clauses and does not finish. Vivification count was never
the objective, and D-VIVIFY-SCHED's "this is a YIELD fix" note has to be read as "yield per tick", not
"clauses rewritten". **This is still TWO instances and is NOT a verdict** ([[chaotic-instance-veto]]);
the §5 A/B is what decides it, and its abandon-the-window rule stands. Peak RSS 249 MB vs 113 MB — the
round-scoped probing log is real but small, and 249 MB is far under the 8 GB/core allocation.

**C10 hand-off H3 LANDED, not deferred** (`SP-TIER-2` removal-path T3, now closed in
`implementation-notes.md`): `Kernel::vivify_scope` grows to name the band — 3/4/5 = kissat's
tier1/tier2/tier3 — and `run_vivify_dose` spends the redundant budget as three passes at 3/3/7, 3/3/7,
1/7. The "3+3+1 = 7" the policy comment was already claiming was **only ever accounting**: the redundant
side was one pass bounded by `tier2`, so **tier3 was never vivified at all**. This is also the first
live `k.tier1` consumer outside reduce in either direction. `Stats::vivify_tier3` is its own vacuity
witness and is non-zero on all three instances above.

**SCHED-F5 and SCHED-H1's Vivify half** are ticked in the checklist with their own evidence.

**FUZZ: SIX LEGS, 10,000 CASES EACH, 0 FAILURES** (60,000 cases, corpus 14/14 on each) — jobs `1585243`
native DRAT · `1585244` native LRAT · `1585245` `--random-policy` × DRAT · `1585246` `--random-policy` ×
native LRAT · `1585254` **`TACTIX_STOP_FUZZ=97` × `--random-policy` × native LRAT** (the H1 abort path:
the stress cadence bypasses every window, so the new poll is live on cases far smaller than one) ·
`1585255` **`--probing-stress-policy` × native LRAT** (the new-schedule leg: it drives Vivify's CANONICAL
round densely against Probe/Backbone/TransitiveReduce/Congruence in random order with
`kernel_check_invariants` after every step, which is exactly the surface trail reuse lives on). Verdicts
agree between the stressed and unstressed legs modulo timeouts (SAT 5026 vs 5027), so interrupting a
round changed no answer and every UNSAT still produced a checkable chain. **An earlier submission of the
same six was cancelled and redone**: `build/debug/tactix` was rebuilt underneath them mid-flight, which
makes a leg mixed-binary and its green meaningless — the fuzz analogue of the build-dir rule.

**A/B SPEC (not run — no tier campaign here, and the 4-core allocation is oversubscribed).** Same
window, same binary, node-pinned, Tier C, `--baseline kissat --baseline cadical`; two pin dirs if a
Tier B arm goes out concurrently. Arms via the shipped hatches `TACTIX_VIVIFY_REUSE=0` and
`TACTIX_VIVIFY_TIER3=0` (both ship ON). **Read `c vivify2` BEFORE reading PAR-2**: `reused` 0 ⇒ the
mechanism is inert and the window is abandoned; `tier3` 0 ⇒ the tier3 half is inert independently.
Paired draws before any single-instance flip is treated as a verdict ([[chaotic-instance-veto]]).
**Ids: no new SCHED id was minted** — G3, F5 and H1's Vivify half were all already on the board
(G4 and H4+ remain free).

### 2026-08-05 — SCHED-F6: the redundant half of the substrate, shipped OFF on purpose

Requested after `SCHED-C10`, whose SP-TIER-2 audit left exactly this residue: cadical's `mark_added`
family branches on `likely_to_be_kept_clause`, and **we do not populate the redundant half of the mark
substrate it scopes**. `kernel_likely_kept` now implements all THREE disjuncts — irredundant · within the
live `tier2` · within the reduce **survivor envelope**, the one we had no counterpart for, now published
as `Kernel::kept_glue`/`kept_size` out of `reduce_apply_refs` (cadical `reduce.cpp:147-156`).

**It ships OFF, and that is the finding rather than a hedge.** Extra stamps can only WEAKEN a round filter
— more marks, fewer skips — and every stage-F consumer (C1, C2, BCE, X2, X4, Factor) scopes itself to
irredundant clauses. So *on its own* the redundant half is a measured cost buying nothing, and turning it
on by default would spend the 52 % C1 saving F1 measured to populate a set no rule reads. What it buys is
the PREREQUISITE for `SCHED-F7`, the consumer half (open C1/C2's scope to the band, cadical
`subsume.cpp:388`) — and C10 declined to wire that first for precisely the right reason: a consumer that
filters an unpopulated set produces byte-identical artefacts and a green-but-vacuous arm.

**The anti-vacuity pair is counted unconditionally, so it costs nothing to check.** `c touchred` prints
`touch_redundant_dynamic` against `touch_redundant_static6`: the first > 0 proves the stamps fire, and the
two DIFFERING proves the scope tracks the live tier rather than a frozen 6 — which one count cannot show,
and which matters because the blast radius C10 measured is real (`battleship-14-26-sat` live tiers 26/34,
where a static-6 consumer admits **nothing**: 1,025,943 dynamic clause-visits against 0 static).
`test_touch_redundant_scope` pins all three disjuncts separately, including that the envelope readmits a
clause `tier2` alone excludes.

**▶ THE PRE-REGISTERED CHECK PASSES, AND ON THE INSTANCE C10 NAMED IT PASSES BY AN INFINITE MARGIN**
(probe `1584772`, `c touchred`, counters only). Redundant creations in scope, dynamic vs static-6:
`battleship-14-26-sat` **345,038 vs 0** — live tiers 26/34, so a frozen-6 predicate admits *literally
nothing* there and every one of those 345 k stamps exists only because the tier is dynamic ·
`9vliw_bp_mc` **236,570 vs 49,229** (4.8×) · `rbsat` **265,255 vs 147,855** · `hwmcc17miters` 0 vs 0,
which is consistent (it refutes at 1 conflict, so nothing is learned). Both halves hold: `dynamic > 0`
says the stamps fire, and `dynamic != static6` says the scope MOVES with `k.tier2`. **`SCHED-F7` is
therefore unblocked and provably non-vacuous** — the set its consumer would read is large, and on
`battleship` it is a set that did not exist at all before C10's dynamic tiers.

### 2026-08-05 — STAGE F A/B SPECS (all four landed items, none priced) + the F5 stop

**Arms, same-window Tier B + Tier C, two pin dirs from the same commit (CLAUDE.md build-dir rule), node
pinned, `--baseline kissat --baseline cadical`.** Knobs go in the command, not the environment.

| arm | command | what it isolates |
|---|---|---|
| `touchon` | shipped default | F1+F3+F4 together (one kernel switch governs all the round filters) |
| `touchoff` | `TACTIX_TOUCH_MARKS=0` | the whole suppressor family off; restores the pre-stage-F candidate sets exactly, and the re-arm loops are skipped too so the arm is genuinely inert |
| `holdoff` | `TACTIX_SUBSUME_HOLD=0` | F2's elimination-loop wire alone. **Run this one ONLY if `c subhold` is non-zero somewhere** — it read 0 on all four probe instances, so today this arm is predicted byte-identical and would be a wasted window |

**PRE-FLIGHT, checked before any PAR-2 is read** ([[ab-arm-identical-artefacts]]): on the ON arm
`c touch` / `c touch3` / `c touch4` must all show non-zero `skipped`, AND the rule's own `checked`/`ticks`
counter must stay non-zero — all-skipped is a wedge, all-checked is an inert filter. On the OFF arm every
`skipped` must be 0. **Ship criterion:** `#solved` first, then Tier C PAR-2 outside the 6.72 noise floor,
with the fragility line read (`k=1` is not an answer). `mod4block` and the rest of the may-not-gate set do
not decide this alone — and `mod4block` already flipped SAT→UNKNOWN→SAT across the two probes, which is
precisely why.

**Measured at the source (probes `1584708` F1, `1584753` F3, `1584772` on the MERGED tree; counters
only, no wall clock).** On the pre-merge branch: `rbsat` C1 partner visits **165,272 → 79,686**, C2
**5,827,200 → 2,571,322**; `mod4block` BCE ticks **14,943,039 → 7,138,926** with MORE deletions
(4,133 → 6,257) and X2 ticks **32,128 → 18,373** for 2,898 resolvents instead of 8,118; `9vliw` BCE
175,810 literals skipped. Telemetry self-check clean (0 `***UNACCOUNTED***`) on every arm.

**▶ RE-READ AFTER THE MERGE: WHICH CONSUMER CARRIES THE STAGE IS PER-INSTANCE, so do not quote one
instance's split for the tier.** On the merged tree (Stage E's cursor + E3's ceiling + C8's ITE/XOR gates
in front of it) the substrate is busier — `9vliw` 4,895,219 touch marks against 2,102,782 before, because
more rules now reach more candidates and every rewrite is an add — and more marks means fewer skips.
Where that lands differs completely by instance: `9vliw` C1 drops to **4,501** skipped (95,965 on the
branch alone) and the work moves to BCE **177,060** and Factor **65,176**; `rbsat` C1 **74** with Factor
**88,932**; `battleship` is Factor **36,036** and nothing else at all; while `mod4block` is still
overwhelmingly C1/C2 — **782,717 / 781,290** skipped, plus X2 16,002. So the stage fires hard everywhere
it fires, but through a different consumer each time, which is an argument for pricing the family
together (one `TACTIX_TOUCH_MARKS` arm) rather than per item. **The one firm negative: quoting the
pre-merge C1/C2 numbers as HEAD's is wrong on three of the four instances.**

**F6's ships-OFF decision is now MEASURED, not just argued.** `TACTIX_TOUCH_REDUNDANT=1` on the same
instances: `9vliw` touch marks 4,895,219 → **9,715,099** (2.0×) and `battleship` 18,434 → **15,127,577**
(820×), for **zero additional skips anywhere** — C1 4,261 vs 4,501, BCE 178,812 vs 177,060, i.e. noise in
the wrong direction. Exactly the predicted shape: without `SCHED-F7` the redundant half is pure cost.

### 2026-08-05 — SCHED-F3: BCE is why the substrate has TWO stamps

X1 BCE, X2 HyperTernaryRes and X4 Instantiate all got the F1/F2 treatment — filter the canonical round,
re-arm the tail, advance the watermark — but BCE does not fit the substrate as F1 built it. **A clause
becomes blocked on `l` either because it appeared or because an opposite occurrence WENT AWAY**, and
deletion is deliberately not a `touched` producer (it can never create a subsumer, and marking on it would
weaken C1/C2's filter, which is where the measured saving is). Folding deletions in would have paid for
BCE out of C1/C2. So `Kernel::removed` is a **second array on the same clock**, and BCE reads
`kernel_changed_since` while everyone else reads `kernel_touched_since` — which is cadical's own line:
`mark_added` and `mark_removed` set different flags (`internal.hpp:1125-1137`), and `block.cpp:730-733`
says outright "we do not have to try BCE on it again until irredundant clauses with its negation are
removed". Pinned by `test_bce.cpp::test_touch_filter_is_policy_only`, whose second half asserts that a
DELETION with no add re-arms the literal.

Two more per-rule readings of the reference rather than a copy of C1's: HyperTernaryRes uses `need`-free
per-pivot marking because cadical's `ternary` flag is per variable and its comment states the same
predicate (`ternary.cpp:258-266`); Instantiate uses `need = 1`, not 2, because its probe is a
PROPAGATION and one new clause over one of the candidate's variables can change the outcome — the
"a new subsumer needs two shared variables" argument does not transfer. Instantiate's filter is also
deliberately WEAKER than cadical's, which sets a per-clause `instantiated` bit once and never resets it
(`instantiate.cpp:32,211`): ours re-offers the clause as soon as anything in it changes, recovering the
instantiations the once-ever bit forfeits.

**Unlike C1/C2, these three rounds ARE budget-cut under DefaultPolicy** (all three are `budget_aware`,
HyperTernaryRes additionally has the `ternarymaxadd` output governor), so their resume path is reachable
and its `rearmed` counters are live rather than test-only.

**▶ NOT DONE: the delay ladder for metered entries.** It is `!e.meter`-gated inside DefaultPolicy's slot
loop, and giving metered entries a productivity ratio is per-entry policy bookkeeping in the file Stage E
owns. Deferred deliberately rather than collided on.

ctest **44/44**.

### 2026-08-05 — SCHED-F2: resume by RE-ARM, and the bound coupling T5 asks for is refused as self-wedging

**Resume.** F1 blocked a cut round's watermark, which re-offers the HEAD as well as the tail — SCHED-B3's
starvation with extra steps. F2 replaces that with kissat's own arrangement: re-arm the variables of the
candidates the round never reached, then advance the watermark like any other round, so the next candidate
set is {unreached tail} ∪ {new material}. Doing it in the SUBSTRATE rather than through a positional cursor
is what makes it survive the round's own `kernel_collect_garbage`, which invalidates every Ref — a cursor
would have needed a GC-stable key that does not exist. Sharper than kissat on one point: a tail candidate
that fails the filter is not re-armed (its check provably cannot fire under the new, higher watermark).

**▶ THE COUPLING IS REFUSED, AND THAT IS THE RESULT.** T5 lists "C1/C2 completeness → BVE's bound advance".
Reading both references at the use site: **neither does it.** kissat's forward `complete`
(`forward.c:664-670`) gates the elimination ROUND LOOP (`eliminate.c:519-520`) and nothing else; the bound
advance is a different predicate, computed after the loop and reached with subsumption incomplete whenever
the loop exits on rounds or budget. And the conjunct would be **self-wedging**: `strengthen_complete` is
false on any productive round, BVE's resolvents are C2 material by construction, so BVE at bound b feeds
C2, C2 stays productive, and the bound never advances — SP-SLOT-5's failure mode, reproduced by
construction. Full row + removal path: `implementation-notes.md` §13.3 `SP-REPEAT-COUPLE`.

**What landed instead** is the reference's real wire — an incomplete C1/C2 holds the elimination outer
loop open — and it is **INERT under the shipped DefaultPolicy, measured, not assumed**: `Stats::
subsume_holds` (`c subhold`) is 0 because `midround_budget` is off so no round leaves a tail, and every
other way to leave one already sets `round_changed`. Labelled inert per the SP-SLOT-6 convention; must not
be cited as a result. It goes live the moment a budget binds, which is SP-EFFORT-CEIL-1's job.

**The resume path is likewise unreachable under DefaultPolicy** for the same reason, so it is pinned by
`test_touch_resume_rearms_the_tail` rather than by a leg ([[ab-arm-identical-artefacts]] — the SCHED-C6
precedent for a path no policy reaches).

### 2026-08-05 — SCHED-F1: the touched-epoch substrate is a STAMP, so it has no clearer to starve

**Measured at the source (probe `1584708`, 4 Tier B instances × on/off, counters only).** The filter fires
hard and does not wedge: `rbsat` C1 6,855 skipped / 5,187 checked and partner visits **165,272 → 79,686
(−52 %)**, C2 **5,827,200 → 2,571,322 (−56 %)**; `9vliw` C1 −17 %, C2 −14 %; `mod4block` C1 **515.9 M →
326.5 M**. `advances` equals the round count on every instance (17/17, 18/18, 54/54) and `checked` stays
large throughout — both halves non-zero is the non-wedge evidence. `touch marks` is identical across arms
(170,879 / 170,879 on `rbsat`), i.e. the producers are independent of the consumer, which is the property
that makes the stamp unstarvable. `TACTIX_SLOT_STATS=1` self-check clean (0 `***UNACCOUNTED***`).
**Caveat, and it is a lottery, not a verdict:** `mod4block` went SAT (1.46 M conflicts) → UNKNOWN at 300 s
while doing 6.99 M conflicts — 4.8× more search in the same window. It is on the may-not-gate list and a
SAT trajectory flip is exactly what that list exists for; paired draws before anyone reads it as a cost.


`Kernel::touched[v]` holds the value of a monotone `touch_epoch` at v's last change; a consumer keeps a
watermark and reads `touched[v] > w`, "clearing" by snapshotting the epoch. **That shape is the direct
answer to D3's finding.** A per-consumer BIT would need a clear pass, and a clear pass that a scheduling
decline can starve is exactly what wedged the elimination clock shut (`SP-SLOT-5`). A stamp has no clear
step at all, and its producers sit on the kernel's clause-CREATION path (`kernel_add_large_clause`
irredundant, `kernel_add_binary_clause`, `kernel_reserve_vars`, `kernel_touch_sync_fixed`), which no
policy can decline. **Non-wedge is then a theorem, not a hope:** with no add and no root fix the
irredundant clause set only shrinks, and subsumption/strengthening are monotone in it, so "nothing
touched" really does mean "nothing to find". Deletion is deliberately NOT a producer — it can only remove
subsumers — which is where this mark set differs from `bve_retry`'s.

**Two things the naive port would have got wrong.** (1) kissat's `connect_subsuming` (forward.c:488-492)
demands `flags->subsume` on EVERY literal, so an unscheduled clause is dropped from the round's subsumer
index and old-subsumes-new is lost. Our index is round-local and cheap, so a skipped candidate is still
CONNECTED, only not CHECKED — the filter then costs no subsumption the unfiltered round would find.
(2) A root fix makes a clause effectively shorter without any rewrite. Rather than kissat's occurrence
walk we stamp only the fixed variable and let the membership test read the case off it
(`value(l) < 0 && touched_since(var_of(l))`), so `kernel_touch_sync_fixed` is O(new units), not
O(their occurrences).

**N1.** The filter is consulted by the canonical ROUND only. `subsume_forward_candidates` /
`strengthen_candidates` (what `instances()` enumerates) and the single-candidate `*_try` paths stay
complete, so no API caller loses a legal application — the distinction the `Factor` breach
(`SCHED-N1FIX`) turned on. Pinned by `test_subsume.cpp::test_touch_filter_is_policy_only`, which advances
the watermark past everything, asserts the round declines, and then asserts `instances()` still offers the
clause and `apply_instance` still subsumes it.

**Completeness.** A watermark advances only for a round that reached the end of its candidate list; a
budget-cut round (or one ended by a derived unit) leaves it, so its survivors are re-offered.
`test_touch_filter_cut_round_does_not_advance` pins that.

ctest **44/44**. **UNPRICED.** Off arm `TACTIX_TOUCH_MARKS=0` (kernel-side, so it applies under
`--random-policy` too); pre-flight counter the `c touch` line — `marks` must keep growing, and per rule
BOTH `skipped` and `checked` must be non-zero (all-skipped = wedged, all-checked = inert) with
`advances` > 0.
### 2026-08-05 — SCHED-H2: a reused policy object was carrying eleven state groups into the next solve

**Not hygiene.** N1 lets any policy drive the solver and the §5.6 learner will run many solves from ONE
policy object per process; state carried from solve *n* to *n+1* makes a trajectory — and any RL reward
computed from it — a function of episode ORDER. It also sits on the incremental/competition path.

**THE BLAST RADIUS, established before anything was changed.** Fifteen groups, classified, each against the
reference's own use site (`kissat search.c:45-70 start_search` + `kimits.c:99-136 kissat_init_limits`;
`cadical internal.cpp:499-608 init_search_limits`, whose restart/rephase/stabilize/reluctant/elimbound
family is reset **"in any case"**, incremental or not).

| state | verdict | reference |
|---|---|---|
| `averages[2]` (per-mode glue + decision-rate EMAs) | **LEAKED** | kissat `search.c:56 kissat_init_averages`; cadical `internal.cpp:582 init_averages` |
| `restart.next_conflicts` | **LEAKED** | cadical `internal.cpp:568` "reset in any case"; kissat `kimits.c:120` |
| `reduce.next_conflicts` | **LEAKED** (re-based only on the `sqrt` arm) | kissat `kimits.c:110`; cadical `internal.cpp:508` |
| `mode.{count,conflicts_limit,ticks_limit,ticks_at_focus_entry}` | **LEAKED** | kissat `mode.c:19-40 kissat_init_mode_limit`; cadical `internal.cpp:596-601` |
| `reluctant.{u,v,wait,trigger}` | **LEAKED** | kissat `reluctant.c:4-14`; cadical `internal.cpp:603-608` |
| `rephase.{next_conflicts,count}` | **LEAKED** | cadical `internal.cpp:559-562` "in any case"; kissat `kimits.c:116` |
| `reorder.{next_conflicts,count}` | **LEAKED** | kissat `kimits.c:113` (cadical has no reorder) |
| `walk_anchor` | **LEAKED** | cadical `last.walk.ticks` / `SET_EFFORT_LIMIT`'s `TICKS − LAST` window |
| `probe_clock` / `elim_clock` (+ their new-material watermarks) | **LEAKED** | kissat `kimits.c:126-135 INIT_CONFLICT_LIMIT(probe/eliminate, true)` |
| `inprocess.{initialized,ticks}` | **LEAKED**, and the worse one — the accumulator pairs with the kernel's ONE global tick meter, so on a fresh kernel `search_ticks()` UNDERFLOWS | kissat keeps `search_ticks` as its own counter |
| `SlotEntry` runtime (`anchor`, `limit`, `applies`, `cursor`, `tm`, `ord`, the three marks, the delay ladder) | **LEAKED** | cadical `Last` / `last.NAME.ticks = TICKS` |
| `DefaultBranching` randec (`until`, `next`, `sequences`, `rng`) + `DefaultCandidateOrder::rng_` | **LEAKED** | kissat re-seeds `solver->random = seed` at `search.c:70`; `kimits.c:107` re-bases the randec limit |
| `bve_rounds_run_`, `bve_level_`, `SlotEntry::doses` | already reset | kissat `kimits.c:128-130`; cadical `internal.cpp:468` |
| `bump_reasons_delay_`, `retier_`, every env arm, `default_reduction_.ramp` | already re-read/reset per solve | — |
| sub-policy pointers, slot COMPOSITION (rule/enabled/group/permille/meter/thresh/max_delta/dosed/`cursor_enabled`/`order`), `randec_enabled`, `chrono_levels` | **deliberately persistent** — configuration, and what a posed slot carries | — |

**The fix is `DefaultPolicy::reset_schedule(k)`, called at solve entry after the env arms.** It re-BASES
rather than zeroes (`conflicts + delta`, `anchor = search_ticks(k)`), which is why one reset serves both a
fresh formula and an incremental continuation — exactly kissat's `CONFLICTS + SCALED` and cadical's
`stats.conflicts + opts.NAMEint`. **The shipped single-solve path is bit-identical**: every counter is 0 at
solve entry (nothing propagates during ingest), so each re-based value equals its constructor default —
observed (the pre/post traces below agree to the byte) and now PINNED:
`assert_reset_is_a_noop_on_a_fresh_kernel` asserts the zero-counter precondition and compares 23 re-based
fields against a pristine policy, so a future re-based field whose zero-counter value differs from its
default fails a test instead of quietly shifting every tier number.

**PRE-FIX FAILURE, and it is not subtle** (`tests/test_solve_reset.cpp`, 160-var random 3-SAT at ratio 4.7,
UNSAT). Solve #2 on a reused policy vs solve #1 on a fresh one:

| | conflicts | decisions | props | ticks | restarts | proof |
|---|---|---|---|---|---|---|
| reused #2 (pre-fix) | 300 | 616 | 8547 | 35,226 | 49 | 7,033 B |
| fresh #1 | 3583 | 4979 | 141,513 | 1,319,661 | 115 | 165,241 B |

Two different answers to the same question from the same binary. The reused arm skipped the whole
preprocessing phase (`inprocess.initialized` still true) and restarted on a stale limit. Post-fix the two
rows are identical in all 16 compared fields including the proof bytes.

**REACHABILITY AT THE SOURCE.** The test asserts the dirt as well as the identity:
`state groups dirty after a solve: 12/12` — every group the reset touches is genuinely non-pristine after
one real solve, so the identity assertion cannot go vacuously green
([[ab-arm-identical-artefacts]]). `DefaultPolicy::solves` counts entries and is asserted 2 vs 1.

**The posing surface survives.** `reset_schedule_per_solve` (default true) is the opt-out for a test or
preset that poses a schedule and wants `solve()` to honour it; `test_restart_reduce`'s restart storm sets
it, and would otherwise have quietly degraded into "PHP(4,3) is UNSAT". The test pins both directions.

**Gates:** ctest **45/45** (44 + `solve_reset`), `config_wiring` / `effort_specs` / `option_coverage` green
(no `Kernel` config field added — the two new fields are `DefaultPolicy`'s), corpus 14/14. Fuzz, both
formats, `--random-policy`, **6000 cases each and 0 failures on all four legs**: on-branch `1584972`
(native DRAT) / `1584978` (native LRAT), and **re-run on the merged tree** after the rebase onto the
Sweep-governor + Stage-F merge — `1585219` / `1585222`, 1886/1887 UNSAT answers proof-checked with the
restatement path reached on 1718 cases.

**No A/B is owed.** The change is provably inert on the one-solve-per-process path the tiers measure; a
tier window would only re-measure noise. What it buys is the multi-solve harness the §5.6 programme needs.

### 2026-08-05 — SCHED-E3: the effort ceiling exists now, and it is measured inert

Closes ledger row **`SP-EFFORT-CEIL-1`** (`docs/implementation-notes.md` §13.2), the ceiling half of the
effort budget — SCHED-B1 landed the floor/threshold half and the row was miscredited to it until A3 caught
the split. Filed as `SCHED-E3` rather than a Stage-C id because two agents collided on `SCHED-C8` today.

**READING THE USE SITES CORRECTED THE ROW'S OWN HEADLINE.** "Every reference effort budget has an UPPER
clamp" is not true. There are THREE schemes:
- **kissat `SET_EFFORT_LIMIT`** (`kimits.h:135-170`) — `REFERENCE = search_ticks − last`, floored at
  `mineffort`·1e6, `DELTA = effort·REFERENCE`, `LIMIT = OLD + DELTA`. **Floor only. kissat has no
  `maxeffort` option at all** (grep: `mineffort` is the only member of the family). **Our budget is a
  transliteration of exactly this**, so against kissat we already matched and the row over-stated the gap.
- **cadical's ticks macro `SET_EFFORT_LIMIT`** (`limit.hpp:136-164`) — neither floor nor ceiling.
- **cadical's PROPAGATIONS scheme** — the only clamped one, and it is an ORDERED composition whose last
  step is a FLOOR: `max(delta,mineff)` → `min(delta,maxeff)` → `max(delta, 2·active())`
  (`elim.cpp:781-787`, `subsume.cpp:351-357`). On a large formula the active floor lifts the delta back
  ABOVE the ceiling. Reading the three as a clamped range gets that backwards, so
  `test_scheduling::test_effort_ceiling` pins the ORDER, not just the clamp.

Ported in cadical's order, to the entries whose currency is the one cadical clamps: **BVE ↔ elim, 2e9
resolutions** and **SubsumeForward/Strengthen ↔ subsume, 1e8 partner visits** (SCHED-B4 made ours the same
unit as `subchecks`). **TransitiveReduce deliberately gets NO numeric ceiling** — cadical's 1e8 is in
PROPAGATIONS and our meter is `transitive_ticks`, a cache-line proxy; the standing method note says transfer
the ratio, never the unit.

**▶ THE MEASUREMENT IS THE RESULT** (job `1584734`, `c slot … maxeff/clamped/floored`):

| arm | BVE | SubsumeForward | Strengthen |
|---|---|---|---|
| shipped (2e9 / 1e8 / 1e8) | clamped **0** | **0** | **0** |
| forced `TACTIX_MAXEFF_*=1e5` | 15 / 19 / 10 | 18 / 21 / 13 | 18 / 21 / 13 |

(three columns of the forced row = `9vliw` / `b22_1` / `sted5`.) `floored` is 0 everywhere: our natural
deltas already exceed 2·active() at these sizes. **And even the forced 20,000×-tighter ceiling leaves
`b22_1` and `sted5` byte-identical** — those rounds never spend to their limit, so the ceiling is not the
binding constraint on our budgets at all. The forced arm is there for one reason: it proves the clamp is
wired rather than dead code, which a `clamped 0` reading alone could never distinguish from a missing call.

**So it ships ON with the inertness recorded, not as a green headline.** One compare per occasion, on
reference-parity grounds; the row closes as CLOSED-and-inert rather than as an accepted deviation, because
we no longer LACK the mechanism. **What the row does NOT close: Congruence and Substitute still have no
meter at all** — that is `SCHED-A2b`'s open finding, and the 131,327 ms Congruence application the A3 row
cited belongs to it, not to the ceiling.

**▶ READY-TO-RUN A/B (`SCHED-E3`) — and the honest recommendation is DO NOT SPEND A WINDOW ON IT YET.**
The pre-flight already ran: `clamped 0` on three instances means the ON and OFF arms are the same binary
behaviour, and a window would measure noise ([[ab-arm-identical-artefacts]] — this is exactly the arm that
passes vacuously). Run it only if a Tier C instance ever shows `clamped > 0` at the shipped constants.
The command, for when that happens:
```
python3 tools/bench/run.py --tier C --slurm --baseline kissat --baseline cadical \
  --solver build/pin-e3-c/tactix \
  --solver 'env TACTIX_EFFORT_CEIL=0 build/pin-e3-c/tactix'
```
Pre-registered counter: `c slot … clamped` > 0 on the ON arm for at least one entry, `clamped 0` on OFF.
**Abandon the window if the ON arm's clamp counts are all zero** — that is the pre-flight, and it currently
fails. The *interesting* experiment is the opposite one and it is a DOSE question, not a parity one: sweep
`TACTIX_MAXEFF_BVE` downward until it binds and see whether a smaller ceiling helps; that belongs with the
phase-6 dosing exhibit, not with this row.


### 2026-08-05 — SCHED-E2: the ordering arms, one knob per rule

Six arms on `CandidateOrderPolicy`, selected per rule by `TACTIX_ORDER_<RULENAME>` (the knob name is
derived from the registry name, so a newly dosed rule gets one without editing a list). `native` is the
shipped default for every rule and stays there — **no arm ships without a measurement**.

**Three arms are reference ports, and two of them read backwards from the option name alone.**
- `heuristic` — kissat `transitive.c:270-294`: transitive probes are sorted by `flags[i].transitive` (the
  untried mark) FIRST, then by EVSIDS heap score ascending in stable mode / VMTF `links[i].stamp` ascending
  in focused mode. We enumerate by literal index, so this is a real unimplemented reference mechanism, not a
  variation. **kissat's mark-then-score composition is also the honest statement about how order and
  resumption interact**: it puts a per-item repetition MARK ahead of the score rather than rotating a scored
  list, which is Stage F territory (`SP-REPEAT-*`) — E1's cursor rotates first and a score arm then
  re-imposes its own head, so under a score arm the cursor survives only as a tie-break. Said in the code.
- `negocc-desc` — cadical `probe.cpp:621-624` ranks probes by `noccs(-p)` and `:763` pops from the BACK, so
  the literal occurring negated MOST often is probed first. The comment ("occur negated less frequently come
  first") describes stack position, not probe order.
- `occ-asc` — cadical's elimination schedule is a priority queue on occurrence count, cheapest first. Ours
  is already occ⁺·occ⁻ ascending — a PRODUCT where the reference uses a SUM — so this is a variation.
`reverse` and `random` are controls: the cheapest test that order matters at all, and the structureless
floor a structured arm must beat. `random` is splitmix64 off a constant seed, because a random order that
does not replay cannot be fuzzed or bisected.

**A knob that cannot take effect ABORTS.** Setting `TACTIX_ORDER_X` on an entry that runs as a canonical
batch round (the policy never sees its candidates) exits with a message rather than being ignored — that is
the vacuous-A/B failure mode, and it is cheaper to make it loud than to discover it in a banked window.

**▶ REACHABILITY, PRE-REGISTERED AND MET** (job `1584734`, probe `e2e3-arms.*`, `c cand-order` `moved`):

| rule | arm | lists ranked | positions moved |
|---|---|---|---|
| Probe | `negocc-desc` | 34/34 | 718,018 / 718,068 |
| Backbone | `occ-desc` | 34/34 | 717,132 |
| TransitiveReduce | `heuristic` | 33/34 | 436,819 |
| BVE | `occ-asc` | 15/15 | 148,409 |

(9vliw; `b22_1` and `sted5` agree for the first three. **BVE reads `lists 0` on both of those** — it is
never dosed there, so its arm is genuinely unexercised and the counter says so rather than the leg passing
vacuously.) The probe build also carried SCHED-E3, whose shipped ceilings measure `clamped 0` on all three
instances, so the ordering arms ran on an unchanged budget.

**UNPRICED, and the tempting number is a bundle.** All four arms at once move `9vliw` 348,630 → 234,825
conflicts and its proof 64.0 → 43.4 MB, and `b22_1` 494,805 → 450,530. That is one instance for the big
number and four knobs at once ([[chaotic-instance-veto]]); it says the axis is live, not which arm is good.

**▶ READY-TO-RUN A/B (`SCHED-E2`) — ONE RULE PER ARM, NEVER THE BUNDLE.** Start with Probe, which has both
the largest reference disagreement and the tightest budget (E1 measured it advancing 0.3 % of its list).
Two pin dirs from the same commit for a concurrent B+C pair; check the shas match.
```
tools/bench/buildlock.py --gc
cmake -S . -B build/pin-e2-b -DCMAKE_BUILD_TYPE=Release && cmake --build build/pin-e2-b -j8
cmake -S . -B build/pin-e2-c -DCMAKE_BUILD_TYPE=Release && cmake --build build/pin-e2-c -j8
sha256sum build/pin-e2-{b,c}/tactix        # must match
python3 tools/bench/run.py --tier B --slurm --baseline kissat --baseline cadical \
  --solver build/pin-e2-b/tactix \
  --solver 'env TACTIX_ORDER_PROBE=negocc-desc build/pin-e2-b/tactix'
python3 tools/bench/run.py --tier C --slurm --baseline kissat --baseline cadical \
  --solver build/pin-e2-c/tactix \
  --solver 'env TACTIX_ORDER_PROBE=negocc-desc build/pin-e2-c/tactix'
```
Pre-registered counter: `c cand-order` `Probe ... moved > 0` on the ON arm, `moved 0` on the OFF arm.
Ship criterion: **#solved not worse on either tier and PAR-2 better than the 6.72 noise floor on at least
one, not worse than it on the other**, may-not-gate set excluded from deciding alone. Then, only if Probe
lands, the same one-knob window for `TACTIX_ORDER_TRANSITIVEREDUCE=heuristic` (the kissat port) and
`TACTIX_ORDER_BVE=occ-asc`. `reverse`/`random` are diagnostics for "does order matter on this family at
all" and are worth a Tier A sweep, not a Tier C window.


### 2026-08-05 — SCHED-E1: candidate order becomes a policy object, and Probe was seeing 0.3 % of its list

**Two commits.** (1) `run_slot_phase` split along the DOSED/BATCH axis — pure restructuring, byte-identical
DRAT + identical trajectory + identical per-entry `c slot` telemetry on `9vliw_bp_mc`, `FmlaEquivChain_4_6_6`,
`b22_1`, `sted5_0x24204-50` (job `1584689`). (2) `CandidateOrderPolicy` + `SlotEntry::cursor`.

**The sub-policy super-class question (H6) is already answered:** `SubPolicy` has existed since S1b
(`a36b4c2`, which un-deferred H6 when `BveBoundPolicy` became the second arm). `CandidateOrderPolicy` is the
fifth category, not a second bespoke one — PROGRESS's Track-H line saying otherwise was stale and is fixed.

**N1, stated precisely.** `rank()` is a PERMUTATION and the cursor is a ROTATION; neither can remove a
candidate, and both live in the policy where no guard can see them. `apply_instance` is unchanged and still
total over every payload from every source — `tests/test_scheduling.cpp::test_cursor_preserves_n1` pins three
separable halves of that: `instances()` returns the same set wherever the cursor is (the rule cannot read it
any more), `apply_instance` on a payload the cursor rotated PAST gives the same result and the same stat
deltas as on a kernel whose policy never had a cursor, and all seven arms are multiset-preserving. The
rule-side rotation and `Kernel::transitive_cursor` are GONE — that was order logic living in a rule.

**▶ THE MEASURED FINDING (job `1584716`, probe `e1-cursor.*`, three arms one window).**
- **The migration is exact.** `cursoff` (the two new cursors off, only B3's migrated one live) is
  byte-identical to the pre-E1 build on all four instances — same proof sha256, same byte count, same
  conflicts. Moving a cursor rule→policy is not supposed to change anything, and it did not.
- **The generalization is emphatically NOT inert.** On `9vliw`, Probe rotates on **44 of 45** doses and
  advances **2,719 of 878,094** enumerated candidates — **0.3 %**. Before E1, Probe re-probed the same
  ~60-literal head on all 45 phases and never reached the other 99.7 % of its schedule. Backbone 3.6 %
  (31,880 / 877,967), rotating 44/45. TransitiveReduce (which already had the cursor) 40-52 rotations.
  This is SCHED-B3's starvation finding at a much larger scale, and it is why the item exists.
- **BVE's cursor is deliberately OFF and its counter shows it** (`rotated 0`): its schedule is
  occ⁺·occ⁻-ordered, not key-ordered, so a key cursor would resume at an arbitrary place, and it already has
  the repetition device the others lack (`bve_retry`). Per-item marks for the rest are Stage F.

**UNPRICED, and the direction is not one-signed:** conflicts `b22_1` 530,802 → 494,805 (−6.8 %),
`FmlaEquivChain` 384,785 → 375,614 (−2.4 %), `sted5` identical (its Probe/Backbone produce nothing either
way, so a reordered walk finds the same nothing), `9vliw` 332,391 → 348,630 (+4.9 %). Proofs shrink on three
of four. One instance moving 6.8 % is not a verdict ([[chaotic-instance-veto]]).

**▶ READY-TO-RUN A/B (`SCHED-E1`) — PRE-FLIGHT FIRST.** Read `c cand-order` on two Tier B instances before
spending a window: if `rotated` is 0 for Probe and Backbone the arm is vacuous there and the window should be
abandoned. The knobs go IN THE COMMAND. Tier B and Tier C are concurrent arrays, so **two pin dirs from the
same commit** (CLAUDE.md build-dir rule) and check the shas match.
```
tools/bench/buildlock.py --gc
cmake -S . -B build/pin-e1-b -DCMAKE_BUILD_TYPE=Release && cmake --build build/pin-e1-b -j8
cmake -S . -B build/pin-e1-c -DCMAKE_BUILD_TYPE=Release && cmake --build build/pin-e1-c -j8
sha256sum build/pin-e1-{b,c}/tactix        # must match
python3 tools/bench/run.py --tier B --slurm --baseline kissat --baseline cadical \
  --solver build/pin-e1-b/tactix \
  --solver 'env TACTIX_CURSOR_PROBE=0 TACTIX_CURSOR_BACKBONE=0 build/pin-e1-b/tactix'
python3 tools/bench/run.py --tier C --slurm --baseline kissat --baseline cadical \
  --solver build/pin-e1-c/tactix \
  --solver 'env TACTIX_CURSOR_PROBE=0 TACTIX_CURSOR_BACKBONE=0 build/pin-e1-c/tactix'
```
Pre-registered counter: `c cand-order` `rotated` > 0 for Probe AND Backbone on the ON arm and exactly 0 on the
OFF arm. Ship criterion: **#solved not worse on either tier and PAR-2 no worse than the 6.72 noise floor on
both**, with the may-not-gate set excluded from deciding alone. A per-rule follow-up (cursor on Probe only,
Backbone only) is worth running ONLY if the pair moves — three arms per tier otherwise buys noise.

### 2026-08-05 — SCHED-C8: BVE learns ITE + XOR gates, closing `SP-BVE-GATE-1` (UNPRICED, A/B specced)

**What was and was not there before.** E1c did ship gate extraction with a `--bve-gates` knob, so the
row was never "no gates" — it was **exactly two of four types**: `BVE_GATE_EQUIV` (two binaries) and
`BVE_GATE_AND_OR` (base clause + input binaries). **ITE and XOR did not exist**, and both references
ship them on (kissat `ifthenelse`=1; cadical `elimites`=1, `elimxors`=1/`elimxorlim`=5). Now all four.
kitten `definitions` remains **phase 4** (Track R · R5) and is not part of this closure.

**V6 Congruence's extractor was NOT reused, and the reason is structural, not effort.** `congruence.cpp`
does have `congruence_extract_xor` / `congruence_extract_ite`, but it builds a FORMULA-WIDE `ClauseIndex`
by one arena scan per apply, keyed on whole sorted clauses, holding proof ids and no arena refs. BVE
needs the opposite object: a **per-pivot** partition of the *already materialized* `pos`/`neg` `MClause`
lists (root-false literals stripped, delete-handles attached) into gate vs antecedent, recomputed after
every elimination mutates the neighbourhood. Reuse would mean rebuilding an O(arena) index per candidate
and then mapping ids back to MClause slots. What was reused is the *idea*: the parity-probe enumeration
and the ITE normalization argument. Recorded so a later pass does not "fix" the duplication.

**A/B SPEC — pre-registered, unrun (no SLURM submitted this session).**
- **Arms, knob IN THE COMMAND** (never the environment): `gates-all` = `--bve-gates=all` (shipped) vs
  `gates-noitexor` = `--bve-gates=equiv,andor` (the pre-C8 behaviour). Both arms from **one** commit.
- **Tiers:** same-window **Tier B + Tier C**, `--baseline kissat --baseline cadical`, node-pinned.
  **TWO pin dirs** — `build/pin-n4-c8-b/` and `build/pin-n4-c8-c/`, built from the same commit with
  matching binary shas (CLAUDE.md build-dir rule; the second `--acquire` refuses a live path).
- **PRE-FLIGHT, checked before any score is read:** on `gates-all`, `c bve-gates` must show **ite > 0 and
  xor > 0** on at least one scored instance; on `gates-noitexor` it must show **exactly `0 ite, 0 xor`**
  and a strictly smaller `resolvent pairs saved`. Already demonstrated at HEAD: `9vliw_bp_mc` under the
  shipped DefaultPolicy reports **154 equiv, 5853 andor, 306 ite, 16 xor; 6,101,504 resolvent pairs
  saved**. An arm whose counters did not move was not tested.
- **Ship criterion:** keep ON (the default) unless `gates-all` is worse on **#solved** on either tier, or
  worse by more than the 6.72 noise floor on PAR-2 on BOTH tiers. Rationale for the asymmetry: this is a
  reference-parity mechanism that both references default ON, and it is an *acceptance* lever, so its
  effect is expected to be small and diffuse rather than headline. The may-not-gate set may not gate
  alone; a single-instance flip owes paired draws ([[chaotic-instance-veto]]).
- **Secondary readout to record either way:** `bve eliminated` per instance. The lever's claim is that
  the SAME numeric `bve_bound` now admits more variables — if `eliminated` does not move, the row is
  closed on parity grounds only and that must be said plainly.
- **Consequence for `SP-FACTOR-1`:** its removal path **R3** ("SP-BVE-GATE-1 landing changes what
  `bve_level_` means") is now live. R3 is a *reopen* trigger, not a result — reopening still needs R2's
  falsifier first (show `bve_level_` non-zero at Factor's occasions).

**Not touched here:** `default_policy.cpp` / the inprocessing slot, the conflict-analysis path, and
`tools/docs/` (parallel agents own those).

### 2026-08-05 — Sweep governor A/B: ON costs a Tier C solve, and the cost is two instances

Window `20260805-205741-C-sweepon` (git `cf5eb36`), node017, `sweepon` (shipped) vs
`TACTIX_SLOT_DISABLE=Sweep`, both baselines. **sweepon 382.51 (28/30) vs sweepoff 333.51 (29/30)** —
+49.00 PAR-2 and one solve (`minxor128`, TO vs 992.5 s). T3: kissat subset 1.722 → 1.459×, cadical full
1.224 → **1.067×, inside the band**.

**The cost is concentrated, which is the actionable part.** `constraints_18_0.3_2` +291.1 s (42 rounds,
zero emissions — the survey predicted this one) and `minxor128` +207.5 s; ex-both the sign flips to
+7.62 favouring Sweep, and Sweep wins `em_11_3_4_cmp` −158.4 s and `sat-bench-trig-bhaskara` −112.0 s.
So: dose or target the governor, do not disable the rule. Fragile at k=2; `minxor128` owes paired draws.

**Reading this log.** Newest first. Entries older than the current session are compressed to **result +
pointer** — the full account is in the cited report, design doc or commit, which are never edited.

### 2026-08-05 — SCHED-D3: the slot gates, and the flip-vs-event trap that wedged elimination shut

One gate per clock, both "the interval is necessary but not sufficient":

- **ELIMINATE — new-material watermark.** Admit only if `k.stats.bve_marks` or `kernel_root_fixed(k)` has
  grown since the last phase's ENTRY (kissat `eliminate.c:33-38`
  `limits.eliminate.variables.{eliminate,subsume}`; cadical `elim.cpp:75-80` `last.elim.fixed` /
  `last.elim.marked`). Snapshot at ENTRY, not exit: material the phase itself produces re-admits the next
  one, so a budget-cut pipeline is never gated out of its own follow-up. That is weaker than kissat's
  completion-gated advance and equals cadical's restore-on-incomplete case (`elim.cpp:774-775`/`:909`).
- **PROBE — reduce coupling.** After the first phase, require a reduce since the last probing phase
  (cadical `probe.cpp:23`). *(kissat's mirror-image rule — skip when a reduce happened at THIS conflict
  count, `probe.c:21`/`eliminate.c:28` — is not ported: it defers a phase by one conflict and would only
  add a decline reason to account for.)*

**▶ THE FINDING, and it is a measured trap, not a design note.** Both references count the mark-bit FLIP
(kissat `variables_eliminate`, cadical `stats.mark.elim`) because their elimination round CLEARS every
scheduled variable's flag and so re-arms the flip. Our `bve_retry` bit is cleared **only by a BVE attempt**,
and BVE's during-search admission can decline every dose on a formula — after which no bit is ever cleared,
no flip ever happens, and a flip-counting watermark shuts the elimination clock **for the rest of the
solve**, taking SubsumeForward / Strengthen / Instantiate / BCE down with it (they share the clock and have
nothing to do with BVE's density gate). Measured on `sted5`: 6 elimination phases → **1**, 274 k conflicts →
**1.15 M**, `bve-marks 0` for the whole run. Counting mark EVENTS implements the same predicate against the
bit lifecycle we actually have. The transliteration was faithful and wrong — the lifecycle around the
counter is part of the mechanism.

**Reachability, measured at the source — whole of Tier A (200 instances), `c slot-clock blocked`:**

| gate | instances it fired on | episodes |
|---|---|---|
| ELIMINATE watermark | **10 / 200** | 11 |
| PROBE reduce coupling | **0 / 200** | 0 |

**`TACTIX_SLOT_STATS`'s self-check still balances: 0 `***UNACCOUNTED***` over 40 Tier A instances.** Neither
D2 nor D3 could break it by construction and that is worth saying: D2's decline is "the other clock's
entry is not offered here at all" (before `occasions++`) and D3's is clock-level (no entry is reached), so
the per-entry `occasions == admitted + declines` identity is untouched — no new decline REASON was added.

The elimination watermark is live but rare, which is the shape a safety valve should have. `sted5` and
`qwh.50` block 0 times on either clock, and on `sted5` D3 is behaviourally identical to D2 (12/6 phases,
same conflict count) — so **there may be nothing to price on Tier B/C**, and the A/B spec says to read the
`blocked` counters first and abandon the window if they are zero.

**The probe reduce coupling is PROVABLY inert under our reduce cadence, not merely unlucky.** Probe
interval ≈ 2500·n·log10(n+9), reduce interval 2000+300·r; matching cumulative conflicts gives
r ≈ 2.9·n·√log n, so the probe interval is ≈2.9·√log n times the reduce interval — always > 1, so two probe
phases can never fall inside one reduce interval. It is kept (one compare, the reference's own gate, and it
becomes live the moment the probing clock takes cadical's near-constant `probe.cpp:980` spacing, which is
precisely where cadical needs it) but it is labelled inert in the code and must not be cited as a result.

Fuzz on the D3 binary: DRAT 2000/0, native LRAT 2000/0, `--random-policy` × DRAT 2000/0; ctest 43/43;
regression corpus 14/14. *(The `--random-policy` leg exercises the RULE layer, which Stage D does not
touch: `random_policy.cpp` references neither `DefaultPolicy` nor the slot, and D3's only kernel change is
a counter increment. It is the standing N1 leg, not a test of these three items — the reachability
counters are.)*

**▶ READY-TO-RUN A/B (`SCHED-D3`) — RUN THE PRE-FLIGHT FIRST AND BE WILLING TO CANCEL.** Like D2 there is
no env arm; the gate-free build is the parent commit.
```
tools/bench/buildlock.py --gc
git worktree add build/src-d2 <D2 commit>      # two clocks, no gates
cmake -S build/src-d2 -B build/pin-n4-d3-off-b -DCMAKE_BUILD_TYPE=Release && cmake --build build/pin-n4-d3-off-b -j8
cmake -S build/src-d2 -B build/pin-n4-d3-off-c -DCMAKE_BUILD_TYPE=Release && cmake --build build/pin-n4-d3-off-c -j8
cmake -S . -B build/pin-n4-d3-on-b -DCMAKE_BUILD_TYPE=Release && cmake --build build/pin-n4-d3-on-b -j8
cmake -S . -B build/pin-n4-d3-on-c -DCMAKE_BUILD_TYPE=Release && cmake --build build/pin-n4-d3-on-c -j8
sha256sum build/pin-n4-d3-{off,on}-{b,c}/tactix

tools/bench/run.py --tier B --slurm --nodelist node017 --baseline kissat --baseline cadical \
  --solver 'build/pin-n4-d3-on-b/tactix' --name gated \
  --checked-arm 'ungated=build/pin-n4-d3-off-b/tactix'
tools/bench/run.py --tier C --slurm --nodelist node017 --baseline kissat --baseline cadical \
  --solver 'build/pin-n4-d3-on-c/tactix' --name gated \
  --checked-arm 'ungated=build/pin-n4-d3-off-c/tactix'
```
**Pre-flight (a cheap LOCAL sweep, no cluster time): run the ON binary under `TACTIX_SLOT_STATS=1` over
the tier's instances and sum `blocked`.** Tier A gives 11 episodes over 200 instances; if Tier B/C give
**0**, the two arms are byte-identical by construction and the window buys nothing —
**do not submit it** ([[ab-arm-identical-artefacts]]), record the zero and close the item as an
unpriceable safety valve. **Ships if** (and only if) the gate fires and `#solved` does not drop; a PAR-2
move on 10-in-200 firing frequency would be noise, not signal.

### 2026-08-05 — SCHED-D2: two inprocessing clocks, and the "5×" headline corrected to ~2.4×

`SlotGroup{Probe,Eliminate}` on every slot entry (read from the authored table BY NAME, so it is
independent of D1's order arm); two `SlotClock`s; the phase body extracted to
`run_slot_phase(k, group, stop)`; the search loop checks probing then elimination, the reference order
(kissat `search.c:214-217`, cadical `internal.cpp:322-325`). Both clocks are seeded at conflicts=0, so
preprocessing is a probing phase immediately followed by an elimination phase — kissat's
`probe_initially` → `eliminate`, cadical's `inprobe` → `elim`.

**Intervals are kissat's**: probe = `probeint`(100)·n·log10(n+9), eliminate = `eliminateint`(500)·
n·log10(n+9)², both through `scale_inprocessing_delta`, each keyed on ITS OWN phase count. cadical's probe
reschedule (`probe.cpp:980`, 25·`inprobeint`·log10(phases+9)) is essentially CONSTANT spacing — ~2385
conflicts at phase 0, ~2500 at phase 1 — and adopting it would have changed the schedule SHAPE at the same
time as the split. *(Its own comment at `probe.cpp:889-891` claims a sqrt schedule; the code is
logarithmic. Read the code.)*

**▶ CORRECTION TO `SP-SLOT-1`'s HEADLINE.** "Probing fires ~5× less often" is the ratio of the BASE
CONSTANTS at equal phase counts. The realized ratio is the cumulative one, and because each clock keys on
its own count it is ≈√(5·log) ≈ **2.4×**, widening only as √log — not 5× and not fast. The reference agrees
on two instances (`--statistics`; both counters INC'd once per phase in `kissat_probe`/`kissat_eliminate`,
declaration + INC sites read per [[reference-counter-scope]]):

| instance | kissat probings / eliminations | ours after D2 | ours before D2 (one clock) |
|---|---|---|---|
| `sted5` | 11 / 5 (2.2×) | 12 / 6 (2.0×) | 6 phases, everything |
| `qwh.50.1250` | 7 / 3 (2.3×) | 6 / 3 (2.0×) | — |

So the mechanism roughly DOUBLES probing frequency; it does not quintuple it, and we now sit just under
kissat's own ratio rather than a factor of two below it.

**Group mapping**, argued per row in `AUTHORED_SLOT_ORDER`'s comment rather than guessed: PROBE =
Congruence, Substitute, HyperTernaryRes, Probe, Backbone, Vivify, Sweep, TransitiveReduce, Factor (every
one a member of kissat's `probe()` or cadical's `inprobe()`); ELIMINATE = SubsumeForward, Strengthen, BVE,
Instantiate, BCE (kissat's `eliminate()` forward-subsume + BVE; cadical's `elim()` subsume → elim_round
→ instantiate at `elim.cpp:947` → `block()`). BCE is the one row no reference schedules by default —
cadical's placement inside `elim` is the only evidence there is, and it is recorded as such.

**WHERE THE COST ACTUALLY LANDS, and it is not where "probing fires twice as often" suggests.** Effort
budgets are per-entry and anchored on `search_ticks` since THAT ENTRY last ran, so twice the occasions
means half the reference window each — a probing rule's total share of search work is unchanged. What
doubles is the number of PHASES, and therefore the per-phase scaffolding: the root backtrack, the
`kernel_dense_connect` occurrence-index build (O(irredundant arena), and every probe-group dense consumer —
HyperTernaryRes, Sweep, Congruence — needs it), the disconnect, and the garbage collection. So D2 is a
GRANULARITY change plus a fixed per-phase overhead, not a dose increase; if it costs on Tier C, look at
the phase scaffolding first and at the probing per-mille second.

**UNPRICED, and the one item in this stage expected to move numbers.** Probing at 2× is a real shift in
when effort is spent and may cost before it pays. Fuzz on the D2 binary: DRAT 2000/0, native LRAT 2000/0,
`--random-policy` × DRAT 2000/0. Source counter `c slot-clock` (`TACTIX_SLOT_STATS=1`) prints
per-clock phase counts; equal counts mean the split did not take.
`test_scheduling::test_two_slot_clocks_diverge` fails if the two intervals ever coincide.

**▶ READY-TO-RUN A/B (`SCHED-D2`) — THE PRIORITY OF THE THREE.** There is deliberately NO env arm: the
one-clock build is the PARENT COMMIT, so the arms are two binaries, not one binary and a knob. Two pin dirs
per tier pair, all four built from their own commit, shas checked ([[build-path-ownership]]).
```
tools/bench/buildlock.py --gc
git worktree add build/src-d1 <D1 commit>      # one clock
cmake -S build/src-d1 -B build/pin-n4-d2-off-b -DCMAKE_BUILD_TYPE=Release && cmake --build build/pin-n4-d2-off-b -j8
cmake -S build/src-d1 -B build/pin-n4-d2-off-c -DCMAKE_BUILD_TYPE=Release && cmake --build build/pin-n4-d2-off-c -j8
cmake -S . -B build/pin-n4-d2-on-b -DCMAKE_BUILD_TYPE=Release && cmake --build build/pin-n4-d2-on-b -j8
cmake -S . -B build/pin-n4-d2-on-c -DCMAKE_BUILD_TYPE=Release && cmake --build build/pin-n4-d2-on-c -j8
sha256sum build/pin-n4-d2-{off,on}-{b,c}/tactix   # off-b == off-c, on-b == on-c

tools/bench/run.py --tier B --slurm --nodelist node017 \
  --baseline kissat --baseline cadical \
  --solver 'build/pin-n4-d2-on-b/tactix'  --name twoclocks \
  --checked-arm 'oneclock=build/pin-n4-d2-off-b/tactix'
tools/bench/run.py --tier C --slurm --nodelist node017 \
  --baseline kissat --baseline cadical \
  --solver 'build/pin-n4-d2-on-c/tactix'  --name twoclocks \
  --checked-arm 'oneclock=build/pin-n4-d2-off-c/tactix'
```
**Pre-flight:** on the ON arm `c slot-clock` must show `probe phases` ≈ 2× `eliminate phases` (`sted5`:
12 vs 6); on the OFF arm the line is absent entirely — that is the reachability proof, and it also means
the ON arm's probe count must be compared against the OFF arm's `c inprocess N phases` line, not against a
missing one. (`c inprocess` sums both clocks on the ON arm, by design.) Equal probe/eliminate counts on the
ON arm ⇒ vacuous.
**Ships if:** `#solved` does not drop on either tier AND Tier C PAR-2 improves by more than the 6.72 noise
floor, OR `#solved` gains on either tier. **Sized honestly:** doubling probing frequency moves a lot of
effort; a Tier C loss inside the floor is not a refutation of the mechanism but a dosing question — the
next lever is then the probing rules' per-mille (`TACTIX_SLOT_PERMILLE`), not reverting the split, because
one clock is a measured reference gap either way. **Read `#solved` before PAR-2 and read the fragility
line**; the may-not-gate set may not decide it alone.

### 2026-08-05 — SCHED-D1: the slot order is DATA, and four SP-SLOT-4 accidents are gone

`AUTHORED_SLOT_ORDER` in `default_policy.cpp`, reachable as `default_slot_order()`, replaces `strcmp` +
`move_to_back("BVE"/"Factor")`. Membership stays registry-derived; a rule the table does not name is
appended, and `test_scheduling::test_authored_slot_order_covers_registry` fails the build if one exists (so
the append is a safety net, never the design). **Order stays POLICY** — nothing in a guard or `instances()`
reads the table.

**Order shipped** (probing pipeline, then elimination pipeline): Congruence, Substitute, HyperTernaryRes,
Probe, Backbone, Vivify, Sweep, TransitiveReduce, Factor · SubsumeForward, Strengthen, BVE, Instantiate,
BCE. **Was** (ASCII strcmp + two move-to-backs): BCE, Backbone, Congruence, HyperTernaryRes, Instantiate,
Probe, Strengthen, Substitute, SubsumeForward, Sweep, TransitiveReduce, Vivify, BVE, Factor.
Evidence per position is in the table's comment; where the references disagree (kissat vivifies then
sweeps, cadical sweeps then vivifies) we take kissat's and say so.

**All four SP-SLOT-4 accidents close together:** ELS now precedes every BIG reader; Congruence opens the
pass adjacent to the Substitute that consumes it; Vivify precedes TransitiveReduce; BCE closes the pass
instead of opening it. The old "Factor last so the NEXT phase's BVE sees the factored formula" rationale is
retired — with the authored order, THIS pass's BVE does, which is what both references do.

**UNPRICED.** Fuzz on the D1 binary: DRAT 2000/0, native LRAT 2000/0, `--random-policy` × DRAT 2000/0.

**▶ READY-TO-RUN A/B (`SCHED-D1`).** Two pin dirs, same commit, matching binary shas
([[build-path-ownership]] — a Tier B array and a Tier C array may not share one).
```
tools/bench/buildlock.py --gc
cmake -S . -B build/pin-n4-d1-b -DCMAKE_BUILD_TYPE=Release && cmake --build build/pin-n4-d1-b -j8
cmake -S . -B build/pin-n4-d1-c -DCMAKE_BUILD_TYPE=Release && cmake --build build/pin-n4-d1-c -j8
sha256sum build/pin-n4-d1-{b,c}/tactix      # MUST match

tools/bench/run.py --tier B --slurm --nodelist node017 \
  --baseline kissat --baseline cadical \
  --solver      'build/pin-n4-d1-b/tactix' --name authored \
  --checked-arm 'legacy=env TACTIX_SLOT_ORDER=legacy build/pin-n4-d1-b/tactix'
tools/bench/run.py --tier C --slurm --nodelist node017 \
  --baseline kissat --baseline cadical \
  --solver      'build/pin-n4-d1-c/tactix' --name authored \
  --checked-arm 'legacy=env TACTIX_SLOT_ORDER=legacy build/pin-n4-d1-c/tactix'
```
**Pre-flight, before reading any PAR-2:** `TACTIX_SLOT_STATS=1 <arm> <instance>` must print
`c slot-order  authored Congruence Substitute …` on one arm and `c slot-order  legacy   BCE Backbone …` on
the other. Same string on both ⇒ the arm never took and the run is vacuous.
**Ships if:** authored is ≥ legacy on `#solved` on both tiers and no worse than the 6.72 noise floor on
Tier C PAR-2 — the bar is deliberately "does not cost", since the warrant is reference parity, not a
number. **Reverts if** it loses a Tier C solve that survives paired draws.

### 2026-08-05 — the merged tree's 38 "LRAT" failures are drat-trim's CONVERTER, not our emitter

Report `benchmarks/reports/2026-08-05-drat-trim-conversion-crosscheck.md`. Reopened against the frontend
N2 fix; **the fix is complete for what it covers and HEAD is not fuzz-red on native LRAT.**

**The leg was mislabeled.** Job `1584461`'s solver argv is `[tactix, --random-policy]` — no `--lrat` — so
it ran native **DRAT** (drat-trim: VERIFIED) plus the harness's `drat-trim -L` → `lrat-check` cross-check.
`fuzz.py` has two LRAT oracles with two messages, and the bundles carry the *conversion* one
(`lrat-check rejected the LRAT proof`, not `... the native LRAT (exit N)`).

**Decisive experiment.** On the failing CNF, **kissat's and cadical's own DRAT proofs are rejected by the
same pipeline with the byte-identical message** (`multiple literals unassigned in hint 161: 80 80`; file
clause 161 is `80 80 0`), while our native LRAT verifies under `lrat-check` AND `cake_lpr`. Three
solvers, one rejection — the proof is not the variable. Five-clause auditable reproducer in the report.

**Mechanism, in the checkers' source.** (1) drat-trim's parser sorts and DELETES duplicate literals as it
reads the CNF (`drat-trim.c parse()`), so it holds `80 80` as a unit and passes; `-L` cites input clauses
BY ID; lrat-check re-reads the undeduped file and cannot propagate that id. Same *shape* as the frontend
breach, with drat-trim's converter in the producer role. (2) An input empty clause makes `-L` emit a
**zero-byte** proof → `Verification incomplete`. Both solver-independent. Tautologies, duplicate clauses
and plain UNSAT are fine — measured, so the predicate stays narrow.

**Fixed in the harness, not the solver.** `conversion_crosscheck_unsound()` returns a reason for exactly
those two classes and `run_case` skips the cross-check **visibly**: new counted `CaseOutcome.skips`,
surfaced as `oracles skipped: {'lrat-convert': N}` per leg and in `check-one`. The **native-LRAT oracle is
deliberately not gated** — it reads ids our own emitter wrote and is what covers the frontend restatement.
`selftest.sh` pins the skip, the fact that the underlying disagreement is real (so the skip cannot outlive
its reason), and that the native oracle still fires; its own `expect_clean kissat` arm had been red on
this since `lrat_verified()` landed.

**No corpus entry, deliberately.** The contract is a case that FAILS on the current binary; here the
binary is correct and the rejected artifact is produced by `drat-trim -L`. The invariant worth protecting
is the oracle's, so it lives in `selftest.sh`.

**Gates.** Both bundles stop reproducing on `min.cnf` and `case.cnf` at the recorded seed, with the native
oracle passing and the path live (`c input 10` / `c input 6` restated — not vacuous); `cake_lpr`
`s VERIFIED UNSAT` on all four. `selftest.sh` green (its kissat/cadical arms included). ctest 43/43,
corpus 14/14. Fuzz **at two seeds** (`20260709`, the one that produced the report, and `20260805`),
6000 cases each, **six legs, all 0 failures** — jobs `1584462`–`1584467`; on the decisive leg the
restatement path is reached by 1321 of 1916 UNSAT answers. **Two seeds is now the expectation for a
proof-path change** — the frontend branch was validated at one, which is how a whole case population
went unexamined.

### 2026-08-05 — SCHED-C5 PRICED: neutral on both tiers, ships ON on parity; and T3 regressed hard

**Namespace key (short prefixes have collided three times here; prefix a new id space distinctly at
birth).** `R<n>` has four meanings: **Track R · R<n>** = the reference rule inventory · **RV-R<n>** = the
2026-07-29 post-Track-R review · **R-a…R-d** = the variable-retirement design
(`docs/design/variable-retirement-design-v2.md`) · **`R`** in `F/R/τ/ρ` = the calculus's redundant-clause
set. Also: `SCHED-<stage><n>` = Track SCHED · `SP-…` = a scheduling-parity ledger row
(`implementation-notes.md` §13) · `D-…` = an engineering-deviation ledger row · `TB<n>` = Track T ·
`S-a…S-f` = Track R · R5's Sweep increments · `RL1`–`RL4` = the §5.6 RL stages · `L<n>` = a standing
lesson and nothing else (never cite one by number).

### 2026-08-05 late — the merge fuzz: DRAT green, **native LRAT RED (38/6000)**

Jobs `1584460` (native DRAT) / `1584461` (native LRAT), 6000 `--random-policy` cases each against
`build/pin-wrapfuzz`, the first fuzz of the merged combination of this session's five branches.
**DRAT 6000/0. LRAT 38 failures, all `lrat-check rejected the LRAT proof`** — the full lead, including the
`p cnf 1 1` / `0` minimal case, is in the resume anchor, which is where the next session should start.

**A side result worth banking: the three `--random-policy` × native-DRAT rejections recorded earlier today
(indices 888 / 2785 / 3296, seed base 20260709) appear CLOSED.** This DRAT leg runs the same harness from
the same seed base and covers those indices with 0 failures. That matches the localization — the failing
line on 888 was the second half of an equivalence pair and the stats pointed at **Sweep's equivalence
emission**, which is exactly what the S-e parity fix (`8796b26`) repaired. Confirm before deleting the
note; nothing was promoted to `tools/fuzz/corpus/regressions/`.

### 2026-08-05 — ▶▶ N2 (1/2) CLOSED: Sweep's general path wrote every lemma NEGATED

`8796b26`; reports `benchmarks/reports/2026-08-05-sweep-proof-rejection.md`, `…-sweep-n2-blast-radius.md`,
`…-sweep-se-polarity-fix.md`, `…-sweep-n2-postfix-survey.md`. **S-e decoded sub-solver literals with the
parity INVERTED — both sub→kernel decode sites tested `(sl & 1u) == 0`** — so each general-path lemma was
emitted as the negation of what was derived. The verdict and `F` were always correct; only the proof lines
were wrong, which is why nothing but a checker could see it. **Fixed by one shared `sweep_kernel_lit`**, so
the two sites can no longer disagree. `asconhashv12` now verifies on drat-trim AND cake_lpr; regression
corpus `012`; `sweep_proofs` extended to run over the REAL emitted proof rather than a reconstruction;
`--sweep-stress-policy` added. Blast radius surveyed and post-fix re-surveyed: 1 instance of 4 affected,
**no banked result invalidated**, emitting set unmoved by C5.

### 2026-08-05 — ▶▶ N2 (2/2) CLOSED: the frontend's input normalization, enumerated end to end

`16b6e75`; report `benchmarks/reports/2026-08-05-input-clause-normalization-n2.md`.
**The rule:** the checker reads the ORIGINAL FILE and numbers clauses 1..N by position (cake_lpr refuses a
file whose count disagrees with the header), so input clause `i` must carry proof id `i`, and checkers hold
a clause as a literal LIST, not a set — `-1 -1` is not unit for them. `ingest_clause` deduped literals, so
such a clause's id named a different clause for the checker than for us.
**Verdicts over all 10 normalizations:** repeated literal → BREACH, fixed. Empty input clause /
contradicting input units → BREACH (latent), fixed — emitting the refutation mid-input BURNED an id the
checker had already given to a later input clause. Tautology, duplicate unit, duplicate binary,
post-refutation clauses, literal order, a lying header → not breaches, **for one reason worth keeping: an
id is citable only if it was RECORDED, and `ingest_clause` records in exactly three places** (`unit_ids`,
`binary_ids`, a clause header) — everything it drops writes to none. A missing final `0` → a real
divergence, NOT fixed (moved to Phase 5 as input validation).
**Fix:** the deduped clause becomes its own one-antecedent RUP lemma and the stored id re-points to it.
Lemma ids must exceed every input id and `N` is unknown until the input ends, so `finish_input`
(`frontend/parse.hpp`) DEFERS both the restatements and the parse-time refutation to end-of-input. CaDiCaL
takes the same lemma shape but reserves the input range from the declared count; we cannot, since our
parser treats that count as advisory. **⚠ That deferral is the prime suspect for tonight's red LRAT leg.**
**Why enumerating beat symptom-chasing, measured:** a duplicate is only VISIBLE to the checker when the
duplicated literal is the one left unassigned — 440 UNSAT cases reached the path and only 25 of them
rejected pre-fix (5.7 %), so patching the failing cases would have left the rest live. Corpus `013`+`014`
both fail on a pre-fix build. `micro_proofs`'s own `grep -qi 'verif'` was tightened at the same time — the
identical landmine to the fuzz oracle below, masked there by an exit-code check.

### 2026-08-05 — ▶▶ THE FUZZ ORACLE ITSELF ACCEPTED `NOT VERIFIED`

`fuzz.py`'s native-LRAT oracle tested `"VERIF" not in output.upper()`, which matches `c NOT VERIFIED` too,
so **every LRAT rejection in the project's history was recorded as a pass** and only crashes ever failed an
LRAT leg. Fixed. Consequence, stated once and carried in the anchor: **any pre-2026-08-05 "native-LRAT
green" claim established nothing** and must be re-earned rather than re-read. Every such figure elsewhere
in this file is left in place as a record of what was run, not as evidence.

### 2026-08-05 — SCHED-C6: eager subsumption, and WHERE it runs is the whole result

`f1bc2c6`, ledger row `SP-EAGERSUB-1` closed. Layering, mechanism, the 3.7× placement swing
(8,297 deletions / 28,000 reason-skips at the tail of `analyze_apply` vs 30,613 / 0 in `backjump_apply_to`)
and the reason-guard deviation that no fuzz or tier leg can cover are in the checklist item above; docs in
`implementation-notes.md` §15 + M3's calculus entry. **UNPRICED**; the A/B spec (arms, pre-flight counter,
ship criterion) is in the anchor's owed-measurements list.
**Fuzz, rebased onto the frontend fix: five legs green** — and the PRE-rebase run is the attribution:
against the old parent the two native-LRAT legs were red (18/501 and 11/485 UNSAT) and **all 29 were the
frontend `ingest_clause` breach**, shown two ways (29/29 inputs carry a repeated literal; 29/29 reproduce
on the pre-C6 binary). **Direct N2 evidence where the mechanism is heavily active, which fuzz cannot
supply** (fuzz cases are ≤100 vars with few conflicts): `php-8` with **11,385 eager deletions** verifies on
drat-trim, cake_lpr AND lrat-check. The OFF arm is a verified control: `TACTIX_EAGER_SUBSUME=0` at the new
HEAD is **byte-identical to the pre-change binary** on four instances.

### 2026-08-05 — C1/C2's deferred `binary_is_redundant` condition: the trigger FIRED, and there is NO defect

`9e5a690`. `subsume_forward.hpp` carried a forward-looking condition naming "a V3b substitute-rewrite
re-points edges at an SCC representative" as the event that would make a `binary_is_redundant` guard
MANDATORY at three sites. **The named event has fired** — V3's rewrite ships and is scheduled, and X1/E1/E2
have all deleted binaries non-equivalence-preservingly since the note was written.
**▶ NO GUARD IS NEEDED, BECAUSE THE NOTE'S CRITERION WAS THE WRONG ONE** (do not let a future edit turn
this back into "deferred"). It demanded that no rule later delete `D` in a way that enlarges the model set.
What soundness actually needs is only that `D ∈ F` **at the moment `C` is deleted** — the argument closes
there, and every later `F`-rewrite carries its own obligation, which composes. **The single fatal fate is
`D` being FORGOTTEN** — dropped from `R` with neither entailment nor witness — **and no binary can be**:
`kernel_add_binary_clause` has no `redundant` parameter, the watch word has no spare bit, and the Forget
paths act on arena `Ref`s only. All five binary deleters are enumerated and classified in
`implementation-notes.md` §14; `binary_is_redundant` would be a constant `false`.
**Landed:** the corrected argument + the ONE remaining trigger (a redundant-binary lifecycle) in the four
source files, `calculus.md` C1/C2 soundness lines, `implementation-notes.md` §14.
`docs/design/v4-transitive-reduce-design.md` §5 is frozen and superseded on this point.
**Test with a negative control:** `test_binary_subsumer_then_substitute` runs the composition end to end,
and suppressing V3's `kernel_push_witness` pair makes it FAIL — so it is load-bearing. Reachability counted
at the source: 3,826 binary-subsumer uses with 553 of those binaries later deleted, in 15.8 % of processes
(DRAT leg); 269 later deleted, **65 of them by V3 Substitute**, in 8.4 % (LRAT leg). The uniform policy
saturates this pattern, so no bespoke stress policy was written.

### 2026-08-05 — SCHED-C5 landed and PRICED: neutral both tiers, ships ON on parity; and T3 regressed

`8b44dc7` / `772c5cd`; windows `20260805-115027-B-c5` / `20260805-115039-C-c5` (git `0418770`), node017,
arms = shipped default vs `TACTIX_FACTOR_DEMOTE=0` with the knob in the command. Pre-flight passed all
three pre-registered conditions (`c factor-dem` non-zero vs exactly zero; arms not byte-identical; `rbsat`
5.5× reproduces). **Verdict NEUTRAL** — same `#solved` on both tiers; Tier B −0.62 toward OFF is FRAGILE at
k=2 and Tier C +10.25 toward ON is UNDECIDED (`frb45-21-2` is 102 % of it). The reversed-port finding and
the reference evidence (kissat has no option for it; cadical's `factorunbump` defaults 1, "[1: as in
kissat]") are in the checklist item. Demote is proof-checked on a real instance where it fires:
`clique_n2_k10`, 99 fresh vars demoted, 792 RAT lemmas in core, VERIFIED by drat-trim and lrat-check.
**The window's real news is not C5:** T3 regressed on BOTH halves with kissat flat — see the gate row.

### 2026-08-06 — SCHED-G1: Congruence is priced, budgeted, and the unit gap is NOT what the note said

Branch `sched-g1-congruence`. Design frozen at `docs/design/2026-08-05-g1-congruence-governor-design.md`;
ledger row `SP-CONG-BUDGET-1` (§13.3, the first accepted deviation that runs *stricter* than the
reference). Detail in the checklist item — three things worth keeping here:

**(1) Two recorded numbers were wrong and both are corrected in place.** `SCHED-A2b`'s "~44 s per
Congruence application on `T105.2.0`" is stale by ~5×: at HEAD it is **8.25 s**. The rule is still the
largest single cost in the codebase — **140,303 ms, 47.1 % of a 300 s solve, two thirds of the whole
inprocessing slot** — so the item stands, but on the right number. And the profile overturns half of
the earlier cost attribution: extraction is indeed the majority (57 %), but the missing 33 % is the
**gate-vector teardown (16.6 %)** and the closure's **table/`occ` half (19.5 %)**, while **proof
emission is 0.006 %**. Method note: the profile came from stage timers *inside* the rule
(`TACTIX_CONG_TIME`, kept, default-off), not from `perf` — the teardown is invisible to a
symbol-attributed profile because it is `operator delete` charged to the allocator.

**(2) kissat gives congruence NO effort budget at all** — no `SET_EFFORT_LIMIT` in `congruence.c`, no
`congruenceeffort` in `options.h`. So this budget is OURS, like BCE's 20 ‰, and it is justified by a
COST GAP rather than by parity: our round extracts ~20× the reference's gate volume. Recorded that way
in the ledger, with the removal path being the cost, not the governor.

**(3) The 2,231-vs-14 unit gap is three mechanisms, and the one on record is not the load-bearing one.**
`val_merge` — accepted merges with a root-assigned participant, i.e. exactly where kissat's
`merge_literals` learns a unit — is **0 on every instance measured**, so a value-driven closure has
nothing to fire on today. What kissat has and we do not is **`find_units`, a binary-watch pre-pass**
that is not gate reasoning at all, plus the cascade it seeds, plus an accounting overlap with V3
Substitute. Split out as **`SCHED-G4`**, ordered so the free part (the overlap) is settled first.

**Reachability, counted at the source** (`c congruence … budget stops`, once per cut round, in the
`BudgetScope` destructor so an extraction-only cut still counts): `T105` 7 stops of 8 rounds —
**47.1 % → 4.4 % of the solve, 72 % of merges retained** · `9vliw` 13/15 · `b22_1` 10/30 · `sted5` 5/9
· **`hwmcc17miters` 0 of 1**, the exempt opening round, unchanged in time and in its 710,722 merges.
Both readings the counter exists for are therefore available, and the honest cost is on the record:
`9vliw` loses merges (75 → 30) on an instance where the whole rule was 188 ms. A/B specced, unrun.

**N2 — six fuzz legs, 10,000 cases each, 0 failures** (`1585188` DRAT · `1585189` native LRAT ·
`1585190` `--random-policy`×DRAT · `1585193` congruence-stress×native-LRAT · `1585191`/`1585192`
congruence-stress×**budget**×LRAT/DRAT). **And the case fuzz cannot reach — a round CUT AFTER it had
already emitted merges — is checked on real instances instead** (jobs `1585210`-`1585212`): `b22_1`
10 stops / 1,276 merges and `9vliw` 13 stops / 30 merges both `s VERIFIED` under drat-trim **and**
`s VERIFIED UNSAT` under cake_lpr, with the unbudgeted `hwmcc17miters` (711 units) as the control;
`congruence_unjustified` **0** everywhere. `test_congruence::test_budget_stops_round` pins the
mechanism, including that `work_budget == 0` stays unbounded and that a cut is not a terminate.

**A coverage finding that is not ours but is now measured:** on 120 generated fuzz cases DefaultPolicy
runs **zero** Congruence rounds — the cases are solved before the inprocessing slot is ever reached —
so the DRAT/LRAT/`--random-policy` legs prove nothing about this rule, which is what the congruence
stress policy is for. The budget is therefore driven from THERE: `TACTIX_CONG_STRESS_BUDGET=N` hands
each apply a fresh **log-uniform** budget in `[1,N]` (a flat draw over `[1,64]` cut 220 rounds and
produced **0 gates** — it never reached the closure, which is the N2-critical stop), paired with
`TACTIX_STOP_FUZZ=<huge>` because the rule's stop checks are strided 1-in-256 unless `stop_fuzz` is set
and a tiny case never reaches 256 items.

### 2026-08-05 — SCHED-A2b: it is now IMPOSSIBLE to schedule an inprocessing rule without a governor

`tools/docs/check-effort-specs.py`, ctest `effort_specs`. The gap it closes is STRUCTURAL, not a mistake
someone made: the slot is built from the registry (`tier == RuleTier::Inprocess`) while the effort ledger is
a separate table keyed by NAME, so a registry line alone schedules a rule and a missing `EFFORT_SPECS` row
silently yields `meter=nullptr, permille=0` — one unbudgeted canonical round on every phase, forever. Detail
in the checklist item.

### 2026-08-05 — S-f MEASURED (and now VOID): Sweep is a WASH ex-fragile, and costs a Tier C solve

Windows `20260805-002315-B-sf2` / `20260805-002318-C-sf2`, node017; default (Sweep ON at 100‰) vs
`TACTIX_SLOT_DISABLE=Sweep`. Tier B **92.32 (25/28)** vs 103.19 (24/28) — −10.86 and +1 solve, but FRAGILE
at k=2 on `mod4block`, an established lottery. Tier C 406.25 (**28**/30) vs **339.77 (29/30)** — +66.48 and
**−1 solve**, fragile at k=2. **Honest read: once the fragile instances come out Sweep is a WASH on both
tiers (−0.08 B, +3.90 C)**, neither the win the Tier B headline suggests nor the loss the Tier C headline
suggests — except that a Tier C solve flip is worth 2400 PAR-2, so `#solved` is the number to read.
**Decision (per the standing baseline that every implemented proof rule ships ON): keep Sweep scheduled at
kissat's own 100‰** and treat the lost solve as the open question rather than a reason to disable a rule.
**▶ THIS WINDOW IS NO LONGER USABLE** — it predates the N2 fix, so it scored a correct UNSAT as unsolved on
a rejected proof. Re-running it is the anchor's next performance step.

### 2026-08-05 — ▶▶ S-f: Sweep was ALREADY SCHEDULED by S-d, and unbudgeted

**Recorded because it shipped.** The S-d entry claimed Sweep was "registered but NOT scheduled, so default
runs are unchanged". False: **the S4 arrangement enables EVERY `Inprocess`-tier rule** — `on` starts true
and only the named ablation knobs turn it off — so **registering the rule IS scheduling it**. With no
`EffortSpec` row it then got no meter and no budget on every inprocessing phase: `9vliw` solves in ~14 s
without Sweep and **did not finish in 300 s** at that HEAD. Nothing caught it because
`check-config-wiring.py` tests the mirror image, the unit tests call `sweep_apply` directly, and fuzz was
green (it is a performance regression). **Fixed with a budget, not an off-switch:**
`EffortSpec{"Sweep", 100, meter_sweep, batch, budget_aware}` — 100‰ is kissat's own `sweepeffort` — and
`sweep_apply` now reads `Kernel::work_budget` into `round_ticks`, which had defaulted to 0 = unbounded.
After: `9vliw` **12.26 s budgeted vs 12.16 s sweep-off**. The lock-out is SCHED-A2b.

### 2026-08-05 — FULL STAGE B (B1–B5) RE-PRICED: neutral-to-positive; both deciders are lotteries

Windows `20260804-232942-B-stagebfull` / `…-C-stagebfull`, node017, pre-Stage-B vs all five. Headlines
+3.99 B / +50.81 C, each **fragile at k=1** — flagged `▶ UNDECIDED` by the new fragility line *before*
either was read as a verdict. `j3037` is an established lottery, so **−6.32 is the admissible Tier C read
and that is inside the 6.72 floor**; `sted5` got draws (**K′=10, Stage B faster in 5 of 10, median wall
ratio 0.96** over a range of 0.01–14.27, against the instance's own spread of 14.3×/101×) and is likewise a
lottery. **Verdict: Stage B lands** — Tier B −13.23 ex-lottery, Tier C neutral.

### 2026-08-05 — V5 Sweep S-e: the general path

The sub-solver's own refutation is replayed into the main proof: every lemma in derivation order with
antecedents translated from sub-solver ids to main-formula proof ids, the final lemma being the target
binary. This rests on `env ⊆ F` holding LITERALLY (S-c). Anything unmappable **declines**, because a wrong
line here is an unverifiable proof rather than a missed optimisation, and a pair is validated DRY in both
directions before either half is written. Two things worth recording because guessing cost time on both:
**the direction targets are READ OFF the derivation, not predicted from the assumptions** (`SweepEquiv`
normalises its pair to `{min,max}`, so predicting silently swaps the two directions whenever the seed is
the larger literal, and the general path then looks merely "unreachable" rather than mis-wired); and
**reachability is asserted, not hoped for** — the oracle finds only 8 non-RUP claims in 4380, so
`test_se_general_path_emits` uses the fixture that proves its own premise and asserts `sweep_general >= 1`.
It shipped with the inverted-parity breach above.

**▶ AND AN N2 DEFECT IN S-d, FOUND BY FUZZ.** `sweep_guard` is the pure precondition
`!inconsistent && level == 0` and deliberately does NOT demand a root FIXPOINT (that would make the guard
depend on scheduling state and cost N1) — but `sweep_apply` never completed the closure itself, which every
other inprocessing rule does (`inprocess_reach_root`). So Sweep could run with propagation pending, the RUP
replay ran off a non-fixpoint root, and literals it forced at level 0 never passed through
`propagate_service` — the only place `unit_ids[v]` is written. **Attributed rather than assumed:** a
`TACTIX_NO_SWEEP` kill switch gave 3/3 crash with Sweep, 0/3 without, refuting the tempting hypothesis that
it was pre-existing in the shared RUP-replay primitive. **DRAT could not see it** (it re-derives its own
units), so only the LRAT leg exposed it.

### 2026-08-04 — the day's results, compressed (full detail in the cited reports/commits)

- **▶▶ N2 DEFECT FIXED: LRAT unit ids were missed off the root level** (`52a4e8f`, shipped in TB4b the same
  day). `propagate_service` gated the LRAT root-unit sweep on the SOLVER's level while `emit_root_units`
  already applies the correct PER-LITERAL test, and a propagation-forced literal is stamped at its TRUE
  level — so a clause whose other literals are all root-false forces a literal at level 0 while the solver
  stands higher. Those kept `unit_ids[v] == 0`, and **0 is LRAT's line terminator**, so a later direct
  citation emitted a TRUNCATED antecedent list. **Certificate defect only — no verdict was ever wrong.**
  **The other half of the finding: `kernel_reason_walk` reads `unit_ids` only under `UNIT_REASON`, so
  reason-walk chains are structurally immune** — which is why the first reproduction came back green and
  why the defect could exist at all. Permanent guard: assert in `kernel_lrat_chain` that no antecedent id is
  0. Demonstrated with two binaries differing only in that conjunct (unfixed 36 assert-aborts, fixed 0 in
  10,000); corpus `011`.
- **▶▶ Broad Factor admission HELD, and the `j3037` veto RETIRED** — report
  `benchmarks/reports/2026-08-04-broad-factor-admission-retest.md`. The knob is a LONG-CLAUSE DETECTOR, not
  a fine separator (it flips Factor ON for 14 of 28 Tier B / 19 of 30 Tier C). **`j3037` was a lottery**
  (18 paired draws, K′=11; the 08-03 pattern reproduced in NONE, and broad is 26× faster on it in the new
  window). **The real loss is `ContextModel`, which REPLICATES 3/3 windows** — replication for the specific
  knob is exactly what `j3037` lacked. Ledger: Tier B 38.39 (28/28) vs 86.51 (25/28), **+3 solves incl.
  `fclqcolor`**, beating cadical; Tier C 414.70 (28/30) vs 309.05 (29/30); **T3 decides it** (kissat
  1.346 → 1.500×). Routed to **(b′) targeted admission** — the blanket knob is the wrong instrument, not the
  wrong idea. **Method worth reusing: the instances a knob provably cannot touch are a FREE in-window noise
  estimator** (measured 0.84–1.14 across nine of them), and pre-registration paid twice.
- **▶▶ THE FACTOR ORACLE: our admission gate is never the best of the three arms** — report
  `…-factor-oracle-ablation.md`. `facoff` 150.14 (22/28) B / **285.90 (29/30)** C · `shipped` 86.42 (25/28) /
  296.82 · `facon` **34.59 (28/28)** / 406.55 (28/30) · **ORACLE 32.25 / 254.55**. Headroom **+54.17 PAR-2
  and +3 solves on Tier B, +42.27 on Tier C**. Tier B wants Factor ON and Tier C wants it OFF; the gate is
  mis-calibrated in OPPOSITE directions on the two. **AND A DEFECT IN THE GATE ITSELF:** `k.binary_ids` is
  size-monotone and learned binaries enter the same map, so the numerator is "binaries EVER created" against
  a fixed denominator — **the gate LOOSENS monotonically over the solve** and a t=0 classification is only a
  LOWER BOUND on reachability.
- **▶▶ WHY our scheduling is cruder than Kissat's — diagnosed, and it is STRUCTURAL.**
  `docs/design/2026-08-04-scheduling-sophistication-diagnosis.md` (frozen). The crudeness is concentrated in
  the layer BETWEEN rules, and that layer has never been anyone's unit of work; coverage is opportunistic —
  every scheduling coupling we have sat adjacent to a rule someone was porting. Five causes, **four of them
  structural**: (A) the unit of work is a RULE · (B) the rule/policy seam has no wiring owner · (C) missing
  kernel state makes the faithful port not compile and the cheap substitute reads as a tunable · (D) **the
  eval loop SELECTS for admission gates** (per-instance PAR-2 at n≈30 with no held-out set is a
  classifier-fitting instrument; the record is 0-for-5) · (E) the deviation register has the wrong schema.
  **N1 IS NOT THE CAUSE — hypothesis REFUTED**: `restart.cpp`, a RULE, reads `k.h->` state directly and §5.3
  sanctions it, so the seam is a convention gap, not a prohibition. **The counterweight, kept deliberately:**
  six mechanisms ported FAITHFULLY, measured WORSE, and correctly kept simpler (sqrt reduce cadence, Walk's
  elimination anchor, mid-round C1/C2, Factor's coupled bound, `factorcandrounds`, minimize depth) — those
  are compliant and must not be "fixed".
- **SCHED-A3 enumeration pass** — 418 reference options; the 4 HIGH gaps and the 6 corrections are in the
  SCHED-A3 checklist item and `implementation-notes.md` §13.4–§13.6. Confirmed AT PARITY and recorded so they
  are never re-audited: chrono/chronolevels, EVSIDS decay, glue promotion on use, `otfs`, target phases,
  restart trail reuse, the bump-reasons rollback ladder.
- **SCHED-A4 · the ORACLE column** — see the checklist item. **SCHED-B3/B4/B5** — see Stage B.
- **▶▶ THE AGGREGATOR NOW REPORTS SINGLE-INSTANCE SENSITIVITY** (`d955602`): `--aggregate` prints the minimum
  number of instances whose removal flips the sign, names them, and prints the flipped value; `k=1` prints
  **UNDECIDED**. Back-run over 12 windows: **most Tier C verdicts are 1–2 instance decided**, including a
  landed change's confirming run. Structural: at a 1200 s timeout a solve flip is worth 2400 PAR-2, so any
  Tier C verdict that changes a solve count is single-instance decided **by construction**.
- **▶▶ `mod4block` IS A LOTTERY FOR BOTH SOLVERS.** kissat's own range there is **4.36 s → TIMEOUT (73×)**,
  and its default is 4.4× worse than its own chrono-off, needing 715,338 conflicts where our baseline needs
  418,796 — **so our 13.6 s was a lucky draw, never an engineering advantage**, and the standing claim that
  "no reference solver defends `mod4block`" is retracted. Against Stage B, **0 of 7 clean paired draws**
  reproduce its timeout (Stage B solves 5 of 7). **Method: the discriminator is cheap and belongs BEFORE any
  per-instance verdict gates anything** — perturb the BASELINE with knobs that cannot matter mechanically and
  see whether the instance moves. It took four jobs.
- **▶▶ I WAS WRONG ABOUT THE NODE — the "runner manufactured a confound" claim is REFUTED by its own
  re-run.** The runner DID correlate arm with node (solver-major task order), fixed by instance-major
  ordering + a `--nodelist` pin (`e670fb2`+`9c9344d`, kept as hygiene). But pinned, the aggregate reproduced
  to within **0.02 PAR-2** and the seven "cross-node winners" scored −129.64 s pinned vs −119.60 s unpinned.
  Seven of 28 spilled and happened to be instances where Stage B genuinely wins: **a post-hoc pattern on a
  hand-picked subset with no control, published as evidence.**
- **The BVE ramp ceiling's stated warrant was RETRACTED evidence** (`7e3d74a`, which also cleared the last 18
  lesson-number citations from `src/policies/**`) — see Track S · S1c. Also measured: `--reorder=0` costs
  kissat a non-detection (−0.67, inside the floor), not "nothing".
- **V5 Sweep S-c** (detector) — see Track R · R5.
- **▶▶ V6 COST: HALF THE CLOCK GONE, byte-identical proofs** (`ee9f3ab`) — `T105.2.0` 193.8 s → **97.8 s**
  of V6 time (solve 524 → 433 s), `T87.2.0` −53.4 %, `hwmcc` −48.9 %, `FmlaEquiv` −61.6 %, reproduced on an
  independent second sample. **297 MB of byte-identical DRAT and identical congruence counters is what
  licensed landing it without a tier gate.** **The biggest lever was not the sort — the clause index's mapped
  clause id was NEVER READ**, so an `unordered_map<SmallKey,uint64_t>` was a *set*; it is now a flat
  open-addressed `SmallKeySet`. Also: `sorted_keys` computed once instead of twice, exact `reserve()`, LRAT
  buffers hoisted to caller-owned scratch, `find` demoted from `std::function` to a template parameter.
  **Deliberately NOT done** (re-profile first — the old proportions no longer describe the remaining time):
  `CongGate`'s vectors → arena, ITE's node-based `ce` map, the closure's duplicate `occ` re-registration, the
  AND-extraction `tags` sort. **Process note: editing a probe script while jobs are executing it truncates
  the running instance** — bash reads scripts incrementally.
- **▶▶ COMPOSITION SHIPPED, unconditional** (`675a953`) — neutral (+1.41 B, −2.65 C, both inside the floor,
  no solve changes), shipped because it removes a hand-designed formula-shape separator at no measured cost
  and closes a real capability gap (applied factorings on `fclqcolor` go 78 → 164, where 78 was ~kissat's
  BINARY-restricted 80). **`fclqcolor`, the last Tier B timeout, falls only with composition AND broad
  admission** (164 factorings / 5.80 M conflicts → `s UNSATISFIABLE`, against 78 / 11.46 M → TO), at a
  284.7 s/300 s cliff, and broad is held on its own Tier C loss. **The `mod4block` trade belongs to BROAD, not
  composition** (14.0 s with composition vs 13.8 s without). **kissat's fourth governor (d), the
  `reduction > bound` acceptance coupling, is INERT here and was NOT shipped** — byte-identical arms on
  `mod4block` because `bve_level_` is 0 when Factor runs there; the same coupling was already measured dead
  on 2026-07-25 and is now measured dead again WITH composition on. **Do not re-try without a new mechanism.**
- **The 07-30 Factor reference study VERIFIED against live source.** Its core claim holds: **kissat has NO
  formula-shape admission of any kind**. Corrections: the "cadical's `factorthresh` is dead code" claim was
  **WRONG and retracted the same day** — it is read via TOKEN-PASTE, so grep finds only the declaration, and
  it IS cadical's one admission-shaped condition; and kissat's `INC (eliminations)` counts ENTERED, not
  completed, calls. **We are four sites from the `bve_level_` → `factor_bound` coupling and it still would
  not help** — refuted three ways, latest with composition ON.
- **▶▶ THE FRAGILE THREE ARE A CHRONO DEFECT (3 of 3); `veer_axi` is NOT.** `j3037`/`jgiraldezlevy`/`urq45`
  are 54 % of the Tier C gap to kissat (top 8 = 97 %; we WIN on 11 of 30). Chrono OFF moves ours 7.8× / 1.45×
  / 2.8× where kissat moves 1.26× / 2.4× / inert. **They are NOT intrinsically unmeasurable** — both
  baselines band at 2.6–3.1 % there, *lower* than kissat's own band on `goldcrest`. At comparable
  perturbation kissat ranges 1.23–2.43× where we range 8.4–23×. **`veer_axi` is NOT a chrono defect
  (hypothesis tested and refuted: chrono-off is 1.10× WORSE in conflicts, 0.90× in wall).** What remains
  unexplained there: **507× kissat's decisions and 262× its propagations for 6.3× the conflicts**, at 7.03
  props per decision — real propagating descents that almost never conflict. Ruled out: substituted-variable
  decisions (R-d fixed those, 84.03 % → 0.24 %), restart cadence, trail reuse, rule cost, chrono.
  **Measurement hygiene from the same probe: `veer_axi` moves 32 % on WALL CLOCK at a BIT-IDENTICAL
  trajectory** — pair the arms in one submission and read wall deltas against the deterministic counters.
  **Method guard: `--walkinitially=1` was VACUOUS** (byte-identical conflicts), caught only by capturing
  conflicts rather than wall time.
- **TB4b unit chrono routing SHIPPED** (`8118c68`) — a learned unit was forced to `target = 0`, discarding
  the whole descent; kissat routes it through the same chrono determination and steps back ONE level. On
  `veer_axi` **14,088 of 18,635 conflicts learn a unit** and the caller counted 14,108 chronological
  backtracks while `backjump` overrode 14,088 of them, so **the `c chronological` stat was reporting
  behaviour we did not perform**. Result `veer_axi` 195.5 → 31.9 s; gate −8.78 PAR-2 on the 27 non-lottery
  instances. Also the after-conflict snapshot order (step down one level, THEN snapshot). It introduced the
  LRAT root-unit defect above.
- **TB4b's confirming Tier C, and `frb45` joins the may-not-gate set** — −36.54 PAR-2 all-30, but **+27.56
  excluding the fragile three** (the entire win is `jgiraldez`, `urq45`, `j3037`, precisely what TB4b
  targeted) and **+9.45 excluding `frb45` too**, which probe 1582186 measures CHAOTIC (bit-identical under
  replication and under chrono 90/105/115, but 6.2× from `walk600` to `bumplim9`). Filed as **(c′)**: T3's
  kissat half moved 1.271× → 1.346×.
- **▶ Tier C NOISE FLOOR re-measured on an IDLE partition: 6.72 PAR-2, not 10.8** — three bit-identical arms
  (364.50 / 362.17 / 357.78), per-instance median 4.8 s. The 10.8 was contention-inflated (it overlapped
  another Tier C array by ~1 h 12 m) and remains the right bar only for a run that shared the partition.
- **▶ SUBSET RE-DERIVED with replicates: T3's kissat half is ROBUST to the list's composition** — the
  corrected 15-instance subset gives 1.269× against the shipped 24-instance list's 1.271×. The shipped list
  was UNDER-excluding (15 of 30 vs 6). **I had the fragility argument backwards:** the list header calls four
  exclusions "chaos-suspect" from OUR swings, but the criterion is about the BASELINES — kissat's replicate
  band on `j3037` is 39 s against a sweep delta of 203 s, so its exclusion is well-founded. **Do not carry a
  fragility measurement across solvers.** Not adopted (see Open items). **Harness defect found:** `--no-check`
  applies only to the solver-under-test arm while `--checked-arm` hardcodes check AND score.
- **`D-LRAT-FALLBACK` SHIPPED** (`3c3de3b`) — `urq45`: 591 s solve → drat-trim exhausted its budget →
  `--lrat` re-solve 618.8 s → **cake_lpr 119.0 s VERIFIED**, i.e. **37.7× drat-trim's 4,491.98 s**. **And the
  real number to worry about: that 4,491.98 s was 93.6 % of a 4,800 s budget** — the instance has been ~6 %
  from a false unsolved on every window we have quoted. Residuals in Open items.
- **`D-STRENGTHEN-BUDGET`: interruptible C1/C2 is MEASURED NEGATIVE and reverted** — see Open items. **Two
  method traps, both caught:** the motivating probe's 80.1 s "win" was cross-window and measured −0.8 % once
  both arms ran in one window ([[same-window-ab-hatch]]); and the fuzz leg built to make coverage
  non-vacuous **was itself vacuous** (on tiny instances the budget computes to 0 and the guard
  short-circuits).
- **Plan revisions.** **rev 80: N4, the strategy floor, is a non-negotiable** (user directive) — N3 measures
  the OUTCOME, N4 constrains the MECHANISM, and a rule scheduled more crudely than kissat schedules its
  counterpart is an unmet requirement, **not deferrable to §5.6**; §5 gains obligation 5, naming the two
  shapes that have shipped — **shape (i): we own the mechanism but never wire it** (the more dangerous,
  because the mechanism's presence hides the gap from a reader and from `grep`) and **shape (ii): an
  instance-level gate substituted for a per-application quality bound**. *(The plan itself labels these
  (a)/(b); renumbered here only to avoid colliding with the Factor-admission `(a)`/`(b′)` labels.)* **rev 81: the §5.6 RL stages are `RL1`–`RL4`** so `L<n>`
  means a lesson only — motivated by a mechanical checker that could not do its job, `check-progress.py`
  reporting false DANGLING errors on correct plan text. **rev 82: a STAGED LANDING may run at a temporary
  regression**, under five conditions ALL pre-registered before the regressing increment ships (a named
  recovery sequence, a mechanism argument, a recovery point and ceiling in PAR-2 and solves, revertibility, a
  kill criterion); it never covers N1/N2, a lost solve must be called out rather than absorbed, and the debt
  is discharged or reverted **by the phase gate**.
- **R8 moved out of Track R into Track SCHED** (`SCHED-C6`/`C7`).

### 2026-08-03 — the day's results, compressed (full detail in the cited reports/commits)

- **V6 Congruence: gate matching is AT PARITY; the gap is COST (7.6×) and the UNIT channel.** kissat's
  headline `congruent` BUNDLES a binary-equivalence scan that is our V3, so the two counters are not
  comparable at face value. The one real outcome gap is **`congruent_units` 2,231 vs our 14**.
  **Mechanism: kissat's closure is VALUE-DRIVEN** — it propagates each learned unit, re-simplifies gates
  against the new values, and **retires value-resolved gates**; our `canonicalize` contains **zero
  `k.value` reads** and never drops a resolved gate. `congruence_unjustified` is 0, so nothing is being
  deliberately skipped. Now **Track SCHED · SCHED-G1**.
- **V6 COST PROFILED** (`perf record` over a full T105 solve): `congruence_extract_gates` 6.13 %, the
  `SmallKey` machinery ~13.6 %, allocator ~13.6 % spread. **The closure's
  `unordered_map<vector<Lit>,uint32_t>` is 1.2 % — the structural hypothesis was REFUTED; do not swap it.**
  Attack EXTRACTION and the index sort. *(Half of this was then banked by the V6 cost work above.)*
- **plan rev 79:** baselines WRITE proofs but are no longer VERIFIED (baseline checking was 6.07 h of 9.08 h
  on one Tier C window); **Tier D frozen at 793**, and it overlaps Tiers B/C so it is not independent
  confirmation.
- **T3's INSTRUMENT restored** — see Open items.

### Earlier

The weekly archives `docs/log/2026-W28.md` … `2026-W31.md` carry the day-by-day entries for phases 0–2 and
the first half of phase 3; every landed item above cites its own commit, report or design doc.
