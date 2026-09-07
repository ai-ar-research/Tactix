# Dense-entry ownership and clean Release cost

The coupled physical-mode repair removes a measured redundant watch image,
but does not yet establish a runtime win. Production remains unchanged at
**1.2787518x Kissat** on checked Rank 1–40; the **≤1.01x goal is not met**.
The isolated cumulative stack remains UNFUZZED and unpromoted. This report
extends the [runtime review](2026-09-07-runtime-review.md) and
[Vivify repair](2026-09-07-vivify-retry-repair.md), without revising their frozen
observations. Retain supported general fixes, not unsupported cost ceilings or
trajectory identity as an end in itself.

## Correct the stopped-build cost inference

5384 profiles the exact stopped executable used in 5382, with semantic event
tracing disabled. It nevertheless contains dormant hot-loop diagnostics under
`TACTIX_ENABLE_CONFLICT_STOP`: `kernel_pose_trace` alone accounts for **1.31%**
of self samples, and other dormant checks are inlined into propagation.
Thus 5382's 1.13834x time and excess instructions do **not** isolate clean
production implementation overhead. Do not simply subtract 1.31%: sampled
cycles, inlined checks, and runtime are different quantities.

The 5384 stopped arms reproduce their own earlier conflict/decision/propagation
counts; both stop UNKNOWN at 1,000,002 conflicts and 1,924,776 decisions. They
are diagnostics, not checked verdicts or acceptance measurements. The prior
exact trajectory trace covers one million conflicts, not the last two.

5385 instead profiles the immutable clean Release candidate from 5379 and
pristine Kissat on full SAT Ranks 17/21/34. All six models check against the
original CNFs, and every arm exactly reproduces its own unprofiled 5379 work
counters. No pose helper appears. `perf record` uses user cycles, 199 Hz and
DWARF call stacks; no lost samples are reported. Raw compressed perf streams,
self/call-path reports, executed sampled commands and hashes are retained.
The actual sampled command is `solve.profile.command`; the wrapper's inherited
top-level command field describes the base runner, not this instrumentation.

| Rank | Tactix top self sample | Kissat top self sample | Interpretation |
|---|---|---|---|
| 17 | propagation 35.96% | search propagation 49.92% | Tactix also pays substantial preprocessing/maintenance. |
| 21 | propagation 20.45% | search propagation 23.63% | Tactix inclusive BVE is 25.48%; investigate memory ownership. |
| 34 | propagation 67.65% | search propagation 69.48% | Common hot path, but full trajectories are not identical. |

These percentages identify hot paths, not cross-solver cost fractions: routine
boundaries differ and the full solves perform different work. Profiled wall
times are not substituted for unprofiled checked-gate results.

## Memory cause and two coupled repairs

5386 observes the existing trace build with only slot-memory output selected,
no actual stop. Its checked Rank-21 SAT solve reproduces clean Release work
counters. At BVE end its peak RSS is 4,713,316 KiB and current RSS 3,085,272
KiB. The watch arena has **222,311,689 words / 268,435,455 capacity**, alongside
1,204,663,832 bytes of dense capacity and 133,208,352 reconstruction words of
capacity. This is a live BVE ownership issue, not merely allocator retention.
The observer inventories major owned pools, not every local allocation.

1. **5387, publication:** `kernel_add_large_clause` now adds sparse watches
   only when `large_watches_connected` is true. Replacements already honor
   that flag; the existing invariant forbids large watches while disconnected.
   Dense occurrences, proof ids, arena placement, marks and cursors are
   unchanged. Kissat `clause.c::new_large_clause` selects watch/reference
   publication using physical watching mode; CaDiCaL also separates occurrence
   publication from sparse watch attachment.
2. **5391, Eliminate entry:** DefaultPolicy replaces deduplicate-only entry
   with existing `kernel_flush_large_watches`, which both disconnects large
   watches and deduplicates binaries. Occurrence connection follows; the
   existing phase exit disconnects dense indexes and rebuilds sparse watches.
   Kissat `eliminate.c` enters dense mode before elimination and resumes sparse
   mode afterward; CaDiCaL `elim.cpp` resets watches before elimination rounds
   specifically to release this ownership. Publication repair is needed so
   resolvents cannot reintroduce the disconnected image.

No new kernel primitive, applicability guard, proof vocabulary, reconstruction
scheme, budget, tuned constant or benchmark selector is added. This is an
existing physical invariant and pipeline boundary, not an abstract calculus
change. Public N1 calls remain available. A dense phase can change root
propagation/reconnection pose, so unchanged full search work is **not** assumed.

Publication alone is neutral: 5388 has 12 checked models, no timeout, and
640.076/641.672 = **0.997513x** preceding-candidate time (one wave).
5390 reproduces the same 362 memory rows and same BVE watch/capacity peak as
5386. That negative result exposed the missing entry transition; publication
alone is retained as an invariant repair, not claimed as a speedup.

With both changes, 5394 has a checked model, clean scratch and 366 memory rows,
matching its own clean 5392 Release work counters exactly. At BVE end:

| Owned metric | Preceding 5386 | Coupled 5394 |
|---|---:|---:|
| Peak RSS, KiB | 4,713,316 | 3,935,732 |
| Current RSS, KiB | 3,085,272 | 2,274,204 |
| Watch words | 222,311,689 | 7,856,864 |
| Watch capacity, words | 268,435,455 | 8,388,607 |
| Dense capacity, bytes | 1,204,663,832 | 1,204,663,832 |
| Reconstruction capacity, words | 133,208,352 | 133,208,352 |

This supports removal of the redundant watch image, not complete memory
competitiveness. Dense and reconstruction storage remain large; the new
Release peak is 3,935,936 KiB versus Kissat's 1,436,392 KiB. The trajectories
also change, so the runtime delta is not a pure allocation-cost experiment.
One-sided BVE reconstruction is a possible separate semantic project, not part
of this repair; it would require a fresh soundness/API review and gates.

## Tests, failure retention and provenance

5387's actual-object old-negative/new-positive fixture publishes four/zero
sparse words while both publish three dense occurrences. The actual native
entry/add fixture publishes three dense occurrences with watching false.
Fixed tests cover sparse/dense, redundant/irredundant, binary preservation,
delete/reconnect and root propagation. Trace CTests pass 76/76 and fresh
Release 77/77. An initial native fixture missing-header compile failure was
corrected before execution; it is described in the manifest.

5391's first scheduling fixture failed an assertion because it posed a
non-preprocessing slot at zero conflicts and never reached the observer.
Development paused and a bounded Claude repair was invoked under plan §8.2.
Claude's terminal report classified **fixture wiring only, no solver defect**;
it changed only the fixture/test, adding one conflict, a non-null zero meter,
zero retry advice and non-vacuous admission/entry/restoration assertions.
No implementation source was changed by that repair, and no fuzz campaign
was launched or inspected. Initial failed logs and the terminal repair report
are retained in 5391.

The corrected old/new fixture observes connected/four versus disconnected/zero
large words at actual scheduler entry. Both restore four large and two binary
words at exit, with no elimination. Actual native dense entry is disconnected.
Full serial reruns pass **76/76 trace**, **77/77 fresh Release**, and **77/77
independent B/C Release**, in 20.79, 17.44 and 17.30 seconds. Candidate-specific
rule/policy checks pass against isolated sources, not symlinked main-tree tools.

Immutable source/build authorities:

- 5387: `.claude/worktrees/runtime-dense-publication-5387`; Release pins
  5388/5389 both SHA-256
  `8153c20f308ef3ddcf9c9d2446c1dcfe16e4f25ec03c7a1748944cd6a042150b`.
- 5391: `.claude/worktrees/runtime-eliminate-dense-entry-5391`; Release
  `build/pin-5392-eliminate-dense-release` and independent
  `build/pin-5393-eliminate-dense-bc` both SHA-256
  `5236e63f5ba3fc27e448d23c2a4b92e1948bb9c1390f336cc0852eed66cf64e2`.
- Their source deltas, actual-object fixture provenance, build/test/repair logs
  and hashes live in the respective 5387/5391 manifests. Runtime pins retain
  cumulative patches relative to unchanged production `c2a95a6`.

## Gate snapshot and next decision

| Campaign | SLURM job | Scope |
|---|---|---|
| 5384 | 1669085 | Stopped-pin sampled cost, complete diagnostic. |
| 5385 | 1669087 | Clean Release sampled full solves, 6/6 checked/clean. |
| 5386 | 1669093 | Preceding memory ownership, checked/clean. |
| 5388 | 1669094 | Publication-only runtime, 12/12 checked/clean. |
| 5389 | 1669095 | Publication-only B28+C5, proof checks pending. |
| 5390 | 1669107 | Publication-only memory, checked/clean. |
| 5392 | 1669173 | Coupled four-arm runtime screen, 12/12 checked/clean. |
| 5393 | 1669174 | Coupled B28+C5, proof checks pending. |
| 5394 | 1669214 | Coupled memory ownership, checked/clean. |
| 5395 | 1669252 | Coupled full fixed Rank 1–40, newly running. |

The complete 5392 screen has 12/12 checked models, no timeout, and clean
scratch. Candidate/preceding is 652.579/639.625 = **1.020252x**,
candidate/production **0.708396x**, and candidate/Kissat **1.205145x**.
The one-wave candidate/preceding/Kissat witness rows are:

| Rank | Preceding seconds | Coupled seconds | Kissat seconds |
|---|---:|---:|---:|
| 17 | 151.710 | 171.856 | 113.550 |
| 21 | 137.288 | 130.030 | 96.521 |
| 34 | 350.627 | 350.693 | 331.423 |

Rank 17 worsens and Rank 21 improves; Rank 34 work counters are unchanged.
Rank 17 grows from 165,557 to 179,361 conflicts and from 1,016,171,447 to
1,158,167,288 propagations. Rank 21 changes from 97,517/60,978,863/840,322,789
conflicts/decisions/propagations to 81,990/66,644,458/763,244,718. These are
mixed effects, not a global win or grounds to revert a sound invariant solely
to recover one trajectory. Finish the complete checked full-view comparisons
before choosing a consolidation stack.

Preceding Vivify B28+C5 **5380 / 1668924** is now terminal: all **66/66
checked/clean**, no timeouts or solve losses; candidate/production total
1064.174/1115.725 = **0.953796x**. Full Rank 1–40 **5383 / 1669005** still has
proof checks pending. Neither unfinished full-view campaign may establish
acceptance. 5395 preserves the exact 5314/5383 matrix and pristine Kissat.
All normal arrays are full-width, 12 logical CPUs/48000 MiB, one bound solver,
no throttle/node constraint; proof/model checking and cleanup are mandatory.

Next: finish exact jobs and retained sealers, rank full-view runtime/memory
outliers, isolate any weaker work from this boundary before more cost changes,
and consolidate only supported repairs. One Claude-owned fuzz campaign remains
required before promotion. No new fuzz authority is inferred from fixed tests.

## Checkpoint hygiene

The [artifact index](2026-09-07-dense-entry-artifact-index.json) owns the
timestamped campaign/pin/parent/residue inventory. Large binaries/perf streams
remain local and hashed; small patches, runners, reports and evidence are
durable. Active checker scratch remains job-owned. No manual deletion is
justified or performed. `compact-docs` routes detailed history here and leaves
PROGRESS as the current resume surface; no frozen prior artifact is rewritten.
N1–N4 and open production deviations remain intact. Push remains unapproved
after the prior auto-review rejection and is not retried.

Final main-tree validation passes 77/77 CTests in 20.40 seconds. All 30 size
budgets pass (four near-cap warnings); PROGRESS is 116 lines. Option coverage
accounts for all 418 reference options, all 42 section references resolve,
and rule/policy boundaries pass. The progress checker has zero errors and two
historical warnings; its escalated read-only run observes the actual queue.
The [validation record](2026-09-07-dense-entry-validation.json) retains the
main CTest log and hashes of Andrew's three-page TeX/PDF, compiled twice from
`report-to-andrew/20260907-1813-dense-entry/` with no final overfull boxes.
Editable docs/helpers pass diff whitespace checks. Raw native profile padding,
CRLF matrices and patch context retain their exact hashed bytes; their format
warnings are not normalized by editing immutable evidence.
