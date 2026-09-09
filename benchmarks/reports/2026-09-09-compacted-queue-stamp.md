# Compacted queue stamps: source-order cause and isolated repair

## Outcome and decision

Rank 32's missing transitive source is now explained by an actual ordering-key
discrepancy, not by inactive endpoints, source membership, priority flags, or
the sorting algorithm. Isolated6357 preserves detached queue records across
physical compaction. Production remains unchanged and the candidate is unfuzzed.
Runtime benefit and the full causal replay remain open until measured.

This is a general private-guidance repair: no benchmark selector, added kernel
primitive, guard, comparator, budget, public action, proof delta or model-map
change. B/C, full fixed50, certificates, memory and repeated causal timing remain
independent gates. The approved matched Rank41–50 reference6108 is already
complete (20/20 checked); it is included in the fixed50 authority, not rerun.

## Actual diagnostic evidence

Campaign `benchmarks/runs/20260909-6352-rank32-transitive-schedule`, job1677576
submitted2026-09-09T05:46:18, completed both arms on the normal12CPU/48000MiB
allocation. Validator6356 freezes `summary.json`. Every ordinary and selected
trace and partial-proof byte/hash matches6345 per arm through the3M stop;
runner-owned partial proof/input cleanup is complete. These are stopped partial
proofs, not accepted final certificates.

The new parser validates four snapshots per arm: before/after sorting for each
of two calls at conflict1368343, decisions105388247, focused mode. It validates
framing, ordinals, types, source uniqueness, ascending compact-index input and
complete before/after multiset equality. Both actual outputs are correctly
sorted under their own recorded keys. Seven new parser tests pass (63 total
benchmark tests at this increment).

Both calls have identical source membership, active/value state, compact and
external identities, key identities, score bits, and priority flags (all zero).
The sole input difference is one underlying stamp, repeated on two source rows:

| Sources (external) | Key compact/external | Tactix stamp | Kissat stamp |
|---|---|---:|---:|
|933,936|72/144|0|229273|

The native comparator intentionally applies IDX again to a variable index;
Tactix's existing second-index bridge is faithful. Do not change it. Call0 has
64833 sources, call1 has64857. Source936's zero-based reverse variable position
is64831 vs19097 in call0 and64856 vs1099 in call1. This explains how an active,
unassigned source can miss Tactix's budgeted prefix. It does not yet prove that
the repair closes the later selected-decision split or improves full runtime.

## Producer audit and repair

Native authority: `tools/solvers/variants/kissat-macro-heap-6090/src/compact.c`.
`kissat_compact_literals` aliases roots, but `kissat_finalize_compacting`
clears later fixed exports **before** `compact_queue`. The queue copier thus
preserves the first fixed representative's own stamp, not the last alias's.
An initial reading of the queue copier alone suggested last-alias ownership;
the full call sequence disproved that before any implementation of alias transfer.

Tactix's `heuristics_compact_vars` rebuilt its live queue after zeroing every
link record. The retained fixed key lost its stamp. Isolated6357 uses the
existing generic map-and-copy helper for link records, then rebuilds and
restamps the same live queue and maps the same cursor as before. Explicitly
detached records remain detached. Stable scores and physical/public identities
are not aliased or reassigned. There is an extra temporary link-vector allocation
in compaction; full-view peak RSS must be checked, not assumed unchanged.

CaDiCaL's `compact.cpp` and `queue.cpp` were also inspected: its mapper retains
the first fixed index, filters the active queue and separately maps ranking
arrays. Its representation/cursor conventions differ; Kissat remains the
tie-break for this policy's actual transitive-key consumer.

Build authority: `benchmarks/runs/20260909-6357-compact-stamp-build/manifest.json`.
Incremental and cumulative zero-context patches are frozen there. Only
`src/policies/heuristics.cpp` and the new `tests/test_compact_stamp.cpp` differ
from6305. Submitted sources, production and the independent cpu-q5784 campaign
are untouched.

## Correctness controls and classified failures

- Two independent Release builds pass106/106 tests; trace passes105/105.
  Release/BC hash: `00a23b9c8f5ec050cb0a1eb05a903c44496b8fd40c650e62eae7ae182a78a695`.
  Trace: `0c774270938d21563fee79df709d38e06d89c67d29316925ab4d89479c6fe1b5`.
- The new fixed fixture has24 cases/144 stamp observations: both import orders,
  both ranking modes, identity/dead-variable remapping, three stamps including
  UINT32_MAX, repeated compaction, live queue/cursor, public literal identity
  and two-phase Decide surface, plus null-guidance controls.
- Frozen-parent control6359 hit the fixture's detached-membership assertion:
  zeroed detached index0 can look self-connected. This is a causal predecessor
  state defect, not a new unexplained candidate crash. Recovery6361 records that
  membership instead of requiring old code to pass; all other assertions remain.
  Actual parent misses144/144 stamps and has36 detached-membership failures.
  The unmodified candidate fixture passes with neither problem.
- Native control6360 initially failed compilation because IDX's assertion macro
  requires a pointer named `solver`. Recovery6361 adds that local alias only;
  the actual native collector then passes12/12 cases, preserving the first stamp
  despite a distinct later fixed stamp. Both failures and recovery evidence stay
  in their original directories; no failed artifact is overwritten.
- Gate-checkpoint helper6364 initially requested a binary-hash field absent from
  the frozen runner's provisional schema. It failed before writing a checkpoint.
  It now validates immutable pin hashes and matrix identities for all records,
  and final-record binary hashes when present; certificate classification still
  uses the maintained fail-closed parser. No solver result was excluded.

## Next actions and limits

B28+C5 campaign6362/job1677578 since2026-09-09T06:10:42 is independent of all
older campaigns. Proceed after66 expected solver results, treating pending checks
as provisional only. Then submit6365 Rank32's3M candidate-only causal replay
against cached native6303, and6366 full fixed Rank1–50 pricing. First validate
the inherited1M decision hash; if it changes, localize earlier instead of
claiming closure from an invalid inherited prefix. Compare all50 work and RSS
with6305 and the fixed reference, then repeat decision-changing timing pairs.

Claude-owned latest-source fuzz is still unavailable; no fuzz job, log or
artifact was launched or inspected. No denied transfer or push was retried.
Pending memory-storage and added-subsumption-material designs remain unapproved
and unimplemented. This repair needs neither design. No promotion or aggregate
competitiveness claim is made.
