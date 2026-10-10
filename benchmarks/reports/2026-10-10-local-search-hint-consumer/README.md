# Local-search hint consumer: st_540 regression cause and Tier D exposure

**Date:** 2026-10-10. **Status:** read-only analysis. No code or plan change was made (user decision:
report only).

## Question

On Tier D row `st_540_25_12_12_9.af` (SAT), the no-MAB baseline `d170b4a5` prints
`c localsearch 1 hints offered, 35 applied` and solves in 3,293 s. The final API-gap code
(`benchmarks/runs/20261010-tierd-apigap-final`, Release `d540ef32…`) prints
`1 hints offered (79 phases, 0 replacements), 0 applied / 0 target installs from 0 attempts` and
times out at 5,000 s. Kissat-SC2024 solves the row in 3,021 s; Satsuma+Kissat times out.

## Cause

The hint is the Affine producer's partial candidate (`c affine 1 calls, 82 equations, 1 candidates`,
79 known phases). In `d170b4a5`, DefaultPolicy ran `LocalSearchGuidance` (S8) immediately before every
rephase Walk slot, rotating DirectCsp/Affine/Parity/Onehot/BalancedSignature. That S8 copied the raw
candidate into **saved and target** phases, and Walk then started from that state. "35 applied" means
35 of the 40 walks changed phase memory. The current code has no DefaultPolicy consumer for Affine
(or Parity/Onehot/Balance/DirectCsp) candidates. They are published and then never read.

| Commit | Date | Effect on the hint path |
|---|---|---|
| `84e8d197a` "Retire semantic constructors behind certified actions" | 10-03 | Retires constructor publication and constructor-specific phase guidance from active scheduling (the Affine stage as well: 0 `c affine` calls at this commit). Sets `rephase.structural_walk_seed = false` and empties the rotation. S8 becomes target-only, and seeding moves to a private Walk import (`WalkRequest.seed_candidate`). |
| `156afef6b`, `60446aa0d`, `a49191bfa` | 10-04 | Restore LymphoSAT SAT constructors and the default Affine opportunity. Connect partial models to search guidance through private seeded Walk, default on. |
| `a9f1e14d4` "Keep structural Walk seeding opt in" | 10-04 | Sets the default back to off "until DefaultPolicy owns an admission rule and a preparation allowance separate from Walk search work". |
| `b56b1b0ce` | 10-07 | Adds the public split-budget Walk profile (FullCurrentCnf/StrictlyBetter, `walk_preparation_budget`, walk3, C v13). DefaultPolicy adoption stays pending. |
| `555ecb4b0` | 10-09 | Removes the off-by-default config entirely. DefaultPolicy always runs unseeded Walk. |

The bisection (`benchmarks/probes/scratch/search-divergence-bisect`, `r1`/`r2b/classify.tsv`) puts
both st_540 (row 234, first divergence at conflict 2,788) and `pcmax-scheduling-m12` (row 1002,
conflict 2,694) at `84e8d197a`. Its parent `a2f90364c` matches the baseline. At the st_540 divergence,
the parent picks external variable 238 with `target=1|saved=1` and the child with `target=-1|saved=-1`.
That is a phase-memory difference, not an implication difference. The full parent run prints
`1 hints offered, 14 applied`; the child prints `0 hints offered, 0 applied` with the same 14 walks.

**Intent.** Dropping the consumer was deliberate and documented. It was not an accidental side effect:

- `docs/winner-integration-plan.md`, "General constructor guidance and CDCL phases", says: "DefaultPolicy
  runs unseeded Walk; its unused structural-seeding diagnostic is removed. Random and future learned
  policies retain the exact seeded action; a DefaultPolicy consumer would first need a reviewed
  admission rule and separate preparation/search allowances."
- [Partial-model guidance ablation](../2026-10-05-partial-model-guidance-ablation.md) (21 oddball rows,
  5,000 s), relative to `d170b4a5`:
  - Raw seeded Walk followed by a raw target reinstall: +28.4%, plus one checker allocation failure.
  - Private Walk-only seed: +12.0%.
  - No seed: +70.0%, which that report calls "guidance removal rejected".

  The ablation kept no-seed as the regression-safe default only while the split-budget replacement was
  developed. That replacement exists as a public action (`b56b1b0ce`) but has never been adopted by
  DefaultPolicy.

The only DefaultPolicy S8 consumer today is graph labeling's
(`DefaultPolicy::install_graph_labeling_hint`). It installs only an authenticated, exact-objective
improvement of the GraphLabeling producer. Five rows below use it (`graphlabel` producer).

## Tier D exposure

Rows that offer at least one hint in either run:

- 85 rows in total: 78 in `d170b4a5` and 85 in the final code.
- Producers: 49 Affine, 10 Affine+Balance, 11 Balance, 5 Parity, 2 Affine+Parity, 3 DirectCsp,
  5 GraphLabeling (one also Parity).
- In the final code, 80 of the 85 rows offer a hint that is never applied.

Campaigns compared:

| Arm | Campaign |
|---|---|
| Before | `benchmarks/runs/20260928-am546-no-mab-tactix-hybrid-tierd-cpuq` |
| After | `benchmarks/runs/20261010-tierd-apigap-final` |
| Satsuma+Kissat | `benchmarks/runs/20260928-am547-satsuma-hybrid-tierd-cpuq` |
| Kissat-SC2024 | `benchmarks/runs/20260928-am540-four-winner-tierd-cpuq` |

All arms ran on 4 CPUs with 32 GB and a 5,000 s limit.

**63 rows where the `d170b4a5` consumer actually applied the hint:**

| Arm | Solved | PAR-2 sum (10,000 s penalty) |
|---|---|---|
| Before | 57 | 111,653 s |
| After | 54 | 131,189 s (+17.5%) |
| Satsuma+Kissat | 56 | — |
| Kissat-SC2024 | 57 | — |

**Losses.** All 3 hint-row losses had the consumer active before:
- st_540 (Kissat-SC2024 solves it).
- `pcmax-scheduling-m12-…-SAT` (no reference solves it).
- `lec_mult_DvW_12x12` (both references solve it).

**Gains.** None of the 3 hint-row gains depended on the old consumer:
- `pcmax-scheduling-m37` and `lightsout_sat_24` timed out before, with no statistics.
- `abw-K-dwt__234` had 0 applied before and is now solved through the graph-labeling install.

**Slower and faster rows.** Rows count if the runtime changed by more than 1.5× and more than 10 s;
all of these had the consumer active before:
- 5 slower: `lightsout_sat_23` 509→1,878 s, `oddball_29_4_ttf` 943→2,163 s, `oddball_26_4_ttf`,
  `oddball_24_5_ttf`, `pcmax-m26`.
- 4 faster: `stb_792_333` 154→4 s, `oddball_112_5_ttf` 182→2 s, `oddball_19_4_ttf`,
  `lec_mult_CvW_12x11`.

**Whole Tier D, for comparison.** 18 rows lost and 13 gained across 1,149 shared rows
([report](../2026-10-10-tierd-no-mab-vs-final/README.md)). The hint rows account for 3 of the losses
and 3 of the gains.

**Caveat.** Before and after differ by every commit from `d170b4a5` to `50c619192`. Only rows 234 and
1002 are bisected to `84e8d197a`. The other row-level changes are associations, not attributions.
Taken together, the evidence is consistent with the ablation: losing the consumer costs more than
st_540 alone (two more lost rows and five materially slower ones, all consumer-active). Some
consumer-active rows are faster now, so the old raw mechanism was not uniformly helpful either.

`hint_rows.py` regenerates the table below from the four campaigns.

- **Class** uses the same thresholds as above.
- **Applied** shows `c localsearch … applied`, before / after.
- **✗** marks a solved row whose certificate was not verified (a checker crash or OOM, the same on
  both arms).
- **TO** means timeout.
- **—** means the result has no statistics.

| Class | Row | Exp | Producer | Applied before / after | Before (d170b4a5) | After (final) | Satsuma+Kissat | Kissat-SC2024 |
|---|---|---|---|---|---|---|---|---|
| lost | `st_540_25_12_12_9.af` | SAT | affine | 35 / 0 | SAT 3293 | TO | TO | SAT 3021 |
| lost | `pcmax-scheduling-m12-8049-55035-SAT` | SAT | affine | 40 / 0 | SAT 4510 | TO | TO | TO |
| lost | `lec_mult_DvW_12x12` | UNSAT | affine | 36 / 0 | UNSAT 4106✗ | TO | UNSAT 4069✗ | UNSAT 4003 |
| gained | `pcmax-scheduling-m37-28831-324346-SAT` | SAT | affine | — / 0 | TO | SAT 3504 | TO | TO |
| gained | `lightsout_sat_24_unbounded_direct_corner_0_unsat` | UNSAT | parity | — / 0 | TO | UNSAT 3579 | TO | TO |
| gained | `abw-K-dwt__234.mtx-w50` | SAT | graphlabel | 0 / 2 | TO | SAT 28 | SAT 4186 | SAT 1788 |
| slower | `lightsout_sat_23_unbounded_direct_random50_2_unsat` | UNSAT | parity | 15 / 0 | UNSAT 509 | UNSAT 1878 | UNSAT 1340 | UNSAT 1365 |
| slower | `oddball_24_5_ttf.normalised` | UNSAT | affine,balance | 14 / 0 | UNSAT 139 | UNSAT 325 | UNSAT 297 | UNSAT 231 |
| slower | `pcmax-scheduling-m26-6398-62377-UNSAT` | UNSAT | affine | 12 / 0 | UNSAT 95 | UNSAT 173 | UNSAT 72 | UNSAT 108 |
| slower | `oddball_26_4_ttf.normalised` | UNSAT | affine,balance | 26 / 0 | UNSAT 779 | UNSAT 1263 | UNSAT 763 | UNSAT 1174 |
| slower | `oddball_29_4_ttf.normalised` | UNSAT | affine,balance | 26 / 0 | UNSAT 943 | UNSAT 2163 | UNSAT 1011 | UNSAT 3434 |
| faster | `lec_mult_CvW_12x11` | UNSAT | affine | 27 / 0 | UNSAT 1554 | UNSAT 881 | UNSAT 1506 | UNSAT 1745 |
| faster | `stb_792_333.apx_0` | SAT | affine | 13 / 0 | SAT 154 | SAT 4 | SAT 60 | SAT 482 |
| faster | `oddball_19_4_ttf.normalised` | UNSAT | affine,balance | 12 / 0 | UNSAT 65 | UNSAT 29 | UNSAT 55 | UNSAT 61 |
| faster | `oddball_112_5_ttf.normalised` | SAT | balance | 12 / 0 | SAT 182 | SAT 2 | SAT 174 | SAT 615 |
| both-unsolved | `lec_mult_CvK_12x12` | UNSAT | affine | — / 0 | TO | TO | TO | TO |
| both-unsolved | `pcmax-scheduling-m24-17855-226744-SAT` | SAT | affine | 35 / 0 | TO | TO | SAT 805 | SAT 2678 |
| both-unsolved | `pcmax-scheduling-m43-38782-385402-SAT` | SAT | affine | 44 / 0 | TO | TO | TO | TO |
| both-unsolved | `sgp_7-4-6.shuffled-as.sat05-2671` | SAT | affine | 38 / 0 | TO | TO | TO | TO |
| both-unsolved | `1-ET-256-K-55` | UNSAT | affine | 40 / 0 | TO | TO | TO | TO |
| both-unsolved | `lec_mult_DvK_12x12` | UNSAT | affine | 34 / 0 | TO | TO | TO | TO |
| both-unsolved | `pcmax-scheduling-m19-10199-62102-UNSAT` | UNSAT | affine | 40 / 0 | TO | TO | UNSAT 2708 | TO |
| same | `lec_mult_KvW_12x11` | UNSAT | affine | 29 / 0 | UNSAT 3839 | UNSAT 3637 | UNSAT 4044 | UNSAT 3623 |
| same | `lightsout_sat_25_unbounded_direct_planted250_4_sat` | SAT | affine,parity | 0 / 0 | SAT 0 | SAT 0 | SAT 445 | SAT 449 |
| same | `mrpp_4x4#12_12` | UNSAT | affine | 4 / 0 | UNSAT 9 | UNSAT 7 | UNSAT 7 | UNSAT 7 |
| same | `gm24sparrc` | UNSAT | affine | 0 / 0 | UNSAT 4 | UNSAT 4 | UNSAT 5 | UNSAT 1 |
| same | `stb_495_168.apx_2_DC-AD` | UNSAT | affine | 13 / 0 | UNSAT 154 | UNSAT 120 | UNSAT 151 | UNSAT 112 |
| same | `marg5x5.shuffled-as.sat03-1455` | UNSAT | parity | 0 / 0 | UNSAT 0 | UNSAT 0 | UNSAT 0 | TO |
| same | `pcmax-scheduling-m13-2011-12813-UNSAT` | UNSAT | affine | 14 / 0 | UNSAT 142 | UNSAT 101 | UNSAT 116 | UNSAT 81 |
| same | `oddball_53_5_tto_zp.normalised` | SAT | balance | 0 / 0 | SAT 6 | SAT 6 | SAT 4179 | SAT 1804 |
| same | `oddball_54_5_tto_zp.normalised` | SAT | balance | 0 / 0 | SAT 6 | SAT 6 | SAT 552 | SAT 2357 |
| same | `oddball_80_5_tto_zp.normalised` | UNKNOWN | balance | 0 / 0 | SAT 14 | SAT 14 | TO | TO |
| same | `oddball_52_5_tto_zp.normalised` | SAT | balance | 0 / 0 | SAT 6 | SAT 6 | SAT 1222 | SAT 4922 |
| same | `oddball_26_5_ttf.normalised` | UNSAT | affine,balance | 27 / 0 | UNSAT 879 | UNSAT 737 | UNSAT 687 | UNSAT 869 |
| same | `QG7-gensys-icl006.sat05-3132.reshuffled-07` | UNSAT | affine | 12 / 0 | UNSAT 268 | UNSAT 213 | UNSAT 108 | UNSAT 197 |
| same | `rook-42-0-1` | UNSAT | graphlabel | 0 / 1 | UNSAT 648 | UNSAT 748 | UNSAT 655 | UNSAT 827 |
| same | `lec_mult_DvK_11x10` | UNSAT | affine | 15 / 0 | UNSAT 422 | UNSAT 394 | UNSAT 389 | UNSAT 412 |
| same | `gensys-ukn004.shuffled-as.sat05-3844` | UNSAT | affine | 14 / 0 | UNSAT 383 | UNSAT 469 | UNSAT 485 | UNSAT 373 |
| same | `oddball_69_5_tto_zp.normalised` | SAT | balance | 0 / 0 | SAT 11 | SAT 11 | SAT 1198 | TO |
| same | `1-ET-256-K-65` | UNSAT | affine | 10 / 0 | UNSAT 143 | UNSAT 151 | UNSAT 122 | UNSAT 149 |
| same | `oddball_70_5_tto_zp.normalised` | SAT | balance | 0 / 0 | SAT 11 | SAT 11 | SAT 4376 | SAT 4301 |
| same | `SGI_30_70_22_70_7-dir.shuffled-as.sat03-154` | UNSAT | directcsp | 37 / 0 | UNSAT 3769✗ | UNSAT 3648✗ | UNSAT 3387✗ | UNSAT 3612✗ |
| same | `lec_mult_CvD_11x11` | UNSAT | affine | 21 / 0 | UNSAT 496 | UNSAT 435 | UNSAT 484 | UNSAT 428 |
| same | `rook-52-0-1` | UNSAT | graphlabel | 0 / 1 | UNSAT 2351 | UNSAT 2237 | UNSAT 2691 | UNSAT 2856 |
| same | `gm16spctrc` | UNSAT | affine | 15 / 0 | UNSAT 701 | UNSAT 705 | UNSAT 571 | UNSAT 590 |
| same | `oddball_51_5_tto_zp.normalised` | SAT | balance | 0 / 0 | SAT 6 | SAT 6 | SAT 202 | SAT 820 |
| same | `oddball_67_5_tto_zp.normalised` | SAT | balance | 0 / 0 | SAT 10 | SAT 10 | SAT 1239 | SAT 2068 |
| same | `1-TC-256-K-64` | UNSAT | affine | 19 / 0 | UNSAT 647 | UNSAT 624 | UNSAT 669 | UNSAT 569 |
| same | `1-TC-256-K-68` | UNSAT | affine | 9 / 0 | UNSAT 98 | UNSAT 100 | UNSAT 94 | UNSAT 110 |
| same | `at-least-two-vmpc_28` | SAT | graphlabel,parity | 22 / 1 | SAT 1603 | SAT 1292 | SAT 1482 | SAT 1583 |
| same | `lec_mult_CvK_12x11` | UNSAT | affine | 28 / 0 | UNSAT 2938 | UNSAT 3086 | UNSAT 3014 | UNSAT 3162 |
| same | `WS_500_16_90_70.apx_2_DC-AD` | UNSAT | affine | 17 / 0 | UNSAT 326 | UNSAT 241 | UNSAT 326 | UNSAT 223 |
| same | `lec_mult_KvW_11x10` | UNSAT | affine | 15 / 0 | UNSAT 453 | UNSAT 469 | UNSAT 433 | UNSAT 412 |
| same | `SGI_30_60_20_50_3-dir.shuffled-as.sat03-114` | UNSAT | directcsp | 22 / 0 | UNSAT 921 | UNSAT 951 | UNSAT 762 | UNSAT 782 |
| same | `lightsout_sat_22_unbounded_direct_planted150_4_sat` | SAT | affine,parity | 0 / 0 | SAT 0 | SAT 0 | SAT 98 | SAT 99 |
| same | `stb_588_138.apx_1_DC-AD` | UNSAT | affine | 18 / 0 | UNSAT 408 | UNSAT 412 | UNSAT 398 | UNSAT 370 |
| same | `1-TC-256-K-71` | UNSAT | affine | 7 / 0 | UNSAT 71 | UNSAT 80 | UNSAT 56 | UNSAT 62 |
| same | `rook-56-1-1` | UNSAT | graphlabel | 0 / 1 | UNSAT 608 | UNSAT 635 | UNSAT 504 | UNSAT 500 |
| same | `SGI_30_60_19_60_6-dir.shuffled-as.sat03-112` | UNSAT | directcsp | 22 / 0 | UNSAT 943 | UNSAT 982 | UNSAT 791 | UNSAT 800 |
| same | `lec_mult_CvW_12x12` | UNSAT | affine | 34 / 0 | UNSAT 3034✗ | UNSAT 3804✗ | TO | UNSAT 3360 |
| same | `oddball_56_5_tto_zp.normalised` | SAT | balance | 0 / 0 | SAT 7 | SAT 7 | TO | SAT 2533 |
| same | `lec_mult_DvW_12x11` | UNSAT | affine | 30 / 0 | UNSAT 2090 | UNSAT 2156 | UNSAT 1618 | UNSAT 2179 |
| same | `oddball_20_5_ttf.normalised` | UNSAT | affine,balance | 3 / 0 | UNSAT 5 | UNSAT 6 | UNSAT 5 | UNSAT 6 |
| same | `schooltt-5-3-8-2-4-5-10-0.1-0.2-seed4` | UNSAT | affine | 15 / 0 | UNSAT 263 | UNSAT 209 | UNSAT 106 | UNSAT 104 |
| same | `WS_500_16_70_10.apx_2_DC-AD` | UNSAT | affine | 18 / 0 | UNSAT 397 | UNSAT 370 | UNSAT 389 | UNSAT 344 |
| same | `oddball_22_5_ttf.normalised` | UNSAT | affine,balance | 7 / 0 | UNSAT 21 | UNSAT 12 | UNSAT 3 | UNSAT 13 |
| same | `stb_495_168.apx_1_DC-AD` | UNSAT | affine | 13 / 0 | UNSAT 160 | UNSAT 157 | UNSAT 181 | UNSAT 140 |
| same | `urq45` | UNSAT | parity | 0 / 0 | UNSAT 0 | UNSAT 0 | UNSAT 0 | UNSAT 151 |
| same | `stb_418_125.apx_2_DC-AD` | UNSAT | affine | 22 / 0 | UNSAT 747 | UNSAT 646 | UNSAT 685 | UNSAT 799 |
| same | `Schur_161_5_d38` | UNSAT | affine | 42 / 0 | UNSAT 4552 | UNSAT 3987 | UNSAT 4072 | UNSAT 4001 |
| same | `lec_mult_DvW_11x10` | UNSAT | affine | 17 / 0 | UNSAT 330 | UNSAT 297 | UNSAT 247 | UNSAT 292 |
| same | `lec_mult_CvK_11x10` | UNSAT | affine | 14 / 0 | UNSAT 359 | UNSAT 395 | UNSAT 358 | UNSAT 366 |
| same | `lec_mult_CvW_11x10` | UNSAT | affine | 15 / 0 | UNSAT 237 | UNSAT 227 | UNSAT 159 | UNSAT 225 |
| same | `oddball_13_5_ttf.normalised` | UNSAT | affine,balance | 7 / 0 | UNSAT 25 | UNSAT 22 | UNSAT 19 | UNSAT 17 |
| same | `hwb-n24-02-S786928571.shuffled-as.sat03-1618` | UNSAT | parity | 12 / 0 | UNSAT 89 | UNSAT 95 | UNSAT 94 | UNSAT 108 |
| same | `oddball_17_5_ttf.normalised` | UNSAT | affine,balance | 2 / 0 | UNSAT 4 | UNSAT 4 | UNSAT 1 | UNSAT 1 |
| same | `stb_418_125.apx_1_DC-AD` | UNSAT | affine | 23 / 0 | UNSAT 783 | UNSAT 721 | UNSAT 680 | UNSAT 743 |
| same | `pcmax-scheduling-m30-14113-167638-UNSAT` | UNSAT | affine | 22 / 0 | UNSAT 931 | UNSAT 910 | UNSAT 444 | UNSAT 726 |
| same | `lec_mult_KvW_10x10` | UNSAT | affine | 11 / 0 | UNSAT 172 | UNSAT 167 | UNSAT 164 | UNSAT 156 |
| same | `1-ET-256-K-70` | UNSAT | affine | 8 / 0 | UNSAT 88 | UNSAT 71 | UNSAT 70 | UNSAT 58 |
| same | `oddball_57_5_tto_zp.normalised` | SAT | balance | 0 / 0 | SAT 7 | SAT 7 | SAT 1451 | TO |
| same | `lec_mult_CvK_11x11` | UNSAT | affine | 20 / 0 | UNSAT 1076 | UNSAT 1051 | UNSAT 1048 | UNSAT 1203 |
| same | `oddball_24_4_ttf.normalised` | UNSAT | affine,balance | 18 / 0 | UNSAT 235 | UNSAT 165 | UNSAT 347 | UNSAT 262 |
| same | `1-TC-256-K-63` | SAT | affine | 9 / 0 | SAT 113 | SAT 143 | SAT 143 | SAT 97 |
| same | `gm16sparrc` | UNSAT | affine | 1 / 0 | UNSAT 2 | UNSAT 2 | UNSAT 0 | UNSAT 1 |

## Options for restoring a DefaultPolicy consumer

Each option changes default behaviour, so each needs `revise-plan` (the plan text above), a
`calculus`/implementation-notes update for the S6/S8 consumer contract, full Debug, both-policy fuzz
and the four §8.4 gates. None has a Kissat reference: the mechanism comes from LymphoSAT and Tactix,
so N4 parity is against `d170b4a5`/LymphoSAT practice.

| Option | Mechanism | For | Against |
|---|---|---|---|
| (a) Faithful `d170b4a5` | Before each rephase Walk slot, import the rotated candidate into saved+target, then run ordinary Walk from it | Only option likely to reproduce the st_540 / pcmax-m12 trajectory past the bisected divergence. Matches the configuration that solved the 3 lost rows. | Contradicts the current plan text and the ablation's conclusion that raw candidates must stay out of CDCL phase state. Needs a new S8 payload (saved+target) or a Walk raw-start profile. Repeated raw inheritance is linked to the +28.4% / checker-failure arm and to the 4 rows that are faster without it. |
| (b) Split-budget seeded Walk | DefaultPolicy calls the existing public FullCurrentCnf/StrictlyBetter Walk with `seed_candidate`. Admission comes from the walk3 objective comparison. Preparation allowance is derived separately from Walk search effort. | The plan's named replacement, already public, typed, replayed and fuzzed. Raw candidates never reach CDCL; only a strictly better complete Walk result exports. Closest to the ablation's Walk-only arm (+12% vs `d170` on oddballs, against +70% for no seed). | The st_540 trajectory will not match `d170b4a5`, and recovery of the 3 lost rows is unproven. Needs a derived (not tuned) preparation allowance and admission rule. |
| (c) Target-only S8 per new publication | At a natural full-root window, install a new candidate once as targets through public S8, mirroring `install_graph_labeling_hint` (admission: new publication with an exact or improved objective) | Smallest change, with a DefaultPolicy precedent and its counters (`c localphase`). Leaves saved/best and Walk untouched, and ordinary target capture supersedes it. | Differs from `d170b4a5` (Walk does not start from the candidate; stable-mode saved phases are unchanged). Affine/Balance candidates are partial, and their objectives are producer-local, so the admission rule needs a reviewed definition. Effect on the lost rows is unknown. |

The plan treats (b) and (c) as independent consumers, so they can be combined. Choosing between them
needs a matched enabled/disabled arm on the 85 hint rows above plus the standing gates. The 12-row
identity set stays byte-identical wherever a row offers no hint.
