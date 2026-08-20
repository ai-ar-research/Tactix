# LymphoSAT Tier-D module attribution (2026-08-20)

**Question.** LymphoSAT (SC2026 winner) bundles 126 specialized solvers and a
kissat-derived fallback behind a profile-and-dispatch core. Which module is
responsible for each of its Tier-D answers?

**Method.** The stock dispatcher never names the winning module and launches
its fallback silently, and the frozen baseline campaign deleted solver stderr,
so attribution required a rerun. `tools/solvers/variants/lymphosat-modlog/`
rebuilds the frozen pin's dispatcher with two added stderr lines (`solved by
<module>`, `dispatching fallback`); all 127 nested solver binaries are
byte-identical to `benchmarks/baselines/sc24-26-32g-5000s/pins/lymphosat/`
(attested; composite sha256 `e56744…`, frozen `977b41…`).
`tools/bench/lymphosat_attribution.py` reran all 1,166 Tier-D instances under
the frozen contract (5,000 s wall, 32 GiB RLIMIT_AS, one solver per task,
node017/node018) as array 1635270 (%40 user-directed throttle recorded in the
manifest) and parsed the dispatcher trace per instance. Attribution-only: no
proof/model re-checking; verdict validity remains owned by the frozen
campaign, against which every rerun verdict is cross-checked.

**Data.** Campaign `benchmarks/runs/20260819-tierd-lymphosat-modlog/`:
`results.json` (aggregate + per-instance records) and `results-table.tsv`
(per instance: verdict, wall seconds, peak RSS, responsible module, ordered
attempt sequence). A first launch was cancelled and archived in the
campaign's `incident-20260820-node-tmp-aborts/`: proofs on node-local /tmp
exhausted the dense nodes' disks under concurrency and aborted sub-solvers;
no result of that launch is used here.

## Results

1,166 instances: **948 solved** (vs 947 frozen), 218 UNKNOWN (all timeouts
with the fallback still running at the limit). Every solved instance is
attributed; solved-wall median 62.8 s, max 4,957.8 s.

| module | SAT | UNSAT | total | median wall (s) | max wall (s) |
|---|---:|---:|---:|---:|---:|
| fallback | 292 | 360 | 652 | 183.3 | 4957.8 |
| random-circuits | 17 | 0 | 17 | 3.7 | 137.7 |
| cryptography-simon | 15 | 0 | 15 | 132.2 | 141.0 |
| p-center | 0 | 14 | 14 | 0.0 | 0.1 |
| software-verification | 0 | 14 | 14 | 19.8 | 27.3 |
| tseitin-formulas | 2 | 12 | 14 | 0.0 | 21.2 |
| md5-equivalence-checking | 13 | 0 | 13 | 5.5 | 26.1 |
| gf3-sat | 12 | 0 | 12 | 0.1 | 0.1 |
| heule-nol | 11 | 0 | 11 | 0.0 | 0.4 |
| oddball-weighing | 11 | 0 | 11 | 1.5 | 3.1 |
| minimum-disagreement-parity | 8 | 0 | 8 | 0.3 | 0.8 |
| clique-coloring | 0 | 8 | 8 | 0.2 | 0.4 |
| cryptography-ascon | 2 | 6 | 8 | 367.2 | 486.5 |
| graceful-production | 7 | 0 | 7 | 3.0 | 5.8 |
| *(61 further modules)* | | | 144 | | |

- **Targeted modules win 296 of 948 solves (31%)** across 75 distinct
  modules; the kissat-derived fallback wins 652 (69%): 588 where no matcher
  matched and 64 rescues where a matched specialist declined first.
- **Dispatch is nearly always one-shot:** 1,049 instances used a single
  attempt; 117 used two (specialist then fallback); none used more.
- Specialist wins are typically fast (most medians under 10 s): the matchers
  select instances their solver dispatches quickly, and the 5,000 s budget is
  effectively the fallback's.
- **8 abnormal attempts** (specialists `independent-set-reconfiguration`,
  `influence-maximization` exiting 1); the cascade absorbed them — 7 of the 8
  instances were still solved by the fallback. This is LymphoSAT-native
  behavior, not a harness artifact.

## Verdict cross-check vs frozen LymphoSAT arm

5 flips in 1,166, all explained, none systematic:

| instance | frozen | rerun | explanation |
|---|---|---|---|
| as-p153-l57-o0-rot-rfl-II-68-i | UNKNOWN @5000 | SAT @4169.8 | boundary |
| VanDerWaerden_2-3-14_186 | UNSAT @4983.6 | UNKNOWN @5000 | boundary |
| BubbleVsPancakeSort_9_4 | UNKNOWN @5000 | UNSAT @4852.8 | boundary |
| 1x1x13_1x… | UNKNOWN @5000 | SAT @4957.8 | boundary |
| lockchart-group1-L220-K317 | SAT @306.6 | UNKNOWN @5000 | randomized specialist |

The lockchart case is `mechanical-master-key` variance: across the 12-instance
lockchart family it sometimes solves directly (33–146 s) and sometimes returns
UNKNOWN after consuming budget, after which the fallback may (3 instances,
1907–2772 s) or may not (4 instances) finish in the remainder. Per-instance
flips at this scale carry no verdict weight.

## Caveats

- UNKNOWN rows have `peak_rss_kb = None` (the kill races the RSS metric).
- `random-circuits` runs the retained Ice Lake binary
  (`benchmarks/baselines/sc24-26-32g-5000s-v1/lymphosat-build.json`); its 17
  wins confirm it executes on these nodes.
- Wall times here are a fresh measurement window; the frozen campaign's times
  remain the canonical baseline numbers.
