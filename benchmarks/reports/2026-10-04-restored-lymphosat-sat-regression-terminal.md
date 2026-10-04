# Restored LymphoSAT SAT constructors: terminal regression result

Frozen checkpoint `156afef6b08469bd489d380c511b300b9645dfc2` is **not
accepted**.  Its B28+C5 campaign passes, but Rank1--50 exceeds the aggregate
solver-time ceiling and WV100/WV2 lose seven retained SAT solves.  All four
arrays are terminal and have no missing rows, wrong verdict, rejected model, or
rejected proof.  The candidate binary SHA-256 is
`3582d04bca4537865102ab7175d06b29174ed796bc5d13dc896c9dd1b70da8f4`.

| Set / job | Gate | Solver seconds | Independent checks | Checker censor | Solver timeout | Lost retained solve |
|---|---|---:|---:|---:|---:|---:|
| B28+C5 / `1748123` | **pass** | 998.177 / 999.876 ceiling | 32 | 1 | 0 | 0 |
| Rank1--50 / `1748124` | **fail** | 9,530.106 / 8,521.522 ceiling | 42 | 8 | 0 | 0 |
| WV100 / `1748125` | **fail** | 2,646.841 | 93 | 2 | 5 | 5 |
| WV2--89 / `1748126` | **fail** | 3,781.591 | 75 | 2 | 12 | 2 |
| Total | **fail** | -- | 242 | 13 | 17 | 7 |

The 13 proof checks stopped at the agreed 1,800-second bound.  Twelve are
`censored-within-bound`; WV100 `ncc_none_21015_5_3_3_0_0_11` is inconclusive
because its baseline check was already censored.  Censoring is not independent
verification, but none of these rows is a checker rejection.  Solver caps were
1,200 seconds for B28+C5, Rank and WV100, and 600 seconds for WV2.  The scorer
uses the agreed completed-proof allowance
`baseline + max(30% of baseline, 20 seconds)`.

## Per-instance diagnosis

The frozen policy accidentally omitted historical affine constructor instance
8.  Every large new loss below has one affine call in the accepted baseline and
zero in the candidate:

| Set / task | Instance | Baseline | Candidate | Baseline affine outcome |
|---|---|---:|---:|---|
| Rank / 14 | `mdp-32-10-sat` | 0.116 | 975.244 | SAT model |
| Rank / 27 | `cabp-V-nos6` | 222.228 | 1,034.627 | budget stop |
| Rank / 49 | `mdp-28-14-sat` | 0.066 | 107.420 | SAT model |
| WV100 / 50 | `mdp-32-11-sat` | 0.116 | 1,200.004 timeout | SAT model |
| WV100 / 62 | `mdp-32-14-sat` | 0.317 | 1,200.005 timeout | SAT model |
| WV100 / 63 | `mdp-36-10-sat` | 0.467 | 1,200.005 timeout | SAT model |
| WV100 / 64 | `mdp-36-14-sat` | 0.467 | 1,200.004 timeout | SAT model |
| WV100 / 96 | `Break_20_72` | 134.586 | 1,200.005 timeout | one equation, no model |
| WV2 / 37 | `SC25_Timetable_C_496...` | 482.234 | 600.005 timeout | budget stop |
| WV2 / 41 | `mdp-32-10-sat` | 0.116 | 600.005 timeout | SAT model |

This is direct causal evidence for the six MDP losses: the accepted path solved
them inside affine construction.  The `cabp`, Break and timetable rows establish
trajectory correlation rather than a direct affine solution, because their old
affine attempts produced no candidate.  They require focused repaired-source
checks before recovery is claimed.

Mutable source commit `60446aa0d` restores affine instance 8 after exact-one
admission and before structure-changing preprocessing.  A focused policy-path
fixture already requires the productive affine case to return a validated SAT
model with zero decisions and conflicts.  This repair was made after the frozen
binary and is not relabeled by these results.

## Disposition and next check

Preserve these campaigns as negative evidence.  Do not rerun the historical
comparison and do not repeat all four gates immediately.  First use the repaired
source on the affected affine subset: the six directly solved MDP rows plus
`cabp-V-nos6`, Break and timetable.  Recovery of the direct rows and stable
behavior on the three trajectory-sensitive rows is the prerequisite for a
later consolidated gate.  The separate partial-guidance increment must retain
exact candidate identity, independently selectable Walk/CDCL consumers and raw
use telemetry; it makes no performance claim from this campaign.

Raw immutable evidence remains in:

- `benchmarks/runs/20261004-restored-lymphosat-sat-b28c5/`
- `benchmarks/runs/20261004-restored-lymphosat-sat-rank1-50/`
- `benchmarks/runs/20261004-restored-lymphosat-sat-wv100/`
- `benchmarks/runs/20261004-restored-lymphosat-sat-wv2-89/`
