# Tier D: FastEliminate-fidelity code vs no-MAB and the reference solvers

Final, complete comparison: all 1,166 Tier D instances classified, and every proof-check memory-out
rechecked at 64 GB.

| Arm | Code | Campaign |
|---|---|---|
| Tactix, this run | `0f7c10174` (FastEliminate fidelity), Release `a7c66bab…` | `benchmarks/runs/20261010-tierd-fastel` (job 1770131) + `benchmarks/runs/20261011-tierd-fastel-recheck-64g` (job 1771469) |
| Tactix no-MAB | `d170b4a5` | `benchmarks/runs/20260928-am546-no-mab-tactix-hybrid-tierd-cpuq` |
| Satsuma+Kissat (paired with no-MAB) | — | `benchmarks/runs/20260928-am547-satsuma-hybrid-tierd-cpuq` |
| Reference solvers, after their 8 h / 64 GB reruns | — | `benchmarks/baselines/sc24-26-32g-5000s` + the three rerun campaigns of [the baseline rerun](../2026-10-08-tierd-baseline-rerun-fixed64/README.md) |

Every arm: 4 CPUs, 32 GB, 5,000 s solve limit. This run: cpu-q, 8 h proof-check window, automatic
proof routing, 64 GB recheck of every checker memory-out (same 8 h window), throttle `%150` (user).
No-MAB was checked with a 4 h window; the "verified ≤ 4 h" row counts only checks that finished
within 14,400 s, for a like-for-like comparison.

## Headline

| | No-MAB | This run |
|---|---|---|
| Solved | 1,005 (484 SAT, 521 UNSAT) | **1,006** (481 SAT, 525 UNSAT) |
| Verified, 8 h checks + 64 GB recheck | — | **999** (481 SAT, 518 UNSAT) |
| Verified, checks ≤ 4 h | 986 | **990** |
| Verified PAR-2 sum (unverified = 10,000 s) | 2,230,498 | **2,110,652** (−5.4%) |
| Same, checks ≤ 4 h | 2,230,498 | **2,178,771** (−2.3%) |

Instance level against no-MAB: 17 gained (12 SAT, 5 UNSAT), 16 lost (15 SAT, 1 UNSAT).

## 64 GB recheck

The 7 proof checks that ran out of memory at 32 GB were re-solved and rechecked at 64 GB; 6 verify.

| Instance | Solve (s) | Check (s) | Result |
|---|---|---|---|
| SGI_30_70_22_70_7 | 3,972 | 18,478 | verified (drat-trim → CakeLPR, peak 61 GB) |
| BubbleVsPancakeSort_9_4 | 4,858 | 17,604 | verified |
| lec_mult_CvW_12x12 | 3,529 | 10,493 | verified (dsr-trim → Trestle) |
| pcmax-scheduling-m19 (UNSAT) | 4,878 | 17,135 | verified |
| lec_mult_DvW_12x12 | 4,302 | 15,142 | verified |
| grs-256-64 | 1,168 | 9,304 | verified |
| FmlaImplyChain_3_7_8 | 3,663 | 1,035 | **not verified**: dsr-trim exceeded 64 GB (peak 67 GB) |

Unverified solves remaining (7): FmlaImplyChain (checker memory), and six 8 h checker timeouts
(cfi-rigid-s2-0064, REGRandom-K4-L2-Seed35, crafted_n12_d6_c4, ncc_none_21015, oisc nested-11 and
-13; the oisc and ncc certificates are the known never-verifying ones).

## Against every reference solver

One scorer for the references: the frozen baseline runner's rule (a SAT answer counts only if the
model checks **and** the output follows the competition protocol), applied to the frozen campaign
with every 8 h / 64 GB rerun record substituted. PAR-2 counts unverified as 10,000 s.

| Solver | Solved | Verified | Verified PAR-2 sum |
|---|---|---|---|
| **Tactix, this run** | **1,006** | **999** | **2,110,652** |
| Tactix no-MAB (4 h checks) | 1,005 | 986 | 2,230,498 |
| Satsuma+Kissat | 965 | 946 | 2,634,766 |
| kissat-sc2024 | 890 | 883 | 3,299,728 |
| kissat-4.0.4 | 881 | 872 | 3,371,597 |
| AE-Kissat-MAB | 875 | 863 | 3,428,275 |
| LymphoSAT | 947 | 852 (920 counting 68 correct models with non-protocol output) | 3,501,270 |

The [five-arm collection](../2026-10-01-tierd-five-arm-terminal-results.md) predates the reruns
and scored LymphoSAT's models leniently; its numbers (e.g. LymphoSAT 863, kissat-sc2024 860) are
therefore not directly comparable to this table.

## Gained instances (vs no-MAB)

`*` = solved but proof not verified.

| Instance | Exp | No-MAB | This run | Satsuma+Kissat |
|---|---|---|---|---|
| par32-2 | S | TO | S 3,215 | TO |
| ramsey_4_4_18 | U | TO | U 0 | U 1,089 |
| ITC2021_Late_10 | S | TO | S 3,194 | TO |
| lightsout totalizer corner | U | TO | U 1 | TO |
| 1x1x14_1x2x9_pair | S | TO | S 4,477 | TO |
| noL-11-10 | S | TO | S 46 | S 102 |
| par32-4-c | S | TO | S 1,632 | TO |
| pcmax m24 (SAT) | S | TO | S 362 | S 805 |
| SDP_132_18 | S | TO | S 1,286 | TO |
| ramsey_4_4_19 | U | TO | U 0 | U 216 |
| FmlaImplyChain_3_7_8 | U | TO | U 3,755* | TO |
| toughsat_factoring_inf | S | TO | S 1,812 | TO |
| SC25_Timetable …E_48…T_50 | S | TO | S 2,822 | S 401 |
| pcmax m19 (UNSAT) | U | TO | U 4,878 | U 2,708 |
| mp1-Nb7T42 | S | TO | S 63 | S 107 |
| abw-K-dwt__234 | S | TO | S 27 | S 4,186 |
| SC25_Timetable …E_50…T_50 | S | TO | S 2,595 | S 4,370 |

## Lost instances (vs no-MAB)

| Instance | Exp | No-MAB | This run | Satsuma+Kissat |
|---|---|---|---|---|
| ntil-90d-39 | S | S 4,990 | TO | TO |
| par32-4 | S | S 975 | TO | TO |
| st_540_25_12_12_9 | S | S 3,293 | TO | TO |
| Break_18_32 | S | S 4,581 | TO | TO |
| bp4_TCO_CSO_ZR | S | S 739 | TO | S 749 |
| VanDerWaerden_2-3-14_186 | U | U 4,963* | TO | TO |
| noL-11-8 | S | S 44 | TO | S 2,182 |
| SDP_134_18 | S | S 4,576 | TO | TO |
| pcmax m15 | S | S 2,319 | TO | S 356 |
| SDP_140_19 | S | S 3,145 | TO | S 1,633 |
| bp4_LPI_FPBEQ_ZR | S | S 1,809 | TO | S 2,149 |
| vdwb_k6_n300 | S | S 881 | TO | TO |
| SC25_Timetable …E_48…T_58 | S | S 862 | TO | TO |
| xorshift_r15_191 | S | S 3,217 | TO | TO |
| 6g_6color_366_050_04 | S | S 940 | TO | S 18 |
| pcmax m12 | S | S 4,510 | TO | TO |

The SAT losses are the search-trajectory divergences attributed in
[the divergence bisect](../2026-10-10-search-divergence-bisect/README.md); the FastEliminate
mechanism (M1) is fixed in this code, and the remaining mechanisms (retired phase hint, residual
seed handoff, always-admitted symmetry unit) and the initial-probing difference stay open. noL-11-8
is the accepted §8.4 exception.

## Limits

- One run per configuration. Near-limit SAT instances flip with trajectory changes, so single
  gained/lost rows are not evidence about any one mechanism.
- The reference solvers' rows come from earlier time windows than this run.
