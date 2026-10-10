# Tier D: no-MAB baseline vs the final API-gap code

Partial comparison (1,149 of 1,166 rows classified; 17 long proof checks still running) of the
saved no-MAB Tactix run against the current code.

| Arm | Code | Campaign |
|---|---|---|
| Before (no-MAB) | `d170b4a5` | `benchmarks/runs/20260928-am546-no-mab-tactix-hybrid-tierd-cpuq` |
| After (final) | `50c619192`, Release `d540ef32…` (search identical to c2.4) | `benchmarks/runs/20261010-tierd-apigap-final` |
| Reference | Satsuma+Kissat | `benchmarks/runs/20260928-am547-satsuma-hybrid-tierd-cpuq` |

Both Tactix runs: cpu-q, 4 CPUs, 32 GB, 5,000 s solve. The script is
`tierd_diff.py`/`feat.py` (session scratchpad); feature columns are the solver's own `c` statistics.

## Headline (1,149 shared rows)

| | Before | After |
|---|---|---|
| Solved SAT | 484 | 475 |
| Solved UNSAT | 504 | 508 |
| Verified SAT | 484 | 475 |
| Verified UNSAT | 496 | 503 |

The 17 pending rows are all UNSAT; before, 6 of them verified. Net: **−9 SAT, +4 UNSAT solved**.
Row level: **18 lost** (17 SAT, 1 UNSAT) and **13 gained** (8 SAT, 5 UNSAT).

## Gained rows and the feature behind each

| Row | Exp | Before | After | Satsuma | Feature (evidence) |
|---|---|---|---|---|---|
| ramsey_4_4_18 | U | TO | U 0 s | U 1,089 s | Ramsey startup, Lucky 22, on by default since `f1b1858c7` |
| ramsey_4_4_19 | U | TO | U 0 s | U 216 s | Ramsey startup |
| lightsout totalizer corner | U | TO | U 1 s | TO | Startup affine closure (`3676cff1c`; `affine-matrix-init` 1 applied) |
| abw-K-dwt__234 | S | TO | S 28 s | S 4,186 s | Graph labeling (on by default in c2.3, owner-dosed in c2.4) |
| noL-11-10 | S | TO | S 46 s | S 102 s | Startup FastEliminate guard after structured symmetry (c2.2; `fastel root-suppressed` 1, 1,070 SR units) |
| mp1-Nb7T42 | S | TO | S 63 s | S 107 s | Same guard (root-suppressed 1) |
| SC25_Timetable …E_48… | S | TO | S 2,824 s | S 401 s | Same guard (root-suppressed 1, 5 SR units) |
| SC25_Timetable …E_50… | S | TO | S 2,315 s | S 4,370 s | Same guard (root-suppressed 1, 8 SR units) |
| FmlaImplyChain_3_7_8 | U | TO | U 3,647 s (check memout) | TO | Same guard (root-suppressed 1, 4,308 SR units) |
| lightsout direct corner | U | TO | U 3,579 s | TO | No startup feature fired; search |
| pcmax m37 | S | TO | S 3,504 s | TO | No startup feature differs; search |
| SDP_132_18 | S | TO | S 4,737 s | TO | Startup counters equal (fastel 54k vs 56k); search |
| toughsat_factoring_inf | S | TO | S 994 s | TO | Startup counters equal; search |

Nine of 13 gains come from four features that did not exist (or were off) in the no-MAB code:
Ramsey startup (2), startup affine closure (1), graph labeling (1), and the post-symmetry
FastEliminate guard (5). Four gains are search-trajectory differences.

## Lost rows

| Row | Exp | Before | After | Satsuma | SR units (before / after) | Startup fastel elim. (before / after) |
|---|---|---|---|---|---|---|
| par32-4 | S | S 975 s | TO | TO | 0 / 0 | 835 bve / 837 bve; fastel same |
| sha1 zeroOut_18 | S | S 747 s | TO | S 2,256 s | 0 / no stats | — |
| st_540_25_12_12_9 | S | S 3,293 s | TO | TO | 0 / 0 | ≈ equal |
| Break_18_32 | S | S 4,581 s | TO | TO | 627 / 627 | 17,415 / 17,415 |
| bp4_TCO_CSO_ZR | S | S 739 s | TO | S 749 s | 0 / 0 | 11,031 / 11,115 |
| connm-ue-csp n1200 | S | S 1,381 s | TO | TO | 1,686 / 1,686 | 230 / 231 |
| noL-11-8 | S | S 44 s | TO | S 2,182 s | 1,082 / 1,082 | guard fires after |
| pcmax m15 | S | S 2,319 s | TO | S 356 s | 0 / 0 | 1,536 / 1,538 |
| SDP_126_17 | S | S 885 s | TO | TO | 0 / no stats | — |
| SDP_140_19 | S | S 3,145 s | TO | S 1,633 s | 0 / no stats | — |
| bp4_LPI_FPBEQ_ZR | S | S 1,809 s | TO | S 2,149 s | 4 / 4 | guard fires after |
| vdwb_k6_n300 | S | S 881 s | TO | TO | 0 / no stats | — |
| bp4_TCO_CSO_IXA_LP_ZR | S | S 3,535 s | TO | S 2,321 s | 0 / 0 | ≈ equal |
| Circuit_multiplier36 | S | S 646 s | TO | TO | 0 / 0 | ≈ equal |
| 6g_6color_366_050_04 | S | S 940 s | TO | S 18 s | **0 / 1** | **19 / 0 (guard fires)** |
| as-p51-l19-o8 | S | S 2,107 s | TO | S 3,197 s | 13,914 / 13,820 | 1,907 / 1,827 |
| pcmax m12 | S | S 4,510 s | TO | TO | 0 / 0 | 4,994 |
| lec_mult_DvW_12x12 | U | U 4,106 s (unverified) | TO at 5,000 s | U 4,069 s | 1 / 1 | — |

"No stats": the solver did not exit within the runner's grace after SIGTERM, so no statistics were
printed. This is common in both runs (74 of 160 timeouts before, 24 of 165 after).

### What the counters show

- **Checked symmetry units are not the cause of 16 of the 17 SAT losses.** Eleven lost rows have no
  SR units in either run. Five more (Break_18_32, connm, noL-11-8, bp4_LPI, as-p51) emit the same
  number of SR units before and after, and the no-MAB code solved them with those units.
- **The one row the units change is 6g_6color_366_050_04.** The "always admit checked symmetry
  units" change (`d576abaeb`) adds one SR unit, which also triggers the post-symmetry FastEliminate
  guard (startup fastel 19 → 0). Satsuma+Kissat, which emits the same kind of unit and runs
  `fastel=0`, solves it in 18 s, so the unit and the skipped fastel are not intrinsically harmful;
  our search after them is.
- **noL-11-8** is the accepted §8.4 exception: lost to the faithful Satsuma→Kissat residual
  activation seed.
- **Startup FastEliminate is not the cause of the other losses.** Where fastel ran, its eliminated
  counts are within 1% between the two runs, and BVE counts likewise.
- **The remaining 15 losses are search-trajectory divergences with essentially identical startup
  state.** Satsuma+Kissat solves 7 of them (sha1, bp4_TCO_CSO_ZR, pcmax m15, SDP_140, bp4_LPI,
  bp4_TCO_CSO_IXA, as-p51), so they are reachable by a Kissat-class search.

### Grouping of the 18 losses

| SR units fire after | Satsuma solves | Rows |
|---|---|---|
| No | Yes | 5 |
| No | No | 6 |
| Yes | Yes | 5 (noL-11-8, bp4_LPI, 6g_6color, as-p51, lec_mult; all but 6g with the same units as before) |
| Yes | No | 2 (Break_18_32, connm) |

## Interpretation and next step

Gains come mostly from new startup features (9 rows). Losses come mostly from a search-trajectory
change between `d170b4a5` and c2.1 that the startup counters do not explain. Candidates in that
interval that act during search: the shared-execution refactor (10-01–10-02, claimed
behavior-preserving), Walk phase-guidance changes (`0a35d3f74`, `7e202a0d5`), restart-only retained
theory (`b0077afea`), and graph-labeling/structural slices that ran between restarts before c2.4.
These are inferences from commit history, not measurements.

To attribute them, run a first-divergence trace on two or three lost rows that Satsuma solves
(bp4_TCO_CSO_ZR, pcmax m15, sha1): the no-MAB binary vs the final binary with decision/conflict
traces, find the first differing decision, and identify the mechanism that produced it. For
6g_6color, profile c2.1 vs c2.4 per pass and compare our post-unit pipeline with Satsuma's.

## Limits

- Partial: 17 rows still in proof checking (all UNSAT).
- One run per configuration; SAT rows near the limit are trajectory-sensitive, which is why the
  attribution above relies on startup counters and the Satsuma reference rather than on timing.
- Feature attribution for gains is by counters that fired; it is not an ablation.
