# V5 Sweep — the T3 degradation is an UNENFORCED effort ceiling, not a wrong currency

**Date:** 2026-08-05 · **Branch:** `sweep-governor-fix` · **Base:** `97e9476` (post-SCHED-Stage-D)
**Frozen on landing** (doc discipline). Companion counters: `c sweep`, `c sweepemit`, `c slot Sweep`.

Sweep ships ON — user directive, non-negotiable. This report diagnoses *why* it degraded Tier C and
fixes the degradation; nothing here turns the rule off or shrinks it toward inert. The reachability
evidence in §8 exists precisely to prove that.

## 1. Verdict in one line

`Stats::sweep_ticks` charges Sweep's work **honestly**; the policy's per-mille ceiling was simply
**never enforced where the work happens**. `sweep_detect_round` polled the round budget only *between*
seeds, and `sweep_env_build` had no poll at all — so a single seed whose occurrence lists are long
could outspend the whole round dose by **41× to 598,573×**. Fixing the enforcement point takes the
overshoot to **≤1.005×** and 100‰ now means what it says.

**The meter hypothesis was tested and is REFUTED as stated.** The currency is not wrong: a
Sweep build tick and a kernel search tick are within ~3.5× in wall cost (§3). What was wrong is that
the meter was consulted, and the answer ignored, at the only point where it could have bound.

## 2. The measured mechanism

All timing via `sbatch`, one instance per task, `--cpus-per-task=1 --mem-per-cpu=8G`, node017-pinned.
**The headline is a pure counter ratio (`handed` vs `spent`, both ticks) and is therefore
contention-invariant**; the wall figures only price it.

### 2.1 `constraints_18_0.3_2` — the clean exhibit (bit-identical trajectory)

Pre-Stage-D probe at `ea15617`+instrumentation (jobs `1584624`/`1584625`), `TACTIX_SLOT_STATS=1`:

| | Sweep ON (shipped) | Sweep OFF |
|---|---|---|
| wall | **597.2 s** | 293.5 s |
| conflicts | 2,258,701 | 2,258,701 — *identical* |
| `stats.ticks` | 8,798,721,421 | 8,798,721,421 — *identical* |
| Sweep wall (instrumented) | **304.85 s** | — |
| rounds / environments / seeds tried | 45 / 45 / **45** | — |
| equivalences | **0** | — |
| `handed` (ticks) | 775,416,000 | — |
| `spent` (ticks) | **32,046,503,581** | — |
| **overshoot** | **41.3×** | — |

The trajectory is bit-identical, so 100 % of the +303.7 s is Sweep's own overhead — and the
instrumented `sweep_ns_total` reads **304.85 s**, matching the A/B delta to 0.4 %.

**Where the time is.** The per-phase wall split (temporary instrumentation, since removed):

```
sweep total 304.848 s = begin 0.000 + build 304.835 + encode 0.006 + solve 0.006 + emit 0.000
```

**99.996 % of Sweep's cost is `sweep_env_build`** — the BFS occurrence walk. The embedded sub-solver,
the thing the design worried about, costs **6 ms**. Per-round setup (`sweep_round_begin`, which is
where a round-local occurrence index would be built) costs **0.000 s**, because the Inprocess slot has
`k.dense` connected and Sweep reuses it. *(This directly answers the "per-round setup, e.g.
`dense_connect`" hypothesis raised during the work: `dense_connect` is the slot's cost, shared by
every rule in it, and Sweep's own per-round setup is unmeasurable. The cost is in-round scanning.)*

**Why the scan is unbounded.** `max_env_vars` (256) and `max_env_clauses` (1024) bound what an
environment *holds*, not what it costs to find. Once the variable cap is reached, `env_add` rejects
essentially every further clause — but the enclosing loop keeps walking every remaining occurrence of
every environment literal, scanning each clause in full for the root-satisfied test:

```
for (const Ref ref : (*r.occ)[l]) {          // no budget poll
  ...
  for (const Lit m : *c) satisfied = ...;    // full clause scan
  ticks += c->size;
  env_add_with_units(...);                   // rejects at the cap, unconditionally
}
```

45 rounds × one seed each = **6.8 s per environment build**, and because the round-tick check sits
*outside* `sweep_detect_seed`, the round is already 41× over budget by the time it is next consulted.

### 2.2 The other three instances (pre-Stage-D, same window)

| instance | Sweep wall | `handed` | `spent` | overshoot | equivalences |
|---|---|---|---|---|---|
| `em_11_3_4_cmp` (Sweep wins) | 0.074 s | 70,108,900 | 76,511,123 | **1.09×** | 3 |
| `sat-bench-trig-bhaskara` (wins) | 0.493 s | — | 972,918,155 | ~1× | 23 |
| `minxor128` (loses a solve) | **1.95 s** | 2,443,105,900 | 2,463,332,872 | **1.01×** | 961 |
| `constraints_18_0.3_2` (loses) | **304.85 s** | 775,416,000 | 32,046,503,581 | **41.3×** | 0 |

Two findings fall straight out:

- **On the instances where Sweep is well-dosed it is essentially free** (0.07–1.95 s) and it pays.
- **`minxor128` is a DIFFERENT mechanism.** Sweep's total wall there is 1.95 s of 1250 s (0.16 %) and
  the ledger is respected to 1.01×. Its cost is **trajectory**: 961 equivalences change the search.
  No governor fix addresses that, and this report does not claim one. `minxor128` at 884–906 s
  (Sweep off, measured twice) against a 1200 s cap is a near-miss instance; treat the solve flip as
  a lottery until paired draws say otherwise ([[chaotic-instance-veto]]; the resume anchor already
  says the same).

### 2.3 Stage D amplifies the defect, exactly as predicted

Re-measured at `97e9476` (Stage D merged; Sweep sits in the PROBE group, which fires ~2.4× more often).
Stage D roughly doubles Sweep's occasions — `constraints_18` goes 45 → 82 — and the unbudgeted scan
scales with them. The shipped pre-fix baseline (`base`, wave `w5`) at post-D frequency:

| instance | occasions | `handed` | `spent` | **overshoot** |
|---|---|---|---|---|
| `constraints_18_0.3_2` | 82 | 665,325,800 | **48,684,784,694** | **73.2×** *(was 41.3× pre-D)* |
| `sat-bench-trig-bhaskara` | 66 | 487,095,700 | 590,179,740 | 1.21× |
| `em_11_3_4_cmp` | 145 | 1,481,590,400 | 1,541,505,234 | 1.04× |
| `minxor128` | 137 | 2,472,752,200 | 2,518,451,904 | 1.02× |

**Two failure modes worth recording separately**, both seen in a deliberately *unbudgeted* control arm
(`w3`'s `base`, before the hatch was corrected — see §9) which removes the between-seeds check too and
so shows the same defect without its one remaining brake:

- **Overshoot without any brake reaches 6,730× (`minxor128`) and 598,573× (`bhaskara`)**, and the
  policy's `remaining0 = limit - meter(k)` then **wraps**, printing `handed
  1,844,674,408,990,499,300` on `em_11`. A ceiling that has already been blown cannot compute a
  remainder.
- **An unbounded environment scan is UNINTERRUPTIBLE.** `timeout` delivers SIGTERM, the handler sets
  the stop flag, and nothing inside the BFS polls it: `constraints_18` in that arm survived its
  1250 s inner timeout and had to be killed by the 35-minute SLURM wall (job `1584641`, state
  `TIMEOUT`). Same family as the §6 SIGTERM defect in implementation-notes. The fix closes this too,
  because the budget now bounds the scan.

## 3. Tick-vs-wall: the currency is sound, so re-pricing is NOT the fix

| phase | ticks | wall | ns / tick |
|---|---|---|---|
| Sweep env build, `em_11` | 75,592,719 | 0.066 s | **0.87** |
| Sweep env build, `bhaskara` | 963,982,154 | 0.378 s | **0.39** |
| Sweep env build, `constraints_18` | 32,045,637,999 | 304.835 s | **9.5** |
| Sweep sub-solver, `constraints_18` | 249,513–865,582 | ~0.006 s | ~7 |
| kernel search tick, `constraints_18` | 8,798,721,421 | ~293 s | **~33** (amortized) |

A Sweep build tick costs **0.39–9.5 ns**; a kernel search tick costs ~33 ns amortized. They are the
same *kind* of unit (occurrence/watch entries inspected, plus clause length) and within ~3.5× at the
worst end. **The meter is not under-pricing by 2712× the way C1/C2 were** — there is no missing
currency term to add. `sweep_ticks` already charges the occurrence walk, the clause scans, the dedup
scan and every sub-solver tick.

That is why the fix is an enforcement point, not a re-pricing: with the ceiling actually applied,
Sweep on `constraints_18` spends 775 M ticks — its exact entitlement — for **9.1 s**, which is
~10 % of the search work, which is what 100‰ was always supposed to buy.

## 4. What the references do that we did not

Read in full: `kissat/src/sweep.c` (1724 lines), `kissat/src/kimits.{c,h}`, `kissat/src/options.h`,
`cadical/src/sweep.cpp`, `cadical/src/limit.hpp`, `cadical/src/options.hpp` (macro definitions read,
not grepped — `NAME##effort` / `NAME##thresh` are token-pasted).

Three guards we lacked:

1. **The ticks limit is polled INSIDE environment expansion, and occurrence walking is charged to the
   same budget.** cadical seeds `sweeper.current_ticks = 2 * clauses.size()` (`sweep.cpp:222`), charges
   `ticks += 1 + cache_lines(occs(lit).size(), …)` per occurrence list (`:1577-1580`), and
   `kitten_ticks_limit_hit` sums sub-solver *and* scan ticks against the limit (`:197-205`). kissat
   charges only `kitten_ticks` but polls its limit at seven sites inside the loops
   (`sweep.c:661,1131,1371,1379,1409,1417,1673`) and pushes the remainder into kitten before every
   environment (`set_kitten_ticks_limit`, `:53-60`). **This is the one we adopted.**
2. **A productivity delay ladder**, checked *before* any setup so a delayed call costs nothing:
   `if (kissat_average(eliminated, swept) < 0.001) BUMP_DELAY(sweep); else REDUCE_DELAY(sweep);`
   (`sweep.c:1718-1721`), consumed at `sweep.c:1655`; linear growth, geometric decay
   (`kimits.c:155-191`). cadical is identical (`sweep.cpp:1885,1951-1955`) and additionally *forfeits*
   the accumulated budget on a delayed call (`:1886`). **We still lack this** — recorded as an open
   gap, not fixed here, because our slot's delay ladder is gated on `!e.meter` (unmetered closures
   only) and wiring a metered entry into it is a `default_policy.cpp` change owned by another track.
3. **A resumable schedule** — `sweep_schedule` (the untried tail, `sweep.c:1531-1552,1612-1631`) plus a
   per-variable `flags.sweep` pending bit and `sweep_incomplete` (`:1503-1509,1568-1585`), so round
   *n+1* continues rather than re-sweeping the same prefix. **Implemented and MEASURED NEGATIVE — see
   §5.** cadical also adds `sweepthresh`·|clauses| as an accumulate-until-worthwhile floor
   (`limit.hpp:150-157`); our `EFFORT_SPECS` already has that mechanism (`thresh`) and Sweep's row does
   not use it — left alone here, since it is a dosing question and not the measured defect.

## 5. The resumption cursor: implemented, measured, REJECTED

Our Sweep had the exact defect kissat's `sweep_schedule` exists to prevent: a round is cut by its
budget after a handful of seeds, and `sweep_seeds` restarts at variable 0 every round, so later rounds
re-sweep the same prefix (`em_11` pre-D: 24 rounds × 6 seeds, all the same 6; `constraints_18`: 45
rounds × 1 seed, the same seed). It looked like pure waste.

It is not. A `Kernel::sweep_cursor` that resumes and wraps (same shape as `transitive_cursor`) was
built and measured:

| `em_11_3_4_cmp` arm (post-D, `97e9476`) | wall | environments | equivalences |
|---|---|---|---|
| Sweep off | 75.03 s | 0 | 0 |
| **with cursor** | **75.76 s** | 378 | **0** |
| without cursor (fix only) | 570.67 s | 2534 | 21 |

**With the cursor Sweep goes inert on `em_11`: 378 environments, zero equivalences, and the instance
reverts to its Sweep-off time.** The repetition was load-bearing — the formula evolves between rounds
(new binaries, new root-fixed variables), so re-visiting a seed later is re-examination under a richer
environment, not a repeat of the same query. kissat can afford to move on because its schedule is a
*small filtered, occurrence-ranked* candidate set that actually completes a pass (and `sweep_completed`
then *doubles* the environment caps); ours is every active variable, so a pass over ~10⁵ seeds at ~10
seeds/round never completes and the cursor is a one-way ticket away from the productive region.

**Not shipped.** The correct port is kissat's whole schedule — `scheduable_variable`'s occurrence
filter and ascending-occurrence rank, a completing pass, and `sweep_completed`-driven cap growth —
which is a rule-sized piece of work, not a cursor. Recorded as an open gap.

## 6. The fix

44 lines, entirely in `src/rules/sweep/`. No `EFFORT_SPECS` row changed; no policy change.

- `SweepBudget {start, limit}` on `SweepRound`, carrying the round's ceiling in `sweep_ticks`.
- `sweep_env_build` polls it at the BFS variable loop, the polarity loop, and — the load-bearing one —
  **inside the occurrence walk**, which is the unbounded term.
- The between-seeds check is unchanged and unconditional.
- Declining to expand is always sound: the environment only shrinks and stays a subset of `F`, which is
  the same argument every existing cap in the file already makes.
- `TACTIX_SWEEP_ENV_BUDGET=0` reverts *only* the inner poll, for the A/B arm.

## 7. Results

Post-Stage-D (`97e9476` + fix), node017, one core per task, `--baseline` arms in the same window.
`base` = `TACTIX_SWEEP_ENV_BUDGET=0` (shipped pre-fix behaviour), `fix` = shipped default,
`off` = `TACTIX_SLOT_DISABLE=Sweep`.

### 7.1 Wall, all four directive instances

| instance | `base` (shipped pre-fix) | **`fix`** | `off` | fix − base | fix − off |
|---|---|---|---|---|---|
| `constraints_18_0.3_2` | 720.5 s | **244.9 s** | 219.2 s | **−475.6 s** | +25.7 |
| `sat-bench-trig-bhaskara` | 364.0 s | **298.3 s** | 605.3 s | **−65.7 s** | **−307.0** |
| `em_11_3_4_cmp` | 562.5 s | **575.8 s** | 75.4 s | +13.3 | +500.4 |
| `minxor128` | **TO** (1200) | **TO** (1200) | 906.3 s | 0 | +343.7 |

**The fix recovers 475.6 s on the primary diagnostic instance and a further 65.7 s on `bhaskara`,
and destroys nothing**: `bhaskara` improves, `em_11` moves +13.3 s (inside this window's node noise —
`constraints_18`'s own `fix` arm reads 227.7 / 229.6 / 244.9 s across three waves, so ±15 s), and
`minxor128` is untouched.

**Two honesty notes about the directive's four-instance framing, both caused by Stage D.**
The tier table those four came from (`20260805-205741-C-sweepon`) predates Stage D.
- `em_11_3_4_cmp` was "Sweep wins −158 s" there. Post-Stage-D it is a Sweep **loss of ~490 s in BOTH
  arms** (base 562.5, fix 575.8, off 75.4). That is trajectory damage from 21 emitted equivalences at
  the PROBE clock's higher frequency, not overhead — Sweep's own wall on it is fractions of a second
  and its ledger discipline is 1.0007×. It is the `minxor128` mechanism, not the `constraints_18` one,
  and no governor change addresses it.
- `sat-bench-trig-bhaskara` was "Sweep wins −112 s"; post-Stage-D with the fix it wins **−307 s**.

### 7.2 Ledger discipline — the contention-invariant result

| instance | `handed` | `spent` (base) | **overshoot (base)** | `spent` (fix) | **overshoot (fix)** |
|---|---|---|---|---|---|
| `constraints_18_0.3_2` | 665,325,800 | 48,684,784,694 | **73.2×** | 665,651,482 | **1.0005×** |
| `sat-bench-trig-bhaskara` | 487,095,700 / 376,971,500 | 590,179,740 | **1.21×** | 379,006,881 | **1.0054×** |
| `em_11_3_4_cmp` | 1,481,590,400 | 1,541,505,234 | **1.04×** | 1,482,585,538 | **1.0007×** |
| `minxor128` | 2,472,752,200 / 2,352,103,200 | 2,518,451,904 | **1.02×** | 2,354,937,720 | **1.0012×** |

This is the shape the tier table showed and could not explain: the defect is **concentrated**, not
diffuse. Three of the four instances were already within 21 % of their dose; `constraints_18` was 73×
over it. A rule that respects its ledger on most instances and ignores it by two orders of magnitude on
one is exactly a rule whose ceiling is checked in the wrong place.

## 8. Reachability — Sweep still fires, and both proof paths still emit

`benchmarks/probes/scratch/sweepmeter/emitcount.sh`, 1497 generated cases per arm, shipped binary with
the fix, counted at the source ([[ab-arm-identical-artefacts]]):

| schedule | cases | rounds | environments | equivalences | binaries | not-RUP | **general (S-e)** |
|---|---|---|---|---|---|---|---|
| **DefaultPolicy** (shipped) | 1,497 | 1,468 | 7,092 | 3,441 | **3,558** | 1,955 | **293** |
| `--random-policy` (native DRAT) | 397 | 18,239 | 1,119,559 | 3,717 | **5,566** | 1,215 | **281** |
| `--random-policy --lrat` | 397 | 18,378 | 1,127,802 | 3,717 | **5,566** | 1,215 | **281** |
| `--sweep-stress-policy` | 1,497 | 3,797 | 180,552 | 20,336 | **29,310** | 6,855 | **1,174** |
| `--sweep-stress-policy --lrat` | 1,497 | 3,790 | 180,122 | 20,336 | **29,310** | 6,855 | **1,174** |
| `+ TACTIX_SWEEP_FORCE_GENERAL` | 1,497 | 3,584 | 173,253 | 29,396 | 4,568 | 29,396 | **2,284** |

Both emission paths are non-zero on the shipped default policy. A green fuzz leg over this is not
vacuous.

**Fuzz (shipped source, `build/debug` = -O1 + ASan/UBSan, one multi-core SLURM job per leg):**

| leg | cases | failures |
|---|---|---|
| `--random-policy`, native DRAT (`1584705`) | 6000 | **0** |
| `--random-policy --lrat`, native LRAT (`1584706`) | 6000 | **0** |
| `--sweep-stress-policy --lrat` (`1584707`) | 6000 | **0** |

Plus six legs on the immediately preceding source (`1584683`–`1584688`), all 0 failures, including
`--random-policy --no-binary` (ASCII DRAT, 4000) and `TACTIX_SWEEP_FORCE_GENERAL=1` (which makes S-d
decline so every pair routes through S-e). `ctest` **44/44**; `tests/sweep_proofs.sh` verifies 41
derivations through the real drat-trim **and** lrat-check; `micro_proofs` OK.

## 9. Tier C A/B spec (ready to run; NOT submitted)

Not run here — the directive reserves a full campaign. Everything below is pre-registered.

- **Window:** one same-window Tier C (30) run, node-pinned, `--baseline kissat --baseline cadical`
  (standing rule). Two pin dirs if a Tier B arm goes out concurrently (CLAUDE.md build-dir rule); build
  both from the same commit and check the binary shas match.
- **Arms (2), one binary, env hatch:**
  - `govon` = shipped default (in-environment budget enforcement ON).
  - `govoff` = `TACTIX_SWEEP_ENV_BUDGET=0` — reverts *only* the inner poll, so the arm is exactly the
    pre-fix scheduler and differs in nothing else.
  - Do **not** add a `TACTIX_SLOT_DISABLE=Sweep` arm to this run. Sweep ships ON; a rule-off arm is a
    diagnostic, not a candidate ([[no-rule-off-configs]]).
- **Pre-registered counter (reachability, checked BEFORE reading PAR-2):** on `govon`, over the tier,
  `c sweep` rounds > 0 and equivalences > 0 on at least the instances where `govoff` finds them, and
  `c sweepemit` binaries > 0. **If Sweep is inert on `govon` the run is void** — that is the failure
  mode this fix must not have.
- **Pre-registered discipline counter:** `c slot Sweep` `spent/handed` ≤ 1.05 on **every** instance in
  `govon`. This is contention-invariant and is the direct test of the mechanism; it should hold even
  where PAR-2 does not move.
- **Ship criterion:** `govon` ships unless it **loses a solve** relative to `govoff` outside the
  may-not-gate set (`mod4block`, `j3037`, `jgiraldezlevy`, `urq45`, `frb45`, `ContextModel`). PAR-2 is
  reported but does not gate on its own: read `#solved` first and the `--aggregate` fragility line
  before any sign claim (k=1 means the run has not answered the question).
- **Expected direction, from the four probes:** `constraints_18_0.3_2` −475 s,
  `sat-bench-trig-bhaskara` −66 s, `em_11_3_4_cmp` and `minxor128` unchanged. Instances where Sweep's
  `spent/handed` was already ~1 should be byte-identical; **that is the free control set** — if an
  instance with overshoot 1.00 in `govoff` moves at all, something other than this change moved.
- **Sequencing (recorded at the coordinator's request):** this run should precede the SCHED-D2 pricing
  A/B. Stage D multiplies Sweep's occasions, so pricing D2 over the pre-fix Sweep would re-measure this
  defect at higher frequency and attribute it to the clocks.

## 10. Artifacts

- Probe scripts + raw output: `benchmarks/probes/scratch/sweepmeter/` (regenerable).
- Waves: `w1` (pre-D instrumented decomposition), `w3` (post-D, instrumented), `w4` (post-D, shipped
  binary — reproduces `w3` within noise), `w5` (post-D, corrected `base` arm).
- **Correction, recorded because it invalidated one arm:** the first version of the
  `TACTIX_SWEEP_ENV_BUDGET=0` hatch disabled the round budget *entirely* rather than only the inner
  poll, so `w3`/`w4`'s `base` arm measured an **unbudgeted** Sweep, not the shipped baseline. `w5`
  re-runs `base` against the corrected hatch. The `fix` arms were never affected.
