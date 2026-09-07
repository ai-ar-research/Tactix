# Progress

The plan owns scope/gates, calculus owns semantics, implementation notes own
guards/representation, the parity ledger owns N4, and frozen reports own evidence.

## Current resume anchor

- Goal: **Rank 1–40 aggregate runtime ≤1.01x Kissat**, with B28+C5 regression
  gates and general implementation repairs. **Not met:** production is 1.2787518x.
- [Runtime review](benchmarks/reports/2026-09-07-runtime-review.md) owns the audit
  of Claude's work. Campaign 5291's mixed proof modes/unchecked serialized arms
  cannot support “93% search quality” or an implementation-cost ceiling.
- Latest: [Vivify retry repair](benchmarks/reports/2026-09-07-vivify-retry-repair.md).
  Isolated learned-large retry advice closes all 2107 BVE attempts and matches
  **1,928,902 decisions through one million conflicts**. Checked three-witness
  time is 0.72237x preceding candidate, 0.69939x production, 1.18630x Kissat
  (one wave). **UNFUZZED/unpromoted; full Rank 1–40 and B28+C5 pending.**
- Production source remains `c2a95a6`; Release SHA-256
  `de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f`.
  Retain accepted binary substitution/liveness, arena/watch, connected-pose and
  dense-image lifecycle repairs. No source was promoted at this checkpoint.

## Active jobs and gates

**IN FLIGHT:** full Rank 1–40 / 5383 / `1669005`; new B28+C5 / 5380 /
`1668924` (64/66 final records; tasks 64/65 checking proofs).
Watch these exact jobs; never resubmit or accept unfinished checks. Preceding
Congruence B28+C5 5372, runtime 5379 and diagnostics 5377/5378/5381/5382
are terminal and clean. User
interactive allocation `1668242` (`bash`) is untouched.

| Authority | Accepted evidence |
|---|---|
| [5295 starting audit](benchmarks/runs/20260907-5295-checked-runtime-audit/summary.json) | 80/80 checked/clean; starting Tactix/Kissat 1.2192x. |
| [5314 production Rank 1–40](benchmarks/runs/20260907-5314-pose-r2-ranks1-40/summary.json) | 80/80 checked/clean, both 40 solves (22 SAT/18 UNSAT); 8643.178/6759.074 s = 1.2787518x. |
| [5315 production B28+C5](benchmarks/runs/20260907-5315-pose-r2-BC-gate/summary.json) | 66/66 checked/clean; final-r2/starting time 1.01897x. |
| [5356 preceding runtime screen](benchmarks/runs/20260907-5356-empty-factor-runtime/summary.json) | 12 checked models; preceding cumulative/production time 1.11539x on three witnesses. One wave, no promotion. |
| [5357 preceding B28+C5](benchmarks/runs/20260907-5357-empty-factor-BC-gate/summary.json) | 66/66 checked/clean, no timeout/solve loss; candidate/production 1060.139/1115.715 s = 0.950188x. |
| [5371 repaired runtime screen](benchmarks/runs/20260907-5371-congruence-prefix-runtime/summary.json) | 12/12 checked models; candidate/preceding 0.86041x, candidate/production 0.96375x, candidate/Kissat 1.63903x. Three witnesses, one wave; no global claim. |
| [5372 preceding B28+C5](benchmarks/runs/20260907-5372-congruence-prefix-BC-gate/summary.json) | 66/66 checked/clean, no solve loss/timeout; candidate/production 1016.888/1117.371 s = 0.910072x. |
| [5379 retry runtime screen](benchmarks/runs/20260907-5379-vivify-retry-runtime/summary.json) | 12/12 checked models; 642.637/889.623/918.858/541.717 s candidate/preceding/production/Kissat. One wave, no global claim. |
| [5381 repaired prefix](benchmarks/runs/20260907-5381-vivify-retry-extended/summary.json) | All 1,928,902 decisions match through one million conflicts; not a verdict or cost gate. |

Rank 1–40 is not an annual/union Tier-D replacement. Memory competitiveness
remains an independent acceptance dimension.

## Exact next action

1. Complete 5383 and 5380 with the tool-owned SLURM watchers. Run
   `seal-vivify-ranks-5383.py` and `seal-vivify-retry-gates-5380.py`
   under `benchmarks/probes/scratch/` only against
   final checked/clean records. Score the full requested view and retain
   per-instance work/time/memory, solve losses and timeout transitions.
2. Prioritize the largest remaining full-view gaps. Rank 34's checked solve is
   now 351.436 versus native 331.297 s, with fewer conflicts but more propagations;
   do not spend another campaign making a stronger/neutral trajectory identical.
   Trace-free prefix diagnostic 5382 shows 1.13834x time and excess instructions/
   memory at matching conflict/decision counts, not exact work equality. Profile
   common-prefix hot paths; Ranks 17/21 and full-view outliers remain cost leads.
3. Preserve `.claude/worktrees/runtime-vivify-retry-5378` and every submitted
   predecessor. `build/pin-5379-vivify-retry-release` and independent
   `build/pin-5380-vivify-retry-bc` share SHA-256
   `50cfa880aa01e15f47d63eb28ce0ea47fb51bfc912ecc58d3b755fb21134bbc8`.
   The repair uses existing mark advice; no new kernel primitive or guard.
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
- [Retry repair](benchmarks/reports/2026-09-07-vivify-retry-repair.md) owns
  5377–5383. `SP-VIVIFY-RETRY-1` stays open in production. Fixed native/old/new,
  76 serial trace and 77 fresh Release tests pass; one initial harness cleanup
  failure is retained. The cumulative stack is still unfuzzed investigation.
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

- Isolated learned-large Vivify retry repair closes the BVE/choice frontier.
- Repaired prefix matches 1,928,902 decisions through one million conflicts.
- Checked witness time improves 27.8% versus the preceding candidate, one wave.
- Full Rank 1–40 and B28+C5 are running; production unchanged.
- Living docs remain within all 30 configured size budgets.

Historical evidence is routed by the runtime review; Git owns ordinary
chronology. Previous resume text is recoverable with `git show d1a67838:PROGRESS.md`.
