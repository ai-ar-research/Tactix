# Rank31: reject source-observer attribution and measure exact Release externally

Frozen September 11, 2026. This succeeds the completed full50 tradeoff
[7288](2026-09-11-realloc-full50-decision-7288.md), not its immutable measurements.

## The lighter observer does not reproduce the regression

[7286 summary](../runs/20260911-7286-memory-only-rank31/summary.json) has both
expected whole UNSAT outcomes, exact own Release raw counters and certificates,
including every physical dense counter. Proof checking remains asynchronous.
[7294 comparison](../runs/20260911-7286-memory-only-rank31/comparison-7294.json)
matches all10538 structural rows and reports every observer-to-Release error.

| Measurement | Candidate peak KiB | Parent peak KiB | Candidate minus parent KiB |
|---|---:|---:|---:|
| Repeated Release |562920|495156|67764|
| Heavy event trace7277 |505724|509956|-4232|
| Memory-only7286 |509084|510784|-1700|

Removing general event tracing is insufficient: the lighter observer still
reverses the adverse Release relationship. Its candidate peak is53836KiB below
Release and its parent peak15628KiB above. These are failed diagnostic premises,
not solver correctness failures or a memory improvement that can be scored.

The lighter candidate's first observed high-water maximum occurs at row2757,
Collect entry, conflicts915887/decisions119684150. Parent's maximum occurs at
row230, BVE capacity end, conflicts15346/decisions8293636, following dense payload
capacity growth from67108860 to134217724bytes. The candidate has less current RSS
at that same structural BVE boundary. None of these observations identifies the
unobserved Release peak. A high-water maximum may also precede the row that first
reports it. Stop extending this source-instrumentation approach for attribution.

## Next discriminating experiment

[7295](../runs/20260911-7295-memory-syscalls-rank31/manifest.json) runs the exact
7267/7253 Release binaries under external `strace`, tracing only brk, mmap,
mremap and munmap with timestamps and call stacks. No solver source, preload,
allocator hook, in-process diagnostic flag, budget, rule guard or schedule changes.
The tracer is inserted after the existing taskset/perf arguments so it observes
only the solver, never the checker. Normal evaluation campaigns remain untouched.

Pinned strace6.12 supports libdw stack tracing. Its preflight passes the existing
35815-operation allocation fixture and records actual allocator syscalls with
the fixture's own main frames. Six new wrapper tests cover both solver aliases,
outer wrappers, checker bypass, failure/missing output, existing output, ambiguous
commands and negative task indices. Strict whole-work/certificate and final-log
hash gates remain unchanged in substance. No new correctness exemption is made.

The pair is job1681165, submitted06:26:46EDT. Each task retains12logicalCPUs,
48000MiB,1200solver-only seconds,16000checker seconds and5h allocation, fullwidth
without node constraints. This is one diagnostic pair, not timing acceptance.
Tracing can change wall time, and the external wrapper's peak includes the tracer;
compare observed absolute peaks with Release before interpreting allocation
history. Mapping sizes and brk extent are not resident memory. Unwind gaps must
remain explicit. No RSS reproduction or allocator cause is assumed in advance.

Kissat's local vector implementation packs surviving slices in place with memmove
and shrinks the stack; Tactix's current FlatLists defrag still allocates a fresh
packing buffer. CaDiCaL's distinct clause collector deliberately uses a separate
to-space. These are structural differences, not proof of the Rank31 peak cause;
do not substitute a convenient representation rewrite for the pending diagnosis.

## Gate and hygiene status

All50 allocator outcomes remain exact,8096.416s/fixed7239.010=1.118442439x; the
runtime target and memory parity remain open. Full50 checks were42/50 complete
at the current read; the successor audit owns its own published snapshot.
Keep the measured9/25 savings and every31/27/other memory increase and38 slowdown.
Retained7144 and productionc2a95a6 remain unchanged. Preserve closures4/32/33/49/46,
Rank21 scored/shelved, N1--N4 and the proof-rule/strategy boundary.

[7293 erratum](2026-09-11-memory-log-snapshot-erratum-7293.json) invalidates only
two prematurely captured live-log hashes in audit7290. Now completed7286 solver
records bind final log paths, sizes and hashes; those final versions may be owned.
Never freeze live7295 traces before a solver record publishes their final hashes.
The eight canonical patch files remain unchanged with byte-exact gzip packaging
under7292; do not restage or normalize them. No new files are deleted here.

Codex debugs directly. Current-source Claude-owned fuzz and remaining independent
checks/zero residue/consolidation are not accepted, and no denied fuzz access,
transfer or push is retried. New reproduction controls distinguish exact peaks,
reversed direction and same-direction-but-perturbed observations; every changed
tool is covered by focused tests before submission.
