# Effort-budgeted inprocessing scheduler — design (plan rev 53)

> **Frozen snapshot — do not edit.** A design/reference record from when this
> work was done; it is *not* kept in sync. Living truth: `docs/calculus.md` (what
> the rules are), `DEVELOPMENT_PLAN.md` (scope/roadmap), `PROGRESS.md` (status).

**Trigger:** plan rev 53 — replace the run-each-rule-to-fixpoint one-shot
preprocessing (`default_policy.cpp` phase loop) with kissat's dosing discipline
(per-technique effort budgets + growing intervals + mode gating, interleaved with
search), and enable the built-but-disabled probing rules under it. Supersedes the
`INPROCESS_PHASE_BUDGET` application count and the BVE-last / Factor-density
stopgaps. Continues `docs/design/inprocessing-revamp-design.md` §B (the tick
budget it deferred) and §C (SAT-safe scheduling).

## 1. Evidence measured for this design (2026-07-21)

All numbers on the reference machine (Apple M3 Pro), jobs=1, release build.

### 1.1 Tick-unit calibration — kissat's constants transfer

| solver | instance | ticks | wall | ticks/s |
|---|---|---|---|---|
| tactix | 9vliw_bp_mc | 277.0 M | 10.42 s | 26.6 M/s |
| kissat 4.0.4 | 9vliw_bp_mc | 248.7 M (`search_ticks`) | 5.91 s | 42.1 M/s |

Same order of magnitude (1.6×), so kissat's absolute floor `mineffort = 10`
(= 10 M ticks ≈ 0.4 s of our search) is a usable constant rather than something
that must be re-derived.

### 1.2 Where kissat actually spends inprocessing effort (our exact Tier B)

`kissat --statistics`, profiling block, seconds per technique:

| instance | total | search | probe | vivify | sweep | elim | congr | factor |
|---|---|---|---|---|---|---|---|---|
| 9vliw_bp_mc | 5.79 | 3.06 | 1.91 | 0.51 | 0.83 | 0.76 | 0.08 | 0.19 |
| FmlaEquivChain_4_6_6 | 8.44 | 4.48 | 2.82 | 0.99 | 0.43 | 0.99 | 0.30 | 0.51 |
| battleship-14-26 | 5.06 | 4.64 | 0.39 | 0.25 | 0.10 | 0.01 | 0.00 | 0.00 |
| circuit_800gates | 8.54 | 6.24 | 1.92 | 1.07 | 0.42 | 0.31 | 0.05 | 0.20 |
| clique_n2_k10 | 4.27 | 3.81 | 0.44 | 0.28 | 0.13 | 0.01 | 0.00 | 0.01 |
| combined-crypto1 | 3.16 | 2.52 | 0.56 | 0.31 | 0.18 | 0.06 | 0.01 | 0.02 |
| **hwmcc17miters-oski15** | **2.15** | **0.00** | **1.52** | 0.00 | 0.00 | 0.00 | **1.52** | 0.00 |
| mod4block | 13.56 | 10.40 | 2.52 | 1.27 | 0.31 | 0.59 | 0.02 | 0.75 |
| mp1-blockpuzzle | 7.11 | 6.26 | 0.76 | 0.49 | 0.17 | 0.07 | 0.02 | 0.02 |
| qwh.50.1250 | 7.86 | 6.40 | 1.27 | 0.55 | 0.24 | 0.11 | 0.06 | 0.19 |
| rbsat-v760 | 4.56 | 3.76 | 0.72 | 0.40 | 0.17 | 0.07 | 0.01 | 0.08 |
| square.2.0.i | 2.42 | 1.17 | 0.94 | 0.23 | 0.37 | 0.26 | 0.12 | 0.10 |
| summle_X4044 | 4.35 | 2.55 | 1.61 | 0.43 | 0.82 | 0.14 | 0.10 | 0.13 |
| x9-08014 | 8.05 | 7.36 | 0.65 | 0.39 | 0.24 | 0.01 | 0.00 | 0.00 |

Readings that drive the design:

1. **Inprocessing is 8–33 % of kissat's runtime on 13/14 instances** — a bounded
   fraction of search, exactly what `SET_EFFORT_LIMIT` encodes. It is never a
   one-shot preprocessing burst.
2. **Vivify is the single largest probing sub-technique** (0.23–1.27 s) on every
   instance. We do not run it. Sweep is second (0.10–0.83 s) and we do not have
   it.
3. **BVE (`eliminate`) is small** — 0.01–0.99 s, and *zero* on the instances where
   our BVE has historically hurt (clique 0.01 s, x9 0.01 s, battleship 0.01 s).
4. **kissat's initial preprocessing runs NO BVE, NO vivify and NO transitive
   reduction** (`fastel = 0`; `probe_initially` = congruence → substitute →
   backbone → sweep → substitute → factor). Every BVE resolution kissat performs
   happens *during search*, after probing has already simplified the formula and
   with a resolution budget proportional to search work. Our slot does the
   opposite: a single unbudgeted BVE burst on the raw formula at conflicts = 0.
   This is the structural root of the bimodal-BVE saga (PROGRESS 2026-07-21) —
   the BVE-last ordering stopgap approximates "let the other rules go first",
   which kissat gets for free from its schedule.

### 1.3 Our current dosing is size-blind and quadratic

`INPROCESS_PHASE_BUDGET = 300` bounds *applications*, not work:

- On `hwmcc17miters-oski15` (1 454 067 vars / 4 368 063 clauses) the whole
  preprocessing pass eliminates **300 variables** — 0.02 % of the formula.
- Each application costs `bve_schedule` = **O(formula)** (`bve_apply`,
  `bve.cpp:554`), and each elimination ends in **`kernel_collect_garbage`**
  (`bve.cpp:545`) = O(arena). So the pass is O(applications × formula): the count
  bound is load-bearing for *runtime*, not for simplification quality.
- Simply raising it is catastrophic — with the cap at 1e8 and 12 outer rounds,
  `hwmcc17miters-oski15` goes **74 s → > 600 s (timeout)**, and increment 1 of the
  revamp already recorded the cap as hypersensitive (circuit 51 s at 300 ↔ 133 s
  at 400).

So the budget must be denominated in *work* and the per-dose cost must be
proportional to that work. Both halves are required; either alone fails.

### 1.4 Corrected diagnosis of the worst Tier B instance

PROGRESS recorded `hwmcc17miters-oski15` (our 34× worst) as "needs Sweep +
probing". Measurement says otherwise:

- kissat solves it with **congruence alone** — 1.52 s of 2.15 s, `search_ticks = 4`,
  0 conflicts, `congruent: 711 434 (49 % of variables)`.
- **Our congruence finds the same collapse**: `[cong] 2 058 212 gates, 710 722
  equivalences added`, and Substitute *does* apply them (`do_substitution
  changed=1`; the duplicate-binary bail-out at `substitute.cpp:286` does not
  fire). A second congruence pass then finds 0 further equivalences — we are at
  the same congruence fixpoint kissat reaches.
- Yet we then spend **292 174 584 decisions for 1 193 conflicts** (74 s): after
  the collapse the residual formula is refuted with almost no search, but each
  conflict costs a ~245 k-deep descent.

So the gap is **not** a missing technique in the collapse; it is that kissat
*refutes inside* congruence/probe while we hand a collapsed-but-not-refuted
formula to search. This is a separate work item from the scheduler (recorded in
PROGRESS); Sweep is not the lever it was assumed to be here.

## 2. The mechanism being ported

### 2.1 kissat `SET_EFFORT_LIMIT` (`kimits.h:135-170`)

```
REFERENCE = search_ticks - last_ticks[phase]        // search work since this phase last ran
if (REFERENCE < mineffort * 1e6) REFERENCE = mineffort * 1e6      // 10 M
DELTA     = (NAMEeffort / 1000) * REFERENCE
LIMIT     = <technique's own work counter> + DELTA
```

The technique loops until its own counter exceeds `LIMIT`. Two properties matter:
the budget is a **fraction of the search work that preceded it** (self-tuning,
pathology-proof), and each technique is metered in **its own currency** — kissat
deliberately caps `eliminate_resolutions` (a resolvent count) and `forward_steps`
(an occurrence-scan count) with a delta derived from ticks. Ratios transfer;
units do not.

Default effort fractions (`options.h`): eliminate 100 ‰, vivify 100 ‰, forward
100 ‰, sweep 100 ‰, factor 50 ‰, backbone 20 ‰, transitive 20 ‰, substitute 10 ‰,
`mineffort` 10 (millions).

### 2.2 kissat interval growth (`kimits.h:112-131`, `kimits.c:39-57`)

```
DELTA  = NAMEint * SHAPE(count)          // NLOGN for probe, NLOG2N for eliminate
SCALED = (4.5*(log10(C+10)-5)^2 + 25) * DELTA        // C = binary + irredundant clauses
next.conflicts = conflicts + SCALED
```

`probeinit/probeint = 100` (shape `n·log10(n+9)`), `eliminateinit/eliminateint =
500` (shape `n·log10(n+9)²`).

Both additionally **refuse** to run in the same conflict as a clause-DB
reduction — `if (solver->last.conflicts.reduce == conflicts) return false`
(`probe.c:21`, `eliminate.c:28`; the field is set at `reduce.c:194`). Note this
corrects `docs/design/inprocessing-revamp-design.md` §C-ii, which read the same
gate as *piggybacking* simplification onto a reduction. It is the opposite, and
increment 2 must implement the refusal. (That doc is a frozen snapshot, so the
correction lives here.)

### 2.3 CaDiCaL's refinement — deferral with accrual (`limit.hpp:141-148`)

Where kissat *raises* a too-small reference to `mineffort` and always runs,
CaDiCaL *defers*: if the earned `DELTA` cannot even amortize the technique's
data-structure setup (`DELTA < NAMEthresh * clauses`), it returns without running
**and without moving the anchor**, so the budget keeps accruing. This is the
better fit for our rules, several of which pay an O(arena) enumeration before any
useful work (Vivify, Strengthen, SubsumeForward) — running them on a budget
smaller than their setup is pure waste.

## 3. Architecture

**All scheduling state lives in `DefaultPolicy`** (plan §5 rev 17: rules carry no
scheduling). No kernel change and no guard change, so N1 is untouched: every rule
stays applicable at any time through the registry, and a policy that wants no
budget simply does not keep a ledger.

### 3.1 Search-tick attribution, policy-side

Our `k.stats.ticks` is a single global propagation meter (kernel.cpp:426, :456),
covering search *and* probing. kissat needs the two separated. We get that
without touching the kernel: the policy snapshots `k.stats.ticks` around every
inprocessing application, accumulates the delta into `inprocess.ticks`, and
defines

```
search_ticks() = k.stats.ticks - inprocess.ticks
```

This is also what the mode-switch limits should use (kissat's `mode.c` reads
`search_ticks`), otherwise inprocessing work would shorten stable phases.

### 3.2 Per-technique effort currency

Every slot entry carries its own work meter, chosen so that "one unit" is
proportional to real cost:

| rule | currency | source |
|---|---|---|
| Probe (V1), Backbone (V2), Vivify (C3) | ticks | `k.stats.ticks` delta (they propagate) |
| BVE (E1) | resolutions | new `stats.bve_resolutions` (kissat `eliminate_resolutions`) |
| Strengthen (C2), SubsumeForward (C1) | checked candidates | existing `stats.*_checked` |
| TransitiveReduce (V4) | checked edges | existing `stats.transitive_checked` |
| Congruence (V6), Factor (E2), Substitute (V3) | — (all-or-nothing) | delay counter + deferral |

Congruence, Factor and Substitute expose only `INSTANCE_CANONICAL`, so they
cannot be titrated; they get kissat's *delay* treatment (skip N consecutive
phases after an unproductive run) plus CaDiCaL's deferral instead of a budget.

### 3.3 Dose granularity per rule (from the rule-surface audit)

- **Dose-friendly** — literal/variable payloads, GC-immune, cheap re-validation:
  Probe, Backbone, TransitiveReduce, BVE. The policy enumerates once per phase
  via `instances()` and applies payloads until the budget is spent. For BVE this
  also removes the O(formula) `bve_schedule` that today runs *per elimination*.
- **Dose-hostile** — arena-`Ref` payloads invalidated by the GC inside the first
  successful dose, plus an O(arena) clause-start validation per dose: Vivify,
  Strengthen, SubsumeForward. These keep running as canonical rounds and are
  metered post-hoc (kissat does exactly this for `substitute`).
- **All-or-nothing**: Congruence, Factor.

### 3.4 GC is reclamation, not semantics

`bve_eliminate` ends with `kernel_collect_garbage` (`bve.cpp:545`) — an O(arena)
pass *per eliminated variable*. That is a scheduling/efficiency decision living
inside a rule (precisely the leakage the 2026-07-21 revisit-codebase pass routed
here). Moving it out: the canonical `bve_apply` reclaims at the end of its round
(self-contained semantics preserved for tests and RandomPolicy), while the
`apply_instance` dosing path leaves reclamation to the caller, which reclaims once
per phase/round. Not collecting is always sound — deletes are lazy everywhere and
every reader skips garbage — so N1 is preserved for any policy that never
reclaims at all.

## 4. Increment plan

Each increment: smallest complete unit → ctest + fuzz (DRAT + native LRAT, both
DefaultPolicy and `--random-policy`) → Tier A clear → Tier B A/B → PROGRESS in the
same commit. Revert anything that misses.

1. **Effort ledger + BVE dosing + GC out of the per-elimination path.**
   Replaces `INPROCESS_PHASE_BUDGET` with per-technique budgets in real
   currencies; BVE dosed through `instances()`/`apply_instance()` from a single
   schedule per phase. Composition, order and the preprocessing-only trigger are
   unchanged, so the measurement isolates *dosing cost*.
   **Findings while landing it:**
   (a) *The GC move is the whole efficiency win, and it is trajectory-neutral.*
   With reclamation restored per dose, conflict counts are byte-identical
   (tr-n300-s055 1 076 759 both ways) but `hwmcc17miters-oski15` goes from 60 s
   to not finishing in 55 min (killed): at 37 509 eliminations, an O(arena)
   collection per eliminated variable dominates everything else. This also re-explains the earlier
   "unbounded BVE is catastrophic" result — it was the collections, not the
   elimination volume.
   (b) *The budget binds.* `square.2.0.i` stops at 1 000 046 resolutions against
   the 1 000 000 ceiling (18 420 variables eliminated) and drops 5.85 s → 2.9 s;
   the old application cap had let it run to a much more expensive fixpoint.
   (c) *A static candidate snapshot is a real (small) quality loss vs kissat.*
   kissat re-scores its elimination heap incrementally — `update_after_removing_
   clause` (`eliminate.c:109`) re-scores every variable of every removed clause —
   so its ranking stays fresh at O(clause) per removal, while a snapshot taken
   once per round goes stale. Measured cost on the Tier A `tr-*` family: same
   eliminations (47 vs 48) but +5–13 % conflicts. Re-enumerating per dose is not
   an option at scale (`bve_schedule` is O(formula), and the budget now allows
   tens of thousands of eliminations), so kissat-style incremental re-scoring is
   the only design that is cheap at both scales — a follow-on.
   **Result: Tier B PAR-2 21.41 → 20.85, 14/14 both** (same window, jobs=4; sum
   of times 299.7 → 291.9). Wins: summle 10.9→4.8, 9vliw 11.8→6.6, square
   5.5→2.9, qwh 3.3→1.9, hwmcc 71.0→55.5, circuit 10.3→8.9. One real regression:
   `combined-crypto1` 8.1→24.7 s — the old cap stopped it at exactly 300
   eliminations, the budget lets it do 386, and those 86 extra eliminations
   triple the conflicts (310 k → 1 015 k) on a high-variance SAT instance. That
   is the BVE U-shape again, and it is the concrete evidence for increment 4:
   at conflicts = 0 there *is* no search work to take a fraction of, so the
   `mineffort` floor is the weakest part of this port — kissat's own answer is to
   not run BVE at preprocessing at all (`fastel = 0`). Tier A 200/200 (0.55,
   jobs=2; neutral vs baseline at equal concurrency: 121.74 vs 121.78 sum).
   Peak RSS +1.5 % on the largest instance (1.708 → 1.734 GB), so deferred
   reclamation does not trade memory for speed. Fuzz: DefaultPolicy DRAT 1000/0,
   random-policy DRAT 400/0, native LRAT 400/0, random-policy+LRAT 400/0.
2. ~~**Restore during-search inprocessing**~~ — **MEASURED → REVERTED
   2026-07-21, and it reorders the rest of this plan.** Two variants, both
   net-negative on Tier B: (A) one slot-wide growing interval — 22.82 → 27.71
   (+21 %), 12/14 worse; (B) per-technique intervals, BVE preprocess-only —
   23.01 → 24.10 (+4.7 %), 7 wins / 7 losses. Neither lost a solve, and neither
   reproduced the 2026-07-18 SAT-disruption failure (`battleship` untouched), so
   the bounded doses + the reduce gate did their job. The ablation that explains
   it, on `summle_X4044`: preprocess-only/BVE-on 5.3 s, during-search/BVE-on
   18.4 s, preprocess-only/BVE-off 21.6 s, during-search/BVE-off 8.7 s —
   **simplification during search is a 2.5× win, more BVE during search is a
   3.5× loss**, and variant A coupled them. **The real conclusion: with our
   current composition there is nothing worth doing during search** — BVE at
   preprocessing already captures the value, and re-running Congruence/
   Substitute/Strengthen/Factor costs more than it returns. The techniques kissat
   spends its during-search budget on are Vivify and Sweep, which we do not run.
   So during-search inprocessing is not a standalone increment; it returns as a
   consequence of increment 3, reusing variant B's per-technique-interval shape.
3. **Vivify on** (NOW THE NEXT INCREMENT), under its tick budget — kissat's
   largest probing spend, and the one technique we have built but never
   scheduled. **Risk to watch:** its canonical apply is a full UNBOUNDED round,
   so the post-hoc meter can only skip *later* rounds, not stop one in progress;
   a large instance may need a real bound (a per-candidate dose is blocked by the
   Ref-payload staleness in §3.3). Then Probe, Backbone, TransitiveReduce, one at
   a time, each with its own A/B.
4. **Adopt kissat's composition split** — BVE out of preprocessing (kissat's
   `fastel = 0`), probing-style techniques first; retire the BVE-last ordering
   stopgap and the Factor `density = 8` magic number if the schedule subsumes them.
5. **V5 Sweep** (needs the embedded kitten sub-solver, plan O11) — a separate
   rule-dev item, sized on its own.

N1/N2/N3: N1 — no guard or kernel change, all state policy-side, rules stay
individually applicable (fuzzed via `--random-policy`). N2 — no rule's proof delta
changes; enabling more rules *increases* DRAT/LRAT coverage on real instances,
which is half the point of rev 53. N3 — this exists to move N3 forward; each
increment is reverted if Tier B/C do not confirm.
