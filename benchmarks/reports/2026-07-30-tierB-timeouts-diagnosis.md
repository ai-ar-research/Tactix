# The four Tier B instances we cannot solve — diagnosis

**Date:** 2026-07-30 · **debug-performance ladder, steps 0–4** on the four
timeouts of window `20260729-220951` (`veer_axi…p23_transition`,
`or_randxor_k3_n540_m540`, `mp1-klieber2017s-0500-023-t12`,
`unsat-set-b-fclqcolor-10-07-09`) plus the two near-misses (`DivS_862_11`,
`2013113162201nw`).

**Shape:** SLURM `cpu-dense-hwulab-q`, 1 core / 8 GB per task, four job arrays —
**1574764** (140 tasks, %36), **1574882** (36, %20), **1574949** (16, %16),
**1575173** (17, %17) — 209 solver runs in total.

**Evidence.** Arm definitions: `benchmarks/probes/tierB-timeouts{,2,3,4}.{sh,mf}`
(+ `.sbatch`). Every arm's one-line result and the full statistics of the arms
cited here are collated into
`benchmarks/probes/tierB-timeouts-1574764.out` by
`benchmarks/probes/tierB-timeouts-collate.py`; the 209 per-task raw files are
scratch and are not committed (slurm-jobs retention rule). Every number quoted
below was machine-checked against the raw output before this report was written.

**Binary provenance.** Every tactix arm ran `build/pin-final/tactix` — the exact
copy that produced `20260729-220951` (git `77fc578`, mtime 21:51 < run 22:09),
*verified before any arm was read*: `sanity-mod4block` = **13.52 s /
418,796 conflicts** against the known baseline 12.79 s / 418,796. (HEAD has since
moved to `75b6308`, which adds the X2 governors; §6 shows they are inert here.)
Diagnostic arms do **not** write proofs; §5 prices that at ±2 %.

> **This is a diagnosis, not a change.** Nothing is proposed as a default. Every
> rule-off arm below is a *diagnostic for a scheduling defect* — the standing
> user directive is that every implemented proof rule ships ON — and §8
> pre-registers the experiments that would turn any of it into a landed change.
> Per plan §8.1 nothing here was tuned on these instances.

---

## 0. Verdict in one page

| instance | dominant term | is it the same defect? | cheapest measured fix |
|---|---|---|---|
| `mp1-klieber2017s` | `TransitiveReduce` trajectory damage — **823 binary deletions cost 15× the conflicts**, its own time is 1.7 % — plus no elimination | **A** | force-BVE **390.5 → 42.5 s**; `−TransitiveReduce` **→ 18.4 s**; all three levers → **12.97 s = kissat parity** |
| `2013113162201nw` (near-miss) | `HyperTernaryRes` trajectory damage (own time: 0.06 %) | **A** | `−HyperTernaryRes` **75.4 → 1.4 s** |
| `veer_axi…p23` | `TransitiveReduce` **own runtime, 26–49 % of the clock** (8.7 × 10⁹ ticks; kissat spends 0.03 s there) + no elimination + a large unexplained residual | **A + open** | force-BVE **2,200 → 550 s**; all three → **387 s** (still over the 300 s cap). Shipped default **does** solve it — in **2,199.81 s**, i.e. 687× kissat |
| `or_randxor` | search-tree size; **BVA on non-binary clauses is missing**. The only true wall: **unsolved at 3,600 s, 42.5 M conflicts = 69× kissat's 617 k**, at engine parity (props/conflict 0.99×) | **B** | none — 20 of 20 arms are TO |
| `unsat-set-b-fclqcolor` | search-tree size; same missing BVA | **B** | none — best arm 241 s |
| `DivS_862_11` (near-miss) | `Strengthen`+`SubsumeForward` **own runtime, 98.2 %** | **C** | `−all rules` **123 → 9.2 s** |

**A = the inprocessing slot's dosing is calibrated on binary-dense formulas** —
two rules that should shrink the formula (BVE, Factor) are gated off by a binary
density threshold of 6, and two rules that should barely run
(`TransitiveReduce`, `HyperTernaryRes`) are metered in currencies that
under-price them by orders of magnitude. **B = a rule-SCOPE gap**: our Factor is
binary-clause-driven; kissat's `factorsize` default is 5, and restricting kissat
to *our* variant loses rxor entirely and costs fclq 12.6×. **C = the only two
slot entries with no mid-round budget.**

**Our search is not the problem.** On rxor, klieber and fclq our conflict count
sits on top of the baseline stripped of exactly its load-bearing pass; on fclq we
beat CaDiCaL outright (369 s vs TO). The residual engine term is ~1.8×.

---

**The facts being diagnosed** (`20260729-220951`, tactix `102.28` PAR-2 at 24/28
vs kissat `9.54` at 28/28, 300 s cap, both arms writing and checking DRAT,
throttle 48):

| instance | tactix | kissat | ratio |
|---|---|---|---|
| `veer_axi…p23_transition` | TO (300 s) | 3.20 s | **93.6×** |
| `or_randxor_k3_n540_m540` | TO | 12.38 s | **24.2×** |
| `mp1-klieber2017s-0500-023-t12` | TO | 12.54 s | **23.9×** |
| `unsat-set-b-fclqcolor-10-07-09` | TO | 16.46 s | **18.2×** |
| `2013113162201nw` | 72.64 s | 6.00 s | 12.1× |
| `DivS_862_11` | 115.47 s | 16.47 s | 7.0× |

(The 187×/48×/48×/36× figures in the resume block are PAR-2 ratios — `2 ×`
timeout over the reference second. The wall ratios against the 300 s cap are
above; the conclusions are the same either way.)

## 1. What the four instances are

| instance | st | vars | clauses | bin % | tern % | **bin/var** | occ/var | family |
|---|---|---|---|---|---|---|---|---|
| `veer_axi…p23_transition` | U | 521,158 | 1,306,928 | 36.9 | 63.1 | **0.93** | 6.6 | HW model checking (yosys/VeeR AXI transition relation); **pure 2/3-clause** gate encoding |
| `or_randxor_k3_n540_m540` | U | 1,080 | 7,287 | 0.0 | 29.3 | **0.00** | 27.0 | random 3-SAT ∨ random XOR; clause sizes 3–6, **no binaries at all** |
| `mp1-klieber2017s-0500-023-t12` | S | 30,587 | 91,703 | 66.7 | 33.3 | **2.00** | 7.0 | Klieber encoding; **exactly 2 binaries + 1 ternary per gate** (AND-gate Tseitin) |
| `unsat-set-b-fclqcolor-10-07-09` | U | 205 | 4,549 | 21.4 | 78.1 | **4.76** | 62.3 | clique-colouring; tiny and dense |
| `2013113162201nw` (near-miss) | S | 5,029 | 19,934 | 0.0 | **99.7** | **0.00** | 11.9 | all-ternary (sat03 reshuffle) |
| `DivS_862_11` (near-miss) | S | 862 | 855,716 | 0.0 | 0.0 | **0.00** | **10,889** | 855k clauses of length 11 over 862 variables |

Structural similarity: **veer and klieber are the same shape** (pure
binary+ternary gate encodings); **veer, fclqcolor and nw are ternary-heavy**;
**rxor, nw and DivS contain literally zero binary clauses**.

### The selection signature, over the whole tier

Sorting all 28 Tier B instances by binary density and marking our ratio to
kissat in `20260729-220951`:

| binaries/var | instances | our worst ratio | our median ratio |
|---|---|---|---|
| **< 6** (18 instances) | contains **all four timeouts and both near-misses** | **93.6×** (veer) | 2.55× |
| **≥ 6** (10 instances) | battleship, 9vliw, x9, DecompressReader, clique, qwh, oddball, SCPC, rbsat, mp1-blockpuzzle | **2.2×** (rbsat) | **0.75× (we win)** |

Six is not an arbitrary cut: it is the threshold of **both** of our elimination
admission gates — `factor.cpp:206` (Factor short-circuits below 6 binaries/var)
and `default_policy.cpp:940` (the during-search BVE dose is declined below 6),
and BVE additionally never runs at preprocessing (`defer_preprocess`, S4). So
**on every one of these six instances tactix performs zero variable elimination
and zero BVA at the shipped default**, while kissat eliminates 40 % (nw), 43 %
(veer), 53 % (fclq), 86 % (klieber) of the variables and 1,257 on rxor's
1,080-variable formula (cumulative over rounds, hence kissat's ">100 %").

Honest caveat: all four timeouts falling in the sparse group is
*p ≈ 0.05* under a random assignment (C(18,6)/C(28,6)), so the ranking alone is
suggestive, not decisive. What makes it a diagnosis rather than a correlation is
that the mechanism is named, mechanistic and directly testable — and §5a tests
it.

---

## 2. Step 0–2 — quantify and decompose

`c timing` gives the slot share directly; **search = solve − slot**. tactix
prints full statistics on the SIGTERM path, so a capped arm is still a full
decomposition.

**Three of the four are a constant factor; one is a wall.** At a 3600 s cap the
shipped default **solves veer in 2,199.81 s** (UNSAT, 137,637 conflicts) —
**687× kissat's 3.20 s**; klieber (390.54 s) and fclq (368.93 s) already solve
inside 600 s. **`or_randxor` is the exception and it is qualitatively
different: still UNSOLVED at 3,600 s after 42,481,210 conflicts** — **69× the
617 k conflicts kissat needs** — with the slot at 1.9 %, i.e. 98 % of an hour
spent in a search that is not converging. It is also unsolved at 600 s under
**all twenty** configurations tried (10.7–12.2 M conflicts every time). That is
consistent with §5a′: without BVA over non-binary clauses the formula is simply
not the one the references search, and kissat itself is unsolved at 300 s once
`--factorsize=2` restricts it to our variant.

| instance | tactix | conflicts | props/conf | dec/conf | **slot %** | kissat | conflicts | props/conf | cadical |
|---|---|---|---|---|---|---|---|---|---|
| veer | **2,199.81 s** (600 s cap: TO) | 137,637 (@600: 18,424) | **71,367** | **4,213** | **30.9 %** (@600: 54.2 %) | 3.37 s | 2,967 | 2,002 | 4.03 s / **3 conflicts** |
| rxor | **TO @3600** | 42,481,210 (@600: 10,794,198) | 62.6 | 2.08 | 1.9 % | 12.27 s | 616,974 | 63.5 | 17.91 s |
| klieber | 390.54 s | 646,204 | **6,416** | 1.94 | 4.3 % | 12.29 s | 64,149 | 2,308 | 65.35 s |
| fclq | 368.93 s | **15,288,656** | 27.8 | 1.87 | 2.7 % | 16.16 s | 918,618 | 26.3 | **TO @300** (12.2 M) |
| nw | 75.42 s | 711,461 | 623 | 2.14 | 2.9 % | 5.97 s | 107,093 | 358 | 19.07 s |
| DivS | 123.07 s | 14,733 | 6.2 | 24.7 | **99.0 %** | 16.81 s | 163,413 | 15.2 | 5.34 s |

**The localized dominant term, per instance:**

| instance | dominant term | evidence |
|---|---|---|
| **veer** | **`TransitiveReduce`'s own runtime — 26–49 % of the clock** (585.7 s of the 2,200 s solve; 292.9 s of one 600 s window and 152.9 s of another at the same 8.7 × 10⁹ ticks, so read the ticks, not the seconds), *plus* a search that needs 71,367 props/conflict | `c timing TransitiveReduce 585653 ms (26.6 %) in 1,298,245 applications` (full solve); `c transitive … 8,719,976,746 ticks` over 998,760 candidates = **8,731 ticks each**; **kissat spends 0.03 s (0.91 %) on transitive on this instance** |
| **rxor** | **search-tree size**, nothing else. Slot 1.9 %, props/conflict **0.99× kissat** (62.6 vs 63.5 — engine parity to within 1 %) | **42.48 M conflicts at 3,600 s and still unsolved**, against kissat's 617 k; 20 of 20 configurations behave identically |
| **klieber** | **`TransitiveReduce` trajectory damage** (its own time is 1.7 %) + a formula never eliminated (props/conflict 2.78× kissat) | `−TransitiveReduce` = **18.37 s** (21×), 42,174 conflicts |
| **fclq** | **search-tree size** (16.6× conflicts) at engine parity (props/conf 1.06×, conflicts/restart 38 vs 43) | slot 2.7 %; best rule-off arm only 241 s |
| **nw** | **`HyperTernaryRes` trajectory damage** (its own time is 46 ms = 0.06 %) | `−HyperTernaryRes` = **1.42 s** (53×), 22,700 conflicts vs 711,461 |
| **DivS** | **`Strengthen` + `SubsumeForward` own runtime — 98.2 % of the clock** (109.1 s + 11.4 s in 4 + 5 applications) | `−all rules` = **9.23 s** (13×) |

---

## 3. Step 3 — rule out a missing rule (baseline ablation)

**Method warning honoured:** kissat's `probe` is an **umbrella** (`probe.c`:
congruence → substitute → backbone → vivify → sweep → transitive → backbone →
factor). `--no-probe` therefore isolates nothing; it is reported only as a
family bound. Each instance's *named* load-bearing pass is below.

| instance | kissat default | the pass that carries it | other passes |
|---|---|---|---|
| veer | 3.37 s | **`--no-congruence` → 182.56 s (54×, 2.65 M conflicts)**; **cadical `--no-decompose` → 6.86 s**, **cadical `--no-congruence` → TO @300 (1.84 M conflicts)** | sweep, factor, eliminate, vivify, definitions: **free** (3.3–3.5 s). `--no-substitute` 7.8 s, `--no-preprocess` 10.5 s |
| rxor | 12.27 s | **`--no-factor` → TO @300 (9.77 M conflicts, >24×)** | sweep 14.6, congruence 11.1, eliminate 13.7, vivify 13.9, substitute 11.4, definitions 10.4; **`--no-preprocess` is FASTER (9.28 s)** |
| klieber | 12.29 s | **`--no-eliminate` → TO @300 (564 k conflicts, >24×)** | vivify 94.9, sweep 33.6, congruence 37.9, factor 37.8, definitions 49.0, preprocess 27.3 |
| fclq | 16.16 s | **`--no-factor` → 192.02 s (12×, 13.7 M conflicts)** | sweep 16.0, congruence 20.1, eliminate 21.1, vivify 20.8, substitute 18.8, definitions 15.9 |

**There is no *single* shared missing rule.** The load-bearing pass is different
on every instance — congruence, factor, eliminate, factor — and we implement all
four families (Congruence V6, Factor E2, BVE E1). The closest thing to a shared
missing capability is **Factor**, which carries two of the four outright and
contributes on both near-misses (§5a′).

Two asymmetries worth recording:

* **On veer, kissat needs neither elimination nor factoring** (`--no-eliminate`
  3.52 s, `--no-factor` 3.53 s, both at the *same* 2,967 conflicts) — its
  congruence closure alone shrinks the problem. **We do need elimination there**
  (§5a), because our congruence merges but the formula it leaves behind is never
  eliminated.
* **L11 survives:** `--no-sweep` costs kissat 0.0–2.7× here; Sweep is not the
  lever on any of the four, and neither is definitions (10.4–49 s, never
  load-bearing).

---

## 4. Step 4 — is our search competitive? Yes, on three of four

Compare our conflict count with the **baseline stripped of exactly that pass**:

| instance | tactix conflicts | baseline-minus-its-pass | verdict |
|---|---|---|---|
| rxor | 10.79 M @600 s, **42.48 M @3600 s** (unsolved) | kissat `−factor` 9.77 M (TO @300) | **parity** — our search ≈ kissat without Factor, and neither converges |
| klieber | 646,204 (solved, 390 s) | kissat `−eliminate` 563,721 (TO @300) | **parity** |
| fclq | 15.29 M (solved, 369 s) | kissat `−factor` 13.73 M (192 s); kissat all-off 14.51 M; **cadical 12.25 M, TO @300** | **parity — and we BEAT CaDiCaL here** |
| veer | 18,424 (TO @600) | cadical `−congruence` 1.84 M (TO @300) | **not comparable**: we make ~100× *fewer* conflicts but each costs 51× more propagation. Distinct mechanism (§5) |

So on rxor / klieber / fclq the cardinal debug-performance reading holds: **our
core search is competitive; the entire gap is simplification that does not
happen.** The residual engine term is ~1.8× (fclq: 41.4 k conflicts/s vs kissat's
56.8 k) — real, but an order of magnitude smaller than the gap.

---

## 5. Is there a shared cause? Yes for two of the four — one defect class in the slot's dosing

### 5a. Passes that must run, don't — the binary-density admission gate

`TACTIX_BVE_DOSE_DENSITY=0` (the gate removed, everything else shipped):

| instance | shipped | **force BVE** | eliminated | effect |
|---|---|---|---|---|
| **veer** | 2,199.81 s (TO at a 600 s cap) | **550.36 s, UNSAT** | — | **4.0×**; props/conflict at 600 s **101,854 → 38,240** |
| **klieber** | 390.54 s | **42.50 s** | **26,845** (kissat: 26,345) | **9.2×**; props/conflict 6,416 → 1,092 |
| **nw** | 75.42 s | **35.69 s** | 2,938 | 2.1× |
| fclq | 368.93 s | 343.01 s | — | 1.08× |
| rxor | TO | TO | 0 | none (no binaries to seed on) |
| DivS | 123.07 s | 138.37 s | 0 | none |

**Not one of the six regresses, two are transformed, and veer is 4× faster.**
`TACTIX_FACTOR_DENSITY=0` is much weaker (veer TO @600, klieber
363.7 s, nw 82.5 s, fclq 505.6 s) — and the reason is not the gate.

### 5a′. Our Factor is binary-clause-driven; kissat's `factorsize` default is 5

`factor.hpp:1` calls E2 "the **binary-clause-driven** variant"; `binary_partners`
walks only binary watches. kissat's `factorsize` default is **5**. Restricting
kissat to *our* variant (`--factorsize=2`) is therefore the decisive
like-for-like test, and it is unambiguous:

| instance | kissat default | **kissat `--factorsize=2`** | kissat `--no-factor` | tactix |
|---|---|---|---|---|
| **rxor** | 12.27 s / 617 k | **TO @300, 10.02 M conflicts** | TO @300, 9.77 M | TO @3600, 42.48 M |
| **fclq** | 16.16 s / 919 k | **203.94 s, 14.30 M** | 192.02 s, 13.73 M | 368.93 s, 15.29 M |
| **klieber** | 12.29 s / 64 k | **95.14 s, 706 k** (`factored` 723 → 1) | — | 390.54 s, 646 k |
| **nw** | 5.97 s / 107 k | **17.41 s, 330 k** | — | 75.42 s, 711 k |

CaDiCaL agrees on rxor: `--no-factor` → **TO @300, 7.36 M conflicts** (default
17.91 s). **Both references lose rxor entirely without non-binary BVA, and our
conflict counts sit exactly on top of theirs once it is removed.**

So on rxor and fclq the cause is a **rule-scope gap**, not a dosing one: no
gate, dose or schedule of our Factor can fire on a formula with zero binary
clauses (rxor, nw), and on fclq the binary-only variant reproduces kissat's
`--no-factor` number to within 6 %.

### 5b. Passes that should barely run, dominate — meters in the wrong currency

Every one of these rules costs *nothing measurable in its own right* on the old
Tier B, which is why none of it was visible before yesterday.

| rule | meter | what the meter counts | what one unit actually costs |
|---|---|---|---|
| `TransitiveReduce` | `transitive_checked` | one **candidate edge** | a **full BFS of the binary implication graph, with no early exit** (hpp deviation 3) — on veer **8,731 ticks per candidate** |
| `HyperTernaryRes` | `ternary_ticks` | occurrence-list entries | output, not work: 56,291 redundant ternaries added to a 19,872-clause formula (nw) |
| `Strengthen`, `SubsumeForward` | `strengthen_checked` / `forward_checked` | checks, **post hoc only** | these are the **only two slot entries that are neither `dosed` nor `budget_aware`** — one round cannot be cut, whatever the ledger says (DivS: 27 s per application) |

**And for `TransitiveReduce` the mismatch is not merely a mis-pricing — it makes
the budget structurally unable to bind.** The ledger computes

```
e.limit = e.meter(k) + reference / 1000 * e.permille;   // default_policy.cpp:866
```

where `reference = search_ticks(k) - e.anchor` is in **search ticks** (10⁸–10⁹
between phases) while `meter_transitive` returns **candidate edges** (10⁴–10⁶ in
total). At 20 ‰ the allowance is ~4 orders of magnitude larger than the whole
candidate set. **Measured, not inferred:** at
`TACTIX_SLOT_PERMILLE=TransitiveReduce:1` — a **twentyfold** cut — the search is
unchanged on the instances that matter here (array 1575173; wall times marked †
are cross-window against the wave-1 shipped run and are *not* a controlled
comparison — read the counters, not the seconds):

| instance | shipped: conflicts / candidates / V4 ticks | `TransitiveReduce:1` | `:5` |
|---|---|---|---|
| nw | 711,461 / 90,614 / 7.12 M | **711,461 / 90,614 / 7.12 M** (identical) | — |
| klieber | 646,204 / 1,490,804 / 844 M | **646,204** / 1,355,069 / 780 M | **646,204** |
| veer (@600 s) | 998,760 candidates / **8.720 G** ticks | 989,559 / **8.734 G** (identical work) | — |
| mod4block (in-window control) | 418,796 / — | **418,796**, 13.00 s vs 13.01 s | — |

On nw the arm is **bit-identical** — same conflicts, same candidates, same
ticks. On veer a twentyfold cut leaves the tick count *higher*. On klieber it
removes 9 % of the candidates and leaves the conflict count **exactly**
unchanged. **The rate is not a control** — a tick-scaled allowance is orders of
magnitude beyond anything a candidate counter can reach, so the ledger's
decision is the same at 1 ‰ and at 20 ‰.

*(Measurement note worth carrying: on veer the two 600 s windows charge the
**same** 8.7 G V4 ticks at 292.9 s and 152.9 s of wall. V4's work is invariant;
its wall share is not. Every conclusion here rests on the tick and conflict
counters, never on a single wall number.)*

**And the rate test isolates V4's two costs, which are different things.** On
veer the cost is V4's own *work* (8.7 G ticks). On klieber it is V4's *output*:
shipped and `:1` both delete **exactly 823** binaries (of 61,138) and both take
**exactly 646,204** conflicts, while `−TransitiveReduce` deletes none and takes
**42,174**. So **823 redundant-binary deletions cost klieber 15× its conflict
count** — an L3-class effect (a logically free deletion is not a
trajectory-free one), and one that a *work* budget only fixes to the extent
that it also cuts deletions. This materially weakens R1's prediction for
klieber; see §8/R1.

For contrast, kissat sets the same limit with `SET_EFFORT_LIMIT (limit,
transitive, transitive_ticks)` — currency `transitive_ticks`, the **same unit
family as the reference** — and checks it *inside* the walk (`transitive.c:181`,
within `transitive_reduce`), so a single source cannot overrun. We check only
between candidates, and our unit cannot bind. Three concrete divergences from the
solver we transliterated: wrong currency, no in-walk check, no early exit.

**The same test says the rate is the wrong control for X2 as well** — but for a
different reason. `HyperTernaryRes` *is* metered in a tick currency, so its rate
does bind: at cadical's `ternaryeffort=8‰` (against our 20) nw goes 75.42 →
**41.79 s** and klieber 390.54 → **261.77 s**. But `−X2` gives 1.42 s and
62.95 s. The rate throttles *work*; X2's damage is *output* (rounds and
resolvents), which is why cadical bounds it with `ternaryrounds=2` and
`ternarymaxadd` as well.

Single-rule-off diagnostics (600 s cap; **read as a scheduling signal, never as a
proposed default**):

| instance | shipped | −TransitiveReduce | −HyperTernaryRes | −Strengthen | −Vivify | −all rules |
|---|---|---|---|---|---|---|
| **klieber** | 390.54 | **18.37 (21×)** | 62.95 (6.2×) | 140.20 | 177.17 | 327.06 |
| **nw** | 75.42 | not run (rate arm only) | **1.42 (53×)** | 51.49 | 10.89 | 378.06 |
| **veer** | TO (18,424 conf) | TO, but slot 54.2 % → **5.4 %**, conflicts ×2.2 | TO | 593.11, **UNSAT — solves** | TO | TO (296 k conf) |
| **fclq** | 368.93 | 352.31 | 360.56 | 356.59 | 336.02 | 241.22 |
| **rxor** | TO | TO | TO | TO | TO | TO |
| **DivS** | 123.07 | 128.75 | 128.75 | 151.13 | 135.39 | **9.23 (13×)** |

Combining the two levers (array 1574949) — and note it is **not additive**:

| arm | **veer** | klieber | nw | fclq |
|---|---|---|---|---|
| shipped | 2,199.81 (TO at a 600 s cap) | 390.54 | 75.42 | 368.93 |
| force-BVE | 550.36 **UNSAT** | 42.50 | 35.69 | 343.01 |
| force-BVE + −TransitiveReduce | **394.61 UNSAT** | 46.58 | — | 346.70 |
| **force-BVE + −TR + −X2** | **387.46 UNSAT** | **12.97 s, 85,489 conflicts** (kissat 12.29 / 64,149) | — | 337.17 |
| −X2 alone | TO | 62.95 | **1.42** | 360.56 |
| force-BVE + −X2 | — | — | **52.38** | — |

On klieber the three together land on **kissat parity** (12.97 vs 12.29 s, and
1,706 props/conflict against kissat's 2,308). On veer they take it from
unsolved to **387 s** — real, but still above Tier B's 300 s cap and 115× kissat,
so veer is only *partly* explained by the slot. On nw they *fight*: `−X2` alone
is 1.42 s but `force-BVE + −X2` is 52.38 s. **Treat these as a family, never as
independent additive knobs (L15).**

Two readings that matter:

* **On klieber, turning off ONE rule (21×) beats turning off ALL of them (1.2×).**
  That is the cleanest possible statement that the problem is *scheduling*, not
  the rule inventory: the other twelve rules are worth having, and
  `TransitiveReduce` is actively poisonous there.
* On DivS the pair is non-separable: `−Strengthen` (151 s) and
  `−SubsumeForward` (145 s) each let the other consume the slot; only
  `−everything` (9.2 s) collapses it. Same shape as **L15**. Cutting their
  *rate* to 1‰ (`TACTIX_SLOT_PERMILLE=Strengthen:1,SubsumeForward:1`) gives
  **92.21 s** — the rate can decline the second and later rounds but cannot
  interrupt the first, which is exactly what "no mid-round budget" means.
* `Vivify` is a third over-dosed entry on this corpus: nw `−Vivify` **10.89 s**
  (6.9×), klieber `−Vivify` 177.17 s (2.2×). It *is* `budget_aware`, so the
  defect there is dose size, not the absence of a budget.

### 5c. So: shared cause — stated precisely

> **The defect is that the inprocessing slot's admission and metering were
> calibrated on binary-DENSE formulas.** On binary-sparse ones the two rules
> that would shrink the formula (BVE, Factor) are switched off by a density
> gate, and the cheap-probing-family rules whose meters under-price them
> (`TransitiveReduce`, `HyperTernaryRes`) run essentially unbounded — burning the
> clock (veer, 48.8 %) or the trajectory (klieber 21×, nw 53×). Both directions
> are the *same* defect: the ledger does not know what these rules cost or buy
> on this class of formula.

**Which of the four are the same defect, and which are distinct** — the answer
the task asked for:

| instance | same defect? |
|---|---|
| **klieber** and **nw** (near-miss) | **SAME defect**, twice: an over-dosed cheap-probing-family rule (TR / X2) on top of a suppressed BVE. Each is fixed by a single existing knob (18.4 s, 1.4 s). |
| **veer** | **SAME defect + a large unexplained residual.** TR is 48.8 % of its clock (klieber's mechanism in its self-time form) and forcing BVE alone already takes it from unsolved to 550 s. But the best of every combination is **387 s** — solved, yet still 4× over Tier B's cap and 115× kissat. The extra term is scale (521 k variables, 101,854 props/conflict) and it is **not localized** by this report (§9.1). |
| **rxor** and **fclq** | **DISTINCT from the other two, and the SAME as each other — a rule-SCOPE gap.** Both need BVA on **non-binary** clauses (kissat `factorsize=5`); restricting kissat to our binary-only variant costs it rxor entirely and fclq 12.6×, and CaDiCaL loses rxor too. rxor has **zero** binaries, so no dose, gate or schedule of our Factor can ever fire; no arm of the 20 we ran moves it (10.7–12.2 M conflicts at 600 s, all TO; 42.48 M at 3,600 s, still TO). fclq additionally carries a ~1.8×-per-conflict residual and CaDiCaL times out on it as well. |
| **DivS** (near-miss) | **DISTINCT — the unbounded-round defect**: `Strengthen`/`SubsumeForward` have no mid-round budget. |

---

## 6. What was ruled OUT, and how

| ruled out | evidence |
|---|---|
| **Proof writing** | veer TO both ways; klieber **390.54 → 385.54** with DRAT; fclq **368.93 → 376.65**. ±2 %, both signs. Not the cause. |
| **Raw engine speed / BCP** | props/conflict vs kissat: rxor **0.83×**, fclq **1.06×**, nw 1.74×. conflicts/restart fclq 38 vs kissat 43. The two large props/conflict ratios (klieber 2.78×, veer 51×) *shrink with elimination* (klieber 6,416 → 1,092 under force-BVE), i.e. they are a property of the un-simplified formula, not of the engine. |
| **A single shared missing rule** | the load-bearing pass differs per instance (§3): congruence / factor / eliminate / factor. (Factor is load-bearing on **two** of the four, which is the closest thing to a shared missing capability — see §5a′.) |
| **Sweep (V5/R5)** | `--no-sweep` costs kissat 0.0× (veer 3.28), 1.19× (rxor), 0.99× (fclq), 2.7× (klieber, where eliminate is >24×). **L11 upheld** — do not re-open Sweep on this evidence. |
| **Our Congruence being weak on veer** (my first hypothesis — refuted) | `TACTIX_CONG_TRACE`: round 1 extracts **459,940 gates and adds 161,462 equivalences**, against kissat's 195,888 congruent variables. V6 is doing its job. (It *does* re-extract 384,970 gates five more times for 4,008 equivalences each — waste worth 14 s, not the defect.) On klieber, congruence adds **0** equivalences in 8 rounds and kissat's `congruent` is 73 — correctly irrelevant on both sides. |
| **The X2 governors that landed at HEAD (`75b6308`)** | `ternary_occ_limit=100`, `ternary_max_add_percent=1000` (cadical's defaults). On nw X2 adds 56,291 clauses to a 19,872-clause formula = **283 %**, under the 1000 % bound, and its occurrence lists are ~6 per literal, under 100. **Both governors are inert on exactly the instance X2 destroys.** The binding cadical knobs would be `ternaryrounds=2` (we run **25 rounds** on nw) and `ternaryeffort=8‰` (we use 20‰). |
| **The pinned-binary hazard** | sanity arm reproduced mod4block at 13.52 s / 418,796 conflicts before any arm was read. |
| **A `$TMPDIR` collision** | wave 1 was re-submitted after four tasks died on a shared scratch path; every arm since stages to `$TMPDIR/tbto*-$SLURM_ARRAY_TASK_ID/`, and every arm's `p cnf` header and verdict were checked against `B.list`. |

---

## 7. The uncomfortable methodological finding

**`meter_transitive` counting candidates instead of ticks is a *documented,
deliberately-chosen* deviation**, and the honest-currency alternative was
measured and **banked negative on 2026-07-25** (`default_policy.cpp`:
"ticks-meter @ kissat's 20‰ = 12.22 vs 10.67 … BANKED NEGATIVE"). That A/B ran
on **the 14-instance Tier B — which did not contain veer or klieber**, the two
instances where the wrong currency costs 293 s and 372 s respectively.

The same applies to §5a: the BVE density gate was worth −7 B / −32 C PAR-2 (L9)
*on the old sets*.

**The lesson is not "those measurements were wrong" — it is that a
measured-dead knob is only dead on the corpus that measured it.** Every knob
banked negative before 2026-07-29 was banked on a Tier B that omitted its four
worst instances, and each of them deserves exactly one re-run on the 28.

### Candidate standing lessons (for PROGRESS — the main agent owns that file)

* **A measured-dead knob is dead only on the corpus that measured it.** The
  TransitiveReduce ticks-meter was banked negative on 2026-07-25 at a cost of
  1.55 Tier B PAR-2; on the 28-instance set the rule it fails to bound costs
  ~293 s of wall on veer and ~372 s on klieber. Re-run every pre-2026-07-29
  negative bank once, on the 28.
* **A rule's meter can be in the wrong currency, and the symptom is invisible
  until the formula changes shape.** V4 meters candidates while a candidate is a
  full BFS; X2 meters ticks while its failure mode is output. Both are ~0 % of
  the clock on the old corpus and 21×/53× on the new one. When a rule is added,
  ask what ONE unit of its meter costs on the worst-case formula shape, not on
  the tier.
* **An admission gate written as a formula *shape* test partitions the corpus,
  not the decision.** The binary-density-6 gate does not distinguish
  "elimination helps" from "elimination hurts"; it distinguishes
  gate-encoded-with-many-binaries from everything else, and every instance we
  lose badly on is on the wrong side of it.

---

## 8. Ranked plan — each with a pre-registered prediction

Every probe below is `sbatch`-ready against `build/pin-final` (or a fresh build
in a private directory) and reuses `benchmarks/probes/tierB-timeouts*.sh`.

**Coverage of the four, at a glance** (which action can recover which instance;
`✓` = measured, `?` = predicted):

| action | veer | rxor | klieber | fclq | near-misses |
|---|---|---|---|---|---|
| **R1** TransitiveReduce metering | ✓ −586 s of the 2,200 s solve | — | ? 18.4 s if OFF, but the rate test says the damage is its *output*, not its work (§5b) | — | — |
| **R2** BVE admission | ✓ 550 s (solves) | — | ✓ 42.5 s | — | nw ✓ 35.7 s |
| **R3** X2 governor | — | — | ✓ 63 s | — | nw ✓ 1.4 s |
| **R1+R2+R3 together** | ✓ 387 s (solves, still > 300 s cap) | — | **✓ 12.97 s (kissat parity)** | ✗ 337 s | — |
| **R5** Factor clause size | — | **only lever** | contributes (kissat −7.7× at size 2) | **only lever** | nw (kissat −2.9×) |
| **R4** Strengthen budget | — | — | — | — | DivS ✓ 9.2 s all-off |

Ranking is by *measured recovery ÷ implementation cost*: **R2** (an admission
predicate) is the only cheap change with a **measured** recovery (klieber 390 →
42.5 s); **R1** (a meter currency change plus kissat's early exit) fixes a real
unit defect and 586 s of veer's clock but its klieber payoff is unproven;
**R5** is the only path to the other two and is a rule-dev project.

> **Recommended order: R2 → R1 → R5 → R3 → R4.** (The R-numbers are the section
> labels below, not a priority.) R2 first because it is the one cheap change
> with a measured solve; R1 next because the unit mismatch is a defect
> regardless of its payoff and it removes 586 s from veer; R5 third because it
> is worth twice as much as anything else and needs the most lead time; R3 and
> R4 last because neither converts an unsolved instance.

**PAR-2 accounting** (28 instances, 300 s cap ⇒ an unsolved instance is
600/28 = **21.43** PAR-2 points; today 102.28):

| action | instances recovered | Tier B(28) PAR-2 after |
|---|---|---|
| today | — | **102.28** |
| **R2** (klieber 390 → 42.5 s — the one *measured* recovery) | klieber | ≈ **81.4** |
| R1 (uncertain for klieber; see its prediction) | 0–1 | 81.4–102.3 |
| + R3 (nw is already solved; it just gets 74 s cheaper) | — | ≈ 78.7 |
| + R4 (DivS 123 → ~20 s) | — | ≈ 75.0 |
| + R5 (rxor, fclq at ~40 s) | rxor, fclq | ≈ **35.0** |
| + veer under the 300 s cap | veer | ≈ **13.6** (kissat 9.54) |

**Note the asymmetry: R2 is cheap and buys one solve outright; R5 is the
expensive one and buys two; R1 is cheap and certainly right (the unit mismatch
is a defect either way) but its klieber payoff is unproven.** And veer's 387 s
best is *outside* the cap, so none of R1–R3 converts it into a Tier B solve on
its own — which is why §9.1 is the single most important open question here.

### R1 — Meter `TransitiveReduce` in ticks, and give `walk_reaches` kissat's early exit

* **Hypothesis.** V4's budget is denominated in candidates while each candidate
  is a full BFS with no early exit, so the 20‰ ledger cannot bind. It costs veer
  **48.8 % of the clock** (kissat: 0.03 s) and klieber **21×** in trajectory.
* **Probe.** Rebuild with `meter_transitive → k.stats.transitive_ticks` and an
  early `return true` at `q == dst`; then
  `python3 tools/bench/run.py --tier B --slurm --throttle 48 --solver <newbuild>/tactix --baseline kissat`
  plus the same on Tier C. (Diagnostic pre-check already in hand:
  `TACTIX_SLOT_DISABLE=TransitiveReduce`.)
* **CONFIRMS.** klieber < 40 s (a solve, worth 20.9 PAR-2 points), veer's slot
  share < 10 %, no core14 instance worse than +2 s.
* **KILLS.** Any core14 regression that survives a second window — specifically
  `rbsat` (+17 s) and `mp1-blockpuzzle` (out of envelope), the two instances that
  banked this arm negative in 2026-07-25, plus `FmlaEquivChain` and `9vliw`,
  which the code comment names as needing the *unbudgeted* reduction.
* **The kill criterion is already pre-tested (array 1575173, shipped controls in
  the SAME window) and it does NOT fire.** Cutting V4's rate twentyfold
  (`TransitiveReduce:1`) on exactly those four instances:

  | instance | shipped (same window) | `TransitiveReduce:1` |
  |---|---|---|
  | `rbsat-v760c43649gyes10` | 13.00 s / 392,306 | **5.64 s / 199,344 (2.3× FASTER)** |
  | `mp1-blockpuzzle` | 10.25 s / 291,305 | 10.22 s / 291,305 (identical) |
  | `FmlaEquivChain_4_6_6` | 30.10 s / 982,451 | **25.19 s / 662,513 (1.2× faster)** |
  | `9vliw_bp_mc` | 14.88 s / 303,058 | **10.45 s / 239,823 (1.4× faster)** |
  | `mod4block` (control) | 13.01 s / 418,796 | 13.00 s / 418,796 (identical) |

  Every one is neutral or better, and three of the five change their conflict
  count, so the rate really did bind on them. The 2026-07-25 negative bank does
  not reproduce in today's configuration. **Caveat, and it is the important
  one:** on mod4block and mp1-blockpuzzle the arm is *bit-identical*, i.e. the
  rate never bound there — so this pre-test bounds the risk on the three
  instances V4's rate does reach and says nothing about the two where only a
  ticks meter would bite. Those two must be re-measured against the real change.
* **Prediction — and the honest uncertainty.** veer 2,200 → ~1,600 s (still
  outside the 300 s cap); core14 −1 to +2 PAR-2. **For klieber the prediction is
  weak and I say so up front:** its damage is V4's *output* (823 deletions →
  15× conflicts, §5b), not V4's work, and a ticks budget only cuts deletions
  in proportion to the work it removes. V4 spends 844 M ticks on klieber against
  a 20 ‰-of-search-ticks allowance of roughly 200 M, so a ticks meter should cut
  its work ~4×; whether that takes 646,204 conflicts back toward 42,174 is
  **not predictable from anything measured here**. Range: klieber 390 → 60–350 s.
  Tier B(28) 102.3 → **81–102** — i.e. R1 may recover klieber's solve or may
  not, and that is the first thing the arm must be read for.
* **If R1 does not recover klieber**, the follow-on is not more dosing: it is to
  ask why deleting 823 *redundant* binaries costs 15× the conflicts (a
  clause-quality/watch-order question, and a second L3 sighting).

### R2 — Replace the binary-density admission of the BVE dose

* **Hypothesis.** `binaries/var ≥ 6` is a proxy for "elimination is safe here"
  that is *false on every sparse instance in the new tier*; below it we do no
  elimination at all, at preprocessing or during search.
* **Probe.** One Tier B(28) + Tier C array with `TACTIX_BVE_DOSE_DENSITY=0`, and
  a second with the gate replaced by a *cost* test (occurrence-weighted
  resolution budget) rather than a density test.
* **CONFIRMS.** klieber < 60 s, nw < 45 s, veer under 600 s, **and** mod4block /
  crypto1 within their envelopes (13.2 s / 16 s) — the two instances L9 says
  the gate exists for.
* **KILLS.** mod4block or crypto1 blowing up (L9's −7 B / −32 C), which would
  mean the gate must become *instance-aware*, not removed. That is the §5.6
  dosing exhibit, not a constant.
* **Prediction.** `DOSE_DENSITY=0` recovers klieber (−20.9 PAR-2) and gains
  ~1.4 more from nw, and loses 1.5–3 PAR-2 to mod4block + crypto1 — net
  positive, but **not** a shippable constant; the shippable form is the cost
  test. (Note the trap: the raw seconds look enormous — veer alone moves 2,200 s
  → 550 s — while the PAR-2 gain is bounded by 21.43 per instance actually
  brought under the 300 s cap, and veer is not one of them. Score this in
  PAR-2, not seconds.)

### R3 — Give X2 a governor that is not inert

* **Hypothesis.** The occ/max-add pair just landed cannot bind on ternary-heavy
  sparse formulas; the binding cadical knobs are `ternaryrounds` (2 vs our 25)
  and `ternaryeffort` (8‰ vs our 20‰).
* **Probe, run (array 1575173).** `TACTIX_SLOT_PERMILLE=HyperTernaryRes:8`
  (cadical's `ternaryeffort`) on nw, klieber and mod4block. **Result: nw 75.42 →
  41.79 s (1.8×), with 19 rounds / 38,578 resolvents instead of 25 / 56,291;
  klieber 390.54 → 261.77 s (1.5×) — both nowhere near the 1.42 s / 62.95 s that
  `−X2` gives; mod4block is bit-identical (13.12 s / 418,796 conflicts, i.e. the
  rate never binds there).** *(Relevant to the concurrent X2-effort work: 8 ‰ is
  worth 1.5–1.8× here, not the order of magnitude the rule-off arms show.)* So
  **the effort rate is
  the WRONG control for X2** — what has to be bounded is *rounds* and *output*,
  which is `ternaryrounds=2` (we run 25 on nw, 17 on mod4block) and a
  `ternarymaxadd` expressed against the ORIGINAL clause count rather than the
  current one.
* **Follow-on probe.** A real per-round counter (note:
  `TACTIX_SLOT_MAX_APPLIES` counts slot *considerations*, so it cannot emulate
  `ternaryrounds` — PROGRESS records this), then `rounds ≤ 2`.
* **CONFIRMS.** nw < 10 s and mod4block still ≤ 14 s (mod4block is where X2's
  value lives, L15/L18).
* **KILLS.** mod4block regressing — then X2's dose is non-separable from BCE's
  and the pair must be judged together (L15).
* **Prediction.** rounds=2 takes nw to 2–15 s and costs mod4block < 1 s; the
  effort cut alone does less.

### R4 — Make `Strengthen`/`SubsumeForward` budget-aware

* **Hypothesis.** They are the only slot entries whose single round is
  unbounded; on a 855k-clause instance one application is 27 s.
* **Probe.** Add the `work_budget` read (the shape `Vivify`/`Factor`/`BCE`
  already have) and re-run Tier B(28) + C. Pre-check already run:
  `divs-slowsubsume` (rate 1‰) = **92.21 s** vs 123.07 shipped — the rate cuts
  the later rounds, the first round survives, and the residual 92 s is the
  unbounded first round. That is the number a mid-round budget must remove.
* **CONFIRMS.** DivS < 30 s with the slot share below 40 %.
* **KILLS.** any Tier C instance that depends on a *complete* subsumption round
  (the same hazard the `dose_cut` bookkeeping exists for).
* **Prediction.** DivS 123 → 20–40 s; tier-level effect small (this is one
  instance's shape), so judge it on DivS + no regressions, not on PAR-2.

### R5 — Extend Factor beyond binary clauses (`factorsize`) — **the reference-side test is already DONE and it CONFIRMED**

* **Hypothesis.** Our E2 is the binary-clause-driven variant; kissat's default
  `factorsize=5` is what wins rxor (>24×) and fclq (12×).
* **Probe, pre-registered and run (array 1574949).** `kissat --factorsize=2`,
  i.e. kissat restricted to *our* variant. Prediction filed before the run:
  *"rxor > 100 s; fclq > 100 s."*
* **RESULT — confirmed, and stronger than predicted.** rxor **TO @300**
  (10.02 M conflicts, vs 12.27 s at the default); fclq **203.94 s** (12.6×);
  klieber **95.14 s** (7.7×, `factored` 723 → 1); nw **17.41 s** (2.9×). CaDiCaL
  `--no-factor` loses rxor as well (TO @300). **The gap on rxor and fclq is
  Factor's clause-size scope, not our selection or dosing.**
* **Next step (rule-dev, not this report).** Raise E2 from binary-driven to
  clause-size ≤ 5, i.e. the general m×n grid over clauses rather than over binary
  partners (`factor.cpp binary_partners` → occurrence lists filtered by size).
  Proof obligation is unchanged (add-RAT dividers/quotients + delete), so the
  five-primitive vocabulary already covers it; the work is detection + grid
  selection. Gate it on the Tier B(28) + Tier C pair with rxor/fclq as the
  targets and `x9`/`mp1-blockpuzzle` (today's Factor beneficiaries) as the
  regression watch.
* **Prediction for the implementation.** rxor TO → 15–60 s, fclq 369 → 30–120 s;
  the two together are worth ≈ **40 PAR-2** on Tier B(28) — the largest single
  item in this report.

### R6 — Stop Congruence re-extracting a 385k-gate closure for 4k merges

* **Hypothesis.** Rounds 2–6 on veer re-extract 384,970 gates each and add
  4,008 equivalences (14.2 s total). The material watermark
  (`binaries_mark`/`root_fixed_mark`) admits the re-run because congruence's own
  added binaries move the mark.
* **CONFIRMS / KILLS / prediction.** −10 s on veer, nothing elsewhere; only
  worth doing after R1/R2. Listed so it is not lost.

### R7 — The walk step counts on UNSAT instances look alarming and are NOT the lever

Worth writing down precisely because the counter invites the wrong conclusion.
On a **refutation**, rxor spends **91.2 × 10⁹ walk steps** over its 3,600 s run
(899 M flips, 31 walks) and fclq **10.4 × 10⁹**. That is a local search on a
formula with no model, and it is invisible in `c timing` because Walk is a
policy op, not a registry rule — it lands inside "search + scaffolding".

**But the walk-off arm prices it at ~4 %, not at the 25–50 % the step count
suggests.** Like-for-like at a 600 s cap, no proofs: `rxor-walk0` 11,254,103
conflicts vs `−BCE` 10,850,103 and `−Vivify` 10,797,513 — i.e. removing walk
buys about 4 % more conflicts. The steps are cheap (a 1,080-variable formula
fits in L1). `klie-walk0` 212 s vs 390 s shipped is the one instance where walk
is expensive, and klieber is SAT.

**Conclusion: do not act on this.** It is a §5.6 dosing note (walk dose on
refutations), not a defect, and it is exactly the L14 shape — a huge counter
that evaporates when you measure the thing it supposedly implies.

### Not to do

* **Do not open Sweep or Reorder on this evidence** (§6; L11 upheld).
* **Do not treat any rule-off arm here as a shipped configuration** — the
  standing directive is that every implemented proof rule ships ON, and every
  number in §5b is a diagnostic for the scheduling defect named in §5c.

---

## 9. What I could not determine

1. **What still costs veer its remaining 385 s.** The shipped default *does*
   solve it — at **2,199.81 s**, 687× kissat — and the best measured
   configuration is **387.46 s** (`force-BVE − TransitiveReduce − X2`), still
   121× kissat's 3.37 s and 96× CaDiCaL's 4.03 s / **3 conflicts**. R1+R2+R3
   therefore take veer from 2,200 s to ~390 s, which is a 5.7× win and *still
   outside Tier B's 300 s cap*; the remaining ~120× is **not localized by
   anything in this report**. The next probe on veer is a profile
   (`perf record`) of the `force-BVE −TR −X2` configuration, not another
   rule-off arm — every rule-off arm has now been run.
2. **Whether our Substitute actually removes the 161,462 variables Congruence
   merges on veer.** There is **no counter** — `c substitute` prints rounds only,
   and `c congruence` prints neither gates nor merges. kissat prints
   `substituted: 195,888 (38 %)`. Adding `congruence_merged` and
   `substituted_vars` counters is a prerequisite for any further work on veer.
3. **Why `veer −Strengthen` solves (593 s) when Strengthen's own cost there is
   1.5 s.** The arm has a wholly different trajectory (2.6 × 10⁹ decisions,
   199,532 BCE deletions, 4.1 × 10⁹ walk steps). Treated as chaos, not signal —
   L17's warning about reading a trend across arms that change which rules run.
4. **Whether our binary-only Factor could be made to fire usefully on fclq
   without R5.** Forcing it (`TACTIX_FACTOR_DENSITY=0`) makes fclq *worse*
   (505.55 s vs 368.93), and kissat at `factorsize=2` gets no benefit either —
   so the density gate is not what is holding fclq back, but I cannot separate
   "our binary Factor is badly selected" from "the binary case simply has no
   value here" without R5's implementation.
5. **The fclq residual.** After every scheduling fix the best arm is 241 s against
   kissat's 16 s, and CaDiCaL times out. If R5 says the gap is Factor's scope,
   fclq is an R5 instance; if not, fclq is an open search-quality question and
   the only one of the six that is.
