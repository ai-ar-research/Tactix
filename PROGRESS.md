# Progress

The plan owns scope/gates, calculus owns semantics, implementation notes own
guards/representation, the parity ledger owns N4, and frozen reports own evidence.

## Current resume anchor

- Goal: **Rank 1–40 aggregate runtime ≤1.01x Kissat**, with B28+C5 regression
  gates and general component repairs. **Not met:** production is 1.2787518x.
- [Runtime review](benchmarks/reports/2026-09-07-runtime-review.md) owns the
  audit of Claude's work and retained/rejected evidence. Mixed proof modes and
  unchecked serialized arms do not establish “93% search quality.”
- Latest implementation: [Watch-pair append](benchmarks/reports/2026-09-07-watch-pair-append.md), candidate 5466,
  adds an exact-layout two-word fast path within existing capacity on 5459.
  Both Release suites pass 83/83; 180 actual old/new cases have identical
  whole-state digests. Three exact-work timing pairs and B28+C5 are queued.
- Preceding [Bound lifecycle](benchmarks/reports/2026-09-07-bound-lifecycle.md), candidate 5459,
  restores the documented phase-bound advance and retry flags after quota cuts.
  Release 82/82 twice, trace 81/81; two old/new cut-bound fixtures pass.
  It retains [5450's strict phase stop](benchmarks/reports/2026-09-07-eliminate-phase-budget.md).
  Rank-37 Factor schedules match; differing gain-2 admission selects this repair.
  Causal replay now matches all 35,346,388 decisions through 100000 conflicts;
  272 portable boundaries also agree. Full runtime and B28+C5 remain pending.
- [Complete phase-budget view](benchmarks/reports/2026-09-07-phase-budget-full-view.md):
  5454 has all 40 pairs at **1.170283x**, 63/80 checked at the solver snapshot.
  No timeout/verdict loss; goal still unmet. Largest losses are Ranks 31/37/32.
  Phase-entry 5434 is now fully checked at 1.205580x; different windows prevent
  attributing their ratio difference to one repair.
- [Short witnesses and native bound](benchmarks/reports/2026-09-08-short-witnesses-and-bound.md):
  5458 confirms native bound 1→2 after the quota cut. All nine short-B solves
  are checked; old/new work, proof bytes and slot logs agree, so they are
  controls rather than cut reproducers. Square now has a short causal probe.
- [Full view and next boundaries](benchmarks/reports/2026-09-07-runtime-next-boundaries.md):
  Rephase's full 40-pair view is **1.202034x**, now 80/80 checked and clean.
  Its nine timing proofs and B28+C5 are now fully checked. Metadata 5426 has a
  small 0.993882x median exact-work gain, not a solution to the aggregate gap.
- [Root-entry follow-up](benchmarks/reports/2026-09-07-root-entry-followup.md):
  5430 restores missing root closure before Probe/Eliminate, with distinct
  search-tick/phase-saving scopes. Actual old/new scheduler fixtures confirm it.
  Independent 5426 removes redundant watch-metadata writes without state changes.
  Both are **UNFUZZED**, unpromoted; phase-entry timing/gates are in flight.
- [Rephase closure](benchmarks/reports/2026-09-07-rephase-root-closure.md) owns
  exact Rank-35 prefix/heap closure and now-checked paired 0.658243x preceding
  candidate, 1.183228x Kissat. New clean profiles prioritize inprocessing cost.
- [Dense-entry evidence](benchmarks/reports/2026-09-07-dense-entry-cost.md) owns
  the preceding candidate: Rank-21 peak RSS falls about 16.5%, runtime is mixed.
- Correction: stopped-prefix 5382 contains dormant hot-loop instrumentation;
  its 1.13834x time is not clean production overhead. Use clean profiles 5385.
- Production remains `c2a95a6`; Release SHA-256
  `de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f`.
  No source was promoted. Retain supported general repairs; do not chase
  trajectory identity when work is already stronger or neutral.

## Active jobs and gates

| Campaign | Job | Pending scope |
|---|---|---|
| 5423 Rephase full Rank 1–40 | `1669887` | Complete: 80/80 checked, 1.202034x. |
| 5429 metadata B28+C5 | `1669969` | Complete: 66/66 checked, 0.924072x production. |
| 5433 phase-entry prefix | `1669971` | Complete: unchanged from Rephase through 100,000. |
| 5434 phase-entry Rank 1–40 | `1669972` | Complete: 80/80 checked, 1.205580x. |
| 5435 phase-entry B28+C5 | `1669973` | Complete: 66/66 checked and clean; 0.920290x production. |
| 5436 Rank-37 prefix | `1669994` | Complete: first decision split at conflict 29,767. |
| 5437 Rank-35 Sweep binaries | `1670050` | Complete: two duplicate multiplicities only; unique sets equal. |
| 5440 Vivify maximum timing | `1670118` | Complete: six checked proofs, median 0.995069x. |
| 5441 Vivify maximum B28+C5 | `1670119` | Complete: 66/66 checked, 0.922120x production. |
| 5447 replacement cursor timing | `1670198` | Complete: six checked proofs; median 0.976662x. |
| 5448 replacement cursor B28+C5 | `1670199` | Fixed cumulative gate; queued. |
| 5449 Rank-37 elimination limit | `1670217` | Complete: meter exceeds phase limit by 26 before extra round. |
| 5453 Rank-37 budget prefix | `1670291` | Complete: repaired boundary, next split at Factor, conflict 32,073. |
| 5454 phase-budget Rank 1–40 | `1670292` | All 80 expected solves, 1.170283x; proof tail running. |
| 5455 phase-budget B28+C5 | `1670293` | Fixed cumulative gate; queued. |
| 5457 Factor selected window | `1670359` | Complete: observer-neutral, first-call grids equal by ordinal. |
| 5458 native bound verbosity | `1670509` | Complete: quota cut followed by 1→2; prefix unchanged. |
| 5462 short B bound witnesses | `1670635` | Complete: nine checked/clean; old/new behavior identical. |
| 5463 Rank-37 bound prefix | `1670637` | Complete: all 35,346,388 decisions and 272 boundaries match. |
| 5464 bound B28+C5 | `1670636` | Fixed cumulative gate; checker tail running. |
| 5465 bound Rank 1–40 | `1670638` | Full unchanged paired view; running. |
| 5468 watch-pair timing | `1670699` | Three exact-work predecessor pairs; queued. |
| 5469 watch-pair B28+C5 | `1670700` | Fixed cumulative gate; queued. |
| 5470 Rank-31 bound prefix | `1670801` | New largest loss: complete 100000-conflict stream; queued. |
| 5471 Square bound prefix | `1670919` | Short B inprocessing witness, stop at 10000 conflicts; queued. |

User-requested periodic fuzz checkpoint 5442 is owned by background Claude
process `2920923` (parent tool session `30094`), testing immutable source 5438.
Read only its terminal response; do not inspect its jobs, logs or fuzz artifacts.
Continue independent development while pending; no passing gate is inferred.
This campaign does not cover 5445/5450/5459/5466; include all in the next periodic checkpoint.

Keep these checks running while proceeding on a provisional-pass assumption;
revisit them at checkpoints, never label pending certificates verified.
5383/5389/5393/5395/5421/5422 are terminal and checked; diagnostics through 5424 are terminal
(5401/5411 cancelled, 5413 timed out; scratch cleaned). User interactive allocation
`1668242` is untouched. Exact audit and stop limitations are in the latest report.
5425's two profiles and 5428's six timing proofs are now fully checked and clean.

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
| [5395 coupled Rank 1–40](benchmarks/runs/20260907-5395-eliminate-entry-ranks1-40/summary.json) | 80/80 checked/clean; both 40 solves; 8378.399/6762.780 s = 1.238899x. |
| [5422 Rephase B28+C5](benchmarks/runs/20260907-5422-rephase-root-BC-gate/summary.json) | 66/66 checked/clean, no solve loss/timeout; candidate/production 0.936764x. |

Rank 1–40 is not an annual/union Tier-D replacement. Memory competitiveness
is an independent acceptance dimension, not discharged by a runtime ratio.

## Exact next action

1. Evaluate 5468 with `snapshot-watch-pair-runtime-5468.py`: require all three
   complete statistics/proof-byte identities before interpreting paired timing.
   Latest frozen source `.claude/worktrees/runtime-watch-pair-5466` and Release
   5466/5467 share SHA `152da8f372022836…`; its manifest owns exact identities.
   5458 confirms native bound advancement after the cut; 5463 closes the
   100000-conflict prefix, including all 272 portable boundaries. First-call 5457 grids
   are equal by member ordinal; raw observer emission order is not a mismatch.
   5462's short cases have identical old/new work/proof/slot traces; they do
   not reproduce the repaired cut. Run `compare-square-prefix-5471.py` after
   job 1670919 finishes to localize Square's substantial inprocessing difference.
   Short diagnostics do not replace the fixed regression gates.
   Preceding source `.claude/worktrees/runtime-bound-advance-5459` is frozen;
   Release 5459/BC 5461 share SHA `84300eafeef6c973…`, trace 5460 `509877b2…`.
   Its manifest owns full hashes, tests and patches. Watchers 50218 (5463) and
   18033 (5458) completed. No runtime gain is established for 5459.
   5453 is terminal and compared: 7,918,162 matching rows (was 7,504,348).
   `boundary-localization.json` places the next coarse split at the second
   Factor call, conflict 32,073: common input active 148610/binary 181122;
   output Tactix active 148613/binary 181013, Kissat 148618/180960. This
   Factor admission difference is now closed by 5459;
   coarse counts alone were not its proof. Inspect full 5454 work,
   runtime and memory, and seal 5454/5455 with `seal-eliminate-limit-gates-5455.py`.
   5454's complete solver view is 1.170283x. `next-losses.json` selects Rank 31
   (+154.211 s, conflicts 1.542264x) while Rank 37's work drops materially.
   Run `compare-rank31-prefix-5470.py` after job 1670801 finishes; localize its
   earliest common-state component boundary without inferring weakness from
   divergence alone. No causal timing attribution from different campaign windows.
   5437's unique binary sets agree: two physical duplicate copies explain the
   count split. Do not restore duplicates without evidence of a causal defect.
   Vivify 5440's small exact-work 0.995069x median gain now has six checked proofs.
   5447 now has three complete exact-work/proof-byte pairs: median 0.976662x
   predecessor (range 0.976171–0.989633), now all six proofs checked.
   Metadata 5426 is checked but remains independent/unfuzzed.
2. Revisit active checker outcomes without blocking the investigation; run
   `seal-rephase-gates-5422.py` and `seal-root-followup-gates-5435.py` when complete.
   `seal-rephase-replicas-5421.py` has sealed all nine timing proofs.
   Preserve per-instance work/time/memory and label verification status.
   Any rejected proof/model pauses dependent work under plan §8.2.
3. Preserve submitted sources/pins. Earlier phase-entry source is
   `.claude/worktrees/runtime-inprocess-root-5430`; Release
   `build/pin-5430-inprocess-root-release` and independent
   `build/pin-5432-inprocess-root-bc` share SHA-256
   `44cac3efc60149ab78dca2a5e6b17e552f2f860d4797226d6a0889ae93c1fe26`.
   5430 owns patches/tests; trace pin 5431. Separate metadata source/pins belong
   to 5426 (Release 5426/5427, SHA `bb4e3fbc…`); both descend from Rephase 5417.
   Private maximum-selection source `.claude/worktrees/runtime-vivify-max-5438`
   descends from 5430, excludes metadata, and owns Release 5438/5439 (matching
   SHA `2e316cb494cc10e24…`); its manifest owns full hashes, patch and tests.
   Predecessor `.claude/worktrees/runtime-replacement-cursor-5445` descends
   from 5438; builds 5445/5446 share SHA `b92994ec05c61c31…`. No submitted
   source may be edited. Its 840-case fixture preserves physical/derived state.
   Predecessor `.claude/worktrees/runtime-eliminate-limit-5450` adds only the private
   phase-budget stop and 12-scenario fixture. Release 5450/5452 share SHA
   `c7f57cbcb92f2611…`; trace 5451. Its manifest owns full hashes and patches.
   Claude's terminal fixture review passed: the initial expected elimination
   count was wrong at equality, not a solver defect; original failure retained.
4. Continue periodic Claude-owned fuzzing after meaningful implementation batches,
   in the background as requested. Check session `30094` for its terminal report;
   do not inspect/reinterpret fuzz artifacts. Consolidation still requires all
   runtime/B28+C5/fuzz gates. A reported soundness/crash/proof/model failure
   pauses dependent work and invokes bounded Claude repair immediately.
5. Keep broad cleanup separate. Preserve proof-before-deletion, native physical
   duplicates, N1–N4 and open ledger obligations. Push remains **unapproved**
   after the earlier auto-review rejection; do not retry or bypass it.
   Approval requested for separate future solve/check jobs to release solver
   allocations sooner; no contract/runner change until explicitly approved.

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

- Closed Rank-37's full 100000-conflict decision prefix after the bound repair.
- Checked all nine short-B controls; old/new work, proof and slot traces agree.
- Selected Square's inprocessing difference for a short causal prefix.
- Sealed replacement-cursor timing: six checked proofs, median 0.976662x.
- Newer candidates remain isolated; proof checking and fuzz stay background.

Git and primary reports own history; prior resume text is in `a336e013:PROGRESS.md`.
