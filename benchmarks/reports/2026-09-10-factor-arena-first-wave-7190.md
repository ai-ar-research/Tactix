# Factor arena: first runtime wave and complete-cost measurement

Frozen September10,2026.7180 remains isolated investigation material, based on
retained7144 with normal telemetry. No solver source changed this checkpoint.
Productionc2a95a6, fixed Kissat denominator and existing campaign pins/runners
are unchanged. The previous turn was progress:7184 completed the telemetry
ablation and7185 preserved the new Factor correctness gates.

## Measurement and first observations

7186 predeclares three paired31/35/38 waves, six concurrent tasks per wave,
with reversed middle arm order. Each uses12logicalCPUs/48000MiB,1200solver-only
seconds,16000checker seconds and5h allocation on cpu-dense-hwulab-q, fullwidth
without a node constraint. Immutable7183 runner/checkers are copied exactly.
All66 BC solver outcomes/33 exact pairs permitted launch without proof wait.
Wave0/job1680503 was submitted23:44:08EDT; watcher88237 remains attached.
All six outcomes are expected UNSAT with exact retained work/certificates.
Wave1/job1680511 was submitted23:53:22 immediately after this solver gate;
proof checking did not block progression.

|Rank|Arena seconds|Retained seconds|Arena/retained peak RSS KiB|
|---:|---:|---:|---:|
|31|525.085|526.529|563464/563464|
|35|355.777|356.135|600660/600660|
|38|340.367|338.002|78312/72092|

These first pairs have exact retained non-timing statistics and certificate
bytes/hash.31 is0.27% faster and35 nearly neutral, while38 is slower and adds6220KiB peak RSS.
This is not a median, accepted gain, or proof of the memory-growth cause. Keep
all three paired ratios/ranges, within-arm stability, and RSS; no minimum gain
threshold or repeat-to-select. Independent checks remain asynchronous.

7187 predeclares three complete Factor Rank35 profile pairs against the same
control, with the established199Hz user-cycle/DWARF8192 wrapper. Wave0/job1680509
launched23:50:42 after the unprofiled35 pair matched; watcher27341 attached.
Review binds all three actual scheduler call instructions and argument setup:
candidate0x507a13/0x5082c3/0x508e62; control0x507a93/0x508343/0x508ee2. They are
initial Probe, search Probe and Eliminate respectively, not labels guessed from
frequency or numeric order. Raw disassemblies stay hash-bound locally.

The frozen profile reader requires exact own unprofiled work/proofs, zero lost
samples, bound compressed data and conserved sample periods.7188 changes only
the selected function envelope to Factor, retaining the replayed extraction
contract. Include its index construction/destruction, lookup, grid application
and sparse-watch rebuild descendants. Profile RSS includes instrumentation and
does not supersede unprofiled7186 memory. No profile result is claimed yet.

## Leverage guardrail

Fresh7189 scans all fifty retained7167 records, including shelved21. Reported
individual-rule totals are Vivify592.072s, Sweep416.659s, Congruence411.901s,
BVE299.904s, Substitute263.313s and Factor215.195s. Missing timer rows remain
absent, not assumed zero. These are printed timers, not complete lifecycle
prices or predicted removable work. Even the whole reported Factor time is
well below the760.6589s still needed for the aggregate target. Finish this
bounded structural experiment, then prioritize larger lifecycle gaps and
demonstrated Rank31/43 search-work deficits; do not chase increasingly small
Factor fixes merely because its code is now familiar.

## Validation and handoff

317 benchmark-tool and22 watcher tests pass, including41 perf-reader tests.
The new runtime identity fixture rejects five task/rank/arm/input/status changes;
the reused fingerprint rejects work/certificate/schema changes.7189 tests timer
versus solve-row separation, absent rows and duplicate rejection. There is no
new solver behavior requiring another solver suite/fuzz run;7180 full suites,
sanitizers,288 independent proofs and7183 exact BC gate remain the authorities.

Read-only: check-submit-factor-arena-runtime-7186.py and
validate-factor-arena-profiles-7187.py. Submit a successor only after its entire
preceding solver wave is exact; do not wait for checks. Run
extract-factor-arena-wave-7188.py WAVE once per published profile pair, then
assess all three repetitions together. No successful writer is rerun.

7190 audit owns the final checkpoint's launch/hash/validation states. No new
manual residue deletion this checkpoint; active checker scratch and all
compressed original profiles are preserved. Current-source Claude-owned fuzz
remains unavailable under the denied-transfer boundary; no access, retry or
waiver. Proof completion/zero residue, memory parity, consolidation and the
<=1.01 convergence target remain open. Established4/32/33/49/46 closures and
N1–N4 stay protected. Andrew TeX/PDF is local and unsent.
