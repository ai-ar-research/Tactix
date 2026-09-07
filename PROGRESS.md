# Progress

The plan owns scope/gates, calculus owns semantics, implementation notes own
guards/representation, the parity ledger owns N4, and frozen reports own evidence.

## Current resume anchor

- Goal: **Rank 1–40 aggregate runtime ≤1.01x Kissat**, with B28+C5 regression
  gates and general component repairs. **Not met:** production is 1.2787518x.
- [Runtime review](benchmarks/reports/2026-09-07-runtime-review.md) owns the
  audit of Claude's work and retained/rejected evidence. Mixed proof modes and
  unchecked serialized arms do not establish “93% search quality.”
- Latest: [Asynchronous-check checkpoint](benchmarks/reports/2026-09-07-async-check-warmup.md).
  First choice split is conflict 13,832 after 18,516,149 matching decision rows.
  Complete heap matches at the first warm-up decision at 13,563; subsequent
  warm-up backtracking reinserts selected variables in different order.
  No new semantic repair or promotion; cumulative candidates remain **UNFUZZED**.
- [Dense-entry evidence](benchmarks/reports/2026-09-07-dense-entry-cost.md) owns
  the latest semantic candidate: Rank-21 peak RSS falls about 16.5%, runtime is mixed.
- Correction: stopped-prefix 5382 contains dormant hot-loop instrumentation;
  its 1.13834x time is not clean production overhead. Use clean profiles 5385.
- Production remains `c2a95a6`; Release SHA-256
  `de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f`.
  No source was promoted. Retain supported general repairs; do not chase
  trajectory identity when work is already stronger or neutral.

## Active jobs and gates

| Campaign | Job | Pending scope |
|---|---|---|
| 5395 coupled full Rank 1–40 | `1669252` | 77/80 checked; tasks 35, 42, 43 still checking. |

Keep these checks running while proceeding on a provisional-pass assumption;
revisit them at checkpoints, never label pending certificates verified.
5383/5389/5393 are terminal and checked; 5396–5410 diagnostics are
terminal (5401 deliberately cancelled and cleaned). User interactive allocation
`1668242` is untouched. Exact audit and stop limitations are in the latest report.

| Accepted authority | Result |
|---|---|
| [5295 starting audit](benchmarks/runs/20260907-5295-checked-runtime-audit/summary.json) | 80/80 checked/clean; starting Tactix/Kissat 1.2192x. |
| [5314 production Rank 1–40](benchmarks/runs/20260907-5314-pose-r2-ranks1-40/summary.json) | 80/80 checked/clean; both 40 solves (22 SAT/18 UNSAT); 8643.178/6759.074 s = 1.2787518x. |
| [5315 production B28+C5](benchmarks/runs/20260907-5315-pose-r2-BC-gate/summary.json) | 66/66 checked/clean; final-r2/starting time 1.01897x. |
| [5379 Vivify witnesses](benchmarks/runs/20260907-5379-vivify-retry-runtime/summary.json) | 12 checked models; 0.722370x preceding candidate and 1.186297x Kissat, one wave. |
| [5380 Vivify B28+C5](benchmarks/runs/20260907-5380-vivify-retry-BC-gate/summary.json) | 66/66 checked/clean, no timeout/solve loss; candidate/production 0.953796x. |
| [5383 Vivify Rank 1–40](benchmarks/runs/20260907-5383-vivify-retry-ranks1-40/summary.json) | 80/80 checked/clean; both 40 solves; 8217.738/6764.776 s = 1.214783x. |
| [5388 publication witnesses](benchmarks/runs/20260907-5388-dense-publication-runtime/summary.json) | 12 checked models; 0.997513x preceding candidate: neutral, one wave. |
| [5389 publication B28+C5](benchmarks/runs/20260907-5389-dense-publication-BC-gate/summary.json) | 66/66 checked/clean; no timeout/solve loss; candidate/production 0.950318x. |
| [5392 coupled witnesses](benchmarks/runs/20260907-5392-eliminate-entry-runtime/summary.json) | 12 checked models; 1.020252x preceding candidate, 1.205145x Kissat; mixed, one wave. |
| [5393 coupled B28+C5](benchmarks/runs/20260907-5393-eliminate-entry-BC-gate/summary.json) | 66/66 checked/clean; no timeout/solve loss; candidate/production 0.922328x. |

Rank 1–40 is not an annual/union Tier-D replacement. Memory competitiveness
is an independent acceptance dimension, not discharged by a runtime ratio.

## Exact next action

1. Trace Rank 35's selected warm-up implications at conflict 13,563, especially
   variable 109796 relative to 109815/109877; compare reasons, trail order and
   watch/assignment producers before the differing warm-up backtrack inserts.
   Root removal matches (5406); first warm-up heap is fully identical (5410).
   Use validated observers 5402 with narrow imply filters, not broad 5401.
   Use provisional solver results to guide work while checks run. Rank 27's three-arm
   decision streams match through 100,000 conflicts (28,605,461 rows); its split
   is later. Coupled Rank 37 regresses substantially; inspect it next. Memory
   capacity remains independent. Provisional full-view coupled ratio is 1.238899x.
2. Revisit active checker outcomes without blocking the investigation; run
   `seal-eliminate-ranks-5395.py` when complete; 5383 is already sealed.
   Preserve per-instance work/time/memory and label verification status.
   Any rejected proof/model pauses dependent work under plan §8.2.
3. Preserve submitted sources/pins. Latest source is
   `.claude/worktrees/runtime-eliminate-dense-entry-5391`; Release
   `build/pin-5392-eliminate-dense-release` and independent
   `build/pin-5393-eliminate-dense-bc` share SHA-256
   `5236e63f5ba3fc27e448d23c2a4b92e1948bb9c1390f336cc0852eed66cf64e2`.
   Earlier Vivify source/pins and publication-only 5387 remain immutable.
4. Consolidate only supported general repairs, finish runtime/B28+C5 gates,
   then invoke **one Claude-owned fuzz campaign** under plan §8.2. Do not inspect
   or reinterpret Claude's fuzz artifacts. Route reproduced soundness/crash/
   proof/model failures to bounded Claude repair immediately.
5. Keep broad cleanup separate. Preserve proof-before-deletion, native physical
   duplicates, N1–N4 and open ledger obligations. Push remains **unapproved**
   after the earlier auto-review rejection; do not retry or bypass it.

## Immutable evidence and validation

- The runtime review routes historical campaigns. The
  [Vivify report](benchmarks/reports/2026-09-07-vivify-retry-repair.md) owns
  5377–5383, predecessor links, all 2107 BVE attempts and exact
  1,928,902 decisions through one million conflicts, not the full trajectory.
- The dense-entry report owns 5384–5395, both physical-mode repairs, profiles,
  measured memory ownership, initial fixture failure and Claude's terminal
  fixture-only repair. Final tests: 76/76 trace, 77/77 in each Release build.
  No new fuzz was launched or inspected; the candidate remains unfuzzed.
- The [heap report](benchmarks/reports/2026-09-07-rank35-heap-localization.md) owns 5396–5405, validated stream/window and direct heap
  observers, exact prefix/heap witnesses and cancellation cleanup. Both new
  trace builds pass 76/76 fixed tests; production source remains unchanged.
- Production passes 77/77 CTests. Claude's authoritative
  [r2 fuzz report](benchmarks/reports/2026-09-07-sweep-pose-fuzz.md) is PASS;
  regressions 028–030 remain. This does not cover isolated candidates.
- N4 verification remains pending. Historical accepted source `bad5cbf` and
  `20260829-1930-tierd-ranks1-40-accepted-fresh` are not same-window baselines.
  Frozen references: `benchmarks/baselines/sc24-26-32g-5000s/`. T1/T2 and
  historical T3 passed; T4-K/T4-VBS/subsequent T5 remain open.
- Raw large streams stay local with hashes; small patches, runners, comparators
  and witnesses are durable. Legacy prefix `start_decisions` wraps with warm-up
  counts and must never be used for alignment.

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

- Narrowed Rank 35's heap-order difference to warm-up after a common entry heap.
- Matched all three Rank-27 decision streams through 100,000 conflicts.
- Completed Vivify Rank 1–40: 80/80 checked/clean, 1.214783x Kissat.
- Made proof checks asynchronous for dependent investigation, retaining final gates.
- Kept living docs within their budgets; detailed evidence remains in frozen reports.

Git and primary reports own history; prior resume text is in `a336e013:PROGRESS.md`.
