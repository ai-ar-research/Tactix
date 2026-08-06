# N4, measured behaviourally: how our schedule ACTS against kissat's, per technique

**Date** 2026-08-06 · **branch** `audit-n4-shape` · **HEAD** `4f4989d` (no source change —
this pass measures, it does not fix) · **tactix binary** `build/pin-n4shape/tactix`
sha `7ec995d2852cabec` · **reference** kissat 4.0.4, pinned clone (`kissat/src` unmodified
since 2026-07-23) plus the `-DMETRICS` variant at
`tools/solvers/variants/kissat-metrics/kissat` (`gcc 13.1.0 -W -Wall -O3 -DNDEBUG -DMETRICS`).
**Jobs** `1587305` (30 tasks, part 1) · `1587348` (8 tasks) + `1587381` (3 tasks, part 2).
**Probe scripts** `benchmarks/probes/n4-shape.{sh,mf,sbatch}`,
`n4-shape-forced.{sh,mf,sbatch}`, `n4-shape-table.py`. Raw output
`benchmarks/probes/scratch/n4-shape/` and `…/n4-forced/` (regenerable; `TABLE.md` there is
the generated data appendix).

## 0. Why this pass exists, and what it is NOT

Track SCHED closed eleven of twelve HIGH parity rows. That establishes we **own** the
mechanisms. It does not establish that our schedule **behaves** as well, and 2026-08-06
showed the difference is not academic: four separate governors were found not to bind at
all while their ledger rows read "closed".

So this pass measures **behaviour on the same formulas**: occasions per technique per solve,
realized interval shape, which signals each governor responds to, and yield per occasion.
It is **counters only** — no wall-clock figure is taken anywhere, which is why it could run
alongside six other agents' arrays without qualification. Nothing here is a PAR-2 claim and
nothing here should be quoted as one.

**▶ The headline, stated plainly up front: N4 is NOT met behaviourally.** Two whole rule
families do not run at all on most of this set. Counting only the instances where kissat's
counterpart yields anything (9 of the 10 — `hwmcc17miters` is refuted before either solver
eliminates anything), **BVE eliminates 0 variables on 7 of 9** where kissat eliminates 104 to
2,736,829, and **Factor adds 0 extension variables on 6 of 9** where kissat adds 98 to 32,157.
In both cases the cause is measured, is ours, and is a **one-line change**. That is not "a
rule we schedule more crudely"; it is a rule that our own schedule switches off. Detail in
§4.1–§4.3.

## 1. The instance set, and why it is defensible

Ten instances, chosen to span formula SHAPES rather than to sample a tier, because a shape
is what a governor keys on. Every one is already in a frozen tier list, so nothing was
hand-picked for outcome, and eight of the ten are instances the SCHED ledger already cites —
which is what lets a row here be compared against an earlier reading rather than replacing it.

| instance | tier | shape | why it is in the set |
|---|---|---|---|
| `9vliw_bp_mc.shuffled` | B | VLIW-processor BMC miter, 179 k clauses, 88.8 % binary | the binary-dense structured case; the ledger's standing probe instance |
| `b22_1` | B | BMC circuit, second structured family | independent structured draw, so §4 is not one family |
| `T105.2.0` | C | very large structured miter (~3.1 M vars) | the Congruence cost-gap exhibit (`SP-CONG-BUDGET-1`) |
| `hwmcc17miters-…-oski15a08b00s` | B | equivalence-checking miter, refuted in 1 conflict | the family V6 Congruence exists for — the preprocessing-only case |
| `combined-crypto1-…-cryptocplx-40` | B | crypto + random-WFF mix, SAT | crypto shape, SAT side |
| `aes_decry_2_rounds.debugged` | C | AES 2-round decryption, UNSAT | the second crypto shape, and large (279 k vars) |
| `clique_n2_k10.sanitized` | B | combinatorial (clique), UNSAT | pure combinatorial, no circuit structure |
| `or_randxor_k3_n540_m540` | B | random 3-XOR conjunction, UNSAT | the XOR shape — the only one where an XOR gate extractor can be judged |
| `sted5_0x24204-50` | B | SAT, the SP-SLOT-5 elimination-clock exhibit | keeps this pass comparable to `SCHED-D3` |
| `battleship-14-26-sat` | B | combinatorial SAT, the dynamic-tier exhibit (`SP-TIER-2`) | the instance where a static tier admits nothing |

4 SAT / 6 UNSAT, 10 distinct families, both tiers, sizes from 1-conflict to 3.1 M variables.
Both solvers ran all ten under one 900 s SIGTERM cap (SIGTERM, not SIGKILL, so both still
print statistics on a timeout). Nine of ten were solved by both. **`or_randxor` is our only
non-solve** (UNKNOWN at 16,404,286 conflicts against kissat's UNSAT at 616,974) — reported,
and §4.6 says why it is in the set anyway.

### 1.1 The three honest caveats, before any number is read

1. **The metrics build is behaviourally identical on 9 of 10 — not 10 of 10.** The `kpin`
   arm exists solely to check that (kissat's METRIC tier is compiled out of the shipped
   `-O3 -DNDEBUG` build, so the per-technique counters are only visible in a `-DMETRICS`
   build, and a build that changed the trajectory would describe a different solver).
   Result: identical verdict AND identical conflict count on nine instances. **`T105.2.0`
   diverges — 605,325 conflicts with `-DMETRICS` against 393,595 without.** Cause not
   identified; every budget-relevant counter kissat reads (`search_ticks`, `probing_ticks`,
   `kitten_ticks`, `factor_ticks`, `transitive_ticks`, `backbone_ticks`, `sweep_completed`,
   `literals_factor`, `eliminate_resolutions`) is COUNTER tier and present in both builds, so
   the obvious explanation is ruled out. **T105's kissat column is therefore used
   qualitatively only** and no ratio from it is banked. Next step if it matters: bisect
   `-DMETRICS` on that instance.
2. **`c slot … ok N` is not a productivity counter.** It is incremented only in
   `run_batch_entry`, so for the DOSED entries (Probe, Backbone, TransitiveReduce, BVE) it is
   structurally 0 and must never be read as "never produced anything". Everything below uses
   `adm` for occasions and the rule's own `c <rule>` line for yield. Likewise `c probe 0
   rounds` / `c backbone 0 rounds` are 0 because those rules count canonical rounds and the
   policy applies them one instance at a time.
3. **Every kissat counter used here was read at its INC site first**
   ([[reference-counter-scope]]). `eliminated` and `subsumed` each have a second site in
   `fastel.c`, dead at `fastel`=0 (`options.h:62`). `congruent` (`congruence.c:948`) counts
   literal-class MERGES, which is what our `merges` counts — `congruent_gates` is the gate
   volume and the two must not be crossed. `units` (`flags.c:70`) is the whole-solver root-unit
   total, not any one rule's, and is quoted as such. `sweep_units` has three sites, all inside
   the sweeper.

## 2. Occasions per technique, per solve

`adm` = occasions our policy ADMITTED (occasions offered minus every decline).
kissat column = calls of that technique (`closures`, `substitutions`,
`backbone_computations`, `vivifications`, `sweep`, `transitive_reductions`,
`factorizations`, `forward_subsumptions`), each one an `INC` at the top of the entry point.

| instance | conflicts t/k | probe phases t/k | elim phases t/k | Congruence | Substitute | Backbone | Vivify | Sweep | Transitive | Factor | C1 fwd |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 9vliw | 313 484 / 206 660 | 14 / 12 | 6 / 5 | 19 / 8 | 28 / 45 | 42 / 45 | 42 / 22 | 42 / 18 | 42 / 22 | **5 / 23** | 18 / 10 |
| b22_1 | 486 198 / 268 413 | 17 / 13 | 8 / 6 | 31 / 11 | 34 / 49 | 51 / 49 | 51 / 24 | 51 / 24 | 51 / 24 | **0 / 25** | 24 / 12 |
| T105.2.0 | 312 157 / 605 325 | 11 / 16 | 5 / 7 | 20 / 8 | 23 / 62 | 33 / 61 | 33 / 30 | 33 / 9 | 33 / 30 | **0 / 24** | 15 / 8 |
| hwmcc17miters | 1 / 0 | 0 / 1 | 0 / 0 | 1 / 1 | – / 0 | – / 0 | – / 0 | – / 0 | – / 0 | – / 0 | – / 0 |
| crypto1 | 761 919 / 162 604 | 20 / 10 | 9 / 4 | 18 / 6 | 21 / 33 | 60 / 33 | 60 / 16 | 60 / 7 | 60 / 16 | **0 / 15** | 27 / 8 |
| aes_decry_2 | 935 184 / 1 332 217 | 21 / 27 | 9 / 12 | 38 / 17 | 45 / 106 | 63 / 105 | 63 / 52 | 63 / 23 | 63 / 52 | **0 / 44** | 27 / 19 |
| clique_n2_k10 | 156 066 / 570 051 | 9 / 14 | 4 / 6 | 6 / 5 | 6 / 35 | 27 / 35 | 27 / 17 | 27 / 5 | 27 / 17 | 16 / 15 | 11 / 12 |
| or_randxor | 16 404 286 / 616 974 | 77 / 16 | 19 / 7 | 12 / 7 | 12 / 39 | 229 / 39 | 229 / 19 | 229 / 8 | 229 / 19 | **0 / 11** | 29 / 12 |
| sted5 | 217 006 / 169 679 | 12 / 11 | 5 / 5 | 10 / 4 | 10 / 25 | 36 / 25 | 36 / 12 | 36 / 4 | 36 / 12 | **0 / 13** | 11 / 10 |
| battleship | 309 229 / 258 175 | 12 / 10 | **1 / 4** | 2 / 4 | 2 / 19 | 35 / 19 | 35 / 9 | 35 / 4 | 35 / 9 | 35 / 5 | **1 / 8** |

Three structural facts jump out of this table before any yield is looked at.

**(a) The two PHASE cadences match.** Realized probe : eliminate phase ratio is 2.13–2.40
for us and 2.17–2.50 for kissat, on every instance where both clocks fire. `SCHED-D2` got
the shape right, and the "~2.4× rather than 5×" correction in `SP-SLOT-1` is not a defect —
kissat's realized ratio is also ~2.3 for exactly the same reason (each clock keys on its own
phase count).

**(b) Inside a phase, kissat has seven per-technique rates and we have one.** On 9vliw,
kissat's calls per probing phase are substitute **3.75**, backbone **3.75**, factor 1.92,
vivify 1.83, transitive 1.83, sweep 1.50, congruence 0.67. Ours are Probe = Backbone =
Vivify = Sweep = TransitiveReduce = **3.00 exactly, on all ten instances** —
`probe_phases × INPROCESS_PHASE_ROUNDS`, with no per-technique modulation at all. The four
entries that do differ (Congruence, Substitute, Factor, HyperTernaryRes) differ only because
a *decline* fired, not because the schedule intends a different rate.
Two named causes: kissat's probe pipeline calls `kissat_binary_clauses_backbone` and
`kissat_substitute` **twice per round** (`probe.c:33-40`), and kissat gives sweep and
congruence their own delay ladders (§4.4, §4.5).

**(c) Our per-technique intervals are ~30 % longer at the phase level and diverge by up to
7× at the technique level.** Conflicts per occasion on 9vliw (ours / kissat's, kissat's read
straight off its `interval` column): probe phase 22 392 / 17 222 · elimination phase
52 247 / 41 332 · Congruence 16 499 / 25 833 · Substitute 11 196 / 4 593 · Backbone
7 464 / 4 593 · Vivify 7 464 / 9 394 · Sweep 7 464 / 11 481 · TransitiveReduce 7 464 / 9 394 ·
Factor **62 697 / 8 985** · C1-forward 17 416 / 20 666. So we re-run Vivify/Sweep/Transitive
*more* often than the reference and Backbone/Substitute/Factor *much* less.

## 3. Yield per occasion

Like-for-like counters only; a pair with no honest counterpart was left out rather than
invented. `t/k` = ours / kissat's.

| instance | BVE vars elim | Factor vars | Congr. merges | Congr. gates | Sweep equiv+units | Sweep seeds | Vivify vivified | Vivify checks | Subst vars | Backbone units | Transitive deleted | C1 subsumed |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 9vliw | **9 802 / 14 890** | 4 734 / 6 021 | 17 / 147 | 25 457 / 25 309 | **0 / 524** | 42 / 9 839 | 4 435 / 22 986 | 9 932 / 36 530 | 215 / 953 | 214 / 549 | 1 465 / 2 029 | 5 514 / 11 337 |
| b22_1 | **0 / 29 726** | **0 / 429** | 1 126 / 127 | 865 536 / 80 760 | **0 / 591** | 176 / 11 416 | 9 730 / 41 957 | 100 274 / 105 093 | 1 446 / 831 | 82 / 44 | 2 790 / 1 749 | 1 111 / 15 682 |
| T105.2.0 | **0 / 2 736 829** | **0 / 12 755** | 2 692 / 14 589 | 10 280 972 / 18 738 720 | **0 / 1 511** | 192 / 32 772 | 6 928 / 103 554 | 255 778 / 263 377 | 12 992 / 20 026 | 65 / 3 834 | 1 433 / 8 525 | 339 399 / 1 068 089 |
| hwmcc17miters | 0 / 0 | 0 / 0 | **710 722 / 711 434** | 1 592 904 / 1 510 553 | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 |
| crypto1 | **0 / 601** | **0 / 98** | 4 / 40 | 16 745 / 4 378 | **0 / 111** | 63 / 1 255 | 12 150 / 22 501 | 98 090 / 57 507 | 48 / 126 | 0 / 0 | 520 / 994 | 3 208 / 3 396 |
| aes_decry_2 | **0 / 94 738** | **0 / 32 157** | 17 357 / 56 418 | 1 333 053 / 386 574 | **0 / 15 687** | 172 / 8 705 | 11 463 / 136 989 | 59 711 / 231 802 | 54 986 / 62 634 | 60 / 46 | 8 774 / 1 233 | 30 677 / 83 542 |
| clique_n2_k10 | 31 / 244 | 168 / 184 | 0 / 0 | 180 / 180 | 0 / 0 | 201 / 284 | 1 651 / 31 025 | 4 269 / 35 214 | 0 / 0 | 0 / 1 | 2 / 114 | 17 / 95 |
| or_randxor | **0 / 1 257** | **0 / 446** | 0 / 1 | **0 / 4 984** | 0 / 4 | 3 267 / 1 416 | 26 839 / 50 387 | 1 361 771 / 102 134 | 0 / 16 | 0 / 0 | 0 / 5 | 3 / 34 |
| sted5 | **0 / 692** | **0 / 999** | 0 / 0 | 9 107 / 1 224 | 0 / 0 | 54 / 1 171 | 3 518 / 21 227 | 65 450 / 102 473 | 1 / 1 | 0 / 2 | 1 102 / 1 270 | 1 896 / 1 459 |
| battleship | **0 / 104** | 182 / 182 | 0 / 0 | 0 / 0 | 0 / 0 | 76 / 469 | 1 454 / 2 058 | 6 848 / 3 418 | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 |

## 4. Part 2 — separating "the schedule refuses it" from "the rule cannot do it"

A zero-yield row has two very different causes and the N4 verdict depends on which. Part 2
forces the dose and re-reads the yield. **These arms perturb the trajectory deliberately —
none of them is a performance comparison.**

### 4.1 BVE — the density gate, not the rule, is the reason for the zeros ▶ CRUDER

`TACTIX_BVE_DOSE_DENSITY=0` (the invented binary-density admission off, everything else
shipped):

| instance | BVE `adm` before/after | vars eliminated before/after | kissat |
|---|---|---|---|
| b22_1 | 0 (`dns 21`) / **18** | 0 / **27 104** | 29 726 |
| aes_decry_2 | 0 (`dns 24`) / **24** | 0 / **96 357** | 94 738 |
| crypto1 | 0 (`dns 24`) / **27** | 0 / **886** | 601 |
| sted5 | 0 (`dns 8`) / **18** | 0 / **126** | 692 |

The rule reaches 91 %, 102 %, 147 % and 18 % of kissat's elimination yield the moment the
gate is out of the way. **So the zeros are the gate, entirely.** `bve_dose_declined`
(`default_policy.cpp:1587-1610`) declines every during-search occasion whenever
`binaries < 6 × output_vars`.

Read at the source across the whole set, the seven zero-elimination instances split into two
distinct causes, and only the first belongs to this row:

| cause | instances | BVE decline counters |
|---|---|---|
| **the density gate declines EVERY during-search occasion** | b22_1, T105.2.0, crypto1, aes_decry_2, or_randxor, sted5 — **6** | `dns` 21/21, 12/12, 24/24, 24/24, 26/26, 8/8 |
| the elimination CLOCK closed after preprocessing, so there was only ever one occasion and it was the `defer_preprocess` decline | battleship — **1** | `occ 1, pre 1` (§4.3) |

- **Class:** wrong MECHANISM — a formula-shape admission gate standing in for a
  per-application quality bound. Exactly the failure `N4`'s own text names as a **defect**
  (plan §1.1, and ledger theme T4). It has no counterpart in kissat or cadical.
- **What kissat does instead:** admission is interval + new-material watermark only
  (`eliminate.c:21-38`); *per-candidate* cost is bounded by `eliminatebound` (ramping
  0→1→2→4→8→16 with completion, `eliminate.c:339-372`) and by the round resolution limit.
  Nothing looks at binary density.
- **One-line change:** `dose_density` default 6 → 0. The comment above the function already
  concedes the family "has measured badly more than once"; what is new here is that it is
  measured as binding on **6 of 9** and costing the entire family, not as a tuning knob.
  A tier pair is owed before it ships; this row is the pre-flight that says the arm is not
  vacuous.
- **Incidental reachability datum for `SP-EFFORT-CEIL-1`:** on `or_randxor` BVE reports
  `clamped 2`. `SCHED-E3` measured `clamped 0` for every entry on `9vliw`/`b22_1`/`sted5` and
  labelled cadical's shipped constants inert; this is the first instance on which the 2e9
  resolution ceiling actually fires at the shipped value. It stays a marginal mechanism — 2
  occasions out of 26 offered — but "inert" should now read "inert on the sizes measured so
  far, fires on a 16 M-conflict solve".

### 4.2 Factor — the same gate, plus a genuine WIRING bug that cascades ▶ CRUDER

Factor adds 0 extension variables on 6 of 9. The decline counters split it cleanly into two
causes:

| instance | Factor declines | kissat `factored` |
|---|---|---|
| b22_1 / crypto1 / sted5 / or_randxor | `dns` = **every** occasion (51/51, 60/60, 36/36, 229/229) | 429 / 98 / 999 / 446 |
| aes_decry_2 / T105.2.0 | `warm` = **every** occasion (63/63, 33/33) | 32 157 / 12 755 |
| 9vliw | `bud` 37 of 42 (admitted 5) | 6 021 (we get 4 734) |

The `warm` rows are the interesting ones, and they are a **cascade, not a threshold**. Our
`factordelay` port (`factor_entry_declines`, `default_policy.cpp:1795-1817`) is a faithful
transliteration of kissat's `log10(active) <= eliminations + factordelay` including the
truncation correction — but the `eliminations` input is `bve_rounds_run_`, which is
incremented **inside `run_dosed_entry`, after BVE's dose** (`default_policy.cpp:1774`), i.e.
only on an ADMITTED BVE occasion. kissat's `INC (eliminations)` is the first statement of
`kissat_eliminate` (`eliminate.c:595`), i.e. once per elimination phase. So:

> a density-declined BVE freezes Factor's warm-up counter at 0, which pins Factor's
> admission threshold at 10^5 active variables forever, which refuses Factor on every
> occasion of a 279 120-variable formula.

Measured directly: with `TACTIX_BVE_DOSE_DENSITY=0` on aes, BVE goes 0 → 24 admissions and
Factor's declines move from `warm 63, dns 0` to `warm 9, dns 54` — the warm-up opens after
the first BVE round, exactly as the mechanism predicts, and the invented density gate then
takes over. So on aes the two defects are stacked: fixing either alone still leaves Factor at
zero.

- **Class:** WIRING GAP (one input read from the wrong place) sitting behind an invented
  gate. Two separate one-line changes: advance the `eliminations` analogue on every
  elimination PHASE rather than on an admitted BVE dose, and retire the density gate per §4.1.
- Note this row does **not** reopen `SP-FACTOR-1` (the acceptance-bound coupling), which
  stays an accepted deviation. It is about whether Factor runs at all.

### 4.3 The elimination-clock watermark can close for the rest of the solve ▶ CRUDER

`battleship-14-26-sat`: `c slot-clock … eliminate phases 1 next 16992 blocked 1` at 309 229
conflicts. The interval elapsed at conflict 16 992 and never fired again, because the
new-material gate held. Consequence, read off the slot lines: SubsumeForward, Strengthen,
BVE, Instantiate, BCE and CCE each received **exactly one occasion in the entire solve**, and
BVE's was `pre`-declined — so **the whole elimination pipeline ran zero times**. kissat ran
4 eliminations, 8 forward subsumptions, eliminated 104 variables and subsumed 1 459 clauses.

This is not the `SP-SLOT-5` flip bug (fixed by `SCHED-D3`, which counts mark EVENTS). It is
the **producer set**. Read at the sites:

| | fed by |
|---|---|
| kissat `variables_subsume` (`inline.h:52-66`) | every clause ADD — `new_binary_clause` marks **unconditionally, including learned binaries** (`clause.c:71-72`), `new_large_clause` marks when irredundant (`clause.c:98`) — **plus every clause strengthened or shrunken** by forward subsumption (`forward.c:421,454`), vivification (`vivify.c:746,835,1163`) and strengthening (`strengthen.c:37`) |
| kissat `variables_eliminate` (`inline.h:39-50`) | every irredundant-large REMOVAL (`clause.c:154`) and every binary deletion (`clause.c:181-182`, ungated) |
| ours `Stats::bve_marks` | irredundant-large ADD + binary ADD + root-fixed sync (`kernel.cpp:111,149-150,240-241,309-310,381-423`) — **no removal producer, and no strengthening producer** |

Our source comment at `kernel.cpp:104` records the removal half as a deliberate, measured
negative ("kissat marks on REMOVAL only … measured here and is NEGATIVE"), which is fine as
far as it goes — but the *strengthening* producers are a third set, neither ported nor
ledgered, and the consequence measured here (a permanently closed elimination clock) has not
been recorded before.

- **Class:** MISSING FEEDBACK SIGNAL. Two candidate one-line closers, either of which
  re-opens the clock: mark on clause strengthening (kissat's `forward.c`/`vivify.c`/
  `strengthen.c` sites, which our C1/C2/Vivify already reach), or count a phase in which any
  rule reported `Ok` as new material.
- **Reachability witness already exists:** `SlotClock::blocked_episodes`, printed as
  `blocked N`. It reads 1 on battleship and 7 on `or_randxor`, and 0 elsewhere — so this is
  a two-instances-in-ten row, not a universal one. Say so.

### 4.4 Sweep — the zero yield is a RULE finding, and this pass says so ▶ CRUDER on the governors, but they are not the cure

Our Sweep finds **0 equivalences on 9 of 9** instances where it ran, including five where
kissat finds 111–15 687 equivalences+units. That looks exactly like a starved governor, and
it is not. Two escalating arms:

| arm | b22_1 candidates → equivalences | 9vliw | aes |
|---|---|---|---|
| shipped | 1 152 → **0** | 1 152 → 0 | (63 envs) → 0 |
| `sweepbig` (kissat's max caps: vars 8 192, clauses 32 768, depth 3; effort ×1 000) | 23 406 → **0** | 258 → 0 | – |
| `sweepfree` (+ the two per-environment bounds with no reference counterpart lifted: `env_ticks` 200 k → 200 M, `max_candidates` 32 → 1e6) | **1 226 656 → 1** | 55 119 → **0** | 460 196 → **48** |
| kissat, same formula | 51 949 equivalence queries → **545 equivalences** | → 419 | → 15 687 equiv+units |

At **24× kissat's equivalence-query count** we confirm 1 where it confirms 545. The
mechanism our Sweep is supposed to have is present and correct in outline (model-based
candidate seeding at `sweep.cpp:434-445`, model refinement at `:319-331` — kissat's own
scheme), so this is not a missing-mechanism row either. **It belongs to the rule, and this
pass routes it there rather than to Track SCHED.** That is the most useful thing in §4:
it stops `SP-SWEEP-2` and `SP-SWEEP-3` from being sold as the fix for the yield.

What DOES remain an N4 gap for Sweep, now correctly bounded to "stops paying for nothing"
rather than "recovers the yield":

1. **No productivity delay ladder.** kissat: `if (average(eliminated, swept) < 0.001)
   BUMP_DELAY(sweep) else REDUCE_DELAY(sweep)` (`sweep.c:1717-1720`), consumed before any
   setup at `:1655`, linear growth / geometric decay (`kimits.c:155-191`). Ours: none. Our
   generic ladder is gated on `SlotEntry::closure`, and Sweep is not a closure — so
   `SP-SWEEP-2`'s ledger wording ("gated on `!e.meter`") is now **stale**: since `SCHED-G1`
   the gate is spelled `e.closure` (`default_policy.cpp:2046-2062`). The substance is
   unchanged and confirmed: Sweep ran 42/51/60/63 zero-yield occasions and nothing backed it off.
2. **No completion-driven environment ramp.** kissat scales the caps by `sweep_completed`:
   `vars = min(256 << completed, 8192)`, `clauses = min(1024 << completed, 32768)`,
   `depth = min(sweep_completed + sweepdepth, 3)` (`sweep.c:107-137`), where `sweep_completed`
   increments when a whole scheduled pass finishes (`:1606,1641`). Ours are the three
   constants at kissat's *initial* values with no ramp (`sweep.hpp:96-98`) and no
   completeness concept at all.
3. **Two per-environment bounds with no reference counterpart.** `SweepLimits::env_ticks`
   (200 k cumulative sub-solver ticks per environment) and `max_candidates` (32 equivalence
   queries per environment). kissat has neither: it pushes the REMAINING ROUND limit into
   kitten per environment (`sweep.c:53-60`) and caps only the round. This is the same shape
   as T4's density gates — a static per-unit bound where the reference has a shared,
   adaptive budget — and neither is in the ledger.

### 4.5 Congruence — right structure, cruder ladder threshold ▶ MIXED

Occasions 2–3× kissat's; gate volume 3–10× kissat's on five instances for one third of its
merges; **parity on the family it exists for** (`hwmcc17miters`: 710 722 merges against
711 434, one round, refuted in 1 conflict). Governors: we have kissat's closure watermark and
a delay ladder plus a 100 ‰ budget kissat does not have (`SP-CONG-BUDGET-1`, a deliberate
stricter deviation for a measured cost gap — reconfirmed here, e.g. `b22_1` 865 536 gates for
1 126 merges against kissat's 80 760 for 127). The watermark is working: on `or_randxor`,
`mat 162` of 229 occasions and `dly 55`.

The one behavioural gap is the **ladder's threshold**:

| | bump condition |
|---|---|
| kissat (`congruence.c:4630-4632`) | `average(equivalent, active) < 0.001` — a RATE |
| ours (`default_policy.cpp:2090-2096`) | `ar != ApplyResult::Ok` — i.e. only when the round changed **nothing at all** |

So a round that merges 1 variable in 10^6 halves our delay and bumps kissat's. Same for the
Vivify-irr ladder? No — there we *do* use kissat's rate form (`produced * 100 < tried`,
`default_policy.cpp:1970`), which is why this row is a threshold slip rather than a design
gap. **Class: WRONG CONSTANT/PREDICATE, one-line change** (compare merges against active
variables at the 0.001 rate, as the vivify ladder already compares against `tried`).

### 4.6 `or_randxor` — our XOR gate extractor finds nothing ▶ NOT an N4 row, but it must be said

kissat: 7 closures, **4 980 XOR gates** (100 % of its 4 984 gates), 1 merge, UNSAT at
616 974 conflicts. Ours: 12 admitted Congruence rounds, **0 gates of any kind** (`c conggates
0 and, 0 xor, 0 ite`), UNKNOWN at 16 404 286 conflicts. The formula is literally a conjunction
of 3-XORs and `CONG_XOR_ARITY = 4` is at kissat's own default, so arity is not the blocker.
This is the "4 vs 4 984" asymmetry from the brief, and on this instance it is 0 vs 4 984.
**It is an extraction defect in the rule, not a schedule gap** — routed to rule-dev, and
recorded here because the instance is in the set and the number is a strong one.
(Also note kissat gets only **1** merge out of those 4 980 gates, so the gate count alone is
not the win; do not turn this into a gate-volume target.)

## 5. The per-technique verdict table

The deliverable. "Behaviourally" throughout: mechanism inventory is not the question.

| # | technique / mechanism | verdict | the specific gap, classified |
|---|---|---|---|
| 1 | **Two inprocessing clocks** (probe vs eliminate cadence + growth key) | **EQUAL** (ahead of cadical) | none. Realized ratio 2.13–2.40 vs kissat's 2.17–2.50 on every two-clock instance. `SCHED-D2` landed the right shape; cadical's own probe spacing is near-constant, so we are ahead of it |
| 2 | **Within-phase multiplicity** (per-technique rate inside one phase) | **CRUDER** | MISSING STRUCTURE. kissat calls `backbone` and `substitute` twice per probe round (`probe.c:33-40`); we call each entry once per outer round, so five entries share one rate on all ten instances. One-line change: duplicate those two positions in `AUTHORED_SLOT_ORDER` |
| 3 | **BVE / elimination admission** | **CRUDER — the sharpest row** | WRONG MECHANISM. Invented `binaries < 6·vars` gate declines every during-search occasion on 6 of 9; gate off ⇒ 91–147 % of kissat's yield on 3 of the 4 tested. §4.1. One-line change (`dose_density` 6 → 0), tier pair owed |
| 4 | **Factor admission** | **CRUDER** | (a) WIRING GAP: `factordelay`'s `eliminations` input reads `bve_rounds_run_`, advanced only on an ADMITTED BVE dose, where kissat's advances per elimination phase — a density-declined BVE freezes Factor's warm-up (aes `warm 63/63`, T105 `warm 33/33`). (b) the same invented density gate on four more instances. §4.2 |
| 5 | **Elimination-clock new-material watermark** | **CRUDER** | MISSING FEEDBACK SIGNAL. Fed by irredundant/binary ADDs + root fixes only; kissat is additionally fed by every clause REMOVAL and every clause STRENGTHENED by forward/vivify/strengthen. Closes permanently on battleship: whole elimination family = 1 occasion in 309 k conflicts. §4.3 |
| 6 | **Sweep governors** | **CRUDER** (but see the caveat) | THREE items: no productivity delay ladder (kissat `sweep.c:1717-1720`); no completion-driven env-cap ramp (`sweep.c:107-137`, 256→8192 / 1024→32768 / depth 2→3); two invented per-environment bounds (`env_ticks`, `max_candidates`) where kissat shares one round budget. **Caveat: fixing all three will not recover the yield — the 0-equivalences result survives a 1 000× dose (§4.4) and is a RULE finding.** |
| 7 | **Congruence governors** | **MIXED** | Stricter-than-reference budget by design (`SP-CONG-BUDGET-1`, reconfirmed). Watermark works. One WRONG PREDICATE: the delay ladder bumps only on a completely inert round, where kissat bumps below a 0.001 merge RATE. Our own vivify ladder already has the rate form |
| 8 | **Vivify governors** | **EQUAL or ahead** | Full parity: kissat's four scopes with its own 3/3/1 + irr shares, the irr delay ladder at the same 1 % rate, tier bands from the LIVE tiers (kissat `vivifyfocusedtiers`=1; cadical's `vivifycalctier` defaults 0, so ahead of cadical), per-clause bit with tail re-arm. **The 2–4× yield-per-check gap is candidate QUALITY, not cadence** — named mechanism `SP-VIVIFY-3` (cadical `vivifyflush`: sort literals by occurrence, `stable_sort` the schedule, drop prefix-duplicates). This measurement upgrades that MED row |
| 9 | **Backbone governors** | **CRUDER** | MISSING GROWTH RAMP + missing attempt marks. kissat's internal round cap is `backbonerounds(100) × backbone_computations`, capped at `backbonemaxrounds` 1e3 (`backbone.c:344-350`) — 2 130 internal rounds on 9vliw — and it drops satisfied probes via per-literal `flags->backbone0/1`. Ours is one pass per dose, no ramp, no marks. (Our per-probe selectivity is 8× better — 0.82 % vs 0.099 % failed on 9vliw — on 21× fewer probes) |
| 10 | **Substitute governors** | **CRUDER** | MISSING BUDGET + missing pipeline position. kissat: 2 rounds, 10 ‰, hand-rolled with **no `mineffort` floor** (`substitute.c:568-602` — the floor is precisely why it could not use the macro). Ours: no `EffortSpec` row at all (`D-SUBST-UNBOUNDED`), and one pipeline position where kissat has two. Confirms `SP-SUBST-2` behaviourally: 1/2–1/3 of kissat's calls |
| 11 | **TransitiveReduce governors** | **EQUAL** | 20 ‰ = kissat's `transitiveeffort`; resumption via `SlotEntry::cursor` against kissat's per-var `flags->transitive`. Yield mixed both ways (we win aes 8 774/1 233 and b22_1 2 790/1 749) |
| 12 | **SubsumeForward / Strengthen governors** | **EQUAL, one constant cruder** | 100 ‰ = kissat's `forwardeffort`; upper clamp 1e8 = cadical's `subsumemaxeff` (**kissat has none — ahead of kissat**); touched-epoch marks landed. Open: `SP-SUBSUME-3` — one `OCCLIM` of 1 000 for both populations where cadical splits `subsumeocclim` 1e2 / `subsumebinlim` 1e4 |
| 13 | **Effort-window SCOPE** | **DEVIATION (more generous), and the ledger's justification is WRONG** | kissat has exactly TWO effort references, `last.ticks.probe` / `last.ticks.eliminate`, written at the END of each group's phase unconditionally (`probe.c:86`, `eliminate.c:601`); `SET_EFFORT_LIMIT` only READS them, so all techniques in a phase share one window and the group's spend is bounded by Σ effort. We anchor per ENTRY and advance only on the admitted path, so a long-declined entry accrues several phases' windows. §6 |
| 14 | **Phase round cap** | **more generous than the reference** | `INPROCESS_PHASE_ROUNDS` 3 vs kissat's `proberounds` 2 / `eliminaterounds` 2; and kissat's probe loop breaks on `before == solver->active` (no variable-count change), stricter than our `round_changed` (any change) |
| 15 | HyperTernaryRes / Instantiate / BCE / CCE | **NOT COMPARABLE** | kissat 4.0.4 has no ternary, instantiate, block or cover, so every constant is ours. Recorded because the ledger already says so; noted that HyperTernaryRes was `bud`-declined 41 of 42 occasions on 9vliw, i.e. its 8 ‰ share rarely affords a round |

**Tally over the 15 rows: 7 CRUDER (2, 3, 4, 5, 6, 9, 10) · 1 MIXED (7) · 4 EQUAL-or-ahead
(1, 8, 11, 12) · 2 deviations in the more-generous direction (13, 14) · 1 not comparable (15).**

## 6. Where we are AHEAD, honestly

An N4 answer that only lists gaps is not an audit.

1. **The two-clock split is at kissat's shape and ahead of cadical's** (row 1), and the
   realized ratio measured here (2.13–2.40 vs 2.17–2.50) says so empirically rather than by
   formula-reading.
2. **Vivify's scope/share/ladder structure is at full kissat parity and ahead of cadical**
   — four scopes, kissat's own 3/3/1+irr shares actually spent that way since `SCHED-G3`,
   the irr ladder at kissat's rate form, and the bands read from the LIVE tiers where
   cadical's `vivifycalctier` defaults 0.
3. **We carry an upper effort clamp kissat does not have at all** (`SP-EFFORT-CEIL-1`:
   kissat ships no `maxeffort`), and it is labelled inert rather than sold as a win.
4. **Congruence is budgeted where kissat gives it no budget** — deliberately stricter, with
   the cost gap named (`SP-CONG-BUDGET-1`), and reconfirmed here on two new instances.
5. **The dynamic retention tier is live and consequential** — `battleship` reports
   `c tiers 26/34 live after 57 retiers (retention band 879 500 clause-visits dynamic vs 0
   static)`: a static-6 consumer there would admit nothing. This is `SCHED-C10`'s measurement
   reproduced independently.
6. **The decline telemetry itself.** Every finding in §4 came out of
   `TACTIX_SLOT_STATS`'s per-entry occ/adm/decline-reason lines with their accounting
   self-check (`0 ***UNACCOUNTED***` on all ten instances). Neither reference has an
   equivalent, and without it rows 3, 4 and 5 are invisible — a governor that declines
   everything and a governor that is never reached look identical from the outside. That is
   not scheduling sophistication, but it is the instrument that makes N4 checkable, and it
   deserves to be in the N4 answer.

## 7. Ledger corrections this pass produces

For `docs/implementation-notes.md` §13, via the calculus skill. Not applied here — this
branch changes no source and no living doc.

1. **`SP-SLOT-3` (CLOSED by `SCHED-B1`) rests on a false source claim.** The row and the
   code comment at `default_policy.cpp:1775-1780` both say kissat "writes `last.NAME.ticks`
   inside `SET_EFFORT_LIMIT`, i.e. only on a call that actually runs". There is no
   `last.NAME.ticks`: kissat has `last.ticks.probe` and `last.ticks.eliminate` only, both
   written at the END of the group's phase (`probe.c:86`, `eliminate.c:601`) regardless of
   which techniques ran, and `SET_EFFORT_LIMIT` (`kimits.h:135-170`) only reads them. So
   kissat's effort window is per-GROUP and resets every phase; ours is per-ENTRY and accrues
   across declined occasions. Our scheme is the more generous one. The row should become a
   labelled deviation with that argument, not a parity closure.
2. **`SP-SWEEP-2`'s wording is stale.** "Our ladder … is gated on `!e.meter`" — since
   `SCHED-G1` the gate is `SlotEntry::closure` (`default_policy.cpp:2046-2062`). The gap is
   unchanged (Sweep is not a closure, so it still cannot back off) but the text points at a
   spelling that no longer exists.
3. **`SP-SWEEP-3` should be reclassified.** It is filed as a repetition/waste row. §4.4
   shows the waste is real but the *yield* is not recoverable by any dosing change, so the
   row's removal path must not promise yield. And two per-environment bounds with **no
   reference counterpart** (`env_ticks`, `max_candidates`) are absent from the ledger
   entirely — they are T4-shaped and belong in it.
4. **A new HIGH row is owed for the elimination watermark's producer set** (§4.3). `SP-SLOT-5`
   is closed and correctly so — the flip bug is fixed — but the *producers* are a distinct
   gap with a measured wedge, and no row covers it.
5. **A new row is owed for the `bve_rounds_run_` ↔ `eliminations` wiring gap** (§4.2). It is
   not `SP-FACTOR-1` (which is about the acceptance bound and stays an accepted deviation).
6. **T4's membership list gains two and loses none.** Sweep's `env_ticks` and
   `max_candidates` are static per-unit bounds where the reference shares one adaptive
   budget — the same shape the theme exists to catch.

## 8. Bottom line on N4

**Behaviourally, N4 is not met, and the ledger being closed on eleven of twelve HIGH rows is
not evidence that it is.** The mechanisms are present; the schedule they compose does not act
like the reference's on this set:

- On **7 of 9** instances our BVE eliminates **nothing** while kissat eliminates 104 to
  2 736 829 variables. On six of the seven the sole cause is an admission gate of our own
  invention that the plan's own N4 text names as a *defect* rather than a tuning question;
  on the seventh it is the watermark below.
- On **6 of 9** Factor adds **nothing** — on four because of that same gate, on two because a
  warm-up counter is wired to the wrong event.
- On **1 of 10** the entire elimination family runs **once in 309 229 conflicts** because a
  watermark's producer set is smaller than the reference's.
- Of the five probing entries with no per-technique governor, kissat gives three a distinct
  rate and we give all five one rate.

Against that, five rows are genuinely at or above reference strength (the two-clock split,
Vivify's whole scope/share/ladder structure, TransitiveReduce, C1/C2 modulo one constant, the
effort ceiling), two of our deviations are more generous rather than cruder, and the
Congruence budget is deliberately stricter with the reason named.

The encouraging half of the answer: **every CRUDER row above is a wire, a constant or a
missing signal — none of them is a missing mechanism.** Rows 3, 4 and 7 are one-line changes;
row 2 is two lines in a table; row 5 has two candidate one-line closers. And one suspected
scheduling gap (Sweep's zero yield) is shown here NOT to be one, which saves the governor
work that would otherwise have been spent on it.

## 9. Reproducing this

```
# part 1 -- occasions + yields, three arms x ten instances
sbatch --array=1-30 --output=<scratch>/n4-shape/%A_%a.out \
       benchmarks/probes/n4-shape.sbatch
# part 2 -- forced-dose reachability
sbatch --array=1-11 --output=<scratch>/n4-forced/%A_%a.out \
       benchmarks/probes/n4-shape-forced.sbatch
# the tables in sections 2 and 3
python3 benchmarks/probes/n4-shape-table.py <scratch>/n4-shape
```

`n4-shape.sh` needs `tools/solvers/variants/kissat-metrics/kissat` (a `--metrics` build of
the pinned clone, never built inside it) and a tactix binary; it resolves the gitignored
instance cache through `git worktree list` so it works from a linked worktree.
`ctest` 47/47 at this HEAD; no source file was modified by this pass.
