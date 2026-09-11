# Runtime bookkeeping: conclusion and structural checkpoint

Frozen September 10, 2026. This succeeds the first-wave interpretation7179;
it does not change any submitted runner, binary, or historical result.

## Answer and decision

Some optional bookkeeping remains in the actual retained7144/7162 runtime
binary. The heavyweight event trace, rule journal, proof shadow and conflict
stop are compiled OFF, with Release `-O3 -DNDEBUG`. The runner clears inherited
diagnostic environment variables. Binary-scan counters and individual rule
clocks nevertheless execute. Ordinary DefaultPolicy CDCL propagation and
analysis take the untimed direct path, not the timed public dispatch path.

7172 removed the binary-scan diagnostics and individual rule clocks together,
preserving logical work meters, scheduling budgets, rule-call counts, layout,
proof/model paths and coarse solve/slot clocks. Three paired waves7175 produced
all18 expected UNSAT outcomes, exact retained work and certificate fingerprints
under the predeclared timing/binprop exclusions. Four independent checks were
complete at the frozen7184 assessment; pending proofs are not accepted checks.

| Rank | Median ablated/control time | Full paired range | RSS ratio |
|---:|---:|---:|---:|
|31|1.009553|1.006841–1.010057|1.000000|
|35|1.014068|1.009702–1.022663|1.000000|
|38|0.998967|0.997786–0.999287|1.000000|

Both arms' complete runtime spreads meet the predeclared2% stability condition.
Do not enable this ablation by default: it is slower on31/35 and nearly neutral
on38. This measures the net effect of this compiled joint change, not individual
counter prices or zero intrinsic diagnostic cost. Downstream code displacement
and environment remain possible confounds, not proven causes of the slowdown.

The retained binary also contains Factor offered/materialized/reused counters
and runtime trace lookups. Saved7171 attributes roughly0.070% of the inclusive
Factor envelope to its direct getenv child; this is sampled attribution, not
an isolated ablation. The7172 experiment did not remove these diagnostics.
Further source/binary inspection finds disabled runtime-guarded diagnostics:
ParseMemoryTrace retains a null-output check, Walk retains a cached choice-trace
limit/branch and per-walk trace lookup, and Congruence/Substitute stage timers
test cached environment flags before reading clocks. Their diagnostic output
is off in this runner, but their guards are not all compiled away. No complete
zero-bookkeeping or all-diagnostics-cost claim is warranted.

External perf-stat counters and100ms process-tree VmPeak sampling are enabled
in both inspected Tactix and fixed Kissat runner lineages. Decompression,
input/pin/certificate hashing, independent checks and cleanup are outside the
solver measurement. Proof emission, normal statistics and wrapper startup/wait
remain inside. Do not silently alter the common measurement contract to improve
one side's score.7173 owns the detailed runner audit and complete fresh50 table.

## Larger independent Factor lead

7171 measures the complete Factor envelope at53.88–54.04B sampled periods versus
32.62B native.7178 identifies index construction allocations and the per-list
destructor as structural costs; native entry-self alone is not a matched cost
denominator because of differing inlining.

Isolated7180, based directly on retained7144 with normal telemetry, replaces
independently allocated eight-byte occurrence lists with a shared arena. It
preserves borrowed initial binaries, encounter order, cached remainder IDs,
stable deletion, live degrees, logical ticks, budgets and public rule semantics.
Literal heads remain24 bytes; the proposed saving is allocation/locality, not
smaller headers. Dynamic growth may leave unused arena slices: RSS and complete
Factor/runtime measurements must decide, not an allocation-count assumption.

Validation: Release116/116, independent BC116/116, trace115/115;34 fixed layout
scenarios exercise append/relocation/cache edits/stable erase/growth/reset against
vector-of-vectors; four Factor fixtures pass twice under ASan/UBSan/LSan.7181
checks288 DRAT/LRAT proofs from actual parent/candidate libraries with two rejected
negative controls, exact state and certificates. Its570144 bytes of generated
fixed inputs/proofs were removed with a receipt; fixtures can regenerate them.
7183/job1680435 has all66 expected B28+C5 outcomes and33 exact pairs,64 checked
at23:35 EDT. Proof checks continue asynchronously. No arena performance claim,
production integration or fuzz acceptance is made.

Fresh7167 full50 has48/50 checked-clean at23:35 EDT; its complete solver result
remains8072.059/7239.010=1.115077752x fixed Kissat. The target and memory parity
remain open. Next: three paired31/35/38 arena/control runtime waves, then matched
complete Factor profiles with reviewed current-binary call sites. The66 exact
solver outcomes allow this without waiting for the last proofs. Current-source
fuzz remains unavailable under the recorded denied-transfer boundary; no retry,
artifact inspection or waiver. Productionc2a95a6 is unchanged.

## Authorities and test consideration

- `benchmarks/reports/2026-09-10-runtime-telemetry-assessment-7184.json` owns all
  paired times, placement, phase decomposition, hashes and stability calculations.
- `benchmarks/reports/2026-09-10-runtime-evaluation-audit-7173.md` owns the
  original runtime-binary/harness audit and per-rank table.
- `benchmarks/runs/20260910-7180-factor-arena-build/manifest.json`,7181 proof
  summary,7182 sanitizer summary and7183 BC manifest own the isolated prototype.
- New checkpoint7185 binds this evidence and replays validators. The report adds
  no solver behavior; existing7184 median/range/invalid-input controls cover its
  aggregation. New7180 tests cover its changed representation. No test gate is
  relaxed, and successful artifact writers must not be rerun.
