# Largest-gap-first restart

The user explicitly resumed after the previous September10 06:00EDT cutoff
and then prioritized the largest bottlenecks/gaps. A replacement stopping time
has been requested but not supplied. This is a bounded diagnostic checkpoint,
not a solver repair or a new performance gain. Defer isolated ITE6969 full50.
Current6730 remains unpromoted at8300.164/7239.010=1.146588277x fixed Kissat;
all50 current outcomes are independently checked and clean. Production and the
fixed reference remain unchanged.

## Selection and expected leverage

The immutable [current full50 comparison](2026-09-09-reduce-watermark-full50-6750/summary.json)
selects31/35 as the two largest UNSAT losses. Their phase times come from
the corresponding checked6743 records, not the independent XOR6950 candidate.

|Rank|Current / fixed native seconds|Excess seconds|Slot / outside seconds|Current / native conflicts|
|---|---:|---:|---:|---:|
|31|484.219 /264.532|219.687|158.565 /325.440|9019812 /4540757|
|35|509.074 /306.699|202.375|180.856 /327.968|8942669 /4751990|

Together422.062seconds is39.77% of the net1061.154second gap, not of the gross
sum of positive losses. Roughly two-thirds of their current solve time lies
outside the inprocessing slot. Nearly doubled conflicts establish changed
search work, but do not apportion the wall gap between work and unit cost.
Native nested CPU timers and current wall timers are not interchangeable.

The existing [all50 census6814](../runs/20260910-6814-inprocessing-cost-census/summary.json)
already identifies aggregate named-rule costs: Vivify626.184s, Sweep433.148s,
Congruence409.478s, BVE300.050s, Substitute264.002s and Factor213.799s.
These prioritize inspections, not predicted savings. Small isolated gate-loop
changes cannot be the principal convergence strategy. Existing32/39/47 profiles
remain transfer controls; do not repeat them merely to obtain favorable noise.

Prior31 diagnosis remains valid: missing preconnection cleanup explains the
early BVE schedule residual, but composition5920 regressed full Rank31 and
TierC. Another early-prefix match would not overturn that rejection. Prior35
lifecycle6642 establishes a legitimate TransitiveReduce binary deletion,
which explains one later Reorder score residual, not the complete runtime gap.
Do not restore that binary merely to recreate the earlier branch. Rank21 stays
shelved; its separate substantial loss is neither solved nor erased.

## New diagnostic and next action

[Campaign7002](../runs/20260910-7002-largest-gap-release-profiles/manifest.json),
job1679638 since2026-09-10T07:36:38EDT, profiles unchanged6730 and ordinary
Kissat on31/35 in four independent full-width tasks. Each requests12logical
CPUs,48000MiB,1200solver-only seconds and16000checker seconds in5hours on
cpu-dense-hwulab-q, with no node constraint. The immutable5544 sampling wrapper
is copied byte-for-byte:199Hz user-cycle samples and8192-byte DWARF call stacks.
This is a permitted missing native diagnostic, never a denominator refresh.

Fresh validator7004 requires all four expected UNSAT outcomes, exact own-arm
counters and certificate bytes/hash, every candidate non-timing statistic,
valid self/callpath reports, zero lost samples and bound compressed data.
Use provisional records to proceed without waiting for independent checking;
pending certificates are not accepted. Solver perf-stat lists are intentionally
empty under this retained sampling wrapper, so do not infer IPC from them.

Next rank dominant search and inprocessing callpaths after replay validation,
then choose a source-backed causal test with material potential across the
full50. Distinguish an avoidable repeated operation from merely a hot necessary
operation. A cost-only change must preserve current work and scheduling/CDCL
semantics; a work-changing repair needs its own worse-boundary justification.
Preserve4/32, stronger33/49, and46 closures. No FlatLists redesign, promotion,
fuzz access or denied push/transfer retry is authorized.

## Tests, isolation and handoff

No solver, parser, checker or sampling behavior changed; no new solver regression
test is warranted yet. Reused readers/runners are covered by262 benchmark-tool
and22 scheduler-tool tests, all passing. New preparation assertions enforce the
top-two UNSAT selection, exact four-task mapping, checked baseline identities
and immutable hashes. Every subsequent implementation change must explicitly
consider a new unit test. Six documentation/semantic checks pass; size checks
warn about low reserve but all living docs stay within existing caps.

The first submission attempt could not create a sandbox SLURM socket; it was
interrupted before submission. The permission-enabled submission returned the
single job above, with all four tasks observed running. No campaign script,
pin or frozen report was modified; no benchmark material was deleted.
Previous interactive allocation1668242 is terminal COMPLETED at07:26:54.
The old06:00 monitor is finished, not restarted. XOR6980 remains49/50 checked
with Rank43 checking; ITE6991 B/C66/66 and6994 timing18/18 are checked/clean.
Current-source fuzz remains unavailable/unaccepted. The new Andrew report is
local and unsent; the successor audit owns later job and certificate states.
