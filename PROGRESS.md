# Progress

Living status. **Sync contract:** update this file in the same commit as (a) completing/starting any checklist item, (b) any DEVELOPMENT_PLAN.md
revision (via revise-plan). If this file and the plan disagree, the plan is authoritative — fix this file.

- **Plan revision tracked:** rev. 81 (2026-08-04) — rev 80 made **N4, the strategy floor**, a non-negotiable; rev 81 renamed the §5.6 RL stages `L1`–`L4` → `RL1`–`RL4`
- **Current phase:** 3 (inprocessing) — since 2026-07-14 (phase 2 closed at T2 the same day, report `benchmarks/reports/2026-07-14-t2-gate.md`)
- **Last updated:** 2026-08-04 late — **plan rev 80 (N4, the strategy floor, is now a non-negotiable)**. **Item (a) CLOSED: broad Factor admission re-tested at HEAD and HELD**, but the `j3037` veto that held it on 08-03 is **RETIRED as a measured lottery** (K′=11); the real loss is `ContextModel`, which **replicates 3/3 windows**. Broad is **+3 Tier B solves incl. `fclqcolor`** and **−1 Tier C solve**, and costs T3 on both halves — routed to **(b′) targeted admission**, now the top item. **T3 at HEAD: kissat subset 1.346× (was 1.271× pre-TB4b — see (c′)), cadical 1.069× subset / 0.990× full.** **TB4b's confirming Tier C collected** (−36.54 all-30, +9.45 excluding the chaotic set). **`frb45` newly measured chaotic.** **V6 congruence: outcome at parity, cost profiled to extraction + the index sort.** **Tier D built and frozen** (793), never run. **Also this session: an N2 defect FIXED** (LRAT unit ids missed off the root level, shipped in TB4b the same day — `52a4e8f`), the **N4 scheduling-parity ledger LANDED** (216 mechanisms, 132 gaps) with **Track SCHED** staged, and an **OPEN N1 BREACH recorded** in `Factor` (`SCHED-N1FIX`, top of the queue).

## ▶ RESUME HERE (2026-08-04 late) — phase 3; **next step = `SCHED-N1FIX` (an OPEN N1 BREACH), then (b′)**

Read this block, then the top Log entries. Full per-instance numbers: the tier tables below.

**NOTHING IN FLIGHT** — every array and fuzz leg from this session is complete and collected.
**▶ NO *RELEASE* BUILD MATCHES HEAD — build a fresh dir before the next measurement**, and never reuse one
([[build-path-ownership]]). `pin-lratfix` (Release) carries the propagate fix but predates the
`kernel_lrat_chain` assert; `pin-facablate` predates both. Only `pin-assert-fixed`/`-nofix` are current, and
they are **Debug** builds for the N2 demonstration — not usable for timing. Sweep the 19 stale `build/pin-*`
at wrap-up; all are reproducible from their commits.

**WHERE WE STAND** (window `20260804-1543`, git `6018366`, idle partition, one submission each):
**Tier B 86.51 (25/28)** vs kissat 9.78 and cadical 40.38 · **Tier C 309.05 (29/30)** vs kissat 230.74 and
cadical 312.02. **T3 both halves: kissat subset 1.346× · cadical subset 1.069× / full 0.990×.**
Note the kissat half moved **1.271× (08-03, pre-TB4b) → 1.346× (today, post-TB4b)** — see (c′).
**Fuzz green at HEAD:** 10,000/0 on each of DRAT · native LRAT · `--random-policy --lrat` (jobs 1582606–08), plus the paired assert demonstration behind the N2 fix (unfixed 36 aborts / fixed 0). ctest 38/38. Regression corpus now 11 entries.

**▶ ITEM (a) IS SETTLED: broad Factor admission is HELD, and the `j3037` veto is RETIRED.** Report
`benchmarks/reports/2026-08-04-broad-factor-admission-retest.md`. The three things to carry forward:
- **`j3037` was a LOTTERY** (K′=11 paired draws, probes 1581982+1582186): the 08-03 pattern did not
  reproduce in ANY of 18 pairs, and in the new tier window broad is **26× FASTER** there. That hold was
  decided by a coin flip. **Do not cite it again.**
- **The real loss is `ContextModel`** — 10.0 s → 1200 TO (120×), and it **replicates 3/3 windows for this
  knob** (22.2× / 54.9× / 120.2× across three bases). That is what distinguishes it from `j3037`.
- **Broad is +3 Tier B solves (incl. `fclqcolor`) and −1 Tier C solve, and costs T3 on BOTH halves**
  (kissat 1.346 → 1.500×). The gate decides it.

**▶▶ TOP OF THE QUEUE — `SCHED-N1FIX`, an OPEN N1 BREACH.** `Factor`'s guard reports applicable and
`factor_instances` offers an instance, but `factor_apply` NoOps on the density gate, so **no API caller —
LLM, RL, or RandomPolicy — can apply a legal factoring on a binary-sparse formula.** N1 is the top
non-negotiable and this outranks every performance item below. Fix = MOVE the predicate to DefaultPolicy
(BVE's identical gate already lives there correctly); DefaultPolicy behaviour is byte-identical, the API
surface is restored, and whether to KEEP the predicate stays a separate measured question. Track SCHED.

**THEN, in order** — re-ranked 2026-08-04 late, after the broad-admission re-test.

**(b′) TARGETED FACTOR ADMISSION — the biggest measured prize on the board, and now cleanly labelled.**
Broad admission's ledger is **+3 Tier B solves** (`or_randxor`, `mod4block`, **`fclqcolor` — the last Tier B
timeout**) against **−1 Tier C solve** (`ContextModel`) plus one more concentrated regression
(`mp1-Nb5T15` +801 s). **Those two instances ARE the entire Tier C cost**: quarantine them and broad is
**+8.89 PAR-2, ~1.3 noise floors**, while Tier B more than halves. So the blanket knob is the wrong
instrument, not the wrong idea. **What is new: per-instance outcomes on BOTH sides of the admission decision
over the 14/28 B and 19/30 C instances broad flips** — the strongest labelled signal §5.6's admission-dosing
exhibit has had. **Prior art says a hand-designed separator will fail:** four independent admission gates
were each measured against NO gate and all four lost badly (27–52 vs 9.17 PAR-2, `2026-07-28-all-rules-on.md`);
the best static BVE-skip separator made 4/13 misclassifications, i.e. chance
(`2026-07-20-bve-skip-feasibility.md`); and the 2026-07-30 reference study found no such axis in kissat or
cadical. So **do not build a fifth static gate**; this is phase-6 arm material and the honest near-term move is to bank the labelled data. Report §7.
**▶ AND THE QUESTION IS "WHEN TO RUN FACTOR", NOT "WHAT THRESHOLD" (probe 1582267, report §5b).** Both
regressions reproduce **bit-identically** under `TACTIX_FACTOR_DENSITY=0` — Factor-ON reached through a
route sharing no code with the scope branch (`mp1-Nb5T15` 11,249,172 conflicts under BOTH arms; both
`ContextModel` arms TO at 578 factorings). **The scope knob is innocent; Factor's PRESENCE is the cause**,
and it is **trajectory poison, not overhead** (36× / **83×** the conflicts). Tuning the threshold cannot
separate these cases because the threshold is not what reaches them. **WHEN to eliminate is a large
measured lever** — removing BVE admission entirely costs **+192.67 Tier C PAR-2 and three solves**
(window `20260730-111853`) — but no shipped admission FEATURE has earned it: the same density predicate on
Factor is never the best of three arms, and on Tier B(28) removing it is flat (105.21 vs 105.84).

**(b) `fclqcolor`** is NO LONGER blocked on "needs broad admission" — broad solves it (TO → 244.3 s). It is
blocked on *targeted* admission. Its own defect is unchanged and still worth pricing independently: applied
factorings SATURATE at 78 across `factor_size` 3/5/7/9 while offers rise 42 k → 51 k — the bottleneck is
ACCEPTANCE, and the **27×27 grid cap (S2a)** is the first thing to price.

**(c′) TB4b's own gate is NOT clean — reconsider it.** Its confirming Tier C (`20260804-141431`) is
**−36.54 PAR-2 over all 30 but +27.56 EXCLUDING the fragile three**, i.e. the entire win is the three
instances it targeted. `frb45` is ~2/3 of that cost and is **newly measured chaotic** (Log), softening it to
**+9.45**, ~1.4 noise floors — but T3's binding kissat half also moved **1.271× → 1.346×** across the same
ship. **Not a revert order**; an unclosed question with a real number attached.

**(c) V6's remaining cost** — now **Track SCHED · SCHED-G1**. −50 % banked at byte-identical proofs (`ee9f3ab`);
T87+T105 are still ~19 % of the Tier C kissat gap. **Re-profile first** — the old proportions no longer
describe the remaining ~98 s. **The outcome is AT PARITY; do not re-open it** (per kind on T105: AND 3,423
vs kissat 4,348, XOR 268 vs 531, ITE 0 vs 18 — and kissat's headline `congruent` BUNDLES a scan that is our
V3, L14). **Attack EXTRACTION and the index sort, NOT the closure table** (profiled job 1580096:
`congruence_extract_gates` 6.13 %, `SmallKey` machinery ~13.6 %, the closure map only 1.2 % — that
structural hypothesis was REFUTED). **Measured dead, do not redo:** extraction volume · arity caps · a
round/effort budget · the proof chain (`chain_fail = 0`) · stale-key eviction (`stale = 0`) · the XOR
collision rate · C0 sign absorption · the ITE extractor (0 merges vs kissat's 18).
Untouched leads: `CongGate` vectors → arena (blocked only by `tests/test_congruence.cpp:246`), the closure's
duplicate-accumulating `occ`, ITE's node-based `ce` map.

**(d) The rest of TB4b.** `jumpreasons` and cadical's `chronoreusetrail` are unported. Motive: trail reuse
COLLAPSED 85 % → 17 % on `veer_axi` once trails got deeper, and `chronoreusetrail` is by name that
interaction. **`j3037`/`urq45` chrono sensitivity is NOT explained** — they learn ~10 units in 3.4 M
conflicts, so unit routing is irrelevant there; a different divergence drives their 7.8×/2.8×.

**(e) V6's unit channel** (2,231 vs our 14; `find_units` is only 844 of it — the rest is real).
**(f)** First Tier D run — built and frozen (793), never executed, a phase-3-exit prerequisite.
**(g)** Track R · R5 S-c. **(h)** `--track` flag so T4's SC25-set PAR-2 is first-class.
**(i)** Revisit `D-LRAT-FALLBACK`'s doubled per-task wall limit (Tier C 1:42 → 3:22) — it reserves 2× the
wall for a rarely-firing path.

**BOUNDED, do not over-invest:** `D-VIVIFY-SCHED` is a yield fix (Vivify 0.7–1.9 % of clock, direction
unmeasured); `D-SUBST-UNBOUNDED` should buy ~0 (kissat WITH both bounds substitutes MORE than we do without);
V4 TransitiveReduce ceiling ≈−5.7 PAR-2; V6's ITE extractor is dead (0 merges, kissat 18); the mid-round
C1/C2 budget is measured-negative at the default (the DOSE axis is still live).

**STANDING DIRECTIVES.** *Every implemented proof rule ships ON* ([[no-rule-off-configs]]). **Plan rev 77:
no crude approximations of an understood mechanism** — implement what matches or exceeds kissat, or ship the
approximation with a stated reason AND a named closer in the deviation ledger; an approximation of a known
mechanism is **a defect, not a §5.6 arm**. **Plan rev 78: when #solved and PAR-2 conflict, #SOLVED WINS**
(cost still measured, reported, and given a closer; losing a solve is never covered).

**MEASUREMENT HYGIENE — read before trusting any delta.** Tier C noise floor **6.72 PAR-2** between
bit-identical arms on an IDLE partition (window `20260804-030938`; per-instance median **4.8 s**). The older
**10.8** figure is the CONTENDED bar — keep it for runs that shared the partition, since it was measured
under contention (window `20260731-013523`,
01:35–03:28, overlapped the full Tier C array `20260731-021615`, 02:16–04:04, by ~1 h 12 m). Treat 10.8 as an
upper bound on quiet-partition noise, and re-measure it on an idle partition before any verdict hangs on it.
**▶ THE MAY-NOT-GATE SET (a per-instance verdict on any of these is inadmissible until it replicates).**
`j3037`, `jgiraldezlevy`, `urq45` (probe 1579386: stable under replication ≤1.0 %, **8–23× under unrelated
knobs**) · `mod4block` (`2c77bce`: 73× for kissat too) · **`frb45-21-2` — ADDED 2026-08-04** (probe 1582186:
bit-identical under replication and under `chrono` 90/105/115, but **6.2×** across `walk600` 105.1 s →
`bumplim9` 652.3 s) · **`ContextModel`** (L20's knife-edge). Small Tier C aggregates near any of them are
unreliable — quote per-instance replicate ranges.
**The set is not a licence to quarantine at will.** A NEW loss is real until it goes through the
discriminator itself, and quarantining by analogy is the specific error the 2026-08-04 pre-registration
forbade — `ContextModel` looked like `j3037` and was the opposite (it REPLICATES 3/3 windows).
**Never mix 14- and 28-instance Tier B PAR-2.**
**▶ AND WALL CLOCK MOVES INDEPENDENTLY OF TRAJECTORY UNDER CONTENTION.** `veer_axi` gave **201,607 ms** and
**152,283 ms** on two runs of the SAME binary with **bit-identical** decisions, conflicts and propagations —
a **32 %** spread, from sharing the partition with a tier array. **Read wall-clock deltas against the
trajectory counters** (decisions/conflicts/propagations are deterministic; wall time is not), and pair A/B
arms in ONE submission. A single probe on that instance cannot resolve anything under ~1.3×.
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

#### Tier B (28) — window `20260804-154329` (git `6018366`), shipped default, `broad` = `TACTIX_FACTOR_SCOPE_PCT=50`, every arm writing DRAT

| # | instance | st | shipped | broad | kissat | ×kis |
|---|---|---|---|---|---|---|
| 1 | `mod4block_2vars_10gates_u2_autoenc-sc2009.cn` | UNK | **TO** | 245.8 | 18.7 | 32.1 |
| 2 | `or_randxor_k3_n540_m540.cnf.xz` | UNK | **TO** | 19.4 | 12.4 | 48.4 |
| 3 | `unsat-set-b-fclqcolor-10-07-09.sat05-1282.re` | UNK | **TO** | 244.3 | 17.3 | 34.7 |
| 4 | `DivS_862_11.cnf.sanitized.cnf.xz` | SAT | 113.1 | 117.9 | 18.7 | 6.1 |
| 5 | `sted5_0x24204-50.cnf.xz` | SAT | 80.2 | 31.4 | 8.5 | 9.4 |
| 6 | `mp1-klieber2017s-0500-023-t12.cnf.xz` | SAT | 68.5 | 60.8 | 12.2 | 5.6 |
| 7 | `PRP_30_38.cnf.xz` | SAT | 46.4 | 42.6 | 16.8 | 2.8 |
| 8 | `qwh.50.1250.shuffled-as.sat03-1655.cnf.xz` | SAT | 45.7 | 45.9 | 11.1 | 4.1 |
| 9 | `2013113162201nw.shuffled-as.sat03-1668.cnf.x` | SAT | 44.9 | 51.3 | 6.2 | 7.2 |
| 10 | `b22_1.cnf.xz` | UNS | 39.7 | 35.2 | 9.2 | 4.3 |
| 11 | `veer_axi_yosyshq_appnote_123_veer_axi-p23_tr` | UNS | 32.9 | 29.8 | 3.2 | 10.2 |
| 12 | `FmlaEquivChain_4_6_6.sanitized.cnf.xz` | UNS | 25.0 | 26.2 | 12.6 | 2.0 |
| 13 | `combined-crypto1-wff-seed-1-wffvars-450-cryp` | SAT | 17.5 | 16.8 | 4.2 | 4.1 |
| 14 | `9vliw_bp_mc.shuffled.cnf.xz` | UNS | 15.8 | 15.4 | 8.6 | 1.8 |
| 15 | `circuit_48in64out_with_800gates_4in4out_dist` | SAT | 15.1 | 17.2 | 11.3 | 1.3 |
| 16 | `summle_X4044_steps7_I1-2-2-4-4-8-25-100.cnf.` | SAT | 14.7 | 5.4 | 5.9 | 2.5 |
| 17 | `x9-08014.sat.sanitized.cnf.xz` | UNS | 13.1 | 13.0 | 10.8 | 1.2 |
| 18 | `mp1-blockpuzzle_5x10_s7_free4.cnf.xz` | UNS | 12.9 | 12.7 | 9.2 | 1.4 |
| 19 | `SCPC-500-5.cnf.xz` | UNS | 8.4 | 8.4 | 11.5 | 0.7 |
| 20 | `SC25_Timetable_C_392_E_45_Cl_25_D_7_T_50.nor` | SAT | 6.8 | 6.5 | 9.2 | 0.7 |
| 21 | `rbsat-v760c43649gyes10.cnf.xz` | SAT | 5.8 | 5.8 | 5.9 | 1.0 |
| 22 | `hwmcc17miters-xits-iso-oski15a08b00s.sanitiz` | UNS | 5.7 | 5.7 | 3.8 | 1.5 |
| 23 | `square.2.0.i.smt2-cvc4.cnf.xz` | UNS | 3.9 | 4.8 | 4.0 | 1.0 |
| 24 | `oddball_13_5_ttf.normalised.cnf.xz` | UNS | 3.1 | 3.1 | 19.0 | 0.2 |
| 25 | `battleship-14-26-sat.cnf.xz` | SAT | 1.6 | 1.6 | 6.8 | 0.2 |
| 26 | `pyhala-braun-sat-35-4-04.shuffled.cnf.xz` | SAT | 0.9 | 7.3 | 4.0 | 0.2 |
| 27 | `DecompressReader_read_12.cnf.xz` | UNS | 0.5 | 0.5 | 6.5 | 0.1 |
| 28 | `clique_n2_k10.sanitized.cnf.xz` | UNS | 0.0 | 0.0 | 6.1 | 0.0 |
|  | **PAR-2 / solved** |  | **86.51** (25/28) | **38.39** (28/28) | **9.78** (28/28) | **8.845** |

**The `broad` arm is the item-(a) re-test and it is HELD** — see the Log and
`benchmarks/reports/2026-08-04-broad-factor-admission-retest.md`. It is **+3 solves here**
(`or_randxor`, `mod4block`, and **`fclqcolor`, the last Tier B timeout**) and beats cadical's 40.38,
but it loses a Tier C solve and costs T3 on both halves. Ex-`mod4block` (documented lottery):
shipped 67.49 (25/27) vs broad 30.70 (27/27).

#### Tier C (30) — window `20260804-154341` (git `6018366`), same submission, both baselines carried

| # | instance | st | shipped | broad | kissat | cadical | ×kis |
|---|---|---|---|---|---|---|---|
| 1 | `128_125.sanitized.cnf.xz` | UNK | **TO** | **TO** | **TO** | **TO** | 1.0 |
| 2 | `T87.2.0.cnf.xz` | UNS | 768.2 | 802.0 | 259.7 | 399.5 | 3.0 |
| 3 | `asconhashv12_opt64_H4_M2-LOD9_m0_2_U19.c.cnf` | UNS | 623.1 | 609.4 | 100.3 | 271.6 | 6.2 |
| 4 | `frb45-21-2.used-as.sat04-884.cnf.xz` | SAT | 598.4 | 598.6 | 93.4 | 158.0 | 6.4 |
| 5 | `T105.2.0.cnf.xz` | UNS | 583.2 | 588.2 | 288.0 | 259.2 | 2.0 |
| 6 | `goldcrest-and-9.cnf.xz` | UNS | 538.6 | 493.1 | 103.9 | 169.8 | 5.2 |
| 7 | `j3037_9_rggt_b.cnf.xz` | SAT | 467.7 | 18.0 | 157.4 | 25.3 | 3.0 |
| 8 | `sat-bench-trig-bhaskara.cnf.xz` | UNS | 352.2 | 355.8 | 378.5 | 402.5 | 0.9 |
| 9 | `minxor128.cnf.xz` | UNS | 308.5 | 506.3 | 230.3 | 279.3 | 1.3 |
| 10 | `Grain_no_init_ver1_out200_known_last105_0_u.` | UNS | 244.8 | 205.0 | 234.2 | 264.0 | 1.0 |
| 11 | `constraints_18_0.3_2.sanitized.cnf.xz` | UNS | 243.5 | 243.6 | 196.8 | 249.7 | 1.2 |
| 12 | `ecarev-110-4099-22-30-7.cnf.xz` | SAT | 242.7 | 258.1 | 224.3 | 190.6 | 1.1 |
| 13 | `circuit_32in32out_with_500gates_6in6out_dist` | SAT | 225.3 | 224.2 | 114.7 | 475.8 | 2.0 |
| 14 | `StConn_8_32.sanitized.cnf.xz` | UNS | 192.7 | 258.6 | 276.8 | 957.3 | 0.7 |
| 15 | `ER_400_20_4.apx_2_DC-ST.cnf.xz` | UNS | 181.5 | 180.9 | 152.1 | 172.5 | 1.2 |
| 16 | `php17-mixed-35percent-blocked.cnf.xz` | UNS | 175.4 | 171.5 | 180.8 | 242.3 | 1.0 |
| 17 | `constraints_25_4_5_12_12_0_0_0.sanitized.cnf` | UNS | 167.2 | 177.3 | 132.5 | 192.2 | 1.3 |
| 18 | `mod2c-rand3bip-sat-250-2.shuffled-as.sat05-2` | SAT | 147.2 | 153.5 | 40.8 | 185.2 | 3.6 |
| 19 | `af-synthesis_stb_50_120_4_sat.cnf.xz` | SAT | 133.3 | 134.2 | 141.6 | 136.0 | 0.9 |
| 20 | `Ptn-7824-b18.cnf.xz` | SAT | 131.3 | 40.9 | 254.9 | 10.2 | 0.5 |
| 21 | `urq45.cnf.xz` | UNS | 127.9 | 127.1 | 118.9 | 989.1 | 1.1 |
| 22 | `DLTM_twitter690_74_16.cnf.xz` | SAT | 121.9 | 48.7 | 116.7 | 272.6 | 1.0 |
| 23 | `em_11_3_4_cmp.cnf.xz` | SAT | 102.2 | 237.5 | 107.4 | 102.0 | 1.0 |
| 24 | `aes_decry_2_rounds.debugged.cnf.xz` | UNS | 71.5 | 70.9 | 90.5 | 79.1 | 0.8 |
| 25 | `SCPC-1000-18.cnf.xz` | SAT | 39.8 | 39.8 | 169.1 | 26.0 | 0.2 |
| 26 | `1-ET-256-K-70.sanitized.cnf.xz` | UNS | 39.7 | 40.6 | 59.6 | 28.4 | 0.7 |
| 27 | `EDP3-11000.cnf.xz` | SAT | 24.0 | 40.4 | 67.2 | 210.8 | 0.4 |
| 28 | `ContextModel_output_7_3_9.bul_.dimacs.cnf.xz` | SAT | 10.0 | **TO** | 39.4 | 64.8 | 0.3 |
| 29 | `mp1-Nb5T15.cnf.xz` | SAT | 5.2 | 806.1 | 78.0 | 129.2 | 0.1 |
| 30 | `jgiraldezlevy.2200.9086.08.40.41.cnf.xz` | SAT | 4.7 | 210.7 | 114.3 | 17.4 | 0.0 |
|  | **PAR-2 / solved** |  | **309.05** (29/30) | **414.70** (28/30) | **230.74** (29/30) | **312.02** (29/30) | **1.339** |

**Reading this table.**
- **`broad` loses exactly one solve — `ContextModel`, 10.0 s → TO (120×)** — and that loss **replicates
  3/3 windows for this knob** (22.2× / 54.9× / 120.2× across three bases), which is what makes it real
  where `j3037`'s was not. No proof rescue fired and no verdict mismatch anywhere in this window.
- **The whole Tier C cost is TWO instances:** `ContextModel` +2,390 s and `mp1-Nb5T15` +801 s of a
  +3,170 s delta (7 instances >10 % worse, 4 >10 % better, **19 flat**). Quarantining both leaves broad at
  **+8.89 PAR-2**, ~1.3 noise floors. That is the (b′) argument.
- **`j3037` inverted:** 467.7 → **18.0 s, broad 26× FASTER**, confirming the LOTTERY verdict from probes
  1581982/1582186 and retiring the veto that held this change on 08-03.
- **The free control set.** The nine instances broad provably cannot admit Factor on at t=0 must run
  identical trajectories; their wall ratios here are **0.84 / 0.99 / 1.00 / 1.06 / 0.99** (Tier C) and
  **0.96 / 1.04 / 1.14** (Tier B) — an in-window contention estimate at zero cost. **Membership must be
  argued per instance, not assumed:** the classification is a t=0 LOWER BOUND (the gate is re-evaluated on
  the live formula), and `mod4block` — predicted inert at 7.9 % scope — in fact moved TO → 245.8 s.

**▶ T3, BOTH HALVES, from this one window** (`tools/bench/subset_score.py`, subset = 24 of 30):

| half | requirement | measured | status |
|---|---|---|---|
| **CaDiCaL** | within 10 % on Tier C | **1.069×** (subset) / **0.990×** (full) | **MET on the full tier**, 0.069 over on the subset |
| **Kissat** | parity on the rule-neutral subset | **1.346×** | **NOT met** — and WORSE than 08-03's 1.271× (pre-TB4b); see (c′) |

**The Kissat half remains the binding constraint.** The `broad` arm moves it the wrong way —
**1.346 → 1.500×** (cadical 1.069 → 1.191×) — which is what decides item (a) independently of the
solve count.

**▶ WITHIN-WINDOW vs ACROSS-WINDOW ARE DIFFERENT BARS, and the second is BIGGER (measured 2026-08-04).**
The 6.72 floor is between bit-identical arms *inside one window*. **Across windows the same shipped solver
moved 12.23 PAR-2**: Tier C **309.05** (`20260804-154341`) vs **296.82** (`20260804-165516`) — two runs the
same day, on an idle partition, at commits that differ only in documentation, i.e. **byte-identical solver
behaviour**. That is **1.8 within-window floors**. Tier B was stable across the same pair (86.51 vs 86.42).
**Consequence: never compare an absolute Tier C PAR-2 across windows** — only same-window arms, which is
L6 with a number attached. Quoting "where we stand" from one window is precise to about ±12 on Tier C.

**Noise floor — USE 6.72 for SAME-WINDOW arms (window `20260804-030938`, idle partition, three
bit-identical arms: 364.50 / 362.17 / 357.78, per-instance median 4.8 s).** The older 10.8 below is the CONTENDED bar; keep it only for
runs that shared the partition.
**The older 10.8 bar** was three bit-identical arms in a CONTENDED window (`20260731-013523`, which overlapped a full Tier C array by ~1 h 12 m), so it is an upper bound on contended noise, not a measurement of quiet-partition noise. Use 6.72 for idle, 10.8 only for runs that shared the partition.

**Older windows** — aggregates only, per-instance detail in each run's `results.json`, and **not cross-window comparable** (see the bar above). The shipped Tier C ratio moved 1.007–1.066× across the 07-29→07-31 windows at an UNCHANGED default, which is the whole argument for same-window A/B. **Never mix 14- and 28-instance Tier B PAR-2** — pre-2026-07-29 Tier B numbers are `core14`.

**▶ PROTOCOL BREAK — never quote a pre-2026-07-28 Tier C aggregate against a current one.** Before the like-for-like proof protocol settled, an instance whose DRAT check exceeded the checker budget was scored UNSOLVED even though we solved it (verified: `urq45`, a 354.5 s solve scored unsolved on a 2400.1 s check), overstating our Tier C PAR-2 by up to ~+68.

## Standing lessons — measured, do not re-derive

**▶ MOVED to [`docs/lessons.md`](docs/lessons.md)** — 22 active entries, freely revisable, **not to be
cited by number in any document.** They exist to guide research and decisions; when one informs a choice,
write the finding and its primary evidence into the document instead of pointing at a number. Six were
DELETED on 2026-08-04 for resting on single draws from chaotic instances.

**What stays here, because it is STATUS not a lesson — the may-not-gate set.** A per-instance verdict on
`mod4block`, `j3037`, `jgiraldezlevy`, `urq45`, `frb45` or `ContextModel` **may not gate a change alone**
([[chaotic-instance-veto]]); perturb the baseline with a mechanically-inert knob first, and judge on the
remainder or a replicated distribution. Details in the measurement-hygiene block above.
*(A hostile re-validation on 2026-08-04 DELETED six entries that rested on single draws from these
instances. Do not cite lesson numbers in documents — state the finding and its primary evidence
instead; see the policy at the top of `docs/lessons.md`.)*

## Open items (small, none blocking)

- **▶ `D-LRAT-FALLBACK` — the plan REQUIRES an LRAT re-check that `run.py` does not implement.** §8.1:
  *"A proof that outruns even this is re-checked as native LRAT before the instance is called unsolved
  (`--lrat` + cake_lpr verified `urq45` in 77 s where drat-trim needed 1447 s)."* The runner picks the format
  ONCE from the arm's command (`proof_format()`, `run.py:112`) and has no re-check path, so a DRAT check that
  blows the budget scores our instance **unsolved** with no second chance. **This has cost real results
  three times, all on `urq45`:** 2026-07-27 (354.5 s solve, 2400.1 s check); **2026-08-03, where it took a
  scored solve off the `broad` arm** — a correct UNSAT in 619.3 s — and so contributed to the 27/30 that
  HELD broad Factor admission; and again the same night, where both our arms spent ~1 h of the gate's
  wall-clock checking it. **Fix:** on a scored arm whose DRAT check exhausts its budget, re-run with
  `--lrat` and verify with cake_lpr before calling the instance unsolved. Costs one re-solve plus a ~77 s
  check against 4800 s burned. Route: bench runner (and it subsumes the older "verify LRAT not DRAT" item).
  **Implementation, self-contained:** the trigger is `check_proof`'s own message — it returns
  `(False, "checker exceeded its {budget}s budget")` on timeout, and **only that** may trigger the fallback;
  a genuine REJECTION (checker ran and said no) must still score unsolved, since that is a real N2 failure.
  It must be a **re-SOLVE with `--lrat`, not a conversion** — turning DRAT into LRAT needs the same
  drat-trim backward search that just timed out. Guard it to `score_proofs` arms (since rev 79 baselines are
  not verified at all, so that implies our arm, whose CLI takes `--lrat`). Record the rescue in the result
  (`proof_rescue`, `proof_rescue_time`) and surface a count in `summarize()`. **Verify by: (1) ctest; (2) a
  targeted `tierC/urq45` probe that ASSERTS the rescue actually fired — a silent non-trigger is the
  identical-artefacts trap; (3) confirming a genuinely rejected proof still scores unsolved, via the fuzz
  corpus's deliberately-broken wrappers.**
- **Filed by the 2026-08-03 doc audit** (details in the Log entry of that date):
  - **`D-STRENGTHEN-BUDGET`** — C1 Strengthen / SubsumeForward are the only inprocessing slot entries that are
    neither dosed nor budget-aware, and one round cannot be interrupted. C2 Strengthen owns **97.8 s of 109.0
    (89.7 %)** of `DivS_862_11` in **four applications**. Had no ledger row and no checklist item. Route via
    the calculus skill when it becomes a guard change.
  - ~~`D-FACTOR-REV78`~~ — **CLOSED 2026-08-04 late. Verdict still HELD, but on a DIFFERENT instance.**
    The 08-03 pass held it on `j3037`; the follow-up it filed ("replicating that loss") ran and
    **REFUTED it** — `j3037` is a lottery (K′=11 paired draws; broad **26× faster** on it in the new
    window). The re-test at HEAD holds broad on **`ContextModel`** instead, which replicates 3/3 windows,
    and on T3 (kissat 1.346 → 1.500×). Its other 08-03 "loss", `urq45`, was indeed the checker-budget
    artefact and is scored correctly now that `D-LRAT-FALLBACK` has shipped. **When citing the 08-03
    report, cite the 08-04 one with it** — `benchmarks/reports/2026-08-04-broad-factor-admission-retest.md`
    supersedes its per-instance conclusion. Live work continues as **(b′)**.
  - **T3 instrument — CLOSED 2026-08-03.** `benchmarks/tiers/C-ruleneutral.list` (24 of 30,
    derived from window `20260727-002213-C-kabl`, frozen with its known defect + closer in the header) and
    `tools/bench/subset_score.py` (scores any Tier C run over the subset AND the full tier from one
    `results.json` — the subset is a scoring VIEW, never a separate run). The kissat arm needed no code:
    `run.py` already takes repeatable `--baseline`, it had simply stopped being passed on Tier C. **Standing
    rule from here: every Tier C run carries `--baseline kissat --baseline cadical`**, or T3's kissat half
    goes unmeasurable again.
  - ~~**Tier D does not exist**~~ — **BUILT AND FROZEN 2026-08-03** (793 instances, plan rev 79). Never RUN;
    running it is the phase-3-exit prerequisite (§11 O9), and `--tier D` still has no per-track flag for T4's
    SC25-set PAR-2.
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
| N1 | Any applicable rule appliable via API at any time, incl. branching choice | **✗ ONE OPEN BREACH (found 2026-08-04, N4 audit): `Factor`.** Its guard reports applicable and `factor_instances` offers `INSTANCE_CANONICAL`, but `factor_apply` NoOps on the density gate, so **no API caller can apply a legal factoring on a binary-sparse formula**; the comment at `factor.cpp:552` claims the opposite and `TACTIX_FACTOR_FORCE` is an env hatch, not an API capability. Fix = MOVE the predicate to DefaultPolicy (where BVE's identical gate correctly lives) — **Track SCHED · SCHED-N1FIX, lands regardless of any A/B.** Otherwise: ◐ v1 API frozen (revs 20–21); §8.4 demos 1–3 prove it end-to-end; audits upheld. Two leaks found+fixed (rev 34 payload aliasing; rev 40 off-fixpoint Decide via `trail_tainted`). Final acceptance = CCE plugin (phase 4) |
| N2 | Proof production on every path (DRAT/LRAT) | ◐ DRAT since phase 1; native LRAT 2026-07-14; every fuzz UNSAT checker-verified on all formats under random schedules; audit UPHELD |
| N3 | DefaultPolicy comparable to Kissat (T4 gate) | ◐ T1+T2 passed. **2026-08-04 (window `20260804-1543`, git `6018366`): Tier B(28) 86.51 (25/28) vs kissat 9.78 · Tier C 309.05 (29/30) vs kissat 230.74 and cadical 312.02.** T3 halves: **kissat subset 1.346×** (the binding phase-3 constraint; was 1.271× pre-TB4b), cadical subset 1.069× / full tier 0.990×. Tier A 0.98×. **Tier D BUILT AND FROZEN 2026-08-03** (`benchmarks/tiers/D.list`, 793 instances, rev 79) — never yet RUN, and running it is a phase-3-exit prerequisite (§11 O9). |
| **N4** | **DefaultPolicy's SCHEDULING at least as sophisticated as Kissat's default strategy** (rev 80, user 2026-08-04) | ✗ **KNOWN UNMET, now MEASURED: 216 mechanisms audited, 132 gaps (27 high).** Ledger LANDED (`docs/implementation-notes.md` §13, living; full audit frozen at `docs/design/2026-08-04-n4-scheduling-parity-audit.md`); remediation staged as **Track SCHED**. Four structural highs: ONE inprocessing clock where both references keep TWO · effort meters SHARED where both keep them private · slot order emergent from `strcmp` · ceilings frozen at phase entry. Two confirmed gaps already: (a) `Kernel::factor_bound` IS kissat's `reduction > bound` acceptance test but defaults to 0 and is **never coupled to elimination progress** (kissat ramps it 0→1→2→4→8→16, `factor.c:72/939`); (b) our `binaries ≥ 6·vars` Factor admission gate has **no counterpart in either reference** and measured **worse than no gate at all** on Tier B (86.42 vs 34.59 for always-admit; oracle 32.25; −3 solves). N4 is a *mechanism* floor — it may not be discharged by a tier number, and never deferred to §5.6. |

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
  | CaDiCaL | ≤ 1.10× cadical, whole Tier C | **MET on the full tier: 0.990×** (window `20260804-154341`, git `6018366`); **1.069×** on the rule-neutral subset, i.e. 0.069 over there. Superseded the 08-03 reading of 1.117×. |
  | Kissat | parity on the rule-neutral subset | **NOT MET: 1.346×** (window `20260804-154341`, git `6018366`). **This is the BINDING half** — and it REGRESSED across the TB4b ship, 1.271× (08-03, pre-TB4b) → 1.346×, which is the open question filed as (c′). Full tier 1.339× kissat. |

  **Both prerequisites are now DONE** (2026-08-03): `benchmarks/tiers/C-ruleneutral.list` is materialized and
  `tools/bench/subset_score.py` scores both views from one window, and every Tier C run carries
  `--baseline kissat --baseline cadical`. **Caveat that still stands:** the subset is derived from a
  SINGLE-SHOT sweep ablation and 4 of its 6 exclusions are measured perturbation-FRAGILE instances, so the
  list itself needs re-deriving with replicates before a near-parity verdict on it is trustworthy.

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
  **▶ SUPERSEDED 2026-08-02:** R-d retirement took Tier C 329.63 → 358.12 (1.039× → 1.129×) in exchange for
  +2 Tier B solves, banked under plan rev 78 — so the CaDiCaL half is NOT met at the current default. The
  status table at the top of this gate row governs.
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

Diagnosed 2026-07-21: four tail instances were 69 % of Tier B time, each with a different cause.
**TB1/TB2/TB3/TB5/TB6 are CLOSED** — detail in the W30/W31 log archives and the cited probes.

- [x] **TB1** congruence is NOT the FmlaEquiv gap (the all-off control, L8's origin); structural-hashing
  gap real but ≤ 0.7 s · **TB2** x9's outlier premise dissolved (1.17–1.19× vs kissat) · **TB3** Factor
  verifiably non-inert on x9 · **TB5** per-prop cost is AT PARITY with kissat on all residuals — the
  residual is SEARCH SHAPE, and the watch-layout engineering lane is measured-dead · **TB6** default mode
  alternation BEATS both pinned modes; mode schedule is a phase-6 arm, not a static defect
- [x] **TB4** chrono ON by default (`CHRONO_LEVELS=100`). The first measurement (+13 %) was a PORTING
  ERROR, not a verdict — analyzing from above the conflict level silently DROPPED the conflict on 84 % of
  chrono backtracks; fixed → −2.4 %. **`stats.backjump_stale` MUST stay 0.** Lesson: found only because a
  negative result was audited rather than accepted
- [ ] **TB4b** the remaining chrono divergences. **(A) the one-literal-on-conflict-level shortcut** and
  **(B) the watch-repair primitive** are DONE (`fc57eff`; the 2026-07-28 negative verdict was retired as
  a BROKEN arm, not a crippled one — report `2026-08-01-tb4b-watch-repair.md`). **Ships OFF**: ON and
  CRIPPLED are indistinguishable on Tier C and the proof-size axis FLIPS SIGN between tiers, so it is a
  §5.6 exhibit, not a default. **Unit chrono routing SHIPPED** 2026-08-04 (`8118c68`, `veer_axi` 6×) —
  and introduced the N2 defect fixed in `52a4e8f`. **Still unported: `jumpreasons`, cadical's
  `chronoreusetrail`.** `j3037`/`urq45` chrono sensitivity remains UNEXPLAINED.

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

### ▶ Track W — S6 `Walk` local search (plan rev 71) — **SHIPPED ON, and CLOSED for phase 3**
*(its phase-6 hand-off moved to the Phase 6 section — nothing in this track is phase-3 work)*

**▶ WALK IS ENABLED BY DEFAULT — no knob needed.** `ROTATION` fires it on **2 of every 6 rephase events**
(`default_policy.cpp:809-814`, `{BEST, WALK, INVERTED, BEST, WALK, ORIGINAL}`) at
**`WALK_PERMILLE = 560`** (`default_policy.hpp:310`); `walk_guard` is only
`!inconsistent && h != nullptr && nvars > 0` (`walk.cpp:411`) and **there is no off switch** — no
`TACTIX_NO_WALK` exists. Witness on a stock build: `c walk 12 walks (12 improved, 106,336,087 flips)`.
*(Wording note: "gated" below means **passed its gate**, i.e. shipped — not switched off.)*

Local search was the largest missing mechanism (`--walkeffort=0` costs kissat Tier B 8.62 → 29.22).
`H`-only policy op, no proof delta: `src/policies/ops/walk/`, design `docs/design/s6-walk-design.md`
(§6 carries the full ledger of what was tried). Detail: the W-commits + the W30/W31 log archives.

- [x] **W1** walker core (`8da55c9`) · **W4a** `--walk-stress-policy` (`43510fc`) — ProbSAT/CB scoring,
  kernel untouched (legal at any level, N1); fuzz 10k/0 × 6 legs incl. ASan+UBSan
- [x] **W2** schedule — **Walk AUGMENTS Best, it does not replace it.** Kissat's `B,W,I,B,W,O` ported
  literally REGRESSED Tier B (13.63 vs 10.04); cause was the lost Best consumption, not the walk
  (kissat saves phases at ASSIGN, we at unassign — `implementation-notes.md` §8). Shipped `B,B+W,I,B,B+W,O`
- [x] **W3** warmup measured, **NOT adopted** — our saved phases already come from a real trail
- [x] **W4** gate PASSED, shipped ON: Tier B **8.97 vs 9.88** (−9.2 %), Tier C **FLAT** (Walk wins and loses big).
  **Banked negatives, do not retry blind:** effort anchored on the last inprocessing phase (9.69 vs 8.97);
  accept-only-if-better-than-all-previous (worse than both arms)
- [x] **W4b/W4c/W4d** the `mp1-Nb5T15` poisoning is the **target** channel, not the dose, and **kissat has
  it too**; six reference-matching attempts exhausted, five dead or context-dependent; W4d's
  "stop walking when it stops paying" is DEAD (it was under-dosing, and would have been a novel mechanism
  neither reference has). Five arms sit behind knobs for §5.6
- [x] **W4f · walk DOSE swept and SHIPPED at 560 per-mille** (`0c52fdd` → `61138b8`): root defect was a UNIT
  confusion — we scale a per-mille of search ticks but SPEND in walk steps (~13× less scanning than
  cadical). Tier C 392.98 → 322.80, `mp1-Nb5T15` TO → 26.1 s. Lesson (L12 again): the sharp Tier C
  preference for 640 was an artefact of the then-wrong `bumpreasons` rate; re-swept, 400..800 shows no
  trend. Report `2026-07-27-tier-c-push.md` §1/§8.2

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
run it you cannot stop), leaving only a STATIC per-instance decision — and the best such separator measured
4/13 misclassifications, i.e. chance (`2026-07-20-bve-skip-feasibility.md`). Phase-6 context feature, not an engineering
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

### ▶ Track SCHED — the scheduling floor (plan rev 80 §1.1 N4, §5 obligation 5)

**Namespace:** `SCHED-<stage><n>` is a NEW id space — never read a bare `B1`/`C2` here as Track S/Track R.
**▶ RENAMED from "Track N4" on 2026-08-04, hours after it landed.** It was named after the
non-negotiable it discharges, which gave `N4` two meanings — a REQUIREMENT (plan §1.1) and a TRACK —
and produced ids like `N4-N1FIX` carrying both non-negotiable names at once. That is the `R<n>` mistake
again (see the four-namespace key under `## Log`), caught early enough to be free. **`N4` alone now
always means the non-negotiable.** The two frozen 2026-08-04 design docs still say "Track N4"; frozen
docs are never edited, so read that as Track SCHED.
**Work list:** the scheduling-parity ledger (`docs/implementation-notes.md` §13, living) + the frozen
audit `docs/design/2026-08-04-n4-scheduling-parity-audit.md` + the diagnosis
`docs/design/2026-08-04-scheduling-sophistication-diagnosis.md`. **216 mechanisms audited, 132 gaps
(27 high).** Themes **T1** dropped wires · **T2** repetition-not-dose · **T3** right formula wrong meter ·
**T4** invented instance gates · **T5** cross-keyed ramps · **T6** emergent order.
**Tags:** `[wire]` = wiring a mechanism we already own · `[arch]` = new architectural support ·
`[revamp]` = routes out via the revamp skill.

**Standing protocol (stated once, applies to every item).** Same-window Tier B+C A/B, fresh
`build/pin-n4-<item>` ([[build-path-ownership]]). **Every item names a SOURCE-LEVEL readout, pre-registered
and checked FIRST — an arm whose source counter did not move was not tested** ([[ab-arm-identical-artefacts]],
L22/L25b). Noise floor 6.72; **#solved beats PAR-2**; `mod4block`/`crypto1`/`j3037`/`frb45`/the fragile
three may not gate alone ([[chaotic-instance-veto]]). Rule-touching items carry DRAT + native-LRAT +
random-policy 10k/0; items opening NEW schedules add a stress-policy leg.
**▶ EVERY PRIOR DOSE NEGATIVE MEASURED THROUGH THE BROKEN METER IS VOID** and must be re-derived after
Stage B — named: the V4 ticks-meter arm, the V4 early-exit arm, the C1/C2 mid-round budget arm, every
per-entry per-mille sweep on a ticks-metered rule, and the BVE ramp-ceiling 8-vs-16 choice.

**Track exit (checked at the phase-3 gate):** no HIGH ledger row remains `WE_ARE_CRUDER`/`WE_LACK_IT`
without a closure or an accepted deviation row carrying a measurement, AND the ledger has been
regenerated from the REFERENCE's option set (SCHED-A3).

- [ ] **▶▶ SCHED-N1FIX · Factor's density gate is an N1 VIOLATION — fix regardless of any A/B** `[wire]`
  **This is a non-negotiable breach, not a performance item, and it outranks everything else in the track.**
  `factor_guard` is the pure `level==0` precondition and reports APPLICABLE; `factor_instances`
  (`factor.cpp:937-941`) offers exactly one instance; `factor_apply_instance` routes it straight back into
  `factor_apply`, which returns **`NoOp` whenever `binary_ids.size() < 6·num_output_vars`**
  (`factor.cpp:588,624`). **So on any binary-sparse formula NO caller of the public API — an LLM policy, an
  RL policy, RandomPolicy — can apply a legal factoring at all.** The rule's own comment at
  `factor.cpp:552` asserts the opposite ("N1 keeps every factoring applicable via the API regardless"); it
  is false. `TACTIX_FACTOR_FORCE` is an env hatch, **not an API capability**. Rev 76's carve-out covers the
  built-in POLICY declining — here the RULE declines, so the capability is removed. **Contrast: BVE's
  identical density gate lives in `default_policy.cpp:1034-1052` as a policy `continue` and is N1-clean.**
  **Fix = MOVE, not delete:** relocate the predicate to DefaultPolicy's slot loop. DefaultPolicy behaviour
  is byte-identical (the same occasions are declined), the API surface is restored, and whether to KEEP the
  predicate then becomes a separate measured question (already measured both ways: deleting it is +3 solves
  on Tier B, −1 solve on Tier C — `2026-08-04-factor-oracle-ablation.md`). **Gate: byte-identical DRAT on
  3 instances + a test that `tactix_rule_apply(Factor)` succeeds on a binary-sparse formula where it
  currently NoOps.**

#### Stage A — the instrument and the guard rails (no behaviour change; nothing later ships without these)

Causes (D) and (E) of the diagnosis are INSTRUMENT defects — the eval loop selects for gates, and the
register has the wrong schema. Fix the instrument before spending a tier window on a mechanism.

- [ ] **SCHED-A1 · per-entry slot telemetry** `[arch]` — one `c slot` line per entry per solve:
  occasions / admitted / declined{preprocess,material,delay,density,guard,budget,cap} / handed / spent /
  applies / yield / completed, mirrored into `TACTIX_STAT` ids. **Removes the "starved vs nothing to do"
  blindness that makes today's dose experiments uninterpretable**, and is the readout every later item
  pre-registers against. Gate: byte-identical proofs + identical `c stats` on 3 instances.
- [ ] **SCHED-A2 · config-wiring conformance test** `[arch]` — a table of every `Kernel` scheduling-config
  field with its intended writer, and a ctest that FAILS if a field tagged `DefaultPolicy` has no writer in
  the policy. **Structurally de-selects T1's sub-shape "we own the mechanism and nothing writes it"** —
  the most common high-severity row (`factor_bound`, `bve_cost_limit`, `bce_occ_limit`,
  `factor_cand_rounds`, `factor_demote_fresh`, `used_glue[1]`, `arena_mark`). Ships with the doc-drift
  corrections the audit found.
- [ ] **SCHED-A3 · regenerate the ledger from the REFERENCE's option set** `[arch]` — every option in
  `kissat/src/options.h` + `cadical/src/options.hpp` gets a row naming our counterpart (file:line) or a
  gap; a test fails when an option has no row. **The only item that converts coverage from opportunistic to
  complete** (diagnosis cause A). **Must read MACROS, not just grep** — cadical's `factorthresh` is
  token-pasted and reading it as dead code produced two wrong published claims (L19 correction).
- [ ] **SCHED-A4 · per-instance ORACLE column in the benchmark runner, on by default** `[arch]` — a gate must
  beat BOTH always-on and the oracle to ship. **De-selects cause D at the instrument**, the only level at
  which a selection pressure can be removed. It is what exposed the Factor gate in one window.

#### Stage B — an honest meter (T3). *Every dose number in the solver is currently unreliable.*

- [ ] **SCHED-B1 · ceiling at RUN time · anchor on the ADMITTED path · per-entry `thresh`** `[wire]`+`[arch]`
- [ ] **SCHED-B2 · private per-rule tick meters (`Kernel::tick_sink`)** `[arch]` — a `uint64_t *` the kernel's
  charge sites increment IN ADDITION to `stats.ticks`; pure accounting, nothing in the kernel reads it.
- [ ] **SCHED-B3 · V4 currency → ticks, together with a resumption cursor** `[arch]`
- [ ] **SCHED-B4 · C1/C2 currency → per-partner-visit** `[wire]` · [ ] **SCHED-B5 · SwitchMode growth-key
  off-by-one + `search_ticks` scope** `[wire]`

#### Stage C — the free wires (T1/T5): mechanisms we own and never wired

- [ ] **SCHED-C1 · Vivify ← the dynamic tiers** `[wire]` — `VIVIFY_TIER2 = 6` while `k.tier2` is computed
  every reduce and read by nothing else. **Cheapest high-severity item on the board.**
- [ ] **SCHED-C2 · Factor acceptance bound ← `bve_level_`** `[wire]` — **do NOT re-try the naive port**: it is
  measured negative 3× and kissat at `--eliminatebound=0` is fine, so this needs a NEW mechanism or an
  accepted-deviation row, not a re-run.
- [ ] **SCHED-C3 · BVE bound-ramp advance guarded to phase completion** `[wire]` · [ ] **SCHED-C4 · split
  `TACTIX_REDUCE_SQRT` into cadence and ramp** `[wire]` (one env var moves both, so they cannot be measured
  apart) · [ ] **SCHED-C5 · demote fresh extension vars + retire the invented gates (T4)** `[wire]`

#### Stage D — the authored schedule and the two clocks (T6 + the global slot rows)

- [ ] **SCHED-D1 · an explicit, authored phase order AS DATA** `[wire]` — replaces `strcmp` + `move_to_back`.
  Cheap, and it makes order a first-class policy object rather than an accident of the alphabet.
- [ ] **SCHED-D2 · split the slot into a PROBE group and an ELIMINATE group** `[arch]` — the two clocks both
  references keep. **The largest single structural gap**: probing currently fires ~5× less often and the
  gap widens over the solve.
- [ ] **SCHED-D3 · slot-level new-material watermark + the reduce coupling** `[arch]`

#### Stage E — order and resumption become POLICY OBJECTS (the architecture the RL agent needs)

- [ ] **SCHED-E1 · `CandidateOrderPolicy` sub-policy + generalized `SlotEntry::cursor`** `[arch]`
- [ ] **SCHED-E2 · the ordering arms, one per rule, each independently gated** `[wire]`

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

**▶ WHY THIS SERVES PHASE 6 (the user's explicit requirement).** Every `[arch]` item above adds
*observability or control* the learned policy needs and cannot get today: A1 gives per-entry
occasions/declines/yield (the reward-shaping signal, and the labels the §5.6 admission exhibit lacks);
B2's private meters make per-rule cost attributable, without which no credit assignment over rules is
possible; E1 makes candidate ORDER a policy object — and under a cutting budget **ordering IS the
scheduler**, so this is a control surface, not a micro-optimisation; F1's touched-epoch substrate is what
lets an agent ask "what is new since I last acted?" instead of re-deriving state. **None of it narrows
`instances()` or touches a guard** — the N1 argument is per-item in the frozen audit's companion, and the
one genuine N1 breach in the codebase (SCHED-N1FIX) is *repaired* by this track, not created by it.

### Other phase-3 items

- [x] Effort-budgeted scheduler increment 1 + prerequisites (revs 53–55; design `docs/design/effort-scheduler-design.md`)
- **Exit:** T3

## Phase 3.5 — engineering ingenuity on top of kissat (PIN, rev 67)

Deliberately unelaborated (§9 row 3.5 + §11 O15): once kissat performance is matched, a dedicated pass for novel engineering beyond the reference
lineage. Concretize at the T3 gate via revise-plan.

## Phase 4 — tuning, bindings, extensibility proof

- [ ] **Decide O16b at phase entry** (PR/SR clause learning as a catalog candidate — the checkable form of
  auto-invented rules; needs dpr-trim or PR→DRAT in the N2 story).
  *(The old "catalog rules remaining" line is GONE, reconciled 2026-08-04: S7 Reorder and X1 BCE landed in
  phase 3 Track R, and V5 Sweep is in progress there as Track R · R5 — none of them is phase-4 work.)*
- [ ] **§5.5 hygiene, conditional (moved from phase 3, 2026-08-04):** SubsumeForward occs/marks alloc and
  `binary_ids` `.emplace` on the CDCL hot path — **escalate via §5.4 ONLY with a profile**, which is why
  this is not phase-3 work. (The third item, the `literal_redundant` depth cap 900 vs 1000, is CLOSED
  measured-dead — `88cff2d`, probe 1570502.)
- [ ] Independent-lineage candidates: CMS best_inv rephase (with Walk), rotation presets (O6)
- [ ] Python bindings; policy presets (MAB scheduler, classification) — §5.6 seed
- [ ] Demo policies 4–6 incl. external branching (N1 end-to-end)
- [ ] §8.4 overhead budget: default vs hardcoded twin ≤ 5% on Tier B
- [ ] **Extensibility acceptance test: CCE (X3) as pure plugin (§5.3)** — **the GATE check only.** The
  rule itself is phase-3 work (**Track R · R6**, catalog §5.2 puts X3 in phase 3 per rev 72), so this
  row is where the phase-4 exit criterion is *evaluated*, not a second implementation task.
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
- [ ] RL1: windowed/discounted-UCB bandit over the macro scheduler's slot choice points (DefaultPolicy schedule = permanent arm)
- [ ] LearnedPolicy joins the random-policy fuzz rotation + determinism trace tests
- [ ] Gate L-safe measured: within +2% PAR-2 of DefaultPolicy on Tier C medians
- [ ] RL2: contextual bandit, tabular contexts (features per O12; **rev 68: must include the density-gate family — binary density, dose
  productivity, elimination yield — per §5.6's elimination-dosing exhibit**)
- [ ] **Elimination-dosing exhibit obligations (rev 68, §5.6):** per-entry dose on/off joins the scheduled choice points; DefaultPolicy's gate
  thresholds = permanent arm; success = gates matched where right, em_11 recovered where wrong, no hand threshold load-bearing
- [ ] RL3 experiment: composed-batch sequencing (tabular RL) under its recorded kill criterion; neighbor-move arm proposals (TACO shape, in-run)
- [ ] Anytime check (§8.5, rev 28): interrupt LearnedPolicy anywhere — no segment re-solves or sits in a non-solving state
- [ ] Offline per-family preset layer (classify → presets as arms)
- [ ] Gate L-win measured on Tier D + per-family attribution report (≥ 2 family wins); O13 distillation decision
- **Exit:** §8.5 gates (L-safe, then L-win); stretch — improves the T5 configuration

- [ ] **W4e · Walk dosing hand-off (from phase-3 Track W, moved here 2026-08-04).** The five knob-gated
  Walk arms (`TACTIX_WALK_NO_TARGET`, `_MAX_STEPS`, `_WARMUP`, rotation, effort anchor) +
  `stats.walk_minimum` as a reward proxy become the **second dosing family** in the §5.6 exhibit,
  alongside elimination dosing (rev 68). Each arm is a measured big-win-on-one-class /
  3–20×-loss-on-the-other, which is exactly the labelled signal a learner needs. **Nothing to build
  before phase 6** — the arms already exist behind knobs.

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


**▶ THIS LOG HOLDS THE CURRENT ISO WEEK ONLY (2026-W32).** Completed weeks are frozen verbatim in
`docs/log/` — index and the maintenance contract at [`docs/log/README.md`](docs/log/README.md).
W31 (41 entries: all-rules-ON, TB4b, task #11, R-d, L19–L23) · W30 (31: E2 Factor, V6, S4, Walk) ·
W29 (2) · W28 (2). **Archives are never edited**; where an archived number was later corrected, the
standing lessons and the current tier tables above are authoritative.

### 2026-08-04 late — ▶▶ THE FACTOR ORACLE: our admission gate is never the best of the three arms
Report `benchmarks/reports/2026-08-04-factor-oracle-ablation.md`. Three arms (`shipped` density gate ·
`facon` = always admit · `facoff` = `TACTIX_NO_FACTOR`) + both baselines, one submission per tier
(`20260804-1655`, git `91afdc4`). The ORACLE is the per-instance best of on/off — the ceiling on ANY
admission mechanism.

| arm | Tier B (28) | Tier C (30) |
|---|---|---|
| `facoff` never | 150.14 (22/28) | **285.90 (29/30)** |
| `shipped` gate | 86.42 (25/28) | 296.82 (29/30) |
| `facon` always | **34.59 (28/28)** | 406.55 (28/30) |
| **ORACLE** | **32.25 (28/28)** | **254.55 (29/30)** |

- **Headroom left on the table: +54.17 PAR-2 AND +3 SOLVES on Tier B, +42.27 PAR-2 on Tier C** (6.3
  noise floors). That is the size of item (b′).
- **Tier B wants Factor ON** (`facon` within 2.3 PAR-2 of the oracle, solves all 28); **Tier C wants it
  OFF** (`facoff` beats the gate by 10.9 = 1.6 floors). The gate beats the WRONG trivial policy on each
  tier by a wide margin, but **on neither tier is it the best of the three**, and it is mis-calibrated
  in OPPOSITE directions on the two.
- **Labels: Tier B helps 12 / hurts 2 / neutral 14** (6 of the 12 are TIMEOUT → solve); **Tier C helps 3
  / hurts 10 / neutral 17.** The tiers genuinely disagree about Factor. "Value falls as instances get
  harder" is the obvious candidate feature — a 58-instance tier-level correlation, NOT a per-instance
  predictor; turning such a correlation straight into a threshold is what L15 measured dead four times.
- **▶ DOES NOT LICENSE porting kissat's per-grid bound** — refuted three ways (below).
- **▶ AND A DEFECT IN THE GATE ITSELF:** `k.binary_ids` is size-monotone (deletion keeps the key,
  `kernel.hpp:472-476`) and **learned binaries enter the same map** (`analyze.cpp:441`), so the
  numerator is "binaries EVER created" against a fixed original-variable denominator. **The gate
  LOOSENS monotonically over the solve** — an instance below 6 at `conflicts==0` crosses it purely by
  learning. It is not the formula-shape test its comment describes, and it explains why `mod4block`
  (7.9 % scope at t=0, predicted inert) moved TO → 245.8 s under broad admission: a t=0 classification
  is a LOWER BOUND on reachability.

### 2026-08-04 late — the 07-30 Factor reference study VERIFIED against live source, with corrections
Three source-reading agents re-checked `docs/design/2026-07-30-factor-admission-reference-study.md`
against kissat 4.0.4 and cadical 3.0.0 at HEAD. The study's core claim **holds**: kissat has NO
formula-shape admission of any kind — its entire "when" logic is an option, the `factordelay`
`log10(active) <= eliminations + 4` warm-up, a completion-gated new-material watermark, and a tick
budget; damage is bounded per GRID by `reduction > bound` ramping 0→1→2→4→8→16 with elimination
progress (`factor.c:939`/`:72`, `eliminate.c:363-365`). Corrections:
- ~~**cadical's `factorthresh` is DEAD CODE**~~ — **THIS ENTRY WAS WRONG; RETRACTED SAME DAY.** The
  07-30 study was RIGHT: `factorthresh` is live and IS cadical's one admission-shaped condition. It is
  read via TOKEN-PASTE (`limit.hpp:150` `opts.NAME##thresh`, instantiated `factor.cpp:962`), so the
  identifier never appears literally and `grep` finds only the declaration — which is exactly how both
  this entry and L19's exhibit (a) got it wrong. See the L19 correction. **Nothing was built on the
  retracted claim**; it appeared in this Log and in commit `b0779e0`'s message, both superseded here.
- `INC (eliminations)` is the FIRST statement of `kissat_eliminate` (`eliminate.c:595-598`), so it
  counts ENTERED not completed calls; it coincides only because there is one call site. Do not conflate
  it with the genuine `complete` flag that drives the bound ladder.
- **▶ WE ARE FOUR SITES FROM THE COUPLING, AND IT STILL WOULD NOT HELP.** `bve_level_` exists and
  doubles on completed BVE doses (`default_policy.cpp:1183-1186`); `factor_bound` consumes exactly what
  kissat feeds from there; they are simply never connected — the purest form of "we own the mechanism
  but never wire it" (§5 obligation 5, shape (a)). **But porting it is REFUTED three ways:** (1) P0's
  own arm — kissat at `--eliminatebound=0`, its bound pinned to ZERO, *improved* `mod4block`
  18.35 → 6.54 s, so the bound is not what saves kissat; (2) the gain histograms STRADDLE — winners at
  mean δ 1.4 (`edp3`) and δ 13.5 (`sted5`), losers at 4.2/4.8 BETWEEN them, and bound 8 cost `sted5`
  19 → 274 s; (3) the coupling measured negative three times, latest 2026-08-04 with composition ON.
  **Recorded verdict: do not re-try without a NEW mechanism.**

### 2026-08-04 late — ▶▶ N2 DEFECT FIXED: LRAT unit ids were missed off the root level (shipped in TB4b, same day)
Found by the calculus-conformance review (25 candidates filed, 16 refuted by adversarial verifiers, 9
survived; this was the only soundness-severity one). **Certificate defect on the LRAT path only — no
verdict was ever wrong.**
- **The bug.** `propagate_service` gated the LRAT root-unit sweep on the **SOLVER's** level
  (`if (lrat && k.level == 0)`) while `emit_root_units` already applies the correct **per-literal** test
  (`a.level != 0 → continue`, `propagate.cpp:33`). A propagation-forced literal is stamped at its TRUE
  level (D-LEVEL-2: `kernel_assign_ref` takes the max level among the reason's other literals, **starting
  at 0**), so a clause whose other literals are all root-false forces a literal at level 0 **while the
  solver stands higher**. Those literals kept `unit_ids[v] == 0`, and **0 is LRAT's line terminator**, so a
  later direct citation emitted a **TRUNCATED** antecedent list — rejected by every LRAT checker.
- **Introduced by TB4b's unit chrono routing (`e404668`), TODAY.** Pre-chrono the true level always equalled
  the current level at a fixpoint; pre-TB4b a learned unit backjumped to 0 before propagating. TB4b asserts
  a learned unit at level 0 while standing higher — 14,088 of 18,635 conflicts on `veer_axi`.
- **Why nothing caught it.** DRAT re-derives units itself, so **every tier run and every DRAT leg was
  blind**; and the shipped LRAT legs were **vacuous on this path** (fuzz CNFs are ~90 vars, the routing
  needs `level-1 > chrono_levels = 100`) — already recorded as vacuous, but the non-vacuous version was
  never built.
- **▶ THE FIRST REPRODUCTION CAME BACK GREEN, and the reason is the finding's other half.**
  `kernel_reason_walk` reads `unit_ids` **only** under `reason == UNIT_REASON`; otherwise it cites the
  reason clause and recurses. **Reason-walk chains are structurally immune**, so only the direct citation
  sites are exposed — rare enough to pass 10,000 LRAT cases, and the reason the defect could exist at all.
- **Fix + permanent guard.** Drop the `k.level == 0` conjunct; **assert in `kernel_lrat_chain` that no
  antecedent id is 0** (family of `Proof::add_rup`'s no-tautology / no-empty-chain asserts, **L13**).
- **Demonstrated, not argued** — two debug binaries differing ONLY in that conjunct, both asserting, chrono
  forced *in the command* (env propagation through SLURM was unconfirmed, [[ab-arm-identical-artefacts]]):
  **unfixed 36 assert-aborts** before the max-failures cap (~2,600 cases) · **fixed 0 in 10,000**.
  Release confirmation **10,000/0 × 3** (DRAT · LRAT · `--random-policy --lrat`). ctest 38/38.
  Regression corpus **011-lrat-unit-id-missed-off-root-level** (39 vars, ddmin-shrunk).

### 2026-08-04 late — ▶▶ WHY our scheduling is cruder than Kissat's — diagnosed, and it is STRUCTURAL
Design pass `docs/design/2026-08-04-scheduling-sophistication-diagnosis.md` (frozen). User question:
*"I felt this has been a repeating theme… why is that?"* Four independent auditors; the load-bearing
claims re-verified by hand before landing.
- **The crudeness is concentrated in the layer BETWEEN rules, and that layer has never been anyone's
  unit of work.** Rule-INTERNAL fidelity is genuinely high (the rev-77 audit's own retractions prove
  it). **Coverage is opportunistic, not systematic:** every scheduling coupling we have is one that sat
  adjacent to a rule someone was porting; the gaps live in files we never had a rule-shaped reason to
  open (`tiers.c`'s consumers, `classify.c`, `eliminate.c`'s `variable_score`).
- **Five causes.** (A) the unit of work is a RULE — nothing is the unit of work for scheduling.
  (B) the rule/policy seam has no wiring owner. (C) missing kernel state makes the faithful port not
  compile, and the cheapest substitute is a constant that then reads as a tunable. (D) **the eval loop
  SELECTS for admission gates** — per-instance PAR-2 at n≈30 with no held-out set is a classifier-fitting
  instrument, and the record is **0-for-5**. (E) rev 77 recurs because its register has the wrong schema
  and its closers are prose. **Four of the five are STRUCTURAL** — "be more rigorous" fixes ~none.
- **▶ VERIFIED BY HAND (three claims, all held).** (1) `k.tier1/k.tier2` are recomputed every reduce from
  the glue distribution (`reduce.cpp:38-62`) and have **no reader outside `reduce.cpp`**, while Vivify
  tests `constexpr VIVIFY_TIER2 = 6` (`vivify.hpp:56`, used `vivify.cpp:26,151`) — kissat reads
  `solver->tier1[0]` (`vivify.c:128-133`). **We compute the dynamic tier and consume the hardcoded one.**
  (2) `factor_bound`'s only writers are the env knob and the API setter; `bve_level_` ramps
  (`default_policy.cpp:1185`) and feeds `k.bve_bound` (`:1119`) but never `factor_bound`.
  (3) **N1 IS NOT THE CAUSE — hypothesis REFUTED**: `restart.cpp:22-48`, a RULE, reads `k.h->stable`,
  `k.h->scores.score[]`, `k.h->links[].stamp` directly, and §5.3 sanctions H-state accessors. The seam is
  a **convention gap, not a prohibition**, so the fix is cheap. N1's one distorting effect runs the other
  way: purity pushed the density gate into the rule body where the policy cannot reach it.
- **Five rev-77 closers have FIRED SILENTLY**, incl. `D-VIVIFY-SCHED` whose own row admits "the condition
  was met and nothing was revisited". A CLOSED row can also hide open residuals (`D-CONG-UNIT`), and **ten
  rule headers carry deviation lists the ledger cross-references zero times.**
- **The counterweight, kept deliberately:** six mechanisms we ported FAITHFULLY, measured WORSE, and
  correctly kept the simpler version (sqrt reduce cadence, Walk's elimination anchor, mid-round C1/C2,
  Factor's coupled bound, `factorcandrounds`, minimize depth). Those are compliant and must not be "fixed".
- **Route: Track SCHED** (staged plan, below). The two items that change outcomes are (1) build the ledger by
  enumerating the REFERENCE's option set, not our own recall, and (2) make the benchmark runner print a
  per-instance ORACLE column by default, so a gate must beat both always-on and the oracle to ship —
  de-selecting cause D at the instrument, the only level at which a selection pressure can be removed.

### 2026-08-04 late — plan rev 81: the §5.6 RL stages are `RL1`–`RL4`; `L<n>` now means a lesson only
Naming only — no scope, requirement or gate change. 17 references renamed across the plan and PROGRESS.
- **Motive: a mechanical checker could not do its job.** `tools/docs/check-progress.py` validates that
  every lesson citation still resolves; with `L<n>` meaning both a standing lesson AND an RL stage it
  reported **false DANGLING errors on correct plan text**. No regex separates them — the stages appear as
  `L1 — non-stationary MAB`, `L4 removed`, `§8.5 L4 discipline`. The checker's `AMBIGUOUS` carve-out is
  now removed, so **all lesson numbers are strictly checked**, and it additionally ignores identifiers in
  backticks (a name in code font is a MENTION, prose like "(L20's knife-edge)" is a CITATION).
- **Third namespace collision found in one day**, after `R<n>` (four meanings, with its own key) and
  `Track N4` vs non-negotiable `N4`. Short shared prefixes are a recurring defect here, not three
  accidents — **prefix a new id space distinctly at birth** (`RL`, `SCHED`, `SP-`), and prefer a
  descriptive prefix over the next free letter.
- `L-safe`/`L-win` keep their names (no digit, no collision). Frozen artifacts still say `L1`–`L4` for the
  stages and are not edited.

### 2026-08-04 late — plan rev 80: **N4, the strategy floor**, is now a non-negotiable
User directive after observing this as a repeating theme. §1.1 gains a FOURTH non-negotiable:
**DefaultPolicy's SCHEDULING of the proof rules — order, effort budgets, warm-ups, watermarks,
acceptance bounds and their ramps, and the couplings between them — must be at least as sophisticated
as Kissat's default strategy, preferably more.**
- **N3 measures the OUTCOME; N4 constrains the MECHANISM.** A rule scheduled more crudely than Kissat
  schedules its counterpart is an *unmet requirement, not a tuning question*, and **may not be deferred
  to the phase-6 learner** — §5.6 exists to exceed a reference-strength strategy, never to excuse a
  weaker one.
- **Made checkable by the scheduling-parity ledger** (N4's operational test): per Kissat scheduling
  mechanism, either an at-least-as-strong analogue or a deviation-ledger row with a named closer.
  Reviewed at every phase gate; an empty or stale ledger is itself a finding. **It does not exist yet.**
- **§5 gains obligation 5**, the scheduling analogue of rev 77, naming the two shapes that have actually
  shipped: **(a) we own the mechanism but never wire it** — the more dangerous, because the mechanism's
  presence makes the gap invisible to a reader and to `grep`; **(b) we substitute an instance-level gate
  for a per-application quality bound.**
- Strengthening only: N1–N3 unchanged. R4 now points at N4. Living `N1–N3` references updated to
  `N1–N4` across the plan, CLAUDE.md, `docs/README.md`, `docs/calculus.md` and six skills (the §13
  history entries and frozen design docs keep their original wording, as records).

### 2026-08-04 late — ▶▶ BROAD FACTOR ADMISSION: HELD, and the `j3037` veto RETIRED
Item (a) closed. Report `benchmarks/reports/2026-08-04-broad-factor-admission-retest.md`; one same-window
Tier B + Tier C pair at git `6018366` (`20260804-154329-B` / `20260804-154341-C`) plus probes 1581982 /
1582186 / 1582267.
- **What the knob IS (computed from the CNFs, zero jobs).** The scope test is guarded by `!admit`
  (`factor.cpp:600`), so it is strictly additive over the density gate (`:581`). Classifying every instance
  at t=0: broad flips Factor **ON for 14 of 28 Tier B and 19 of 30 Tier C**. **The 50 % threshold is a
  LONG-CLAUSE DETECTOR, not a fine separator** — ≥95 % of clauses are size ≤5 nearly everywhere, so it
  excludes only `crypto1`/`mod4block`/`urq45`/`Grain`/`constraints_*`/`circuit_*`. Call it "Factor ON almost
  everywhere", not "broad admission". **On `j3037` it is literally Factor-OFF vs Factor-ON** (density 2.10 vs
  a gate of 6.0; no clause longer than 4 ⇒ scope fraction 100 %) — confirmed in every probe task: 0
  factorings shipped, ~1,690 broad, and `TACTIX_FACTOR_DENSITY=0` reproduces the broad arm **bit-identically**
  (161,045 conflicts both).
- **`j3037` was a LOTTERY — the hold it produced was a coin flip.** 18 paired draws over Factor-independent
  knobs, K′=11 clean. **The 08-03 pattern did not reproduce in ANY pair**: broad never timed out, shipped
  timed out once; median conflict ratio 0.82 with an IQR spanning 0.44–1.24. In the new tier window
  `j3037` is **467.7 → 18.0 s, broad 26× FASTER**. Registered as a falsifiable prediction before the run.
- **The real loss is `ContextModel`**, 10.0 s → 1200 TO, and it **REPLICATES 3/3 windows for this knob**
  (22.2× on 08-01, 54.9× on 08-03, 120.2× today, three different bases). Replication for the specific knob
  is exactly what `j3037` lacked. No proof rescue fired and no verdict mismatch anywhere in the window.
- **The ledger.** Tier B **38.39 (28/28)** vs shipped 86.51 (25/28) — **+3 solves** (`or_randxor`,
  `mod4block`, **`fclqcolor`, the last Tier B timeout**), beating cadical's 40.38. Tier C **414.70 (28/30)**
  vs shipped 309.05 (29/30). **T3 decides it: broad moves the binding kissat half 1.346 → 1.500×** and the
  cadical half 1.069 → 1.191×. HOLD.
- **But the Tier C cost is TWO INSTANCES** — `ContextModel` +2,390 s and `mp1-Nb5T15` +801 s of a +3,170 s
  delta (7 instances >10 % worse, 4 better, 19 flat). Quarantine both and broad is **+8.89 PAR-2**. Routed
  to (b′): the blanket knob is the wrong instrument, not the wrong idea.
- **Method (see report §8).** Two of the four decisive facts cost **zero jobs**. The instances a knob
  provably cannot touch are a **free in-window noise estimator** — measured 0.84–1.14 across nine of them,
  which is also what showed `j3037`'s 08-03 "loss" (1.10×) sitting inside the same band. Pre-registration
  paid twice: it forced a second probe batch when K′ came in at 5 against a floor of 6, and it forbade
  quarantining `ContextModel` by analogy — which would have shipped a change that costs the gate.

### 2026-08-04 late — TB4b's confirming Tier C, and `frb45` joins the may-not-gate set
`20260804-141431-C-tb4b` (array 1581854), collected.
- **tb4b 304.91 (29/30) vs pre 341.45 (29/30) — −36.54 PAR-2, no solve change.** But **EXCLUDING the fragile
  three it is +27.56**, i.e. the entire win is `jgiraldez` 801→4.5 s, `urq45` 635→127 s, `j3037` 978→442 s —
  precisely the instances TB4b targeted.
- **`frb45-21-2` is ~2/3 of that cost** (90.2 → 588.6 s, 6.53×) and probe 1582186 measures it **CHAOTIC**:
  bit-identical under replication AND under `chrono` 90/105/115 (7,856,563 conflicts every time), but
  **6.2×** from `walk600` (105.1 s) to `bumplim9` (652.3 s). Excluding it too, TB4b costs **+9.45 PAR-2**,
  ~1.4 noise floors.
- **Open, filed as (c′):** T3's binding kissat half moved **1.271× (08-03, pre-TB4b) → 1.346× (today)**.
  Not a revert order; a real number attached to an unclosed question.

### 2026-08-04 — ▶▶ `D-LRAT-FALLBACK` SHIPPED: the runner no longer discards correct solves (`3c3de3b`)
Plan §8.1 has always required it; `run.py` never did it. On a DRAT check that outruns its budget the runner
now re-**solves** with `--lrat` (a conversion is impossible — DRAT→LRAT needs the same backward search that
just timed out) and verifies with cake_lpr.

| stage | `tierC/urq45.cnf.xz` |
|---|---|
| solve | 591.0 s → 919 MB DRAT |
| drat-trim | **exhausted its budget** |
| `--lrat` re-solve | 618.8 s → 3,289 MB LRAT |
| **cake_lpr** | **119.0 s — VERIFIED** |
| record | `solved: true`, `proof_rescue: "lrat"` |

- **cake_lpr 119.0 s against drat-trim's 4,491.98 s on the same instance = 37.7×.**
- **▶▶ AND THE REAL NUMBER TO WORRY ABOUT: `urq45` verified in 4,491.98 s of a 4,800 s budget — 93.6 %.**
  It scraped through today's gate; a busier node flips it to a false unsolved. That is the argument for this
  feature, and it says the instance has been ~6 % from the cliff on every window we have quoted.
- **Trigger is exactly one condition** — `check_proof`'s timeout message, now the single-sourced constant
  `CHECKER_TIMEOUT_PREFIX` because it is a control signal. A genuine REJECTION still scores unsolved (N2);
  a verdict DISAGREEMENT on the re-solve is never rescued and is recorded as such; baselines are never
  re-solved.
- **Verified three ways**: the forced-rescue probe with 8 pre-registered assertions including
  `proof_rescue == "lrat"` and "the check TIMED OUT, did not reject" (all PASS — a non-firing rescue looks
  exactly like a working one, so this was asserted, not eyeballed); a rejected proof still scoring unsolved
  via `tools/fuzz/broken/truncate-proof.sh`; and ctest 38/38.
- **⚠ COST: the per-task SLURM wall limit doubled** to `2*(timeout + checker_budget) + 120` — Tier C tasks go
  **1:42 → 3:22** — because the old limit could not fit the runner's own worst case and SLURM would SIGKILL
  mid-rescue, returning MISSING. **This trade is worth revisiting**: it reserves 2× the wall for a path that
  fires rarely.
- Drive-by fix: local `--jobs` mode unpacked 4 of a 5-tuple and would have raised `ValueError` — collateral
  from the rev-79 `verify_proofs` split, never exercised because every run goes through SLURM.
- **NOT done:** LRAT is 3.6× the DRAT size here (3.3 GB); on a larger instance cake_lpr's RSS (4.02 GiB
  here) could approach the 8 GB cap. The rescue reuses `checker_budget` rather than a separate knob — look
  again if Tier D ever rescues.


### 2026-08-04 — ▶▶ TB4b UNIT CHRONO ROUTING SHIPPED: `veer_axi` 6×, −8.78 PAR-2 on the 27 non-lottery instances
`8118c68`. Two changes, different evidence — see the commit for the split.
- **Unit chrono routing (the measured win).** A learned unit was forced to `target = 0`, discarding the whole
  descent; kissat routes it through the same chrono determination (`learn.c learn_unit` →
  `determine_new_level(solver, 0)`) and steps back ONE level. Sound because `kernel_assign_unit` stamps at
  level 0 explicitly (true-level assignment, D-LEVEL-2) wherever the solver stands.
  **Motive:** on `veer_axi` **14,088 of 18,635 conflicts learn a unit**, and the caller COUNTED 14,108
  chronological backtracks while `backjump` overrode 14,088 of them — so we backtracked chronologically on
  **20 conflicts (0.1 %)** where kissat does on **2,821 of 2,967 (95 %)**. The `c chronological` stat was
  reporting behaviour we did not perform.
  **Result: `veer_axi` 195.5 → 31.9 s** (second window; 202.6 → 36.0 s in the first), 65× kissat → ~9×.
- **The after-conflict snapshot order** (`kissat_backtrack_after_conflict`: step down one level, THEN
  snapshot target/best, THEN go to target). Ours snapshotted at the full trail, inflating the
  `target_assigned`/`best_assigned` high-water mark by the conflict level's own literals. `kernel_backtrack`
  is now the analogue of `..._without_updating_phases`; the four call sites snapshot explicitly. **Inert on
  both decisive instances**, but the combined gate is better than unit-routing alone (−8.78 vs −5.30), the
  first evidence it helps.
- **Gates, priced BOTH ways** (`20260804-141429-B`): all 28 → **85.30 (25/28)** vs pre 72.82 (26/28);
  **excluding `mod4block` → 66.23 vs 75.01 = −8.78 PAR-2 at identical solves on 27 of 28.** The entire
  penalty is one lottery instance (see the entry below). Tier C `20260804-141431` was still running at
  session end.
- Fuzz 10,000/0 × 4 legs (DRAT + LRAT × ± forced). **The default unit-routing legs are VACUOUS by
  construction** — the path needs `level-1 > 100` and fuzz CNFs are ~90 vars — hence a `chrono_levels=0` leg;
  the snapshot legs ARE non-vacuous (verdicts 5715/280/4005 vs 5713/285/4002). ctest 38/38.
- Three test assertions changed: `test_switchmode`/`test_rephase` called `kernel_backtrack` directly and
  asserted it snapshots phases — they encoded the coupling, not an invariant.


### 2026-08-04 — ▶▶ `mod4block` IS A LOTTERY FOR BOTH SOLVERS — no per-instance verdict on it may gate a change
Two of tonight's HOLD decisions were decided by per-instance results on this instance. Both were decided by
noise. **kissat's own range on `mod4block` is 4.36 s → TIMEOUT, over 73×:**

| kissat arm | conflicts | wall |
|---|---|---|
| `--chrono=0` | 175,401 | **4.36 s** |
| base (default) | 715,338 | 19.32 s |
| `--chronolevels=60` | 1,365,970 | 35.83 s |
| `--chronolevels=160` | 6,987,130 | **247.50 s** |
| `--restartint=8` | 9,081,119 | **TO** |

- **`chronolevels` alone swings KISSAT 12.8×** (60 → 160). Ours swings 7.6× (100 → 120) on a knob that
  changes at most **4 backtracks** — chrono fires 4 times there and it learns 7 units.
- **kissat is NOT efficient on `mod4block` — it is PAYING.** Its default is **4.4× worse than its own
  chrono-off** (19.32 vs 4.36 s). And it needs **715,338 conflicts** where our baseline needs **418,796**, so
  **our 13.6 s was a lucky draw, never an engineering advantage.** L10's "no reference solver defends; the
  density gate is ours alone" rests on that same draw and should be read accordingly.
- **▶▶ STANDING RULE: `mod4block` may not gate a change on its per-instance result.** Judge changes on the
  27-instance remainder, or on a replicated distribution, never on this instance's single draw. It has now
  vetoed **broad Factor admission** and **unit chrono routing** in one night; the second veto was overturned
  when priced without it (+15.83 PAR-2 with it, **−5.30 without**, identical solves on 27 of 28).
- **The same suspicion attaches to `j3037`**, which is what actually held broad Factor admission on Tier C —
  it is one of the three measured perturbation-FRAGILE instances, and its three arms spanned
  218.5 s → 1092.2 s → TO in a single window. **Broad Factor admission deserves re-evaluation on the same
  basis before its hold is treated as settled.**
- **Method:** the discriminator is cheap and should be run BEFORE any per-instance verdict gates anything —
  perturb the BASELINE with knobs that cannot matter mechanically (here `chrono_levels` ±1, ±20 against a
  rule that fires 4 times) and see whether the instance moves. It took four jobs.


### 2026-08-04 — ▶▶ THE FRAGILE THREE ARE A CHRONO DEFECT (3 of 3); `veer_axi` is NOT, and stays unexplained
Two findings, deliberately kept apart because the first tempted an over-broad conclusion the second refutes.

**(1) The "fragile three" own 54 % of the Tier C gap to KISSAT, and chrono is implicated on all three.**
Gap decomposition (window `20260804-080524`, ours 10,409 s vs kissat 6,781 s = 3,629 s): `j3037` **848 s
(6.26×)** · `jgiraldezlevy` **639 s (6.79×)** · `urq45` **473 s (5.04×)** — **top 3 = 54 %, top 8 = 97 %**;
we WIN on 11 of 30 worth −473 s.
- **Same-magnitude perturbation, both solvers — chrono OFF:**

  | instance | ours | kissat |
  |---|---|---|
  | `j3037` | 332.7 → **42.6 s (7.8× FASTER)** | 146.3 → 116.2 (1.26×) |
  | `jgiraldezlevy` | 202.4 → 293.5 (1.45× slower) | 110.5 → **45.4 (2.4× faster)** |
  | `urq45` | 205.7 → **74.6 s (2.8× FASTER)** | 111.1 → 111.4 (**inert**) |

- **These instances are NOT intrinsically unmeasurable.** Both baselines are stable run-to-run there —
  kissat bands **4.1 / 3.5 / 3.1 s (2.6–3.1 %)**, cadical 2.1 / 0.7 / 19.8 — *lower* than kissat's own band
  on `goldcrest` (8.3 %) or `minxor128` (6.4 %). At comparable perturbation magnitude kissat ranges
  **1.23–2.43×** where we range **8.4–23×**.
- **Method guards.** `--walkinitially=1` was **vacuous** — byte-identical conflicts to base on all three
  (1,910,509 / 2,481,889 / 10,255,109); caught only by capturing conflicts, not wall time. `--bump=0` is
  **not** a comparable perturbation (it times kissat out on two of three, 22.9 M and 75.6 M conflicts) and is
  excluded rather than quoted as "kissat swings 13×".
- **Routes to TB4b**, the one open Tier B track ("watch repair onto highest-level literals, unit chrono
  routing, snapshot timing, `jumpreasons`, cadical's `chronoreusetrail`") — filed as cleanup, actually the
  largest identified item on T3's BINDING half. **Scope it to three instances, not four.**

**(2) `veer_axi` is NOT a chrono defect — hypothesis tested and REFUTED.** Paired arms, one submission:

| | base | chrono off |
|---|---|---|
| decisions | 221,100,578 | 192,469,369 (0.87×) |
| conflicts | 18,635 | 20,454 (**1.10× WORSE**) |
| propagations | 1,554,104,528 | 1,216,272,653 |
| solve | 152.3 s | 137.5 s (**0.90×**) |

10 %, not a collapse. **So `veer_axi` does not join the fragile three and TB4b does not consolidate it.**
- **What R-d DID fix, measured at HEAD:** decisions on substituted variables **84.03 % → 0.24 %**, decisions
  1.61 G → 221 M. The original defect is closed.
- **What remains, unexplained:** **507× kissat's decisions and 262× its propagations for 6.3× the
  conflicts** (221 M / 1.554 G / 18,635 vs 436 K / 5.94 M / 2,967), at **7.03 props per decision** — so these
  are real, propagating descents that almost never conflict, not dead variables. Search is 90 % of the clock.
  **Ruled out:** substituted-variable decisions, restart cadence, **trail reuse** (2,324 restarts, 1,980
  reused a prefix — our `restart.cpp` is a faithful transliteration of kissat's stable/focused
  `reuse_trail`), rule cost (9.7 %), and chrono.

**▶ MEASUREMENT HYGIENE — `veer_axi` moves 32 % on WALL CLOCK at a BIT-IDENTICAL trajectory.** Two runs of
the same binary on the same input gave **201,607 ms** and **152,283 ms** with identical decisions, conflicts
AND propagations; the slow one shared the partition with a tier array. **A single probe on this instance
cannot resolve anything under ~1.3×** — pair the arms in one submission, and read wall-clock deltas against
the trajectory counters, which are deterministic.


### 2026-08-04 — ▶▶ V6 COST: HALF THE CLOCK GONE, byte-identical proofs (zero trajectory change) — `ee9f3ab`
The trajectory-PRESERVING half of the V6 cost item, landed by a sub-agent under the byte-identical-DRAT
oracle. **This is engineering only: same conflicts, same merges, same proof — less wall clock.**

| instance | V6 before | V6 after | Δ | solve |
|---|---|---|---|---|
| `T105.2.0` | 193,772 ms (37.0 %) | **97,800 ms (22.6 %)** | **−49.5 %** | 524.4 → 433.2 s |
| `T87.2.0` | 248,725 ms (33.6 %) | **115,886 ms (20.8 %)** | **−53.4 %** | 740.1 → 555.8 s |
| `hwmcc17miters-…oski15a08b00s` | 5,627 ms (89.0 %) | 2,875 ms | −48.9 % | 8.34 → 5.45 s |
| `FmlaEquivChain_4_6_6` | 3,030 ms | 1,165 ms | −61.6 % | 33.06 → 29.67 s |

Reproduced on an independent second Tier C sample (jobs 1581275–78).

- **PROOF IDENTITY HELD ON ALL FOUR — 297,468,966 B of byte-identical DRAT**, matching md5s, and identical
  `c conflicts` / `c conggates` (per kind) / `c congruence` (rounds, gates, merges, units, unjustified).
  That is what licenses landing it without a tier gate.
- **The biggest lever was not the sort — the clause index's mapped clause id was NEVER READ.** XOR and ITE
  only ask "does this clause exist", so an `unordered_map<SmallKey,uint64_t>` was a *set*. It is now a flat
  open-addressed `SmallKeySet` with keys stored in the bucket array: one cache line per probe instead of a
  bucket read plus a node chase, one contiguous allocation instead of one malloc per indexed clause. XOR
  probes it **2^(s−1)** times and ITE 6 times **per indexed clause per apply**.
- Also: `sorted_keys` computed **once** per apply instead of twice (the second introsort recomputed a
  bit-identical answer over data the code documents as immutable), keys materialised by value into a
  contiguous vector, exact `reserve()` on `CongGate`'s two vectors, LRAT `pairs` and XOR `reps` buffers
  hoisted to caller-owned scratch, and `find` demoted from `std::function` to a template parameter so the
  union-find lookup in the hottest loop inlines.
- **Verified at HEAD after merge: `11 rounds, 53,371,397 gates, 3,713 merges, 14 units, 0 unjustified` on
  T105** — the alarm counter for D-CONG-SKIP/D-CONG-TAUT is **0**, unchanged.
- **Side effect worth knowing: composition (`675a953`) cut congruence's own volume ~15 %** — pre-composition
  T105 is `13 rounds / 63,076,649 gates`, HEAD is `11 / 53,371,397`. Factor reshaping the formula gives V6
  less to chew on, compounding with the engineering win.
- **NOT done, deliberately** (all recorded by the agent): `CongGate`'s two vectors → arena, blocked only by
  a `tests/test_congruence.cpp:246` comparison; ITE's node-based `ce` map (ITE yield is ~dead); the
  closure's `occ` re-registering duplicates on every re-pop; the AND-extraction `tags` sort (~6 s of the
  remaining 98 s). **Re-profile before picking any of these up — the old profile's proportions no longer
  describe the remaining time.**
- **Process note banked:** editing a probe script *while jobs are executing it* truncates the running
  instance — bash reads scripts incrementally. Cost one Tier C round's md5 lines. A warning now sits at the
  top of `benchmarks/probes/cong-proofid-2026-08-04.sh`.


### 2026-08-04 — ▶▶ COMPOSITION SHIPPED (neutral, removes the density gate); the mod4block trade is BROAD's, not composition's
Gate windows `20260804-080512-B-comp` / `20260804-080524-C-comp`, git `675a953`, hatch
`TACTIX_COMPOSITE_DENSITY=50` restores the old gate as the `nocomp` arm.

| arm | Tier B | Tier C |
|---|---|---|
| **comp** (unconditional — SHIPPED) | 75.75 (26/28) | **346.98** (29/30) |
| nocomp (old ≥ 50 binaries/var gate) | 74.34 (26/28) | 349.63 (29/30) |
| compbroad (+ `FACTOR_SCOPE_PCT=50`) | **55.16 (27/28)** | — |
| kissat / cadical | 10.05 (28/28) | 226.02 / 309.69 |

- **Composition alone is NEUTRAL: +1.41 B, −2.65 C, both inside the 6.72 clean noise floor, no solve changes
  on either tier.** Shipped on that basis — it removes a hand-designed formula-shape separator (plan rev 77;
  the L15/L19 family) at no measured cost, and closes a real capability gap: applied factorings on
  `fclqcolor` go **78 → 164**, where 78 was ~kissat's BINARY-restricted 80.
- **▶ THE `mod4block` TRADE BELONGS TO BROAD ADMISSION, NOT COMPOSITION.** `mod4block` is **14.0 s with
  composition vs 13.8 s without**; only the `compbroad` arm loses it (300 TO). And `fclqcolor` needs BOTH —
  composition alone leaves it a timeout. So `compbroad` reaches 27/28 by **trading `mod4block` for
  `fclqcolor`**, at a 284.7 s/300 s cliff. Broad admission stays HELD.
- **▶ THERE IS NO REFERENCE MECHANISM TO PORT FOR `mod4block`, and L10 already said so:** *"no reference
  solver defends; the density gate is ours alone"* — kissat pays **4.14×** there with its own scheduling
  (18.9 s against our 13.3 s). The gate was not a crude approximation of something kissat has; it was an
  advantage kissat lacks. Removing it is safe only because composition alone does not disturb `mod4block`.
- **▶ kissat's fourth governor (d) is INERT here and was NOT shipped.** Its acceptance test is
  `reduction > bounds.eliminate.additional_clauses` (`factor.c:72/939`); our analogue is `bve_level_`
  (both start at 0, both double on a completed elimination). Probes 1581094–97: on `mod4block` the coupled
  and uncoupled arms are **byte-identical — 239 factorings, 589,034 offers, both TO** — because `bve_level_`
  is **0** when Factor runs there, so the bound never binds. On `fclqcolor` it is mildly negative
  (157 factorings vs 164, still UNSAT). The identical coupling was already measured dead 2026-07-25
  (design §3.e, mp1 5.3 → 14.9 s); re-measuring WITH composition on was justified — the old negative was
  taken with composition gated off — but the answer is the same. **Reverted; do not re-try without a new
  mechanism.**
- **What remains for `fclqcolor`:** it needs composition AND broad admission, and broad is held on its own
  Tier C solve loss. The open question is now narrow and is a §5.6 admission question — **WHEN to widen
  Factor's scope** — alongside BVE dosing and BCE, not a static rule.


### 2026-08-04 — ▶▶ `fclqcolor` SOLVED in a probe: the COMPOSITION density gate was the whole blocker
Probes 1580855/1580856. **The last Tier B timeout falls.**

| arm | factorings | conflicts | result |
|---|---|---|---|
| broad admission, no composition | 78 | 11.46 M | **TO @ 300 s** |
| **broad + composition** (`TACTIX_COMPOSITE_DENSITY=1`) | **164** | **5.80 M** | **`s UNSATISFIABLE`** |

- **Mechanism.** Composition (`Kernel::factor_composite`) widens the factor/remainder universe over the
  extension variables Factor itself creates, so later chains in the same pass compose over them — kissat's
  `resize_factoring`, which it calls from `apply_factoring` (`factor.c:838`) **with no gate at all**. We
  gated it at **≥ 50 binaries/var**; `fclqcolor` sits at **4.76**, so we never composed. That is exactly why
  applied factorings saturated at **78 across `factor_size` 3/5/7/9** while offers rose 42 k → 51 k, and why
  78 ≈ kissat's **binary-restricted 80** rather than its default 120. With composition we reach **164**.
- **The gate was the hand-designed-separator family** plan rev 77 forbids and L15/L19 measured **0-for-5**.
  Removed rather than re-tuned (user directive 2026-08-04).
- **▶ WHAT BOUNDS COMPOSITION INSTEAD — kissat's real governors, and we already have three of four.**
  (a) the `factor_ticks` effort budget, checked in kissat's main loop (`if (*ticks > limit) break`,
  `factor.c:914`) and here as the EffortSpec 50 ‰ handoff; (b) a **score-ordered candidate heap**, so the
  budget is spent best-first; (c) one attempt per literal per pass. **(d) is the one we lack: kissat accepts
  a grid only when `reduction > factoring.bound`, where `bound = solver->bounds.eliminate.additional_clauses`
  (`factor.c:72/939`) — the acceptance threshold is COUPLED to BVE's growth allowance, so factoring gets
  pickier exactly as elimination gets more permissive.** Ours (`Kernel::factor_bound`) is a flat **0** =
  "any positive gain".
- **▶ IF THE TIER GATE REGRESSES, PORT (d) — DO NOT REINSTATE A DENSITY GATE.** Known risk: composite ON took
  `mp1` 15.5 → 5.4 s and broke `battleship` 1.3 → 197 s. A version of the bound coupling was measured dead
  on 2026-07-25 (design §3.e — it rejected the small-delta top-up grids the dosed regime exists to add, mp1
  5.3 → 14.9 s), **but that was measured with composition GATED OFF, so the negative does not transfer.**
  `TACTIX_COMPOSITE_DENSITY=<n>` is retained as an A/B hatch that restores the old gate at threshold n.
- Gates running: Tier B `20260804-080512` (arms comp / nocomp / comp+broad / kissat), Tier C
  `20260804-080524`. Fuzz legs `1580858` (DRAT) and `1580859` (LRAT + `--random-policy`) on the sanitizer
  build.


### 2026-08-04 — ▶ Tier C NOISE FLOOR re-measured on an IDLE partition: **6.72 PAR-2**, not 10.8
Window `20260804-030938-C-nf1`, three **bit-identical** arms of `build/pin-sbgate/tactix`, nothing else on
the partition. **nf1 364.50 · nf2 362.17 · nf3 357.78, all 29/30 → aggregate spread 6.72 PAR-2.**
Per-instance range: **median 4.8 s, mean 11.9 s**; worst `jgiraldezlevy` 69.2 s, `j3037` 52.7 s,
`T87` 49.1 s.
- **The standing 10.8 was CONTENTION-INFLATED, as suspected** — it came from `20260731-013523`, which
  overlapped another full Tier C array for ~1 h 12 m. The clean floor is **62 %** of it. **Use 6.72 for a
  quiet partition; 10.8 remains the right bar for a run that shared one.**
- **What it changes.** T3's CaDiCaL half misses by **5.44 PAR-2** (350.56 against a 345.12 band) — **still
  INSIDE the clean floor**, so that half stays not-robustly-missed. The mid-round budget's Tier C cost
  (**+13.32**) is now **clearly ABOVE** the floor, strengthening its negative verdict.
- **Quote the per-instance number for a single instance:** a median of 4.8 s means most Tier C instances
  reproduce to a few seconds, and the aggregate floor is carried by a handful of large swingers — the same
  fragile set (`jgiraldezlevy`, `j3037`) that dominates every other Tier C delta.


### 2026-08-04 — ▶ SUBSET RE-DERIVED with replicates: T3's kissat half is ROBUST to the list's composition
Window `20260804-004328-C-k_a`, 12 arms (kissat/cadical × ±sweep × 3 repeats) × 30 instances.
Criterion: exclude iff the sweep delta exceeds that instance's OWN replicate band AND the relative move
exceeds 25 % — the test single-shot data could not support. Analysis `tools/bench/subset_rederive.py`.

- **▶▶ THE DECISION-RELEVANT RESULT — the gate verdict does not depend on this list.** Scoring the
  `20260803-211105` window over the corrected **15**-instance subset gives **shipped/kissat 1.269×** against
  the shipped 24-instance list's **1.271×**. **T3's binding half is robust to the subset's composition.** The
  cadical half does move (0.987× → 1.181×) because the corrected list drops instances where we beat cadical
  heavily (`StConn` 0.41×, `EDP3` 0.11×, `circuit_32in32out`) — but that half is not the constraint.
- **The shipped list was UNDER-excluding, not over-excluding.** The replicated criterion excludes **15 of
  30** where the single-shot one excluded 6: nine instances are genuinely sweep-sensitive and were being
  kept, e.g. `em_11` (kissat 2.77×, cadical 2.53×, bands 27/32), `DLTM` (kissat 4.10×, band 56), `StConn`
  (cadical 0.41×, d=660 ≫ band=66).
- **▶ I HAD THE FRAGILITY ARGUMENT BACKWARDS, and the replicates say so.** The list's header warns that four
  exclusions are "chaos-suspect" because `ContextModel`/`j3037`/`jgiraldezlevy`/`urq45` swing 8–23× under
  unrelated knobs. **That fragility is OURS, measured on tactix arms (probe 1579386); the criterion is about
  the BASELINES.** kissat's own replicate band on `j3037` is **39 s** against a sweep delta of **203 s**, so
  its exclusion is well-founded — a baseline can be perfectly stable where we are chaotic. Do not carry a
  fragility measurement across solvers.
- **Not replacing the frozen list tonight.** The verdict is unchanged either way, the new list halves the
  instrument (15 vs 24 instances, so a noisier aggregate), and a gate artefact changes by review, not at
  03:00. Evidence and tooling are committed; the swap is a one-line decision when someone wants it.
- **Harness defect found and worked around:** `--no-check` applies only to the solver-under-test arm, while
  `--checked-arm` hardcodes check AND score — so `k_a` ran proof-free while `k_b`/`k_c` were checked and
  scored, and several arms lost solves to checker-budget timeouts. Analysis therefore uses **verdict-based**
  solve times, not the scored flag, which is correct for a baseline ablation anyway (§8.1: a baseline's
  failed check is never scored against it). Verified harmless: an unchecked vs checked arm of the SAME
  config differ by a median **1.9 s**, against **2.8 s** between two checked arms.

### 2026-08-04 — ▶▶ D-STRENGTHEN-BUDGET: interruptible C1/C2 is MEASURED NEGATIVE, default REVERTED
Same-window A/B via the `TACTIX_NO_MIDROUND_BUDGET` hatch. **Tier B 72.22 vs 70.51 · Tier C 366.58 vs
353.26**, identical solves (26/28, 29/30), both T3 halves worse — and **inert on its own sentinel**
(`DivS_862_11` 102.4 s on vs 103.2 s off, where C2 owns 89.7 % of the solve). **Mechanism:** at 100 per mille
the budget binds ~once per solve and a stopped round just DEFERS its candidates to the next round — the work
is rescheduled, not saved. **The DOSE axis is live where interruptibility is dead** (1 per mille → DivS
11.5 s), so the mechanism stays behind `TACTIX_MIDROUND_BUDGET=1` as a §5.6 arm and the ledger row is
re-scoped to "**dose** C1/C2". Kept: the `c strengthen` stats line (it had none) and the `c budgetstop`
reachability witness. **Two method traps, both caught:** the motivating probe's 80.1 s "win" was
cross-window and measured **−0.8 %** once both arms ran in one window; and the fuzz leg built to make
coverage non-vacuous **was itself vacuous** (verdicts 5715/270/4015 vs the control's 5715/271/4014 — on tiny
instances the budget computes to 0 and the guard short-circuits), so the real coverage is the two tier
gates. Full detail: `benchmarks/reports/2026-08-04-midround-budget-negative.md`.

### 2026-08-03 — the day's results, compressed (full detail in the cited reports/commits)
Compressed 2026-08-04 under compact-docs: each of these has a frozen report or commit carrying the
detail, so the Log keeps result + pointer only.
- **V6 Congruence: gate matching is AT PARITY; the gap is COST (7.6×) and the UNIT channel.** Per kind on
  T105 AND 3,423 vs kissat 4,348, XOR 268 vs 531, ITE 0 vs 18 — kissat's headline `congruent` BUNDLES a
  binary-equivalence scan that is our V3 (L14's scope rule). The one real outcome gap is
  **`congruent_units` 2,231 vs our 14** (`find_units` is only 844 of it; the rest is real).
  **Mechanism: kissat's closure is VALUE-DRIVEN** — it propagates each learned unit and re-simplifies
  gates against the new values, and **retires value-resolved gates**; our `canonicalize`
  (`congruence.cpp:497-586`) contains **zero `k.value` reads** and never drops a resolved gate, which is
  the suspect for the 2.89× volume factor. Five of kissat's six unit-deriving conditions have no
  analogue. `congruence_unjustified` is 0, so nothing is being deliberately skipped. Route: rule-dev,
  staged (Stage 1 value-aware `canonicalize` + an outer fixpoint; gating condition
  `congruence_unjustified` stays 0). **Now folded into Track SCHED · SCHED-G1.**
- **V6 COST PROFILED (job 1580096, `perf record` over a full T105 solve):** `congruence_extract_gates`
  **6.13 %**, the `SmallKey` machinery supporting it **~13.6 %** (8.2 % lookups + **5.4 % `sorted_keys`
  and its introsort**), allocator ~13.6 % spread. **The closure's `unordered_map<vector<Lit>,uint32_t>`
  is 1.2 % — my structural hypothesis was REFUTED; do not swap it.** Attack EXTRACTION and the index sort.
- **`fclqcolor` LOCATED:** Factor OFFERS the widened candidates and ACCEPTANCE saturates at 78 factorings
  across `factor_size` 3/5/7/9 while offers rise 42 k → 51 k. The 27×27 grid cap (S2a) is first to price.
- **plan rev 79:** baselines WRITE proofs but are no longer VERIFIED (baseline checking was 6.07 h of
  9.08 h on one Tier C window); Tier D frozen at **793**, and it overlaps Tiers B/C so it is not
  independent confirmation.
- **T3's INSTRUMENT restored:** `benchmarks/tiers/C-ruleneutral.list` (24 of 30) +
  `tools/bench/subset_score.py`. **Standing rule: every Tier C run carries `--baseline kissat --baseline
  cadical`**, or T3's kissat half goes unmeasurable again.

