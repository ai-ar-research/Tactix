# Progress

[DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) owns stable scope and acceptance
criteria. This file is the compact resume anchor; Git, immutable run manifests,
and frozen reports own history and evidence.

## Current objective

The online-configurable strategy/N1 refactor is accepted at `a08f9f6a`, and
native DSR is landed at `20223134` as Tactix's canonical whole-solver
certificate. Direct-SR R16 is accepted and landed from immutable source
`298c3467` after complete tests, proof preflight, performance gates, and the
passing replacement Claude fuzz checkpoint `1689057` with the forward-repaired
`dsr-trim` pin.

Complete the general Satsuma symmetry producer, starting with its 57-instance
WV100 group. Preserve N1--N4 and the accepted default throughout. Do not submit
a full Tier-D run at this boundary. Kissat-only tuning, benchmark dispatch, and
sequential portfolios remain out of scope; MAB scheduling remains open.

## Authorities

| Item | Authority |
|---|---|
| Accepted refactor | source `a08f9f6a`; [N1/refactor closure](benchmarks/reports/2026-09-13-online-refactor-n1-final.md) |
| Strategy design | [online proposal](docs/design/2026-09-11-online-strategy-proposal.md) and [preservation addendum](docs/design/2026-09-11-strategy-behavior-preservation-addendum.md) |
| Native DSR | source `20223134`; [checker repair](benchmarks/reports/2026-09-13-dsr-trim-tautology-hash-fix.md); [passing Claude checkpoint](benchmarks/reports/2026-09-13-native-dsr-claude-fuzz-r2/terminal-report.md) |
| DSR baselines | [B27+C5](benchmarks/baselines/b27c5-tactix-a08f9f6a-v1/manifest.json) and [Rank1--40](benchmarks/baselines/rank1-40-tactix-a08f9f6a-v1/manifest.json) |
| Direct-SR evidence | source `298c3467`; [performance gate](benchmarks/reports/2026-09-13-direct-sr-performance-gate.md), [failed replacement checkpoint](benchmarks/reports/2026-09-13-direct-sr-r16-claude-fuzz/terminal-report.md), [local elaborator repair](benchmarks/reports/2026-09-13-dsr-trim-forward-elaboration-fix.md), and [passing r2 checkpoint](benchmarks/reports/2026-09-13-direct-sr-r16-claude-fuzz-r2/terminal-report.md) |
| Winner target | [fixed WV100 set](benchmarks/tiers/WV100.tsv), 33 SAT / 67 UNSAT; [experiment protocol](docs/design/2026-09-13-wv100-mechanism-integration.md) |
| Satsuma evidence | [transition screen](benchmarks/reports/2026-09-13-wv100-satsuma-transition.md), [orbitopal G0](benchmarks/reports/2026-09-13-wv100-orbitopal-sr-g0.md), and [symmetry G2](benchmarks/reports/2026-09-13-wv100-structured-symmetry-g2.md) |
| Current producer gates | [support-local performance gate](benchmarks/reports/2026-09-14-support-local-sr-performance-gate.md) and [clique diagnosis](benchmarks/reports/2026-09-14-clique-symmetry-producer-diagnosis.md) |

The root checkout is authoritative. `.claude` contains workflow instructions,
not implementation or project-document changes.

## Accepted base

- The refactor exposes typed configurable rules, all legal explicit instances,
  caller-selected batches/rounds/closures, free composition and repetition,
  scoped fixpoints, budgets/stops, and online reconfiguration. DefaultPolicy
  retains static hot dispatch; `--random-policy` uses the same typed executor
  and seeded replay. Its accepted gates were B28+C5 **1.002481x** and
  Rank1--50 **1.010277x** versus their retained references, with Claude job
  `1687182` passing.
- Native DSR emits one streaming whole-solver certificate and checks it through
  `dsr-trim` to LSR to pinned Trestle. Anonymous scratch cleanup covers exit,
  timeout, crash, cancellation, and signals. Its accepted paired gates were
  B27+C5 **0.995207x** versus reference and Rank1--40 **0.990299x** on the
  complete solver-time vector. Remaining Rank proof checks stay nonblocking.

## Accepted direct-SR increment

- R16 exposes complete validated direct-SR instances plus typed single, batch,
  pass, fixpoint, budget, stop, replay, interruption, and reconfiguration paths.
  It also repairs DSR root publication, signed substitutions, checker/kernel
  population agreement, C proof attachment, and transactional proof-only
  lemmas identified by the failed first Claude checkpoint.
- The landed solver, API, rule, proof, and semantic-document blobs match
  immutable R16 source `298c3467`. The later support-local producer and its
  diagnostic counters remain only in the working tree for a separate increment.
- Immutable source `298c3467` passes the **15/15** proof preflight, **148/148**
  Release tests, and **147/147** applicable ASan/UBSan tests. B28+C5 passes all
  **33/33** checks at 952.263 versus 950.837 solver seconds (**1.001500x**).
  Rank1--50 solves all **50/50** with no timeout at 8,115.735 versus 8,123.812
  seconds (**0.999006x**); slow proof checks remain nonblocking.
- Claude job `1689026` failed: 64 solver-emitted SR certificates were
  elaborated into Trestle-rejected LSR. The cause was the `dsr-trim`
  elaborator, repaired at `0b8cc0ef` plus patch `a5653c3a`.
- Claude job `1689057` reran the unchanged source and legs with that checker
  pin. Its terminal audit reports **PASS**. All 20 legs exit 0 under the repaired
  status aggregator. There are 0 failing cases in 15,108 audit cases and 3,350
  stock cases. No required key is missing, forward DSR-to-LSR-to-Trestle had 0
  rejections, and identity and cleanup checks are exact. This closes the R16
  Claude fuzz gate. The passing and superseded failed fuzz pins were removed
  after audit; their bounded manifests, terminal reports, and reproducers remain.

## Active Satsuma producer work

- R16's exact row producer solves `php_sudoku_p15_h14` in 0.166 seconds with 91
  checked SR additions. A typed unbounded diagnostic shows the same producer
  solves channel rank 17 in 3.156 seconds with 190 checked additions, locating
  that miss in the default detector budget.
- The support-local exact-automorphism candidate removes full-formula rescans
  per adjacent-row generator. On the normal focused protocol it solves channel
  rank 17 in 2.921 seconds with 190 additions; `dsr-trim` and Trestle accept the
  combined proof. Immutable candidate source `992e0414` and its independently
  reproduced Release binary
  `96cdeac8c35a53981feaaf506ccebe8d413d1b860ce49a61c83c147246b8e0b6`
  pass all **148/148** Release tests and all **147/147** sanitizer-applicable
  tests. The candidate remains uncommitted and separate from the accepted R16
  increment.
- Candidate-only B28+C5 job `1689071` solves all 33 expected outcomes at
  958.310 versus 950.837 seconds (**1.007859x**) and passes the 1.01 timing
  limit. Rank1--50 job `1689096` solves all 50 expected outcomes at 8,303.463
  versus 8,123.812 seconds (**1.022114x**) and **fails** that limit. The
  [bounded gate report](benchmarks/reports/2026-09-14-support-local-sr-performance-gate.md)
  therefore rejects the candidate pending a causal overhead repair. Slow proof
  checks remain background work.
- Clique rank 4 still times out with zero SR transitions from the exact-row
  producer. A formula-derived colored-CNF graph finds 11 general generators;
  a direct-SR prototype applies 22 checked units from them but still times out.
  Pinned Satsuma instead emits 91 units and closes UNSAT in about 0.013 seconds.
  The [diagnosis](benchmarks/reports/2026-09-14-clique-symmetry-producer-diagnosis.md)
  identifies stabilizer-chain clause fixing over completed automorphisms as the
  missing general mechanism.

## Operations

- Existing proof-checking jobs continue in the background and do not block
  independent work or trigger reference reruns.
- Completed Tier-D array `1687184` is frozen at
  [`20260913-0905-tierd-refactor-final-a08f9f6a`](benchmarks/runs/20260913-0905-tierd-refactor-final-a08f9f6a/).
  It used `cpu-q`, c4, 32 GiB, a 1,200-second solver timeout, and anonymous
  scratch cleanup. It gives current Tactix 17/100 checked effective WV100
  coverage; no replacement full Tier-D run is planned now.
- Focused WV100 work uses `cpu-dense-hwulab-q`, c8, 32 GiB, and 300 solver
  seconds. B28+C5 and Rank1--50 gates use the same partition with c12, 32 GiB,
  and 1,200 seconds.
- Storage guard PID `1458620` checks repository use every ten minutes. Above
  700 GiB it holds only pending repository jobs and later releases only its own
  holds; it never pauses running jobs. Current repository use is 40 GiB.
- WV100 freezes the first 100 all-configuration ranking rows solved within
  1,200 seconds and 32 GiB by exactly one checked winner: 34 LymphoSAT, 57
  Satsuma+Kissat, seven AE-Kissat-MAB, and two kissat-sc2024. Solved Tactix rows
  remain in the denominator.

## Next actions

1. Remove the measured support-local default-path overhead and repeat B28+C5
   and Rank1--50 only after a causal repair; do not land the failed candidate.
2. Implement deterministic, resource-bounded stabilizer-chain clause fixing
   over formula-derived colored-CNF automorphisms. Close clique rank 4 with an
   effective transition and checked direct DSR, then continue across the
   57-row Satsuma WV100 group. Do not run full Tier D.

<!-- track-sched:begin -->
- Kissat creation order remains the production default; `TACTIX_ARENA_ORDER=1`
  is a CaDiCaL diagnostic arm. Unsupported formula-dependent profiles stay
  removed.
- Retain sound general repairs through downstream regressions; every commit must
  preserve N1--N4.
<!-- track-sched:end -->
