# BVE memory ownership and near-complete rank4 prefix

## Decision

Memory peaks on the three selected witnesses precede garbage collection.
The next storage experiment should address dense-vector growth and compaction,
not shorten the collector's old-arena lifetime. A narrow realloc-backed private
FlatLists payload buffer has been proposed to the user; **explicit revamp
approval is pending**. No structural or production solver change was made.
The runtime target remains unachieved; accepted production is still c2a95a6.

The proposed first stage preserves exact capacities, list/entry order,
initialization and pointer-invalidation contracts while changing only payload
allocation. Kissat uses realloc for vector-stack growth; Tactix currently uses
std::vector reserve and a fresh vector during defragmentation. Realloc may avoid
temporary double residency, but is not guaranteed to do so. Rank21's compaction
case remains distinct: a growth-only replacement does not establish its repair.
The immutable6154 observer/cumulative6107 Release is the rollback authority.
Required gates: allocation/overflow/lifetime/failure tests, exact work/proof and
capacity replay, repeated memory/timing measurements, B/C, and Claude-owned
fuzz before promotion. Clause storage, both-side reconstruction, proof rules,
public N1 actions and scheduling are outside the proposed change.

## Checked memory evidence

Collector campaign[6148](../runs/20260908-6148-collect-memory/summary.json),
job1677036, independently checks all three original SAT models and cleanup.
Every non-timing statistic and emitted proof byte matches6115. Every previous
slot's capacity trajectory matches6129. Eight collector checkpoints distinguish
entry, survivor-order construction, reserve, sort, copy, swap, reindex and exit.
The temporary-capacity field measures the other clause arena, not all scratch.
For all three ranks the first maximum-RSS observation is **collector entry**.
This rules out that collection as the cause of the already-recorded maximum.

Finer campaign[6156](../runs/20260909-6156-bve-capacity/summary.json),job1677040,
also has3/3 independent model checks, cleanup, complete6115 work/proof replay
and exact6148 slot/collector capacity replay. Existing BVE callbacks emit a row
only when one of eight pool capacities changes; no list scan or per-candidate
output is added. These are observed intervals, not single-allocation proofs.

| Rank | Observed peak KiB | Previous peak KiB | Dense capacity change in peak interval |
|---|---:|---:|---|
| 21 | 2985752 | 2827256 | ordered arena512MiB−4B →128MiB−4B |
| 25 | 3408232 | 3348992 | large arena512MiB−4B →1GiB−4B |
| 9 | 3232192 | 2896372 | ordered arena512MiB−4B →1GiB−4B |

At rank9's peak sample both clause-arena and reconstruction capacities are
unchanged from the preceding sample; dense capacity rises by512MiB. Rank25
likewise grows only the sampled large dense arena. Rank21 lowers its ordered
capacity while maximum RSS rises, consistent with a copying compaction interval.
Several unsampled candidates/suboperations may lie between records; no claim
that the printed pivot alone caused the maximum. Instrumented time is not
Release acceptance, and the different peak numbers across observer builds are
not a measured memory improvement.

Reference inspection: `kissat/src/vector.c` enlarges via stack realloc;
`kissat/src/allocate.c` handles the allocation, and dense clause collection is
in-place in `kissat/src/collect.c`. CaDiCaL's `src/collect.cpp`/`arena.cpp` use
separate from/to spaces. Tactix's peak-localized dense payload is distinct from
either solver's clause collector. E1's both-side witnesses remain required by
the current N1 reconstruction contract and were not weakened for memory parity.

## Implementation and tests

[6146 build](../runs/20260908-6146-collect-memory-build/manifest.json) adds
diagnostic-only collector checkpoints and fixed sparse/dense/empty fixtures.
Actual old trace objects fail the expected missing24-row assertion. The first
Release build failed byte identity: disassembly shows a reversed equality-CMP
operand order in the collector. That build was not submitted. Explicitly
preprocessor-excluding every new call restores exact Release SHA86a251c9…;
retry pins6150/6151 pass100Release/99trace tests.

[6154 build](../runs/20260909-6154-bve-capacity-build/manifest.json) changes only
the trace implementation and adds a capacity-transition fixture. Actual old
objects fail the expected missing-three-row assertion. New suites pass101/101
Release and100/100 trace; Release remains byte-identical6107. Trace SHA is
recorded with the source inventory, observer patch and test logs. No new
semantic gate is claimed from diagnostic-only tests.

Run preparation initially encountered a generated pins/__pycache__ directory,
then a read-only already-copied file. The unsubmitted scaffold was verified
empty except matching pins, resumed from the manifest's explicit file list,
and never submitted without a complete manifest. Two zero-byte fixed-test
temporary files from intentional old-object aborts were removed by exact
validated ownership. No certificate, live job, pin or unique result was removed.

## Rank4 and accounting authority

[6160](../runs/20260909-6160-rank4-current-prefix/summary.json),job1677046,
matches **907045 selected decision rows through650000 conflicts** and all380
projected search boundaries. Startup retains the known extra unchanged
Substitute slot. Both arms stop as requested and delete partial certificates.
The full solves end at651764 conflicts, so this is near-complete prefix evidence,
not full-trajectory identity or a final proof check. Unit cost is now the leading
rank4 hypothesis; propagation counters still have different conventions.

The old watcher reported FAILED because unbounded accounting returned older
records sharing numeric1677046. Timestamp-bound accounting shows exactly the
current pair, submitted2026-09-09T00:19:08, bothCOMPLETED/0:0. No rerun occurred.
The maintained watcher now accepts `--since`, rejects ambiguous incarnations,
checks every task and exit code, and retries transport/accounting observation
failures on the same job. Thirteen fixed tests cover these cases; the exact
timestamp-bound current job also passes. Existing live watchers were not restarted.

## Remaining authority and limits

[6159 table](2026-09-09-fresh-rank50-6159/comparison.md) retains all50 ranks,
100 solver records and96 checked/clean certificates. Four proofs remain pending;
there is no partial aggregate or complete cleanup claim. Matched41–50 and all
fixed reference values are unchanged. BC6114 is66/66 checked; full6115 is48/50
checked at the prior audit. The validation companion records current counts.
No Claude-owned fuzz artifacts were accessed, no denied transfer/push retried,
and no source promoted. General closures and the proof-rule/strategy boundary
remain required. The structural experiment waits for user approval; independent
proof checking and further cost diagnosis can continue.
