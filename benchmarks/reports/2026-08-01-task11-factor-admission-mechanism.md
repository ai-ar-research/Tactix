# Task #11 — the extension-variable mechanism: located, and the fix refuted

**Date** 2026-08-01 · **git** `172bacf` · **Binary** `build/pin-t11-c/tactix` (frozen; probes and
tier runs share it) · **Allocation shape** partition `cpu-dense-hwulab-q`, 1 cpu/task, 8 GB/cpu,
throttle 48, every arm writing and checking DRAT. Frozen on landing.

**Status: the mechanism is IDENTIFIED and reference-anchored; the candidate fix is REFUTED at tier
level; broad Factor admission's Tier B value is REPRODUCED in a clean factorial.**

Supersedes the routed lever of `2026-07-31-factor-broad-admission-profiling.md` §6.

---

## 1. The four things that were wrong with the routed lever

The 2026-07-31 diagnosis routed task #11 to "*when* Factor runs". Source-verified against the
reference clones, that routing does not survive:

1. **`factorthresh` is dead code in cadical** — declared `options.hpp:125`, never read anywhere in its
   source. The probe `--factordelay=0 --factorthresh=0` therefore measured `factordelay` alone.
2. **`factordelay`'s polarity was read backwards.** It is an allowance *added* to the elimination-round
   count (`factor.cpp:941`: `delay_limit = eliminations + delay`, skip when `log_active > delay_limit`;
   kissat `factor.c:1074` is the same shape). `=0` is the **most-delayed** setting, so the probe showed
   that delaying Factor **more** costs cadical 2.3–9× — the opposite of the reading it was given.
3. **The gate never binds on either loser, for either solver.** log10(13,797) = 4 ≤ 0+4 (ContextModel),
   log10(2,200) = 3 ≤ 0+4 (jgiraldez); our analogue (`default_policy.cpp:1176`) admits at
   active < 10⁵ with zero BVE rounds. **We already match cadical here exactly.**
4. **The first-dose allowance is inert** (kissat `factoriniticks` 700M, which we copied, vs cadical
   300M). Probe 1577487: bit-identical on all three mechanism instances. Not a skipped arm — the new
   `c factor … ticks` counter shows Factor's whole-solve spend is 104.4M / 10.9M / 30.9M ticks, far
   under either constant. Banked as **L19**.

## 2. The mechanism, anchored to the reference

**Cadical's extension-decision share, measured on the same instances** (probe 1577499; patched copy
built *outside* the repo per L14, same version 3.0.0; `stats.decisions_extension` counted in `decide()`
against a `max_var` snapshot at `Internal::solve` entry — and configured **without** `-q`, which
compiles the statistics out entirely):

| instance | ours (broad admission) | cadical | |
|---|---|---|---|
| ContextModel | **31.39 %** | **0.10 %** | **314×** |
| jgiraldezlevy | 0.38 % | 0.01 % | 38× |
| sted5 | 6.19 % | **8.81 %** | cadical branches **more** |

**Cadical does not suppress extension variables — it discriminates**, near-zero where ext branching is
poison and *more than us* where ext vars are essential, while adding **more** of them overall (627 vs
459). A static priority cannot produce that shape, which is why all five prior knobs traded one
instance for another.

**The channel is reason-side bumping** (probe 1577502; counters `Stats::bumps{,_extension,_ext_reason_only}`):

| instance | bumps per EXT var vs per ORIGINAL var | ext bumps that are REASON-SIDE ONLY |
|---|---|---|
| ContextModel | 133,575 vs 44,911 = **2.97× more** | **71.68 %** |
| jgiraldezlevy | 70,211 vs 244,820 = 0.29× | 46.38 % |
| sted5 | 5,458 vs 12,369 = 0.44× | 36.16 % |

A BVA extension variable *is* the hub of the clauses it replaced, so it appears in a great many
reasons. Reason-side bumping promotes it for its **structural position** rather than for evidence, and
the promotion self-reinforces (promoted → decided more → in more reasons → promoted more). That is why
one-shot burial cannot hold: our demotion moves the share only 31.4 % → 32.2 %.

## 3. The fix — and its refutation

`TACTIX_NO_EXT_REASON_BUMP` (`Kernel::no_ext_reason_bump`) withholds extension variables from the
reason-side channel only; conflict-side bumping is untouched, so an ext var still rises when it earns
it. N1-clean and shippable in principle (reason-side bumping is `H`-only advice with no proof
obligation; the arm changes only the ORDER candidates are offered in, never which are legal).

**Single-instance probe (1577506) looked excellent**, and was misleading:

| instance | broad | broad + arm | |
|---|---|---|---|
| ContextModel | 403.1 s / 31.39 % | 166.3 s / 2.36 % | 2.4× |
| jgiraldezlevy | 425.8 s / 0.38 % | 34.5 s / 0.10 % | 12.3× |
| sted5 | 16.8 s | 312.6 s | 0.05× |

**Tier B, 4-arm factorial, one window** (`20260801-105456`) — the decisive measurement:

| arm | PAR-2 | solved |
|---|---|---|
| shipped | 105.65 | 24/28 |
| **broad admission alone** | **64.15** | **26/28** |
| **the arm alone** | **127.42** | **23/28** |
| broad + arm | 113.45 | 24/28 |
| *kissat* | *9.91* | *28/28* |

**Tier C** (`20260801-092459`): shipped **323.88** (29/30) vs cadical **315.38** = **1.027×**
(T3's CaDiCaL half met, consistent with the 1.007–1.066 band); **broad + arm 468.43 (27/30)** — it
loses `mp1-Nb5T15` (65.8 → TO) and `j3037` (351.2 → TO), plus frb45 +343 s.

**Verdict: the arm is independently harmful.** Alone, with no broad admission at all, it costs +21.8
Tier B PAR-2 and a solve. It is not trading sted5 for the losers; it is a tier-wide negative on both
tiers. Default stays OFF; kept as a §5.6 arm because the *mechanism* it isolates is real and measured.

## 4. Two extrapolation errors of mine, both worth banking

1. **A tier estimate from a two-instance decomposition.** From ContextModel −288 s and jgiraldez
   −431 s I predicted Tier B ≈ 87 and Tier C ≈ 334. Measured: 113.45 and 468.43. The other 26/28
   instances moved, including `DecompressReader_read_12` 8.5 → 300 TO. This is L12/L16 exactly,
   committed again.
2. **The wrong baseline.** The probe compared the arm against *broad admission*, the configuration it
   was repairing — a 2.4× win on ContextModel. Against the **shipped default**, which is what a ship
   decision is measured against, the same instance is **19.1 s → 189.6 s, still 10× worse**. Banked as
   **L21**: measure a candidate fix against the shipped default, not against the broken arm it repairs.

Reinforcing both, and banked separately as **L20**: ContextModel is rock-stable under replication
(20.06 / 19.97) and ~20× fragile under *any* perturbation — TB4b's conflict reuse costs it 19.1×, a
change with nothing to do with Factor. At the default we solve it in 19–20 s against **cadical's
64.2 s**, i.e. 3× faster. Its +464 s under broad admission is therefore partly fragility, and task
#11's founding framing ("our Factor is 7–28× worse than cadical's") is overstated.

## 5. Where task #11 actually stands

- **The Tier B prize is real and reproduced**: broad Factor admission alone is **64.15 vs 105.65,
  +2 solves** (`sted5`, `or_randxor`) in a same-window factorial — an independent confirmation of the
  2026-07-31 result (65.86, 26/28).
- **Its blocker remains Tier C**, and the blocker is now better characterised: not "when Factor runs"
  (matched), not the first-dose allowance (inert), not a formula-shape separator (0 for 5), and not
  reason-side priority (refuted here).
- **What survives**: the *discrimination* cadical exhibits, with a named channel, a per-instance
  feature already instrumented (`c extbumps` reason-side share), and a measured reference target.
  Every static setting of it has now been measured. Per L18's precedent, **the exhaustion is the
  result**: E2 Factor admission is the best-labelled §5.6 dosing exhibit in the repo, and the decision
  belongs to the phase-6 learner rather than to a sixth constant.

## 6. Artefacts

Landed default-off / inert, mod4block identity re-confirmed at **418,796** conflicts on every build:
- `c factor … ticks` (`Stats::factor_ticks` on the stats line) — what distinguished "bound the opening
  burst" from "inert" in §1.4
- `c extbumps` + `Stats::bumps{,_extension,_ext_reason_only}` — the channel attribution of §2
- `Kernel::no_ext_reason_bump` / `TACTIX_NO_EXT_REASON_BUMP` (default off) — the §3 arm
- `DefaultPolicy::FACTOR_INIT_TICKS` / `TACTIX_FACTOR_INIT_TICKS` — the extracted first-dose allowance

Probes `benchmarks/probes/t11-{initticks,cadical-extshare,extbumps,reasonbump}.*` (jobs 1577487,
1577499, 1577502, 1577506). Tier runs `20260801-092452-B`, `20260801-092459-C`, `20260801-105456-B`.
Patched reference: `/home/hwu/cadical-extdiag` (outside the repo, never committed).

**Caveat on absolute numbers:** these windows shared the partition with a concurrent TB4b Tier C array,
so absolute PAR-2 is not comparable to earlier windows. Within-window A/B and the cadical ratio are
valid — cadical was timed in the same window under the same contention.
