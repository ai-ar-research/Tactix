# Independent-lineage experiment 3: Glucose adaptive reduce interval — REVERTED

- Date: 2026-07-15. Hardware: Apple M3 Pro (6P+6E), 36 GB, macOS 15.7.7 (O1).
- Campaign: phase-3 independent-lineage revisitation, item (3). Committed
  then reverted as a pair; implementation preserved in history.
- Baseline: HEAD (= e7a19ff source); Tier B baseline = the C1-commit
  3-run medians (source-identical binary).

## What was tested

Glucose's adaptive reduce interval (Solver.cc reduceDB, MIT,
transliterated): the next reduction is postponed by specialIncReduceDB
(=1000 conflicts) per DB-health signal — glue ≤ 3 at the deletion boundary
(worst kept clause) and glue ≤ 5 for the best-ranked clause. Architecture
shape: the Reduce rule's canonical ranking published the two signals as
stats (TACTIX_STAT_REDUCE_BOUNDARY_GLUE/BEST_GLUE, additive ids 17/18 —
the §5.6 observables pattern), the policy read them in update_reduce_limit,
and the demo3 mirror consumed them through the API (rev-21 mechanism
working as designed; demo3 stayed stat-for-stat exact). Recorded
deviations: our candidate list excludes reasons and used/tier-protected
clauses (glucose ranks all learnts), and the boundary sits at our canonical
volume, not glucose's fixed half.

## Validation (green) and verdict (revert at Tier B)

13/13 suites; fuzz default 1000/0, random-policy 400/0, native-LRAT 400/0;
Tier A 200/200 twice, PAR-2 0.57/0.57, s055 25.0/25.7 s jobs=4 (solo
24.02 s — below the baseline band; conflicts −7% on that instance).

Tier B, 3-run per-instance medians (jobs=4): **baseline 11/14 PAR-2 166.1
→ adaptred 10/14 PAR-2 210.5** (run spread < 3 points — the loss is
systematic, not noise):

| Mover | baseline | adaptred |
|---|---|---|
| mod4block (SAT) | 58.9 | **TIMEOUT** |
| circuit | 99.4 | 146.6 |
| mp1-blockpuzzle | 216.9 | 233.8 |
| summle | 3.3 | 17.2 |
| best win | FmlaEquivChain −1.0 s | (noise-level) |

Tier C was deliberately not run: the campaign judges on Tier B medians +
Tier C, and a consistent lost-solve + 44-point Tier B deficit makes
adoption impossible regardless of Tier C (cheapest-tier discipline).

## Interpretation

The signals double-count DB health in this architecture. Glucose ranks ALL
learnts, so "boundary glue ≤ 3" genuinely means the DB is full of good
clauses. Our kissat-lineage retention (5-bit used recency + glue tiers +
promotion, landed with S2/T1) already removes good clauses from the
candidate list before ranking — the same health information has been acted
on — so the glucose bumps merely postpone cleaning an already-filtered
residue: the DB bloats, propagation pays (circuit/mod4block are exactly
the clause-heavy losers). kissat's omission of interval adaptation is
again empirically supported: used-bit retention supersedes it. The s055
improvement (random-transition class, few good clauses, bumps rarely
fire) does not transfer.

Campaign scoreboard after 3 of 4: measured 3, adopted 0, reverted 3 —
each with a recorded architectural reason (trajectory variance /
premise inversion on UNSAT-random / health double-counting). The
independent-lineage survey's shapes keep CONFIRMING our landed
implementations rather than beating them, which is itself the survey
obligation paying out.

## Phase-6 note

The observables pattern (rule ranking → stats → policy/mirror) worked
exactly as §5.6 needs — first live exercise of the reward-substrate
mechanism. The two stat ids revert with the experiment (no reader without
the bump; rev-20 principle) but the pattern and implementation are proven
and in history for the phase-6 arm sets.
