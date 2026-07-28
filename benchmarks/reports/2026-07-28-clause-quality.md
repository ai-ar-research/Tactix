# Clause-quality decomposition vs kissat (phase-3 item 5)

**Date:** 2026-07-28 · **probes:** 1570461 / 1570464 · **binaries:**
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

| instance | tactix conflicts | kissat conflicts | ratio |
|---|---|---|---|
| FmlaEquivChain_4_6_6 | 669,260 | 377,701 | **1.77×** |
| 9vliw_bp_mc | 195,297 | 206,660 | **0.94×** |
| x9-08014 | 274,180 | 353,471 | **0.78×** |

On two of the three named residual carriers we need **fewer** conflicts than
kissat and are still not faster — so on those the residual is per-conflict COST
or trajectory luck, not clause quality. Only FmlaEquiv shows a real conflict
deficit, and it got worse than the 1.53× on record (1.77× here).

**This retires the general reading of "our clause quality is worse".** It is an
FmlaEquiv-family statement.

## 3. On FmlaEquiv our learned clauses are bigger and much higher-glue

| | tactix | kissat (end-of-run progress row) |
|---|---|---|
| mean learned size | **19.66 lits** | ~10 |
| mean learned glue | **7.67** | ~1.8 |
| minimize+shrink removal | **27.2 %** | — |

Same counters on the instances where we are AHEAD on conflicts:

| instance | mean size | mean glue | minimize+shrink removal |
|---|---|---|---|
| FmlaEquivChain | 19.66 | 7.67 | **27.2 %** |
| 9vliw_bp_mc | 26.95 | 17.36 | 38.1 % |
| x9-08014 | 16.04 | 8.48 | 38.5 % |

The internal contrast is the sharper signal: our own minimization removes
**27.2 %** of resolved literals on FmlaEquiv against ~38 % on both other
instances. Whatever makes FmlaEquiv hard for us defeats minimize+shrink
specifically.

**Caveat, stated because it limits the comparison:** kissat's size/glue in the
progress table are EMAs over a recent window, ours are cumulative means over the
whole run. A 2×/4× gap is larger than that mismatch plausibly explains, but the
kissat column should be re-measured from an equivalent counter before anyone
sizes a fix off it. That re-measurement is the first step of any follow-up, not
an optional refinement.

## 4. What this rules in and out

- **Rules OUT** a global clause-quality deficit: 9vliw and x9 refute it.
- **Rules IN** minimization effectiveness on the FmlaEquiv family as the next
  lever — and gives it a measurable target (27.2 % → ~38 %) rather than the
  vague "improve clause quality".
- **Leaves open** the per-conflict cost question on 9vliw/x9, where we win on
  conflicts and still do not win on time. That is a different investigation
  (TB5's reframe) and should not be folded into this one.

## 5. Instrumentation added

`Stats::learned_lits`, `learned_glue_sum`, `learned_raw_lits` and two printed
lines (`c learnedsize`, `c reduction`). Counters only — no trajectory effect,
verified by the unchanged Tier A/B numbers in the same session.
