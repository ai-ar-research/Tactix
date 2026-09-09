# Added Rank45 localization and exact-work cache gates

## Outcome and decision

The Rank1–50 goal remains unmet; no source is promoted. Latest complete measured
stamp6357 is 8705.330/7239.010 seconds = 1.202558x fixed Kissat. Its added ten
ranks cost 2.856755x; the already-checked matched6108 native additions remain
the fixed reference, not the cpu-q measurements. The new6383 cache full50 run
uses the same 12-logical-CPU/48000MiB allocation shape. Keep every rank scored.

Cache6387 now passes all66 expected B28+C5 solver outcomes. All33 candidate
verdicts, proof bytes/hashes and every non-timing statistic exactly reproduce
6357's6362 gate. At frozen6392,64/66 certificates were checked; later audit
counts supersede only pending status, never the snapshot. Candidate/production
runtime is B0.740452x and C0.932985x, without a solve loss. These broad gate
ratios do not isolate cache benefit.

Proceeding immediately after solver closure, not proof completion:

| Campaign | Job and submission EDT | Purpose |
|---|---|---|
|6405 wave0|1677791,2026-09-09T07:28:15|Six independent cache6383/6357 runs on32/33/35|
|6405 wave1|1677829,2026-09-09T07:39:13|Reversed arms, submitted after six exact-work outcomes|
|6407 full50|1677792,2026-09-09T07:28:15|Tactix-only fixed50 aggregate, original40 and added10 views|
|6415 Rank45 schedule|1677833,2026-09-09T07:40:02|Three read-only actual-sort-input observers|

All are full-width, single bound solver/thread, no hard-node constraint or
throttle. Solver cap1200 seconds excludes decompression/checking/cleanup.
Timing/full runs reserve16000 checker seconds in5h; bounded diagnostics use25min
and delete hashed partial proofs, which are not final-proof acceptance.
Existing production, pins, reference manifests and cpu-q5784 scripts are unchanged.

## Rank45 is a short, adverse added-set witness

Selection is from frozen6371, not a new tier or tuned exception:
`tierD24-26/efc1b7cc3861df8c2b68ca10399b794e-oddball_19_4_ttf.normalised.cnf.xz`, UNSAT.

| Arm | Seconds | Conflicts | Decisions | Peak RSS KiB |
|---|---:|---:|---:|---:|
|stamp6357|71.236|4,435,138|5,893,257|84,496|
|predecessor6305|44.135|2,754,580|3,611,660|73,472|
|fixed Kissat|45.187|2,754,580|3,611,660|31,976|

This is predominantly a changed-work lead, not yet a causal attribution to
one source statement. Propagation currencies differ and memory remains open.

Three-arm100K diagnostic6402/job1677775 independently establishes that all
184,622 predecessor/native five-column decision rows are identical. Stamp6357
matches160,836 rows, then differs at conflict83117, decision counter160451:
current `[83117,160451,56,827,0]`, predecessor/native `[83117,160451,59,827,0]`.
The literal is still827: this is the first differing decision **row/level**,
not the first different selected literal.

Ordinary Tactix inprocessing boundaries agree through index212. First difference
is the end of the second TransitiveReduce at76464:6357 leaves4579 binaries,
where6305/native leave4574. Both start4579. Account for both invocations at that
conflict: all arms first remove32 binaries (4677→4645), then current removes0
while predecessor/native remove5 (4579→4574).

Neutral ALL-source census6410/job1677788 reproduces **every**6402 ordinary trace
and each arm's own stopped-proof bytes/hash. Its validated source/candidate
framing accounts for all37 versus32 deletions and no failed-source units.
The exact five missing physical reductions in both predecessor and native are
`(-5680,-854)`, `(-1411,-1196)`, `(-1281,-1113)`, `(-1275,-1113)`, `(-862,-854)`;
there are no current-only reductions.

Predecessor/native agree on all1470 source records (source/count/outdegree) and
3949 candidate records (source/destination/ordinal/redundant/failed). Their
logger tick placement differs; no raw tick identity is claimed. Current's first
source difference follows730 common source records: after479/−479, it processes
1523/−1523 rather than91/−91. Global source sequence is not a phase ordinal.
An active source does not imply its comparator-key variable is active: this
comparator deliberately follows the reference's second IDX mapping.

Next,6415 records actual before/after full schedule membership, external and
compact identities, second-IDX keys, score bits, stamps and priorities for both
76464 invocations. Fresh6413 adds only the existing6350 read-only adapter to6357;
predecessor/native reuse6350. Release is byte-identical6357:
`00a23b9c8f5ec050cb0a1eb05a903c44496b8fd40c650e62eae7ae182a78a695`.
New trace pin is `b1dec5273b056251120782c551df58d6ccdd45a933099691e5de757bbaa49d1b`.
Require all6410 ordinary+census traces and each own partial proof exact before
interpreting the additional snapshots. Do not revert stamp retention blindly:
it closed Rank32's known ordering loss. No comparator, budget, guard, proof,
API, kernel primitive or production source changed in this diagnostic.

## Correctness and test consideration

- New6413 full106/106 Release and105/105 trace tests pass. Actual shared C/C++
  writer filter/framing/tie/repeated-snapshot outputs are byte-identical.
  Existing seven scheduling-parser tests cover the unchanged observer schema;
  no new parser behavior warrants duplicate tests. Whole-parent replay tests
  actual instrumentation neutrality.
- Maintained `validate_stream_accounting` and two new regression tests correct
  a real diagnostic assumption. Observer `start_decisions` is derived from
  final solver counter minus cumulative observed rows modulo2^64. Preprocessing
  resets that counter while the observer retains rows; it is not the actual
  first decision counter. Rank45's derived start is−386 modulo2^64, but first
  observed counter is1. Validator6404 failed this assumption and remains frozen.
  Successor6409 checks real stream edges/count/literals/boundary and the actual
  derived formula. No proof checker or verdict criterion was weakened.
- All65 benchmark-tool tests pass, including the new positive reset shape and
  negative edge/count/width controls. Audit6420 owns SLURM/docs aggregate tests.
- Rank35 extension6389/job1677718 is complete: inherited40,636,523 rows/hash
  `6a3ebeb3a46d7ad5`, zero overlap, exact counter adjacency,58,560,855 equal suffix
  rows. Total99,197,378 decision rows match through1M. This is not full-formula,
  whole-solve or final-proof identity; its later adverse split remains unknown.
- Current-source fuzz is still unavailable and Claude-owned. No fuzz artifacts
  were inspected, no denied transfer/push retried, and no promotion inferred.
  Rank50's material tracker and private FlatLists design still lack approval.

No solver/build/proof/model failure occurred. Early wave1 submission was refused
before submission because its preceding solver wave was incomplete; the single
successful successor submission is listed above. No measurement was discarded.
A first apply_patch context mismatch changed nothing. No manual deletion was
performed. Terminal bounded runners own partial-proof/input cleanup and retain
hashes; live checker scratch is untouched.

## Authorities and exact continuation

Frozen primary results:

- `benchmarks/runs/20260909-6387-vivify-count-cache-BC-gate/solver-checkpoint-6392.json`
- `benchmarks/runs/20260909-6389-rank35-stamp-extension/summary.json`
- `benchmarks/runs/20260909-6402-rank45-stamp-prefix/summary.json`
- `benchmarks/runs/20260909-6410-rank45-transitive-census/summary.json`
- `benchmarks/runs/20260909-6413-rank45-schedule-build/manifest.json`
- `benchmarks/reports/2026-09-09-rank45-cache-audit-6420.json` owns later status,
  integrity, tests, exact accounting, local-only artifact hashes and cleanup.

Run6417 once all three6415 results exist; validate every replay before comparing
recorded keys. Localize the producer of differing key state before proposing a
general repair. Keep Rank32 and all50 regressions visible. For6405, submit wave2
only after12 expected exact-work outcomes;6419 requires all18 and both arm
spreads≤2% before absolute timing claims. Run6418 after all50 outcomes, requiring
all50 cache certificates/non-timing statistics exact6357. Pending proof checks
never block these steps but do block checked acceptance. Do not rerun successful
artifact-writing helpers. Andrew's local report is under
`report-to-andrew/20260909-0745-rank45-cache/`; it is not externally sent.
