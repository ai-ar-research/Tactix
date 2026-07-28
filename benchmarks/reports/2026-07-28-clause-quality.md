# Clause-quality decomposition vs kissat (phase-3 item 5)

**CORRECTION (same day, probe 1570503).** The first pass was taken on a build
whose `conflict_reuse` (TB4b(A)) was still defaulted ON — an experimental
configuration, not the shipped one. Every tactix number below is the re-measured
SHIPPED configuration; the TB4b(A) column is kept because the contrast is
informative. The finding survives, the magnitudes do not: FmlaEquiv is 2.49×
(reported 1.77×) and 9vliw is 1.15× (reported 0.94× — we are NOT ahead there).

**Date:** 2026-07-28 · **probes:** 1570461 / 1570464 / 1570503 · **binaries:**
`build/probe/tactix` (release + the new clause-quality counters), kissat 4.0.4
`-s` · single-instance runs, one core, no contention control — this is a
DECOMPOSITION probe, not a timing measurement, so allocation shape is not
load-bearing here.

## 1. The question

The Kissat gap has been attributed to "search quality" since TB5 reframed it
(per-propagation cost is at parity; the residual is search SHAPE). The named
symptom was FmlaEquiv's 1.53× conflicts. This probe asks what the clauses we
learn actually look like, before anyone touches minimization depth or retention.

## 2. The conflict gap is ONE instance, not a pattern

| instance | tactix (shipped) | kissat | ratio | tactix w/ TB4b(A) |
|---|---|---|---|---|
| FmlaEquivChain_4_6_6 | 938,595 | 377,701 | **2.49×** | 669,260 (−29 %) |
| 9vliw_bp_mc | 236,719 | 206,660 | **1.15×** | 195,297 (−17 %) |
| x9-08014 | 274,180 | 353,471 | **0.78×** | 274,180 (never fires) |

On x9 we need **fewer** conflicts than kissat and are still not faster, so there
the residual is per-conflict COST, not clause quality. 9vliw is near parity.
Only FmlaEquiv shows a real conflict deficit — and it is far worse than the
1.53× on record: **2.49×**.

**This retires the general reading of "our clause quality is worse"** — it is an
FmlaEquiv-family statement — while making that family's deficit bigger than
anyone had recorded.

## 3. On FmlaEquiv our learned clauses are bigger and much higher-glue

| | tactix (shipped) | kissat (end-of-run progress row) |
|---|---|---|
| mean learned size | **18.79 lits** | ~10 |
| mean learned glue | **7.99** | ~1.8 |
| minimize+shrink removal | **21.6 %** | — |

Same counters on the other two instances:

| instance | mean size | mean glue | minimize+shrink removal |
|---|---|---|---|
| FmlaEquivChain | 18.79 | 7.99 | **21.6 %** |
| 9vliw_bp_mc | 24.86 | 16.42 | 40.8 % |
| x9-08014 | 16.04 | 8.48 | 38.5 % |

The internal contrast is the sharper signal, and the correction widened it: our
own minimization removes **21.6 %** of resolved literals on FmlaEquiv against
~40 % on both other instances. Whatever makes FmlaEquiv hard for us defeats
minimize+shrink specifically.

**The depth cap is NOT the cause** (probe 1570502): `TACTIX_MINIMIZE_DEPTH` at
900 / 1000 / 100000 gives byte-identical conflicts, sizes and yields, so the
recursion never approaches the cap. That closes the standing deferred item
"`literal_redundant` depth cap 900 vs 1000" as measured-dead — the 900-vs-1000
near-miss cannot be costing anything here.

**TB4b(A) is the one lever measured to move it:** conflict-reuse takes FmlaEquiv
from 938,595 → 669,260 conflicts and its yield from 21.6 % → 27.2 %. It does not
pay tier-wide (see PROGRESS), but on this family the mechanism is right.

**Caveat, stated because it limits the comparison:** kissat's size/glue in the
progress table are EMAs over a recent window, ours are cumulative means over the
whole run. A 2×/4× gap is larger than that mismatch plausibly explains, but the
kissat column should be re-measured from an equivalent counter before anyone
sizes a fix off it. That re-measurement is the first step of any follow-up, not
an optional refinement.

## 4. What this rules in and out

- **Rules OUT** a global clause-quality deficit: x9 (0.78×) and 9vliw (1.15×) refute it.
- **Rules IN** minimization effectiveness on the FmlaEquiv family as the next
  lever — and gives it a measurable target (**21.6 % → ~40 %**) rather than the
  vague "improve clause quality".
- **Rules OUT** the minimization DEPTH CAP as the mechanism (probe 1570502).
- **Leaves open** the per-conflict cost question on 9vliw/x9, where we win on
  conflicts and still do not win on time. That is a different investigation
  (TB5's reframe) and should not be folded into this one.

## 5. Instrumentation added

`Stats::learned_lits`, `learned_glue_sum`, `learned_raw_lits` and two printed
lines (`c learnedsize`, `c reduction`). Counters only — no trajectory effect,
verified by the unchanged Tier A/B numbers in the same session.
