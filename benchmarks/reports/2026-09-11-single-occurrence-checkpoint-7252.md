# Single-image occurrence storage: tested scaffold, pending consumer cutover

Retained7144 remains the investigation baseline, productionc2a95a6 unchanged.
The original fifty are all checked-clean at8072.059/7239.010=1.115077752x;
760.6589s still separates the current total from the1.01x target. This is a
staged structural implementation, not a claim of runtime or memory improvement.

## Complete GC experiment: local gain, adverse net controls

[7251](2026-09-11-gc-forward-assessment-7251.json) freezes every predeclared
7240 runtime pair and7241 profile. All18 runtime and6 profiled solver outcomes
have exact own retained work and certificate fingerprints. Four runtime and
zero profile checks were complete at assessment; subsequent published checks
belong to7252 audit. B/C7238 is66/66 independently checked-clean.

| Rank | Whole-runtime paired median | Full range | RSS ratio |
|---:|---:|---:|---:|
|31|1.008247649|1.005890160--1.020113764|1.000000|
|35|1.010018356|1.009853366--1.018172606|1.000000|
|38|0.996779655|0.996012590--0.999140345|1.000000|

Every arm's complete spread is below2%. Complete Reduce's median sampled
ratio is0.929310611, range0.925341775--0.932876463:7.069% local reduction.
Nested GC ratios are0.876022892,0.866754055 and0.874479705 (approximately).
No favorable repetition is selected. The nested scopes overlap and do not
sum. Rank31/35 outside-slot medians are1.010774431/1.016564838; exact logical
work does not imply equal whole-binary cost. Code layout and environment are
possible explanations, not established causes.

Do not promote7233 or submit its full50 on this evidence. Retain the code,
tests and measurements; this is not an arbitrary minimum-gain rejection.
It improves a local scope but loses end-to-end on both principal witnesses.
Only1212213220 bytes of the third verified raw profile expansion were removed;
7248 owns the first two receipts. Compressed originals and sample streams remain.

## Mini-design and implemented stage1

Prior7160/7246/7247 establish substantial occurrence/reconnection work across
Eliminate and Probe, with boundary differences that preclude simply treating
all current/native function deltas as removable costs. Current Eliminate owns
both a large-only payload and a mixed binary/large payload; every large entry
is copied into both at construction and both are maintained under mutation.
Probe already omits the mixed image, and forward owns a deliberately partial
index. Neither distinction may disappear to make a benchmark easier.

[7249 build](../runs/20260911-7249-single-occurrence-build/manifest.json) adds
`ConnectedOccurrences`, a four-byte-entry owning mixed stream backed by the
existing FlatLists storage policy. A four-byte per-literal large population
and an allocation-free forward range expose the large-only projection.
There is no second large-reference payload, independent per-list allocation,
stored pointer into the arena, new bit-width limit or scheduling constant.
Stable erase/replace/filter operations maintain cached counts. Binary and
large duplicates remain distinct; stale large references are not inspected or
deleted by storage. First-occurrence deletion does not collapse all duplicates.

This is only the first migration stage: Kernel/OccIndex do not use the new
class yet. Release and independent B/C solver hashes are exactly retained7162's
`cdcb735715d4eb2c23d43bbff14cb9d5ccedaa41592aab693cdc779b531dbdcf`.
Therefore no real-instance rerun or new proof acceptance is inferred from this
scaffold. No current guard, calculus entry, proof delta or public action changed.
The separate prototype source is immutable after this build.

Kissat dense/watch sources retain one mixed connected image and rebuild it at
required boundaries. CaDiCaL's occurrence abstraction is a clause-pointer
vector with separate counts; it is not a drop-in ordering/proof substitute.
Both were reviewed. This component is original C++ code over Tactix's existing
flat-vector owner, not copied reference implementation.

## Required full consumer migration

| Consumer | Preserved obligation at cutover |
|---|---|
| Kernel build/add/remove/collection | One payload publication; stable ordering, duplicate multiplicity, logical arena positions and exact live/stale counts; retain mandated rebuild boundaries. |
| BVE scheduling and cleanup | O(1) physical population, legal-prefix-only garbage removal, first-copy binary semantics, score/retry notification order, exact mixed antecedent order. |
| Forward/Strengthen | Keep the private partial partner index distinct; post-forward full reconnection and root suffix replay remain mandatory. |
| Sweep/Substitute public dense sessions | Preserve mixed insertion positions, dirty-stream publication, bulk-retirement clearing and generator lifetimes; do not reconstruct a binary-prefix approximation. |
| Congruence/Ternary/C1 and generic N1 entry | Large-only queries remain available in either dense mode, with unchanged population caps and order; no policy-only eligibility restriction. |
| Invariants/diagnostics/tests | Compare exact projected sequences and counts, not only sorted multisets; preserve physical binary-copy checks and meaningful memory telemetry. |

Filtered large traversal may scan many binaries that the old contiguous mirror
skipped. This is the principal time/memory tradeoff, not a promised speedup.
Probe must keep its direct large path. Full-state old/new fixtures must first
prove that the existing large sequence equals the intended mixed projection
at every migrated boundary; the old general invariant checks only multisets,
although Sweep publication also checks order. Any contrary state requires a
revised representation, not silently sorting or changing its consumer.

After cutover, run complete source suites, actual parent/candidate mutation,
collection, root/unit/retry and free-N1 fixtures, independent DRAT/LRAT and
original-model checks, sanitizers and B28+C5. Then measure three matched
31/35/38 waves, whole-Eliminate envelopes and peak RSS. Full50 transfer and
Claude-owned fuzz remain consolidation requirements. No arbitrary percentage
is required to retain a real net gain, but a smaller payload is not evidence
that the process uses less memory or that the complete solver is faster.

## Validation and next action

Full suites116/116 Release,116/116 independent B/C,115/115 trace all pass.
The new fixture covers21845 mixed sequences through length7, each with16
stable filters, every-position erase/replacement, independent copied owners,
first duplicate deletion, multipass filtered iteration and exact cached counts.
Another300000 interleaved pushes force relocation; subsequent sparse retention
forces existing75-percent-hole packing. Fresh literals, clear, release,
smaller-universe rebuild and payload-boundary values are exercised.
[7250 sanitizers](../runs/20260911-7250-single-occurrence-sanitizers/summary.json)
run the same fixed fixture twice with ASan/UBSan/LSan outside ptrace; both pass.
No assertion, solver, model, proof or sanitizer failure occurred.

Next fork frozen7249 into a new cutover namespace and migrate the complete
consumer table. Add actual old/new boundary fixtures plus retained-capacity
rebuild and moved-owner cases with that change. Do not submit another timing
campaign for the unused scaffold or rerun a successful writer. Old GC proof
checks continue asynchronously; revisit and require checked-clean results.
7252 audit binds source, binaries, reports and aggregate tooling/docs checks.
Andrew TeX/PDF is local/unsent at
`report-to-andrew/20260911-0316-single-occurrence/`. Preserve N1--N4 and closures
4/32/33/49/46;21 stays scored/shelved,31/43 gaps remain open. No fuzz artifact
access/transfer, denied push retry, production promotion or gate waiver.
