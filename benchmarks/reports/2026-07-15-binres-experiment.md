# Independent-lineage experiment 1: Glucose binResMinimize — REVERTED

- Date: 2026-07-15. Hardware: Apple M3 Pro (6P+6E), 36 GB, macOS 15.7.7 (O1).
- Experiment commit: the binres implementation (analyze.cpp binres_minimize;
  Glucose 4.2.1 `minimisationWithBinaryResolution` transliterated, MIT) —
  committed then reverted in the same pair so the implementation (incl. the
  LRAT chain design) stays in history for the recorded revisit.
- Campaign: phase-3 independent-lineage revisitation, item (1)
  (`docs/independent-lineage-survey.md`); protocol = adopt-or-revert on
  Tier B 3-run per-instance medians + Tier C, jobs=4 throughout.
- Baseline: HEAD e7a19ff. Both Tier C runs at the pre-rev-29 timeout
  (3000 s) — internally comparable; not comparable to future 1200 s runs.

## What was tested

Self-subsuming resolution of the learned clause against binaries containing
the asserting literal (size ≤ 30, full-clause LBD ≤ 6 — Glucose defaults),
after the recursive/shrink passes. Absent from kissat 4.0.4 AND CaDiCaL;
survey verdict was CANDIDATE with the recorded counter-argument that the
Kissat lineage deliberately never adopted it.

## Correctness validation (all green; not the reason for the revert)

13/13 unit suites (ASan/UBSan; new test_binres; demo3 mirror stat-exact);
fuzz default 1000/0, random-policy 400/0, native-LRAT 400/0; LRAT verified
on tr-n260-s002 and 9vliw (14,313 binres removals in its chains — the
binary-at-trail-position chain construction holds at scale); deterministic
repeats; Tier A 200/200 twice, PAR-2 0.57/0.57, s055 27.4/27.7 s (in band).

## Performance

| Protocol | baseline | binres | verdict |
|---|---|---|---|
| Tier B, 3-run medians | 11/14, PAR-2 166.1 | 11/14, **161.1** | mild win |
| Tier C (30, jobs=4, 3000 s) | **29/30, 491.10** | 28/30, 733.68 | clear loss |
| Tier C geomean, 28 solved-both | 1.0× | **1.143×** | 14% slower |

Tier B detail (medians): wins mod4block −30.6 s, FmlaEquivChain −19.1,
circuit −17.9, blockpuzzle −13.5, 9vliw −2.7 (8.25 s single-run best);
losses crypto1 +11.3, summle +2.6, rbsat +2.2. Same solved set.

Tier C detail: binres LOSES mp1-Nb5T15 (SAT; 274.0 s → timeout) and keeps
the baseline's 128_125 timeout. Large losses: circuit +495.6 s, constraints_25
+484.0, Ptn-7824 +477.8, frb45 +315.6, ascon +228.0, constraints_18 +225.3,
af-synthesis +176.3. Large wins exist too (DLTM −219.0, jgiraldez −166.2,
StConn −119.2, goldcrest −118.7) but 12 losses vs 11 wins with a heavier
loss tail. binres fired throughout (9vliw: 14.3k lits; php3 micro: 1).

## Interpretation

- The Tier B +5 PAR-2 points do not survive Tier C, the first tier where
  claims are statistically meaningful (§8.1) and the campaign's designated
  judge. Net effect there: −1 solve, +242.6 PAR-2, 1.143× geomean.
- The per-instance swings (±200–500 s both directions) look like trajectory
  reshuffle, not scan overhead (the watch-list walk is gated and cheap) —
  removing 1–2 literals from small learned clauses changes search paths
  more than clause strength. On this evidence the technique is
  high-variance, slightly net-negative in our solver.
- This retroactively supports the Kissat lineage's deliberate omission
  (survey counter-argument confirmed) and matches DynamicSAT's (CP 2025)
  finding that localized clause-level heuristics are the least valuable
  class to adapt. The independent-lineage campaign working as designed:
  measured, recorded, reverted.
- Caveat recorded honestly: Tier C was single-run per side (repeats at
  3000 s cost ~2 h each); the mp1 timeout and the loss tail could partially
  be run noise. The verdict stands on protocol; the revisit below is the
  hedge.

## Revisit trigger (user instruction, 2026-07-15)

Re-measure binres after C3 Vivify (and the V1/E1 slot) lands: the original
hypothesis was that vivification absorbs binres's payoff — the technique's
value may change (in either direction) once the inprocessing slot is
populated and trajectories stabilize. The implementation survives in git
history (this experiment's commit pair); the LRAT chain construction for
binary-resolution minimization is proven and reusable.

## §5.4 / §5.5 note

No revamp implication — the experiment fit the rule contract cleanly (one
function + call site in analyze.cpp; zero kernel/policy changes). Tooling
observation for phase 3: Tier C runs are dominated by drat-trim (baseline:
3603 s check time for 8.5 GB of proofs inside a ~67 min-solve run) —
run.py checking native LRAT with lrat-check is the recorded candidate.
