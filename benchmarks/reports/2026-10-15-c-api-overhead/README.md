# DefaultPolicy over `tactix.h`: overhead against the C++ build

**Question.** What does it cost to run the shipped default strategy with every action issued
through the public C interface, compared with the production C++ build?

**Answer.** Under 3% overall on every regression set, with identical behaviour. Same-window pairs
on the same machines:

| Set | Instances | Solved (C++ / C interface) | Identical answers | Median slowdown | Total time | Total instructions |
|---|---|---|---|---|---|---|
| Tier A | 200 | 200 / 200 | 200 | +2.4% | +2.0% | not recorded |
| B28+C5 | 33 | 33 / 33 | 33 | +0.9% | +0.9% | +0.9% |
| Rank 1–50 | 50 | 50 / 50 | 50 | +0.8% | +1.5% | +1.4% |
| WV100 | 100 | 97 / 97 | 100 | 0.0% | −0.1% | +0.5% |
| WV2–89 (87 of 89 rows) | 87 | 80 / 80 | 87 | 0.0% | +1.4% | +0.9% |

Every UNSAT certificate in the gate sets has the same SHA-256 in both arms; Tier A matches answers
and certificate sizes. Tier A instances mostly take under 0.1 s, so its ratios are timing noise.
WV2–89 was cancelled at the user's request after 88 of 89 tasks per arm; one row is missing from
each arm (bphp_p23_h22 from the C interface, linked_list_swap_contents from C++), so 87 rows are
paired. The paper reports only Tier A, B28+C5, Rank 1–50 and WV100.

## Arms

| Arm | Binary | Source |
|---|---|---|
| C++ (production) | `a7c66babcd56…` | `0f7c10174`, `build/pin-prod-a7c66bab-gates-rel` |
| C interface (`tactix-public`) | `ef6c2c4011e3…` | `984a72441`, `build/pin-public-port-984a72441-gates-rel` (byte-identical to the working-tree build) |

`tactix-public` (`tests/tactix_public_main.cpp`) reads input with `tactix_read_dimacs` and runs
DefaultPolicy over PublicPort; its census reported 0 fallbacks and 0 declined requests on every row.

## Runs

| Set | C interface | C++ |
|---|---|---|
| Tier A (`run.py --tier A`, both arms in one campaign) | `benchmarks/runs/20261015-120822-A-tactix-public` (array 1779897) | same campaign |
| B28+C5 | `20261015-public-port-v3-regression-b28c5` (1779728) | `20261015-prod-samewindow-regression-b28c5` (1779729) |
| Rank 1–50 | `…-v3-regression-rank1-50` (1779730) | `…-samewindow-regression-rank1-50` (1779731) |
| WV100 | `…-v3-regression-wv100` (1779732) | `…-samewindow-regression-wv100` (1779733) |
| WV2–89 (cancelled) | `…-v3-regression-wv2-89` (1779734) | `…-samewindow-regression-wv2-89` (1779735) |

Gates: `controller_regression.py prepare`, `cpu-dense-hwulab-q`, 12 CPUs, 32 GiB, 1,200 s, full
width. Tier A: 8 CPUs, 32 GiB, 30 s, same partition.

## How the overhead was reduced

Earlier builds of the same strategy (`5fc2ec62c`) cost +2.3% (B28+C5) to +4.9% (Rank 1–50) in
time, up to +13% instructions on call-dense rows. Profiles of ncc_none_21015, knight_18,
mp1-Nb7T45 and oski15a01b01s_opt (`benchmarks/probes/scratch/public-port-overhead`) attributed it
to three per-call costs, removed in `984a72441`: two vDSO clock reads per micro-rule dispatch,
PublicPort's string-keyed census map, and its vector-and-strlen record stack. Before that,
`5fc2ec62c` removed a quadratic arena walk in the public fastel instance validation (up to 199×
on single rows).

## Files

- `extract.py` — pairs the runs; writes `interface-overhead.csv` (instances both arms solved) and
  `summary.json`. Run from the repository root.
- The paper's `data/interface-overhead.csv` is this CSV restricted to the four reported sets.
