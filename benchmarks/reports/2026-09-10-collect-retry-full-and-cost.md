# Collection retry: full-set transfer and residual cost

## Full50 solver checkpoint

[7062](2026-09-10-collect-retry-full50-7062/summary.json) classifies every
candidate and predecessor record before scoring, preserves all50 ranks, and
provides the [complete comparison table](2026-09-10-collect-retry-full50-7062/comparison.tsv).
All50 candidate outcomes are expected:25 SAT and25 UNSAT, no solver timeout,
abnormal exit or solve loss. At sealing36 are checked/clean and14 proofs remain
pending; all50 predecessor and fixed native records are checked/clean.
This is provisional development evidence, not final checked acceptance.

|View|Candidate7030 seconds|Parent6730 seconds|Fixed Kissat seconds|Candidate/Kissat|
|---|---:|---:|---:|---:|
|All50|8130.686|8300.164|7239.010|1.123176512|
|Original40|7516.035|7678.989|6810.066|1.103665515|
|Added10|614.651|621.175|428.944|1.432939964|

The goal remains unmet: about819.286 seconds of further reduction are needed
to reach1.01x on this view. The169.478-second aggregate improvement is
cross-window, not an isolated feature price. Seven changed-certificate/work
ranks contribute97.121 seconds of net gain; the remaining43 unchanged-work
ranks contribute72.357 seconds. That latter change is not assigned causally
to advice repair, code placement or hardware without a discriminating test.

|Changed rank|7030 seconds|6730 seconds|Fixed native seconds|Candidate conflicts|Parent conflicts|
|---|---:|---:|---:|---:|---:|
|24|273.167|265.763|207.984|5663731|5490895|
|27|324.115|308.256|245.726|4921159|4351497|
|31|534.277|484.219|264.532|10993050|9019812|
|33|347.670|362.308|311.385|4866267|5189038|
|35|361.484|509.074|306.699|4751990|8942669|
|37|421.368|407.777|346.372|6902064|6911509|
|39|408.983|430.788|366.837|5784444|6113788|

Only these seven differ after removing timing and the exact advice-counter
line. Do not label all33 differing non-timing-statistic ranks search changes.
Rank31 now has the largest runtime gap,269.745 seconds. Among unshelved
remaining ranks,43/27/37 follow at78.476/78.389/74.996 seconds; Rank21 remains
shelved despite its151.944-second gap. Do not pick a smaller native-prefix
difference while ignoring these adverse outcomes.

Ranks4/32/46/49 retain exact parent certificates and native final C/D counts.
Rank33 changes but remains lower-conflict than native and improves in this
window; preserve its supported stronger behavior, not old trajectory identity.
Maximum candidate RSS remains15.272984GiB. This does not establish memory
parity: e.g. Rank31 and35 RSS are about2.16x/2.18x native. No OOM occurs in this
timing allocation; the memory obligation and unapproved FlatLists redesign
remain separate.

## Rank35 as a residual engineering-cost control

[7059](../runs/20260910-7041-collect-retry-runtime/matched-native-work-7059.json)
binds27 exact major counter correspondences between repaired7030 and checked
native7002 Rank35. They include conflicts, decisions, learned clauses,
chronological backtracks, restarts/reductions, warmup decisions/propagations,
walk steps, BVE eliminated/resolutions, Factor count/ticks, TransitiveReduce
ticks, substitutions, Sweep count/solves/equivalences/units, Congruence
rounds/merges and Strengthen visits/outcomes. Both raw rows are retained for
each correspondence. Six fixed extraction controls reject missing, duplicate,
negative and short records and distinguish exact labels from suffixed labels.

This strengthens the case for profiling residual implementation cost; it is
not complete candidate/pose/effort equality. Congruence gate counts and
Substitute ticks still differ. Propagation and Vivify counters have different
scopes. Earlier1M native prefix equality does not prove full-run equality.

Source review explicitly rejects direct timer subtraction. Tactix's registry
uses steady-clock wall time, and the phase timer begins before root backtrack
and propagation. Native profile.c uses CPU process time and nested profiles;
its search-time simplifier starts after root preparation, and initial Probe
has a separate scope. In particular, native Eliminate includes subsumption,
dense transitions and reconnection, unlike Tactix's BVE-only timer. Do not
price the residual gap as slot-minus-simplify or BVE-minus-eliminate.

[7060](../runs/20260910-7060-rank35-residual-profile/manifest.json),job1679809
since09:58:42EDT, samples the unchanged7030 Rank35 release once with the frozen
199Hz cycles:u/DWARF wrapper. It reuses the already checked native7002 profile,
not another native solve.7063 requires exact complete own work/certificate
replay, zero lost samples and hash-bound complete profile artifacts before
interpretation. Perf peak RSS is not solver memory. Any resulting cost repair
needs focused transition tests, B28+C5 and the required repeated timing/full50
gates; no source change or promotion is made at this checkpoint.

Rank31's independent7055/job1679779 later-prefix diagnostic remains the
priority for excess work. The predecessor and repair agree through1M with
byte-identical macro/boundary traces;7057 must validate the complete2M suffix
and actual threshold overlap before assigning a new cause. Current-source
fuzz remains unavailable; no denied transfer/push retry or fuzz inspection.

## Completed diagnostics and next scope

[7057](../runs/20260910-7055-collect-retry-rank31-2m/summary-7057.json)
validates both clean2M stops, the full inherited1M rolling hash, zero actual
threshold-overlap rows and all30,406,962 suffix rows. Candidate and predecessor
match153,799,751 decisions from startup, hash`8fa190b80c5d286c`. Complete macro
and inprocessing boundary files remain byte-identical. The new Rank31 adverse
effect begins after2M, with no new justification to restore rejected5920.

[7064](../runs/20260910-7064-rank31-full-boundaries/manifest.json),job1679811
since10:06:38EDT, changes the diagnostic strategy: full solves with only macro
and inprocessing boundary observers, using unchanged7030/6730 trace pins and
the existing solver-only environment wrapper. No decision stream or stop is
enabled.7066 requires full own Release work/certificate replay and every byte
of the cached2M boundary prefix before locating the first later differing
phase. The next detailed decision probe can target that phase, avoiding
successive huge prefix extensions. Normal proof checking and cleanup continue
asynchronously. No native rerun or solver/default change is involved.

[7063](../runs/20260910-7060-rank35-residual-profile/solver-summary-7063.json)
validates expected Rank35 UNSAT, exact entire own Release work/certificate,
zero lost samples and complete hash-bound profiles. Main propagation owns
35.11% self samples; heap bubble-down4.96%, private propagation4.61%, watch
rebuild2.03%, garbage collection1.95%, Vivify scheduling1.61%, dense occurrence
rebuild1.33%. Inclusive BVE elimination5.45%, Factor3.99%, Congruence3.23%,
and Vivify scheduling2.90% locate broader code paths; inclusive values overlap
and must not be summed. The native profile has different symbol boundaries.

The profile does not identify one isolated instruction that explains the whole
gap. Prior neutral/adverse6822/6838/6857/6869/6902 experiments remain rejected;
do not repeat snapshot-copy or binary-reconnect tweaks because their functions
are still hot. Prefer the largest source-backed common-state cost or coupled
mechanism, while the full Rank31 skeleton localizes the remaining search loss.
