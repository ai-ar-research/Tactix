# Corrected root lifecycle separates search drift from storage cost

Frozen September 11, 2026, with the first Rank25 pair available. Audit7383 owns
the later published-check/job snapshot. Production c2a95a6 remains unchanged.

## Decision for the user

There is no minimum gain required to retain an improvement. The previous
clause-arena change is preserved as a memory/runtime tradeoff, not mislabeled
as a speedup: full7312 is 8124.477s versus the preceding 8096.416s, and all five
repeated witness medians were adverse. All fifty original certificates/models
are now checked and residue-clean. Fixed Kissat remains 7239.010s; the latest
full score is 1.122318798x, not the 1.01x goal.

The newer root-storage prototype is not rejected. The apparently severe short
Tier-B regression had a confounded baseline: old legacy storage preserved
stale fixed-variable queue entries. We repaired their lifecycle and measured
legacy versus folded storage again. Both corrected arms now produce the same
search work and certificate bytes. This clears the way for meaningful storage
measurements rather than preserving an accidental favorable stale schedule.

## Observed cause and two isolated fixes

7362/job1681406 traces all29 identities identified by7359. Every root is
assigned at C0/D0, propagation315259--339723, within late Lucky's interval
315251--514529, after entry trail2870. Both complete root streams agree;
neither complete deactivation stream contains any of these29 external IDs.
Thus the owner is the late DefaultPolicy Lucky boundary, not warmup or GC.

The original7363 six-archive byte gate failed because enabling global tracing
also adds macro trace-begin, checkpoint and retier rows. It was not waived.
7367 separately preserves every original macro row and state field, permitting
only these explicitly enumerated observer additions and their sequence-number
shift. The other five complete prior archives remain byte-exact. Both full
solves reproduce their own Release work/proofs and both proofs check clean.
The classified diagnostic is not an observer timing measurement.

The isolated7371 source has two policy/guidance changes beyond frozen7339:

- Both existing DefaultPolicy Lucky call sites consume newly proved roots in
  trail order before later guidance. Raw Lucky, proof production, public action
  capability, thresholds and scheduling cadence are unchanged. The deferred
  early call still reconstructs its fixed-variable image on first attachment.
- A new heuristics owner preserves the kernel's existing compact import-order
  and inverse maps. Previously a stopped/resumed solve appended duplicate
  identities. Normal variable growth still appends new entries.

The second defect was discovered by the first independent stopped/resumed
proof fixture7368, not by relaxing a test. The first96 parent/candidate checks
passed, then the sanitized candidate aborted on the second attachment's
duplicate inverse-map assertion. The failing CNF/proof/logs remain available.
Actual optimized parent negative control7372 reproduces48 changed images over
eight fixtures and three reattachments; the required assertion fails before
repair. Lucky parent negative control7364 reproduces140 stale entries over12
fixtures covering1/5/29 roots, two import directions and both folding modes.

7373 validates the repaired source:125 Release tests in each of two independent
builds,124 Trace tests, and17 focused ASan/UBSan/LSan fixtures run twice without
suppression. Earlier7366's LSAN failure under ptrace is retained;7369 reran
that validation outside the sandbox.7374 independently checks72 UNSAT proofs
and72 SAT models, both signs, root counts1/5/29, eager/deferred attachment,
stopped/resumed solves, DRAT/LRAT, parent and candidate Release and candidate
sanitized libraries. Two false-proof controls are rejected. Its120966 bytes
of generated, checked fixtures were removed; the failed7368 witness was not.

## Short causal control and B/C gate

7375 repeats the unchanged B28+C5 matrix against7340. All66 solver outcomes
are expected.29 pairs match every raw non-timing field/certificate; four differ
only in deactivation totals, by5/29/1/5.7381 classifies exactly those four known
deltas with11 negative controls; the original strict reader still reports
CLASSIFICATION_REQUIRED. No generic counter exclusion was introduced.
64/66 were checked at the drafting snapshot; later audit status supersedes
that count, not the frozen solver measurements.

Corrected legacy7378/7379 differs from7371/7373 only by the existing Reduce
compaction call's root-fold boolean. Both retain the lifecycle and collector
fixes. No new compaction call or threshold is added. The legacy control passes
all125 Release tests. Short diagnostic7380/job1681474 gives:

| Corrected arm | Time | Peak RSS KiB | Physical variables | Conflicts |
|---|---:|---:|---:|---:|
| Legacy |26.508s|162076|35847|768682|
| Folded |26.409s|166632|26320|768682|

All non-storage work and complete proof bytes agree under unchanged7350's
explicit storage classification and conserved total slots. Both proofs check
clean. The old uncorrected legacy result was14.791s/312090 conflicts. Therefore
the old difference was search trajectory, not a comparable unit-cost penalty.
The0.099s paired difference is not a replicated speedup; short-run RSS is worse
despite fewer slots. B14 here is FmlaEquivChain, not Tier-D Rank14.

## Rank25: direct evidence for an implementation-storage problem

Previously, ordinary Rank25 Tactix was224.225s versus native246.184s, but used
2484880 versus888820KiB peak RSS. Longer total runtime cannot explain that gap.
7328 observed3015698 fixed Active identities among3586539 physical variables;
eight selected arrays account for294.15MiB, only a partial storage subtotal.
The earlier observer first reached its maximum high-water mark after BVE at
C15000, not at C0; it is not an exact allocation-by-allocation peak decomposition.

New7382/job1681476 wave0 uses the same12CPU/48000MiB/1200s solver-only/16000s
checker/5h contract and immutable7379/7373 pins. The first completed Rank25 pair:

| Metric | Corrected legacy | Folded roots |
|---|---:|---:|
| Solver wall time |224.921s|203.949s|
| Peak RSS |2484864KiB|1757532KiB|
| Final physical variables |3026701|10976|
| Inprocessing slot |38.718s|29.093s|
| Outside-slot search + scaffolding |185.301s|173.980s|
| Machine instructions |1289101958180|1115715689445|

Every reported non-storage work counter and proof-stream hash/byte count
matches. Both SAT models are independently verified against the original CNF;
the stored certificate hash is the proof-stream hash, not a model-byte digest.
Peak RSS falls727332KiB (about29.3%); wall time falls20.972s (about9.3%).
Both process counter observations report3.800GHz and full CPU use. Instructions
fall about13.5%, so this is consistent with eliminating physical traversal and
storage work, not simply a better logical search trajectory. It is one pair:
three-wave medians/spreads are still required, and outside-slot time must not
be called pure CDCL or BCP cost.

The reported slot reduction is distributed: Substitute3.819->1.214s,
Factor2.571->0.130s, Vivify4.457->2.987s, while BVE17.704->17.559s is nearly
unchanged. These are rule-envelope timings, not isolated instruction costs.
The old native RSS remains substantially lower; one improved pair does not
establish memory parity or explain every remaining allocation.

## Runtime bookkeeping double-check

The actual7373 optimized cache remains Release -O3 -DNDEBUG, with event trace,
rule journal, proof shadow and conflict-stop instrumentation compiled OFF.
The copied runtime runner clears diagnostic environment variables. Default
CDCL propagation/analysis still use direct untimed rule application; no new
per-conflict timer was introduced by these fixes.

Optional scan counters, individual inprocessing rule clocks and dormant guards
remain.7185's three-wave joint ablation did not consistently improve runtime;
that is not proof of zero overhead. Both arms retain the same external perf
counters and process-tree VmPeak sampling. Decompression, hashing, independent
proof/model checks and cleanup are outside solver time; proof emission, normal
statistics and wrapper startup/wait remain inside. No evaluation contract,
old campaign script or native denominator was changed to favor Tactix.

## Handoff and limitations

7382 has three declared four-task waves on Ranks25/31, with middle arm order
reversed. Wave0 is submitted. Advance each next wave on complete expected
solver outcomes, exact own repeats and unchanged non-storage work/certificates;
do not wait for proofs. Retain every time/RSS regression and full repeat range.
If storage work changes unexpectedly, diagnose before launching another wave.
Then decide the full1--50 rerun before production promotion, including the
already-closed ranks and fixed7239.010s Kissat denominator.

Full Debug7342/session72197 still runs its final demo after122 earlier passes;
it is not a completed Debug suite. Current-source fuzz remains outside the
recorded denied transfer/access boundary, with no retry or artifact inspection.
No source promotion, push retry or goal completion is claimed. New helpers add
11 deactivation-classification and8 matrix negative controls;377 benchmark and
22 SLURM tests pass. Required living-document caps remain respected.

Audit7383 preserves incremental and cumulative source patches, pin hashes,
failed and successful validation evidence, pending-job mapping and final
checked-residue status without changing earlier artifacts. Andrew's mandatory
local, unsent TeX/PDF is in
`report-to-andrew/20260911-1153-lucky-lifecycle/`.
