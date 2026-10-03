# Nonterminal parity helper retirement and namespace constraint

## Implemented lifecycle

The internal `parity_certificate::Certificate` now supports `retire_helpers` after a complete export. It deletes its remaining active definitions and helper-root unit, retaining any root unit on an original variable so content-based deletion cannot remove the exported consequence. Retirement is idempotent after success; a failed deletion permanently prevents a successful retirement report. Export is single-use once emission begins, so partially emitted attempts cannot silently be retried against stale bookkeeping. Width-only refusal before emission still permits a different width choice.

This changes no public rule, instance, scheduler or kernel variable allocator. It prepares the nonterminal proof helper for live integration; it is not an implemented live pass. Existing terminal refutation does not invoke cleanup. Original model reconstruction is unaffected by these helper-only deletions.

## Rejected namespace reuse

The [first fixed control](../runs/20261003-parity-helper-retirement/) performed two derivations with cleanup between them, restarting helper IDs at 10. Its exact clause-population test found no remaining helper clauses. Canonical checking and explicit-deletion all-addition LRAT/CakeLPR accepted the proof, but forward every-addition DRAT rejected the second derivation at reported proof line 523. Its proof, checker output and source snapshots are retained as rejected evidence, not overwritten.

Local drat-trim source explicitly ignores unit deletions (`drat-trim.c`, deletion handling); removing the clause occurrences does not clear the checker's retained unit state. Thus even complete clause retirement is insufficient to authorize helper-ID reuse in the required routes. This is checker-route divergence, not a claim that the explicitly checked LRAT derivation was logically invalid. No alternative checker was substituted to accept the rejected lifecycle.

## Corrected evidence

The [monotone control](../runs/20261003-parity-helper-retirement-monotone/) retains the sink's high-water mark across both derivations. It emits 500 additions and 496 deletions before the disjoint closing gadget. The fixed population audit verifies no helper literals remain after each retirement and both exported equivalence clauses remain. Separate cases verify original-variable unit retention, idempotence, export lifecycle and permanent failure after a rejected deletion.

Canonical witness-free DSR conversion/DRAT/CakeLPR, forward every-addition DRAT, and every-addition LRAT/CakeLPR all pass; an unsupported unit is rejected. The existing certificate, premise and selector fixed tests pass as well. Prefix LRAT hints never use the disjoint closing gadget's original clause IDs. A separate fixture invokes the actual native `Proof` writer from a hash-verified source/object binding. Its output is byte-identical to the independently checked monotone certificate; the final helper ID is 37. This verifies the native serializer on this fixed sequence, not all future live interleavings.

## Live adapter requirement

Clause retirement must not release the variable namespace. The future action must coordinate a monotone high-water mark with later proof helpers and ordinary kernel extension variables. Allocation and reservation must occur identically with and without proof output; reading the proof writer's private high-water mark to make solver-state decisions would violate the proof/no-proof invariance contract. The kernel's current ordinary extension allocator is not changed or accepted for this use by this checkpoint.

Before exposing the action, implement snapshot occurrence identities/mappings, this shared allocation contract, complete preflight and proof/kernel publication, provenance and repeated scheduling. Full Debug, current-source Claude-owned fuzz and enabled B28+C5/Rank/WV/WV2 gates remain required. No real-instance solver run, new target closure or baseline rerun occurred. The complete theory-to-Boolean integration goal and all 44 targets remain open.
