# Proof-independent external-variable reservation

## Kernel primitive

`kernel_reserve_proof_variables(k, count, first)` reserves a permanent contiguous range of zero-based external IDs after input import. It appends `INVALID_VAR` entries to the external-to-internal map and unrecorded import-rank entries, without adding internal variables, clauses, assignments or heuristic activations. Ordinary variable growth already appends after the map's end, so later live extension variables skip the range. Compaction preserves the external namespace.

The first reservation freezes `original_external_vars` before adding holes. `kernel_new_extension_vars` now preserves an already frozen original boundary, instead of replacing it with a size that includes proof-only IDs. The public original-model width consequently remains the input width. With no reservation, this condition follows the existing allocation behavior.

Capacity is checked against the signed DIMACS limit before mutation; overflow returns false without changing the output argument or maps. Zero reservation is a no-op and does not freeze the original boundary. Neither reservation nor extension allocation consults a proof writer. The caller must use the same preflight/reservation decision when proof output is disabled. This primitive is not yet wired into a live parity action.

Reserved holes cost external-map/import-rank storage and may enlarge external-model/advice buffers. The operation avoids full per-search-variable allocation; it is not constant-space. Callers must account for these costs in bounded preflight, and must never release IDs after clause cleanup. The earlier retirement control shows why checker-cached units prohibit recycling them.

## Focused validation

The [run record](../runs/20261003-proof-variable-reservation/native-result.json) binds the new kernel object, kernel headers, test source and immutable parent link inputs. `test_proof_variable_reservation`, existing `test_kernel`, `test_factor`, and `test_root_fold` pass. The fixture mixes helper reservations, live extensions and compaction, verifies reserved identities never enter the live-variable order, checks overflow/zero behavior and original-model width, and compares proof-on/off external maps. An original fixed variable retains its reconstructed value.

This is a focused Release-object build with assertions enabled in test translation units; it is not full Debug acceptance. The initial harness used an incorrect output-count helper name and assumed compaction removes its sole fixed representative; those harness mistakes were corrected to use the existing output API and explicitly retire an unconstrained variable. An initial link mixed Debug-only invariant references with Release parent objects; the kernel object was rebuilt with the parent's Release mode. No solver failure or benchmark claim is inferred from those build/harness errors.

No new proof rule emits events through this primitive yet. Independent proofs for the complete live action, full Debug, current-source Claude-owned fuzz and all four enabled regression gates remain pending. Existing solver objects in the focused pin are inherited from the documented graph-reflection parent; this is an allocator-specific control, not an all-current-source solver pin.

## Next integration

Connect certified live clause snapshots, bounded row selection and proof preflight to this reservation primitive. Emit the checked consequences, publish them through ordinary kernel clause/unit paths, retire helper clauses while retaining IDs, and stream action/constraint provenance. Then expose the shared typed action and validate repeated/interrupted execution, replay, observation and default scheduling. No real-instance solver experiment, target closure or baseline refresh occurred. All 44 target and broader portfolio-superiority requirements remain open.
