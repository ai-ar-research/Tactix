# Independent-lineage experiment 2: Glucose blocking restarts — REVERTED

- Date: 2026-07-15. Hardware: Apple M3 Pro (6P+6E), 36 GB, macOS 15.7.7 (O1).
- Campaign: phase-3 independent-lineage revisitation, item (2); pure-policy
  change (rev-17 dividend — zero rule/kernel edits). Committed then reverted
  as a pair; implementation preserved in history.
- Baseline: HEAD (= e7a19ff source for src/tests).

## What was tested

Glucose's blocking restarts (Solver.cc "BLOCK RESTART (CP 2012)", MIT,
transliterated): at each conflict, record trail depth in a 5000-entry
window; past 10,000 conflicts, a depth > 1.4× the window average postpones
restarting ("we may be close to a model"). Recorded recompositions for our
two-mode kissat-shaped policy: focused-mode only (Glucose's single
frequent-restart regime maps to focused; blocking a reluctant-doubling Luby
sequence would corrupt it, and stable mode already plays the
stay-near-the-model role); block effect = reset fast glue EMA to the slow
value (Glucose fastclears its 50-entry LBD queue — same "signal must
re-diverge" semantics). Constants: R=1.4, window 5000, lower bound 10000
(Glucose defaults). Demo3 mirror extended identically (13/13 suites green,
demo3 stat-for-stat exact — blocking cannot fire under 10k conflicts on the
pinned instance, and the mirror carries the same arithmetic).

## Result: fails the Tier A precondition — no Tier B/C measurement (rev 18)

| Check | baseline | blocking |
|---|---|---|
| s055 solo | 24.8–25.7 s band | **28.30 s** (conflicts 932,762 → 1,037,268, +11%) |
| Tier A jobs=4 | 200/200 twice (PAR-2 0.57) | **199/200 twice** (PAR-2 0.75; s055 TIMEOUT 30.0 s both runs) |
| tr-n260-s002 solo | ~4.8 s | 5.16 s |
| 9vliw solo | 9.8 s | 9.65 s |

Blocking is ACTIVE and behaves as designed (measured with a temporary
counter: ~9.6k block events on 9vliw/823k conflicts, ~25k on s055/1.04M,
firing in continuous stretches while the trail stays deep — the same
per-conflict re-blocking Glucose exhibits). The damage is concentrated
exactly where the mechanism's premise inverts: on UNSAT
random-3SAT-transition instances a deep trail is not "near a model", and
postponing restarts delays refutation (s055 +11% conflicts). The industrial
references were flat-to-slightly-better (9vliw −0.15 s), consistent with
Glucose's own SAT-side motivation.

## Interpretation

- Rev-22/rev-18 discipline: an experiment that flips Tier A cannot be
  adopted regardless of its Tier B/C prospects; they were correctly never
  measured. Verdict: REVERT.
- The result does NOT say blocking is worthless — it says it is
  family-dependent with a sign flip (plausibly + on SAT-industrial, − on
  UNSAT-random), which is precisely the profile of a **phase-6 bandit arm**
  rather than a static default (§5.6; DynamicSAT's family-dependence
  lesson, and their DSAT-chrono-style Boolean arms are exactly this shape).
  Recorded as an O6 arm-set candidate: `blocking on/off` as a scheduled-
  level arm, implementation ready in history.
- kissat/CaDiCaL's omission of blocking is again empirically supported for
  a two-mode solver: stable mode covers the "stay near the model" need
  without sabotaging focused-mode refutation.

## §5.4 / §5.5 note

Pure policy state + one observation call; contract friction zero. Nothing
structural learned beyond the arm-candidate note above.
