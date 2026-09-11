# Rank25: storage efficiency, not elapsed runtime

Frozen September11,2026. This updates the next action and corrects one
interpretation in7319; it does not rewrite any old report, measurement or pin.

## Direct answer

Full7312 Rank25 is224.225s Tactix versus246.184s fixed Kissat, but peak RSS is
2484880 versus888820KiB (2.7957x). The checked exact-binary7315 diagnostic
reproduces both peaks and each solver's own work/certificate. Longer total
elapsed time therefore does not explain Tactix's excess. Work differs by phase:
Tactix157173 conflicts/545544 decisions versus native577305/1923486, but BVE
resolutions60646264 versus50112618. Less overall search work is not less work
in every simplifier, nor does it prove matched-unit-cost superiority.

The current clause-arena prototype remains isolated, not deleted or promoted.
Its three-wave Rank31 memory improvement is real (78928KiB less than7267), but
full50 is8124.477s versus8096.416s predecessor, +28.061s. All five repeated
witness median runtime ratios were adverse. This is a memory/runtime tradeoff,
not a rejected positive runtime result or a correctness rejection. Full50 was
rerun before promotion; proofs continue asynchronously.42/50 were independently
checked at this reader; audit7321 owns the later snapshot. Production remains
c2a95a6 and checked7144/7167 rollback remains8072.059s.

## Larger structural lead

Tactix kernel_compact_variables filters retired variables but retains root-fixed
variables with Active status in its real internal arrays. The separate
compact_import_order models Kissat's collapsed population for scheduling/RNG;
that is not physical shrinkage of all per-variable storage. Native compact.c
maps every fixed variable to one signed internal representative and compacts
assignment, flags, phases, queues, heaps and trails accordingly.

Rank25's final Tactix statistics retain3026701 internal variables, whereas
native reports3015540 units. These are different solvers' end-state counters,
not a measurement of Tactix's exact fixed population at its peak. Nonetheless
the source difference is explicit and can keep millions of hot-array slots
after their variables cease participating in search.

Compiled fixed fixture7320 finds Assigned16B, WatchHead16B and86 bytes per
internal variable for the selected subset: two values, one Assigned, two watch
heads, jumped-over literal, unit id and three uint64 touch arrays. At the final
Tactix population this is260296286 bytes (about248.24MiB). It omits heuristic,
identity, dense-index, scratch, arena and reconstruction storage. This is an
array-size subtotal, not RSS or a guaranteed removable/peak saving.

The same fixed fixture verifies OccIndex.ordered at96B and its payload pointer
at112B, matching the0x70 owner-relative pointer stored after the Release
kernel_add_large_clause+0x8d5 realloc site. This strengthens the mixed dense
occurrence mapping lead identified by7318 (493056KiB resident later).

## Correction to7319 heap interpretation

7319 incorrectly said the heap-growth stacks had no solver frame. An exploratory
query passed a relative executable path to a parser matching the absolute
binary paths in strace. Correct absolute-path replay resolves the callers;
7320 explicitly reproduces the relative-path error as a negative control.
The raw traces, peak RSS, work/certificates, and7318 whole-VMA join remain valid.

Before the later candidate snapshot, requesting stacks account for a net
871895040-byte heap extent: major positive contributors include vector resize
193679360B, failed-literal processing172154880B, dense-entry watch compaction
98615296B and one variable-compaction uint64-vector helper86077440B. These
are changes in process heap extent requested by stacks, NOT live bytes retained
by those functions. Free chunks can be reused without another brk. The802MiB
resident heap still needs live-versus-allocator-retained decomposition.

## Next action

Measure Tactix's root-fixed/internal population, selected structure capacities,
and allocator live/free totals around Rank25 BVE entry and peak. Require exact
ordinary Release work/certificate and RSS reproduction before attributing an
observer's data. Compare native at matching semantic boundaries, not each
solver's unrelated global peak. Prioritize hot/cold variable storage and dense
lifetime if confirmed; do not settle for another small growth-policy tweak.

Any physical root compression must preserve N1 public identities/actions,
individual proof unit ids, original-model reconstruction, literal/reason
mapping and established scheduling/RNG/queue-stamp closures. Do not blindly
alias root identities as if they were interchangeable proof objects. No such
rewrite is implemented or accepted by this diagnostic checkpoint.

Bookkeeping audit7185 still applies: heavyweight diagnostics compiled off;
optional scan counters, rule clocks and some runtime guards remain. Their
joint ablation did not yield a consistent net runtime win; not zero overhead.
No new solver source, gate, binary or submitted runner changed. Fixed layout
and relative-path negative controls pass;7321 owns aggregate validation.
Andrew's new local TeX/PDF is under report-to-andrew/20260911-0835-rank25-storage/.
No deletion, push retry or denied fuzz access/transfer. Convergence, final
checks, current-source fuzz, consolidation and synchronization remain open.
