# Clause arena: repeated memory benefit, runtime cost, full-set transfer

September11,2026. Productionc2a95a6 is unchanged. The prior bookkeeping recheck
confirmed existing evidence without changing the next action. This continuation
verified the clean94a2f980 handoff,3121 frozen7307 bindings and7308's exact gzip
exceptions, advanced the final runtime wave, and localized the new Release peak.

## Three-wave decision

[7309](2026-09-11-clause-realloc-assessment-7309.json) preserves all30 expected
outcomes and all three paired ratios per witness. Every raw non-timing counter,
including physical dense counters, and every certificate fingerprint matches
actual7267 and its full7283 result. Sixteen independent checks were complete at
the assessment snapshot; pending proofs remain pending, not accepted checks.

| Rank | Median7302/7267 time | Full paired range | Peak RSS delta KiB |
|---:|---:|---:|---:|
|9|1.002712127|1.002145183–1.004324301|-68904|
|25|1.028837552|1.014052965–1.033303850|-16|
|31|1.007409733|1.004876235–1.008550341|-78928|
|35|1.013505268|0.998458055–1.013733742|-9704|
|38|1.007330362|1.006091210–1.009898376|+1436|

RSS is identical across all three repetitions of each arm. Rank31 falls from
562920 to483992KiB,14.0212% or77.0781MiB. Rank9 improves and Rank25 preserves
its previous dense-storage saving, but16KiB is not a material new gain. All five
median runtimes are worse. Rank25's candidate spread is2.2452%, so its absolute
runtime headline fails the plan's2% stability gate. The full paired evidence
is retained; no favorable run replaces it and no extra repeat is selected.

This supports a memory-priority full50 investigation, not an overall runtime
improvement or production acceptance. The new7312 preparation follows the
plan's distinction between an unstable absolute headline and a valid complete
paired experiment; it does not require the former to measure full-set memory.
No scoring exclusion, test gate or existing frozen helper was changed.

## Exact Release diagnosis

7310/job1681255 ran unchanged7302/7267 Release binaries through byte-identical
external observers proven in7299. Both full work/certificate fingerprints match
their unobserved7306 oracles. GNUtime and sampled solver VmHWM exactly reproduce
483992/562920KiB. Diagnostic times never enter the score.

[7311](../runs/20260911-7310-clause-memory-rank31/resident-analysis-7311.json)
places candidate first-maximum detection at121.4905s. The window contains
WatchLists::grow allocating67112960bytes and freeing33558528bytes, from ordinary
propagation. The preceding arena-copy peak has been displaced by sparse-watch
32-to64MiB growth. Previous first-maximum detection at229.9215s still brackets
clause addition allocating93519872bytes and freeing46764032bytes during conflict
analysis. Later non-atomic smaps snapshots report451408/519488KiB total RSS,
with327288/372800KiB in the heap. These are not atomic peak states or live malloc
inventories. All1/2 sampling gaps and3/6 raw HWM decreases remain explicit.

[7313](../runs/20260911-7310-clause-memory-rank31/mapping-analysis-7313.json)
retains a countervailing allocation observation:

| Operation |7302|7267|
|---|---:|---:|
|brk|894|246|
|mmap|1187|391|
|munmap|1165|369|
|mremap|7|2|
|Total|3253|1008|

No failed syscalls or unparsed records; incomplete loader stacks are retained.
The final traced heap extent is216555520 versus384679936bytes. Virtual extents
are not RSS or live allocations. The increased syscall count is a plausible
cost lead, not proof of syscall elapsed cost, glibc threshold behavior, or the
whole-runtime regression's cause. Realloc affects allocation behavior elsewhere
despite identical solver work; counting only avoided arena copies is incomplete.

Do not immediately implement another small watch-buffer change just because
the peak moved. First measure full-set breadth, then prioritize the largest
remaining memory/runtime losses. Rank31's roughly265-second native gap and
known search-work differences cannot be explained by this sub-percent runtime
change. Existing31 retry-producer evidence and larger-gap profiles remain
relevant; outside-slot time is not pure CDCL and unmatched rule totals do not
measure unit cost. No new scheduling/budget/selector change is justified here.

## Full50 and handoff

7312/job1681267 was submitted07:37:08EDT, full width with no node constraint:
12logical CPUs,48000MiB,1200solver-only seconds,16000checker seconds and five
hours. It copies unchanged7302, the existing full50 runner/matrix and fixed
Kissat7239.010s authority. No routine native refresh or denominator change.
The originalcpu-q campaign and other protected campaigns remain hash-exact.
7306's final wave1681257 and all prior proof checkers continue independently.

Next: run validate-clause-full50-7312.py, classify every loss/invalidity, require
all50 exact own7267 work/certificates, and seal its complete per-rank time/RSS
table only after all solver outcomes. Do not wait for proofs to interpret it
provisionally; all independent models/proofs and zero residue remain acceptance
requirements. The pre-transfer7283 view remains8096.416/7239.010=1.118442439,
785.0159seconds above the1.01target. Retained7144/7167 remains8072.059s rollback;
7253's8057.066s is a separate cross-window view, not causal attribution.

No solver source changed this continuation. Existing120/120/119 suites,
32768-clause actual-parent replay,20 sanitizer legs,156proofs/156models and two
rejecting controls remain7302–7304 authorities. The adapter uses existing
tested strict parsers;44 memory tests and348 aggregate benchmark/22 SLURM tests
pass.7314's audit owns the later job/check snapshots and all documentation
checks. Living docs remain within caps; no automatic compaction.

Andrew's TeX/PDF is local at report-to-andrew/20260911-0736-clause-transfer/.
No material files were deleted and no growing diagnostic log was frozen.
Preserve7308 raw/gzip exceptions and7293/7292 authorities. Codex debugs directly;
no denied fuzz access/transfer or push retry. N1–N4, closures4/32/33/49/46,
Rank21 scored/shelved, and7077Rank43 unsubmitted remain. Current-source
Claude-owned fuzz, independent checks/residue, full50, consolidation and the
original convergence goal remain open; no promotion is claimed.
