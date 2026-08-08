# Independent-lineage experiment 4: CMS usage-count Reduce ranking — REVERTED

- Date: 2026-07-15. Hardware: Apple M3 Pro (6P+6E), 36 GB, macOS 15.7.7 (O1).
- Campaign: phase-3 independent-lineage revisitation, item (4) — the final
  item. Committed then reverted as a pair; implementation preserved.
- Baseline: HEAD (= e7a19ff source). Tier B baseline = C1-commit medians;
  Tier C baseline = today's tactix-base run censored to the new 1200 s cap
  (28/30, PAR-2 406.63 — caveat: a censored 3000 s run's contention profile
  differs slightly from a native 1200 s run; second-order here).

## What was tested

CMS-inspired usage-count victim ranking (cryptominisat reducedb.cpp
SortRedClsUIP1, MIT): rank unprotected Reduce candidates by how often each
clause participated in conflict analysis since the last reduction —
fewest uses die first; the previous (glue, size) order becomes the
tie-break. Counter: 8 bits saturating, carved from the glue field's header
headroom (header stays 5 words); bumped in Analyze's mark_used; reset per
reduction (CMS's "in this RDB" window). Rule-side only — zero policy/demo3
changes. **Survey correction recorded** (docs/independent-lineage-survey.md):
CMS ships this ranking only in STATS_NEEDED/FINAL_PREDICTOR builds; its
competition default rotates plain glue/activity sorts. We measured the
usage-count variant as the interesting delta, caveat recorded.

## Validation (green) and verdict

13/13 suites; fuzz default 1000/0 + random-policy 400/0 + native-LRAT
400/0; Tier A 200/200 twice (PAR-2 0.56/0.57); solos s055 25.8 s (band
edge), n260 4.25, 9vliw 10.64.

| Protocol | baseline | uipred |
|---|---|---|
| Tier B 3-run medians | 11/14, **166.1** | 11/14, 173.7 |
| Tier C (jobs=4, 1200 s) | **28/30, 406.63** | 27/30, 466.05 |

Tier B: broad mild loss (9 instances slower — blockpuzzle +42, crypto1
+15, mod4block +15, qwh 0.6→11.7; only circuit −12 better). Tier C: SCPC
lost outright (591.9 → TO); big losses frb45 +793, Grain +494,
constraints_18 +251, urq45 +196. REVERTED per protocol.

## The signal worth keeping: extreme family dependence

Tier C simultaneously contained the campaign's LARGEST wins:
**mod2c 339.6 → 29.7 s (beats kissat's 32.7)** and **ContextModel 424.6 →
166.4** — two of the three ≥10× kissat-gap instances from the morning's
first Tier C read — plus mp1-Nb5T15 274.0 → 2.3, jgiraldez −256,
minxor −213, StConn −167, Ptn −114 (10 wins > 80 s vs 6 losses > 80 s).
Usage-ranking is not a worse ranking; it is a DIFFERENT ranking whose
value flips sign by family with huge magnitude. That is precisely the
§5.6 scheduled-arm profile — ranking ∈ {glue-size, usage} is now the
single strongest measured arm candidate for the phase-6 bandit (stronger
than blocking restarts), and CMS's own default — ROTATING glue/activity
sorts — reads as their static answer to the same observation.

## Campaign close-out (4 of 4 measured)

| # | Technique | Failing gate | Architectural reason |
|---|---|---|---|
| 1 | binResMinimize | Tier C | trajectory-reshuffle variance, net-negative |
| 2 | blocking restarts | Tier A | deep-trail premise inverts on UNSAT-random |
| 3 | adaptive reduce interval | Tier B | health signals double-count vs used-bit retention |
| 4 | usage-count ranking | Tier B+C | family-dependent sign flip, net-negative as a static default |

0 adopted, 4 reverted, every revert with a recorded mechanism — and three
of the four are filed as phase-6 arm candidates (2, 3-as-family-arm, 4).
The campaign's actual product: our landed kissat-lineage shapes survived
four independent-lineage challenges under measurement, and the learned-
scheduling module now has concrete, measured arms with known per-family
behavior instead of speculative ones. The survey obligation (decision (9cecad5)) is
discharged for the currently-landed feature set; remaining survey items
(rotation presets, best_inv, MAB) stay phase 4/6 as planned.

## §5.4 / §5.5 note

No revamp implications; all four experiments fit the contracts cleanly
(two pure-policy, two rule-local). Tooling drag recorded again: Tier C
proof checking (77 min drat-trim for 8.4 GB in this run) — the run.py
native-LRAT/lrat-check switch remains the top tooling candidate.
