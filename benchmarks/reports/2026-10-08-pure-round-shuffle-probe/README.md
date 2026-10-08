# One-round pure literals (fa6859f1d): original-file losses and shuffle probe

`pure1` = Release pin of `fa6859f1d` (Satsuma A–D plus one pure-literal round per
structured-root pass; `build/pin-pure1-rel/tactix` `430190c6…`, build job 1757063;
focused Debug tests 6/6, job 1757065). All runs use `runtime_task.py` via
`make_pair.py` (hwu partition, 8 CPUs, 32 GiB, `--auto-proof
--default-graph-labeling=on`); every SAT model was checked.

## Original gate files at 5,000 s

| Row | c2 | bcd3 (A–D) | pure1 | Runs |
|---|---|---|---|---|
| Break_20_72 (WV2–89) | – | UNKNOWN | UNKNOWN | `20261008-r5000-{bcd,pure1}-break2072` |
| ntil-90d-40 (WV2–89) | – | UNKNOWN | UNKNOWN | `20261008-r5000-{bcd,pure1}-ntil40` |
| Rank754 (fixed44) | UNKNOWN | SAT 3,915.6 | UNKNOWN | `20261007-satsuma-bcd3-pair-r754`, `20261008-r5000-pure1-r754` |

pure1's startup differs from bcd3 (579 vs 728 SR applications on Rank754; different
FastEliminate candidate counts), so the change was exercised.

## Shuffle probe (five renamed / sign-flipped / clause-shuffled copies)

Copies are produced by the deterministic `20261007-satsuma-bcd3-shuffle/pins/shuffle.py`
with seeds 1–5 and staged in `benchmarks/cache/shuffle-probe/`. The c2 and bcd3 columns for
Break_20_72 and ntil-90d-40 are the earlier probe on the same seeds
(`20261007-satsuma-bcd3-shuffle`; its `mc-FAIL` marks are the harness gap documented
in `../2026-10-07-satsuma-bcd-fidelity/README.md`).

| Row (limit) | c2 | bcd3 | pure1 | pure1 times (s) |
|---|---|---|---|---|
| Break_20_72 (1,200 s) | 1/5 | 4/5 | 2/5 | 862.8, 1,114.1 |
| ntil-90d-40 (1,200 s) | 0/5 | 3/5 | 3/5 | 135.4, 1,058.5, 1,134.4 |
| Rank754 (5,000 s) | 1/5 | – | **4/5** | 254.1, 606.1, 975.3, 2,139.2 |

Runs: `20261008-shuf-{c2,pure1}-r754-s{1..5}`, `20261008-shuf-pure1-{break2072,ntil40}-s{1..5}`
(jobs 1757080–1757099).

## Reading

On every row pure1 solves at least as many shuffled copies as c2, and on Rank754 it
solves four of five where c2 solves one. The original-file losses are draw luck
under the faithful port, not a port defect (plan §9.5 noise criterion); profiles for
these rows already matched (`../2026-10-07-satsuma-bcd-fidelity/README.md`,
`results/perf2`). Single draws per copy; Break_20_72 remains within luck range of
both c2 and bcd3.
