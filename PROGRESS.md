# Progress

[DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) owns stable scope and acceptance
criteria. This file is the current resume anchor; Git, immutable manifests, and
frozen reports own history and detailed evidence.

## Objective

Promote the default-off general Satsuma graph-symmetry producer through the
accepted online X7 interface while preserving N1--N4 and native checked DSR.
Then measure it on the fixed 57-row Satsuma WV100 signature group and continue
toward independently checked WV100 coverage of 100/100. Do not submit a full
Tier-D run after this symmetry increment; any future full Tier-D campaign needs
a separate decision after focused WV100 results.

## Authorities

| Item | Authority |
|---|---|
| Online refactor and N1 gate | source `a08f9f6a`; [closure report](benchmarks/reports/2026-09-13-online-refactor-n1-final.md) |
| Native DSR | source `20223134`; [checker repair](benchmarks/reports/2026-09-13-dsr-trim-tautology-hash-fix.md); [Claude PASS](benchmarks/reports/2026-09-13-native-dsr-claude-fuzz-r2/terminal-report.md) |
| Direct SR | source `298c3467`; [performance gate](benchmarks/reports/2026-09-13-direct-sr-performance-gate.md); [Claude PASS](benchmarks/reports/2026-09-13-direct-sr-r16-claude-fuzz-r2/terminal-report.md) |
| Symmetry candidate | snapshot `3bed3dd12240b99182a6688dbeb2ea09a95e67c7`, tree `28c801039631e8e7b9410fe35a4e49ccc5203794`; ref `refs/codex/snapshots/symmetry-3bed3dd12240` |
| Symmetry fuzz | [terminal PASS](benchmarks/reports/2026-09-14-symmetry-claude-fuzz/terminal-report.md), jobs `1689218` and `1689220` |
| Symmetry performance | [B28+C5 and Rank 1--50 report](benchmarks/reports/2026-09-14-graph-symmetry-performance-gate.md), jobs `1689221` and `1689222` |
| Satsuma WV57 screen | campaign `benchmarks/runs/20260914-0701-symmetry-3bed3dd1-wv57`, job `1689304` |
| Fixed winner target | [WV100](benchmarks/tiers/WV100.tsv), 33 SAT / 67 UNSAT; [mechanism protocol](docs/design/2026-09-13-wv100-mechanism-integration.md) |

The root checkout is authoritative. `.claude` contains workflow instructions,
not project implementation or documentation changes.

## Accepted base

- The configurable online-strategy/N1 refactor at `a08f9f6a` supports typed
  composition, explicit legal instances, online reconfiguration, seeded random
  execution, replay, interruption, and configurable strategy-owned limits.
- Native DSR at `20223134` is the canonical whole-solver certificate and uses
  anonymous node-local scratch. Proof production is mandatory; DRAT and LRAT
  remain compatibility formats and are not requirements for new Satsuma rules.
- Direct SR at `298c3467` passes proof, model, API, replay, sanitizer, fuzz, and
  retained-performance gates.

## Active symmetry checkpoint

- The uncommitted candidate vendors DejaVu `4c275e9e` behind a failure-safe
  `noexcept` boundary and exact-checks every proposed automorphism and derived
  group element against the live root formula before X7 mutation.
- Its typed C++ and C settings cover graph representation, traversal, seeded
  ordering, detection/group/fixing/emission limits, scope, batching, stopping,
  interruption, resumption, and `sr3` replay; `sr` and `sr2` remain readable.
- The exact snapshot passes 150/150 Release tests, 149/149 applicable
  ASan/UBSan tests, and the Claude-owned fuzz checkpoint. Two independent GCC
  13.1 builds have identical solver SHA-256
  `41e1a1c74aa70032fc3d6b897a46cc6e9cfff368cc95b3324719df45459240a2`.
- Clique rank 4 closes with 70 SR transitions in 1.863 seconds. Its combined
  DSR passes forward `dsr-trim` elaboration and the resulting LSR passes pinned
  Trestle; anonymous scratch cleanup is clean.
- The compile-time guard audit found no obsolete strategy configuration macros.
  The remaining public-header, platform, debug, journal, trace, and proof-shadow
  guards are active and retained.

## Gate state

| Gate | Result | Proof/check status |
|---|---|---|
| B28+C5 | 949.894 s vs 950.837 s, **0.999008x**, 33/33 outcomes | 31/33 checked clean; remaining checks run in background |
| Rank 1--50 | 8,125.694 s vs 8,123.812 s, **1.000232x**, 50/50 outcomes | 34/50 checked clean; remaining checks run in background |

Both complete solver-time vectors pass the 1.01 limit with no timeout. The
graph producer is default-off, so these runs test default-path overhead.
Completed proof checks still gate promotion but do not block independent work.
No reference solver was rerun.

## Operations

- Focused WV100 work uses `cpu-dense-hwulab-q`, c8, 32 GiB, and a 300-second
  solver timeout. B28+C5 and Rank 1--50 use the same partition, c12, 32 GiB,
  and a 1,200-second solver timeout.
- Repository usage is 108,326,965,785 bytes. Recheck before each submission;
  at 700 GiB or above, hold only pending repository jobs in ten-minute
  intervals. Never pause running jobs.
- Existing proof-checking work remains untouched. The prior full Tier-D run
  `1687184` is retained; no replacement full Tier-D run is planned.
- Focused job `1689304` runs the immutable symmetry snapshot on all 57
  Satsuma-owned WV100 rows at full array width. Its manifest enforces the
  focused resource contract and anonymous scratch; no winner solver is rerun.

## Next actions

1. Analyze checked transitions from WV57 job `1689304` and group the remaining
   misses by general mechanism.
2. Inspect every final validity and cleanup record from jobs `1689221` and
   `1689222`, then commit and promote the
   symmetry increment only if the complete gate establishes N1--N4.
3. Investigate clique-guided fixing and alternating root rounds against the
   largest measured residual group.

<!-- track-sched:begin -->
- Kissat creation order remains the production default; `TACTIX_ARENA_ORDER=1`
  is a CaDiCaL diagnostic arm. Unsupported formula-dependent profiles stay
  removed.
- Retain sound general repairs through downstream regressions; every commit must
  preserve N1--N4.
<!-- track-sched:end -->
