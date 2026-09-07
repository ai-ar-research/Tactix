# Progress

The plan owns scope and gates; calculus owns rule semantics; implementation
notes own guards and representation; the parity ledger owns N4. Frozen reports
and run records own experiment detail.

## Current resume anchor

- Continue general Tactix implementation repairs toward **Rank 1–40 aggregate
  runtime ≤1.01x Kissat**, with B28+C5 regression gates and N1–N4 intact.
  The goal is **not met**: final production campaign 5314 is **1.2787518x**.
- Read [the runtime review](benchmarks/reports/2026-09-07-runtime-review.md)
  for the audit of Claude's prior work and retained/rejected approaches.
  Its correction stands: campaign 5291's mixed proof modes and unchecked,
  serialized arms cannot support “93% search quality” or a C++ cost ceiling.
- Latest evidence: [BVE choice frontier](benchmarks/reports/2026-09-07-bve-choice-frontier.md).
  The isolated cumulative candidate matches 254,416 decisions from startup;
  its next split is explained downstream by unequal elimination relevance
  values already present before BVE. The stamp producer is not yet localized.
- Production source remains `c2a95a6`; live Release SHA-256
  `de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f`.
  Retain accepted binary substitution, conservative liveness filtering,
  arena/watch lifecycles, connected duplicate-pose repair and dense-image fixes.

## Active jobs and gates

**NOTHING IN FLIGHT.** All normal campaigns are terminal. User interactive
allocation `1668242` (`bash`) is untouched.

| Authority | Terminal evidence |
|---|---|
| [5295 starting-source audit](benchmarks/runs/20260907-5295-checked-runtime-audit/summary.json) | 80/80 independently checked/clean; starting Tactix/Kissat 1.2192x. |
| [5314 final production Rank 1–40](benchmarks/runs/20260907-5314-pose-r2-ranks1-40/summary.json) | 80/80 checked/clean, both 40/40 solves (22 SAT/18 UNSAT); 8643.178/6759.074 s = 1.2787518x. |
| [5315 final production B28+C5](benchmarks/runs/20260907-5315-pose-r2-BC-gate/summary.json) | 66/66 checked/clean; final-r2/starting aggregate time 1.01897x. |
| [5351 latest stopped diagnostic](benchmarks/runs/20260907-5351-bve-schedule/summary.json) | Jobs 1668712/14/16/18: eight zero-exit stopped tasks, all scratch clean; no new full verdict or timing gate. |

Full Rank 1–40 is not an annual/union Tier-D replacement. Memory
competitiveness remains an independent open acceptance dimension.

## Exact next action

1. Trace **numeric VMTF stamp publication/compaction** before conflict 95,799
   on Rank 34, using the immutable 5348 observer pins retained by 5351.
   At first BVE schedule, all 13,003 candidate identities, occurrence counts,
   insertion order and inferred relevance order match; every numeric score and
   262 physical heap positions differ. The tenth candidate is Tactix 4387
   versus Kissat 16887 after nine exact candidates/outcomes/tick deltas.
   Capture queue order, stamps, mode and cursor at startup, admitted Reduce
   compaction, and restamping boundaries. Compare the earliest producer before
   changing BVE; a stamp reset must preserve the reference cursor lifecycle.
2. Preserve the isolated cumulative baseline while tracing. It already closes
   original-core, owner-562 binary publication and cross-round retirement
   witnesses. No literal, conflict, formula, rank or identity gate is authorized.
   Fresh semantic repairs require reference-source review, a failing/passing
   differential fixture, immutable pins and focused correctness checks.
3. At consolidation, compose only supported general repairs, run checked
   B28+C5 and the required runtime gates, then invoke one **Claude-owned** fuzz
   campaign under plan §8.2. No isolated candidate below is promoted or fuzzed.
   A reproduced soundness/crash/proof/model failure triggers bounded Claude
   repair immediately; Codex owns ordinary performance/fidelity debugging.
4. Keep codebase cleanup separate from causal repairs. Residual matched-work
   cost and memory, especially Rank 31/17 mechanisms, remain open; profiles and
   equal-counter screens are leads, not causal attribution.

## Isolated candidate authorities

- [Original-core convergence](benchmarks/reports/2026-09-07-original-core-convergence.md)
  owns 5316–5336: separate clausal core from LRAT proof steps; original-unit
  save-time eligibility; sequential certified rewrites and binary publication.
  Emit captured pair proofs before source-deleting rewrites.
- [Reference duplicates and retirement](benchmarks/reports/2026-09-07-reference-duplicates-and-retirement.md)
  owns 5337–5347. Native Kissat also retains the observed duplicate copies:
  deleting them is not a parity repair. Removing the obsolete cross-round
  retirement sort closes all 1,488 heap removals and 13,292 warm-up records;
  fixed 5345 pins the transition. `SP-SUBSTITUTE-ROUND-1` stays open in production.
- 5344's 15 checked models show mixed downstream work and no promotion:
  current/retirement-only/bridge/bridge+retirement/Kissat three-witness totals
  are 920.089/1041.107/1150.392/1017.036/541.763 s. This is not decision-grade
  three-wave or whole-suite evidence.
- [BVE choice frontier](benchmarks/reports/2026-09-07-bve-choice-frontier.md)
  owns 5348–5351. At conflict 95,801 Tactix leaves 5416 unassigned but outside
  its decision heap; BVE retired it at 95,799. Matching EVSIDS score 9.25 does
  not repair that formula-state difference. Earlier producer: elimination
  relevance, not preceding propagation or heap tie handling.
- Raw large diagnostic streams remain local with hashes in retained summaries;
  manifests, source patches, runners, comparators and small witnesses are durable.
  Legacy decision-prefix `start_decisions` wraps with warm-up counts; never
  use it for alignment.

## Validation and immutable authorities

- Production Release passes 77/77 CTests. Claude's authoritative
  [r2 fuzz report](benchmarks/reports/2026-09-07-sweep-pose-fuzz.md) is PASS;
  do not independently inspect/reinterpret its artifacts. Regressions 028–030
  remain retained. No new fuzz campaign was launched for read-only observers.
- N4 verification remains pending; compaction accepts no new deviation.
- Historical accepted source `bad5cbf7548c6abf1181d36150206ac6f52c316f` and
  `20260829-1930-tierd-ranks1-40-accepted-fresh` are historical authority,
  not a same-window baseline for today's source.
- Frozen competition reference: `benchmarks/baselines/sc24-26-32g-5000s/`.
  T1/T2 and historical T3 are passed; T4-K, T4-VBS and subsequent T5 remain open.
- Push is **not authorized**: auto-review previously required explicit approval
  to publish to shared `origin/main`. Keep local commits; do not retry/bypass.

## Architectural contract

<!-- track-sched:begin -->
- Kissat creation order is the unconditional production default;
  `TACTIX_ARENA_ORDER=1` remains an explicit CaDiCaL diagnostic arm only.
- Unsupported formula-dependent profiles stay removed. Preserve sound general
  repairs through downstream regression; do not restore compensating behavior.
- Preserve N1 universal API capability, N2 proof/model accountability,
  N3 runtime/memory competitiveness and N4 reference-strength scheduling.
<!-- track-sched:end -->

## Recent changes

- Final production gate: 80/80 checked, 1.2788x Kissat; B28+C5 66/66 checked.
- Isolated per-round retirement closes the known warm-up heap witness.
- Lightweight observers move the frontier from decisions to BVE entry relevance.
- Four diagnostic arrays finish clean; production is unchanged.
- Explicitly requested compaction replaces campaign chronology with evidence links.

## Historical evidence routing

The [runtime review](benchmarks/reports/2026-09-07-runtime-review.md) owns the
prior-work audit; the [unit-cost/fidelity report](benchmarks/reports/2026-09-02-unit-cost-and-fidelity-stack.md)
owns older experiments. Pre-compaction PROGRESS is recoverable with
`git show 6e2d609:PROGRESS.md`; dated statuses there are historical.
