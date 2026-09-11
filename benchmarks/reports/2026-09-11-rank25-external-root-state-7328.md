# Rank 25: unchanged-executable root state at reconstruction growth

Frozen September 11, 2026. Diagnostic 7327 changes the next action from
instrumentation to structural storage design. It does not promote a solver,
claim a runtime gain, or relax the exact-RSS gate.

## Outcome and decision

Job 1681324 completed both Rank 25 SAT tasks. Both executables are byte-exact
ordinary 7302 (SHA256 fc1a4ca2634c4312b22dc844a1fd7ce5897bae34481f74b362a1220fa1519c54).
Only the candidate is externally debugged. Both match ordinary full work and
certificate fingerprints, including physical counters; both original models
independently pass and both final records are clean.

GNU time reports candidate 2484900 versus control 2484880 KiB. The +20 KiB
difference FAILS strict ordinary-RSS reproduction. Do not label this an exact
decomposition of the unobserved ordinary peak or a memory improvement. The
earlier source census's -16 KiB failure remains recorded, not overwritten.

The debugger nevertheless directly reads the unchanged executable's state at
the source-backed growth boundary. This supports the retained-root mechanism,
not an attribution of all ordinary peak bytes. Further attempts to match RSS
exactly are not the next development task. Start an isolated hot/cold root-state
design using the source-level representation discrepancy as its premise;
validate actual repair memory/runtime on uninstrumented pins.

## Exact capture boundary and observations

The external GDB script breaks at 0x40ed1f in kernel_push_witness, after the
old contents have been copied but before operator delete frees the old buffer.
It uses register and process-memory reads only: no inferior calls, injected
allocator hooks, source edits, or per-rule tracing. The breakpoint disables
itself after the selected growth. Configured instruction bytes and vector
pointer invariants are checked. No strace process competes for ptrace.

At C15000 / D106337:

| Quantity | Direct debugger observation |
|---|---:|
| Internal variables | 3586539 |
| Fixed / fixed Active | 3015698 / 3015698 |
| Retired / unassigned / non-root Active | 539192 / 31649 / 0 |
| Eight selected per-variable arrays | 308442354 B (294.15 MiB) |
| Old full reconstruction vector | 237305856 B |
| New reconstruction capacity | 474611712 B |
| /proc VmHWM and VmRSS before/after reading | 2485180 KiB |
| Paused smaps total RSS | 2485180 KiB |
| Heap VMA RSS | 821748 KiB |

The selected arrays are values, Assigned, watch heads, jumped-over literals,
unit IDs and three touch arrays. The same fixed population occurs in source
census 7323's post-application row. One fewer variable is retired at this
earlier within-application point, as expected. The external read lasts about
2.367s while the solver is stopped; diagnostic wall/perf include debugger work
and must not be used as solver performance measurements.

The new reconstruction allocation lies in a 463492 KiB VMA with 233220 KiB
resident. The old allocation lies in a MERGED 399868 KiB resident VMA: that
whole region is not attributable solely to the old vector. Current capacity,
VMA RSS and historical maximum remain different measurements. GNU-time maximum
is 280 KiB below the paused /proc value; preserve this discrepancy rather than
silently choosing one counter. Earlier external observations also retained
raw HWM decreases. No cause of this counter discrepancy is proven here.

## Structural repair direction and safeguards

Tactix kernel_compact_variables maps every Active variable to a physical slot,
including root-fixed ones. Both kissat/src/compact.c and cadical/src/compact.cpp
instead fold fixed variables into one representative and shrink internal
arrays. Tactix's compact_import_order implements the scheduling image but does
not physically compact those root slots. This can affect both storage and
loops that scan the internal-variable domain; a speedup is not yet measured.

CaDiCaL's compaction also preserves LRAT/FRAT unit IDs in external->ext_units
before remapping internal indices. That is a useful mechanism reference, not
a drop-in port: Tactix must keep universal public rule applicability, stable
external literals, distinct proof-unit IDs and original-model reconstruction.
Do not collapse unrelated proof objects just because their values are fixed.

The next mini-design should stage external fixed-value/unit identity storage,
then physical hot-state compaction and all dependent mappings, with rollback
to unchanged 7302. Preserve reason/trail positions, binary-only propagation
debt, root-unit naming, extension variables, factor marks, queue stamps,
stable heap order and established scheduling/RNG closures. Audit all applicable
public instance surfaces rather than assuming fixed variables are invisible.
Existing compact-stamp and API tests exercise distinct fixed identities; add
positive/negative roots, repeated compaction, mixed retired/root/live states,
non-root reasons, DRAT/LRAT checks and reconstructed-model tests as needed.
No partial proof/identity migration is implemented or accepted in this turn.

Do not tune dense hole thresholds or settle for another small growth-policy
change as a substitute. Root-array footprint is only a subset of excess memory;
dense storage/lifetime and the largest Rank 31 time gap remain open.

## Validation and handoff

Nine new Python decoder/wrapper tests pass. Compiled fixture 7326 links the
existing ordinary object files and verifies the actual 7872-byte Kernel layout,
eight-variable populations, C17/D29, and a 64->128B witness-buffer overlap.
The initial helper completed build, plain execution and debugger validation,
then failed constructing its manifest because sha received a str instead of
Path. Original logs/helper and empty manifest remain unchanged; the separate
finalizer seals passing evidence without rebuilding or rerunning.

Campaign 7327 uses the established full-width 12CPU/48000MiB contract on
cpu-dense-hwulab-q, 1200s solver-only and 16000s checker budget, no node
constraint/throttle. Existing campaigns/runners and the fixed Kissat reference
are unchanged. Full7312 remains 8124.477s / 7239.010s = 1.122318798x; 48/50
checked at the reader, remaining checks asynchronous. Clause-arena prototype
remains an isolated memory/runtime tradeoff, not a proven speedup. Production
c2a95a6 and checked7144/7167 rollback remain untouched.

Audit7328 owns aggregate tests, source hashes, published checker states and
finalized record-bound logs. Andrew's new local unsent report is under
report-to-andrew/20260911-0907-external-root-state/. No material deletion,
denied fuzz access/transfer, push retry, source promotion or goal completion.
