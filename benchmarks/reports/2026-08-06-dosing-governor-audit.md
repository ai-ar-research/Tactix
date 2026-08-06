# Dosing-governor audit — every `EFFORT_SPECS` row read against its reference use site

**Date** 2026-08-06 · **Branch** `audit-dosing` · **Base** `4f4989d` · **Scope** all 15 scheduled
inprocessing rules (14 metered + the one alternate-governor entry, Substitute) plus the Walk dose.
**FROZEN on writing** (doc discipline). **Nothing was fixed by this pass** — three other agents were
editing the tree; every item below is stated to become a one-line change.

## 0. Why, and the method

Four dosing defects were found on 2026-08-05/06, and every one was an implementation bug rather than a
bad constant: the effort ledger **wrapped** (unsigned `now − anchor` on a non-monotone reading, dose
deleted, `benchmarks/reports/2026-08-06-solve-loss-diagnosis.md`); a batch **pre-check that can never
fire**; a Sweep **poll outside the loop that spends** (`2026-08-05-sweep-governor-diagnosis.md`); and
**governors neither reference has** (the `binary_ids ≥ 6·vars` gates). This pass tests the rest of the
family against those four shapes:

- **(a) unfireable** — is the limit computed so the check can never trip?
- **(b) mis-placed** — is the poll outside the loop that actually spends?
- **(c) mis-currencied** — does the meter count something other than the work?
- **(d) inert-by-default** — does the governor ship off, false, or zero?

Every reference claim below was **read at the use site**, not from an option name — the standing traps
apply (cadical token-pastes `opts.NAME##thresh`, so `grep` finds nothing; `factordelay`=0 is the *most*
delayed setting; a counter is not a total or a partition unless checked).

**Measurement.** Counters only, no timing claim, unthrottled (contention-invariant).
12 Tier B instances (`benchmarks/probes/audit_dosing_instances.txt`), `TACTIX_SLOT_STATS=1`, 300 s cap,
`cpu-dense-hwulab-q`. **Four** arms on two binaries:

| arm | job | binary | what it is |
|---|---|---|---|
| `base` | `1587266` | `pin-audit-dosing` (shipped `4f4989d`) | the shipped ledger |
| `instr` | `1587336` | `pin-audit-instr` | + `handed`/`spent`/`completed`/`ok` on the DOSED and Vivify paths (this pass's temporary instrumentation, since **reverted**; it is also defect **D7**'s one-line fix) |
| `congini` | `1587356` | `pin-audit-instr` | `TACTIX_CONG_INIT_TICKS=1` — removes the 1e12 opening allowance so Congruence's per-round overshoot is readable |
| `midround` | `1587368` | `pin-audit-instr` | `TACTIX_MIDROUND_BUDGET=1` — turns on the C1/C2 poll that ships off |

All 12 instances report counters in every arm (10 UNSAT, 1 SAT, `or_randxor_k3_n540_m540` UNKNOWN at the
300 s cap — the SIGTERM path still prints the ledger). 11 of them exercise the whole slot;
`hwmcc17miters` is refuted inside the **opening Congruence round**, so it contributes that one entry's
line and nothing else. Summariser: `benchmarks/probes/audit_dosing_summarize.py`.

**Read `worst`, not the aggregate.** The aggregate `spent/handed` is a ratio of sums and is therefore
dominated by the long runs, which hand out large budgets they do not spend; it can hide a 35× single-dose
overshoot inside a 0.54 average. Every verdict below is driven by the per-instance maximum, with the
aggregate quoted only for context.

**Instrumentation identity evidence** — the `instr` binary must not have moved anything. All nine BATCH
entries' ledger lines are **byte-identical between `base` and `instr` on 11 of 11 instances that
finished** (`or_randxor` differs only because a 300 s cut lands at a different candidate);
`benchmarks/probes/audit_dosing_identity.sh`.

## 1. The verdict table

`worst` = the largest single-instance `spent/handed`; `sp/hd` = the same ratio aggregated over the 11
instances that exercise the whole slot (`instr` arm); `cmpl/adm` = doses that finished inside their
budget. `→` marks a metric that is **only readable at all** because of this pass's instrumentation.

| entry | ‰ | currency | reference limit + WHERE it polls (read at the use site) | our poll | occ | adm | **worst** | sp/hd | cmpl/adm | verdict |
|---|---|---|---|---|---|---|---|---|---|---|
| **TransitiveReduce** | 20 | `transitive_ticks` | kissat `transitive.c:320` `SET_EFFORT_LIMIT(limit,transitive,transitive_ticks)`; charged per **dst walk** (`:159`) and polled **right there** (`:181`), plus again per source (`:349`) | policy, between **sources** only; rule reads no `work_budget` | 534 | 534 | →**35.72×** | →0.542 | →0.39 | **D1 · (b)+(c) HIGH** |
| **Strengthen** (C2) | 100 | partner visits | kissat `forward.c:544` polls `forward_steps > steps_limit` in the candidate loop, **no option gate**; cadical `subsume.cpp:459` polls `subchecks >= check_limit`, gate `subsumelimited` default **1** | `midround_budget && …` — **ships false**; batch pre-check dead | 182 | 182 | **5.70×** | 0.807 | 0.49 | **D3 · (d) HIGH** |
| **SubsumeForward** (C1) | 100 | partner visits | as above | as above | 182 | 182 | **2.49×** | 0.333 | 0.62 | **D3 · (d) HIGH** |
| **Vivify** | 100 | `vivify_ticks` | cadical `vivify.cpp:1744` `SET_EFFORT_LIMIT(totallimit,vivify,`**`true`**`)`, `vivifythresh`=20 live from call 1; kissat `vivify.c:1422` cumulative tier limits | per candidate (`vivify.cpp:573`) — **correct**; but `thresh` **unfireable** | 534 | 534 | →0.99× | →0.916 | →0.51 | **D2 · (a) HIGH** (poll clean) |
| **Sweep** | 100 | `sweep_ticks` | kissat `sweep.c:143` + 7 in-loop polls; cadical charges the occurrence walk to the same budget; **`sweepthresh`=5 LIVE** (`sweep.cpp:1890` passes `!opts.sweepcomplete`, default 0) | in-expansion poll (`SweepBudget`) — **correct**; **no `thresh`** | 534 | 534 | 1.04× | 1.008 | **0.00** | **D4 · missing MED** (poll clean) |
| **Factor** | 50 | `factor_ticks` | kissat `factor.c:1104-1117` (allowance gated on `factorizations > 1`, INC'd per **call**); cadical `factor.cpp:961-969` (`!stats.factor`, `stats.factor++` per call). **Every admission gate precedes `SET_EFFORT_LIMIT`** (kissat `:1088-1100`, cadical `:936-958`) | quit points before each popped seed + inside the grow loop; **heap build unpolled**; anchor advanced on a density decline; allowance re-armed | 534 | 60 | 0.00× | 0.000 | 0.75 | **D5 · (a) MED · D6 · (b) MED · D10 · (b) LOW** |
| **Substitute** | — | — | kissat `substitute.c:568-592` breaks a round at `Δsubstitute_ticks > 0.010·(search_ticks − last.ticks.probe)`; `substituterounds`=2 | **none** (no `EFFORT_SPECS` row) | 534 | 213 | — | — | — | **D8 · known** (`D-SUBST-UNBOUNDED`) |
| **Congruence** | 100 | `congruence_ticks` | kissat has **no** congruence effort limit at all (`congruence.c` has no `SET_EFFORT_LIMIT`, no `congruenceeffort` option) — the share is ours | `!(outp & 255)` extraction window + 7 closure polls | 535 | 188 | **1.02×**¹ | 0.463¹ | 0.26 | **CLEAN** |
| **BCE** | 20 | `bce_ticks` | cadical `block` has **no effort limit** — `blockocclim`=100 / `blockmaxclslim`=1e5 are the governors | per blocking literal | 182 | 182 | **1.00×** | 0.290 | 0.40 | **CLEAN** (occ cap off by measured choice) |
| **CCE** | 4 | `cce_ticks` | cadical `cover.cpp:616` polls `propagations.cover < limit` in the candidate while-condition; `covereffort`=4 | per candidate clause (`cce.cpp:382`) | 182 | 182 | 1.11× | 0.944 | 0.04 | **CLEAN** |
| **Instantiate** | 20 | `instantiate_ticks` | cadical `instantiate` has **no effort limit**; `instantiateocclim`=1 / `instantiateclslim`=3 (we match both exactly) | per candidate clause (`instantiate.cpp:279`) | 182 | 182 | 1.03× | 0.367 | 0.14 | **CLEAN** |
| **HyperTernaryRes** | 8 | `ternary_ticks` | cadical `ternary.cpp:371` `SET_EFFORT_LIMIT(limit,ternary,`**`true`**`)`, `ternarythresh`=6; `ternaryocclim`=100 / `ternarymaxadd`=1000 % | per pivot (`ternary.cpp:281`) | 534 | 276 | 1.03× | 0.052 | 0.09 | **CLEAN** (thresh one occasion late — D2's second half) |
| **Backbone** | 20 | `backbone_ticks` | cadical `backbone.cpp:542` `SET_EFFORT_LIMIT(totalticks,backbone,`**`false`**`)` → `backbonethresh`=5 is **inert at any value**; `ticks_limit` polled per round AND passed into the round | policy, per probe instance | 534 | 534 | →1.04× | →0.234 | →0.53 | **CLEAN** |
| **Probe** | 20 | `probe_ticks` | cadical `probe.cpp:796` `probeeffort`=8; `probethresh`=0 ⇒ predicate is `DELTA < 0`, **inert** | policy, per probe instance | 534 | 534 | →1.26× | →0.401 | →0.20 | **CLEAN** |
| **BVE** | 100 | `bve_resolutions` | kissat `eliminate.c:478/523` polls `eliminate_resolutions > resolution_limit` **between variables** — the same granularity as ours; `eliminateocclim`=2e3 / `eliminateclslim`=100 (we match exactly) | policy, per variable | 182 | 60 | →0.11× | →**0.015** | →**1.00** | **CLEAN but INERT** (see §4.11) |
| Walk | 560 | walk steps | kissat `walk.c:413` `walkeffort`=50 (unit mismatch, deviation recorded) | one budget per walk | — | — | — | — | — | **CLEAN** (see D11) |

¹ Congruence's `base`/`instr` ratio is meaningless because `handed` is dominated by the 1e12 opening
allowance; the numbers quoted are the `congini` arm, which removes it. `wrapped 0` on **every entry ×
every instance × every arm** — the ledger-wrap fix holds.

## 2. Defects, ranked by whether the governor binds at all

### D1 — HIGH · TransitiveReduce has no in-walk poll, and credits its meter only per SOURCE · shape (b)+(c)

`reduce_source` (`src/rules/transitive_reduce/transitive_reduce.cpp:178-225`) walks every candidate `dst`
of a source, accumulating cost into the round-local `w.ticks`, and credits
`k.stats.transitive_ticks += w.ticks` only at the source's three exits (`:183`, `:202`, `:223`). The rule
is `budget_aware = false`, so it never reads `Kernel::work_budget`; the **only** budget check is the
policy's between-instances `e.meter(k) > limit` (`default_policy.cpp:1709`), which is one whole source
too coarse *and* cannot see the in-flight spend because the meter has not been credited yet.

kissat does exactly the opposite: `transitive_reduce` charges `inner_ticks` to `transitive_ticks` after
**every** `dst` walk (`transitive.c:159-161`) and polls `transitive_ticks > limit` on the next line
(`:181`), with a second poll in the source loop (`:349`).

**Counter evidence** — `veer_axi_yosyshq_appnote_123_veer_axi-p23_transition`, `instr` arm:

```
c slot TransitiveReduce occ 12 adm 12 ok 21 | handed 2950760 spent 105415217 completed 0
c transitive  0 rounds, 43318 checked, 39 deleted, 0 failed, 105415217 ticks
c meters      probe 3727403 backbone 3080753 vivify 4927071 instantiate 1975113 | global ticks 83134630
```

`spent/handed` = **35.72×**, `completed 0/12`, 43,318 checks for **39** deletions — and
`transitive_ticks` (105 M) **exceeds the entire solve's global tick counter** (83 M). Reproduced on the
same instance at **15.40×** in the `congini` arm (different trajectory, same shape), and it is by a wide
margin the largest single-dose overshoot in the family (next is Strengthen's 5.70×, whose cause is
already known). Arithmetic: 43,318 checks / 12 doses ≈ 3,610 checks per dose against ~245 k ticks handed
per dose, so a single hub source's out-edge fan already spends ~36 doses before the policy's
between-sources poll gets a chance to look.

**Amplifier, not the cause:** `Kernel::transitive_early_exit` ships `false`, so each reachability walk
runs to exhaustion where kissat exits at `dst` (`transitive.c:130`). That is a recorded inference
tradeoff, but it multiplies the per-source cost this defect fails to bound.

**Fix (one-line class):** credit `transitive_ticks` per `dst` inside `reduce_source`'s candidate loop and
break there on a caller-supplied ceiling — i.e. make the row `budget_aware` and poll `k.work_budget` at
that point. That is kissat's exact arrangement.

### D2 — HIGH · Vivify's `thresh` (cadical `vivifythresh`=20) can never decline · shape (a)

`slot_entry_thresh_declines` (`default_policy.cpp:1559`) requires `e.doses > 0`. `e.doses` is incremented
in exactly two places: `run_budgeted_batch` (`:2015`, `if (spent > 0)`) and the `TACTIX_BVE_ONESHOT`
diagnostic (`:1776`). Vivify is routed to `run_vivify_dose` (`:2077`), which never touches `doses`. So
Vivify's `doses` is **0 for the whole solve** and the first conjunct short-circuits forever.

**Counter evidence.** Three entries carry a `thresh`: Factor 7, HyperTernaryRes 6, Vivify **20** — the
largest. Over 534 occasions × 11 instances the `bud` column (which for a batch entry counts *only* the
thresh decline, since the other budget pre-check is dead) reads:

| entry | thresh | `bud` declines |
|---|---|---|
| HyperTernaryRes | 6 | **258** |
| Factor | 7 | **92** |
| Vivify | **20** | **0** |

**Second half of the same defect.** Read at the use site, cadical passes the `THRESHHOLD` argument
**unconditionally `true`** for vivify (`vivify.cpp:1744`) and ternary (`ternary.cpp:371`); only *factor*
conditions it on the round count (`stats.factor`, `factor.cpp:962`). So `doses > 0` is Factor's semantics
applied to all three, and it additionally delays HyperTernaryRes's threshold by one occasion.

**Fix:** increment `e.doses` on the Vivify path (or key the conjunct on `e.applies > 1`), and restrict
the `doses > 0` conjunct to the Factor row.

### D3 — HIGH · the only mid-round bound on C1/C2 ships off, and BOTH references ship theirs on · shape (d)

This corroborates the already-found defect #2 with a fresh, shipped-configuration measurement and a
one-line-fix demonstration.

- kissat `forward.c:544`: `if (forward_steps > steps_limit) break;` inside the candidate loop, **no option
  gate**.
- cadical `subsume.cpp:459`: `if (stats.subchecks >= check_limit) break;` in the schedule loop, gated on
  `opts.subsumelimited` which defaults to **1**.
- ours (`strengthen.cpp:448`, `subsume_forward.cpp:205`): `k.midround_budget && k.work_budget && …`, and
  `Kernel::midround_budget = false` (`kernel.hpp:1075`). The batch pre-check `e.meter(k) > limit`
  (`default_policy.cpp:2035`) is **dead by construction** — `set_entry_limit` set `limit = meter + delta`
  two lines earlier with `delta ≥ MIN_EFFORT_TICKS/1000 × permille > 0` — and neither entry carries a
  `thresh`. **There is therefore no effort bound on a C1/C2 round at all.**

**Counter evidence**, `PRP_30_38`, all three arms of the same binary family:

| arm | Strengthen `handed` | `spent` | ratio | `cmpl/adm` | SubsumeForward ratio |
|---|---|---|---|---|---|
| shipped (`base` = `instr`) | 91,782,600 | 523,014,720 | **5.70×** | 2/30 | **2.49×** |
| `TACTIX_MIDROUND_BUDGET=1` | 111,601,800 | 111,604,146 | **1.00002×** | 0/33 | **0.82×** |

Aggregate over 11 instances: Strengthen **0.807× → 0.234×**, SubsumeForward **0.333× → 0.158×**;
`completed/admitted` for Strengthen 89/182 → 87/193.

**Correction to `benchmarks/reports/2026-08-04-midround-budget-negative.md`.** That report's *mechanism*
claim — "at 100 per mille the budget binds about once per solve, and one stopped round just defers its
candidates" — does not survive. Only **89 of 182 admitted Strengthen rounds finish inside their budget**
(`completed/admitted` 0.49; the rest either overspend or spend nothing), and on `PRP_30_38` **28 of 30
rounds fail to complete** while the round total overshoots by **5.70×**. That is not "about once per
solve". The PAR-2 verdict may stand on its own numbers; the reason given for it does not, and the row
should not be cited as "the budget rarely binds".

**Amplifier:** `SUBSUME_OCCLIM`/`SUBSUME_CLSLIM` and `STRENGTHEN_OCCLIM`/`STRENGTHEN_CLSLIM` are
1000/1000. That is **parity with kissat** (`subsumeocclim`/`subsumeclslim` = 1e3) but 10× cadical's
100/100 in each dimension, so one candidate is up to 10⁶ partner visits — which is why an unbounded round
can reach 5.7× rather than 1.05×.

### D4 — MED · Sweep has no `thresh`, and cadical's `sweepthresh`=5 is LIVE · missing governor

`SET_EFFORT_LIMIT (tickslimit, sweep, !opts.sweepcomplete)` (cadical `sweep.cpp:1890`) with
`sweepcomplete` defaulting to **0**, so the accumulate-until-worthwhile predicate is armed. §13.5
correction C2 already read this as LIVE, but no `EFFORT_SPECS` field or ledger closer followed.

**Counter evidence that it would bind:** Sweep is `bud 0` on **534/534** occasions, `sp/hd 1.008`, and
**`completed 0/534`** — every Sweep round is budget-cut and **none is ever deferred to accumulate a
worthwhile dose**, which is precisely the state `sweepthresh` exists to prevent. Fix: `thresh = 5` on the
Sweep row (one field). Note this is a *different* missing governor from `SP-SWEEP-2`'s delay ladder;
cadical has both, and forfeits the accrued budget on a *delayed* call while preserving it on a
*thresholded* one (`sweep.cpp:1885-1887` vs `limit.hpp:150`) — the two skips are opposite by design.

Correctly **not** ported, and now verified at the use site: `backbonethresh`=5 (`backbone.cpp:542` passes
literal `false`) and `probethresh`=0 (predicate degenerates to `DELTA < 0`).

### D5 — MED · Factor's first-dose allowance re-arms on any zero-spend occasion · shape (a)/(d)

`set_entry_limit` grants `factor_init_ticks_` (700 M) whenever `e.doses == 0`
(`default_policy.cpp:2342`); `e.doses++` fires only `if (spent > 0)` (`:2015`). Both references count
**calls**, not spending calls: kissat `INC (factorizations)` fires on every call past the two admission
gates and the allowance is `s->factorizations > 1` (`factor.c:1104-1113`); cadical
`if (!stats.factor) limit += opts.factoriniticks * 1e6;` immediately followed by `stats.factor++`
(`factor.cpp:961-969`).

**Counter evidence** — `b22_1`:

```
c slot Factor occ 51 adm 0 ok 0 | dec … dns 51 | handed 35700000000 spent 0 completed 0
```

`35,700,000,000 = 51 × 700,000,000` **exactly** — the once-per-solve allowance was granted on all 51
occasions. Two further instances show `49 × 700 M` and `9 × 700 M`. Today this is inert in effect (the
density decline short-circuits before any spend), but the mechanism means the first occasion on which
admission *flips* to true — arbitrarily late in the search — receives an effectively unbounded Factor
round rather than the preprocessing pass the allowance was sized for.

**Fix:** advance `e.doses` on the admitted path instead of on `spent > 0`.

### D6 — MED · Factor's density decline advances the anchor · shape (b)

`run_batch_entry` writes `e.anchor = search_ticks(k)` at `default_policy.cpp:2070`, **before** calling
`run_budgeted_batch` → `factor_dose_config`, where the `binary_ids ≥ 6·vars` admission decides. On a
decline it undoes the *telemetry* (`e.tm.admitted--; e.tm.dec_density++`, `:1904-1905`) but **not the
anchor**. Both references place every admission gate before `SET_EFFORT_LIMIT`, which is where the anchor
is written (kissat `factor.c:1088-1113`; cadical `factor.cpp:936-961`), so a declined occasion leaves the
window growing. This contradicts both `SP-SLOT-3` (recorded **CLOSED**: "anchor advanced only on the
ADMITTED path") and our own comment at `:2068`.

**Counter evidence:** **257 of 534** occasions are density-declined (`dns`); three instances decline
**100 %** of theirs (51/51, 49/49, 9/9), each having advanced the anchor and discarded the accrued
entitlement.

**Clean contrast worth recording:** BVE's identical density gate is **correct** — `bve_dose_declined`
returns at `:1623`, long before the anchor write at `:1782`.

**Fix:** move the density decision into `factor_entry_declines` (which already runs before the anchor
write), or move the anchor write below the rule config.

### D7 — MED · the ledger is unreadable for 6 of 15 entries, which is why D1 and D2 survived

`e.tm.handed`/`spent`/`completed` are written **only** in `run_budgeted_batch`; `e.tm.ok++` **only** in
`run_batch_entry` (`:2100`). So the shipped `c slot` line prints `handed 0 spent 0 completed 0` for
Probe, Backbone, TransitiveReduce, BVE (dosed), Vivify (budget-aware but on the vivify path) and
Substitute (unmetered), and `ok 0` for all four dosed entries **even when they apply**.

**Counter evidence** — `9vliw_bp_mc`, shipped binary:

```
c slot BVE              occ 18 adm 15 ok 0 | … | handed 0 spent 0 completed 0
c slot TransitiveReduce occ 42 adm 42 ok 0 | … | handed 0 spent 0 completed 0
c bve        0 rounds, 9802 eliminated, 2685218 resolutions
c transitive 0 rounds, 32275 checked, 1465 deleted, 46 failed, 10756130 ticks
```

With the instrumentation the same two lines read `BVE … ok 9802 | handed 25170700 spent 2685218
completed 15` and `TransitiveReduce … ok 973 | handed 9729280 spent 10756130 completed 0`.

`report_slot`'s own docstring says the line exists to separate "STARVED (declined for budget)" from
"IDLE (ran and found nothing)" and that "`spent/handed` is the one to read first". For six of fifteen
entries it can do neither. The five added lines that produced every `→` number in §1 left all nine batch
entries **byte-identical on 11/11 finishing instances**, so the fix is free.

### D8 — MED · Substitute has no effort governor at all · known row, now with a number

Already `D-SUBST-UNBOUNDED` + `SP-SUBST-2`. Counter: **213 admitted Substitute closures across 11
instances**, `handed 0 spent 0`, bounded only by the closure watermark (`mat 140`) and the delay ladder
(`dly 181`). kissat bounds the same rule twice — `substituterounds`=2 and a per-round break at
`Δsubstitute_ticks > 0.010 × (search_ticks − last.ticks.probe)` (`substitute.c:568-592`, hand-rolled, so
no `mineffort` floor). Reported here only to attach a measurement to an existing row.

### D9 — LOW · the effort window is structurally ZERO for 2 of every 3 occasions

`run_slot_phase` walks each entry `INPROCESS_PHASE_ROUNDS = 3` times, and `search_ticks` is deliberately
**frozen** for the phase's duration (`default_policy.hpp:785-791`, the monotonicity fix). The anchor is
advanced at the end of every admitted occasion, so outer rounds 2 and 3 compute `now − anchor == 0` and
are floored to `MIN_EFFORT_TICKS`, i.e. a constant `1e7/1000 × permille` regardless of how much search
preceded the phase.

**Counter evidence** — `DecompressReader_read_12`, all three probing-clock entries, 11 occasions each:

```
c slot HyperTernaryRes  occ 11 adm 11 | handed  880000   (= 11 × 80000  = 11 × 1e7/1000 ×  8)
c slot Probe            occ 11 adm 11 | handed 2200000   (= 11 × 200000 = 11 × 1e7/1000 × 20)
c slot TransitiveReduce occ 11 adm 11 | handed 2200000   (= 11 × 200000 = 11 × 1e7/1000 × 20)
```

Every occasion on that instance received **exactly** the `MIN_EFFORT_TICKS` floor — the per-mille of
search work never entered the computation at all.

**Reference, and a correction to SP-SLOT-3's reference column.** kissat writes its effort anchor **once
per phase, after the round loop** (`probe.c:86` `last.ticks.probe = search_ticks`; `eliminate.c:601`
`last.ticks.eliminate`) and `SET_EFFORT_LIMIT` reads *that* phase anchor (`kimits.h:141-142`) — so every
round of a kissat probing phase gets the **full** window. The per-technique anchor
(`last.NAME.ticks`) is **cadical's**, not kissat's; SP-SLOT-3 attributes it to both. Ours is therefore
*stricter* than kissat, not looser — the finding is that the advertised effort RATE is not the realised
one for two thirds of occasions, and that this is a scheduling choice nobody has made deliberately.

Related, not a defect: `INPROCESS_PHASE_ROUNDS = 3` against kissat `proberounds`=2 / `eliminaterounds`=2
and cadical's single `inprobe` pass + `elimrounds`=2 — our pipeline takes 1.5–3× the occasions per phase.

### D10 — LOW/latent · Factor's seed-heap build is unpolled and over-priced · shape (b)+(c)

The heap-fill loop (`factor.cpp:634-645`) calls `score_of` for every live literal, and `score_of` charges
`charge_factor_ticks(k, k.watches.heads[l].size)` = `1 + |watch|/32` factor_ticks (`:596`) for what is a
single O(1) `heads[l].size` read. The first `over_budget()` poll is **after** the loop (`:647`). kissat's
equivalent `schedule_factorization` (`factor.c:164-185`) charges **nothing** and reads `SIZE_WATCHES` in
O(1) as well.

**Honest status: not observed to bind.** Factor's aggregate spend is 4,355,971 against 183.5 G handed over
60 admits. The hazard is a formula where `live_lits + Σ|watch|/32` exceeds the floored
dose (`1e7/1000 × 50` = 500,000), i.e. roughly ≥ 250 k live literals with the touch filter cold; Tier B
does not reach it.

### D11 — LOW · two unsaturated, uncounted anchor subtractions remain outside the four ledger sites

`search_ticks(k) - walk_anchor` (`default_policy.cpp:1448`) and
`search_ticks(k) - mode.ticks_at_focus_entry` (`:128`). The wrap fix made the four
`set_entry_limit`/`ledger_remaining` sites saturating **with a `wrapped` counter**; these two have
neither. **I could not construct a reachable wrap**: both anchors are written outside an inprocessing
phase, `search_ticks` is monotone non-decreasing there, and `reset_schedule` zeroes `inprocess` *before*
re-basing (`:233-234`) so a second solve on the same kernel re-bases consistently — `T0 + S` never falls
below `T0`. The value of the one line is the counter, per the ledger's own rule that a wrap "must be
REPORTED, not merely guarded". Corroboration that the shipped four are sound: **`wrapped 0` on every one
of 15 entries × 12 instances × 4 arms.**

## 3. Failure-shape coverage

| shape | found | not found |
|---|---|---|
| **(a) unfireable** | **D2** (Vivify thresh, `doses` never advances); **D5** (Factor allowance re-arms); the `run_batch_entry` `meter > limit` pre-check (already documented dead in-source, no new row) | every other limit is computed against a counter that can reach it |
| **(b) mis-placed** | **D1** (TransitiveReduce, 35.7×); **D6** (Factor anchor before the admission); **D10** (Factor heap build) | Vivify, Sweep, BCE, CCE, Instantiate, HyperTernaryRes, Congruence, C1/C2 all poll **inside** the loop that spends — verified line by line against the reference's own poll site |
| **(c) mis-currencied** | **D1**'s second half (meter credited per source, so the policy's poll reads a stale meter) | all 14 metered entries' meter and in-rule poll read the **same** counter; Vivify/Instantiate poll `k.stats.ticks` but `tick_sink` is scoped to their private counter around the apply, so the deltas are equal by `kernel_charge_ticks` |
| **(d) inert-by-default** | **D3** (`midround_budget=false`); **D4** (Sweep `thresh` absent); **D8** (Substitute ungoverned) | `bce_occ_limit = 0` is inert but **deliberate and measured** (kernel.hpp:1036-1042 — cadical can afford `blockocclim` because it ships `block` off; we depend on BCE); `max_delta`/`active_floor` inert as `SP-EFFORT-CEIL-1` already records |

## 4. Governors verified CORRECT — do not re-audit these

A clean row is the point of the exercise. Each of the following was read at both use sites and, where
measurable, counted.

1. **Congruence's mid-round budget.** The `!(outp & 255)` extraction window plus seven closure polls hold
   a round to **max 1.02× / aggregate 0.463×** (`congini` arm, which strips the 1e12 opening allowance).
   The `base` arm's apparent overshoot was an artifact of `handed` including that allowance — a coarse
   read of the shipped ledger would have filed a defect here that does not exist.
2. **Congruence's opening allowance is granted exactly once per solve** — `handed = 1e12 + small` on
   every instance, i.e. `e.doses` *does* advance for Congruence (contrast D5's Factor).
3. **Sweep's in-expansion poll** (`SP-SWEEP-1`'s fix) — independently reconfirmed at **max 1.04× /
   aggregate 1.008×** on 11 instances rather than 4. The 73.2× / 598,573× overshoot is gone.
4. **Poll placement** for BCE (max 1.00×), Instantiate (1.03×), HyperTernaryRes (1.03×), Backbone
   (1.04×), CCE (1.11×), Probe (1.26×), Vivify (0.99×) — every one inside the spending loop, matching the
   reference's own site.
5. **Currency alignment** for all 14 metered rows (see §3 row (c)).
6. **Per-rule private meters are LIVE** — `c meters probe/backbone/vivify/instantiate` non-zero on all 11
   instances that reached probing (the single all-zero line is `hwmcc17miters`, refuted at conflict 1).
   `report_slot`'s own warning ("if they read zero, every rule budgeting on them is silently unbudgeted")
   does not fire.
7. **The `thresh` decline correctly does NOT advance the anchor** — `slot_entry_thresh_declines` returns
   before the anchor write, exactly as cadical writes `last.NAME.ticks` *after* the threshold check
   (`limit.hpp:150-158`). Nor does it clobber the entitlement: `e.limit` is recomputed from the
   un-advanced anchor next occasion.
8. **`backbonethresh` and `probethresh` are correctly not ported** — verified at the use site as inert
   (literal `false`; and `DELTA < 0`).
9. **Occurrence/size limits match a reference exactly**: BVE 2000/100 = kissat `eliminateocclim`
   2e3 / `eliminateclslim` 100; Instantiate 1/3 = cadical `instantiateocclim` 1 / `instantiateclslim` 3;
   C1/C2 1000/1000 = kissat `subsumeocclim` / `subsumeclslim` 1e3 (cadical's 100/100 is the stricter of
   two disagreeing references — a choice, not a defect, though see D3's amplifier).
10. **BVE's density decline does not advance the anchor** (contrast D6).
11. **BVE's effort budget is INERT but not defective** — `sp/hd 0.015`, max 0.11×, **`completed 60/60`**:
    every BVE dose runs its whole schedule inside budget. The binding governors are the density admission
    (`dns 96/182`) and the retry / `bve_min_bound` filters, **not** the 100 ‰ resolution dose or the 2e9
    ceiling. Same for Vivify (max 0.99×) and Factor (0.00×). Record this so a flat BVE stat is not read
    as budget starvation.
12. **The ledger-wrap fix holds** — `wrapped 0` across 15 entries × 12 instances × 4 arms.
13. **`clamped 0 / floored 0`** everywhere, as `SP-EFFORT-CEIL-1` already recorded; re-confirmed on 11
    instances rather than 3.

## 5. Suggested order of work

1. **D1** — the only governor overshooting by more than 6×, and the fix is kissat's literal arrangement.
2. **D7** — five lines, and without them D1-class defects on the four dosed entries are invisible. Do
   this *first* if anything is to be measured afterwards.
3. **D2** — one conjunct plus one increment; a 20 ‰-of-clauses threshold currently does nothing.
4. **D3** — not a code change but a decision: both references poll mid-round unconditionally, and the
   report that banked the negative states a mechanism the counters contradict.
5. **D4**, **D5**, **D6** — one field / one moved increment / one hoisted call each.
6. **D9** — a scheduling decision (per-occasion vs per-phase anchor), not a bug fix; needs a tier pair.
7. **D8**, **D10**, **D11** — robustness; expected yield ≈ 0 (D8's row already says so).

## 6. Reproduction

```
benchmarks/probes/audit_dosing_slot.sh        # the probe (array task; CACHE= for a worktree)
benchmarks/probes/audit_dosing_instances.txt  # the 12 Tier B instances
benchmarks/probes/audit_dosing_summarize.py   # the §1 table, incl. worst-instance sp/hd
benchmarks/probes/audit_dosing_identity.sh    # base-vs-instr batch-entry identity check
```

```
sbatch --partition=cpu-dense-hwulab-q --ntasks=1 --cpus-per-task=1 --mem-per-cpu=8G \
       --time=00:12:00 --job-name=audit-dose --array=0-11 --output=/dev/null \
       --export=ALL,CACHE=$REPO/benchmarks/cache,TAG=base \
       --wrap "benchmarks/probes/audit_dosing_slot.sh $PWD/build/pin-X $OUTDIR"
python3 benchmarks/probes/audit_dosing_summarize.py $OUTDIR
```

The `instr` arm needs the five-line D7 instrumentation, which is **not** in the tree (reverted; `ctest`
47/47 on the reverted build). Its diff is described exactly by D7: `e.tm.handed` / `e.tm.spent` /
`e.tm.completed` around the dose in `run_dosed_entry`, the same three in `run_vivify_dose`, and
`e.tm.ok++` on the dosed apply path.
