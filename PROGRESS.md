# Progress

[DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) owns stable scope and acceptance
criteria. This file is the compact resume anchor; Git, immutable run manifests,
and frozen reports own history and evidence.

## Current objective

The online-configurable strategy/N1 refactor is accepted at `a08f9f6a`, and
native DSR is landed at `20223134` as Tactix's canonical whole-solver
certificate. The working tree adds general direct-DSR substitution redundancy
and its first orbitopal Satsuma producer. The increment is **not accepted**:
its first Claude checkpoint failed. R16 has repaired that candidate and passed
both complete performance gates; fresh full tests and the replacement Claude
checkpoint remain required before landing.

Finish those gates while preserving N1--N4 and reference-default behavior, then
complete the Satsuma symmetry producer and resume mechanism-driven work on the
fixed WV100 target. Do not submit a full Tier-D run at this boundary. Kissat-only
tuning, benchmark dispatch, and sequential portfolios remain out of scope; MAB
scheduling remains an open design choice.

## Authorities

| Item | Authority |
|---|---|
| Accepted refactor | source `a08f9f6a`; [N1/refactor closure](benchmarks/reports/2026-09-13-online-refactor-n1-final.md) |
| Strategy design | [online proposal](docs/design/2026-09-11-online-strategy-proposal.md) and [preservation addendum](docs/design/2026-09-11-strategy-behavior-preservation-addendum.md) |
| Native DSR | source `20223134`; [checker repair](benchmarks/reports/2026-09-13-dsr-trim-tautology-hash-fix.md); [passing Claude checkpoint](benchmarks/reports/2026-09-13-native-dsr-claude-fuzz-r2/terminal-report.md) |
| DSR baselines | [B27+C5](benchmarks/baselines/b27c5-tactix-a08f9f6a-v1/manifest.json) and [Rank1--40](benchmarks/baselines/rank1-40-tactix-a08f9f6a-v1/manifest.json) |
| Direct-SR evidence | [performance gate](benchmarks/reports/2026-09-13-direct-sr-performance-gate.md) and [failed Claude checkpoint](benchmarks/reports/2026-09-13-direct-sr-claude-fuzz/terminal-report.md) |
| Winner target | [fixed WV100 set](benchmarks/tiers/WV100.tsv), 33 SAT / 67 UNSAT; [experiment protocol](docs/design/2026-09-13-wv100-mechanism-integration.md) |
| Satsuma evidence | [transition screen](benchmarks/reports/2026-09-13-wv100-satsuma-transition.md), [orbitopal G0](benchmarks/reports/2026-09-13-wv100-orbitopal-sr-g0.md), and [symmetry G2](benchmarks/reports/2026-09-13-wv100-structured-symmetry-g2.md) |

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

## Working direct-SR increment

- X7 accepts a clause, witness literals, and finite substitution; it validates
  the complete SR condition before mutation. Typed online v2 supports explicit
  actions, selected batches, resumable complete enumeration, orbitopal
  production, composition, budgets/stops, and reconfiguration. All limits and
  producer choices are validated strategy settings.
- DefaultPolicy's producer recognizes exact orbitopal matrices, validates each
  adjacent-row swap against the complete clause multiset, and emits prefix
  fixing before Factor. The current bounded screen reduces zero-yield overhead
  to 0.002--0.088 seconds while the effective control emits all 66 checked SR
  additions; see the performance report above.
- Claude job `1688763` rejected the earlier candidate for wrong verdicts,
  invalid ordinary proofs, checker/kernel population mismatches, and invalid
  DSR-to-LSR output. The repair publishes DSR root facts, canonicalizes signed
  substitution sources, validates direct non-LRAT Congruence conclusions,
  rejects late/duplicate C proof attachment, and retires proof-only Substitute,
  BVE, and Sweep lemmas at completion and interruption boundaries.
- Corrected-parser preflight `1688775` passes all **15/15** retained S3/S4/P1
  DSR, DRAT, and LRAT cases; focused rule, API, online, and Sweep proof tests pass.
- R11 pin `5fee7dab` passed full Release/sanitizer tests and proof preflight but
  failed performance at **1.163x** B28+C5 because broad proof-only Congruence
  replay made rank 8 take 117.0 seconds. Its gate jobs were canceled without
  rerunning a reference.
- R15 source `cb32c410` restricts replay to the two direct shapes required by S3
  and separates DSR publication from solver `unit_ids`. Preflight `1688872`
  passes **15/15**, full tests `1688873` pass **148/148 Release** and **147/147
  sanitizer-applicable**, and rank 8 returns to 2.670 seconds.
- R15 B28+C5 job `1688874` is clean on all **33/33** outcomes but measures
  961.725 versus 950.837 seconds (**1.011451x**), missing the strict 1% target by
  1.38 seconds; rank 33 contributes 12.234 seconds. Rank1--50 job `1688903`
  completed its solver phase at 48/50 solves: ranks 12 and 47 timed out after
  the same proof-only replay regression, while final proof checks remain active.
- Those three Rank regressions preserve decisions, conflicts, and rule outcomes
  but add 1.1--1.9 billion proof-only Congruence propagations. R16 source
  `298c3467` keeps the S3-required selected mixed ITE/AND admission check while
  removing duplicate canonical replay. Preflight `1688957` passes **15/15**;
  probe `1688958` restores the exact reference propagation trajectories on
  ranks 17, 22, and 48. B28 rank-33 probe `1688961` likewise restores the exact
  reference decisions, conflicts, propagations, and Congruence work at 313.580
  seconds versus 310.992 seconds (**1.008322x**). Substituting that result into
  the completed R15 vector gives a provisional 951.217 seconds versus the
  retained 950.837 seconds (**1.000400x**). Full candidate-only B28+C5 job
  `1688962` then completed all **33/33** solver outcomes at 952.263 seconds
  (**1.001500x**), and all 33 proof/model checks pass. Rank1--50 job `1688979`
  completed all **50/50** expected outcomes with no timeout at 8,115.735 seconds
  versus retained 8,123.812 seconds (**0.999006x**). Its proof/model checks
  continue in the background. Neither gate reran a retained reference.
- Claude froze replacement checkpoint `build/pin-srfuzz-direct-r16-298c3467`
  after its corrected H1--H4 harness caught every planted failure and passed
  **4,046** self-checks. Exactly one non-array fuzz job, `1689026`, is pending
  on `cpu-dense-hwulab-q`; its terminal audit remains required for acceptance.
- Fresh immutable-source test job `1689035` was canceled after its external
  scratch build exposed a harness-path error: six source-conformance scripts
  could not locate their Git worktree, while all 142 executed binary tests
  passed. Corrected job `1689048` binds those scripts to the immutable R16
  source, rebuilds in node-local scratch, requires byte identity with the
  performance pin, and passes **148/148 Release** plus **147/147** applicable
  ASan/UBSan tests.
- Focused WV100 job `1689041` tested three structured Satsuma signatures from
  the same R16 pin under the c8/32-GiB/300-second protocol. It solves
  `php_sudoku_p15_h14` UNSAT in 0.166 seconds after 91 direct-SR additions;
  clique rank 4 and channel rank 17 time out, confirming that the current
  row-only producer does not yet cover the broader Satsuma mechanisms.

## Operations

- Existing proof-checking jobs continue in the background and do not block
  independent work. Do not rerun retained reference solvers.
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
  holds; it never pauses running jobs. Its 22:22 check measured 39 GiB.
- WV100 freezes the first 100 all-configuration ranking rows solved within
  1,200 seconds and 32 GiB by exactly one checked winner: 34 LymphoSAT, 57
  Satsuma+Kissat, seven AE-Kissat-MAB, and two kissat-sc2024. Solved Tactix rows
  remain in the denominator.

## Next actions

1. Finish the single replacement Claude-owned fuzz checkpoint and audit every
   retained failure before promotion.
2. Leave Rank proof checks in the background while using the focused WV100
   results to extend the general Satsuma symmetry producer.
3. After every gate passes, land the coherent N1--N4 increment and resume WV100
   by mechanism/signature group.

<!-- track-sched:begin -->
- Kissat creation order remains the production default; `TACTIX_ARENA_ORDER=1`
  is a CaDiCaL diagnostic arm. Unsupported formula-dependent profiles stay
  removed.
- Retain sound general repairs through downstream regressions; every commit must
  preserve N1--N4.
<!-- track-sched:end -->
