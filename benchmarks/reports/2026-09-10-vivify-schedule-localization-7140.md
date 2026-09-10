# Vivify scheduling: algorithm specialization matters more than copying alone

No solver code or policy changed. The proposed7135 structural pilot remains
unapproved; the rejected7116 BVE prototype remains frozen and unpromoted.
This increment strengthens the read-only diagnosis and repairs the diagnostic
itself, without invoking Claude or starting another solver experiment.

## Exact-PC attribution, including a caught diagnostic omission

[7138](../runs/20260910-7138-vivify-schedule-pc-localization/summary.json) reads
all three existing7116 Rank35 compressed sample streams plus the frozen7082
native stream. It verifies their binary and stream hashes, complete sample
counts/periods, exact Probe attribution and the complete selected self-symbol
universe against frozen summaries. Each selected leaf PC must lie in exactly
one selected binary symbol interval and at an exact disassembled instruction.
No new solver run, raw-profile expansion, source reconstruction or deletion.

The first7137 diagnostic compared demangled names exactly. GNU `nm` and `perf`
render one cloned C++ introsort signature differently: the template prefix
agrees, but one displayed parameter list differs. As a result,7137 reported
zero sort self periods while the frozen profile has10.188 billion in wave1.
Its assertions validated matched names, not completeness of all selected names.
This was an offline diagnostic omission, not a solver or certificate failure.
7137's partial output and script remain frozen as the failing reproducer;
do not use it as complete scheduling attribution. Runtime7127, profile7130,
phase7133 and assessment7135 are unaffected: their aggregation did not join
perf symbol names against `nm` signatures.

The new maintained `perf_self_pc` helper uses hash-bound machine-code identity,
not guessed name normalization.7138 retains every observed spelling as alias
evidence and requires equality with the entire selected perf-symbol universe.
Six new unit tests cover different demangler spellings, unselected leaves,
unknown/interior/out-of-range PCs, overlapping intervals, unselected target
intervals and invalid addresses/sizes. Existing strict sample and annotation
tests remain in force. This diagnostic change explicitly considered and added
regressions; no solver unit test is needed without a solver change.

## Source-reviewed regions, not claimed removable instruction latency

[7139](../runs/20260910-7138-vivify-schedule-pc-localization/reviewed-regions-7139.json)
requires exact half-open instruction boundaries against7138's complete
inventories. Reported figures are medians of the three candidate observations,
not the best wave. Selected blocks are disjoint self regions, but they exclude
out-of-line calls and slow paths; they are not complete operation envelopes.

|Reviewed region|Candidate median billion sampled cycles|
|---|---:|
|Occurrence increment/touched-list fast path|4.707|
|Live large-clause population scan|1.799|
|Repeated irredundant-regime calculation inside candidate comparator|3.757|
|Inspected candidate partition-swap block|0.588|
|Literal-snapshot append fast path|1.301|

The two hot comparator blocks at `0x4d6cf3..0x4d6d21` and
`0x4d6d64..0x4d6d92` load the live-population values, recompute division by10
and compare the result on each irredundant comparison. They correspond to
`vivify.cpp`'s `live_irredundant / 10 > live_redundant` check inside the generic
candidate comparator. The generated arithmetic is reciprocal multiplication
and shifting, not a hardware divide instruction. Sampling/skid means the
weights on following loads/compares cannot be read as exact instruction cost.

The candidate occurrence-count loop has the same essential role as native's
counting loop, whose inspected region has4.397 billion sampled cycles in the
one frozen native observation. Counts are necessary for the retained ordering;
they are not copied-key overhead. Different windows and saturation/touched-list
semantics preclude claiming identical per-operation prices from these numbers.

## Consequence for the proposed design

The strongest local lead is not merely moving fewer bytes. The ordinary pure
irredundant path still comparison-sorts candidate records, with repeated
generic mode/regime logic. Kissat's corresponding `countref` path performs
stable radix ranking by inverse size, maximum occurrence count and priority.
Tactix has a separate radix/reverse path for the large-irredundant skip-sort
regime; that existing repair must not be confused with native-like ranking of
the ordinary pure-irredundant schedule.

If7135 is approved, its compact homogeneous-schedule design should explicitly
use native-equivalent fixed-pass radix ordering for the ordinary pure
irredundant case, retaining logical arena position as the exact final tie.
Redundant precise ordering, const/mixed/reuse-off/flush fallbacks, root-count
timing, pass-time membership, priority and probe/clause pose remain unchanged.
This is a refinement of the pending proposal, not approval or implementation.
Do not substitute a cached-boolean tweak for the whole representation pilot.

The necessary work and relatively small inspected copy/swap regions also
caution against promising the proposed15% complete-Vivify/1% whole-runtime
targets. Those gates remain unproven and unchanged. The diagnosis does not
establish full N4 parity, fix Rank31's excess search work, or close the819.2859s
full50 deficit. It provides a more specific algorithmic hypothesis and a
reliable short diagnostic for deciding whether a structural pilot is worthwhile.

## Handoff

Audit7140 binds the old and new diagnostic evidence, unchanged solver/pins,
published proof/cleanup states and live SLURM accounting; it reruns benchmark,
SLURM and documentation checks. No code/source gate or acceptance bar is waived.
Continue published proof follow-up in the background.7135's new structural
scope still requires user approval under `revamp`; no new solver job or
rewrite is authorized by this diagnostic. Do not rerun successful7137/7138/
7139 writers or rewrite their evidence.

No material deletion this increment. Prior compressed profile streams and
originals remain. Claude-owned fuzz remains unavailable and uninspected;
no denied transfer or push retry. Andrew's report is local/unsent at
`report-to-andrew/20260910-1549-vivify-schedule-localization/`.
