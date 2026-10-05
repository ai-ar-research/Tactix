# Partial-model guidance ablation

**Date:** 2026-10-05  
**Status:** terminal diagnostic evidence; split-budget replacement remains unaccepted

## Question and fixed evidence

This ablation asks whether a LymphoSAT partial model should directly seed CDCL,
seed only Walk, or be omitted.  It uses the saved `d170b4a5` Tier-D results as
the fixed reference; they were not rerun.  Three immutable Tactix campaigns run
the same 21 `oddball_*` rows at 5,000 solver seconds and 32 GiB:

| Arm | Commit | Job | Semantics |
|---|---:|---:|---|
| preceding | `8b250917` | `1748917` | seeded Walk followed by raw partial-model target reinstall |
| Walk-only repair | `0a35d3f74` | `1748977` | private seeded Walk; only a winning complete Walk assignment reaches CDCL |
| no automatic seed | `a9f1e14d4` | `1749005` | ordinary Walk; explicit controller seeded-Walk and S8 actions remain available |

The old saved `d170b4a5` implementation also copied partial-model phases into
Kissat saved/target phase state before Walk, so its partial model could affect
later CDCL directly.  The Walk-only repair records zero automatic
`LocalSearchGuidance` applications, target installs, or local-phase decisions.
It records 148 private Walk imports and 139 exports.

## Terminal oddball results

| Arm | Valid | Solver seconds | vs `d170` | Validation seconds | Notes |
|---|---:|---:|---:|---:|---|
| saved `d170b4a5` | 21/21 | 3,359.788 | reference | 10,925.978 | saved completed comparison |
| preceding `8b250917` | 20/21 | 4,315.167 | +28.4% | 11,093.205 | `26_5` checker allocation failure |
| Walk-only `0a35d3f74` | 21/21 | 3,761.701 | +12.0% | 13,281.751 | 12.8% faster and 14.9% less proof data than preceding |
| no-seed `a9f1e14d4` | 21/21 | 5,711.837 | +70.0% | 18,773.710 | guidance removal rejected |

Every solver verdict matches the expected result.  Every Walk-only and no-seed
verdict is independently checked.  The Walk-only repair fixes the preceding
checker failure: `26_5` takes 544.356 solver seconds, 1,139.677 checker seconds,
and a 2.287-GB proof, versus 1,302.800 solver seconds and a 5.312-GB proof that
exhausted checker allocation in the preceding arm.

| Instance | `d170` | preceding | Walk-only | no-seed | Interpretation |
|---|---:|---:|---:|---:|---|
| `112_5_ttf` SAT | 181.942 | 195.897 | 230.418 | 201.570 | imported seeds never export; ordinary Walk is better than repaired seeding |
| `19_4_ttf` UNSAT | 64.696 | 82.921 | 8.880 | 34.921 | private seeded Walk is strongly useful |
| `26_5_ttf` UNSAT | 878.771 | 1,302.800 invalid | 544.356 | 659.976 | Walk-only repair improves speed and proof validity |
| `26_4_ttf` UNSAT | 779.165 | 522.645 | 675.901 | 1,075.342 | removing the seed is materially harmful |
| `29_4_ttf` UNSAT | 942.814 | 1,657.987 | 1,630.446 | 3,171.919 | no-seed proof grows to 9.052 GB and checks in 10,808.933 s |

The result rejects both extremes.  Persistent raw partial-model inheritance by
CDCL creates harmful and sometimes proof-expensive trajectories, while removing
the partial model loses large benefits on other rows.  A partial model should
remain private guidance for Walk, and CDCL should inherit only a complete Walk
result through the normal saved/target mechanism.

## Default-policy regression gates

The no-seed default is correctness- and regression-safe while the replacement
is developed.  Existing scorer SHA-256
`47594a4fa567effbe8902f251b48c2b4aef53efa57785a6a451e50102a4fb9f2`
accepts all four terminal campaigns:

| Gate | Job | Result |
|---|---:|---|
| B28+C5 | `1749006` | 33/33; 959.055 s <= 999.87615 s; 32 checked, one bounded proof censor |
| Rank1--50 | `1749007` | 50/50; 7,669.874 s <= 8,521.52175 s; 42 checked, eight bounded proof censors |
| WV100 | `1749008` | 100/100; 98 checked, two proof censors; no scorer failure |
| WV2--89 | `1749009` | 89/89; 77 checked, two proof censors and ten unchanged baseline solver censors |

There is no wrong verdict, checker rejection, or acceptance-contract failure.
The 1,800-second proof cap deliberately censors long checks; each comparable
censor satisfies the baseline-plus-`max(30%,20s)` rule.

## Replacement requirement

The old seeded path charges candidate preparation against the same allowance as
ProbSAT search and always starts from a selected candidate even when its current
CNF score is worse.  The replacement must expose separate preparation and
search allowances, make admission explicit, preserve exact selected-action
semantics without silent fallback, and keep raw candidates out of CDCL phase
state.  Default structural seeding stays disabled until focused cluster
prototypes, full Debug, both-policy fuzz, and all four gates accept that
contract.

Archived full-Debug job `1749241` is not solver evidence: the campaign omitted
checker binaries from its archived source and allowed tests to mutate the source
tree whose digest it later rechecked.  Eleven harness tests consequently failed;
a corrected immutable campaign is required.
