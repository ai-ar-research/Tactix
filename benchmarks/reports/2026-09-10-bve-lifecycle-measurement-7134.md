# Combined BVE prototype: repeated whole-runtime and lifecycle measurements

7116 remains an isolated, unfuzzed prototype; productionc2a95a6 is unchanged.
The previous turn was progress: implementation, full suites, sanitizers and
strict proof controls completed, and the B/C gate launched. This increment
closes its solver-work gate and begins the predeclared decision-grade
measurements. No fresh Kissat or fixed50 run is needed here; the checked
full50 control remains1.123176512x the fixed reference.

## Exact-work gate and immutable measurement contracts

[7123 seal](../runs/20260910-7122-bve-import-BC-gate/neutrality-7123.json) contains
66 expected solver outcomes and33 exact reported-work/certificate pairs for
7116 versus7087.64 were checked-clean and two UNSAT proofs pending at seal.
The final two tasks started at14:49:10, later than the first group; queued time
was not mistaken for solver time. Provisional continuation does not close the
checked gate. All source, checker, runner and binary identities remain frozen.

[7125 runtime](../runs/20260910-7125-bve-import-runtime/manifest.json) compares
the complete7116 prototype against7030, not just against stage1. It predeclares
three independent six-task waves on31/35/38, with middle-wave arm order
reversed. Every task has12 logical CPUs,48000MiB,1200 solver-only seconds and
16000 checker seconds, within a five-hour cpu-dense-hwulab-q allocation.
No node constraint or array throttle. The existing runtime/cleanup protocol
is unchanged. Both arms must exactly match the checked7030 work/proof oracle;
do not select or discard repetitions based on their timings.

Wave0 is1680000, submitted14:57:48EDT; wave1 is1680109 at15:07:11;
wave2 is1680118 at15:16:34 after all12 prior exact-work/proof solver outcomes.
Each successor wave advances on complete expected solver outcomes with exact
work/certificates, without waiting for proofs.7127 writes its all18-outcome
summary once, reporting paired medians/ranges, all within-arm spreads, RSS and
printed timers. A first-pair time is not an accepted speedup.

[7128 lifecycle profiles](../runs/20260910-7128-bve-lifecycle-profiles/manifest.json)
separately predeclares three two-task Rank35 pairs with the same immutable
7116/7030 binaries and allocation. The frozen profiling wrapper is changed
only to name the two Tactix arms `candidate` and `previous`; it wraps neither
checker and never runs Kissat. The already tested7125 runtime protocol supplies
measurement, proof handling and cleanup. Sampling is the retained199Hz user
cycle event with DWARF8192 stacks. No solver is recompiled or instrumented.
Profile wave0 is1680007 at15:04:19; wave1 is1680115 at15:11:57.

7130 requires exact own unprofiled work/proof fingerprints, valid self/callpath
reports, zero lost samples and hash-bound raw/compressed profile identities.
Profile peak RSS includes instrumentation and cannot replace7125 solver memory.
7132 attributes each pair;7133 reports all three paired lifecycle ratios and
retains the approved20% complete-Eliminate target. Pending proof states remain
visible throughout. Later job/result observations belong to audit7134.

## Complete scope, validated against the old evidence

The maintained callstack aggregator partitions every sample into initial
Probe, search Probe, Eliminate or no observed phase frame. It retains unresolved
callers explicitly and selects the outermost recognized child within a phase,
so mutually exclusive components conserve the total. Inclusive symbol maps
overlap and are never added together. No-phase ancestry is not assumed CDCL.

The immutable DefaultPolicy sources have identical three phase calls. Manual
source/control-flow review and exact machine group-argument checks identify
their initial/search/Eliminate roles. Each manifest binds the complete call
inventory and five-byte calls, not just a selected convenient address. Changes
in binary layout therefore cannot silently relabel a phase.

[7131 replay](2026-09-10-bve-lifecycle-reader-replay-7131.json) reproduces every
7082 current count, period, component, self/inclusive map and unresolved-caller
entry across72033 samples. Seven new tests cover conservation with huge integer
periods, external frames, unknown ancestry, outermost attribution, overlapping
symbols, ambiguous prefixes and empty input. All15 callstack tests and305
aggregate benchmark tests pass. No new solver unit test was needed: this
increment changes offline aggregation/experiment definitions, not solver code.

## First profile pair: useful, below the intended target so far

[Wave0 extraction](../runs/20260910-7132-bve-lifecycle-samples/wave0/summary.json)
conserves71576 candidate and72187 control samples, with no unresolved phase
caller. Both complete work/proof fingerprints equal their unprofiled outcomes.

| Complete scope |7116 billion sampled cycles|7030 billion sampled cycles|Ratio|
|---|---:|---:|---:|
|Initial Probe|3.701|3.737|0.990367|
|Search Probe|359.462|358.430|1.002881|
|Eliminate|110.059|119.522|0.920829|
|No observed phase frame|871.505|873.748|0.997433|

The complete-Eliminate reduction is7.9% in this one pair, below the20% target;
await all predeclared pairs before the decision. Within Eliminate, the BVE
attempt envelope falls64.372/73.979=approximately0.870x, while forward remains
27.656/27.706 and remaining phase work18.032/17.837 billion cycles. This
localizes the observed savings to the intended attempt path, not the whole
inprocessing subsystem. It does not assign savings to an individual changed
line or establish an exact per-operation price. Inlining can move self-symbol
costs without removing work; complete envelopes avoid that attribution trap.

Remaining observed costs include dense occurrence rebuilding13.110 billion
self cycles and forward-partner search10.969 billion. Read-only source review
finds that Eliminate still maintains both large-only and ordered mixed
occurrence images, and performs antecedent membership/validity scans. Native
also collects after each consistent round, so collection frequency is not a
newly demonstrated scheduling defect. These are candidate explanations for
remaining cost, not authority to broaden the approved design or disable rules.
The unchanged Probe envelope remains much larger than Eliminate; do not lose
sight of the largest absolute remaining inprocessing gaps.

## Hygiene and continuation

Only the two freshly created, verified offline profile expansions were
deleted:1,213,829,300 bytes. Original compressed profiles, extracted compressed
sample streams, source disassemblies and hash-bound evidence remain. No active
checker scratch or Claude-owned fuzz campaign was inspected or modified.
Old campaigns, fixed reference and accepted closures remain unchanged.

Continue remaining7126 runtime waves and7129 profile waves as solver outcomes
permit; extract7132 each pair, then summarize7127/7133 only after all repetitions
exist. Revisit every asynchronous proof and require checked-clean final gates.
If the complete prototype misses its premise, return the full evidence and
larger-cost alternatives; do not lower the20% threshold or cherry-pick a rank.
No promotion, current-source fuzz waiver, full50 memory claim or denied push
retry. Docs stay within existing caps. Andrew TeX/PDF is local/unsent at
`report-to-andrew/20260910-1515-bve-lifecycle-measurement/`.
