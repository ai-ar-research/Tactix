# Full50 memory outcome and prototype retention

September 11, 2026. Source/pins remain 7371/7373, production c2a95a6
unchanged. This report supersedes the pending-full50 decision in 7386, not its
frozen measurements. No solver, old runner, reference or acceptance gate changed.

## Decision

Retain the root-storage prototype as a demonstrated memory improvement.
Do not describe it as an aggregate runtime win yet. There is no minimum-size
gain requirement: the reason to withhold production promotion is the remaining
correctness/consolidation gates and unresolved tradeoffs, not that a useful
improvement is too small. The implementation and source patches are preserved.

Distinguish this from the earlier clause-arena prototype: full7312 was 28.061s
slower than its predecessor and all five repeated witness medians were adverse,
despite memory savings. That was a memory/runtime tradeoff, not a rejected
positive runtime result. Report7324 owns those historical measurements.

## Complete 50-instance rerun

Frozen7385 records all50 expected outcomes, 25 SAT and25 UNSAT, with no solve
loss. Total solver time is8138.708s, versus predecessor8124.477s: +14.231s
(+0.175%). Fixed Kissat remains7239.010s, so the current score is1.124285x;
the1.01x target remains827.3079s away. Checks continue in the background;
7387 observed38/50 independently checked, with no published invalidity.
Pending proofs are not accepted correctness results.

All measurements use the unchanged full-width12CPU/48000MiB,1200s solver-only,
16000s checker contract. No native refresh or old campaign mutation occurred.

## Memory across all50

These are solver peak RSS values, not checker memory or simultaneous fleet use.
Compared with7312,25 peaks decrease,11 are identical and14 increase. Treating
changes smaller than1MiB as tiny for presentation only gives11 reductions,
27 tiny/unchanged results and12 increases; this is not an acceptance threshold.

| Rank | Previous peak KiB | Current peak KiB | Change |
|---:|---:|---:|---:|
|25|2484880|1757532|-29.27% (-710.3MiB)|
|17|1227392|993388|-19.07% (-228.5MiB)|
|33|571412|519708|-9.05% (-50.5MiB)|
|48|972240|927728|-4.58% (-43.5MiB)|
|21|2565388|2644208|+3.07% (+77.0MiB)|
|31|483992|508308|+5.02% (+23.7MiB)|
|32|531520|554808|+4.38% (+22.7MiB)|
|41|155244|166632|+7.34% (+11.1MiB)|

The largest peak remains Rank10 at16016620KiB (15.2746GiB), exactly unchanged.
Ranks14/13/8/5 also retain their previous peaks:12.3154/12.2368/11.3473/
10.1657GiB. Thus the prototype meaningfully helps selected instances but does
not reduce the worst-case peak. All50 measured peaks are below16GiB; the larger
allocation does not prove successful execution under a hard16GiB cap.

For descriptive purposes only, the sum of the50 independent peaks falls from
110128748 to109210388KiB (-0.834%). This is not simultaneous memory usage and
must not hide individual regressions. The median per-instance RSS ratio is
essentially1.0. Memory parity with Kissat is not established: Rank25 still uses
1757532 versus888820KiB (1.977x); Rank10 uses15.2746 versus14.6373GiB.

## Why Rank25 helps, and why the full time gain is limited

The earlier ordinary Rank25 run was already faster than native (224.225 versus
246.184s) despite much higher RSS. Longer runtime cannot be the sole cause.
The diagnostic census observed3015698 root-fixed Active identities among
3586539 physical variables. Keeping these identities in hot physical arrays
imposes storage and traversal costs; physical compaction can remove that cost
while retaining public, proof and model identities. The census is not an exact
object-by-object decomposition of the ordinary RSS peak.

The corrected-legacy/folded three-wave comparison7384 holds reported logical
work and certificate fingerprints fixed: Rank25 median time -8.66%, peak RSS
-29.27% in every pair, inprocessing-slot time -24.86%. Outside-slot search plus
scaffolding improves5.31%; it is not a pure CDCL timer. BVE time is nearly
unchanged, while Factor/Substitute/Vivify envelopes improve more. This is a
specific structural improvement, not a universal speedup of every rule.

The full run reproduces the memory benefit but records212.331s for Rank25,
versus203.949--204.747s in the targeted waves. The cross-window difference is
retained, not replaced with the best targeted time or attributed to a proven
environmental cause. Rank31's matched runtime is neutral and RSS worsens about5%.

Strict7385 finds18 exact-raw,29 storage-only and3 changed-work/certificate
comparisons. New7387 is a sidecar; it does not loosen that classifier:

- Rank22 differs by exactly+3 deactivations; Rank34 by exactly+1. Both retain
  certificates and all other non-storage statistics. Their particular producers
  were not traced, so no stronger causal claim is made.
- Rank41 takes26.260 versus14.740s (+11.520s), with genuinely more search work
  and a different certificate. Its input hash, every non-timing statistic and
  certificate exactly match the corrected Tier-B14 witness7380. Both corrected
  legacy/folded controls take26.508/26.409s with matching non-storage work.
  This is the previously diagnosed Lucky-queue lifecycle trajectory change,
  not a new unexplained root-storage slowdown. It remains fully scored.

## Runtime bookkeeping double-check

The actual7373 cache confirms Release -O3 -DNDEBUG and OFF event trace, rule
journal, proof shadow and conflict-stop instrumentation. The pinned7385 runner
clears diagnostic environment variables before launching the solver. Heavy
root census is not enabled. DefaultPolicy's ordinary propagation/analysis use
the direct untimed path, not a new per-conflict public-rule timer.

Some optional bookkeeping does remain: scan counters, individual inprocessing
clocks and disabled diagnostic guards. The three-wave7185 joint ablation of
scan counters/rule clocks yielded time ratios1.009553/1.014068/0.998967 on
Ranks31/35/38. It did not show a consistent net speedup, and does not establish
zero overhead or measure every guard. External perf counters and process-tree
VmPeak sampling are common to the inspected Tactix/native runner lineages.
Decompression, hashing, independent checks and cleanup are outside solver time;
proof emission, normal statistics and wrapper startup/wait remain inside.

## Priorities and validation

Rank31 remains the largest unshelved runtime gap:534.449 versus264.532s,
+269.917s. Its conflict count also exceeds native; whole-run timing cannot
establish pure unit-cost parity. Prioritize phase-aligned work and profiles
there, then Rank43 (+76.285s), preserving established4/32/33/49/46 closures.
Rank21 remains shelved but scored. The largest unshelved absolute memory excess
is now Rank9 (+1065468KiB), followed by43 (+968764),18 (+913152) and47 (+903976).
Do not keep Rank25 as the default largest memory target after its improvement.

This increment changes only analysis/tooling/status. New7387 tests include12
negative controls for unexpected counter changes, repeated reports, malformed
duplicates, changed work/certificates, mismatched binaries and different inputs.
Existing storage and full50 gates are unchanged. No new solver tests are needed
for this report-only increment; prior125 Release twice/124 Trace,17 sanitizer
fixtures twice and144 fixed proof/model checks remain the source authority.
FullDebug7342's final demo and current-source fuzz remain open, not waived.
No manual benchmark material was deleted; pending certificates and failed
witnesses remain owned by their existing campaigns. No denied fuzz access,
transfer or push was retried. New Andrew TeX/PDF is local and unsent under
report-to-andrew/20260911-1238-full50-memory/.

Authorities: frozen7385 summary/comparison.tsv;7387 classification JSON and
validator;7384 repeated assessment;7383 lifecycle report;7324 census;7185
bookkeeping conclusion. Next: check published asynchronous validation outcomes,
then continue the largest phase-aligned unit-cost/search-work diagnosis rather
than discard the demonstrated memory improvement.

Final checkpoint:377 benchmark tests,22 SLURM tests,12 new classification
negative controls and all seven invoked documentation checks passed. Size
checks retain five low-headroom warnings, with no cap increase. The sandboxed
progress check could not query SLURM; a subsequent authorized read confirmed
the four existing arrays still checking proofs and Debug PID3043191 running
at99.3% CPU after2h24m. The final read-only full50 validator reports39 checked;
the38-check snapshot7387 remains frozen. Andrew's two-page PDF compiled twice
successfully after fixing draft line overflows; final log has no Overfull.
No workflow paused for checker completion. Local evidence/status commit only;
production integration and previously denied synchronization remain open.
