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

## ▶ RESUME HERE (2026-08-05) — phase 3 · **next = find out what Sweep's governor is costing T3**

**IN FLIGHT.** Fuzz on the post-Stage-D merged tree: `1584638` (random-policy × native DRAT) and
`1584639` (× native LRAT), 6000 cases each against `build/pin-staged-fuzz` — **Stage D is a large
scheduling change and this is the first fuzz of the merged combination; read these before trusting
HEAD.** Also `1584635` (`swfix`), a probe from the Sweep-governor investigation. Otherwise tree clean,
pushed, ctest **44/44**, corpus 14/14.

**▶ THE SWEEP GOVERNOR QUESTION IS ANSWERED — see the T3 block.** Sweep ON costs Tier C a solve and
49 PAR-2; with it off the CaDiCaL half of T3 would be MET. The cost is TWO instances and Sweep wins
elsewhere, so the lever is its effort share or a yield-based cutoff, **not** switching the rule off.
**Next step: dose it** — and run paired draws on `minxor128` before treating the solve flip as a verdict.

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
green on DRAT / native LRAT / `--random-policy`×DRAT). D1 authored order · D2 the two clocks · D3 the slot
gates. **D2 is the one expected to move numbers** (probing at ~2×, not the ~5× the ledger headline implies —
see its Log entry). The three A/B specs are in the Log entries and are the next Stage-D work; D3's spec
says to check its `blocked` counter first and abandon the window if it is zero.

**▶ THEN.** Stage E — order and resumption become policy objects, generalizing D1's table.

**▶ OWED MEASUREMENTS (each a job; none blocks the next step).**
- **ALL THREE STAGE-D ITEMS are specced and unrun** — arms, pre-flight counter and ship criterion in each
  Log entry. Pre-flight counters: `c slot-order` (D1), `c slot-clock` phase counts (D2), `c slot-clock`
  `blocked` (D3).
- **`SCHED-C6` A/B is specced and unrun** — arms, pre-flight counter and ship criterion are in its Log
  entry. Pre-flight: `c eagersub` non-zero on ON, exactly `0 subsumed of 0 tried … register 0` on
  `TACTIX_EAGER_SUBSUME=0`.
- **BVE ramp slack-8 ceiling** — Tier C/D re-sweep owed since `489fab1`; knob `TACTIX_BVE_RAMP_MAX`,
  reachability probe already written at `benchmarks/probes/scratch/bveramp-reach.*`.
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
  `docs/design/e1-bve-design.md`; `bve_stress_policy` reusable. Remaining: ITE/XOR gates + kitten (O11), phase 4
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
  and introduced the N2 defect fixed in `52a4e8f`. **Still unported: `jumpreasons`, cadical's
  `chronoreusetrail`.** `j3037`/`urq45` chrono sensitivity remains UNEXPLAINED.

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
  - [ ] **S-f · scheduling.** Sweep IS scheduled (at kissat's own 100‰ `sweepeffort`) and budgeted. **The remaining question is what that budget
    costs.** The one measurement so far (2026-08-05, below) says Sweep is a WASH ex-fragile-instances but **−1 Tier C solve** — and it predates
    the N2 fix, so it scored a correct UNSAT as unsolved on a rejected proof and **cannot be reused**. Re-running it is the anchor's next
    performance step. `TACTIX_SWEEP_PROBE` must not be set in any tier run (the probe is unbudgeted)
- [ ] **R6 · X3 `CCE`** (cadical `cover.cpp`) — delete + push-witness; doubles as the §5.3 **extensibility acceptance test** (pure plugin, no
  kernel edit)
- [ ] **R7 · X5 `Condition`** (cadical `condition.cpp`) — conditional autarkies
- ~~**R8**~~ — MOVED to Track SCHED (`SCHED-C6`/`C7`) 2026-08-04: it was scheduling work filed under the rule inventory. Track R is now purely
  new rule FAMILIES (R5–R7), which is what makes it parallelisable against Track SCHED.

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
HIGH rows, six are now closed (`SP-SLOT-2`/`SP-SLOT-3`/`SP-CAD-1` by Stage B, `SP-VIVIFY-1` by C1, `SP-FACTOR-2` by C5) or accepted
(`SP-FACTOR-1`, §13.3), and **eight remain open**: `SP-SLOT-1` (D2) · `SP-SLOT-4` (D1) · `SP-SLOT-5` (D3) · `SP-REPEAT-*` (F1–F5) · `SP-TIER-2`
(supersedes C1's scope, **unassigned**) · ~~`SP-EFFORT-CEIL-1`~~ **CLOSED by SCHED-E3** (ceiling ported, measured inert; its A3 headline was also
over-general — kissat has no upper clamp at all) · `SP-BVE-GATE-1` (**unassigned**) · `SP-JUMP-1` (**unassigned**). **That list IS the remaining
track work** — Stage D covers three of the eight, Stage E closes a fourth, and **two** unassigned A3 rows still have no stage.

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
  **The 4 HIGH gaps, which are the actionable list:** `SP-BVE-GATE-1` ITE/XOR gate extraction is absent, so BVE is charged the full P·N where both
  references pay P+N−2 (a per-application ACCEPTANCE lever, i.e. the T4 antidote) · `SP-TIER-2` the dynamic tiers have ONE consumer here and EIGHT
  in cadical via `likely_to_be_kept_clause`, which **supersedes SCHED-C1's scope** · ~~`SP-EFFORT-CEIL-1`~~ *(closed by SCHED-E3, and its claim
  "every reference budget has an UPPER clamp" was WRONG — kissat has no `maxeffort` option and our budget is kissat's macro)* ·
  `SP-JUMP-1` `jumpreasons`, default-ON, per-conflict, free.
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
  now read `k.tier2`; the test pins that moving the tier moves the candidate set. *(Scope superseded by `SP-TIER-2` — cadical has eight consumers.)*
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

- [ ] **SCHED-F1 · the substrate + its first consumer (C1/C2)** `[arch]` · [ ] **SCHED-F2 · C1/C2 completeness +
  resume, and the C1→BVE bound coupling it enables** `[arch]` · [ ] **SCHED-F3 · X1/X2/X4 repetition
  suppressors + the delay ladder for metered entries** `[arch]` · [ ] **SCHED-F4 · Factor per-literal marks**
  `[arch]` · [ ] **SCHED-F5 · Vivify yield package** `[arch]`

#### Stage G — the genuine revamps (each needs its own design pass)

- [ ] **SCHED-G1 · Congruence: budget + honest pricing, then the value-driven closure** `[revamp]` — the
  largest single gap in the project (`congruent_units` 2,231 vs our 14).
- [ ] **SCHED-G2 · C1 vs C2: is C1 doing anything?** `[wire]` — **the cheapest decisive check on the board.**
- [ ] **SCHED-G3 · Vivify cross-candidate trail reuse** `[revamp]` · [ ] **SCHED-G5 · binary-first watch
  layout** `[revamp]`. *(V5 Sweep is the largest block of reference scheduling we lack, but it is
  already tracked as **Track R · R5** — not repeated here as a second checkbox.)*

#### Stage H — recorded residuals (cheap, additive, no dependency)

- [ ] **SCHED-H1** termination polling inside C1/C2/Vivify/BCE/X2/X4 rounds · [ ] **SCHED-H2** per-solve reset of
  restart/reduce/rephase/mode/reorder/averages/walk_anchor · [ ] **SCHED-H3** Reduce `first_reducible` scan
  bound

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
- [ ] **(c′) T3's kissat half moved 1.271× → 1.346× across TB4b** — a real number attached to an unclosed
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
