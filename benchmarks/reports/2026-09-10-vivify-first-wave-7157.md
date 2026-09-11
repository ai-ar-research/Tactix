# Compact Vivify: mixed first wave, continue the declared measurement

7144 remains isolated and unpromoted. All six first-wave7150 solver outcomes
match frozen7030 non-timing work and certificate fingerprints. This permits
provisional dependent measurements, not proof acceptance or a performance win.

|Rank|Candidate seconds|7030 seconds|Runtime ratio|Peak RSS ratio|
|---|---:|---:|---:|---:|
|31|527.995|533.427|0.989816788|0.979971477|
|35|357.820|360.295|0.993130629|1.024660357|
|38|340.558|339.008|1.004572163|0.838123140|

Rank31's first pair barely exceeds the1% improvement threshold; Rank35 does
not. Rank35 RSS increases2.466%, while Rank38 runtime increases0.457%.
These mixed first observations do not establish acceptance or a noise-robust
rejection. Complete the predeclared three pairs without selecting repeats or
weakening gates. The unchanged first-wave matrix, exact binaries, resource
context and raw outcomes live in
[7150](../runs/20260910-7150-vivify-compact-runtime/manifest.json).

Runtime wave1 job1680208 was submitted21:35:29 local after all six wave0
outcomes passed exact-work/certificate comparison. The first complete-Vivify
profile pair, job1680206, was submitted21:33:42 after the exact unprofiled
Rank35 pair. No proof-check wait was introduced.7157 audit owns the latest
published checking and SLURM state; proof invalidity pauses dependent work.

## Measure the whole function, not a favorable inner scope

The new `perf_function_envelope` helper counts each sample containing the exact
binary's `vivify_apply` frame once, including external allocator descendants,
root preparation, scheduling, watch rebuild and probing. It includes initial
and search phases. Present Vivify frames are retained even when the outer
phase is absent; unknown phase ancestry is not silently priced as search.
Recursion or clone aliases do not double-count periods. Separate self/phase
partitions conserve the selected total; outside samples are not called CDCL.

Six new tests cover full descendants/self, missing phase, unresolved caller,
duplicate/clone frames, foreign binary/similar names, empty/huge/invalid input.
[7154 replay](2026-09-10-vivify-envelope-replay-7154.json) reproduces both
frozen7132 wave0 complete-Vivify totals exactly:7116 candidate142380811845 and
7030 control140836499634 sampled periods, with no unmatched Vivify phase.
Those are old observations used to validate attribution, not7144 performance.
No old reader or frozen evidence was modified, and no raw profile expansion
was needed. The new extractor requires this replay before interpreting7152.

7153 will extract each new paired wave;7155 will summarize all three unprofiled
waves;7156 will summarize all three full-function profiles. Successful writers
must never be rerun. At least15% lower complete-Vivify sampled cycles on35,
at least1% lower whole-runtime paired medians on BOTH31/35, no runtime/RSS
regression31/35/38, and the unchanged variance/validation gates remain required.

No solver edit, pin rebuild, new fuzz coverage or cleanup deletion this
increment. Current-source Claude-owned fuzz remains unavailable/uninspected;
no denied transfer/push retry. The complete50 gap, memory, transfer and
consolidation obligations remain. Andrew's report is local and unsent.
