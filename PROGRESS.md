# Progress

The plan owns scope/gates, calculus owns semantics, implementation notes own
guards/representation, the parity ledger owns N4, and frozen reports own evidence.

## Current resume anchor

- Goal: **Rank 1–40 aggregate runtime ≤1.01x Kissat**, with B28+C5 regression
  gates and general implementation repairs. **Not met:** production is 1.2787518x.
- [Runtime review](benchmarks/reports/2026-09-07-runtime-review.md) owns the audit
  of Claude's work. Campaign 5291's mixed proof modes/unchecked serialized arms
  cannot support “93% search quality” or an implementation-cost ceiling.
- Latest: [BVE retry frontier](benchmarks/reports/2026-09-07-bve-retry-frontier.md).
  The isolated Congruence stack matches **786,346 decisions** before a split
  at conflict 345034. BVE's initial schedule at 343785 omits 12 native members;
  trace retry-mark producers after the last matching attempt at 240040.
  No new semantic edit. **UNFUZZED, unpromoted; B28+C5 pending.**
- Production source remains `c2a95a6`; Release SHA-256
  `de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f`.
  Retain accepted binary substitution/liveness, arena/watch, connected-pose and
  dense-image lifecycle repairs. No source was promoted at this checkpoint.

## Active jobs and gates

**IN FLIGHT:** 5372 B28+C5 / job `1668833`, tasks 64/65 checking UNSAT proofs;
64/66 final records are checked/clean. Watch this exact job; never resubmit or
count unfinished checks as accepted. 5371 runtime and old 5357 are terminal and
clean. User interactive allocation `1668242` (`bash`) is untouched.
Diagnostics 5373–5376 are terminal, with eight clean intentional stops and
validated observer-prefix neutrality. They are not verdict or timing gates.

| Authority | Accepted evidence |
|---|---|
| [5295 starting audit](benchmarks/runs/20260907-5295-checked-runtime-audit/summary.json) | 80/80 checked/clean; starting Tactix/Kissat 1.2192x. |
| [5314 production Rank 1–40](benchmarks/runs/20260907-5314-pose-r2-ranks1-40/summary.json) | 80/80 checked/clean, both 40 solves (22 SAT/18 UNSAT); 8643.178/6759.074 s = 1.2787518x. |
| [5315 production B28+C5](benchmarks/runs/20260907-5315-pose-r2-BC-gate/summary.json) | 66/66 checked/clean; final-r2/starting time 1.01897x. |
| [5356 preceding runtime screen](benchmarks/runs/20260907-5356-empty-factor-runtime/summary.json) | 12 checked models; preceding cumulative/production time 1.11539x on three witnesses. One wave, no promotion. |
| [5357 preceding B28+C5](benchmarks/runs/20260907-5357-empty-factor-BC-gate/summary.json) | 66/66 checked/clean, no timeout/solve loss; candidate/production 1060.139/1115.715 s = 0.950188x. |
| [5370 repaired stopped replay](benchmarks/runs/20260907-5370-congruence-prefix-extended/summary.json) | Both stops clean; all 475,997 decision records identical through conflict 200000. Not a verdict or timing gate. |
| [5371 repaired runtime screen](benchmarks/runs/20260907-5371-congruence-prefix-runtime/summary.json) | 12/12 checked models; candidate/preceding 0.86041x, candidate/production 0.96375x, candidate/Kissat 1.63903x. Three witnesses, one wave; no global claim. |

Rank 1–40 is not an annual/union Tier-D replacement. Memory competitiveness
remains an independent acceptance dimension.

## Exact next action

1. Complete 5372 with the tool-owned SLURM watcher, then run
   `benchmarks/probes/scratch/seal-congruence-gates-5372.py`. Validate final records,
   checks and scratch cleanup before sealing summaries; retain per-instance
   solve/work/time/memory differences. The same-window production and exact
   preceding cumulative arms must not be conflated.
2. Trace retry-mark producers for missing member 10700 between the second BVE
   attempt at 240040 and scheduling at 343785. Seven prior attempts match exact
   ticks; shared initial occurrence counts agree, but native schedules 12 extra
   members. Use fresh observer-only source/pins; do not mutate submitted builds.
   Prove the mechanism and downstream utility before repairing: Tactix's extra
   elimination of 9959 is not intrinsically worse. No identity-conditioned fix.
   Inprocessing cost on Rank 17 is a separate matched-work profiling lead.
3. Preserve `.claude/worktrees/runtime-congruence-prefix-5369` and its submitted
   builds. Release `build/pin-5371-congruence-prefix-release` and independent
   `build/pin-5372-congruence-prefix-bc` share SHA-256
   `4f406acc56fe6cf8a68d32b8567a845c8cb08cd164286fd838b4398fa9b3a2be`.
   The new patch reuses logical-position helpers; no new kernel primitive.
4. At consolidation, compose only supported general repairs, finish checked
   B28+C5/runtime gates, then invoke **one Claude-owned fuzz campaign** under
   plan §8.2. Do not inspect or reinterpret Claude's fuzz artifacts. A reproduced
   soundness/crash/proof/model failure triggers bounded Claude repair immediately.
5. Keep codebase cleanup separate from causal repairs. Preserve proof-before-
   source-deletion ordering, native duplicate copies, and N1–N4. Push remains
   **unapproved** after the earlier auto-review rejection; do not retry/bypass.

## Isolated candidate provenance

- [Original-core convergence](benchmarks/reports/2026-09-07-original-core-convergence.md)
  owns 5316–5336: clausal core versus LRAT proof steps, save-time original-unit
  eligibility, sequential certified rewrites and physical binary publication.
- [Duplicates and retirement](benchmarks/reports/2026-09-07-reference-duplicates-and-retirement.md)
  owns 5337–5347: native duplicates are real; per-round retirement repair closes
  heap/warm-up state. `SP-SUBSTITUTE-ROUND-1` remains open in production.
- [BVE frontier](benchmarks/reports/2026-09-07-bve-choice-frontier.md) owns 5348–5351;
  [empty Factor housekeeping](benchmarks/reports/2026-09-07-empty-factor-housekeeping.md)
  owns 5352–5359. Empty-pass restamp/reset closes all 15,386 BVE candidates.
- [Congruence frontier](benchmarks/reports/2026-09-07-congruence-prefix-frontier.md)
  owns 5360–5372. `SP-CONG-PREFIX-1` remains open in production. Fixed native
  positive / old negative / repaired positive plus 76 trace / 77 Release tests
  pass; the cumulative stack is still unfuzzed investigation material.
- [Retry frontier](benchmarks/reports/2026-09-07-bve-retry-frontier.md) owns
  5373–5376 and the exact candidate, eligibility and choice-state evidence.
- Raw large streams remain local with hashes; patches, runners, comparators and
  small witnesses are durable. Legacy prefix `start_decisions` wraps with
  warm-up counts and must never be used for alignment.

## Validation and immutable authorities

- Production passes 77/77 CTests. Claude's authoritative
  [r2 fuzz report](benchmarks/reports/2026-09-07-sweep-pose-fuzz.md) is PASS;
  regressions 028–030 remain. No new fuzz campaign was launched here.
- N4 verification remains pending; compaction accepts no new deviation.
- Historical accepted source `bad5cbf7548c6abf1181d36150206ac6f52c316f` and
  `20260829-1930-tierd-ranks1-40-accepted-fresh` are historical, not same-window
  baselines. Frozen references: `benchmarks/baselines/sc24-26-32g-5000s/`.
  T1/T2 and historical T3 passed; T4-K/T4-VBS/subsequent T5 remain open.

## Architectural contract

<!-- track-sched:begin -->
- Kissat creation order is the unconditional production default;
  `TACTIX_ARENA_ORDER=1` is an explicit CaDiCaL diagnostic arm only.
- Unsupported formula-dependent profiles stay removed; retain sound general
  repairs through downstream regression, not compensating behavior.
- Preserve N1 universal API, N2 proofs/models, N3 runtime/memory and
  N4 reference-strength scheduling.
<!-- track-sched:end -->

## Recent changes

- Extended the isolated stack to 786,346 identical decisions before a new split.
- Localized the next initial-schedule discrepancy to BVE retry eligibility.
- Preserved seven matching prior attempts; no speculative heap repair.
- New B28+C5 final proof checks are running; production unchanged.
- Living docs remain within all 30 configured size budgets.

Historical evidence is routed by the runtime review; Git owns ordinary
chronology. Previous resume text is recoverable with `git show 4bd01e1:PROGRESS.md`.
