# b26 vs 1e8295106 slowdown: attribution by pass

**Verdict.** Two source changes cause the per-row losses. Graph labeling and
the structured-root/FirstOpen passes do not.

1. **Residual-seed default flip (`3ecfa8dfe`, kept by `0a15ecaf1`).** This
   change sets `structured_root_residual_search_handoff` from true to false. It
   accounts for noL-11-8 (the lost retained solve), noL-11-6, noL-11-2 and the
   oski rows. Restoring the reseed reproduces 1e8295106 **byte for byte** on all
   six probed rows. The rest of the b26 source is inert there.
2. **FastEliminate/BVE port (`efdced45a`, `37226faa7`).** This accounts for
   Break_20_72 (WV100) and Circuit_multiplier24 (WV2-89). Under the triage rule
   these are accepted.

Graph labeling has no effect on any of the eight rows: on/off gives
byte-identical proofs on both pins. The specialized passes cost the same in
every arm, and their output is identical and consumed in every arm.

## Same-window 2x2 (+ seed arm), job 1755562

Campaign: `benchmarks/runs/20261007-wv289-nol118-2x2/`, run on
cpu-dense-hwulab-q. Resources are 12 CPUs / 32768M with the solver pinned to
one CPU. Each row uses its suite's solve timeout and runs with `--auto-proof`.
Pins: b26 `b26efc0f`, 1e8 `27cf0cab`, and 37226seed `4fa9b118`. The 37226seed
pin is source `37226faa7`, which is b26 with the residual reseed default still
on. All 26 SAT results passed checkmodel against the original CNF. Times are
wall seconds, with proof bytes in brackets.

| Row | b26 GL-on | b26 GL-off | 1e8 GL-on | 1e8 GL-off | 37226seed GL-on |
|---|---:|---:|---:|---:|---:|
| noL-11-8 (WV2 222) | UNKNOWN 600 | UNKNOWN 600 | 38.1 [239.5M] | 38.2 [239.5M] | 38.2 [239.5M] |
| noL-11-6 (R1-50) | 304.0 [1506M] | 304.8 [1506M] | 133.1 [734.7M] | 132.7 [734.7M] | 132.6 [734.7M] |
| noL-11-2 (R1-50) | 265.6 [1668M] | 265.8 [1668M] | 114.5 [755.4M] | 115.8 [755.4M] | 115.3 [755.4M] |
| oski b06 | 652.1 [3475M] | 652.4 [3475M] | 382.0 [2100M] | 382.6 [2100M] | 381.9 [2100M] |
| oski b20 | 557.2 [2784M] | 557.8 [2784M] | 403.5 [1977M] | 403.8 [1977M] | 403.1 [1977M] |
| oski b40 | 440.8 [2461M] | 442.6 [2461M] | 335.9 [1700M] | 335.3 [1700M] | 335.9 [1700M] |
| Break_20_72 (WV100) | 661.0 [5849M] | 661.6 [5849M] | 118.3 [897.9M] | 118.2 [897.9M] | 667.9 [5849M] |
| Circuit_multiplier24 | 582.0 [786.4M] | 582.5 [786.4M] | 70.4 [144.4M] | 70.4 [144.4M] | 583.6 [786.4M] |

The full per-task counters are in `ablation-2x2.json`.

## Per-pass time and consumed output (same runs)

| Row | structured root | SR rule | Lucky | GL work (recognized) | SR output, all arms | Seed | Conflicts b26 / 1e8 |
|---|---:|---:|---:|---|---|---|---:|
| noL-11-8 | 0.34 s | 18-22 ms | 1 ms | 15,834 (0) | 1,082 applied, 1,435 witness lits, 1 graph root unit | off / on | >17.2M / 1.48M |
| noL-11-6 | 0.33 s | 18-22 ms | 1 ms | 15,834 (0) | 1,082 applied, 1,443 lits | off / on | 10.13M / 4.83M |
| noL-11-2 | 0.35 s | 18-22 ms | 1 ms | 15,834 (0) | 1,088 applied, 1,459 lits | off / on | 9.91M / 4.59M |
| oski b06/b20/b40 | 4.4-4.5 s | 0.44 s | 0 | 1.30M (0) | 1,517-1,518 applied, ~18,050 lits | off / on | 12.37M/6.31M, 9.31M/6.38M, 7.40M/4.70M |
| Break_20_72 | 0.35-0.46 s | — | 0.12-0.20 s | 29,224 (0) | none | no handoff | 20.0M / 2.75M (BVE 46,289 vs 45,987 eliminated) |
| Circuit_multiplier24 | 0.07 s | — | 6 ms | 44,852 (0) | none | no handoff | 7.29M / 1.46M (BVE 178 vs 169) |

- **(a) Time.** Every loss is in search plus the inprocessing that scales with
  it. Vivify and Sweep grow with the conflict count. No pass reported a
  measurable cost difference between arms.
- **(b) Output.** The SR output is identical across arms, as are the
  graph-root unit and the root assignments, and search consumes it in every
  arm. Graph labeling recognizes nothing on all eight rows. It installs no
  phases, finds no models and does not change the verdict.
- **(c)** See the 2x2 table above.

## Is the residual reseed owner-faithful? No (job 1755797)

Campaign: `benchmarks/runs/20261007-wv289-nol118-satsuma-queue/`. It ran the
actual SC2026 Satsuma with the exact `run_base.sh` flags:
`fix --full-skip-limit 100000000 --add-reduced-as-unit --bsr`. Kissat activates
variables in first-occurrence order of the file it parses. On every measured
row, the owner's residual queue, after the fixed-unit suffix, **equals the
original first-occurrence order minus the fixed variables**:

| Row | Satsuma units | Active queue | Equal to original minus fixed |
|---|---:|---:|---|
| noL-11-8 | 27 | 1,062 | yes |
| noL-11-6 | 22 | 1,067 | yes |
| noL-11-2 | 9 | 1,080 | yes |
| noL-11-10 (Rank401) | 38 | 1,051 | yes |
| oski15a01b06s_opt | 3,408 | 485,182 | yes |

b26's original seed therefore reproduces the owner's activation pose on these
rows. Fixed variables are deactivated, so the relative order of active
variables is the same. The live-arena reseed in `heuristics.cpp`
(`heuristics_prepare_residual_activation_order`) does not reproduce it. On
Rank401 its queue first differs at index 2: it gives 4,5,7,8,10,11,… where
Satsuma gives 3,4,5,6,…. It defers the third literal of each leading ternary
(`benchmarks/reports/2026-10-07-rank401-reference-policy-causal/`). The 1e8
speed on the six rows comes from that **non-faithful** pose.

## Fix proposal and trade-off

No specialized pass burns time without output on these rows, so the triage rule
assigns no pass-level fix. The seed pose that b26 uses is the owner's pose.

- **Recommended: keep b26's default** (original seed). It matches
  Satsuma→Kissat on all five measured rows and keeps Rank401: SAT 52.088 s
  (factorial task 007), and SAT 42.273 s on b26 full44.
  - A faithful generalization of the handoff needs no default change on these
    rows. It would compute first occurrence over the original clause order,
    skipping satisfied clauses and false literals, then the AMO/SBP clauses,
    then the fixed units.
  - On every measured row that ordering equals the original seed.
- **Option A (`option-a-restore-residual-reseed.patch`, one line).** This
  restores the 1e8 default.
  - Measured gain on the probed rows: −1,307 s.
    - noL-11-8: 600 → 38 s, recovering the retained solve.
    - noL-11-6: −171 s. noL-11-2: −151 s.
    - oski b06: −270 s. b20: −154 s. b40: −105 s.
  - Suite estimate from the 1e8-vs-b26 seed rows:
    - Rank1-50 about −892 s, B28 about −28 s, WV100 about −8 s.
    - WV2-89 about +14 s overall. It regains noL-11-8 but would likely lose
      b26's new solves on lockchart `_4` (211 s) and hcp_CP18_18 (468 s); both
      were seed rows that 1e8 timed out on.
  - **Cost: Rank401.** FirstOpen plus the reseed gives UNKNOWN at 300 s with
    6,964,693 conflicts (`20261007-mp1-cut-seed-factorial` task 005, and causal
    task 000). The default would also stop matching the owner pose.
- **Break_20_72 / Circuit_multiplier24** (+529 s / +512 s): this is the
  FastEliminate/BVE port, which is accepted under the triage rule. They are
  independent of graph labeling and the seed: 37226seed equals b26 byte for byte.

## Secondary: graph-labeling authentication with no output

The suite runs show graph-labeling authentication that recognizes nothing yet
uses large budgets:
- 17.normalised (WV2-89 046): the full 100M Recognition work, exhausted, with 0
  recognized. Solve time is +31 s internal and +57 s wall.
- Rank1-50 rows 004, 007, 009, 012 and 013: 45-93M work each, 0 recognized.

The 100M Recognition cap in `default_strategy_config.hpp` is a fixed constant,
and `GraphLabelingModel.enabled` is false by default (WIP). The suite-level
graph-labeling on/off cost will come from the d576 `-gl-`/`-nogl-` regressions
(jobs 1755593-1755600), which are not run here.

## Pending

The shuffle probe (job 1755806, `benchmarks/runs/20261007-wv289-nol118-shuffle/`)
runs five renamed/flipped/shuffled copies each of noL-11-8 and noL-11-10, under
b26 and 37226seed. It runs on cpu-q at 12/32G because the dense partition is
saturated. It is queued and its results are not in this report.
