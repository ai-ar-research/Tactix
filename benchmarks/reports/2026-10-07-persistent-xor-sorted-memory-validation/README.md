# Persistent XOR sorted-support and memory validation — 2026-10-07

Frozen next increment on runtime checkpoint `4d7069a4c3ca8342a96ad93b1409093a83291ba1`; this is separate from the running 4d benchmark candidate. Source authority: `source-bindings.json`, tree SHA256 `e8914a8a12e5527c27b5d3fcabf1e21231e9bee5d8f4cbff6ffe48a9767012ef`.

## Changes and scope

Persistent matrix synchronization now recognizes monotone extensions of canonical sorted assignment sets even when a newly assigned variable sorts before existing assignments. It conditions only the difference; lost or changed assignments rebuild. Exhausted or interrupted reductions leave the previous complete state intact. The legacy prefix interface keeps its default behavior.

An allocation-free monotonicity pass and one private assignment workspace respect the existing four-support-population reserve. The allocation observer fixture detects the duplicate workspace in the preceding sorted snapshot and passes the corrected snapshot. This is a represented-vector-storage claim, not a whole-process RSS or allocator-overhead bound.

`cached_event` identifies actual cached matrix-event returns. Default usefulness sampling and unchanged-reduction diagnostics now distinguish those from an unchanged capture followed by a real synchronization after interruption.

## Validation

Both Debug and ASan/UBSan/LSan configurations passed nine native fixtures plus two independent proof/model wrappers (11 checks each). Leak detection was enabled with ptrace access for the sanitizer run. Exact binary hashes, compiler flags, commands, reused object hashes, per-check exits and outputs are in `debug/` and `sanitize/`.

The coherent supplement reuses verified unchanged native OBJECT-library objects from the preceding sorted snapshot and replaces exactly `affine_matrix.cpp.o`. It compiles the new memory fixture independently. All other source files and headers were compared to the predecessor source binding before linking. The initial helper incorrectly assumed a static archive and failed before linking; the corrected helper records the actual OBJECT linkage. No immutable predecessor was edited.

This evidence does not qualify subsequent `proof.cpp` witnessless-RUP lease retention or its changed lifecycle fixture; those are being checked separately. Full Debug, Claude-owned fuzz, four-regression acceptance and rank 569 performance for this next increment remain pending. No new instance closure or speedup is claimed.
