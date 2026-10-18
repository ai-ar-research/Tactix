# CG: CryptoMiniSat-guided benchmark collection

CG contains 25 independently checked Tier D instances selected from the pinned CryptoMiniSat 5.14.7 rerun. It is a development target for bringing one instance-independent Tactix DefaultPolicy within 2× the checked CMS solve time.

The rerun was stopped after 601 of 1,166 rows because the user accepted a representative collection. The set contains every checked exclusive or at-least-10× win below 600 seconds in that snapshot (21 rows), then adds the best CMS-over-baseline row from four previously unrepresented families. This is a frozen representative sample, not a complete Tier D census. N1–N4 remain required for solver promotion.

No CNF is duplicated here. `instances.list` and `manifest.json` reference the compressed files in `benchmarks/cache/tierD24-26`; all generated proofs and expanded CNFs were deleted after checking.

## Family coverage

| Family | Rows |
| --- | ---: |
| `mdp` | 7 |
| `folkman` | 3 |
| `par32` | 3 |
| `lightsout` | 2 |
| `1x1x13` | 1 |
| `bp4` | 1 |
| `case11` | 1 |
| `crusti` | 1 |
| `dltm` | 1 |
| `mp1` | 1 |
| `nb13t165` | 1 |
| `ptn` | 1 |
| `vanderwaerden` | 1 |
| `x9` | 1 |

## Solve-wall-time comparison

Times are seconds. `S✓` and `U✓` are independently verified SAT and UNSAT; `S?`/`U?` are unverified answers; `TO` is a timeout. CMS used the pinned `cpu-q` campaign with 32 GiB and a 5,000-second solve cap. The other seven columns are saved historical arms and are included for the requested general pattern; their placement, proof configuration, and some resource details differ, so small timing differences are not causal comparisons.

| ID | Family | CMS | Tactix | Tactix no-MAB | Kissat 4 | Kissat SC24 | AE-Kissat | Satsuma | Lymphosat |
| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 240 | `case11` | 23.866 S✓ | 362.453 S✓ | 1188.123 S✓ | 714.204 S✓ | 799.941 S✓ | 266.033 S✓ | 767.721 S✓ | 1014.830 S✓ |
| 1107 | `dltm` | 5.422 S✓ | 948.622 S✓ | 773.705 S✓ | 263.870 S✓ | 690.997 S✓ | 94.667 S✓ | 239.983 S✓ | 679.091 S✓ |
| 550 | `folkman` | 1.722 S✓ | 50.797 S✓ | 23.497 S✓ | 54.303 S✓ | 38.269 S✓ | 18.495 S✓ | 75.619 S✓ | 41.381 S✓ |
| 103 | `folkman` | 1.886 S✓ | 138.096 S✓ | 47.925 S✓ | 39.075 S✓ | 131.874 S✓ | 128.507 S✓ | 76.267 S✓ | 148.740 S✓ |
| 974 | `folkman` | 2.291 S✓ | 132.992 S✓ | 47.928 S✓ | 123.397 S✓ | 42.191 S✓ | 112.777 S✓ | 62.154 S✓ | 41.827 S✓ |
| 83 | `lightsout` | 7.425 U✓ | 1422.376 U✓ | 509.063 U✓ | 1291.277 U✓ | 1311.214 U✓ | 2276.353 U✓ | 1344.007 U✓ | 1306.645 U✓ |
| 609 | `lightsout` | 7.626 U✓ | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO |
| 284 | `mdp` | 7.225 U✓ | 978.051 U✓ | 984.730 U✓ | 880.360 U✓ | 920.494 U✓ | 1182.049 U✓ | 967.437 U✓ | 928.817 U✓ |
| 114 | `mdp` | 8.476 U✓ | 1100.658 U✓ | 966.690 U✓ | 760.623 U✓ | 1172.141 U✓ | 1565.204 U✓ | 826.660 U✓ | 1182.978 U✓ |
| 253 | `mdp` | 12.581 U✓ | 562.368 U✓ | 500.004 U✓ | 499.090 U✓ | 515.246 U✓ | 1249.052 U✓ | 569.592 U✓ | 543.698 U✓ |
| 330 | `mdp` | 44.827 U✓ | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO |
| 183 | `mdp` | 74.464 U✓ | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO |
| 159 | `mdp` | 86.443 U✓ | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO |
| 388 | `mdp` | 102.699 U✓ | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO |
| 898 | `mp1` | 1.668 S✓ | 62.705 S✓ | 5000 TO | 5000 TO | 5000 TO | 61.001 S✓ | 93.570 S✓ | 2176.517 S✓ |
| 649 | `nb13t165` | 5.575 S✓ | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO | 5000 TO |
| 478 | `par32` | 7.626 S✓ | 1631.867 S✓ | 5000 TO | 5000 TO | 3409.725 S✓ | 5000 TO | 5000 TO | 3831.064 S✓ |
| 162 | `par32` | 22.650 S✓ | 5000 TO | 974.857 S✓ | 5000 TO | 390.120 S✓ | 5000 TO | 5000 TO | 403.233 S✓ |
| 62 | `par32` | 38.412 S✓ | 3215.042 S✓ | 5000 TO | 1267.464 S✓ | 5000 TO | 5000 TO | 5000 TO | 5000 TO |
| 352 | `vanderwaerden` | 273.532 S✓ | 4342.405 S✓ | 4099.237 S✓ | 4944.290 S✓ | 4852.435 S✓ | 5000 TO | 4990.221 S✓ | 74.924 S? |
| 525 | `x9` | 0.034 S✓ | 0.518 S✓ | 0.518 S✓ | 0.416 S✓ | 0.416 S✓ | 0.566 S✓ | 0.467 S✓ | 0.466 S✓ |
| 799 | `1x1x13` | 69.214 S✓ | 587.097 S✓ | 1771.917 S✓ | 787.873 S✓ | 1370.378 S✓ | 1350.125 S✓ | 2099.380 S✓ | 785.633 S✓ |
| 1122 | `ptn` | 2.872 S✓ | 37.315 S✓ | 374.340 S✓ | 236.415 S✓ | 63.459 S✓ | 23.450 S✓ | 268.911 S✓ | 15.489 S✓ |
| 723 | `bp4` | 160.915 S✓ | 2347.471 S✓ | 3535.483 S✓ | 777.803 S✓ | 1727.963 S✓ | 2643.612 S✓ | 2532.963 S✓ | 1756.985 S✓ |
| 353 | `crusti` | 10.632 S✓ | 202.277 S✓ | 205.063 S✓ | 45.340 S✓ | 327.135 S✓ | 311.079 S✓ | 49.647 S✓ | 294.503 S✓ |

## Pattern

The collection has 6 CMS-exclusive checked solves, 15 rows where CMS is at least 10× faster than the fastest verified saved arm, and 4 diversity rows where CMS is 4.26×–8.48× faster. The strongest clusters are `mdp`, `par32`, `lightsout`, and `folkman`; the remaining families test whether the policy generalizes beyond those clusters.

## Reproduce and verify

```bash
python3 benchmarks/collections/CG/build.py
python3 benchmarks/collections/CG/verify.py --hash-inputs
```

`baseline-runtimes.csv` contains the exact long-form values, statuses, provenance campaigns, and comparison caveat for all 200 solver-instance rows.
