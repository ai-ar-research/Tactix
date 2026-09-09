# Native activation stamps: Rank45 producer and isolated repair

## Outcome

Goal remains unmet; production stays c2a95a6 and nothing is promoted.
Fresh accepted reevaluation6227 remains100/100 checked and clean,1.393718345x.
Its per-rank table and zero-residue authority were rechecked, not rerun.

Cache6383 full6407 now has all50 expected outcomes,25SAT/25UNSAT. All50 proof
hashes and every non-timing statistic exactly match6357. Frozen6418 has39/50
checked; all50 fixed-reference certificates are checked. No solve loss,
timeout transition, invalidity or excluded measurement appears in this view.

| Scoring view | Cache seconds | Parent6357 seconds | Fixed Kissat seconds | Cache/Kissat |
|---|---:|---:|---:|---:|
|All50|8660.228|8705.330|7239.010|1.196327675|
|Original40|7437.009|7479.942|6810.066|1.092061222|
|Added10|1223.219|1225.388|428.944|2.851698590|

The45.102-second cross-window reduction is not yet an isolated cache speedup.
Three matched paired waves remain the attribution protocol.6405 waves0/1 have
12 expected exact-work/proof outcomes; final wave2/job1677917 was submitted
2026-09-09T08:04:55 after those outcomes, without waiting for certificates.
Do not selectively repeat noisy arms.6419 will assess all18, paired ratios and
each arm's predeclared2% range/median condition.

Full per-rank/runtime/work/RSS table:
`benchmarks/reports/2026-09-09-count-cache-full50-6418/comparison.tsv`.
Largest remaining gaps are Rank50 SAT376.492s,31 UNSAT210.274s,35 UNSAT192.033s,
21 SAT150.934s and49 SAT136.372s. Added10 contribute794.275 of1421.218 net seconds.
All ranks and memory remain scored; the matched6108 added reference is unchanged.

## Actual Rank45 scheduling input identifies the producer

6415/job1677833 is complete. Each of three arms reproduces **every**6410
ordinary+census trace and its own partial proof bytes/hash. All four before/after
snapshots per arm have valid frames and unchanged membership/key multisets.
Every output is sorted under its recorded actual keys. Both calls are at76464,
with1175 then1187 sources. Current6357 and native agree on all input fields
except one stamp, in both calls:

| Source | Source compact | Key compact/external | Current stamp | Native stamp |
|---|---:|---|---:|---:|
|1523|1|0 /1|5323|1|

In the second call, this moves1523 ahead of91 after two common reverse variable
positions. The neutral census already established current removes0 binaries
versus native/predecessor5 there. The first call's altered tie permutations
still remove32 in every arm. Predecessor6305's source order equals native in
both calls despite differing numeric stamps; equivalent order is not key-state
identity. This rules out sort malfunction or differing membership/score/priority
as the observed input discrepancy. It does not yet prove a repaired whole solve.

The frozen input begins `p cnf 5322 105988`, then `-1 0`. Native activates that
first variable once, assigning stamp1. Tactix allocates its5322-variable queue,
then seeds activation order by bumping the existing queue: first stamp becomes
5322+1. Later deactivation leaves that value on the fixed identity.6357 correctly
preserves it across compaction, exposing the older initialization error that
6305's destructive reset had hidden. Do not revert the Rank32 stamp-retention
repair or replace native's second-IDX comparator mapping.

Actual fixed control6423 reproduces16/16 wrong retained-key observations using
both parser/heuristics attachment paths, both import directions and headers6/9.
Examples: forward fixed variable2 is8 or11 instead of native2. Native control
uses the real flags/queue/collector, not a Python model. Its actual source
`flags.c::activate_literal` enqueues once; `inlinequeue.h` owns the increment.
CaDiCaL `queue.cpp::init_enqueue` likewise distinguishes initialization from a
search bump; Kissat remains the exact numbering tie-break.

## Isolated6424 repair and validation

Source `.claude/worktrees/runtime-activation-stamp-6424` is a fresh6383 clone.
Only `src/policies/heuristics.cpp` and new `tests/test_activation_stamp.cpp`
differ. After existing queue-order seeding, clear provisional numeric stamps,
assign1..activation-count in first-activation order, set queue counter and
refresh the existing search cursor's numeric stamp. Queue order, compact import
image, scores, phases, root deactivation and later stamp-retaining compaction
are unchanged. Header-only identities do not consume native activation numbers.
No new allocation, persistent field, benchmark-dependent setting or constant.

This is policy metadata, not a formula/proof rule. M2's any-unassigned-literal
and both-phase surface, guard and proof delta remain unchanged. No new kernel
primitive, API, rule instance filtering, proof vocabulary, effort budget or
formula-dependent selector. Calculus/guard documentation therefore needs no
semantic edit; this report owns the isolated, unpromoted engineering change.

Authority: `benchmarks/runs/20260909-6424-activation-stamp-build/manifest.json`.
Release and independentBC SHA256:
`5946f59800adbf1bbbb20c64ac4bcbcd00ff8dc980ebfdc04edd844db2cd7eb4`.
Trace SHA256:
`c5ca763ce93f48507648c32cb55027274a79332ef56147814bb917d6dddc9e8c`.

- Full108/108 Release tests in each of two builds and107/107 trace tests pass.
- New16-case fixture checks64 observations across parser attachment, header
  holes, import direction, stable/focused modes and repeated compaction. It
  checks root values, fixed exclusion from guidance, exact live queue order,
  counter/cursor consistency, invariants and every public Decide phase,
  including any header-only unassigned identity.
- Actual parent6383 fails all64 stamp observations without crashing; new
  source matches16/16 frozen native stamp and queue-counter observations
  (control6426). No mirrored implementation is used.
- Supplemental6430 specifically covers the observed units-before-clause shape
  after the pins froze: parent misses16/16, candidate matches16/16; actual native
  confirms stamp1 in all4 cases. No frozen test/source/pin was changed.
- Existing benchmark/slurm/docs tests cover unchanged runner/parser protocols;
  audit6431 records their results. No new proof or model acceptance is inferred
  from stamp comparisons. Broad certificates and latest-source fuzz remain gates.

The unchanged B28+C5 composition is queued/running as6427/job1677854, submitted
2026-09-09T07:57:51. At08:04:30 it had54 expected solver records;6429 correctly
refused a partial checkpoint. Checkers run asynchronously. All new real runs
use12logicalCPUs/48000MiB, one bound solver/thread,1200 solver-only seconds,
16000 checker seconds in5h, no hard node or throttle. Source6424 has no Tier-D
runtime claim before that solver gate and causal replay.

## Failure classification, cleanup and next steps

6422's native micro-fixture failed compilation because IDX's assertion needs
a variable named `solver`, while the fixture used `s`. Original source/log are
retained; corrected6423 compiles and passes. This is a test-adapter compiler
failure, not a solver failure. The initial watcher command missed `--job`;
corrected watcher observed the same existing1677833 through COMPLETED. No
solver/model/proof failure, invalid score exclusion, job resubmission or pin
mutation occurred. No manual deletion or live checker-scratch inspection.
Completed bounded runners hash/delete their own partial proof/input; those are
not final-proof acceptance. Fixed fixture temporary streams close normally.

Next6429 must validate all66 B/C expected outcomes. Then6433/6434 submit only
the repaired Rank45 trace against cached native6402;6435 compares every100K row
and reports both76464 reduction counts without assuming closure. Next preserve
Rank32's established3M/153176971-row closure before broader6424 scoring and
replicated decision-changing timings. No proof-check wait before development.
6415 is done; never rerun6417 or successful6418.6405 finalwave remains live.

Current-source fuzz is unavailable and Claude-owned; no fuzz launch/artifact
inspection or denied transfer/push retry occurred. No promotion. Rank50 material
tracker and private FlatLists redesign still need approval; memory remains open.
All production/baseline/cpu-q5784 authorities are unchanged. Living docs stay
within required caps; no additional compaction was performed.

Primary controls are6423/6426/6430 summaries and6415 summary; final checked
counts, exact accounting, source/protected hashes, cleanup and owned landing
are in `benchmarks/reports/2026-09-09-activation-stamp-audit-6431.json`.
Andrew's local TeX/PDF is under `report-to-andrew/20260909-0800-activation-stamp/`;
it is not externally sent.
