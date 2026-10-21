# CG: CryptoMiniSat-guided benchmark collection

CG contains 23 independently checked Tier-D instances selected from the complete pinned CryptoMiniSat 5.14.7 four-core, 5,000-wall-second census. It is a representation-first development target for bringing one instance-independent Tactix DefaultPolicy within 2× the checked CMS time in the paper-comparable proof mode. Whole-run PBP wall time is evaluated separately against a verified like-for-like PBP baseline, not directly against the proof-free CMS time.

Twenty-five is a soft budget, not a quota. The refresh removes redundant rows and keeps 23 instances spanning 16 normalized families. No family contributes more than three rows; the three `mdp` rows represent sizes 28, 32, and 36. The set covers 18 model-checked SAT and five independently proved UNSAT instances, major/exclusive wins and useful diversity cases, and sub-second through 100+ second CMS targets.

All performance rows come from the proof-free census. SAT models were checked in that run. Proof-free UNSAT reports were admitted only where a separately hash-bound proof-producing CMS result independently verified the same compressed input. The qualitative mechanism labels are selection strata, not claims about CryptoMiniSat's internal causal owner.

No CNF is duplicated here. `instances.list` and `manifest.json` reference compressed files in `benchmarks/cache/tierD24-26`; generated proofs, models, and expanded CNFs were deleted after checking.

## Family coverage

| Family | Rows |
| --- | ---: |
| `folkman` | 3 |
| `mdp` | 3 |
| `par32` | 3 |
| `lightsout` | 2 |
| `bp4` | 1 |
| `crusti` | 1 |
| `dltm` | 1 |
| `mp1` | 1 |
| `nb13t165` | 1 |
| `p160` | 1 |
| `pcmax` | 1 |
| `ptn` | 1 |
| `st` | 1 |
| `stable` | 1 |
| `vanderwaerden` | 1 |
| `x9` | 1 |

## Representation strata

| ID | Family | Outcome | Stratum | Selection reason |
| ---: | --- | --- | --- | --- |
| 1107 | `dltm` | SAT | social-network/graph SAT search | retained distinct family or capped family representative |
| 550 | `folkman` | SAT | Folkman graph coloring | retained distinct family or capped family representative |
| 103 | `folkman` | SAT | Folkman graph coloring | retained distinct family or capped family representative |
| 974 | `folkman` | SAT | Folkman graph coloring | retained distinct family or capped family representative |
| 83 | `lightsout` | UNSAT | GF(2) linear-system UNSAT | lightsout random instance |
| 609 | `lightsout` | UNSAT | GF(2) linear-system UNSAT | lightsout corner instance; exclusive solve |
| 284 | `mdp` | UNSAT | GF(2) matrix UNSAT | mdp size-28 representative |
| 330 | `mdp` | UNSAT | GF(2) matrix UNSAT | mdp size-32 representative; exclusive solve |
| 123 | `mdp` | UNSAT | GF(2) matrix UNSAT | mdp size-36 representative; exclusive solve |
| 898 | `mp1` | SAT | propagation/watch-pose-sensitive SAT | retained distinct family or capped family representative |
| 649 | `nb13t165` | SAT | family-distinct structured SAT | retained distinct family or capped family representative |
| 478 | `par32` | SAT | parity-heavy SAT | par32 shuffled-c variant |
| 162 | `par32` | SAT | parity-heavy SAT | par32 shuffled variant |
| 62 | `par32` | SAT | parity-heavy SAT | par32 SAT03-shuffled variant |
| 378 | `vanderwaerden` | SAT | Van der Waerden coloring | stronger full-census replacement for the old Van der Waerden row |
| 525 | `x9` | SAT | sub-second structured SAT sentinel | retained distinct family or capped family representative |
| 1122 | `ptn` | SAT | family-distinct structured SAT | retained distinct family or capped family representative |
| 723 | `bp4` | SAT | family-distinct combinatorial SAT | retained distinct family or capped family representative |
| 353 | `crusti` | SAT | family-distinct structured SAT | retained distinct family or capped family representative |
| 234 | `st` | SAT | phase/local-search-sensitive SAT | new family; phase/local-search-sensitive witness |
| 256 | `pcmax` | SAT | scheduling/cardinality SAT | new family; exclusive scheduling instance |
| 482 | `stable` | SAT | stable-set graph SAT | new family discovered only by the completed wall-time continuation |
| 625 | `p160` | SAT | dense p160-family SAT | new family; exclusive dense instance |

## Solve-wall-time comparison

Times are seconds. `S✓` and `U✓` are independently checked SAT and UNSAT. CMS used four cores, 32 GiB, no proof production, and a true 5,000-second wall limit. The seven saved arms have differing placement/proof details, so small differences are not causal comparisons.

| ID | Family | CMS | Tactix | Tactix no-MAB | Kissat 4 | Kissat SC24 | AE-Kissat | Satsuma | Lymphosat |
| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1107 | `dltm` | 84.057 S✓ | 948.622 S✓ | 773.705 S✓ | 263.870 S✓ | 690.997 S✓ | 94.667 S✓ | 239.983 S✓ | 679.091 S✓ |
| 550 | `folkman` | 1.519 S✓ | 50.797 S✓ | 23.497 S✓ | 54.303 S✓ | 38.269 S✓ | 18.495 S✓ | 75.619 S✓ | 41.381 S✓ |
| 103 | `folkman` | 1.619 S✓ | 138.096 S✓ | 47.925 S✓ | 39.075 S✓ | 131.874 S✓ | 128.507 S✓ | 76.267 S✓ | 148.740 S✓ |
| 974 | `folkman` | 1.770 S✓ | 132.992 S✓ | 47.928 S✓ | 123.397 S✓ | 42.191 S✓ | 112.777 S✓ | 62.154 S✓ | 41.827 S✓ |
| 83 | `lightsout` | 6.280 U✓ | 1422.376 U✓ | 509.063 U✓ | 1291.277 U✓ | 1311.214 U✓ | 2276.353 U✓ | 1344.007 U✓ | 1306.645 U✓ |
| 609 | `lightsout` | 5.482 U✓ | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO |
| 284 | `mdp` | 3.775 U✓ | 978.051 U✓ | 984.730 U✓ | 880.360 U✓ | 920.494 U✓ | 1182.049 U✓ | 967.437 U✓ | 928.817 U✓ |
| 330 | `mdp` | 12.290 U✓ | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO |
| 123 | `mdp` | 85.165 U✓ | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO |
| 898 | `mp1` | 1.872 S✓ | 62.705 S✓ | 5000 TO | 5000 TO | 5000 TO | 61.001 S✓ | 93.570 S✓ | 2176.517 S✓ |
| 649 | `nb13t165` | 3.774 S✓ | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO |
| 478 | `par32` | 6.475 S✓ | 1631.867 S✓ | 5000 TO | 5000 TO | 3409.725 S✓ | 5000 TO | 5000 TO | 3831.064 S✓ |
| 162 | `par32` | 7.030 S✓ | 5000 TO | 974.857 S✓ | 5000 TO | 390.120 S✓ | 5000 TO | 5000 TO | 403.233 S✓ |
| 62 | `par32` | 4.577 S✓ | 3215.042 S✓ | 5000 TO | 1267.464 S✓ | 5000 TO | 5000 TO | 5000 TO | 5000 TO |
| 378 | `vanderwaerden` | 28.919 S✓ | 1688.011 S✓ | 1614.275 S✓ | 3525.185 S✓ | 3836.743 S✓ | 5000 TO | 1571.293 S✓ | 3731.679 S✓ |
| 525 | `x9` | 0.018 S✓ | 0.518 S✓ | 0.518 S✓ | 0.416 S✓ | 0.416 S✓ | 0.566 S✓ | 0.467 S✓ | 0.466 S✓ |
| 1122 | `ptn` | 2.772 S✓ | 37.315 S✓ | 374.340 S✓ | 236.415 S✓ | 63.459 S✓ | 23.450 S✓ | 268.911 S✓ | 15.489 S✓ |
| 723 | `bp4` | 87.945 S✓ | 2347.471 S✓ | 3535.483 S✓ | 777.803 S✓ | 1727.963 S✓ | 2643.612 S✓ | 2532.963 S✓ | 1756.985 S✓ |
| 353 | `crusti` | 4.628 S✓ | 202.277 S✓ | 205.063 S✓ | 45.340 S✓ | 327.135 S✓ | 311.079 S✓ | 49.647 S✓ | 294.503 S✓ |
| 234 | `st` | 93.140 S✓ | 5000 TO | 3292.618 S✓ | 2327.932 S✓ | 2875.503 S✓ | 5000 TO | 5000 TO | 3052.159 S✓ |
| 256 | `pcmax` | 132.285 S✓ | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO |
| 482 | `stable` | 62.149 S✓ | 2045.211 S✓ | 1964.937 S✓ | 743.956 S✓ | 1980.499 S✓ | 1845.744 S✓ | 777.476 S✓ | 2101.569 S✓ |
| 625 | `p160` | 100.875 S✓ | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO |

## Targeted development status

This is commit-scoped progress evidence, not a replacement for the frozen
eight-arm table or a coherent release regression. Default affine search has
independently checked closures for the two selected Lights Out rows. The
current direct-implication candidate materially improves the MDP row in
whole-run PBP mode; its paper-comparable 2x-CMS measurement and the Folkman SAT
control remain open.

| ID | Family | Checked Tactix | 2x CMS target | Status |
| ---: | --- | ---: | ---: | --- |
| 83 | `lightsout` | 1.569 U✓ | 12.560 | closed |
| 609 | `lightsout` | 1.720 U✓ | 10.964 | closed |
| 284 | `mdp` | 17.790 U✓ | 7.550 | PBP checkpoint; paper-comparable closure unmeasured |
| 550 | `folkman` | 61.452 S✓ | 3.038 | open SAT control |

For ID 284, candidate production and public emitted byte-identical PBP proofs
and passed VeriPB plus CakePB; public overhead was 0.66%. See the
[pinned screen](../../runs/20261020-cg-mdp-direct-implication-screen/REPORT.md).
The later retained-XOR-buffer checkpoint preserved that exact checked proof and
trajectory in two paired draws, reducing mean wall time from 30.955 to 30.640
seconds (1.02%). This whole-run PBP measurement does not decide the fixed
7.550-second paper-comparable closure target.
Two subsequent exact-proof checkpoints removed repeated premise-set rebuilding
and unused one-shot publication snapshots. Exact-final job `1795235` preserved
the same checked 39,899,363-byte PBP proof and all search/work counters while
reducing paired mean wall time from 28.885 to 22.860 seconds (20.86%) with no
peak-RSS increase. The paper-comparable 7.550-second measurement remains open;
22.860 seconds was the representative PBP result at that checkpoint.
The following bounded-clause identity checkpoint removed per-premise heap
copies and sorts while preserving unordered multiset identity. Job `1795237`
again preserved the exact proof and all counters, reducing paired mean wall
time from 22.845 to 21.570 seconds (5.58%) with unchanged peak RSS. Thus 21.570
seconds is now the representative checked PBP result; the paper-comparable
7.550-second measurement remains open.
The next checkpoint packs source provenance only inside full Gaussian
reduction while retaining the public sparse rows and persistent dirty-update
representation. Job `1795240` preserved the exact proof, trajectory, work
counters and peak-RSS gate while reducing paired mean wall time from 21.440 to
19.035 seconds (11.22%) and affine overhead by 11.77%. Thus 19.035 seconds was
the representative checked PBP result at that checkpoint; it still does not
decide the paper-comparable closure measurement.
The matched public-interface follow-up measured a 20.220-second production
mean and 20.550-second public mean (+1.632%), with exact proof and trajectory
identity and no RSS increase. Its corrected reissue ran only the initially
invalid public rows. A separate `--auto-proof` diagnostic timed out all four
draws at 120.33 seconds with incomplete DSR output, demonstrating that this
automatic route is neither a substitute for the default-PBP baseline nor a
completed paper-comparable measurement. Both campaigns deleted their expanded
CNFs, proofs, raw streams, and checker scratch.
A follow-up bounded dirty-row word scratch preserved exact proof, trajectory,
RSS, and cleanup, but job `1795245` improved paired mean wall time only from
19.015 to 18.460 seconds (2.919%), missing its predeclared 3% threshold. It was
reverted rather than accepted by relaxing the criterion after measurement.
The representative accepted PBP result therefore remained 19.035 seconds.
Job `1795246` then tested allocation-free identity checks for leased premise
clauses. Exact proof, trajectory, counters, and cleanup passed, but the paired
wall gain was only 1.235% and peak RSS increased 2,048 KiB. The candidate was
reverted under its predeclared 3% and no-RSS-increase gates. It does not change
the representative result.
Job `1795248` next removed deep source-identity copies from the immediate
compact-PBP path. Its 5.762% wall gain passed, but its 2,048 KiB peak-RSS
increase did not, so that binary was rejected. The factored reissue in job
`1795250` kept separately callable/two-phase identity snapshots, preserved the
exact checked proof, trajectory, and work counters, reduced paired mean wall
time from 18.985 to 17.790 seconds (6.294%), and lowered peak RSS to 45,836
KiB. Thus 17.790 seconds is the representative checked PBP result. It is judged
against the like-for-like PBP baseline, not against the proof-free 7.550-second
CMS-derived target.

## Pattern

The collection has 6 CMS-exclusive checked solves, 13 at-least-10× wins, and 4 diversity rows. The completed census adds `p160`, `pcmax`, `st`, and `stable`; normalizes `vdw` into `vanderwaerden`; and excludes a second `mp1` row as a near-duplicate.

## Reproduce and verify

```bash
python3 benchmarks/collections/CG/build.py
python3 benchmarks/collections/CG/verify.py --hash-inputs
```

`baseline-runtimes.csv` contains all 184 solver-instance rows and their provenance caveat.
