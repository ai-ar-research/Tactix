# Full50 clause-arena result and largest-memory localization

Frozen September 11, 2026. This supersedes the pending full50 checkpoint7314;
all older reports, runners, binaries and timing observations remain unchanged.

## Outcome and decision

Full7312 has all50 expected outcomes (25SAT/25UNSAT), no timeout or solve loss,
and exact own7267 work/certificate fingerprints including physical counters.
The frozen [complete table](2026-09-11-clause-realloc-full50-7312/comparison.tsv)
and [assessment7317](2026-09-11-clause-full50-assessment-7317.json) retain every rank.
Current7302 takes8124.477s versus predecessor8096.416s and fixedKissat7239.010s:
1.1223187977361544x native, +28.061s predecessor, 813.0769s above the1.01 target.
This cross-window difference is not a causal unit-cost measurement. Repeated
7309 witnesses also had adverse median runtime direction; Rank25's2.2452%
absolute spread failed the2% gate, without discarding paired observations.

RSS decreases on32 ranks and increases on18; none is equal. Maximum is
16016620KiB (15.274639GiB), Rank10. No observed16GiB exceedance is not proof of
hard-limit compatibility or native memory parity. The clause-arena change is
an isolated memory/runtime tradeoff, not a speedup or production promotion.
The full summary froze36 checked records; a subsequent reader saw39 checked.
7319 audit owns its later published correctness snapshot. Pending proof checks
continue independently and do not block provisional diagnosis.

Rank25 remains the largest unshelved absolute memory excess:1596060KiB.
Rank31 remains the largest runtime excess:264.665s. Rank21 remains scored but
shelved for debugging. Preserve closures4/32/33/49/46 and N1--N4.

## Exact ordinary-build Rank25 experiment

Campaign7315/job1681305 compared the unchanged7302 Tactix and fixed native
Kissat binaries using the proven external proc/strace observer. It is a missing
native diagnostic, not a refreshed scoring denominator. Each arm reproduces
its own complete work and certificate fingerprint; cross-solver work equality
is neither required nor claimed. Both original SAT models independently pass
and both final records have zero residue. Allocation:12 logical CPUs,
48000MiB,1200 solver-only seconds,16000 checker seconds,5h,full width,no node
constraint. Diagnostic times222.334/242.877s are excluded from scoring.

GNUtime, sampled solver VmHWM and own ordinary Release peak all agree exactly:
Tactix2484880KiB, Kissat888820KiB. The observer retained2202/2411 samples,
1/2 gaps and4/1 raw HWM decreases. [7316](../runs/20260911-7315-rank25-native-memory/resident-analysis-7316.json)
binds completed records and final log hashes; no growing logs were frozen.

Tactix first detects its maximum at192.9198s. The immediately preceding
474615808-byte mmap and237309952-byte munmap both come from
kernel_push_witness during BVE: std::vector reconstruction growth copies the
old buffer into a doubled allocation. This supports a transient-copy lead,
not attribution of the entire1.52GiB excess to that copy. Witness recording is
required SAT reconstruction, not removable diagnostic bookkeeping.
Kissat's first maximum is detected at164.5131s, bracketed by Congruence closure
and watch-vector release; its peak is a different phase and workload.

## Persistent storage, not only the last transient

The [7318 mapping-origin join](../runs/20260911-7315-rank25-native-memory/snapshot-origins-7318.json)
replays validated mapping syscalls to each later smaps snapshot. It retains
whole-VMA RSS, all contributing mapping origins and untracked ranges; it never
apportions RSS among merged mappings. Neither snapshot has a traced memory
syscall entry during its read interval. This is not an atomic peak, malloc
liveness inventory or syscall-completion guarantee.

| Tactix later-snapshot region | RSS KiB | Evidence |
|---|---:|---|
|process heap|821748|851460KiB extent; live objects/allocator retention unresolved|
|512MiB mapping|493056|event320 realloc, kernel_add_large_clause+0x8d5 during BVE|
|clause arena|269440|event325 ReallocWords resize through kernel_add_large_clause|
|reconstruction stack|235268|event326 kernel_push_witness mmap|
|merged compact/watch-head mappings|168120|events161/160, no per-owner RSS split|
|merged dense/head/kernel mappings|155676|events275/210/44, no per-owner RSS split|

Total later RSS is2261508KiB versus native554404KiB. Native later heap is
133052KiB; its clause-arena VMA is112120KiB. These are unmatched later states,
not a phase-aligned subtraction or exact peak decomposition. Full memory
syscalls are609/250 (brk198/43,mmap210/37,munmap188/26,mremap13/144), with no
failed requests. Unwind gaps remain in the bound output. In particular heap
growth lacks solver-frame attribution: do not assign that heap to BVE from
brk alone. Source inspection puts dense occurrence growth on the clause-add
path; exact object layout/capacity confirmation remains the next step.

## Next action and rejected shortcuts

Prioritize the persistent Rank25 dense/heap storage and its lifecycle over a
new small sparse-watch realloc change. Confirm the512MiB mapping's concrete
owner and compare live/holes/capacity and sparse/dense coexistence at BVE
boundaries. Separate required live structures from allocator-retained pages;
do not change growth/defrag thresholds or budgets without causal evidence.
The witness transient remains a bounded secondary repair opportunity, not a
complete memory-gap explanation. Preserve own Release RSS reproduction for
any additional source observer; earlier instrumentation failed that gate.

For Rank31, current conflicts10993050 versus native4540757 already establish
a work gap. Current total propagations3926801518 versus native2864694340 do
not yield pure CDCL unit cost: Tactix's printed outside-slot count and native
profile/search-helper boundaries differ. Native sparse resume can use search
propagation inside elimination; Tactix whole-slot counting includes it. Do not
launch a blanket native metrics build and divide those unmatched quantities.
Reuse saved full Tactix phase journals; obtain missing phase-aligned native
work only with a deliberately matched boundary contract. Rank43 remains a
large search-work lead;7077 remains unsubmitted.

## Validation and handoff

No solver source changed. Six new snapshot-join unit tests cover single and
merged mappings, heap non-attribution, partial/fixed/unmapped/remapped ranges,
time cutoffs/concurrent calls, failed calls, and rejected trace/snapshot input.
Existing native-work and solver-specific7315 fixtures reject semantic or
certificate drift.7319 audit owns aggregate tests, documentation checks,
current campaigns and hash bindings. No old gate or benchmark script changed.

Wrap-up generates local, unsent Andrew TeX/PDF under
report-to-andrew/20260911-0815-full-clause-memory/. No material deletion, denied
fuzz artifact access/transfer or push retry.7308 raw/gzip exceptions and prior
7293/7292 packaging authorities remain. Productionc2a95a6 and checked7144/7167
rollback8072.059s remain unchanged. Target, final checks/zero residue,
current-source Claude-owned fuzz, consolidation and synchronization remain open.
